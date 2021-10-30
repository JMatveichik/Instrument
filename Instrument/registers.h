#pragma once

//начальные адреса регистров
enum RegisterStartAddress{

	//Регистр управления
	ControlRegister = 0x01,

	//Регистр входов
	InputRegisters = 0x02,

	//Верхний регистр положения
	PositionHightRegister = 0x03,

	//Нижний регистр положения
	PositionLowRegister = 0x05,

	//Командный регистр
	CommandRegister = 0x07,

	//Регистр состояний
	StatusRegistr = 0x08,

	//Регистр прогресса
	ProgressRegister = 0x09,

	//Регистр версии прошивки
	VersionRegister = 0x10
};


//Маски регистра управления
enum ControlRegisterBits
{ 
	//бит управление входным затвором
	ShutterBit,

	//бит управление лампой FF
	LampFFBit ,

	//бит управление лампой FF
	LampCSBit,

	//Бит упаравление питанием камеры
	CameraBit,

	//Бит управление заслонкой scr
	ScrBit
};

//Биты регистра входов
enum InputRegisterBits
{
	//бит состояние концевика решетки
	LimitSW,

	//бит состояние датчика полож решетки	
	ZeroSW
};

//Биты регистра команд
enum CommandRegisterBits
{
	//бит значение угла в тиках таймера
	Angle,

	//бит Режим калибровки зазора системы
	Gap,

	//бит Остановка двигателя
	StopEngine,

	//бит Сброс калибровки нуля
	ResetZero,

	//бит Выполнить калибровку нуля
	GetZeroCalibration,
};

//Маски регистра состояния
enum StatusRegisterBits
{
	//бит готовность клиента к работе
	Client,

	//бит запрос на выполнение команды
	CommandRequest,

	//бит Занятость выполнением команды
	CommandBusy,

	//бит Статус калибровки нулевой точки
	ZeroState	
};



