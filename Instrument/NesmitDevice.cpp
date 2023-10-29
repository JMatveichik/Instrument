#include "pch.h"

#include "NesmitDevice.h"
#include "deviceexception.h"
#include "registers.h"
#include "helper.h"
#include "lock.h"


//----------------------------------------------------

NesmitDevice::NesmitDevice() : _connection(nullptr)
{
	 
	//выделяем ресурсы для соединения
	::InitializeCriticalSection(&_nesvitCS);
}

//----------------------------------------------------

NesmitDevice::~NesmitDevice()
{
	disconect();
	::DeleteCriticalSection(&_nesvitCS);
}

//----------------------------------------------------

void NesmitDevice::setlogger(ILogger* pLogger)
{
	logger = pLogger;
}

//----------------------------------------------------

bool NesmitDevice::connect(const MODBUS_CONNECTION_DATA& options)
{
	return connect(options.first, options.second);	
}

//----------------------------------------------------

bool NesmitDevice::connect(const std::string& ip, const int& port)
{
	// Создаем новый контекст Modbus для TCP
	_connection = modbus_new_tcp(ip.c_str(), port);

	if (_connection == nullptr)
	{
		throw device_connection_exception("Не удалось создать контекст Modbus", "modbus_new_tcp" , ip, port);
	}

	// Открываем соединение
	const int connected = modbus_connect(_connection);
	if (connected == -1)
	{
		throw device_connection_exception("Oшибка соединения c прибором", "modbus_connect", ip, port);
	}
	
	// Устанавливаем Modbus адрес устройства (обычно это не требуется для Modbus TCP)
	modbus_set_slave(_connection, 1);

	//конфигурируем параметры соединения
	//configurate();
	
	return true;
}

//----------------------------------------------------

void NesmitDevice::disconect()
{
	//освобождаем  ресурсы modbus
	modbus_close(_connection);
	modbus_free(_connection);
	_connection = nullptr;
}

//----------------------------------------------------

void NesmitDevice::configurate()
{
	uint32_t old_response_to_sec;
	uint32_t old_response_to_usec;

	/* Save original timeout */
	modbus_get_response_timeout(_connection, &old_response_to_sec, &old_response_to_usec);

	/* Define a new and too short timeout! */
	modbus_set_response_timeout(_connection, 0, 0);

	uint32_t to_sec;
	uint32_t to_usec;

	/* Save original timeout */
	modbus_get_indication_timeout(_connection, &to_sec, &to_usec);
	modbus_set_indication_timeout(_connection, 0, 0);
}

//----------------------------------------------------
// В регистр Х007, бит 0 - записать 1.
void NesmitDevice::initialize()
{
	//получаем регистр команд и состояния
	uint16_t COMMAND_AND_STATUS_REGISTER = getregister(CommandAndStatusRegister);
	
	// В регистр Х007, бит 0 - записать 1.
	uint16_t status = bit::setbit(COMMAND_AND_STATUS_REGISTER, Client, true);

	
	//записываем значение в регистр
	if (writeregister(CommandAndStatusRegister, status) == -1)
	{
		std::stringstream ss;
		
		ss << "Ошибка связи со спректрографом НЕСМИТ." << std::endl;
		ss << " - Задание готовности клиента к работе (Client)" << std::endl;

		throw device_exception(ss.str(), "NesmitDevice::initialize()");
	}
}

//----------------------------------------------------
//проверка бита  готовности прибора к работе
//return true - прибор готов
//return false - прибор не готов
bool NesmitDevice::clientready() const 
{
	//получаем регистр команд и состояния
	uint16_t COMMAND_AND_STATUS_REGISTER = getregister(CommandAndStatusRegister);
	
	//проверяем бит готовности клиента к работе
	if (bit::checkbit(COMMAND_AND_STATUS_REGISTER, Client))
		return true;

	return false;
}

//----------------------------------------------------
//проверка бита  занятости прибора
//return true - прибор занят выполнением другой операции
//return false - прибор свободен
bool NesmitDevice::clientbusy() const
{
	//получаем регистр команд и состояния
	uint16_t COMMAND_AND_STATUS_REGISTER = getregister(CommandAndStatusRegister);

	//проверяем бит занятости прибора 1 - занят 0 - свободен
	if (!bit::checkbit(COMMAND_AND_STATUS_REGISTER, CommandBusy))
		return false;

	return true;
}

