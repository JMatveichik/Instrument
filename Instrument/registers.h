#pragma once

//��������� ������ ���������
enum RegisterStartAddress : int
{
	//������� ���������� 8 bit � ������� ������ ������� ������
	ControlAndInputRegister = 0x00,
	
	//������� ������� ���������
	PositionHightRegister = 0x01,

	//������ ������� ���������
	PositionLowRegister = 0x02,

	//��������� ������� 8 bit � ������� ��������� 8 bit
	CommandAndStatusRegister = 0x03,

	//������� ���������
	ProgressRegister = 0x04,

	//������� ������ ��������
	VersionRegister = 0x5
};


//���� �������� ���������� � �������� ������
enum ControlAndInputRegisterBits : unsigned char
{ 
	//��� ���������� ������� ��������
	ShutterBit = 0x00,

	//��� ���������� ������ FF
	LampFFBit = 0x01,

	//��� ���������� ������ FF
	LampCSBit = 0x02,

	//��� ����������� �������� ������
	CameraBit = 0x03,

	//��� ���������� ��������� scr
	ScrBit = 0x04,

	//��� ��������� ��������� �������
	LimitSW = 0x08,

	//��� ��������� ������� ����� �������	
	ZeroSW = 0x08,
};



//���� �������� ������
enum CommandAndStatusRegisterBits : unsigned char
{
	//��� �������� ���� � ����� �������
	Angle = 0x00,

	//��� ����� ���������� ������ �������
	Gap = 0x01,

	//��� ��������� ���������
	StopEngine = 0x02,

	//��� ����� ���������� ����
	ResetZero = 0x03,

	//��� ��������� ���������� ����
	GetZeroCalibration = 0x04,

	//��� ���������� ������� � ������
	Client = 0x08,

	//��� ������ �� ���������� �������
	CommandRequest = 0x09,

	//��� ��������� ����������� �������
	CommandBusy = 0x0A,

	//��� ������ ���������� ������� �����
	ZeroState = 0x0B
};



