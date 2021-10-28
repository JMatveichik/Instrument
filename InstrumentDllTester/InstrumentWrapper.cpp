#include "InstrumentWrapper.h"

InstrumentWrapper::InstrumentWrapper()
{
	m_hDllInstance = nullptr;
}

bool InstrumentWrapper::Load(const char* dllpath)
{

	if (m_hDllInstance)
	{
		FreeLibrary(m_hDllInstance);
		m_hDllInstance = NULL;
	}

	m_hDllInstance = LoadLibrary(dllpath);

	if (!m_hDllInstance)
		return false;

#define CHECK_FUNCTION_POINTER(a) if (!(a)) return false;

	// Initialize function pointers
	CHECK_FUNCTION_POINTER(m_lpInitFunc		= (LPFNINITIALIZE)GetProcAddress(m_hDllInstance, FUNNAME_INITIALIZE));
	CHECK_FUNCTION_POINTER(m_lpShutterFunc	= (LPFNSHUTTER)GetProcAddress(m_hDllInstance, FUNNAME_SHUTTER));
	CHECK_FUNCTION_POINTER(m_lpFilterFunc	= (LPFNFILTER)GetProcAddress(m_hDllInstance, FUNNAME_FILTER));
	CHECK_FUNCTION_POINTER(m_lpLampFunc		= (LPFNLAMP)GetProcAddress(m_hDllInstance, FUNNAME_LAMP));
	CHECK_FUNCTION_POINTER(m_lpDispFunc		= (LPFNDISP)GetProcAddress(m_hDllInstance, FUNNAME_DISP));
	CHECK_FUNCTION_POINTER(m_lpSlitFunc		= (LPFNSLIT)GetProcAddress(m_hDllInstance, FUNNAME_SLIT));
	CHECK_FUNCTION_POINTER(m_lpCloseInstanceFunc = (LPFNÑLOSEINST)GetProcAddress(m_hDllInstance, FUNNAME_CLOSEINST));
	CHECK_FUNCTION_POINTER(m_lpGetZeroFunc = (LPFNGETZERO)GetProcAddress(m_hDllInstance, FUNNAME_GETZERO));
	

	return true;
}

bool InstrumentWrapper::Initialize(const char* str)
{
	if (m_lpInitFunc)
		return m_lpInitFunc(str);

	return false;
}