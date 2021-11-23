// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "instrument.h"
#include "registers.h"
#include "helper.h"
#include "resource.h"
#include "lock.h"


//#define NOT_CHECK_CLIETN_BIT

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

//modbus соединение
modbus_t* mb;

//главное окно вызвавшего приложения
HWND g_mainWnd = HWND_DESKTOP;

//переменная отображения прогресса открытия 
int g_currentProgress = 0;

//
HINSTANCE	hInstance = 0;
HMODULE		g_hModule = 0;

//окно отображения прогресса
HWND g_hwndProgressDlg = NULL;

//строка соединения
std::string connectionString;

//директория dll
std::string initdir;

//выходной поток вывода логов
std::ofstream loger;

//максимальное количество попыток чтения регистров
const int MAX_RETRIES = 3;

//ожидание перед запросом Mmodbus, ms
//если присутствует задержка перед выполнением следующей операции
const int MODBUS_OPERATION_DELAY = 100;

//ожидание в цикле для проверки состояния регистра
const int CYCLE_OPERATION_DELAY = 250;

//событие для остановки опроса ПЛК
HANDLE	hStopEvent	= NULL;

//поток чтения регистров
HANDLE	hReadThread = NULL;
DWORD	readThreadID;

//интервал опроса ПЛК
const DWORD	READ_INTERVAL = 500;


//последнее прочитанное состояние регистров 
uint16_t	CONTROL_AND_INPUT_REGISTER = 0;
uint16_t	HIHTPOS_REGISTER = 0;
uint16_t	LOWPOS_REGISTER = 0;
uint16_t	COMMAND_AND_STATUS_REGISTER = 0;
uint16_t	PROGRESS_REGISTER = 0;
uint16_t	VERSION_REGISTER = 0;

//критическая секция обработки даныых
CRITICAL_SECTION workCS;


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
		//закрываем файл логирования
		loger.flush();
		loger.close();

		CloseHandle(hStopEvent);
		DeleteCriticalSection(&workCS);
	}
	break;

	}
	return TRUE;
}


#pragma region ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ РАБОТЫ С ПРИБОРОМ

int connect(std::pair<std::string, int> opt)
{
	std::stringstream ss;	
	ss << "\t => получены параметры соединения ip (" << opt.first << "):port(" << opt.second << ")" << std::endl;

	mb = modbus_new_tcp(opt.first.c_str(), opt.second);
	int connected = modbus_connect(mb);

	if (connected == -1)
		ss << "\t => ошибка соединения ip (" << opt.first << "):port(" << opt.second << ")" << std::endl;
	else
		ss << "\t => соединение установлено ip (" << opt.first << "):port(" << opt.second << ")" << std::endl;

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

	//флаг соединения
	int connected = -1;

	//если не соеденились пробуем загрузить из файла в текущей директории "connect.txt"
	if (connectstring == nullptr)
	{
		std::string fpath = getfilepath("connect.txt");
		ss << "\t => try read connection string from (" << fpath << ")" << std::endl;

		std::ifstream input;
		input.open(fpath);

		if (input.bad()) {
			ss << "\t => файл  ("<< fpath << ") не найден..." << std::endl;
			return false;
		}			

		connected = connect( helper::connection(input) );

		loger << ss.str().c_str() << std::endl;
		loger.flush();

		return (connected == -1) ? false : true;
	}

	try
	{
		// если передали строку адреса в виде "ipaddress:port" "192.168.10.18:502" 
		ss << "\t => try connect from input string ("<< connectstring << ")" << std::endl;
		connected = connect(helper::connection(connectstring));

		loger << ss.str().c_str() << std::endl;
		loger.flush();

	}
	catch (...)
	{
		connected = -1;
	}

	//если не соеденились пробуем загрузить из файла path
	if (connected == -1)
	{
		ss << "\t => try read connection string from file ("<< connectstring << ")" << std::endl;

		std::ifstream input;
		input.open(connectstring);

		if (input.bad()) {
			ss << "\t => файл  ("<< connectstring <<") не найден..." << std::endl;
			return false;
		}

		connected = connect(helper::connection(input));

		loger << ss.str().c_str() << std::endl;
		loger.flush();
	}

	//если не соеденились пробуем загрузить из файла в текущей директории "connect.txt"
	if (connected == -1)
	{
		std::string fpath = getfilepath("connect.txt");
		ss << "\t => try read connection string from (" << fpath << ")" << std::endl;

		std::ifstream input;
		input.open(fpath);

		if (input.bad()) {
			ss << "\t => файл  (" << fpath << ") не найден..." << std::endl;
			return false;
		}

		connected = connect(helper::connection(input));

		loger << ss.str().c_str() << std::endl;
		loger.flush();

		return (connected == -1) ? false : true;

	}

	//если не удалось соеденится возвращаем  false  
	return connected != -1;
}

