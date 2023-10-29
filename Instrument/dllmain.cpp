// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include <winuser.h>
#include "instrument.h"
#include "resource.h"

#include "connectionhandlers.h"
#include "NesmitDevice.h"
#include "loggers.h"


//проверять состояние бита после записи
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
/*		ГЛОБАЛЬНЫЕ ПЕРМЕННЫЕ			*/
//////////////////////////////////////////

//обертка для работы с устройством
NesmitDevice DEVICE;

//главное окно вызвавшего приложения
HWND g_mainWnd = HWND_DESKTOP;

//переменная отображения прогресса открытия 
int g_currentProgress = 0;

//
HINSTANCE	hInstance = 0;
HMODULE		g_hModule = 0;

//окно отображения прогресса
HWND g_hwndProgressDlg = NULL;

//параметры Modbus соединения 
std::pair<std::string, int> g_connectionData;

//директория dll
std::string initdir;

//выходной поток вывода логов
ILogger*  g_pLogger = nullptr;


//---------------------------------------------------------
#pragma region ОКНО ПРОГРЕССА

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
		ss << "Установка угла решетки " << g_currentProgress << " % ";
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
		ss << "Установка угла решетки " << g_currentProgress << " % ";
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

//показать диалог прогресса
void ShowProgressWnd()
{
	g_mainWnd = GetDebugProcessHandle();
	g_hwndProgressDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DIALOGBOX), NULL, ModalDialogProc);

	std::stringstream ss;
	ss << "Create dialog with handle : " << std::hex << g_hwndProgressDlg;
	OutputDebugString(ss.str().c_str());

	ShowWindow(g_hwndProgressDlg, SW_SHOW);

	SetWindowText(g_hwndProgressDlg, "Регулировка угла");

}

///Закрыть диалог прогресса 
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
		//закрываем файл логирования
		delete g_pLogger;
	}
	break;

	}
	return TRUE;
}


#pragma region ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ РАБОТЫ С ПРИБОРОМ




#pragma endregion

