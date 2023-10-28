#pragma once

#include <string>
#include <modbus.h>

#include "loggers.h"

typedef std::pair<std::string, int> MODBUS_CONNECTION_DATA;

//максимальное количество попыток чтения регистров
const int MAX_RETRIES = 3;

//ожидание в цикле для проверки состояния регистра
const int CYCLE_OPERATION_DELAY = 250;

//ожидание перед запросом Mmodbus, ms
//если присутствует задержка перед выполнением следующей операции
const int MODBUS_OPERATION_DELAY = 100;

//максимальное время установления бита, ms
const int MAX_WAIT_SET_BIT_TIME = 5000;

//минимальный угол наклона диспергирующего устройства, ° * 1000
const int MIN_DISPERGATOR_ANGEL = 8000;

//максимальный угол наклона диспергирующего устройства, ° * 1000
const int MAX_DISPERGATOR_ANGEL = 30000;



//Объект будет создаваться в каждой функции .dll
//Будет освобожден при выходе из области видимости (при выходе из функции .dll)
class NesmitDevice
{
public:
	
	//конструктор по умолчанию
	NesmitDevice();
	~NesmitDevice();
	
public:

	/// <summary>
	/// Установить целевоую систему логирования
	/// </summary>
	/// <param name="pLogger">Целевой объект логирования</param>
	void setlogger(ILogger* pLogger);
	

	//установить соединение с прибором по протоколу Modbus
	//options - пара<std::string, int> адрес прибора в виде строки и modbus port
	bool connect(const MODBUS_CONNECTION_DATA& options);
	
	//установить соединение с прибором по протоколу Modbus
	//ip - адрес прибора в виде строки
	//port - modbus port
	bool connect(const std::string& ip, const int& port);

	//Прервать Modbus соединение с прибором 
	void disconect();


	/// <summary>
	/// Инициализация устройства
	/// </summary>
	void initialize();

	/// <summary>
	/// Проверка бита  готовности прибора к работе
	/// </summary>	
	/// <returns>true - прибор готов;false - прибор не готов</returns>
	bool clientready() const;


	/// <summary>
	/// Проверка бита  готовности прибора к работе
	/// </summary>	
	/// <returns>true - прибор занят выполнением другой операции; false - прибор свободен</returns>
	bool clientbusy() const;

	/// <summary>
	/// Запрошено выполнение команды
	/// </summary>	
	/// <returns>true - принят запрос на выполнение команды; false - прибор свободен</returns>
	bool commandrequested() const;
	

	/// <summary>
	/// Открывает(state = 1) и закрывает(state = 0) затвор.
	/// </summary>
	/// <param name="state">
	/// При значении state = 1 в регистр X001 бит 0, записать значение 1.
	/// При значении state = 0 в регистр X001 бит 0, записать значение 0.
	/// </param>
	void shutter(unsigned char state);

	/// <summary>
	/// Устанавливает фильтр разделения порядков с заданным номером n.
	/// </summary>
	void filter(int value);

	/// <summary>
	/// Включить (state=1) или выключить (state=0) лампу плоского поля (kind=FF) или спектра сравнения (kind=CS).
	/// </summary>
	/// <param name="kind">Лампа плоского поля (kind=FF); Лампа спектра сравнения (kind=CS).</param>
	/// <param name="state">0 - Выключить лампу; 1 - Включить лампу</param>
	void lamp(const char* kind, unsigned char state);

	//Установить ширину щели спектрографа равной state.
	void slit(const char* state);

	//Выполняет калбровку нуля
	void zero();

	//Выполняет (не используемая)
	void settick(const char* state);

	/// <summary>
	/// Закрыть общение с прибором
	/// </summary>
	void close();
	

	/// <summary>
	/// Записать значение угла в регистр
	/// </summary>
	/// <param name="angle">Значение угла </param>	
	void writeangle(const uint16_t& angle);

	/// <summary>
	/// Получить прогресс установления угла
	/// </summary>
	/// <returns></returns>
	double  progress() const;
	

protected :

	/// <summary>
	/// Получтить состояние регистра Modbus
	/// </summary>
	/// <param name="reg">Номер регистра</param>
	/// <param name="isInput">Тип регистра : true - input register; false - holding register </param>
	/// <param name="retries">Число попыток для получения ргистра</param>
	/// <returns>Значание регистра</returns>	
	uint16_t getregister(int reg, int retries = MAX_RETRIES, bool isInput = true) const;

	/// <summary>
	/// Получить соостояние нескольеих регистров
	/// </summary>
	/// <param name="reg">Номер начального регистра</param>
	/// <param name="count">Число регистров для чтения</param>
	/// <param name="retries">Число попыток для получения ргистров</param>
	/// <param name="isInput">Тип регистра : true - input register; false - holding register </param>
	/// <param name="values">Выходные значения регистров</param>
	/// <returns></returns>
	int trygetregisters(int reg, int count, uint16_t* values, int retries, bool isInput = true ) const;
	
	
	/// <summary>
	/// установить бит в заданном регистре 
	/// </summary>
	/// <param name="reg">Номер регистра</param>
	/// <param name="bit">Номер бита</param>
	/// <param name="state">Состояние в кторое нужно переключить бит</param>
	/// <returns></returns>
	int setregisterbit(int reg, unsigned char bit, bool state);


	/// <summary>
	/// Записать значение в регистр
	/// </summary>
	/// <param name="reg">Номер регистра</param>
	/// <param name="value">Значение которое нужно зсписать</param>
	/// <returns></returns>
	int writeregister(int reg, uint16_t value);


	bool waitforsetbit(int reg, unsigned char bit, bool state, int maxtime) const;

	/// <summary>
	/// Настройка параметров протокола
	/// </summary>
	void configurate();

	
private:

	//modbus соединение
	modbus_t* _connection;

	//критическая секция обработки/записи даных
	mutable CRITICAL_SECTION _nesvitCS;
	
	ILogger* logger;
	

	const uint16_t ONE		= 1;
	const uint16_t BADVALUE = 8888;
};

class NESMITCONNECTION
{
public:
	
	NESMITCONNECTION(NesmitDevice* pDevice) : _pDevice(pDevice) {}
	~NESMITCONNECTION()
	{
		_pDevice->disconect();
	}
	
private :
	
	NesmitDevice* _pDevice;
};