//попытаться почитать регистры ПЛК
int trygetregisters(int reg, int count, int retries, bool isInput, uint16_t* values)
{
	//входим в критическую секцию
	lock obj(&workCS);

	std::stringstream ss;
	ss << "trygetregister => " << std::endl;

	///количество прочитанных регистров 
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
			ss << "\tПопытка №" << r << " ошибка чтения регистра (" << modbus_strerror(errno) << ")" << std::endl;		
		
	}

	//обновляем логер
	flushlogger(ss);

	return readCount;
}

//установить бит в заданном регистре 
//reg - номер регистра
//bit - номер бита
//state - состояние в кторое нужно переключить бит
int setregisterbit(int reg, unsigned char bit, bool state)
{
	//входим в критическую секцию
	lock obj(&workCS);

	std::stringstream ss;
	ss << "\tsetregisterbit =>" << std::endl;

	///получаем текущее состояние регистра  
	uint16_t value;
	if (trygetregisters(reg, 1, MAX_RETRIES, true, &value) == -1)
	{
		ss << "\t\t=>Ошибка чтения регистра" << std::endl;

		//обновить лог
		flushlogger(ss);

		return -1;
	}		
	
	//установка нужного бита в нужное состояние
	uint16_t output = helper::setbit(value, bit, state);

	//бит уже в заданном состоянии
	if (output == value)
	{
		ss << "\t\t=> нет изменения состояния регистра => текущее : (" << value << ") новое : (" << output << ")" << std::endl;
		
		//обновить лог
		flushlogger(ss);
		
		//бит установлен
		return 1;
	}

	//для лога
	ss << "\t\t=> изменение бита => register (" << reg << ") bit : (" << (int)bit << ") state : (" << state << ") текущее : (" << value << ")" << " новое : (" << output << ")" << std::endl;
	
	//ожидание между запросами Modbus
	Sleep(MODBUS_OPERATION_DELAY);

	//запись в регистр нового значения
	const int writeCount = modbus_write_registers(mb, reg, 1, &output);

	//если запись не удалась 
	if (writeCount == -1) 
		ss << "\t\t=> ошибка записи регистра => (" << modbus_strerror(errno) << ")" << std::endl;
		
	//обновляем логер
	flushlogger(ss);

//если нужно подтверждение записи
#ifndef CONFIRM_SET_BIT
	
	return (writeCount == -1) ? -1 : state;

#else
	//ожидание между запросами Modbus
	Sleep(MODBUS_OPERATION_DELAY);

	//подтверждение записи - получаем регистр заново
	if (trygetregisters(reg, 1, MAX_RETRIES, true, &value) == -1)
	{
		//обновляем логер
		flushlogger(ss);

		return -1;
	}		
	
	//проверям соответствие на установку бита
	const bool isBitSet = helper::checkbitstate(value, bit, state);

	//если бит установлен правильно
	if (isBitSet)
		ss << "\t\t=> bit : (" << bit << ") установлен в заданное состояние state : (" << state << ")" << std::endl;
	else
		ss << "\t\t=> bit : (" << bit << ") не пререключен в состояние  state : (" << state << ")" << std::endl;
	
	//обновляем логер
	flushlogger(ss);
	
	return (int)isBitSet;
#endif

	
}

