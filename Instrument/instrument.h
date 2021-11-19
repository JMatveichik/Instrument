#pragma once

#ifdef _DLL_MASTER_
	#define IMPEXP __declspec(dllexport)
#else
#	define IMPEXP __declspec(dllimport)
#endif


#define CALLCONV __stdcall

#ifdef __cplusplus

extern "C" {
#endif

	//Выполняет все необходимые действия по инициализации спектрографа. 
	//Параметры: path: имя директория где могут храниться необходимые данные для работы библиотеки. 
	//Эта и другие функции должны возвращать 1 если операция прошла успешно и 0 в противном случае.
	IMPEXP bool CALLCONV  InitInst(const char* path );

	
	//Открывает(state = 1) и закрывает(state = 0) затвор.
	IMPEXP bool CALLCONV Shutter(unsigned char state);

	//Устанавливает фильтр разделения порядков с заданным номером n.
	IMPEXP bool CALLCONV Filter(int value);

	//Включить (state=1) или выключить (state=0) лампу плоского поля (kind=FF) или спектра сравнения (kind=CS).
	IMPEXP bool CALLCONV Lamp(const char *kind, unsigned char state);

	//Установить наклон state для n-го диспергирующего устройства спектрографа.
	IMPEXP bool CALLCONV Disp(unsigned char n, const char *state);

	//Установить ширину щели спектрографа равной state.
	IMPEXP bool CALLCONV Slit(const char *state);

	//Выполняет все необходимые действия по подготовке спектрографа к выключению, а библиотеки к выгружению из памяти.
	IMPEXP bool CALLCONV CloseInst();

	//Выполняет 
	IMPEXP bool CALLCONV GetZero();

	//Выполняет 
	IMPEXP bool CALLCONV SetTick(const char *state);


#ifdef __cplusplus
}
#endif