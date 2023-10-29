#include "pch.h"

#include "NesmitDevice.h"
#include "deviceexception.h"
#include "registers.h"
#include "helper.h"
#include "lock.h"


//----------------------------------------------------

NesmitDevice::NesmitDevice() : _connection(nullptr)
{
	 
	//�������� ������� ��� ����������
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
	// ������� ����� �������� Modbus ��� TCP
	_connection = modbus_new_tcp(ip.c_str(), port);

	if (_connection == nullptr)
	{
		throw device_connection_exception("�� ������� ������� �������� Modbus", "modbus_new_tcp" , ip, port);
	}

	// ��������� ����������
	const int connected = modbus_connect(_connection);
	if (connected == -1)
	{
		throw device_connection_exception("O����� ���������� c ��������", "modbus_connect", ip, port);
	}
	
	// ������������� Modbus ����� ���������� (������ ��� �� ��������� ��� Modbus TCP)
	modbus_set_slave(_connection, 1);

	//������������� ��������� ����������
	//configurate();
	
	return true;
}

//----------------------------------------------------

void NesmitDevice::disconect()
{
	//�����������  ������� modbus
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
// � ������� �007, ��� 0 - �������� 1.
void NesmitDevice::initialize()
{
	//�������� ������� ������ � ���������
	uint16_t COMMAND_AND_STATUS_REGISTER = getregister(CommandAndStatusRegister);
	
	// � ������� �007, ��� 0 - �������� 1.
	uint16_t status = bit::setbit(COMMAND_AND_STATUS_REGISTER, Client, true);

	
	//���������� �������� � �������
	if (writeregister(CommandAndStatusRegister, status) == -1)
	{
		std::stringstream ss;
		
		ss << "������ ����� �� �������������� ������." << std::endl;
		ss << " - ������� ���������� ������� � ������ (Client)" << std::endl;

		throw device_exception(ss.str(), "NesmitDevice::initialize()");
	}
}

//----------------------------------------------------
//�������� ����  ���������� ������� � ������
//return true - ������ �����
//return false - ������ �� �����
bool NesmitDevice::clientready() const 
{
	//�������� ������� ������ � ���������
	uint16_t COMMAND_AND_STATUS_REGISTER = getregister(CommandAndStatusRegister);
	
	//��������� ��� ���������� ������� � ������
	if (bit::checkbit(COMMAND_AND_STATUS_REGISTER, Client))
		return true;

	return false;
}

//----------------------------------------------------
//�������� ����  ��������� �������
//return true - ������ ����� ����������� ������ ��������
//return false - ������ ��������
bool NesmitDevice::clientbusy() const
{
	//�������� ������� ������ � ���������
	uint16_t COMMAND_AND_STATUS_REGISTER = getregister(CommandAndStatusRegister);

	//��������� ��� ��������� ������� 1 - ����� 0 - ��������
	if (!bit::checkbit(COMMAND_AND_STATUS_REGISTER, CommandBusy))
		return false;

	return true;
}

//----------------------------------------------------
//�������� ����  ������� ������� �� ����������
//return true - ������ �� ���������� �������
//return false - ������
bool NesmitDevice::commandrequested() const
{
	//�������� ������� ������ � ���������
	uint16_t COMMAND_AND_STATUS_REGISTER = getregister(CommandAndStatusRegister);
	
	//�������� ����  ������� ������� �� ����������
	if (!bit::checkbit(COMMAND_AND_STATUS_REGISTER, CommandRequest))
		return true;
	
	return false;
}


//�������� �������� � �������
int NesmitDevice::writeregister(int reg, uint16_t value)
{
	//���� � ����������� ������
	lock obj(&_nesvitCS);

	std::stringstream ss;
	ss << "writeregister => register (" << reg << ") : value (" << value << ")" << std::endl;

	//������ � ������� ������ ��������
	int writeCount = -1;

	try
	{
		writeCount = modbus_write_register(_connection, reg, value);
	}
	catch (...)
	{
		writeCount = -1;
	}

	//���� ������ �� ������� 	
	if (writeCount == -1)
		ss << "\t������ ������ (" << modbus_strerror(errno) << ")" << std::endl;
	else
		ss << "\t�������� ����� �������� (" << value << ")" << std::endl;

	//�������� �����
	//flushlogger(ss);

	return writeCount;
}


//��������� ������� ���
uint16_t NesmitDevice::getregister(int reg, int retries/* = MAX_RETRIES*/, bool isInput/* = true */) const
{
	uint16_t value;	
	trygetregisters(reg, ONE, &value, retries, isInput);
	return value;
}


//���������� �������� �������� ���
//reg   - ����� ��������
//count - ����� ��������� ��� ������
//retries - ���������� ������� ��� ������
//isInput - input or holding  register
//values - ����������� ��������� ���������
int NesmitDevice::trygetregisters(int reg, int count, uint16_t* values, int retries/* = MAX_RETRIES*/, bool isInput/* = true */) const
{
	//������ � ����������� ������
	lock obj(&_nesvitCS);

	std::stringstream ss;
	ss << "trygetregister => " << std::endl;

	///��������� ������ ��������� 
	int result  = -1;

	//�������
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
			ss << "\t������� �" << retrie << " ������ ������ �������� (" << modbus_strerror(errno) << ")" << std::endl;
		}
	}

	return result;
}


