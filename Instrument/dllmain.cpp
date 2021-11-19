// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "instrument.h"
#include "registers.h"
#include "helper.h"
#include "resource.h"
#include "lock.h"

//#define NOT_CHECK_CLIETN_BIT




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
bool trygetregisters(int reg, int count, int retries, bool isInput, uint16_t* values)
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

		Sleep(25);

		if (r++ >= retries)
			break;

		if (readCount == -1)		
			ss << "\t������� �" << r << " ������ ������ �������� (" << modbus_strerror(errno) << ")" << std::endl;		
		
	}

	loger << ss.str().c_str();
	loger.flush();

	return (readCount != -1);
}

//���������� ��� � �������� �������� 
bool setregisterbit(int reg, unsigned char bit, bool state)
{
	//������ � ����������� ������
	lock obj(&workCS);

	std::stringstream ss;
	ss << "setregisterbit =>" << std::endl;

	///�������� ������� ��������� ��������  
	uint16_t tab_reg[16];
	if (!trygetregisters(reg, 1, MAX_RETRIES, true, tab_reg))
		return false;
	
	//�������� ����� ���������
	Sleep(50);

	//��������� ������� ���� � ������ ���������
	uint16_t output = helper::setbit(tab_reg[0], bit, state);
	ss << " => setbit register : " << reg << " bit : " << bit << " state : " << state << "output value : " << output;

	//������ � ������� ������ ��������
	int writeConut = modbus_write_registers(mb, reg, 1, &output);

	//���� ������ �� ������ 
	if (writeConut == -1) {
		ss << " ������ ������ (" << modbus_strerror(errno) << ")" << std::endl;
		loger << ss.str().c_str();
		loger.flush();
		return false;
	}

	//�������� ����� ���������
	Sleep(50);

	//������������� ������ �������� ������� ������
	if (!trygetregisters(reg, 1, MAX_RETRIES, true, tab_reg))
		return false;


	return true;//helper::checkbitstate(tab_reg[0], bit, state);
}

///���������� ��� � �������� ������� (X008)
bool setstatusbit(unsigned char bit, bool state)
{
	return setregisterbit(CommandAndStatusRegister, bit, state);
}

///���������� ��� � �������� ������ (X007)
bool setcommandbit(unsigned char bit, bool state)
{
	return setregisterbit(CommandAndStatusRegister, bit, state);
}

///���������� ��� � �������� ���������� (X001)
bool setcontrolbit(unsigned char bit, bool state)
{
	return setregisterbit(ControlAndInputRegister, bit, state);
}

///�������� ���� ��������
bool checkregisterbit(int reg, unsigned char bit)
{
	std::stringstream ss;
	ss << "checkregisterbit => register : " << reg << " bit : " << 1 << std::endl;

	///�������� ������� ��������� ��������  
	uint16_t tab_reg[16];
	if (!trygetregisters(reg, 1, MAX_RETRIES, true, tab_reg))
		return false;	

	return helper::checkbit(tab_reg[0], bit);
}

///��������� ��� � �������� ������� (X008)
bool checkstatusbit(unsigned char bit)
{
	return checkregisterbit(CommandAndStatusRegister, bit);
}

///��������� ��� � �������� ���������� (X001)
bool checkcontrolbit(unsigned char bit)
{
	return checkregisterbit(ControlAndInputRegister, bit);
}


bool writeangle(uint16_t angle)
{
	std::stringstream ss;
	ss << "writeangle =>" << std::endl;

	//������ � ������� ������ ��������
	int writeConut = modbus_write_register(mb, PositionLowRegister, angle);


	//���� ������ �� ������ 	
	if (writeConut == -1) 
	{
		ss << " ������ ������ (" << modbus_strerror(errno) << ")" << std::endl;
		loger << ss.str().c_str();
		loger.flush();
		return false;
	}

	return true;
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

		//������ �������� � ���������� ����������
		if (trygetregisters(0, count, MAX_RETRIES, true, regs))
		{
			EnterCriticalSection(&workCS);

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

			LeaveCriticalSection(&workCS);
		}
		else
		{
			EnterCriticalSection(&workCS);

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

			LeaveCriticalSection(&workCS);
		}

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
		if (hReadThread != NULL)
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
			ss << "  - ��������� ��� ������� ������� �������" << std::endl;
			ss << "  - ��������� ��� ������ ��������� � ��������� ����" << std::endl;
			ss << "  - �������������� ���� <connect.txt> � ������������ � �������� ������� ���������� (IP:PORT )" << std::endl;

			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}		

		
		hReadThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)readdata, 0, 0, &readThreadID);

		//������� 
		ss.str(std::string());

		// �������� � ������� X008, ���-0, �������� 1.
		if (setstatusbit(Client, true))
			return true;

		ss << "������ ����� �� �������������� ������." << std::endl;
		ss << "  - ������� ���������� ������� � ������ (X008 -> CLCNT)" << std::endl;

		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
		return false;
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

#ifndef NOT_CHECK_CLIETN_BIT

		if (!checkstatusbit(Client)) {
			ss << "����������� �� ����� � ������";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}
