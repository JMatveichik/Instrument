#pragma once
#include <string>
#include <windows.h>

#ifdef _M_X64
	#define INSTRUMENTFN
#else
	#define INSTRUMENTFN __stdcall
#endif

typedef bool (INSTRUMENTFN *LPFNINITIALIZE)(const char* path);
typedef bool (INSTRUMENTFN *LPFNSHUTTER)(unsigned char state);
typedef bool (INSTRUMENTFN *LPFNFILTER)(int value);
typedef bool (INSTRUMENTFN *LPFNLAMP)(const char *kind, unsigned char state);
typedef bool (INSTRUMENTFN *LPFNDISP)(unsigned char n, const char *state);
typedef bool (INSTRUMENTFN *LPFNSLIT)(const char *state);
typedef bool (INSTRUMENTFN *LPFNÑLOSEINST)();
typedef bool (INSTRUMENTFN *LPFNGETZERO)(const char *state);


#ifdef _M_X64
	#define FUNNAME_INITIALIZE	"InitInst"
	#define FUNNAME_SHUTTER		"Shutter"
	#define FUNNAME_FILTER		"Filter"
	#define FUNNAME_LAMP		"Lamp"
	#define FUNNAME_DISP		"Disp"
	#define FUNNAME_SLIT		"Slit"
	#define FUNNAME_CLOSEINST   "CloseInst"
	#define FUNNAME_GETZERO     "GetZero"	
#else
	#define FUNNAME_INITIALIZE	"_InitInst@4"
	#define FUNNAME_SHUTTER		"_Shutter@4"
	#define FUNNAME_FILTER		"_Filter@4"
	#define FUNNAME_LAMP		"_Lamp@8"
	#define FUNNAME_DISP		"_Disp@8"
	#define FUNNAME_SLIT		"_Slit@4"
	#define FUNNAME_CLOSEINST   "_CloseInst@0"
	#define FUNNAME_GETZERO     "_GetZero@4"	
#endif



class InstrumentWrapper 
{	

	///çàãðóèòü èç áèáëåîòåêè
public:

	InstrumentWrapper();

	bool Load(const char* dllpath);

	bool Initialize(const char* dllpath);

	bool Shutter(unsigned char state);

	bool Filter(int filter);

	bool Lamp(const char *kind, unsigned char state);

	bool Disp(unsigned char n, const char *state);

	bool Slit(const char *state);

	bool CloseInstance();

	bool GetZero(const char *state);


private:

	HMODULE m_hDllInstance;		///< Äåñêðèïòîð ìîäóëÿ 


	LPFNINITIALIZE	m_lpInitFunc;
	LPFNSHUTTER		m_lpShutterFunc;		
	LPFNFILTER		m_lpFilterFunc;
	LPFNLAMP		m_lpLampFunc;
	LPFNDISP		m_lpDispFunc;
	LPFNSLIT		m_lpSlitFunc;
	LPFNÑLOSEINST	m_lpCloseInstanceFunc;
	LPFNGETZERO		m_lpGetZeroFunc;
};

