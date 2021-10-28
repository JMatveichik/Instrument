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
enum ControlRegisterMasks
{ 
	//бит управление входным затвором
	ShutterBit = 1 << 0,

	//бит управление лампой FF
	LampFFBit = 1 << 1,

	//бит управление лампой FF
	LampCSBit = 1 << 2,

	//Упаравление питанием камеры
	CameraBit = 1 << 3,

	//Управление заслонкой scr
	ScrBit = 1 << 4
};

//Маски регистра входов
enum InputRegisterMasks
{
	//маска состояние концевика решетки
	LimitSW = 1 << 0,

	//маска состояние датчика полож решетки	
	ZeroSW = 1 << 1	
};

//Маски регистра команд
enum CommandRegisterMasks
{
	//маска значение угла в тиках таймера
	Angle = 1 << 0,

	//маска Режим калибровки зазора системы
	Gap = 1 << 1,

	//Остановка двигателя
	StopEngine = 1 << 2,

	//Сброс калибровки нуля
	ResetZero = 1 << 3,

	//Выполнить калибровку нуля
	GetZeroCalibration = 1 << 4,
};

//Маски регистра состояния
enum StatusRegisterMasks
{
	//маска готовность клиента к работе
	Client = 1 << 0,

	//маска запрос на выполнение команды
	CommandRequest = 1 << 1,

	//Занятость выполнением команды
	CommandBusy = 1 << 2,

	//Статус калибровки нулевой точки
	ZeroState = 1 << 3	
};