/*
///установить бит в регистре статуса (X008)
int setstatusbit(unsigned char bit, bool state)
{
	std::stringstream ss;
	ss << "setstatusbit => " << std::endl;

	flushlogger(ss);

	return setregisterbit(CommandAndStatusRegister, bit, state);
}

///установить бит в регистре команд и состояния (X007)
int setcommandbit(unsigned char bit, bool state)
{
	std::stringstream ss;
	ss << "setcommandbit => " << std::endl;
	flushlogger(ss);

	return setregisterbit(CommandAndStatusRegister, bit, state);
}

///установить бит в регистре управления и входов (X001)
int setcontrolbit(unsigned char bit, bool state)
{
	std::stringstream ss;
	ss << "setcontrolbit => " << std::endl;
	flushlogger(ss);

	return setregisterbit(ControlAndInputRegister, bit, state);
}

///проверка бита регистра
int checkregisterbit(int reg, unsigned char bit)
{
	std::stringstream ss;
	ss << "\t=> checkregisterbit => register : (" << reg << ") bit : (" << bit << ")" << std::endl;

	///получаем текущее состояние регистра  
	uint16_t value;
	if (trygetregisters(reg, 1, MAX_RETRIES, true, &value) == -1)
		return -1;	

	bool isSet = helper::checkbit(value, bit);
	return isSet ? 1 : 0;
}

///проверить бит в регистре статуса (X008)
int checkstatusbit(unsigned char bit)
{
	std::stringstream ss;
	ss << "checkstatusbit => " << std::endl;
	flushlogger(ss);

	return checkregisterbit(CommandAndStatusRegister, bit);
}

///проверить бит в регистре управления (X001)
int checkcontrolbit(unsigned char bit)
{
	std::stringstream ss;
	ss << "checkcontrolbit => " << std::endl;
	flushlogger(ss);

	return checkregisterbit(ControlAndInputRegister, bit);
}

//проверить корректность установки бита по результату
bool checksetbitresult(int res, bool expected)
{
	std::stringstream ss;
	ss << "\t\tchecksetbitresult => " << std::endl;

	//ошибка связи
	if (res == -1 )
	{
		ss << "\t\t\t=> Ошибка связи..." << std::endl;
		flushlogger(ss);
		
		return false;
	}
		

	//несоответствиие ожидаемого состояния
	if (res != int(expected))
	{
		ss << "\t\t\t=> Несоответствиие ожидаемого состояния => ожидаемое (" << expected << ")" << " - полученное (" << res << ")" << std::endl;
		flushlogger(ss);

		return false;
	}
		
	ss << "\t\t\t=> Состояние подтверждено => ожидаемое (" << expected << ")" << " - полученное (" << res << ")" << std::endl;
	flushlogger(ss);

	return true;
}
*/

//проверка бита  готовности прибора к работе
//return true - прибор готов
//return false - прибор не готов
bool clientready(std::string cap)
{	
	//вход в критическую секцию
	lock obj(&workCS);	

	//проверяем бит готовности клиента к работе
	if (helper::checkbit(COMMAND_AND_STATUS_REGISTER, Client))
		return true;

	std::stringstream ss;
	ss << "Спектрограф не готов к работе" << std::endl;
	MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

	//обновить логер
	flushlogger(ss);

	return false;	
}

//проверка бита  готовности прибора к работе
//return true - прибор занят выполнением другой операции
//return false - прибор свободен
bool clientbusy(std::string cap)
{
	//вход в критическую секцию
	lock obj(&workCS);

	//проверяем бит занятости прибора 1 - занят 0 - свободен
	if (!helper::checkbit(COMMAND_AND_STATUS_REGISTER, CommandBusy))
		return false;

	std::stringstream ss;
	ss << "Спектрограф занят выполнением операции..." << std::endl;

	MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

	//обновить логер
	flushlogger(ss);

	return true;
}

//записать значение в регистр
int writeregister(int reg, uint16_t value)
{
	//вход в критическую секцию
	lock obj(&workCS);

	std::stringstream ss;
	ss << "writeregister => register (" << reg << ") : value (" << value << ")" << std::endl;

	//запись в регистр нового значения
	int writeCount = -1;

	try 
	{
		writeCount = modbus_write_register(mb, reg, value);
	}
	catch(...)
	{
		writeCount = -1;
	}

	//если запись не удалась 	
	if (writeCount == -1)
		ss << "\tошибка записи (" << modbus_strerror(errno) << ")" << std::endl;
	else
		ss << "\tзаписано новое значение (" << value << ")" << std::endl;

	//обновить логер
	flushlogger(ss);

	return writeCount;
}