#endif
		///������ �������� ��������� ��������� �������
		if (state > 1)
		{
			ss << "������ ��������� ��������� ������� : " << state;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

		//������ ���� � ������� ����������
		if (!setcontrolbit(ShutterBit, (state == 1)))
		{
			ss << "���� ���������� ��������";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

		return true;
	}

	//������������� ������ ���������� �������� � �������� ������� n.
	IMPEXP bool CALLCONV Filter(int value)
	{
		std::stringstream ss;
		std::string cap = "Filter";

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

#ifndef NOT_CHECK_CLIETN_BIT

		if (!checkstatusbit(Client)) {
			ss << "����������� �� ����� � ������";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}
#endif

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
			return false;
		}

		if (!setcontrolbit(bit, (state == 1)))
		{
			ss << "���� ���������� ������";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
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

#ifndef NOT_CHECK_CLIETN_BIT

		if (!checkstatusbit(Client)) {
			ss << "����������� �� ����� � ������";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}
#endif

		//����������� ����� ������ ���������
		bool busy = checkstatusbit(CommandBusy);
		if (busy) {
			ss << "����������� ����� ����������� ��������...";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}


		int ang = std::atoi(angle);
		//������ ������� ���� ������� ��������������� ����������
		if (ang < 8 || ang > 30)
		{
			ss << "������ ������� ���� (�� 8 �� 30) : " << ang << std::endl;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

		//�������� � ����������� �������� �������
		ang *= 1000;

		if (!writeangle(ang))
		{
			ss << "���� ������� ���� ������� ��������������� ����������";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

		bool bitIsSet = true;

		//	� ������� �007, ��� 0 - �������� 0.
		if (!setcommandbit(Angle, false))
		{
			ss << "���� ������� ���� ������� ��������������� ����������. " << std::endl;
			ss << "��������� ���� (Angle) " << std::endl;
			bitIsSet = false;
		}

		//	� ������� �007, ��� 1 - �������� 0.
		if (!setcommandbit(Gap, false))
		{
			ss << "���� ������� ���� ������� ��������������� ����������. " << std::endl;
			ss << "��������� ���� (Gap) " << std::endl;
			bitIsSet = false;
		}
				
		//	� ������� �007. ��� 2 - �������� 0.
		if (!setstatusbit(StopEngine, false))
		{
			ss << "���� ������� ���� ������� ��������������� ����������. " << std::endl;
			ss << "��������� ���� (StopEngine) " << std::endl;
			bitIsSet = false;
		}

		//	� ������� �008. ��� 9 - �������� 0.
		if (!setstatusbit(CommandRequest, false))
		{
			ss << "���� ������� ���� ������� ��������������� ����������. " << std::endl;
			ss << "��������� ���� (CommandRequest) " << std::endl;
			bitIsSet = false;
		}

		if (!bitIsSet)
		{
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

		uint16_t progress = 0;		

		//���������� ���� ���������
		ShowProgressWnd();
		
		//��������� ��� �������� ��������� � ��������� �������
		while (checkstatusbit(CommandBusy)) {

			//�������
			Sleep(250);

			//�������� ������� �� �������� 0-255
			trygetregisters(ProgressRegister, 1, MAX_RETRIES, true, &progress);

			//��������� ��������� �������� ��� ����������� � ����
			g_currentProgress = progress * 100 / 256;

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

		// �������� � ������� X008, ���-0, �������� 1.
		if (!setstatusbit(Client, false))
		{
			ss << "���� ����� �� �������������!";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
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
		}
		

		//modbus_close(mb);
		//modbus_free(mb);

		return true;
	}

	//��������� 
	IMPEXP bool CALLCONV GetZero()
	{
		std::stringstream ss;
		std::string cap = "GetZero";

#ifndef NOT_CHECK_CLIETN_BIT

		if (!checkstatusbit(Client)) {
			ss << "����������� �� ����� � ������";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}
#endif
		
		bool isCmdsComplete = true;

		// �������� � ������� X008, ���-3, �������� 1.
		if (!setstatusbit(ResetZero, true))
		{
			ss << "���� ����� �� �������������!";
			ss << "��������� ���� (ResetZero) " << std::endl;
			isCmdsComplete = false;			
		}

		if (!writeangle(0))
		{
			ss << "���� ����� �� �������������!";
			ss << "��������� ���� (0) " << std::endl;
			isCmdsComplete = false;
		}

		// �������� � ������� X008, ���-9, �������� 0.
		if (!setstatusbit(CommandRequest, false))
		{
			ss << "���� ����� �� �������������!";
			ss << "��������� ���� (CommandRequest) " << std::endl;
			isCmdsComplete = false;
		}

		if (!isCmdsComplete) {
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}


		//��������� ��� �������� ��������� � ��������� �������
		while (checkstatusbit(CommandBusy)) {
			//�������
			Sleep(250);
		}
		
		ss << "���������� ���� ������� ����� �������!";
		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONINFORMATION);

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