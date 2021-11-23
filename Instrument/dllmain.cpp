// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "instrument.h"
#include "registers.h"
#include "helper.h"
#include "resource.h"
#include "lock.h"


//#define NOT_CHECK_CLIETN_BIT

//��������� ��������� ���� ����� ������
//#define CONFIRM_SET_BIT

#pragma comment(linker, "/EXPORT:InitInst=_InitInst@4")
#pragma comment(linker, "/EXPORT:Shutter=_Shutter@4")
#pragma comment(linker, "/EXPORT:Filter=_Filter@4")
#pragma comment(linker, "/EXPORT:Lamp=_Lamp@8")
#pragma comment(linker, "/EXPORT:Disp=_Disp@8")
#pragma comment(linker, "/EXPORT:Slit=_Slit@4")
#pragma comment(linker, "/EXPORT:CloseInst=_CloseInst@0")
#pragma comment(linker, "/EXPORT:GetZero=_GetZero@0")
#pragma comment(linker, "/EXPORT:SetTick=_SetTick@4")

//////////////////////////////////////////
/*		���������� ���������			*/
//////////////////////////////////////////

//modbus ����������
modbus_t* mb;

//������� ���� ���������� ����������
HWND g_mainWnd = HWND_DESKTOP;

//���������� ����������� ��������� �������� 
int g_currentProgress = 0;

//
HINSTANCE	hInstance = 0;
HMODULE		g_hModule = 0;

//���� ����������� ���������
HWND g_hwndProgressDlg = NULL;

//������ ����������
std::string connectionString;

//���������� dll
std::string initdir;

//�������� ����� ������ �����
std::ofstream loger;

//������������ ���������� ������� ������ ���������
const int MAX_RETRIES = 3;

//�������� ����� �������� Mmodbus, ms
//���� ������������ �������� ����� ����������� ��������� ��������
const int MODBUS_OPERATION_DELAY = 100;

//�������� � ����� ��� �������� ��������� ��������
const int CYCLE_OPERATION_DELAY = 250;

//������� ��� ��������� ������ ���
HANDLE	hStopEvent	= NULL;

//����� ������ ���������
HANDLE	hReadThread = NULL;
DWORD	readThreadID;

//�������� ������ ���
const DWORD	READ_INTERVAL = 500;


//��������� ����������� ��������� ��������� 
uint16_t	CONTROL_AND_INPUT_REGISTER = 0;
uint16_t	HIHTPOS_REGISTER = 0;
uint16_t	LOWPOS_REGISTER = 0;
uint16_t	COMMAND_AND_STATUS_REGISTER = 0;
uint16_t	PROGRESS_REGISTER = 0;
uint16_t	VERSION_REGISTER = 0;

//����������� ������ ��������� ������
CRITICAL_SECTION workCS;


//---------------------------------------------------------
#pragma region ���� ���������

#define PROGRESS_TIMER_ID 2021


//modal callback function
BOOL CALLBACK ModalDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HWND mainwnd = GetParent(hwnd);
	HBRUSH hbrBackground = CreateSolidBrush(RGB(255, 255, 255));

	switch (msg)
	{

	case WM_CREATE:
	{
		std::stringstream ss;
		ss << "WM_CREATE with handle : " << std::hex << hwnd;
		OutputDebugString(ss.str().c_str());

	}
	break;


	case WM_PAINT:
	{
		
		std::stringstream ss;
		ss << "WM_PAINT with handle : " << std::hex << hwnd << std::endl;
		OutputDebugString(ss.str().c_str());		

	}
	
	case WM_TIMER:
	{
		HWND hwndProgressBar = GetDlgItem(hwnd, IDC_PROGRESS);
		UINT iPos = SendMessage(hwndProgressBar, PBM_SETPOS, g_currentProgress, 0);

		std::stringstream ss;
		ss << "��������� ���� ������� " << g_currentProgress << " % ";
		SetDlgItemText(hwnd, IDC_HEADER, ss.str().c_str());


		ss << "WM_TIMER with handle : " << std::hex << hwnd << std::endl;
		OutputDebugString(ss.str().c_str());

	}

	break;

	case WM_CTLCOLORSTATIC:
	{		
		HDC hdcStatic = (HDC)wParam;
		DeleteObject(hbrBackground);
		SetTextColor(hdcStatic, RGB(0, 0, 0));
		SetBkColor(hdcStatic, RGB(255, 255, 255));
		SetBkMode(hdcStatic, TRANSPARENT);			
		return (LONG)hbrBackground;
	}
	break;

	case WM_INITDIALOG:
	{
		std::stringstream ss;
		ss << "��������� ���� ������� " << g_currentProgress << " % ";
		SetDlgItemText(hwnd, IDC_HEADER, ss.str().c_str());

		ss << "WM_INITDIALOG with handle : " << std::hex << hwnd << std::endl;
		OutputDebugString(ss.str().c_str());

		SetTimer(hwnd, PROGRESS_TIMER_ID, 250, NULL);

	}
	break;

	case WM_DESTROY:
	{
		std::stringstream ss;

		ss << "WM_DESTROY with handle : " << std::hex << hwnd << std::endl;
		OutputDebugString(ss.str().c_str());

		KillTimer(hwnd, PROGRESS_TIMER_ID);
		//EndDialog(hwnd, TRUE);//destroy dialog window


	}
	break;

	case WM_SIZE:
	case WM_MOVE:
	{
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	}
	return TRUE; //
}

