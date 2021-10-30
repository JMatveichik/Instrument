// InstrumentDllTester.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include "InstrumentWrapper.h"


void TestShutter(InstrumentWrapper& wr, unsigned char state);

int main()
{	

	std::system("chcp 1251");

	InstrumentWrapper wr;
	if (!wr.Load("Instrument.dll"))
		std::cout << "Ошибка загрузки Instrument.dll\n";

	std::string str = "127.0.0.1:502";

	std::cout << "Подключение к НЕСМИТ ... " << str << std::endl;

	if (!wr.Initialize(str.c_str()))
		std::cout << "Ошибка  подключения к НЕСМИТ... " << str << std::endl;
	else
		std::cout << "Подключение к НЕСМИТ установлено ... " << str << std::endl;
		
	
	TestShutter(wr, 1);
	TestShutter(wr, 0);
	
	
}

void TestShutter(InstrumentWrapper& wr, unsigned char state)
{
	std::cout << "Тестирование Shutter : " << (char)state << std::endl;

	if (!wr.Shutter(state)) {
		std::cout << "Ошибка  Shutter... "  << std::endl;
	}
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