//----------------------------------------------------
//проверка бита  запроса команды на выполнение
//return true - запрос на выполнение команды
//return false - готово
bool NesmitDevice::commandrequested() const
{
	//получаем регистр команд и состояния
	uint16_t COMMAND_AND_STATUS_REGISTER = getregister(CommandAndStatusRegister);
	
	//проверка бита  запроса команды на выполнение
	if (!bit::checkbit(COMMAND_AND_STATUS_REGISTER, CommandRequest))
		return true;
	
	return false;
}


//записать значение в регистр
int NesmitDevice::writeregister(int reg, uint16_t value)
{
	//вход в критическую секцию
	lock obj(&_nesvitCS);

	std::stringstream ss;
	ss << "writeregister => register (" << reg << ") : value (" << value << ")" << std::endl;

	//запись в регистр нового значения
	int writeCount = -1;

	try
	{
		writeCount = modbus_write_register(_connection, reg, value);
	}
	catch (...)
	{
		writeCount = -1;
	}

	//если запись не удалась 	
	if (writeCount == -1)
		ss << "\tошибка записи (" << modbus_strerror(errno) << ")" << std::endl;
	else
		ss << "\tзаписано новое значение (" << value << ")" << std::endl;

	//обновить логер
	//flushlogger(ss);

	return writeCount;
}


//Прочитать регистр ПЛК
uint16_t NesmitDevice::getregister(int reg, int retries/* = MAX_RETRIES*/, bool isInput/* = true */) const
{
	uint16_t value;	
	trygetregisters(reg, ONE, &value, retries, isInput);
	return value;
}


//Попытаться почитать регистры ПЛК
//reg   - адрес регистра
//count - число регистров для чтения
//retries - количество попыток для чтения
//isInput - input or holding  register
//values - прочитанное состояние регистров
int NesmitDevice::trygetregisters(int reg, int count, uint16_t* values, int retries/* = MAX_RETRIES*/, bool isInput/* = true */) const
{
	//входим в критическую секцию
	lock obj(&_nesvitCS);

	std::stringstream ss;
	ss << "trygetregister => " << std::endl;

	///результат чтения регистров 
	int result  = -1;

	//попытка
	int retrie = 0;
	
	while (result < 0)
	{
		if (isInput)
		{
			ss << "\tmodbus_read_input_registers =>  register from : (" << reg << ")" << " Count (" << count << ")" << std::endl;
			result = modbus_read_input_registers(_connection, reg, count, values);			
		}
		else
		{
			ss << "\tmodbus_read_registers =>  register from : (" << reg << ")" << " Count (" << count << ")" << std::endl;
			result = modbus_read_registers(_connection, reg, count, values);
		}
		
		Sleep(MODBUS_OPERATION_DELAY);

		if (retrie++ >= retries)
			break;

		if (result < 0) 
		{
			ss << "\tПопытка №" << retrie << " ошибка чтения регистра (" << modbus_strerror(errno) << ")" << std::endl;
		}
	}

	return result;
}


