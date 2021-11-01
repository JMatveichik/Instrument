// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "instrument.h"
#include "registers.h"
#include "helper.h"
#include "resource.h"


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
/*		ГЛОБАЛЬНЫЕ ПЕРМЕННЫЕ			*/
//////////////////////////////////////////

//modbus соединение
modbus_t *mb;
HWND g_mainWnd = HWND_DESKTOP;

int g_currentProgress = 0;
HINSTANCE hInstance = 0;
HMODULE g_hModule = 0;
HWND g_hwndProgressDlg = NULL;
//---------------------------------------------------------


#pragma region ОКНО ПРОГРЕССА

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
			ss << "Прогресс " << g_currentProgress << " % ";
			SetDlgItemText(hwnd, IDC_HEADER, ss.str().c_str());


			ss << "WM_TIMER with handle : " << std::hex << hwnd << std::endl;
			OutputDebugString(ss.str().c_str());
		}

		break;

		case WM_INITDIALOG:
		{
			std::stringstream ss;
			ss << "Прогресс " << g_currentProgress << " % ";
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
	return TRUE;
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
			
		}
		break;

		case DLL_PROCESS_DETACH:
		{
		}
		break;
       
	}
    return TRUE;
}


#pragma region ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ РАБОТЫ С ПРИБОРОМ

bool connect(const char* connectstring)
{
	//флаг соединения
	int connected = -1;

	//если не соеденились пробуем загрузить из файла в текущей директории "connect.txt"
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

	// если передали строку адреса в виде "ipaddress:port" "192.168.10.18:502" 
	std::pair<std::string, int> opt = helper::connection(connectstring);
	mb = modbus_new_tcp(opt.first.c_str(), opt.second);
	connected = modbus_connect(mb);

	//если не соеденились пробуем загрузить из файла path
	if (connected == -1)
	{
		std::ifstream input;
		input.open(connectstring);

		std::pair<std::string, int> opt = helper::connection(input);
		mb = modbus_new_tcp(opt.first.c_str(), opt.second);
		connected = modbus_connect(mb);
	}

	//если не соеденились пробуем загрузить из файла в текущей директории "connect.txt"
	if (connected == -1)
	{
		std::ifstream input;
		input.open("connect.txt");

		std::pair<std::string, int> opt = helper::connection(input);
		mb = modbus_new_tcp(opt.first.c_str(), opt.second);
		connected = modbus_connect(mb);
	}

	//если не удалось соеденится возвращаем  false  
	if (connected == -1)
		return false;

	return true;
}

//получение регистра
bool  getregister(int reg, uint16_t& value)
{
	///получаем текущее состояние регистра  
	uint16_t regs[16];
	int readCount = modbus_read_registers(mb, reg, 1, regs);


	//если не удалось выход 
	if (readCount == -1)
		return false;

	value = regs[0];
	return true;
}

//установить бит в заданном регистре 
bool setregisterbit(int reg, unsigned char bit, bool state)
{
	///получаем текущее состояние регистра  
	uint16_t tab_reg[16];
	int readCount = modbus_read_registers(mb, reg, 1, tab_reg);

	//если не удалось выход 
	if (readCount == -1)
		return false;

	//установка нужного бита в нужное состояние
	uint16_t output = helper::setbit(tab_reg[0], bit, state);

	//запись в регистр нового значения
	int writeConut = modbus_write_registers(mb, reg, 1, &output);

	//если запись не удлась 
	if (writeConut == -1)
		return false;

	//подтверждение записи получаем регистр заново
	readCount = modbus_read_registers(mb, reg, 1, tab_reg);

	//если не удалось выход 
	if (readCount == -1)
		return false;

	return true;//helper::checkbitstate(tab_reg[0], bit, state);
}

///установить бит в регистре статуса (X008)
bool setstatusbit(unsigned char bit, bool state)
{
	return setregisterbit(StatusRegister, bit, state);
}

///установить бит в регистре команд (X007)
bool setcommandbit(unsigned char bit, bool state)
{
	return setregisterbit(CommandRegister, bit, state);
}

///установить бит в регистре управления (X001)
bool setcontrolbit(unsigned char bit, bool state)
{
	return setregisterbit(ControlRegister, bit, state);
}

///проверка бита регистра
bool checkregisterbit(int reg, unsigned char bit)
{
	///получаем текущее состояние регистра  
	uint16_t tab_reg[16];
	int readCount = modbus_read_registers(mb, reg, 1, tab_reg);

	//если не удалось выход 
	if (readCount == -1)
		return false;

	return helper::checkbit(tab_reg[0], bit);
}

