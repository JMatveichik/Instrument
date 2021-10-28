// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "instrument.h"
#include "registers.h"
#include "helper.h"
#include "ModbusTCPConnection.h"

#pragma comment(linker, "/EXPORT:InitInst=_InitInst@4")
#pragma comment(linker, "/EXPORT:Shutter=_Shutter@4")
#pragma comment(linker, "/EXPORT:Filter=_Filter@4")
#pragma comment(linker, "/EXPORT:Lamp=_Lamp@8")
#pragma comment(linker, "/EXPORT:Disp=_Disp@8")
#pragma comment(linker, "/EXPORT:Slit=_Slit@4")
#pragma comment(linker, "/EXPORT:CloseInst=_CloseInst@0")
#pragma comment(linker, "/EXPORT:GetZero=_GetZero@4")
#pragma comment(linker, "/EXPORT:SetTick=_SetTick@4")


CModbusTCPConnection dev;
std::ofstream logger;


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
		case DLL_PROCESS_ATTACH: {
		
			//создание экземпляра класса modbus соединения 
			//pConnection = new CModbusTCPConnection();

			WSADATA ws;
			if (::WSAStartup(MAKEWORD(2, 2), &ws) != 0)
			{
				std::cerr << "Unable to initialize sockets" << std::endl;
				return FALSE;
			}

			logger.open("log.txt");

		}
		break;

		case DLL_THREAD_ATTACH:
		{

		}
		break;

		case DLL_THREAD_DETACH:
		{
			logger.close();
		}
		break;

		case DLL_PROCESS_DETACH:
		{
		}
		break;
       
	}
    return TRUE;
}

extern "C" {
	// Записать в регистр X008, бит-0, значение 1. 
	// Если запись прошла успешно, вернуть в функции значение true.
	// запись прошла с ошибкой, выдать сообщение "НЭСМИТ ошибка связи", выдать значение функции false.
	IMPEXP bool CALLCONV  InitInst(const char* path)
	{
		// если передали строку адреса в виде "ipaddress:port" "192.168.10.18:502" 
		bool connected = dev.Establish(path, &logger);

		//если не соеденились пробуем загрузить из файла
		if (!connected)
		{
			std::ifstream input;
			input.open(path);

			connected = dev.Establish(input, &logger);
		}	

		//если не удалось соеденится возвращаем  false  
		if (!connected)
			return false;	


		std::vector<unsigned char> res;
		unsigned short szData[] =
		{
			1,
			1
		};

		dev.Transact(ReadInputRegisters, (char*)szData, sizeof(szData), res, &logger);

		return true;
	}

	//Открывает(state = 1) и закрывает(state = 0) затвор.
	IMPEXP bool CALLCONV Shutter(unsigned char state)
	{
		//if (connection.)
		return false;
	}

	//Устанавливает фильтр разделения порядков с заданным номером n.
	IMPEXP bool CALLCONV Filter(int value)
	{
		return false;
	}

	//Включить (state=1) или выключить (state=0) лампу плоского поля (kind=FF) или спектра сравнения (kind=CS).
	IMPEXP bool CALLCONV Lamp(const char *kind, unsigned char state)
	{
		return false;
	}

	//Установить наклон state для n-го диспергирующего устройства спектрографа.
	IMPEXP bool CALLCONV Disp(unsigned char n, const char *state)
	{
		return false;
	}

	//Установить ширину щели спектрографа равной state.
	IMPEXP bool CALLCONV Slit(const char *state)
	{
		return false;
	}

	//Выполняет все необходимые действия по подготовке спектрографа к выключению, а библиотеки к выгружению из памяти.
	IMPEXP bool CALLCONV CloseInst()
	{
		return false;
	}

	//Выполняет 
	IMPEXP bool CALLCONV GetZero(const char *state)
	{
		return false;
	}

	//Выполняет 
	IMPEXP bool CALLCONV SetTick(const char *state)
	{
		return false;
	}
}