extern "C" {

	// Записать в регистр X008, бит-0, значение 1. 
	// Если запись прошла успешно, вернуть в функции значение true.
	// запись прошла с ошибкой, выдать сообщение "НЭСМИТ ошибка связи", выдать значение функции false.
	IMPEXP bool CALLCONV  InitInst(const char* path)
	{
		//строка соединения
		std::string connectionString = (path == nullptr) ? "" : path;

		
		std::stringstream ss;
		std::string cap = "НЭСМИТ : Инициализация";
		
		try
		{
			//получаем и сохраняем параметры Modbus
			g_connectionData = ConnectionStringHandlers::getConnectionData(connectionString);
			
			//установка связи с устройством
			DEVICE.connect(g_connectionData);

			//инициализация устройства
			//В регистр Х007, бит 0 - записать 1.
			DEVICE.initialize();
			
		}
		catch (std::exception& ex)
		{
			//закрыть соединение с устройством
			DEVICE.disconect();

			ss << ex.what() << std::endl;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			
			return false;
		}
		//закрыть соединение с устройством
		DEVICE.disconect();
		
		return true;
	}


	//Открывает(state = 1) и закрывает(state = 0) затвор.
	//Делаем чтение бита - 0 регистра Х008.Если бит равен 0, выводим сообщение "НЭСМИТ Не готов.", и выходим возвращая значение функции false.
	//При значении state = 1 в регистр X001 бит 0, записать значение 1.
	//При значении state = 0 в регистр X001 бит 0, записать значение 0.
	//При успешном выполнении команды записи, вернуть значение функции true.
	//При ошибки записи, вывести сообщение "Сбой управления Shutter", вернуть значение функции false.
	IMPEXP bool CALLCONV Shutter(unsigned char state)
	{
		std::stringstream ss;
		std::string cap = "НЭСМИТ : Управление затвором";

		//автоматически закроет соединение с прибором при выходе из функции
		NESMITCONNECTION connection(&DEVICE);
		
		try
		{
			//установка связи с устройством
			DEVICE.connect(g_connectionData);

			//проверка готовности прибора
			if (!DEVICE.clientready())
			{
				ss << "НЭСМИТ не готов к работе." << std::endl;
				MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
				
				return false;
			}

			//управление затврром
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

	//Устанавливает фильтр разделения порядков с заданным номером n.
	IMPEXP bool CALLCONV Filter(int value)
	{
		std::stringstream ss;
		std::string cap = "НЭСМИТ : Filter";

		//автоматически закроет соединение с прибором при выходе из функции
		NESMITCONNECTION connection(&DEVICE);
		
		try
		{
			//установка связи с устройством
			DEVICE.connect(g_connectionData);

			//проверка готовности прибора
			if (!DEVICE.clientready())
			{
				ss << "НЭСМИТ не готов к работе." << std::endl;
				MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
				return false;
			}

			//управление затврром
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

	//Включить (state=1) или выключить (state=0) лампу плоского поля (kind=FF) или спектра сравнения (kind=CS).
	//kind - строковая переменная типа PChar
	//	if (kind<>'CS') and (kind<>'FF') Then Exit;
	//Делаем чтение бита - 0 регистра Х008.Если бит равен 0, выводим сообщение "НЭСМИТ Не готов.", и выходим возвращая значение функции false.
	//	Если kind = 'CS' и state = 1 тогда установить в регистре X001 бит 2 в значение 1
	//	Если kind = 'CS' и state = 0 тогда установить в регистре X001 бит 2 в значение 0
	//	Если kind = 'FF' и state = 1 тогда установить в регистре X001 бит 1 в значение 1
	//	Если kind = 'FF' и state = 0 тогда установить в регистре X001 бит 1 в значение 0
	IMPEXP bool CALLCONV Lamp(const char* kind, unsigned char state)
	{
		std::stringstream ss;
		std::string cap = "НЭСМИТ : Управление лампами";

		//автоматически закроет соединение с прибором при выходе из функции
		NESMITCONNECTION connection(&DEVICE);

		try
		{
			//установка связи с устройством
			DEVICE.connect(g_connectionData);

			//проверка готовности прибора
			if (!DEVICE.clientready())
			{
				ss << "НЭСМИТ не готов к работе." << std::endl;
				MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

				return false;
			}

			//управление лампами
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

	
	//Установить наклон state для n-го диспергирующего устройства спектрографа.
	//Если значение angle менее 8 или больше 30. тогда выходим, возвращая значение функции false.
	//	Делаем чтение бита - 0 регистра Х008.Если бит равен 0, выводим сообщение "НЭСМИТ Не готов.", и выходим возвращая значение функции false.
	//	Делаем чтение бита - 2 регистра Х008.Если бит равен 1. выводим сообщение "НЭСМИТ занят.", и выходим возвращая значение функции false.
	//	Входное значение angle, умножаем на 1000, округляем до целого и записываем в регистр X005 - X006.
	//	В регистр Х007, бит 0 - записать 0.
	//	В регистр Х007, бит 1 - записать 0.
	//	В регистр Х008. бит 2 - записать 0.
	//	Затем показть на экране прогресс - бар, с подписью Движение решетки.
	//	Значение прогрессбара. брать из регистра Х009, где 0 это 0 % , а 255 - это 100 % .
	//	Необходимо чтобы значение обновлялось как минимум 2 раза в секунду.
	//	После того, как в регистре Х008, бит 2 примет значение 1, закрыть окно прогрессбара, и выйти из функции вернув значение true.
	IMPEXP bool CALLCONV Disp(unsigned char state, const char* angle)
	{

		std::stringstream ss;
		std::string cap = "НЭСМИТ : Установить наклон диспергирующего устройства";

		//автоматически закроет соединение с прибором при выходе из функции
		NESMITCONNECTION connection(&DEVICE);

		try
		{
			//установка связи с устройством
			DEVICE.connect(g_connectionData);

			//проверка готовности прибора
			if (!DEVICE.clientready())
			{
				ss << "НЭСМИТ не готов к работе." << std::endl;
				MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

				return false;
			}
			
			//проверка занятости прибора
			if (DEVICE.clientbusy())
			{
				ss << "НЭСМИТ занят другой операцией." << std::endl;
				MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

				return false;
			}
			
			//если в системе испльзуется другой разделитель дробной части
			// она будет отброшена и взята целочисленная часть 12,5 при использовании точки
			// переведется в 12 а при использовании запятой в 12.5
			//приводим к внутреннему значению прибора
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

		//отображаем окно прогресса
		ShowProgressWnd();
		
		//пока прибор занят установкой угла выводим текущий прогресс
		while (DEVICE.clientbusy()) 
		{
			//ожидаем
			Sleep(CYCLE_OPERATION_DELAY);
			
			//обновляем гобальный прогресс для отображения в окне
			g_currentProgress = static_cast<int>(DEVICE.progress());
			
			//обновляем окно прогресса
			UpdateWindow(g_hwndProgressDlg);			
		}

		//закрываем окно прогресса
		CloseProgressWnd();
		
		return true;
	}

	//Установить ширину щели спектрографа равной state.
	IMPEXP bool CALLCONV Slit(const char* state)
	{
		std::stringstream ss;
		std::string cap = "НЭСМИТ : Диспергирующее устройство";		

		return true;
	}

	//Выполняет все необходимые действия по подготовке спектрографа к выключению, а библиотеки к выгружению из памяти.
	IMPEXP bool CALLCONV CloseInst()
	{
		std::stringstream ss;
		std::string cap = "НЭСМИТ : Освобождение ресурсов";		
		
		//автоматически закроет соединение с прибором при выходе из функции
		NESMITCONNECTION connection(&DEVICE);

		try
		{
			//установка связи с устройством
			DEVICE.connect(g_connectionData);

			//освобождение устройства
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
	//Выполняет калбровку нуля
	IMPEXP bool CALLCONV GetZero()
	{
		std::stringstream ss;
		std::string cap = "НЭСМИТ : Калибровка нуля";


		//автоматически закроет соединение с прибором при выходе из функции
		NESMITCONNECTION connection(&DEVICE);

		try
		{
			//проверка готовности прибора
			if (!DEVICE.clientready())
			{
				ss << "НЭСМИТ не готов к работе." << std::endl;
				MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

				return false;
			}
			
			//установка связи с устройством
			DEVICE.connect(g_connectionData);

			//выполнить калибровку
			DEVICE.zero();

			//ожидаем
			while (DEVICE.commandrequested())
				Sleep(CYCLE_OPERATION_DELAY);
		}
		catch (std::exception& ex)
		{
			ss << ex.what() << std::endl;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			return false;
		}
		
		ss << "Калибровка угла наклона решетки завершена..." << std::endl;
		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONINFORMATION);
		
		return true;
	}

	//Выполняет 
	IMPEXP bool CALLCONV SetTick(const char* state)
	{
		std::stringstream ss;
		std::string cap = "SetTick";
		
		return true;
	}
}