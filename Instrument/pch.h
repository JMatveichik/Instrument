// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

#define _DLL_MASTER_

#pragma comment(lib, "Ws2_32.lib")
#pragma comment (lib, "modbus.lib")

#define _WINSOCK_DEPRECATED_NO_WARNINGS

// add headers that you want to pre-compile here
#include "framework.h"

#include <modbus.h>
#include <tlhelp32.h>

#include <windows.h>
#include <CommCtrl.h>

//STL
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <bitset>
#include <chrono>

#endif //PCH_H