//установить бит в заданном регистре 
//reg - номер регистра
//bit - номер бита
//state - состояние в кторое нужно переключить бит
int NesmitDevice::setregisterbit(int reg, unsigned char bit, bool state)
{
	//входим в критическую секцию
	lock obj(&_nesvitCS);

	std::stringstream ss;
	ss << "\tsetregisterbit =>" << std::endl;

	///получаем текущее состояние регистра  
	uint16_t value;
	if (trygetregisters(reg, ONE, &value, MAX_RETRIES, true) == -1)
	{
		ss << "\t\t=>Ошибка чтения регистра" << std::endl;

		//обновить лог
		//flushlogger(ss);

		return -1;
	}

	//установка нужного бита в нужное состояние
	uint16_t output = bit::setbit(value, bit, state);

	//бит уже в заданном состоянии
	if (output == value)
	{
		ss << "\t\t=> нет изменения состояния регистра => текущее : (" << value << ") новое : (" << output << ")" << std::endl;

		//обновить лог
		//flushlogger(ss);

		//бит установлен
		return 1;
	}

	//для лога
	ss << "\t\t=> изменение бита => register (" << reg << ") bit : (" << (int)bit << ") state : (" << state << ") текущее : (" << value << ")" << " новое : (" << output << ")" << std::endl;

	//ожидание между запросами Modbus
	Sleep(MODBUS_OPERATION_DELAY);

	//запись в регистр нового значения
	const int writeCount = modbus_write_registers(_connection, reg, 1, &output);

	//если запись не удалась 
	if (writeCount == -1)
		ss << "\t\t=> ошибка записи регистра => (" << modbus_strerror(errno) << ")" << std::endl;

	//обновляем логер
	//flushlogger(ss);

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
	const bool isBitSet = bit::checkbitstate(value, bit, state);

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

bool NesmitDevice::waitforsetbit(int reg, unsigned char bit, bool state, int maxtime) const
{
	//вход в критическую секцию
	lock obj(&_nesvitCS);
	
	// Определите переменную для хранения времени начала ожидания
	auto startTime = std::chrono::high_resolution_clock::now();

	while (true) 
	{
		// Вычислите прошедшее время с момента начала ожидания
		auto currentTime = std::chrono::high_resolution_clock::now();
		auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();

		// Проверьте, не превысило ли прошедшее время максимальное время ожидания
		if (elapsedTime >= maxtime)
		{
			// Выход из цикла по тайм-ауту
			return false;
		}

		Sleep(MODBUS_OPERATION_DELAY);

		uint16_t REGISTER = getregister(reg);

		if (bit::checkbit(REGISTER, bit) == static_cast<bool>(state)) 
		{
			return true;
		}
	}	
}

/// Открывает(state = 1) и закрывает(state = 0) затвор.
void NesmitDevice::shutter(unsigned char state)
{
	std::stringstream ss;
	
	///Ошибка передачи параметра состояния затвора
	if (state > 1 || state < 0)
	{
		ss << "Ошибка параметра состояния затвора : " << state << std::endl;
		std::invalid_argument(ss.str().c_str());

		//flushlogger(ss);
	}

	//вход в критическую секцию
	lock obj(&_nesvitCS);

	//получаем регистр команд и состояния
	uint16_t CONTROL_AND_INPUT_REGISTER = getregister(ControlAndInputRegister);
	
	// В регистр Х007, бит 0 - записать 1.
	uint16_t status = bit::setbit(CONTROL_AND_INPUT_REGISTER, ShutterBit, (state == 1));
		

	//записываем значение в регистр
	if (writeregister(ControlAndInputRegister, status) == -1)
	{

		ss << "Сбой управления затвором." << std::endl;
		ss << " - Задание бита (ShutterBit : " << (state == 1) << ")" << std::endl;

		throw device_exception(ss.str(), "NesmitDevice::shutter()");
	}


	//--------------------------------
	//ожидаем установку бита заслонки
	
	if (!waitforsetbit(ControlAndInputRegister, ShutterBit, state , MAX_WAIT_SET_BIT_TIME))
	{
		ss << "Сбой управления затвором." << std::endl;
		ss << " - Задание бита (ShutterBit : " << (state == 1) << ")" << std::endl;
		ss << "Бит не был установлен!" << std::endl;

		throw device_exception(ss.str(), "NesmitDevice::shutter()");
	}
}


/// Устанавливает фильтр разделения порядков с заданным номером n.
void NesmitDevice::filter(int value)
{
	throw std::exception("filter : Not implemented yet...");
}

/// Включить (state=1) или выключить (state=0) лампу плоского поля (kind=FF) или спектра сравнения (kind=CS).
void NesmitDevice::lamp(const char* kind, unsigned char state)
{
	std::stringstream ss;
	std::string type(kind);

	///Ошибка передачи параметра идентификатора лампы
	if (type != "CS" && type != "FF")
	{
		ss << "Неверный идентификатор лампы : " << type << std::endl;
		throw std::invalid_argument(ss.str().c_str());
	}

	///Ошибка передачи параметра состояния затвора
	if (state > 1 || state < 0)
	{
		ss << "Ошибка параметра состояния лампы : " << state << std::endl;
		std::invalid_argument(ss.str().c_str());
	}

	//
	unsigned char bit   = (type == "FF") ? LampFFBit : LampCSBit;
	std::string lamp    = (type == "FF") ? "Лампа плоского поля (kind = FF)" : "Лампа спектра сравнения(kind = CS)";
	std::string bitname = (type == "FF") ? "LampFFBit" : "LampCSBit";
	
	//вход в критическую секцию
	lock obj(&_nesvitCS);

	//получаем регистр команд и состояния
	uint16_t CONTROL_AND_INPUT_REGISTER = getregister(ControlAndInputRegister);
	
	// В регистр Х007, бит 0 - записать 1.
	uint16_t status = bit::setbit(CONTROL_AND_INPUT_REGISTER, bit, (state == 1));


	//записываем значение в регистр
	if (writeregister(ControlAndInputRegister, status) == -1)
	{
		ss << "Сбой управления " << lamp << std::endl;
		ss << " - Задание бита (" << bitname << ":" << (state == 1) << ")" << std::endl;

		throw device_exception(ss.str(), "NesmitDevice::lamp()");
	}

	//--------------------------------
	//ожидаем установку бита лампы
	if (!waitforsetbit(ControlAndInputRegister, bit, state, MAX_WAIT_SET_BIT_TIME))
	{
		ss << "Сбой управления " << lamp << std::endl;
		ss << " - Задание бита (" << bitname << ":" << (state == 1) << ")" << std::endl;
		ss << "Бит не был установлен!" << std::endl;

		throw device_exception(ss.str(), "NesmitDevice::lamp()");
	}
}

//Установить ширину щели спектрографа равной state.
void NesmitDevice::slit(const char* state)
{
	throw std::exception("slit: Not implemented yet...");
}


//Выполняет калбровку нуля
void NesmitDevice::zero()
{
	std::stringstream ss;

	//вход в критическую секцию
	lock obj(&_nesvitCS);

	//получаем регистр команд и состояния
	uint16_t COMMAND_AND_STATUS_REGISTER = getregister(CommandAndStatusRegister);
	
	// В регистр Х007, бит 3 - записать 1.
	uint16_t status = bit::setbit(COMMAND_AND_STATUS_REGISTER, ResetZero, true);

	// В регистр Х008. бит 9 - записать 0.
	status = bit::setbit(status, CommandRequest, false);

	
	//записываем значение в регистр
	if (writeregister(CommandAndStatusRegister, status) == -1)
	{
		ss << "Сбой калибровки нуля" << std::endl;
		ss << " - Запись в регистр команд и состояний (CommandAndStatusRegister) " << std::endl;

		throw device_exception(ss.str().c_str(), "NesmitDevice::zero");
	}


	if (writeregister(PositionLowRegister, 0) == -1)
	{
		ss << "Сбой калибровки нуля" << std::endl;
		ss << " - Установка угла (0) " << std::endl;
		
		throw device_exception(ss.str().c_str(), "NesmitDevice::zero");
	}


	//--------------------------------
	//ожидаем установку бита занятости
	if (!waitforsetbit(CommandAndStatusRegister, CommandRequest, false, MAX_WAIT_SET_BIT_TIME))
	{
		ss << "Сбой калибровки нуля." << std::endl;
		ss << " - Превышено время сброса бита занятости (CommandRequest) " << std::endl;
		throw device_exception(ss.str(), "NesmitDevice::zero()");
	}
	
}

//Выполняет (не используемая)
void NesmitDevice::settick(const char* state)
{
	throw std::exception("settick : Not implemented yet...");
}


//записать значение угла в регистр
void NesmitDevice::writeangle(const uint16_t& angle) 
{
	std::stringstream ss;
	
	//ошибка задания угла наклона диспергирующего устройства
	if (angle < MIN_DISPERGATOR_ANGEL || angle > MAX_DISPERGATOR_ANGEL)
	{
		ss << "Ошибка задания угла (от 8° до 30°) : " << angle/1000.0 << std::endl;
		throw std::invalid_argument(ss.str().c_str());
	}

	//вход в критическую секцию
	lock obj(&_nesvitCS);
	
	//получаем регистр команд и состояния
	uint16_t CONTROL_AND_INPUT_REGISTER = getregister(CommandAndStatusRegister);
	
	// В регистр Х007, бит 0 - записать 0.
	uint16_t status = bit::setbit(CONTROL_AND_INPUT_REGISTER, Angle, false);

	// В регистр Х007, бит 1 - записать 0.
	status = bit::setbit(status, Gap, false);

	// В регистр Х007. бит 2 - записать 0.
	status = bit::setbit(status, StopEngine, false);

	// В регистр Х008. бит 9 - записать 0.
	status = bit::setbit(status, CommandRequest, false);

	//записываем значение в регистр
	if (writeregister(CommandAndStatusRegister, status) == -1)
	{
		ss << "Сбой задания угла наклона диспергирующего устройства. " << std::endl;
		ss << " - Запись в регистр команд и состояний (CommandAndStatusRegister) " << std::endl;
		throw device_exception(ss.str().c_str(), "NesmitDevice::writeangle");
	}

	//--------------------------------
	//ожидаем сброс бита Angel
	if (!waitforsetbit(CommandAndStatusRegister, Angle, false, MAX_WAIT_SET_BIT_TIME))
	{
		ss << "Сбой задания угла наклона диспергирующего устройства. " << std::endl;
		ss << " - Сброс бита значение угла в тиках таймера (Angle) " << std::endl;
		throw device_exception(ss.str(), "NesmitDevice::lamp()");
	}

	//--------------------------------
	//ожидаем сброс бита Gap 
	if (!waitforsetbit(CommandAndStatusRegister, Gap, false, MAX_WAIT_SET_BIT_TIME))
	{
		ss << "Сбой задания угла наклона диспергирующего устройства. " << std::endl;
		ss << " - Сброс бита режима калибровки зазора системы (Gap) " << std::endl;
		throw device_exception(ss.str(), "NesmitDevice::lamp()");
	}

	//--------------------------------
	//ожидаем сброс бита StopEngine 
	if (!waitforsetbit(CommandAndStatusRegister, StopEngine, false, MAX_WAIT_SET_BIT_TIME))
	{
		ss << "Сбой задания угла наклона диспергирующего устройства. " << std::endl;
		ss << " - Сброс бита остановки двигателя (StopEngine) " << std::endl;
		throw device_exception(ss.str(), "NesmitDevice::lamp()");
	}

	//--------------------------------
	//ожидаем сброс бита CommandRequest 
	if (!waitforsetbit(CommandAndStatusRegister, CommandRequest, false, MAX_WAIT_SET_BIT_TIME))
	{
		ss << "Сбой задания угла наклона диспергирующего устройства. " << std::endl;
		ss << " - Сброс бита запроса на выполнение команды (CommandRequest) " << std::endl;
		throw device_exception(ss.str(), "NesmitDevice::lamp()");
	}

	//записываем значение угла в регистр
	if (writeregister(PositionLowRegister, angle) == -1)
	{
		ss << "Сбой задания угла наклона диспергирующего устройства. " << std::endl;
		ss << " - Установка  значения угла (PositionLowRegister) " << std::endl;
		throw device_exception(ss.str().c_str(), "NesmitDevice::writeangle");
	}

	
	//--------------------------------
	//ожидаем установку бита занятости
	if (!waitforsetbit(CommandAndStatusRegister, CommandBusy, true, MAX_WAIT_SET_BIT_TIME))
	{
		ss << "Сбой задания угла наклона диспергирующего устройства. " << std::endl;
		ss << " - Превышено время установки бита занятости (CommandBusy) " << std::endl;
		throw device_exception(ss.str(), "NesmitDevice::lamp()");
	}
}

//прогресс установки угла 
double NesmitDevice::progress() const
{
	//вход в критическую секцию
	lock obj(&_nesvitCS);
	
	//получаем регистр прогресса
	uint16_t PROGRESS_REGISTER = getregister(ProgressRegister);

	double progress = PROGRESS_REGISTER * 100.0 / 255.0;	
	return progress;
}


/// Закрыть общение с прибором
void NesmitDevice::close()
{
	std::stringstream ss;
	
	//вход в критическую секцию
	lock obj(&_nesvitCS);

	//получаем регистр команд и состояния
	uint16_t COMMAND_AND_STATUS_REGISTER = getregister(CommandAndStatusRegister);

	
	// В регистр Х007, бит 0 - записать 0.
	uint16_t status = bit::setbit(COMMAND_AND_STATUS_REGISTER, Client, false);

	//Запись регистра команд и состояний	
	if  (writeregister(CommandAndStatusRegister, status) == -1)
	{
		ss << "Ошибка при остановке прибора. " << std::endl;
		ss << " - Запись регистра команд и состояний (CommandAndStatusRegister)" << std::endl;
		throw device_exception(ss.str().c_str(), "NesmitDevice::close()");
	}

	//--------------------------------
	//ожидаем сброс бита Client
	if (!waitforsetbit(CommandAndStatusRegister, Client, false, MAX_WAIT_SET_BIT_TIME))
	{
		ss << "Ошибка при остановке прибора. " << std::endl;
		ss << " - Сброс бита готовности клиента к работе (Client) " << std::endl;
		throw device_exception(ss.str(), "NesmitDevice::close()");
	}	
}
