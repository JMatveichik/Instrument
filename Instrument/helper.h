#pragma once
#include "pch.h"

namespace helper
{
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

};