HWND FindTopWindow(DWORD pid)
{
	std::pair<HWND, DWORD> params = { 0, pid };

	// Enumerate the windows using a lambda to process each window
	BOOL bResult = EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL
		{
			auto pParams = (std::pair<HWND, DWORD>*)(lParam);

			DWORD processId;
			if (GetWindowThreadProcessId(hwnd, &processId) && processId == pParams->second)
			{
				// Stop enumerating
				SetLastError(-1);
				pParams->first = hwnd;
				return FALSE;
			}

			// Continue enumerating
			return TRUE;
		}, (LPARAM)&params);

	if (!bResult && GetLastError() == -1 && params.first)
	{
		return params.first;
	}

	return 0;
}

DWORD GetProcessIdByNameW(LPCSTR name)
{
	PROCESSENTRY32 pe32;
	HANDLE snapshot = NULL;
	DWORD pid = 0;

	snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot != INVALID_HANDLE_VALUE)
	{
		pe32.dwSize = sizeof(PROCESSENTRY32);

		if (Process32First(snapshot, &pe32))
		{
			do
			{
				if (!lstrcmp(pe32.szExeFile, name))
				{
					pid = pe32.th32ProcessID;
					break;
				}
			} while (Process32Next(snapshot, &pe32));
		}
		CloseHandle(snapshot);
	}
	return pid;
}

HWND GetDebugProcessHandle()
{
	DWORD id = GetProcessIdByNameW("ITest.exe");
	HWND g_hwndProgressDlg = FindTopWindow(id);
	

	return g_hwndProgressDlg;
}

//�������� ������ ���������
void ShowProgressWnd()
{
	g_mainWnd = GetDebugProcessHandle();
	g_hwndProgressDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DIALOGBOX), NULL, ModalDialogProc);

	std::stringstream ss;
	ss << "Create dialog with handle : " << std::hex << g_hwndProgressDlg;
	OutputDebugString(ss.str().c_str());

	ShowWindow(g_hwndProgressDlg, SW_SHOW);

	SetWindowText(g_hwndProgressDlg, "����������� ����");

}

///������� ������ ��������� 
void CloseProgressWnd()
{
	EndDialog(g_hwndProgressDlg, TRUE);
	DestroyWindow(g_hwndProgressDlg);
}

#pragma endregion


std::string GetLastErrorAsString()
{
	//Get the error message ID, if any.
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0) {
		return std::string(); //No error message has been recorded
	}

	LPSTR messageBuffer = nullptr;

	//Ask Win32 to give us the string version of that message ID.
	//The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	//Copy the error message into a std::string.
	std::string message(messageBuffer, size);

	//Free the Win32's string's buffer.
	LocalFree(messageBuffer);

	return message;
}

BOOL CALLBACK WinEnum(HWND hwnd, LPARAM lParam)
{
	if (hwnd != NULL)
	{
		g_mainWnd = hwnd;
		return FALSE;

	}

	return TRUE;
}

std::string getfilepath(std::string file)
{
	std::string fullpath = "";
	
	if (initdir.empty())
		return file;

	fullpath = initdir + "\\" + file;
	return fullpath;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	hInstance = (HINSTANCE)hModule;	

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{	
		char dir[MAX_PATH];
		GetModuleFileName(hModule, dir, MAX_PATH);

		initdir = dir;

		const size_t pos_last_slash = initdir.rfind('\\');
		if (std::string::npos != pos_last_slash)
			initdir = initdir.substr(0, pos_last_slash);
		else
			initdir = "";		

		loger.open( getfilepath("log.txt") );
		loger << " ------        ------   " << std::endl;

		hStopEvent = CreateEvent(nullptr, TRUE, FALSE, "StopReadThreadEvent");
		InitializeCriticalSection(&workCS);

		//MessageBox(g_mainWnd, dir, "dir", MB_OK | MB_ICONERROR);
		
	}
	break;

	case DLL_THREAD_ATTACH:
	{
		
	}
	break;

	case DLL_THREAD_DETACH:
	{
		

	}
	break;

	case DLL_PROCESS_DETACH:
	{
		//��������� ���� �����������
		loger.flush();
		loger.close();

		CloseHandle(hStopEvent);
		DeleteCriticalSection(&workCS);
	}
	break;

	}
	return TRUE;
}


#pragma region ��������������� ������� ��� ������ � ��������

int connect(std::pair<std::string, int> opt)
{
	std::stringstream ss;	
	ss << "\t => �������� ��������� ���������� ip (" << opt.first << "):port(" << opt.second << ")" << std::endl;

	mb = modbus_new_tcp(opt.first.c_str(), opt.second);
	int connected = modbus_connect(mb);

	if (connected == -1)
		ss << "\t => ������ ���������� ip (" << opt.first << "):port(" << opt.second << ")" << std::endl;
	else
		ss << "\t => ���������� ����������� ip (" << opt.first << "):port(" << opt.second << ")" << std::endl;

	modbus_set_slave(mb, 1);
	
	loger << ss.str().c_str() << std::endl;
	loger.flush();

	return connected;

}

void flushlogger(std::stringstream &ss)
{
	loger << ss.str().c_str();
	loger.flush();
}

