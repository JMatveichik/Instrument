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

	//��������� ��� ����������� �������� �� ������������� ������������. 
	//���������: path: ��� ���������� ��� ����� ��������� ����������� ������ ��� ������ ����������. 
	//��� � ������ ������� ������ ���������� 1 ���� �������� ������ ������� � 0 � ��������� ������.
	IMPEXP bool CALLCONV  InitInst(const char* path );

	
	//���������(state = 1) � ���������(state = 0) ������.
	IMPEXP bool CALLCONV Shutter(unsigned char state);

	//������������� ������ ���������� �������� � �������� ������� n.
	IMPEXP bool CALLCONV Filter(int value);

	//�������� (state=1) ��� ��������� (state=0) ����� �������� ���� (kind=FF) ��� ������� ��������� (kind=CS).
	IMPEXP bool CALLCONV Lamp(const char *kind, unsigned char state);

	//���������� ������ state ��� n-�� ��������������� ���������� ������������.
	IMPEXP bool CALLCONV Disp(unsigned char n, const char *state);

	//���������� ������ ���� ������������ ������ state.
	IMPEXP bool CALLCONV Slit(const char *state);

	//��������� ��� ����������� �������� �� ���������� ������������ � ����������, � ���������� � ���������� �� ������.
	IMPEXP bool CALLCONV CloseInst();

	//��������� 
	IMPEXP bool CALLCONV GetZero();

	//��������� 
	IMPEXP bool CALLCONV SetTick(const char *state);


#ifdef __cplusplus
}
#endif