//���������� ��� � �������� �������� 
//reg - ����� ��������
//bit - ����� ����
//state - ��������� � ������ ����� ����������� ���
int NesmitDevice::setregisterbit(int reg, unsigned char bit, bool state)
{
	//������ � ����������� ������
	lock obj(&_nesvitCS);

	std::stringstream ss;
	ss << "\tsetregisterbit =>" << std::endl;

	///�������� ������� ��������� ��������  
	uint16_t value;
	if (trygetregisters(reg, ONE, &value, MAX_RETRIES, true) == -1)
	{
		ss << "\t\t=>������ ������ ��������" << std::endl;

		//�������� ���
		//flushlogger(ss);

		return -1;
	}

	//��������� ������� ���� � ������ ���������
	uint16_t output = bit::setbit(value, bit, state);

	//��� ��� � �������� ���������
	if (output == value)
	{
		ss << "\t\t=> ��� ��������� ��������� �������� => ������� : (" << value << ") ����� : (" << output << ")" << std::endl;

		//�������� ���
		//flushlogger(ss);

		//��� ����������
		return 1;
	}

	//��� ����
	ss << "\t\t=> ��������� ���� => register (" << reg << ") bit : (" << (int)bit << ") state : (" << state << ") ������� : (" << value << ")" << " ����� : (" << output << ")" << std::endl;

	//�������� ����� ��������� Modbus
	Sleep(MODBUS_OPERATION_DELAY);

	//������ � ������� ������ ��������
	const int writeCount = modbus_write_registers(_connection, reg, 1, &output);

	//���� ������ �� ������� 
	if (writeCount == -1)
		ss << "\t\t=> ������ ������ �������� => (" << modbus_strerror(errno) << ")" << std::endl;

	//��������� �����
	//flushlogger(ss);

	//���� ����� ������������� ������
#ifndef CONFIRM_SET_BIT

	return (writeCount == -1) ? -1 : state;

#else
//�������� ����� ��������� Modbus
	Sleep(MODBUS_OPERATION_DELAY);

	//������������� ������ - �������� ������� ������
	if (trygetregisters(reg, 1, MAX_RETRIES, true, &value) == -1)
	{
		//��������� �����
		flushlogger(ss);

		return -1;
	}

	//�������� ������������ �� ��������� ����
	const bool isBitSet = bit::checkbitstate(value, bit, state);

	//���� ��� ���������� ���������
	if (isBitSet)
		ss << "\t\t=> bit : (" << bit << ") ���������� � �������� ��������� state : (" << state << ")" << std::endl;
	else
		ss << "\t\t=> bit : (" << bit << ") �� ����������� � ���������  state : (" << state << ")" << std::endl;

	//��������� �����
	flushlogger(ss);

	return (int)isBitSet;
#endif


}

