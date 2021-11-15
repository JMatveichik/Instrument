// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "instrument.h"
#include "registers.h"
#include "helper.h"
#include "resource.h"

#ifdef _MSC_VER
	#pragma comment(lib, "user32.lib")
#endif


#pragma comment(linker, "/EXPORT:InitInst=_InitInst@4")
#pragma comment(linker, "/EXPORT:Shutter=_Shutter@4")
#pragma comment(linker, "/EXPORT:Filter=_Filter@4")
#pragma comment(linker, "/EXPORT:Lamp=_Lamp@8")
#pragma comment(linker, "/EXPORT:Disp=_Disp@8")
#pragma comment(linker, "/EXPORT:Slit=_Slit@4")
#pragma comment(linker, "/EXPORT:CloseInst=_CloseInst@0")
#pragma comment(linker, "/EXPORT:GetZero=_GetZero@4")
#pragma comment(linker, "/EXPORT:SetTick=_SetTick@4")

//////////////////////////////////////////
/*		���������� ���������			*/
//////////////////////////////////////////

//modbus ����������
modbus_t *mb;
HWND g_mainWnd = HWND_DESKTOP;

int g_currentProgress = 0;
HINSTANCE hInstance = 0;
HMODULE g_hModule = 0;
HWND g_hwndProgressDlg = NULL;
//---------------------------------------------------------


#pragma region ���� ���������

#define PROGRESS_TIMER_ID 2021


//modal callback function
BOOL CALLBACK ModalDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HWND mainwnd = GetParent(hwnd);	
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
			ss << "�������� " << g_currentProgress << " % ";
			SetDlgItemText(hwnd, IDC_HEADER, ss.str().c_str());


			ss << "WM_TIMER with handle : " << std::hex << hwnd << std::endl;
			OutputDebugString(ss.str().c_str());
			
		}

		break;

		case WM_INITDIALOG:
		{
			std::stringstream ss;
			ss << "�������� " << g_currentProgress << " % ";
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
	}
	return TRUE; //DefWindowProc(hwnd, msg, wParam, lParam);
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






BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
	
	hInstance = (HINSTANCE)hModule;

    switch (ul_reason_for_call)
    {
		case DLL_PROCESS_ATTACH: 
		{
			/*
			WORD procID = GetCurrentProcessId();
			EnumWindows(WinEnum, GetCurrentProcessId());	
			*/				
		}
		break;

		case DLL_THREAD_ATTACH:
		{

		}
		break;

		case DLL_THREAD_DETACH:
		{
			modbus_close(mb);
			modbus_free(mb);
			
		}
		break;

		case DLL_PROCESS_DETACH:
		{
			modbus_close(mb);
			modbus_free(mb);
		}
		break;
       
	}
    return TRUE;
}


#pragma region ��������������� ������� ��� ������ � ��������

bool connect(const char* connectstring)
{
	//���� ����������
	int connected = -1;

	//���� �� ����������� ������� ��������� �� ����� � ������� ���������� "connect.txt"
	if (connectstring == nullptr)
	{
		std::ifstream input;
		input.open("connect.txt");

		if (input.bad())
			return false;

		std::pair<std::string, int> opt = helper::connection(input);
		mb = modbus_new_tcp(opt.first.c_str(), opt.second);
		connected = modbus_connect(mb);

		return (connected == -1) ? false : true;
	}

	// ���� �������� ������ ������ � ���� "ipaddress:port" "192.168.10.18:502" 
	std::pair<std::string, int> opt = helper::connection(connectstring);
	mb = modbus_new_tcp(opt.first.c_str(), opt.second);
	connected = modbus_connect(mb);

	//���� �� ����������� ������� ��������� �� ����� path
	if (connected == -1)
	{
		std::ifstream input;
		input.open(connectstring);

		std::pair<std::string, int> opt = helper::connection(input);
		mb = modbus_new_tcp(opt.first.c_str(), opt.second);
		connected = modbus_connect(mb);
	}

	//���� �� ����������� ������� ��������� �� ����� � ������� ���������� "connect.txt"
	if (connected == -1)
	{
		std::ifstream input;
		input.open("connect.txt");

		std::pair<std::string, int> opt = helper::connection(input);
		mb = modbus_new_tcp(opt.first.c_str(), opt.second);
		connected = modbus_connect(mb);
	}

	//���� �� ������� ���������� ����������  false  
	if (connected == -1)
		return false;

	return true;
}