bool connect(const char* connectstring)
{
	std::stringstream ss;
	ss << "connect with string : (" << ((connectstring == nullptr) ? "null" : connectstring) << std::endl;

	//���� ����������
	int connected = -1;

	//���� �� ����������� ������� ��������� �� ����� � ������� ���������� "connect.txt"
	if (connectstring == nullptr)
	{
		std::string fpath = getfilepath("connect.txt");
		ss << "\t => try read connection string from (" << fpath << ")" << std::endl;

		std::ifstream input;
		input.open(fpath);

		if (input.bad()) {
			ss << "\t => ����  ("<< fpath << ") �� ������..." << std::endl;
			return false;
		}			

		connected = connect( helper::connection(input) );

		loger << ss.str().c_str() << std::endl;
		loger.flush();

		return (connected == -1) ? false : true;
	}

	try
	{
		// ���� �������� ������ ������ � ���� "ipaddress:port" "192.168.10.18:502" 
		ss << "\t => try connect from input string ("<< connectstring << ")" << std::endl;
		connected = connect(helper::connection(connectstring));

		loger << ss.str().c_str() << std::endl;
		loger.flush();

	}
	catch (...)
	{
		connected = -1;
	}

	//���� �� ����������� ������� ��������� �� ����� path
	if (connected == -1)
	{
		ss << "\t => try read connection string from file ("<< connectstring << ")" << std::endl;

		std::ifstream input;
		input.open(connectstring);

		if (input.bad()) {
			ss << "\t => ����  ("<< connectstring <<") �� ������..." << std::endl;
			return false;
		}

		connected = connect(helper::connection(input));

		loger << ss.str().c_str() << std::endl;
		loger.flush();
	}

	//���� �� ����������� ������� ��������� �� ����� � ������� ���������� "connect.txt"
	if (connected == -1)
	{
		std::string fpath = getfilepath("connect.txt");
		ss << "\t => try read connection string from (" << fpath << ")" << std::endl;

		std::ifstream input;
		input.open(fpath);

		if (input.bad()) {
			ss << "\t => ����  (" << fpath << ") �� ������..." << std::endl;
			return false;
		}

		connected = connect(helper::connection(input));

		loger << ss.str().c_str() << std::endl;
		loger.flush();

		return (connected == -1) ? false : true;

	}

	//���� �� ������� ���������� ����������  false  
	return connected != -1;
}

//���������� �������� �������� ���
int trygetregisters(int reg, int count, int retries, bool isInput, uint16_t* values)
{
	//������ � ����������� ������
	lock obj(&workCS);

	std::stringstream ss;
	ss << "trygetregister => " << std::endl;

	///���������� ����������� ��������� 
	int readCount = -1;

	int r = 0;
	while (readCount == -1)
	{
		if (isInput) 
		{
			ss << "\tmodbus_read_input_registers =>  register from : (" << reg << ")" <<  " Count ("<< count << ")" << std::endl;		
			readCount = modbus_read_input_registers(mb, reg, count, values);			
		}
		else 
		{
			ss << "\tmodbus_read_registers =>  register from : (" << reg << ")" << " Count (" << count << ")" << std::endl;		
			readCount = modbus_read_registers(mb, reg, count, values);			
		}

		Sleep(MODBUS_OPERATION_DELAY);

		if (r++ >= retries)
			break;

		if (readCount == -1)		
			ss << "\t������� �" << r << " ������ ������ �������� (" << modbus_strerror(errno) << ")" << std::endl;		
		
	}

	//��������� �����
	flushlogger(ss);

	return readCount;
}

//���������� ��� � �������� �������� 
//reg - ����� ��������
//bit - ����� ����
//state - ��������� � ������ ����� ����������� ���
int setregisterbit(int reg, unsigned char bit, bool state)
{
	//������ � ����������� ������
	lock obj(&workCS);

	std::stringstream ss;
	ss << "\tsetregisterbit =>" << std::endl;

	///�������� ������� ��������� ��������  
	uint16_t value;
	if (trygetregisters(reg, 1, MAX_RETRIES, true, &value) == -1)
	{
		ss << "\t\t=>������ ������ ��������" << std::endl;

		//�������� ���
		flushlogger(ss);

		return -1;
	}		
	
	//��������� ������� ���� � ������ ���������
	uint16_t output = helper::setbit(value, bit, state);

	//��� ��� � �������� ���������
	if (output == value)
	{
		ss << "\t\t=> ��� ��������� ��������� �������� => ������� : (" << value << ") ����� : (" << output << ")" << std::endl;
		
		//�������� ���
		flushlogger(ss);
		
		//��� ����������
		return 1;
	}

	//��� ����
	ss << "\t\t=> ��������� ���� => register (" << reg << ") bit : (" << (int)bit << ") state : (" << state << ") ������� : (" << value << ")" << " ����� : (" << output << ")" << std::endl;
	
	//�������� ����� ��������� Modbus
	Sleep(MODBUS_OPERATION_DELAY);

	//������ � ������� ������ ��������
	const int writeCount = modbus_write_registers(mb, reg, 1, &output);

	//���� ������ �� ������� 
	if (writeCount == -1) 
		ss << "\t\t=> ������ ������ �������� => (" << modbus_strerror(errno) << ")" << std::endl;
		
	//��������� �����
	flushlogger(ss);

//���� ����� ������������� ������
#ifndef CONFIRM_SET_BIT
	
	return (writeCount == -1) ? -1 : state;

#else
	//�������� ����� ��������� Modbus
	Sleep(MODBUS_OPERATION_DELAY);

	//������������� ������ - �������� ������� ������
	if (trygetregisters(reg, 1, MAX_RETRIES, true, &value) == -1)
	{
		//��������� �����
		flushlogger(ss);

		return -1;
	}		
	
	//�������� ������������ �� ��������� ����
	const bool isBitSet = helper::checkbitstate(value, bit, state);

	//���� ��� ���������� ���������
	if (isBitSet)
		ss << "\t\t=> bit : (" << bit << ") ���������� � �������� ��������� state : (" << state << ")" << std::endl;
	else
		ss << "\t\t=> bit : (" << bit << ") �� ����������� � ���������  state : (" << state << ")" << std::endl;
	
	//��������� �����
	flushlogger(ss);
	
	return (int)isBitSet;
#endif

	
}

