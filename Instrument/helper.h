#pragma once
#include "pch.h"

namespace bit
{

	///Установить/сбросить бит
	static uint16_t setbit(uint16_t inp, unsigned char bit, bool state = true)
	{
		uint16_t output = inp;
		if (state)
			output |= (1 << bit);  // set
		else
			output &= ~(1 << bit); //clear
		
		return output;
	}

	//Проверить бит на положитьельное состояние
	static bool checkbit(const uint16_t& inp, unsigned char bit)
	{
		const uint16_t mask = (1 << bit);
		const uint16_t res = inp & mask;
		return (res != 0);
	}

	//Проверить бит на наличие определенного состояния (true = 1; false = 0)
	static bool checkbit(const uint16_t& inp, unsigned char bit, bool state)
	{
		const bool cur = checkbit(inp, bit);
		return  cur == state;
	}
	
};