//��������� ��������
bool  getregister(int reg, uint16_t& value)
{
	///�������� ������� ��������� ��������  
	uint16_t regs[16];
	int readCount = modbus_read_registers(mb, reg, 1, regs);


	//���� �� ������� ����� 
	if (readCount == -1)
		return false;

	value = regs[0];
	return true;
}

//���������� ��� � �������� �������� 
bool setregisterbit(int reg, unsigned char bit, bool state)
{
	///�������� ������� ��������� ��������  
	uint16_t tab_reg[16];
	int readCount = modbus_read_registers(mb, reg, 1, tab_reg);

	//���� �� ������� ����� 
	if (readCount == -1)
		return false;

	//��������� ������� ���� � ������ ���������
	uint16_t output = helper::setbit(tab_reg[0], bit, state);

	//������ � ������� ������ ��������
	int writeConut = modbus_write_registers(mb, reg, 1, &output);

	//���� ������ �� ������ 
	if (writeConut == -1)
		return false;

	//������������� ������ �������� ������� ������
	readCount = modbus_read_registers(mb, reg, 1, tab_reg);

	//���� �� ������� ����� 
	if (readCount == -1)
		return false;

	return true;//helper::checkbitstate(tab_reg[0], bit, state);
}

///���������� ��� � �������� ������� (X008)
bool setstatusbit(unsigned char bit, bool state)
{
	return setregisterbit(StatusRegister, bit, state);
}

///���������� ��� � �������� ������ (X007)
bool setcommandbit(unsigned char bit, bool state)
{
	return setregisterbit(CommandRegister, bit, state);
}

///���������� ��� � �������� ���������� (X001)
bool setcontrolbit(unsigned char bit, bool state)
{
	return setregisterbit(ControlRegister, bit, state);
}

///�������� ���� ��������
bool checkregisterbit(int reg, unsigned char bit)
{
	///�������� ������� ��������� ��������  
	uint16_t tab_reg[16];
	int readCount = modbus_read_registers(mb, reg, 1, tab_reg);

	//���� �� ������� ����� 
	if (readCount == -1)
		return false;

	return helper::checkbit(tab_reg[0], bit);
}

///��������� ��� � �������� ������� (X008)
bool checkstatusbit(unsigned char bit)
{
	return checkregisterbit(StatusRegister, bit);
}

///��������� ��� � �������� ���������� (X001)
bool checkcontrolbit(unsigned char bit)
{
	return checkregisterbit(ControlRegister, bit);
}


bool writeangle(int angle)
{
	///�������� ������� ��������� ��������  
	uint16_t tab_reg[16];
	
	/*int readCount = modbus_read_registers(mb, PositionLowRegister, 2, tab_reg);

	//���� �� ������� ����� 
	if (readCount == -1)
		return false;
	*/

	tab_reg[0] = HIWORD(angle);
	tab_reg[1] = LOWORD(angle);
	
	//������ � ������� ������ ��������
	int writeConut = modbus_write_registers(mb, PositionLowRegister, 2, tab_reg);

	//���� ������ �� ������ 
	if (writeConut == -1)
		return false;

	return true;
}


#pragma endregion