/*
///���������� ��� � �������� ������� (X008)
int setstatusbit(unsigned char bit, bool state)
{
	std::stringstream ss;
	ss << "setstatusbit => " << std::endl;

	flushlogger(ss);

	return setregisterbit(CommandAndStatusRegister, bit, state);
}

///���������� ��� � �������� ������ � ��������� (X007)
int setcommandbit(unsigned char bit, bool state)
{
	std::stringstream ss;
	ss << "setcommandbit => " << std::endl;
	flushlogger(ss);

	return setregisterbit(CommandAndStatusRegister, bit, state);
}

///���������� ��� � �������� ���������� � ������ (X001)
int setcontrolbit(unsigned char bit, bool state)
{
	std::stringstream ss;
	ss << "setcontrolbit => " << std::endl;
	flushlogger(ss);

	return setregisterbit(ControlAndInputRegister, bit, state);
}

///�������� ���� ��������
int checkregisterbit(int reg, unsigned char bit)
{
	std::stringstream ss;
	ss << "\t=> checkregisterbit => register : (" << reg << ") bit : (" << bit << ")" << std::endl;

	///�������� ������� ��������� ��������  
	uint16_t value;
	if (trygetregisters(reg, 1, MAX_RETRIES, true, &value) == -1)
		return -1;	

	bool isSet = helper::checkbit(value, bit);
	return isSet ? 1 : 0;
}

///��������� ��� � �������� ������� (X008)
int checkstatusbit(unsigned char bit)
{
	std::stringstream ss;
	ss << "checkstatusbit => " << std::endl;
	flushlogger(ss);

	return checkregisterbit(CommandAndStatusRegister, bit);
}

///��������� ��� � �������� ���������� (X001)
int checkcontrolbit(unsigned char bit)
{
	std::stringstream ss;
	ss << "checkcontrolbit => " << std::endl;
	flushlogger(ss);

	return checkregisterbit(ControlAndInputRegister, bit);
}

//��������� ������������ ��������� ���� �� ����������
bool checksetbitresult(int res, bool expected)
{
	std::stringstream ss;
	ss << "\t\tchecksetbitresult => " << std::endl;

	//������ �����
	if (res == -1 )
	{
		ss << "\t\t\t=> ������ �����..." << std::endl;
		flushlogger(ss);
		
		return false;
	}
		

	//��������������� ���������� ���������
	if (res != int(expected))
	{
		ss << "\t\t\t=> ��������������� ���������� ��������� => ��������� (" << expected << ")" << " - ���������� (" << res << ")" << std::endl;
		flushlogger(ss);

		return false;
	}
		
	ss << "\t\t\t=> ��������� ������������ => ��������� (" << expected << ")" << " - ���������� (" << res << ")" << std::endl;
	flushlogger(ss);

	return true;
}
*/

//�������� ����  ���������� ������� � ������
//return true - ������ �����
//return false - ������ �� �����
bool clientready(std::string cap)
{	
	//���� � ����������� ������
	lock obj(&workCS);	

	//��������� ��� ���������� ������� � ������
	if (helper::checkbit(COMMAND_AND_STATUS_REGISTER, Client))
		return true;

	std::stringstream ss;
	ss << "����������� �� ����� � ������" << std::endl;
	MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

	//�������� �����
	flushlogger(ss);

	return false;	
}

//�������� ����  ���������� ������� � ������
//return true - ������ ����� ����������� ������ ��������
//return false - ������ ��������
bool clientbusy(std::string cap)
{
	//���� � ����������� ������
	lock obj(&workCS);

	//��������� ��� ��������� ������� 1 - ����� 0 - ��������
	if (!helper::checkbit(COMMAND_AND_STATUS_REGISTER, CommandBusy))
		return false;

	std::stringstream ss;
	ss << "����������� ����� ����������� ��������..." << std::endl;

	MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

	//�������� �����
	flushlogger(ss);

	return true;
}

//�������� �������� � �������
int writeregister(int reg, uint16_t value)
{
	//���� � ����������� ������
	lock obj(&workCS);

	std::stringstream ss;
	ss << "writeregister => register (" << reg << ") : value (" << value << ")" << std::endl;

	//������ � ������� ������ ��������
	int writeCount = -1;

	try 
	{
		writeCount = modbus_write_register(mb, reg, value);
	}
	catch(...)
	{
		writeCount = -1;
	}

	//���� ������ �� ������� 	
	if (writeCount == -1)
		ss << "\t������ ������ (" << modbus_strerror(errno) << ")" << std::endl;
	else
		ss << "\t�������� ����� �������� (" << value << ")" << std::endl;

	//�������� �����
	flushlogger(ss);

	return writeCount;
}

