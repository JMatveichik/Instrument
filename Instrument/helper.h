#pragma once
#include "pch.h"

namespace helper
{

	template<class T = unsigned char>
	class bitmask
	{
	public:
		bitmask<T>()
		{
			mask[0] = 1;
			for (unsigned i = 1; i < size(); i++)
				mask[i] = mask[i - 1] << 1;
		}

		size_t size() const
		{
			return sizeof(mask) / sizeof(mask[0]);
		}

		T operator [] (unsigned i) const throw(...)
		{
			if (i >= sizeof(T) * 8)
				throw std::out_of_range("bitmask<T>");
			return mask[i];
		}

		bool check_bit(const T& rhs, unsigned i) const { return (rhs & mask[i]) != 0; }

	protected:
		T mask[sizeof(T) * 8];
	};

	///разделить строку на подстроки по разделителю
	static std::vector<std::string> split(std::string src, const char delim)
	{
		std::stringstream ss(src);
		std::vector<std::string> out;

		std::string s;
		while (std::getline(ss, s, delim)) {
			out.push_back(s);
		}

		return out;
	}
	
	//получить праматры Modbus TCP  соединения из строки
	static std::pair<std::string, int> connection(std::string src)
	{
		std::vector<std::string> parts = split(src, ':');
		std::pair<std::string, int> conn;

		
		try {
			conn.first = parts[0];
			conn.second = std::stoi(parts[1]);
		}
		catch(...) 
		{
			conn.first = "";
			conn.second = 0;
		}

		return conn;
	}

	//получить праматры Modbus TCP  соединения из файла
	static std::pair<std::string, int> connection(std::ifstream& inp)
	{
		std::pair<std::string, int> conn;
		
		try {

			std::string line;
			std::getline(inp, line);

			std::vector<std::string> parts = split(line, ':');			
		
			conn.first = parts[0];
			conn.second = std::stoi(parts[1]);
		}
		catch (...)
		{
			conn.first = "";
			conn.second = 0;
		}

		return conn;
	}

};