//записать значение угла в регистр
int writeangle(uint16_t angle)
{
	//вход в критическую секцию
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

	OutputDebugString("Поток чтения регистров запущен...\n");
	
	//в цикле читаем данные из регистров
	while (true)
	{
		if (WaitForSingleObject(hStopEvent, 0) == 0)
			break;

		uint16_t regs[16];
		int count = 6;
		
		EnterCriticalSection(&workCS);

		//читаем регистры и запоолняем переменные
		if (trygetregisters(0, count, MAX_RETRIES, true, regs))
		{		

			//Регистр управления 8 bit  и регистр входов 8 bit
			CONTROL_AND_INPUT_REGISTER = regs[0];
			
			//Верхний регистр положения
			HIHTPOS_REGISTER	= regs[1];

			//Нижний регистр положения
			LOWPOS_REGISTER		= regs[2];

			//Командный регистр 8 bit и Регистр состояний 8 bit
			COMMAND_AND_STATUS_REGISTER = regs[3];
			
			//Регистр прогресса
			PROGRESS_REGISTER = regs[4];

			//Регистр версии
			VERSION_REGISTER = regs[5];
			
		}
		else
		{		

			//Регистр управления 8 bit  и регистр входов 8 bit
			CONTROL_AND_INPUT_REGISTER = -1;

			//Верхний регистр положения
			HIHTPOS_REGISTER = -1;

			//Нижний регистр положения
			LOWPOS_REGISTER = -1;

			//Командный регистр 8 bit и Регистр состояний 8 bit
			COMMAND_AND_STATUS_REGISTER = -1;

			//Регистр прогресса
			PROGRESS_REGISTER = -1;

			//Регистр версии
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
	
	OutputDebugString("Поток чтения регистров остановлен...\n");
}

extern "C" {

	// Записать в регистр X008, бит-0, значение 1. 
	// Если запись прошла успешно, вернуть в функции значение true.
	// запись прошла с ошибкой, выдать сообщение "НЭСМИТ ошибка связи", выдать значение функции false.
	IMPEXP bool CALLCONV  InitInst(const char* path)
	{
		if (hReadThread != nullptr)
			return true;

		//сохраняем строки соединение
		connectionString = (path == nullptr) ? "" : path;
		
		bool isConnected = connect(connectionString.c_str());

		std::stringstream ss;
		std::string cap = "НЭСМИТ : Инициализация";

		///соединение со спектрографом
		if (!isConnected) {

			ss << "Ошибка соединения со  спректрографом НЕСМИТ по строке - " << ((path == nullptr) ? "null" : path) << std::endl;
			ss << "Выполните  следующее :" << std::endl;
			ss << "  - Убедитесь что питание прибора включено" << std::endl;
			ss << "  - Убедитесь что прибор подключен к локальной сети" << std::endl;
			ss << "  - Отредактируйте файл <connect.txt> в соответствии с реальным адресом устройства (IP:PORT )" << std::endl;

			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			
			//обновление лога
			flushlogger(ss);

			return false;
		}		

		//создаем поток чтения регистров
		hReadThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)readdata, 0, 0, &readThreadID);
		
		//задержка перед операцией
		Sleep(MODBUS_OPERATION_DELAY);


		//---------------------------------------------------------------------------------
		//ГОТОВИМ ВЫХОДНОЕ ЗНАЧЕНИЕ РЕГИСТРА

		//вход в критическую секцию
		EnterCriticalSection(&workCS);

		// В регистр Х007, бит 0 - записать 1.
		uint16_t status = helper::setbit(COMMAND_AND_STATUS_REGISTER, Client, true);		

		//выход из критической секции
		LeaveCriticalSection(&workCS);

		//записываем значение в регистр
		if (writeregister(CommandAndStatusRegister, status) == -1)
		{
			
			ss << "Ошибка связи со спректрографом НЕСМИТ." << std::endl;
			ss << " - Задание готовности клиента к работе (Client)" << std::endl;

			//сообщение
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			//обновление лога
			flushlogger(ss);

			return false;
		}
		
		return true;
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

		//проверка готовности прибора
		if (!clientready(cap))
			return false;


		///Ошибка передачи параметра состояния затвора
		if (state > 1)
		{
			ss << "Ошибка параметра состояния затвора : " << state;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			
			flushlogger(ss);

			return false;
		}

		//
		Sleep(MODBUS_OPERATION_DELAY);


		//---------------------------------------------------------------------------------
		//ГОТОВИМ ВЫХОДНОЕ ЗНАЧЕНИЕ РЕГИСТРА

		//вход в критическую секцию
		EnterCriticalSection(&workCS);

		// В регистр Х007, бит 0 - записать 1.
		uint16_t status = helper::setbit(CONTROL_AND_INPUT_REGISTER, ShutterBit, (state == 1));

		//выход из критической секции
		LeaveCriticalSection(&workCS);

		//записываем значение в регистр
		if (writeregister(ControlAndInputRegister, status) == -1)
		{

			ss << "Сбой управления затвором." << std::endl;
			ss << " - Задание бита (ShutterBit : " << (state == 1) << ")" << std::endl;

			//сообщение
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			//обновление лога
			flushlogger(ss);

			return false;
		}		

		//ожидаем установку бита заслонки
		while (true) {

			//
			Sleep(MODBUS_OPERATION_DELAY);

			//заходим в критическую секцию
			EnterCriticalSection(&workCS);

			//проверяем бит регистра состояния заслонки  1 - включена 0 - выключена
			if (helper::checkbit(CONTROL_AND_INPUT_REGISTER, ShutterBit) == (bool)state)
			{
				LeaveCriticalSection(&workCS);
				break;
			}
			LeaveCriticalSection(&workCS);
		}

		return true;
	}

	//Устанавливает фильтр разделения порядков с заданным номером n.
	IMPEXP bool CALLCONV Filter(int value)
	{
		std::stringstream ss;
		std::string cap = "Filter";

		//проверка готовности прибора
		if (!clientready(cap))
			return false;

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
	IMPEXP bool CALLCONV Lamp(const char* kind, unsigned char state)
	{
		std::stringstream ss;
		std::string cap = "НЭСМИТ : ";
		std::string type(kind);

		unsigned char bit = LampFFBit;

		//проверка готовности прибора
		if (!clientready(cap))
			return false;


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
			
			//обновляем лог
			flushlogger(ss);

			return false;
		}

		//---------------------------------------------------------------------------------
		//ГОТОВИМ ВЫХОДНОЕ ЗНАЧЕНИЕ РЕГИСТРА

		//вход в критическую секцию
		EnterCriticalSection(&workCS);

		// В регистр Х007, бит 0 - записать 1.
		uint16_t status = helper::setbit(CONTROL_AND_INPUT_REGISTER, bit, (state == 1));

		//выход из критической секции
		LeaveCriticalSection(&workCS);

		//записываем значение в регистр
		if (writeregister(ControlAndInputRegister, status) == -1)
		{

			ss << "Сбой управления " << cap.c_str() << std::endl;
			ss << " - Задание бита ( "<< type.c_str() << " : " << (state == 1) << ")" << std::endl;

			//сообщение
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			//обновление лога
			flushlogger(ss);

			return false;
		}
		
		//ожидаем установку бита лампы
		while (true) {

			//
			Sleep(MODBUS_OPERATION_DELAY);

			//заходим в критическую секцию
			EnterCriticalSection(&workCS);

			//проверяем бит регистра состояния лампы  1 - включена 0 - выключена
			if (helper::checkbit(CONTROL_AND_INPUT_REGISTER, bit) == (bool)state)
			{
				LeaveCriticalSection(&workCS);
				break;
			}
			LeaveCriticalSection(&workCS);
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

		//проверка готовности прибора
		if (!clientready(cap))
			return false;

		//спектрограф занят другой операцией
		if (clientbusy(cap))
			return false;

		//если в системе испльзуется другой разделитель дробной части
		// она будет отброшена и взята целочисленная часть 12,5 при использовании точки
		// переведется в 12 а при использовании запятой в 12.5
		//приводим к внутреннему значению прибора
		const int ang = int(std::atof(angle) * 1000);

		//ошибка задания угла наклона диспергирующего устройства
		if (ang < 8000 || ang > 30000)
		{
			ss << "Ошибка задания угла (от 8° до 30°) : " << ang << std::endl;
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			//обновляем лог
			flushlogger(ss);

			return false;
		}

		
		

		EnterCriticalSection(&workCS);

		//обновляем гобальный прогресс для отображения в окне
		if (ang == LOWPOS_REGISTER)
		{
			
			//выходим из критической секции
			LeaveCriticalSection(&workCS);

			return true;
		}

		//выходим из критической секции
		LeaveCriticalSection(&workCS);

		//ожидание пред следующей операцией
		Sleep(MODBUS_OPERATION_DELAY);

		if (writeangle(ang) == -1 )
		{
			ss << "Сбой задания угла наклона диспергирующего устройства." << std::endl;
			ss << "Функция writeangle ("<< ang << ")." << std::endl;

			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			
			//обновляем лог
			flushlogger(ss);

			return false;
		}

		
		//ожидание пред следующей операцией
		Sleep(MODBUS_OPERATION_DELAY);		
				
		//---------------------------------------------------------------------------------
		//ГОТОВИМ ВЫХОДНОЕ ЗНАЧЕНИЕ РЕГИСТРА
		
		//вход в критическую секцию
		EnterCriticalSection(&workCS);

		// В регистр Х007, бит 0 - записать 0.
		uint16_t status = helper::setbit(COMMAND_AND_STATUS_REGISTER, Angle, false);

		// В регистр Х007, бит 1 - записать 0.
		status = helper::setbit(status, Gap, false);

		// В регистр Х007. бит 2 - записать 0.
		status = helper::setbit(status, StopEngine, false);		
		
		// В регистр Х008. бит 9 - записать 0.
		status = helper::setbit(status, CommandRequest, false);

		LeaveCriticalSection(&workCS);

		//записываем значение в регистр
		if (writeregister(CommandAndStatusRegister, status) == -1)
		{
			ss << "Сбой задания угла наклона диспергирующего устройства. " << std::endl;
			ss << " - Установка бита (Angle) " << std::endl;
			ss << " - Установка бита (Gap) " << std::endl;
			ss << " - Установка бита (StopEngine) " << std::endl;
			ss << " - Установка бита (CommandRequest) " << std::endl;
			
			//сообщение
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);
			
			//обновление лога
			flushlogger(ss);

			return false;
		}
		//---------------------------------------------------------------------------------				
				

		g_currentProgress = 0;

		//отображаем окно прогресса
		ShowProgressWnd();	

		//ожидаем бита занятости
		
		while (true) {

			Sleep(CYCLE_OPERATION_DELAY);

			//заходим в критическую секцию
			EnterCriticalSection(&workCS);

			//проверяем бит регистра состояния о занятости прибора
			//бит регистра состояния о занятости прибора снят-  1 занят 0 свободен
			if (helper::checkbit(COMMAND_AND_STATUS_REGISTER, CommandBusy))
			{
				LeaveCriticalSection(&workCS);
				break;
			}
			LeaveCriticalSection(&workCS);
		}

		
		while (true) 
		{
			//ожидаем
			Sleep(CYCLE_OPERATION_DELAY);

			//заходим в критическую секцию
			EnterCriticalSection(&workCS);

			//проверяем бит регистра состояния о занятости прибора
			//бит регистра состояния о занятости прибора снят-  1 занят 0 свободен
			if (!helper::checkbit(COMMAND_AND_STATUS_REGISTER, CommandBusy))
			{
				LeaveCriticalSection(&workCS);
				break;	
			}					
			
			//обновляем гобальный прогресс для отображения в окне
			g_currentProgress = PROGRESS_REGISTER * 100 / 255;

			//выходим из критической секции
			LeaveCriticalSection(&workCS);

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
		/*
		std::stringstream ss;
		std::string cap = "НЭСМИТ : Диспергирующее устройство";


		ss << "Вызов функции (" << cap << ") : ";
		if (state[0] == 1)
			ss << " => открытие...";
		else if (state[0] == 0)
			ss << " => закрытие...";
		else
			ss << " => ошибка параметра state : " << state;

		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK);
		*/

		return true;
	}

	//Выполняет все необходимые действия по подготовке спектрографа к выключению, а библиотеки к выгружению из памяти.
	IMPEXP bool CALLCONV CloseInst()
	{
		std::stringstream ss;
		std::string cap = "НЭСМИТ : Освобождение ресурсов";		
		
		
		EnterCriticalSection(&workCS);

		// В регистр Х007, бит 0 - записать 0.
		uint16_t status = helper::setbit(COMMAND_AND_STATUS_REGISTER, Client, false);

		LeaveCriticalSection(&workCS);

		
		if  ( writeregister(CommandAndStatusRegister, status) == -1)
		{
			ss << "Сбой связи со спектрографом!";
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			flushlogger(ss);
		}

		while (true) {

			Sleep(CYCLE_OPERATION_DELAY);

			//заходим в критическую секцию
			EnterCriticalSection(&workCS);

			//проверяем бит регистра состояния о занятости прибора
			//бит регистра состояния о занятости прибора снят-  
			if (!helper::checkbit(COMMAND_AND_STATUS_REGISTER, Client))
			{
				LeaveCriticalSection(&workCS);
				break;
			}
			LeaveCriticalSection(&workCS);
		}
		

		if (hReadThread != nullptr)
		{
			//отсановка потока чтения
			SetEvent(hStopEvent);
			while (WaitForSingleObject(hReadThread, INFINITE) != 0);
		
			//сбрасываем событие остановки потока чтения
			ResetEvent(hStopEvent);

			//закрываем дескриптор потока и сбрасывем его
			CloseHandle(hReadThread);
			hReadThread = nullptr;

			//освобождаем  ресурсы modbus
			modbus_close(mb);
			modbus_free(mb);
		}
		

		return true;
	}

	///-------------------------------------------------------
	//Выполняет калбровку нуля
	IMPEXP bool CALLCONV GetZero()
	{
		std::stringstream ss;
		std::string cap = "GetZero";

		//проверка готовности прибора
		if (!clientready(cap))
			return false;		
		

		//ожидание между запросами Modbus
		Sleep(MODBUS_OPERATION_DELAY);


		//---------------------------------------------------------------------------------
		//ГОТОВИМ ВЫХОДНОЕ ЗНАЧЕНИЕ РЕГИСТРА

		//вход в критическую секцию
		EnterCriticalSection(&workCS);

		// В регистр Х007, бит 3 - записать 1.
		uint16_t status = helper::setbit(COMMAND_AND_STATUS_REGISTER, ResetZero, true);
				
		// В регистр Х008. бит 9 - записать 0.
		status = helper::setbit(status, CommandRequest, false);

		LeaveCriticalSection(&workCS);

		
		//записываем значение в регистр
		if (writeregister(CommandAndStatusRegister, status) == -1)
		{
			ss << "Сбой калибровки нуля" << std::endl;
			ss << " - Установка бита (ResetZero) " << std::endl;			
			ss << " - Установка бита (CommandRequest) " << std::endl;

			//сообщение
			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			//обновление лога
			flushlogger(ss);

			return false;
		}
		
		//ожидание между запросами Modbus
		Sleep(MODBUS_OPERATION_DELAY);

		if (writeangle(0) == -1)
		{
			ss << "Сбой связи со спектрографом!" << std::endl;
			ss << " - Установка угла (0) " << std::endl;

			MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONERROR);

			//обновление лога
			flushlogger(ss);
			
			return false;
		}

		//ожидание между запросами Modbus
		Sleep(MODBUS_OPERATION_DELAY);

		
		//ожидаем бита занятости
		while (true) {

			Sleep(CYCLE_OPERATION_DELAY);

			//заходим в критическую секцию
			EnterCriticalSection(&workCS);

			//проверяем бит регистра состояния запросе  на выполнение  команды. 
			//бит регистра состояния о занятости прибора снят-  0 - запрос 1 -  готово

			if (!helper::checkbit(COMMAND_AND_STATUS_REGISTER, CommandRequest))
			{
				LeaveCriticalSection(&workCS);
				break;
			}

			LeaveCriticalSection(&workCS);
		}

		while (true) {

			//ожидаем
			Sleep(CYCLE_OPERATION_DELAY);

			//заходим в критическую секцию
			EnterCriticalSection(&workCS);

			//проверяем бит регистра состояния о занятости прибора
			//бит регистра состояния о занятости прибора снят
			if (helper::checkbit(COMMAND_AND_STATUS_REGISTER, CommandRequest))
			{
				LeaveCriticalSection(&workCS);
				break;
			}
		
			//выходим из критической секции
			LeaveCriticalSection(&workCS);				

			
		}
		
		
		ss << "Калибровка угла наклона решетки завершена..." << std::endl;
		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK | MB_ICONINFORMATION);

		//ОБНОВЛЕНИЕ ЛОГА
		flushlogger(ss);		

		return true;
	}

	//Выполняет 
	IMPEXP bool CALLCONV SetTick(const char* state)
	{
		std::stringstream ss;
		std::string cap = "SetTick";

		ss << "Вызов функции (" << cap << ") : " << "не используется";

		MessageBox(g_mainWnd, ss.str().c_str(), cap.c_str(), MB_OK);
		return true;
	}
}