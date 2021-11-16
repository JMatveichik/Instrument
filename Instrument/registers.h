#pragma once

//начальные адреса регистров
enum RegisterStartAddress : int
{
	//Регистр управления 8 bit и Регистр входов Регистр входов
	ControlAndInputRegister = 0x00,
	
	//Верхний регистр положения
	PositionHightRegister = 0x01,

	//Нижний регистр положения
	PositionLowRegister = 0x02,

	//Командный регистр 8 bit и Регистр состояний 8 bit
	CommandAndStatusRegister = 0x03,

	//Регистр прогресса
	ProgressRegister = 0x04,

	//Регистр версии прошивки
	VersionRegister = 0x5
};


//биты регистра управления и регистра входов
enum ControlAndInputRegisterBits : unsigned char
{ 
	//бит управление входным затвором
	ShutterBit = 0x00,

	//бит управление лампой FF
	LampFFBit = 0x01,

	//бит управление лампой FF
	LampCSBit = 0x02,

	//Бит упаравление питанием камеры
	CameraBit = 0x03,

	//Бит управление заслонкой scr
	ScrBit = 0x04,

	//бит состояние концевика решетки
	LimitSW = 0x08,

	//бит состояние датчика полож решетки	
	ZeroSW = 0x08,
};



//Биты регистра команд
enum CommandAndStatusRegisterBits : unsigned char
{
	//бит значение угла в тиках таймера
	Angle = 0x00,

	//бит Режим калибровки зазора системы
	Gap = 0x01,

	//бит Остановка двигателя
	StopEngine = 0x02,

	//бит Сброс калибровки нуля
	ResetZero = 0x03,

	//бит Выполнить калибровку нуля
	GetZeroCalibration = 0x04,

	//бит готовность клиента к работе
	Client = 0x08,

	//бит запрос на выполнение команды
	CommandRequest = 0x09,

	//бит Занятость выполнением команды
	CommandBusy = 0x0A,

	//бит Статус калибровки нулевой точки
	ZeroState = 0x0B
};