///проверить бит в регистре статуса (X008)
bool checkstatusbit(unsigned char bit)
{
	return checkregisterbit(StatusRegister, bit);
}

///проверить бит в регистре управления (X001)
bool checkcontrolbit(unsigned char bit)
{
	return checkregisterbit(ControlRegister, bit);
}


bool writeangle(int angle)
{
	///получаем текущее состояние регистра  
	uint16_t tab_reg[16];
	
	/*int readCount = modbus_read_registers(mb, PositionLowRegister, 2, tab_reg);

	//если не удалось выход 
	if (readCount == -1)
		return false;
	*/

	tab_reg[0] = HIWORD(angle);
	tab_reg[1] = LOWORD(angle);
	
	//запись в регистр нового значения
	int writeConut = modbus_write_registers(mb, PositionLowRegister, 2, tab_reg);

	//если запись не удлась 
	if (writeConut == -1)
		return false;

	return true;
}


#pragma endregion


extern "C" {
	// Записать в регистр X008, бит-0, значение 1. 
	// Если запись прошла успешно, вернуть в функции значение true.
	// запись прошла с ошибкой, выдать сообщение "НЭСМИТ ошибка связи", выдать значение функции false.
	IMPEXP bool CALLCONV  InitInst(const char* path)
	{
		std::stringstream ss;
		std::string cap = "НЭСМИТ : Инициализация";

		///соединение со спектрографом
		if (!connect(path)) {

			ss << "Ошибка соединения со  спректрографом НЕСМИТ. Выполните  следующее :" << std::endl;
			ss << "\t- Убедитесь что питание прибора влючено" << std::endl;
			ss << "\t- Убедитесь что прибора подключен к локальной сети" << std::endl;
			ss << "\t- Отредактируйте файл <connect.txt> в соответствии с реальным адресом устройства (IP:PORT )" << std::endl;

			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK|MB_ICONERROR);
		}

		//очищаем 
		ss.str(std::string());

		/*
		bool busy = checkstatusbit(CommandBusy);
		//временно---------------------------------
		setstatusbit(CommandBusy, false);
		//-----------------------------------------
		busy = checkstatusbit(CommandBusy);
		*/
	
		// Записать в регистр X008, бит-0, значение 1.
		if (setstatusbit(Client, true))
			return true;

		ss << "Ошибка связи со спректрографом НЕСМИТ." << std::endl;
		ss << "  - Задание готовности клиента к работе (X008 -> CLCNT)" << std::endl;

		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
		return false;
	}

	//Открывает(state = 1) и закрывает(state = 0) затвор.
	//Делаем чтение бита - 0 регистра Х008.Если бит равен 0, выводим сообщение "НЭСМИТ Не готов.", и выходим возвращая значение функции false.
	//При значении state = 1 в регистр X001 бит 0, записать значение 1.
	//При значении state = 0 в регистр X001 бит 0, записать значение 0.
	//При успешном выполнении команды записи, вернуть значение функции true.
	//При ошибки записи, вывести сообщение "Сбой управления Shutter", вернутьь значение функции false.
	IMPEXP bool CALLCONV Shutter(unsigned char state)
	{
		std::stringstream ss;
		std::string cap = "НЭСМИТ : Управление затвором";

		if (!checkstatusbit(Client)) {
			ss << "Спектрограф не готов к работе";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

		///Ошибка передачи параметра состояния затвора
		if (state > 1) 
		{
			ss << "Ошибка параметра состояния затвора : " << state;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

		//запись бита в регистр управления
		if (!setcontrolbit(ShutterBit, (state == 1)))
		{
			ss << "Сбой управления затвором";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

		return true;
	}

	//Устанавливает фильтр разделения порядков с заданным номером n.
	IMPEXP bool CALLCONV Filter(int value)
	{
		std::stringstream ss;
		std::string cap = "Filter";

		ss << "Вызов функции (" << cap << ") : " << value;		

		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK);

		return false;
	}

	//Включить (state=1) или выключить (state=0) лампу плоского поля (kind=FF) или спектра сравнения (kind=CS).
	//kind - строковая переменная типа PChar
	//	if (kind<>'CS') and (kind<>'FF') Then Exit;
	//Делаем чтение бита - 0 регистра Х008.Если бит равен 0, выводим сообщение "НЭСМИТ Не готов.", и выходим возвращая значение функции false.
	//	Если kind = 'CS' и state = 1 тогда установить в регистре X001 бит 2 в значение 1
	//	Если kind = 'CS' и state = 0 тогда установить в регистре X001 бит 2 в значение 0
	//	Если kind = 'FF' и state = 1 тогда установить в регистре X001 бит 1 в значение 1
	//	Если kind = 'FF' и state = 0 тогда установить в регистре X001 бит 0 в значение 1
	IMPEXP bool CALLCONV Lamp(const char *kind, unsigned char state)
	{
		std::stringstream ss;
		std::string cap = "НЭСМИТ : ";
		std::string type(kind) ;
		
		unsigned char bit = LampFFBit;

		if (!checkstatusbit(Client)) {
			ss << "Спектрограф не готов к работе";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

		///Ошибка передачи параметра состояния лампы
		if (state > 1)
		{
			ss << "Ошибка параметра состояния лампы : " << state;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

		if (type == "FF") 
		{
			cap += "Лампа плоского поля";
			bit = LampFFBit;
		}
		else if (type == "CS") 
		{
			cap += "Лампа спектра сравнения";
			bit = LampCSBit;
		}
		else 
		{
			ss << "Ошибка параметра идентификатора лампы : " << type;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}
			
		if (!setcontrolbit(bit, (state == 1)))
		{
			ss << "Сбой управления лампой";
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
	//	Значение прогрессбара. брать из регистра Х009, где о это 0 % , а 255 - это 100 % .
	//	Необходимо чтобы значение обновлялось как минимум 2 раза в секунду.
	//	После того, как в регистре Х008, бит 2 примет значение 1, закрыть окно прогрессбара, и выйти из функции вернув значение true.

	IMPEXP bool CALLCONV Disp(unsigned char state, const char* angle)
	{
		std::stringstream ss;
		std::string cap = "НЭСМИТ : Диспергирующее устройство";

		//спектрограф не готов к работе
		if (!checkstatusbit(Client)) {
			ss << "Спектрограф не готов к работе";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

		//спектрограф занят другой операцией
		/*bool busy = checkstatusbit(CommandBusy);
		if (busy) {
			ss << "Спектрограф занят выполнением операции";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}
		*/
		
		int ang = std::atoi(angle);
		//ошибка задания угла наклона диспергирующего устройства
		if (ang < 8 || ang > 30)
			return false;	

		//приводим к внутреннему значению прибора
		ang *= 1000;
		
		if (!writeangle(ang))
		{
			ss << "Сбой задания угла наклона диспергирующего устройства";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

		//	В регистр Х007, бит 0 - записать 0.
		//	В регистр Х007, бит 1 - записать 0.
		//	В регистр Х008. бит 2 - записать 0.
		if (!setcommandbit(Angle, false) ||
			!setcommandbit(Gap, false) ||
			!setstatusbit(CommandBusy, false))
		{
			ss << "Сбой задания угла наклона диспергирующего устройства. ";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			return false;
		}

		uint16_t progress = 0;
		bool complete = false;
		

		
		ShowProgressWnd();

		while (!complete) {

			//ожидаем
			Sleep(250);

			getregister(ProgressRegister, progress);

			complete = checkstatusbit(CommandBusy);

			g_currentProgress = progress * 100 / 256;

			UpdateWindow(g_hwndProgressDlg);
		}
			
		CloseProgressWnd();

		return true;
	}
	

	//Установить ширину щели спектрографа равной state.
	IMPEXP bool CALLCONV Slit(const char *state)
	{
		std::stringstream ss;
		std::string cap = "Slit";


		ss << "Вызов функции (" << cap << ") : ";
		if (state[0] == 1)
			ss << " => открытие...";
		else if (state[0] == 0)
			ss << " => закрытие...";
		else
			ss << " => ошибка параметра state : " << state;

		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK);

		return false;
	}

	//Выполняет все необходимые действия по подготовке спектрографа к выключению, а библиотеки к выгружению из памяти.
	IMPEXP bool CALLCONV CloseInst()
	{
		std::stringstream ss;
		std::string cap = "CloseInst";

		ss << "Вызов функции (" << cap << ") : " << "отключчение от спектрографа";

		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK);

		return false;
	}

	//Выполняет 
	IMPEXP bool CALLCONV GetZero(const char *state)
	{
		std::stringstream ss;
		std::string cap = "GetZero";

		ss << "Вызов функции (" << cap << ") : " << "калибровка нуля";

		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK);
		return true;
	}

	//Выполняет 
	IMPEXP bool CALLCONV SetTick(const char *state)
	{
		std::stringstream ss;
		std::string cap = "SetTick";

		ss << "Вызов функции (" << cap << ") : " << "не используется";

		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK);
		return true;
	}
}