extern "C" {
	// �������� � ������� X008, ���-0, �������� 1. 
	// ���� ������ ������ �������, ������� � ������� �������� true.
	// ������ ������ � �������, ������ ��������� "������ ������ �����", ������ �������� ������� false.
	IMPEXP bool CALLCONV  InitInst(const char* path)
	{
		std::stringstream ss;
		std::string cap = "������ : �������������";

		///���������� �� �������������
		if (!connect(path)) {

			ss << "������ ���������� ��  �������������� ������. ���������  ��������� :" << std::endl;
			ss << "  - ��������� ��� ������� ������� �������" << std::endl;
			ss << "  - ��������� ��� ������� ��������� � ��������� ����" << std::endl;
			ss << "  - �������������� ���� <connect.txt> � ������������ � �������� ������� ���������� (IP:PORT )" << std::endl;

			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK|MB_ICONERROR);
		}

		//������� 
		ss.str(std::string());

		/*
		bool busy = checkstatusbit(CommandBusy);
		//��������---------------------------------
		setstatusbit(CommandBusy, true);
		//-----------------------------------------
		busy = checkstatusbit(CommandBusy);
		*/
	
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

		if (!checkstatusbit(Client)) {
			ss << "����������� �� ����� � ������";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

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
	IMPEXP bool CALLCONV Lamp(const char *kind, unsigned char state)
	{
		std::stringstream ss;
		std::string cap = "������ : ";
		std::string type(kind) ;
		
		unsigned char bit = LampFFBit;

		if (!checkstatusbit(Client)) {
			ss << "����������� �� ����� � ������";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

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

		//����������� �� ����� � ������
		if (!checkstatusbit(Client)) {
			ss << "����������� �� ����� � ������";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

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

		//	� ������� �007, ��� 0 - �������� 0.
		//	� ������� �007, ��� 1 - �������� 0.
		//	� ������� �008. ��� 2 - �������� 0.
		if (!setcommandbit(Angle, false) ||
			!setcommandbit(Gap, false) ||
			!setstatusbit(CommandBusy, false))
		{
			ss << "���� ������� ���� ������� ��������������� ����������. ";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

		uint16_t progress = 0;
		bool complete = false;		

		//���������� ���� ���������
		ShowProgressWnd();

		while (!complete) {

			//�������
			Sleep(250);

			//�������� ������� �� �������� 0-255
			getregister(ProgressRegister, progress);

			//��������� ��������� �������� ��� ����������� � ����
			g_currentProgress = progress * 100 / 256;

			//��������� ���� ���������
			UpdateWindow(g_hwndProgressDlg);

			//��������� ��� �������� ��������� � ��������� �������
			complete = checkstatusbit(CommandBusy);			
		}
		
		//��������� ���� ���������
		CloseProgressWnd();

		return true;
	}
	

	//���������� ������ ���� ������������ ������ state.
	IMPEXP bool CALLCONV Slit(const char *state)
	{
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

		return false;
	}

	//��������� ��� ����������� �������� �� ���������� ������������ � ����������, � ���������� � ���������� �� ������.
	IMPEXP bool CALLCONV CloseInst()
	{
		std::stringstream ss;
		std::string cap = "������ : ������������ ��������";

		// �������� � ������� X008, ���-0, �������� 1.
		if (!setstatusbit(Client, false))
		{
			ss << "���� ����� �� �������������!\n����� � ����������� ����� ���������...";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			modbus_close(mb);
			modbus_free(mb);

			return false;
		}		
		
		modbus_close(mb);
		modbus_free(mb);		

		return true;
	}

	//��������� 
	IMPEXP bool CALLCONV GetZero(const char *state)
	{
		std::stringstream ss;
		std::string cap = "GetZero";

		ss << "����� ������� (" << cap << ") : " << "���������� ����";

		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK);
		return true;
	}

	//��������� 
	IMPEXP bool CALLCONV SetTick(const char *state)
	{
		std::stringstream ss;
		std::string cap = "SetTick";

		ss << "����� ������� (" << cap << ") : " << "�� ������������";

		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK);
		return true;
	}
}