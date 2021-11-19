#pragma once
#include "pch.h"

class lock
{
public:

	LPCRITICAL_SECTION lpcs;
	
	
	lock( LPCRITICAL_SECTION lpcs )
	{
		this->lpcs = lpcs;
		EnterCriticalSection( lpcs );
	}

	~lock()
	{
		LeaveCriticalSection( lpcs );
	}

	void Release()
	{
		LeaveCriticalSection( lpcs );
	}

};