//�������� �������� ���� � �������
int writeangle(uint16_t angle)
{
	//���� � ����������� ������
	lock obj(&workCS);

	std::stringstream ss;
	ss << "writeangle =>" << std::endl;	

	return writeregister(PositionLowRegister, angle);
}

#pragma endregion

void configurate()
{
	uint32_t old_response_to_sec;
	uint32_t old_response_to_usec;

	/* Save original timeout */
	modbus_get_response_timeout(mb, &old_response_to_sec, &old_response_to_usec);

	/* Define a new and too short timeout! */
	modbus_set_response_timeout(mb, 0, 0);

	uint32_t to_sec;
	uint32_t to_usec;

	/* Save original timeout */
	modbus_get_indication_timeout(mb, &to_sec, &to_usec);
	modbus_set_indication_timeout(mb, 0, 0);
}



void readdata()
{

	OutputDebugString("����� ������ ��������� �������...\n");
	
	//� ����� ������ ������ �� ���������
	while (true)
	{
		if (WaitForSingleObject(hStopEvent, 0) == 0)
			break;

		uint16_t regs[16];
		int count = 6;
		
		EnterCriticalSection(&workCS);

		//������ �������� � ���������� ����������
		if (trygetregisters(0, count, MAX_RETRIES, true, regs))
		{		

			//������� ���������� 8 bit  � ������� ������ 8 bit
			CONTROL_AND_INPUT_REGISTER = regs[0];
			
			//������� ������� ���������
			HIHTPOS_REGISTER	= regs[1];

			//������ ������� ���������
			LOWPOS_REGISTER		= regs[2];

			//��������� ������� 8 bit � ������� ��������� 8 bit
			COMMAND_AND_STATUS_REGISTER = regs[3];
			
			//������� ���������
			PROGRESS_REGISTER = regs[4];

			//������� ������
			VERSION_REGISTER = regs[5];
			
		}
		else
		{		

			//������� ���������� 8 bit  � ������� ������ 8 bit
			CONTROL_AND_INPUT_REGISTER = -1;

			//������� ������� ���������
			HIHTPOS_REGISTER = -1;

			//������ ������� ���������
			LOWPOS_REGISTER = -1;

			//��������� ������� 8 bit � ������� ��������� 8 bit
			COMMAND_AND_STATUS_REGISTER = -1;

			//������� ���������
			PROGRESS_REGISTER = -1;

			//������� ������
			VERSION_REGISTER = -1;

			
		}
		
		LeaveCriticalSection(&workCS);

		std::stringstream ss;
		for (int i = 0; i < count; i++)
			ss << regs[i] << " ";

		ss << std::endl;
		OutputDebugString(ss.str().c_str());

		Sleep(READ_INTERVAL);
	}
	
	OutputDebugString("����� ������ ��������� ����������...\n");
}

