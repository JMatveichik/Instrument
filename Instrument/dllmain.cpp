// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include <winuser.h>
#include "instrument.h"
#include "resource.h"

#include "connectionhandlers.h"
#include "NesmitDevice.h"
#include "loggers.h"


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

//������� ��� ������ � �����������
NesmitDevice DEVICE;

//������� ���� ���������� ����������
HWND g_mainWnd = HWND_DESKTOP;

//���������� ����������� ��������� �������� 
int g_currentProgress = 0;

//
HINSTANCE	hInstance = 0;
HMODULE		g_hModule = 0;

//���� ����������� ���������
HWND g_hwndProgressDlg = NULL;

//��������� Modbus ���������� 
std::pair<std::string, int> g_connectionData;

//���������� dll
std::string initdir;

//�������� ����� ������ �����
ILogger*  g_pLogger = nullptr;


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
	hInstance = static_cast<HINSTANCE>(hModule);	

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

		g_pLogger = new FileLogger(getfilepath("log.txt"));
		
		
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
		delete g_pLogger;
	}
	break;

	}
	return TRUE;
}


#pragma region ��������������� ������� ��� ������ � ��������




#pragma endregion

extern "C" {

	// �������� � ������� X008, ���-0, �������� 1. 
	// ���� ������ ������ �������, ������� � ������� �������� true.
	// ������ ������ � �������, ������ ��������� "������ ������ �����", ������ �������� ������� false.
	IMPEXP bool CALLCONV  InitInst(const char* path)
	{
		//������ ����������
		std::string connectionString = (path == nullptr) ? "" : path;

		
		std::stringstream ss;
		std::string cap = "������ : �������������";
		
		try
		{
			//�������� � ��������� ��������� Modbus
			g_connectionData = ConnectionStringHandlers::getConnectionData(connectionString);
			
			//��������� ����� � �����������
			DEVICE.connect(g_connectionData);

			//������������� ����������
			//� ������� �007, ��� 0 - �������� 1.
			DEVICE.initialize();
			
		}
		catch (std::exception& ex)
		{
			//������� ���������� � �����������
			DEVICE.disconect();

			ss << ex.what() << std::endl;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			
			return false;
		}
		//������� ���������� � �����������
		DEVICE.disconect();
		
		return true;
	}


	//���������(state = 1) � ���������(state = 0) ������.
	//������ ������ ���� - 0 �������� �008.���� ��� ����� 0, ������� ��������� "������ �� �����.", � ������� ��������� �������� ������� false.
	//��� �������� state = 1 � ������� X001 ��� 0, �������� �������� 1.
	//��� �������� state = 0 � ������� X001 ��� 0, �������� �������� 0.
	//��� �������� ���������� ������� ������, ������� �������� ������� true.
	//��� ������ ������, ������� ��������� "���� ���������� Shutter", ������� �������� ������� false.
	IMPEXP bool CALLCONV Shutter(unsigned char state)
	{
		std::stringstream ss;
		std::string cap = "������ : ���������� ��������";

		//������������� ������� ���������� � �������� ��� ������ �� �������
		NESMITCONNECTION connection(&DEVICE);
		
		try
		{
			//��������� ����� � �����������
			DEVICE.connect(g_connectionData);

			//�������� ���������� �������
			if (!DEVICE.clientready())
			{
				ss << "������ �� ����� � ������." << std::endl;
				MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
				
				return false;
			}

			//���������� ��������
			DEVICE.shutter(state);
		}
		catch (std::exception& ex)
		{
			ss << ex.what() << std::endl;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			return false;
		}
		
		return true;
	}

	//������������� ������ ���������� �������� � �������� ������� n.
	IMPEXP bool CALLCONV Filter(int value)
	{
		std::stringstream ss;
		std::string cap = "������ : Filter";

		//������������� ������� ���������� � �������� ��� ������ �� �������
		NESMITCONNECTION connection(&DEVICE);
		
		try
		{
			//��������� ����� � �����������
			DEVICE.connect(g_connectionData);

			//�������� ���������� �������
			if (!DEVICE.clientready())
			{
				ss << "������ �� ����� � ������." << std::endl;
				MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
				return false;
			}

			//���������� ��������
			DEVICE.filter(value);
		}
		catch (std::exception& ex)
		{
			ss << ex.what() << std::endl;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			return false;
		}

		return true;		
	}

	//�������� (state=1) ��� ��������� (state=0) ����� �������� ���� (kind=FF) ��� ������� ��������� (kind=CS).
	//kind - ��������� ���������� ���� PChar
	//	if (kind<>'CS') and (kind<>'FF') Then Exit;
	//������ ������ ���� - 0 �������� �008.���� ��� ����� 0, ������� ��������� "������ �� �����.", � ������� ��������� �������� ������� false.
	//	���� kind = 'CS' � state = 1 ����� ���������� � �������� X001 ��� 2 � �������� 1
	//	���� kind = 'CS' � state = 0 ����� ���������� � �������� X001 ��� 2 � �������� 0
	//	���� kind = 'FF' � state = 1 ����� ���������� � �������� X001 ��� 1 � �������� 1
	//	���� kind = 'FF' � state = 0 ����� ���������� � �������� X001 ��� 1 � �������� 0
	IMPEXP bool CALLCONV Lamp(const char* kind, unsigned char state)
	{
		std::stringstream ss;
		std::string cap = "������ : ���������� �������";

		//������������� ������� ���������� � �������� ��� ������ �� �������
		NESMITCONNECTION connection(&DEVICE);

		try
		{
			//��������� ����� � �����������
			DEVICE.connect(g_connectionData);

			//�������� ���������� �������
			if (!DEVICE.clientready())
			{
				ss << "������ �� ����� � ������." << std::endl;
				MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

				return false;
			}

			//���������� �������
			DEVICE.lamp(kind, state);
		}
		catch (std::exception& ex)
		{
			ss << ex.what() << std::endl;
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
	//	�������� ������������. ����� �� �������� �009, ��� 0 ��� 0 % , � 255 - ��� 100 % .
	//	���������� ����� �������� ����������� ��� ������� 2 ���� � �������.
	//	����� ����, ��� � �������� �008, ��� 2 ������ �������� 1, ������� ���� ������������, � ����� �� ������� ������ �������� true.
	IMPEXP bool CALLCONV Disp(unsigned char state, const char* angle)
	{

		std::stringstream ss;
		std::string cap = "������ : ���������� ������ ��������������� ����������";

		//������������� ������� ���������� � �������� ��� ������ �� �������
		NESMITCONNECTION connection(&DEVICE);

		try
		{
			//��������� ����� � �����������
			DEVICE.connect(g_connectionData);

			//�������� ���������� �������
			if (!DEVICE.clientready())
			{
				ss << "������ �� ����� � ������." << std::endl;
				MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

				return false;
			}
			
			//�������� ��������� �������
			if (DEVICE.clientbusy())
			{
				ss << "������ ����� ������ ���������." << std::endl;
				MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

				return false;
			}
			
			//���� � ������� ����������� ������ ����������� ������� �����
			// ��� ����� ��������� � ����� ������������� ����� 12,5 ��� ������������� �����
			// ����������� � 12 � ��� ������������� ������� � 12.5
			//�������� � ����������� �������� �������
			const int ang = int(std::atof(angle) * 1000);
			
			DEVICE.writeangle(ang);
			
		}
		catch (std::exception& ex)
		{
			ss << ex.what() << std::endl;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			return false;
		}
		
		//---------------------------------------------------------------------------------			
		g_currentProgress = 0;

		//���������� ���� ���������
		ShowProgressWnd();
		
		//���� ������ ����� ���������� ���� ������� ������� ��������
		while (DEVICE.clientbusy()) 
		{
			//�������
			Sleep(CYCLE_OPERATION_DELAY);
			
			//��������� ��������� �������� ��� ����������� � ����
			g_currentProgress = static_cast<int>(DEVICE.progress());
			
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
		std::stringstream ss;
		std::string cap = "������ : �������������� ����������";		

		return true;
	}

	//��������� ��� ����������� �������� �� ���������� ������������ � ����������, � ���������� � ���������� �� ������.
	IMPEXP bool CALLCONV CloseInst()
	{
		std::stringstream ss;
		std::string cap = "������ : ������������ ��������";		
		
		//������������� ������� ���������� � �������� ��� ������ �� �������
		NESMITCONNECTION connection(&DEVICE);

		try
		{
			//��������� ����� � �����������
			DEVICE.connect(g_connectionData);

			//������������ ����������
			DEVICE.close();
		}
		catch (std::exception& ex)
		{
			ss << ex.what() << std::endl;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			return false;
		}
		
		return true;
	}

	///-------------------------------------------------------
	//��������� ��������� ����
	IMPEXP bool CALLCONV GetZero()
	{
		std::stringstream ss;
		std::string cap = "������ : ���������� ����";


		//������������� ������� ���������� � �������� ��� ������ �� �������
		NESMITCONNECTION connection(&DEVICE);

		try
		{
			//�������� ���������� �������
			if (!DEVICE.clientready())
			{
				ss << "������ �� ����� � ������." << std::endl;
				MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

				return false;
			}
			
			//��������� ����� � �����������
			DEVICE.connect(g_connectionData);

			//��������� ����������
			DEVICE.zero();

			//�������
			while (DEVICE.commandrequested())
				Sleep(CYCLE_OPERATION_DELAY);
		}
		catch (std::exception& ex)
		{
			ss << ex.what() << std::endl;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			return false;
		}
		
		ss << "���������� ���� ������� ������� ���������..." << std::endl;
		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONINFORMATION);
		
		return true;
	}

	//��������� 
	IMPEXP bool CALLCONV SetTick(const char* state)
	{
		std::stringstream ss;
		std::string cap = "SetTick";
		
		return true;
	}
}