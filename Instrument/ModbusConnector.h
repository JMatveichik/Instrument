#pragma once
#include  <modbus.h>
#include  "helper.h"
#include  "pch.h"

class ModbusConnector
{
	private:

		modbus_t* mb;

	public:
		

		ModbusConnector( const char* connectstring)
		{
			mb = nullptr;
			connect(mb, connectstring);
		}

		~ModbusConnector()
		{
			Release();
		}

		modbus_t* Get()
		{
			return mb;
		}

		void Release()
		{
			modbus_close(mb);
			modbus_free(mb);
			mb = nullptr;
		}

		bool isConnected()
		{
			return (connected != -1);
		}


	private:

		//���� ����������
		int connected = -1;
		
		bool connect(modbus_t* mb, const char* connectstring)
		{	

			//���� �� ����������� ������� ��������� �� ����� � ������� ���������� "connect.txt"
			if (connectstring == nullptr)
			{
				std::ifstream input;
				input.open("connect.txt");

				if (input.bad())
					return false;

				std::pair<std::string, int> opt = helper::connection(input);
				mb = modbus_new_tcp(opt.first.c_str(), opt.second);
				connected = modbus_connect(mb);

				modbus_set_slave(mb, 1);

				return (connected == -1) ? false : true;
			}

			// ���� �������� ������ ������ � ���� "ipaddress:port" "192.168.10.18:502" 
			std::pair<std::string, int> opt = helper::connection(connectstring);
			mb = modbus_new_tcp(opt.first.c_str(), opt.second);
			connected = modbus_connect(mb);

			//���� �� ����������� ������� ��������� �� ����� path
			if (connected == -1)
			{
				std::ifstream input;
				input.open(connectstring);

				std::pair<std::string, int> opt = helper::connection(input);
				mb = modbus_new_tcp(opt.first.c_str(), opt.second);
				connected = modbus_connect(mb);

				modbus_set_slave(mb, 1);
			}

			//���� �� ����������� ������� ��������� �� ����� � ������� ���������� "connect.txt"
			if (connected == -1)
			{
				std::ifstream input;
				input.open("connect.txt");

				std::pair<std::string, int> opt = helper::connection(input);
				mb = modbus_new_tcp(opt.first.c_str(), opt.second);
				connected = modbus_connect(mb);

				modbus_set_slave(mb, 1);
			}

			//���� �� ������� ���������� ����������  false  
			return connected != -1;
		}
	
};