extern "C" {

	// �������� � ������� X008, ���-0, �������� 1. 
	// ���� ������ ������ �������, ������� � ������� �������� true.
	// ������ ������ � �������, ������ ��������� "������ ������ �����", ������ �������� ������� false.
	IMPEXP bool CALLCONV  InitInst(const char* path)
	{
		if (hReadThread != nullptr)
			return true;

		//��������� ������ ����������
		connectionString = (path == nullptr) ? "" : path;
		
		bool isConnected = connect(connectionString.c_str());

		std::stringstream ss;
		std::string cap = "������ : �������������";

		///���������� �� �������������
		if (!isConnected) {

			ss << "������ ���������� ��  �������������� ������ �� ������ - " << ((path == nullptr) ? "null" : path) << std::endl;
			ss << "���������  ��������� :" << std::endl;
			ss << "  - ��������� ��� ������� ������� ��������" << std::endl;
			ss << "  - ��������� ��� ������ ��������� � ��������� ����" << std::endl;
			ss << "  - �������������� ���� <connect.txt> � ������������ � �������� ������� ���������� (IP:PORT )" << std::endl;

			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			
			//���������� ����
			flushlogger(ss);

			return false;
		}		

		//������� ����� ������ ���������
		hReadThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)readdata, 0, 0, &readThreadID);
		
		//�������� ����� ���������
		Sleep(MODBUS_OPERATION_DELAY);


		//---------------------------------------------------------------------------------
		//������� �������� �������� ��������

		//���� � ����������� ������
		EnterCriticalSection(&workCS);

		// � ������� �007, ��� 0 - �������� 1.
		uint16_t status = helper::setbit(COMMAND_AND_STATUS_REGISTER, Client, true);		

		//����� �� ����������� ������
		LeaveCriticalSection(&workCS);

		//���������� �������� � �������
		if (writeregister(CommandAndStatusRegister, status) == -1)
		{
			
			ss << "������ ����� �� �������������� ������." << std::endl;
			ss << " - ������� ���������� ������� � ������ (Client)" << std::endl;

			//���������
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			//���������� ����
			flushlogger(ss);

			return false;
		}
		
		return true;
	}


	//���������(state = 1) � ���������(state = 0) ������.
	//������ ������ ���� - 0 �������� �008.���� ��� ����� 0, ������� ��������� "������ �� �����.", � ������� ��������� �������� ������� false.
	//��� �������� state = 1 � ������� X001 ��� 0, �������� �������� 1.
	//��� �������� state = 0 � ������� X001 ��� 0, �������� �������� 0.
	//��� �������� ���������� ������� ������, ������� �������� ������� true.
	//��� ������ ������, ������� ��������� "���� ���������� Shutter", �������� �������� ������� false.
	IMPEXP bool CALLCONV Shutter(unsigned char state)
	{
		std::stringstream ss;
		std::string cap = "������ : ���������� ��������";

		//�������� ���������� �������
		if (!clientready(cap))
			return false;


		///������ �������� ��������� ��������� �������
		if (state > 1)
		{
			ss << "������ ��������� ��������� ������� : " << state;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			
			flushlogger(ss);

			return false;
		}

		//
		Sleep(MODBUS_OPERATION_DELAY);


		//---------------------------------------------------------------------------------
		//������� �������� �������� ��������

		//���� � ����������� ������
		EnterCriticalSection(&workCS);

		// � ������� �007, ��� 0 - �������� 1.
		uint16_t status = helper::setbit(CONTROL_AND_INPUT_REGISTER, ShutterBit, (state == 1));

		//����� �� ����������� ������
		LeaveCriticalSection(&workCS);

		//���������� �������� � �������
		if (writeregister(ControlAndInputRegister, status) == -1)
		{

			ss << "���� ���������� ��������." << std::endl;
			ss << " - ������� ���� (ShutterBit : " << (state == 1) << ")" << std::endl;

			//���������
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			//���������� ����
			flushlogger(ss);

			return false;
		}		

		//������� ��������� ���� ��������
		while (true) {

			//
			Sleep(MODBUS_OPERATION_DELAY);

			//������� � ����������� ������
			EnterCriticalSection(&workCS);

			//��������� ��� �������� ��������� ��������  1 - �������� 0 - ���������
			if (helper::checkbit(CONTROL_AND_INPUT_REGISTER, ShutterBit) == (bool)state)
			{
				LeaveCriticalSection(&workCS);
				break;
			}
			LeaveCriticalSection(&workCS);
		}

		return true;
	}

	//������������� ������ ���������� �������� � �������� ������� n.
	IMPEXP bool CALLCONV Filter(int value)
	{
		std::stringstream ss;
		std::string cap = "Filter";

		//�������� ���������� �������
		if (!clientready(cap))
			return false;

		ss << "����� ������� (" << cap << ") : " << value;

		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK);

		return false;
	}

	//�������� (state=1) ��� ��������� (state=0) ����� �������� ���� (kind=FF) ��� ������� ��������� (kind=CS).
	//kind - ��������� ���������� ���� PChar
	//	if (kind<>'CS') and (kind<>'FF') Then Exit;
	//������ ������ ���� - 0 �������� �008.���� ��� ����� 0, ������� ��������� "������ �� �����.", � ������� ��������� �������� ������� false.
	//	���� kind = 'CS' � state = 1 ����� ���������� � �������� X001 ��� 2 � �������� 1
	//	���� kind = 'CS' � state = 0 ����� ���������� � �������� X001 ��� 2 � �������� 0
	//	���� kind = 'FF' � state = 1 ����� ���������� � �������� X001 ��� 1 � �������� 1
	//	���� kind = 'FF' � state = 0 ����� ���������� � �������� X001 ��� 0 � �������� 1
	IMPEXP bool CALLCONV Lamp(const char* kind, unsigned char state)
	{
		std::stringstream ss;
		std::string cap = "������ : ";
		std::string type(kind);

		unsigned char bit = LampFFBit;

		//�������� ���������� �������
		if (!clientready(cap))
			return false;


		///������ �������� ��������� ��������� �����
		if (state > 1)
		{
			ss << "������ ��������� ��������� ����� : " << state;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

		if (type == "FF")
		{
			cap += "����� �������� ����";
			bit = LampFFBit;
		}
		else if (type == "CS")
		{
			cap += "����� ������� ���������";
			bit = LampCSBit;
		}
		else
		{
			ss << "������ ��������� �������������� ����� : " << type;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			
			//��������� ���
			flushlogger(ss);

			return false;
		}

		//---------------------------------------------------------------------------------
		//������� �������� �������� ��������

		//���� � ����������� ������
		EnterCriticalSection(&workCS);

		// � ������� �007, ��� 0 - �������� 1.
		uint16_t status = helper::setbit(CONTROL_AND_INPUT_REGISTER, bit, (state == 1));

		//����� �� ����������� ������
		LeaveCriticalSection(&workCS);

		//���������� �������� � �������
		if (writeregister(ControlAndInputRegister, status) == -1)
		{

			ss << "���� ���������� " << cap.c_str() << std::endl;
			ss << " - ������� ���� ( "<< type.c_str() << " : " << (state == 1) << ")" << std::endl;

			//���������
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			//���������� ����
			flushlogger(ss);

			return false;
		}
		
		//������� ��������� ���� �����
		while (true) {

			//
			Sleep(MODBUS_OPERATION_DELAY);

			//������� � ����������� ������
			EnterCriticalSection(&workCS);

			//��������� ��� �������� ��������� �����  1 - �������� 0 - ���������
			if (helper::checkbit(CONTROL_AND_INPUT_REGISTER, bit) == (bool)state)
			{
				LeaveCriticalSection(&workCS);
				break;
			}
			LeaveCriticalSection(&workCS);
		}

		return true;
	}

	
	//���������� ������ state ��� n-�� ��������������� ���������� ������������.
	//���� �������� angle ����� 8 ��� ������ 30. ����� �������, ��������� �������� ������� false.
	//	������ ������ ���� - 0 �������� �008.���� ��� ����� 0, ������� ��������� "������ �� �����.", � ������� ��������� �������� ������� false.
	//	������ ������ ���� - 2 �������� �008.���� ��� ����� 1. ������� ��������� "������ �����.", � ������� ��������� �������� ������� false.
	//	������� �������� angle, �������� �� 1000, ��������� �� ������ � ���������� � ������� X005 - X006.
	//	� ������� �007, ��� 0 - �������� 0.
	//	� ������� �007, ��� 1 - �������� 0.
	//	� ������� �008. ��� 2 - �������� 0.
	//	����� ������� �� ������ �������� - ���, � �������� �������� �������.
	//	�������� ������������. ����� �� �������� �009, ��� � ��� 0 % , � 255 - ��� 100 % .
	//	���������� ����� �������� ����������� ��� ������� 2 ���� � �������.
	//	����� ����, ��� � �������� �008, ��� 2 ������ �������� 1, ������� ���� ������������, � ����� �� ������� ������ �������� true.
	IMPEXP bool CALLCONV Disp(unsigned char state, const char* angle)
	{
		std::stringstream ss;
		std::string cap = "������ : �������������� ����������";

		//�������� ���������� �������
		if (!clientready(cap))
			return false;

		//����������� ����� ������ ���������
		if (clientbusy(cap))
			return false;

		//���� � ������� ����������� ������ ����������� ������� �����
		// ��� ����� ��������� � ����� ������������� ����� 12,5 ��� ������������� �����
		// ����������� � 12 � ��� ������������� ������� � 12.5
		//�������� � ����������� �������� �������
		const int ang = int(std::atof(angle) * 1000);

		//������ ������� ���� ������� ��������������� ����������
		if (ang < 8000 || ang > 30000)
		{
			ss << "������ ������� ���� (�� 8� �� 30�) : " << ang << std::endl;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			//��������� ���
			flushlogger(ss);

			return false;
		}

		
		

		EnterCriticalSection(&workCS);

		//��������� ��������� �������� ��� ����������� � ����
		if (ang == LOWPOS_REGISTER)
		{
			
			//������� �� ����������� ������
			LeaveCriticalSection(&workCS);

			return true;
		}

		//������� �� ����������� ������
		LeaveCriticalSection(&workCS);

		//�������� ���� ��������� ���������
		Sleep(MODBUS_OPERATION_DELAY);

		if (writeangle(ang) == -1 )
		{
			ss << "���� ������� ���� ������� ��������������� ����������." << std::endl;
			ss << "������� writeangle ("<< ang << ")." << std::endl;

			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			
			//��������� ���
			flushlogger(ss);

			return false;
		}

		
		//�������� ���� ��������� ���������
		Sleep(MODBUS_OPERATION_DELAY);		
				
		//---------------------------------------------------------------------------------
		//������� �������� �������� ��������
		
		//���� � ����������� ������
		EnterCriticalSection(&workCS);

		// � ������� �007, ��� 0 - �������� 0.
		uint16_t status = helper::setbit(COMMAND_AND_STATUS_REGISTER, Angle, false);

		// � ������� �007, ��� 1 - �������� 0.
		status = helper::setbit(status, Gap, false);

		// � ������� �007. ��� 2 - �������� 0.
		status = helper::setbit(status, StopEngine, false);		
		
		// � ������� �008. ��� 9 - �������� 0.
		status = helper::setbit(status, CommandRequest, false);

		LeaveCriticalSection(&workCS);

		//���������� �������� � �������
		if (writeregister(CommandAndStatusRegister, status) == -1)
		{
			ss << "���� ������� ���� ������� ��������������� ����������. " << std::endl;
			ss << " - ��������� ���� (Angle) " << std::endl;
			ss << " - ��������� ���� (Gap) " << std::endl;
			ss << " - ��������� ���� (StopEngine) " << std::endl;
			ss << " - ��������� ���� (CommandRequest) " << std::endl;
			
			//���������
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			
			//���������� ����
			flushlogger(ss);

			return false;
		}
		//---------------------------------------------------------------------------------				
				

		g_currentProgress = 0;

		//���������� ���� ���������
		ShowProgressWnd();	

		//������� ���� ���������
		
		while (true) {

			Sleep(CYCLE_OPERATION_DELAY);

			//������� � ����������� ������
			EnterCriticalSection(&workCS);

			//��������� ��� �������� ��������� � ��������� �������
			//��� �������� ��������� � ��������� ������� ����-  1 ����� 0 ��������
			if (helper::checkbit(COMMAND_AND_STATUS_REGISTER, CommandBusy))
			{
				LeaveCriticalSection(&workCS);
				break;
			}
			LeaveCriticalSection(&workCS);
		}

		
		while (true) 
		{
			//�������
			Sleep(CYCLE_OPERATION_DELAY);

			//������� � ����������� ������
			EnterCriticalSection(&workCS);

			//��������� ��� �������� ��������� � ��������� �������
			//��� �������� ��������� � ��������� ������� ����-  1 ����� 0 ��������
			if (!helper::checkbit(COMMAND_AND_STATUS_REGISTER, CommandBusy))
			{
				LeaveCriticalSection(&workCS);
				break;	
			}					
			
			//��������� ��������� �������� ��� ����������� � ����
			g_currentProgress = PROGRESS_REGISTER * 100 / 255;

			//������� �� ����������� ������
			LeaveCriticalSection(&workCS);

			//��������� ���� ���������
			UpdateWindow(g_hwndProgressDlg);

			
		}

		//��������� ���� ���������
		CloseProgressWnd();

		return true;
	}

	//���������� ������ ���� ������������ ������ state.
	IMPEXP bool CALLCONV Slit(const char* state)
	{
		/*
		std::stringstream ss;
		std::string cap = "������ : �������������� ����������";


		ss << "����� ������� (" << cap << ") : ";
		if (state[0] == 1)
			ss << " => ��������...";
		else if (state[0] == 0)
			ss << " => ��������...";
		else
			ss << " => ������ ��������� state : " << state;

		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK);
		*/

		return true;
	}

	//��������� ��� ����������� �������� �� ���������� ������������ � ����������, � ���������� � ���������� �� ������.
	IMPEXP bool CALLCONV CloseInst()
	{
		std::stringstream ss;
		std::string cap = "������ : ������������ ��������";		
		
		
		EnterCriticalSection(&workCS);

		// � ������� �007, ��� 0 - �������� 0.
		uint16_t status = helper::setbit(COMMAND_AND_STATUS_REGISTER, Client, false);

		LeaveCriticalSection(&workCS);

		
		if  ( writeregister(CommandAndStatusRegister, status) == -1)
		{
			ss << "���� ����� �� �������������!";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			flushlogger(ss);
		}

		while (true) {

			Sleep(CYCLE_OPERATION_DELAY);

			//������� � ����������� ������
			EnterCriticalSection(&workCS);

			//��������� ��� �������� ��������� � ��������� �������
			//��� �������� ��������� � ��������� ������� ����-  
			if (!helper::checkbit(COMMAND_AND_STATUS_REGISTER, Client))
			{
				LeaveCriticalSection(&workCS);
				break;
			}
			LeaveCriticalSection(&workCS);
		}
		

		if (hReadThread != nullptr)
		{
			//��������� ������ ������
			SetEvent(hStopEvent);
			while (WaitForSingleObject(hReadThread, INFINITE) != 0);
		
			//���������� ������� ��������� ������ ������
			ResetEvent(hStopEvent);

			//��������� ���������� ������ � ��������� ���
			CloseHandle(hReadThread);
			hReadThread = nullptr;

			//�����������  ������� modbus
			modbus_close(mb);
			modbus_free(mb);
		}
		

		return true;
	}

	///-------------------------------------------------------
	//��������� ��������� ����
	IMPEXP bool CALLCONV GetZero()
	{
		std::stringstream ss;
		std::string cap = "GetZero";

		//�������� ���������� �������
		if (!clientready(cap))
			return false;		
		

		//�������� ����� ��������� Modbus
		Sleep(MODBUS_OPERATION_DELAY);


		//---------------------------------------------------------------------------------
		//������� �������� �������� ��������

		//���� � ����������� ������
		EnterCriticalSection(&workCS);

		// � ������� �007, ��� 3 - �������� 1.
		uint16_t status = helper::setbit(COMMAND_AND_STATUS_REGISTER, ResetZero, true);
				
		// � ������� �008. ��� 9 - �������� 0.
		status = helper::setbit(status, CommandRequest, false);

		LeaveCriticalSection(&workCS);

		
		//���������� �������� � �������
		if (writeregister(CommandAndStatusRegister, status) == -1)
		{
			ss << "���� ���������� ����" << std::endl;
			ss << " - ��������� ���� (ResetZero) " << std::endl;			
			ss << " - ��������� ���� (CommandRequest) " << std::endl;

			//���������
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			//���������� ����
			flushlogger(ss);

			return false;
		}
		
		//�������� ����� ��������� Modbus
		Sleep(MODBUS_OPERATION_DELAY);

		if (writeangle(0) == -1)
		{
			ss << "���� ����� �� �������������!" << std::endl;
			ss << " - ��������� ���� (0) " << std::endl;

			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			//���������� ����
			flushlogger(ss);
			
			return false;
		}

		//�������� ����� ��������� Modbus
		Sleep(MODBUS_OPERATION_DELAY);

		
		//������� ���� ���������
		while (true) {

			Sleep(CYCLE_OPERATION_DELAY);

			//������� � ����������� ������
			EnterCriticalSection(&workCS);

			//��������� ��� �������� ��������� �������  �� ����������  �������. 
			//��� �������� ��������� � ��������� ������� ����-  0 - ������ 1 -  ������

			if (!helper::checkbit(COMMAND_AND_STATUS_REGISTER, CommandRequest))
			{
				LeaveCriticalSection(&workCS);
				break;
			}

			LeaveCriticalSection(&workCS);
		}

		while (true) {

			//�������
			Sleep(CYCLE_OPERATION_DELAY);

			//������� � ����������� ������
			EnterCriticalSection(&workCS);

			//��������� ��� �������� ��������� � ��������� �������
			//��� �������� ��������� � ��������� ������� ����
			if (helper::checkbit(COMMAND_AND_STATUS_REGISTER, CommandRequest))
			{
				LeaveCriticalSection(&workCS);
				break;
			}
		
			//������� �� ����������� ������
			LeaveCriticalSection(&workCS);				

			
		}
		
		
		ss << "���������� ���� ������� ������� ���������..." << std::endl;
		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONINFORMATION);

		//���������� ����
		flushlogger(ss);		

		return true;
	}

	//��������� 
	IMPEXP bool CALLCONV SetTick(const char* state)
	{
		std::stringstream ss;
		std::string cap = "SetTick";

		ss << "����� ������� (" << cap << ") : " << "�� ������������";

		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK);
		return true;
	}
}