bool NesmitDevice::waitforsetbit(int reg, unsigned char bit, bool state, int maxtime) const
{
	//���� � ����������� ������
	lock obj(&_nesvitCS);
	
	// ���������� ���������� ��� �������� ������� ������ ��������
	auto startTime = std::chrono::high_resolution_clock::now();

	while (true) 
	{
		// ��������� ��������� ����� � ������� ������ ��������
		auto currentTime = std::chrono::high_resolution_clock::now();
		auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();

		// ���������, �� ��������� �� ��������� ����� ������������ ����� ��������
		if (elapsedTime >= maxtime)
		{
			// ����� �� ����� �� ����-����
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

/// ���������(state = 1) � ���������(state = 0) ������.
void NesmitDevice::shutter(unsigned char state)
{
	std::stringstream ss;
	
	///������ �������� ��������� ��������� �������
	if (state > 1 || state < 0)
	{
		ss << "������ ��������� ��������� ������� : " << state << std::endl;
		std::invalid_argument(ss.str().c_str());

		//flushlogger(ss);
	}

	//���� � ����������� ������
	lock obj(&_nesvitCS);

	//�������� ������� ������ � ���������
	uint16_t CONTROL_AND_INPUT_REGISTER = getregister(ControlAndInputRegister);
	
	// � ������� �007, ��� 0 - �������� 1.
	uint16_t status = bit::setbit(CONTROL_AND_INPUT_REGISTER, ShutterBit, (state == 1));
		

	//���������� �������� � �������
	if (writeregister(ControlAndInputRegister, status) == -1)
	{

		ss << "���� ���������� ��������." << std::endl;
		ss << " - ������� ���� (ShutterBit : " << (state == 1) << ")" << std::endl;

		throw device_exception(ss.str(), "NesmitDevice::shutter()");
	}


	//--------------------------------
	//������� ��������� ���� ��������
	
	if (!waitforsetbit(ControlAndInputRegister, ShutterBit, state , MAX_WAIT_SET_BIT_TIME))
	{
		ss << "���� ���������� ��������." << std::endl;
		ss << " - ������� ���� (ShutterBit : " << (state == 1) << ")" << std::endl;
		ss << "��� �� ��� ����������!" << std::endl;

		throw device_exception(ss.str(), "NesmitDevice::shutter()");
	}
}


/// ������������� ������ ���������� �������� � �������� ������� n.
void NesmitDevice::filter(int value)
{
	throw std::exception("filter : Not implemented yet...");
}

/// �������� (state=1) ��� ��������� (state=0) ����� �������� ���� (kind=FF) ��� ������� ��������� (kind=CS).
void NesmitDevice::lamp(const char* kind, unsigned char state)
{
	std::stringstream ss;
	std::string type(kind);

	///������ �������� ��������� �������������� �����
	if (type != "CS" && type != "FF")
	{
		ss << "�������� ������������� ����� : " << type << std::endl;
		throw std::invalid_argument(ss.str().c_str());
	}

	///������ �������� ��������� ��������� �������
	if (state > 1 || state < 0)
	{
		ss << "������ ��������� ��������� ����� : " << state << std::endl;
		std::invalid_argument(ss.str().c_str());
	}

	//
	unsigned char bit   = (type == "FF") ? LampFFBit : LampCSBit;
	std::string lamp    = (type == "FF") ? "����� �������� ���� (kind = FF)" : "����� ������� ���������(kind = CS)";
	std::string bitname = (type == "FF") ? "LampFFBit" : "LampCSBit";
	
	//���� � ����������� ������
	lock obj(&_nesvitCS);

	//�������� ������� ������ � ���������
	uint16_t CONTROL_AND_INPUT_REGISTER = getregister(ControlAndInputRegister);
	
	// � ������� �007, ��� 0 - �������� 1.
	uint16_t status = bit::setbit(CONTROL_AND_INPUT_REGISTER, bit, (state == 1));


	//���������� �������� � �������
	if (writeregister(ControlAndInputRegister, status) == -1)
	{
		ss << "���� ���������� " << lamp << std::endl;
		ss << " - ������� ���� (" << bitname << ":" << (state == 1) << ")" << std::endl;

		throw device_exception(ss.str(), "NesmitDevice::lamp()");
	}

	//--------------------------------
	//������� ��������� ���� �����
	if (!waitforsetbit(ControlAndInputRegister, bit, state, MAX_WAIT_SET_BIT_TIME))
	{
		ss << "���� ���������� " << lamp << std::endl;
		ss << " - ������� ���� (" << bitname << ":" << (state == 1) << ")" << std::endl;
		ss << "��� �� ��� ����������!" << std::endl;

		throw device_exception(ss.str(), "NesmitDevice::lamp()");
	}
}

//���������� ������ ���� ������������ ������ state.
void NesmitDevice::slit(const char* state)
{
	throw std::exception("slit: Not implemented yet...");
}


//��������� ��������� ����
void NesmitDevice::zero()
{
	std::stringstream ss;

	//���� � ����������� ������
	lock obj(&_nesvitCS);

	//�������� ������� ������ � ���������
	uint16_t COMMAND_AND_STATUS_REGISTER = getregister(CommandAndStatusRegister);
	
	// � ������� �007, ��� 3 - �������� 1.
	uint16_t status = bit::setbit(COMMAND_AND_STATUS_REGISTER, ResetZero, true);

	// � ������� �008. ��� 9 - �������� 0.
	status = bit::setbit(status, CommandRequest, false);

	
	//���������� �������� � �������
	if (writeregister(CommandAndStatusRegister, status) == -1)
	{
		ss << "���� ���������� ����" << std::endl;
		ss << " - ������ � ������� ������ � ��������� (CommandAndStatusRegister) " << std::endl;

		throw device_exception(ss.str().c_str(), "NesmitDevice::zero");
	}


	if (writeregister(PositionLowRegister, 0) == -1)
	{
		ss << "���� ���������� ����" << std::endl;
		ss << " - ��������� ���� (0) " << std::endl;
		
		throw device_exception(ss.str().c_str(), "NesmitDevice::zero");
	}


	//--------------------------------
	//������� ��������� ���� ���������
	if (!waitforsetbit(CommandAndStatusRegister, CommandRequest, false, MAX_WAIT_SET_BIT_TIME))
	{
		ss << "���� ���������� ����." << std::endl;
		ss << " - ��������� ����� ������ ���� ��������� (CommandRequest) " << std::endl;
		throw device_exception(ss.str(), "NesmitDevice::zero()");
	}
	
}

//��������� (�� ������������)
void NesmitDevice::settick(const char* state)
{
	throw std::exception("settick : Not implemented yet...");
}


//�������� �������� ���� � �������
void NesmitDevice::writeangle(const uint16_t& angle) 
{
	std::stringstream ss;
	
	//������ ������� ���� ������� ��������������� ����������
	if (angle < MIN_DISPERGATOR_ANGEL || angle > MAX_DISPERGATOR_ANGEL)
	{
		ss << "������ ������� ���� (�� 8� �� 30�) : " << angle/1000.0 << std::endl;
		throw std::invalid_argument(ss.str().c_str());
	}

	//���� � ����������� ������
	lock obj(&_nesvitCS);
	
	//�������� ������� ������ � ���������
	uint16_t CONTROL_AND_INPUT_REGISTER = getregister(CommandAndStatusRegister);
	
	// � ������� �007, ��� 0 - �������� 0.
	uint16_t status = bit::setbit(CONTROL_AND_INPUT_REGISTER, Angle, false);

	// � ������� �007, ��� 1 - �������� 0.
	status = bit::setbit(status, Gap, false);

	// � ������� �007. ��� 2 - �������� 0.
	status = bit::setbit(status, StopEngine, false);

	// � ������� �008. ��� 9 - �������� 0.
	status = bit::setbit(status, CommandRequest, false);

	//���������� �������� � �������
	if (writeregister(CommandAndStatusRegister, status) == -1)
	{
		ss << "���� ������� ���� ������� ��������������� ����������. " << std::endl;
		ss << " - ������ � ������� ������ � ��������� (CommandAndStatusRegister) " << std::endl;
		throw device_exception(ss.str().c_str(), "NesmitDevice::writeangle");
	}

	//--------------------------------
	//������� ����� ���� Angel
	if (!waitforsetbit(CommandAndStatusRegister, Angle, false, MAX_WAIT_SET_BIT_TIME))
	{
		ss << "���� ������� ���� ������� ��������������� ����������. " << std::endl;
		ss << " - ����� ���� �������� ���� � ����� ������� (Angle) " << std::endl;
		throw device_exception(ss.str(), "NesmitDevice::lamp()");
	}

	//--------------------------------
	//������� ����� ���� Gap 
	if (!waitforsetbit(CommandAndStatusRegister, Gap, false, MAX_WAIT_SET_BIT_TIME))
	{
		ss << "���� ������� ���� ������� ��������������� ����������. " << std::endl;
		ss << " - ����� ���� ������ ���������� ������ ������� (Gap) " << std::endl;
		throw device_exception(ss.str(), "NesmitDevice::lamp()");
	}

	//--------------------------------
	//������� ����� ���� StopEngine 
	if (!waitforsetbit(CommandAndStatusRegister, StopEngine, false, MAX_WAIT_SET_BIT_TIME))
	{
		ss << "���� ������� ���� ������� ��������������� ����������. " << std::endl;
		ss << " - ����� ���� ��������� ��������� (StopEngine) " << std::endl;
		throw device_exception(ss.str(), "NesmitDevice::lamp()");
	}

	//--------------------------------
	//������� ����� ���� CommandRequest 
	if (!waitforsetbit(CommandAndStatusRegister, CommandRequest, false, MAX_WAIT_SET_BIT_TIME))
	{
		ss << "���� ������� ���� ������� ��������������� ����������. " << std::endl;
		ss << " - ����� ���� ������� �� ���������� ������� (CommandRequest) " << std::endl;
		throw device_exception(ss.str(), "NesmitDevice::lamp()");
	}

	//���������� �������� ���� � �������
	if (writeregister(PositionLowRegister, angle) == -1)
	{
		ss << "���� ������� ���� ������� ��������������� ����������. " << std::endl;
		ss << " - ���������  �������� ���� (PositionLowRegister) " << std::endl;
		throw device_exception(ss.str().c_str(), "NesmitDevice::writeangle");
	}

	
	//--------------------------------
	//������� ��������� ���� ���������
	if (!waitforsetbit(CommandAndStatusRegister, CommandBusy, true, MAX_WAIT_SET_BIT_TIME))
	{
		ss << "���� ������� ���� ������� ��������������� ����������. " << std::endl;
		ss << " - ��������� ����� ��������� ���� ��������� (CommandBusy) " << std::endl;
		throw device_exception(ss.str(), "NesmitDevice::lamp()");
	}
}

//�������� ��������� ���� 
double NesmitDevice::progress() const
{
	//���� � ����������� ������
	lock obj(&_nesvitCS);
	
	//�������� ������� ���������
	uint16_t PROGRESS_REGISTER = getregister(ProgressRegister);

	double progress = PROGRESS_REGISTER * 100.0 / 255.0;	
	return progress;
}


/// ������� ������� � ��������
void NesmitDevice::close()
{
	std::stringstream ss;
	
	//���� � ����������� ������
	lock obj(&_nesvitCS);

	//�������� ������� ������ � ���������
	uint16_t COMMAND_AND_STATUS_REGISTER = getregister(CommandAndStatusRegister);

	
	// � ������� �007, ��� 0 - �������� 0.
	uint16_t status = bit::setbit(COMMAND_AND_STATUS_REGISTER, Client, false);

	//������ �������� ������ � ���������	
	if  (writeregister(CommandAndStatusRegister, status) == -1)
	{
		ss << "������ ��� ��������� �������. " << std::endl;
		ss << " - ������ �������� ������ � ��������� (CommandAndStatusRegister)" << std::endl;
		throw device_exception(ss.str().c_str(), "NesmitDevice::close()");
	}

	//--------------------------------
	//������� ����� ���� Client
	if (!waitforsetbit(CommandAndStatusRegister, Client, false, MAX_WAIT_SET_BIT_TIME))
	{
		ss << "������ ��� ��������� �������. " << std::endl;
		ss << " - ����� ���� ���������� ������� � ������ (Client) " << std::endl;
		throw device_exception(ss.str(), "NesmitDevice::close()");
	}	
}
