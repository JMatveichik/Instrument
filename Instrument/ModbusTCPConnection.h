#pragma once

template<class T=unsigned char>
class bitmask
{
public:
	bitmask<T>()
	{
		mask[0] = 1;
		for( unsigned i=1; i<size(); i++ )
			mask[i] = mask[i-1] << 1;
	}

	size_t size() const 
	{ 
		return sizeof(mask)/sizeof(mask[0]); 
	}
	
	T operator [] (unsigned i) const throw(...) 
	{ 
		if (i>=sizeof(T) * 8) 
			throw std::out_of_range("bitmask<T>"); 
		return mask[i]; 
	}

	bool check_bit(const T& rhs, unsigned i) const { return (rhs & mask[i]) != 0; }

protected:
	T mask[sizeof(T) * 8];
};

const unsigned MP_MAX_SIZE = 265;

typedef enum tagMODBUS_COMMAND:unsigned char {	
	ReadCoilStatus=1,			// Read Discrete Output Bit
	ReadInputStatus,			// Read Discrete Input Bit
	ReadHoldingRegisters,		// Read 16-bit register. Used to read integer or floating point process data
	ReadInputRegisters,			// --//--
	ForceSingleCoil,			// Write data to force coil ON/OFF
	PresetSingleRegister,		// Write data in 16-bit integer format
	LoopbackDiagnosis=8,		// Diagnostic testing of the communication port
	ForceMultipleCoils=15,		// Write multiple data to force coil ON/OFF
	PresetMultipleRegisters=16	// Write multiple data in 16-bit integer format
} MODBUS_COMMAND;

#pragma pack(push)
#pragma pack(1)

typedef struct tagMODBUS_HEADER
{
public:
	unsigned short TxID;
	unsigned short ProtID;
	unsigned short length;
private:
	unsigned char  reserved;

public:
	tagMODBUS_HEADER();
} MODBUS_HEADER, *LPMODBUS_HEADER;

typedef struct tagMODBUS_REQUEST : public tagMODBUS_HEADER
{
public:
	MODBUS_COMMAND command;
	unsigned char  data[MP_MAX_SIZE];

public:
	tagMODBUS_REQUEST();

	bool Send(SOCKET sock, MODBUS_COMMAND cmd, const char* data, unsigned data_size, std::ostream* pLog=NULL);

	unsigned ResponseLength() const { return length; }
	const unsigned char* Response() const { return &data[1]; }

} MODBUS_REQUEST, *LPMODBUS_REQUEST;

#pragma pack(pop)

typedef bool (*LPFNTCPFAILURE)(void*); // Returns false if no retry necessary

class CModbusTCPConnection
{
public:
	CModbusTCPConnection() : sock(INVALID_SOCKET), moduleID(0), lpfnErrorCallback(NULL) 
	{
		::InitializeCriticalSection(&m_cs);
	}
	~CModbusTCPConnection()
	{
		::DeleteCriticalSection(&m_cs);
	}

	CModbusTCPConnection(const CModbusTCPConnection& rhs);
	const CModbusTCPConnection& operator=(const CModbusTCPConnection& rhs);

	static void EnableVirtualServer(std::string strIP) { strVirtualServer = strIP; }
	static void SetTimeout(unsigned read, unsigned write, unsigned mulfunction) { nReadTimeout = read; nWriteTimeout = write, nMulfunctionTimeout = mulfunction; }

	void EnableCallback(LPFNTCPFAILURE fn, LPVOID par) { 
		lpfnErrorCallback = fn;
		lpPar = par;
	}

	bool Establish(std::string strIP, unsigned nPort, std::ostream* pLog=NULL);
	bool Establish(std::string str, std::ostream* pLog = NULL);
	bool Establish(std::ifstream& inp, std::ostream* pLog = NULL);

	void Finalize();



	bool Transact(MODBUS_COMMAND cmd, const char* data, unsigned data_size, std::vector<unsigned char>& response, std::ostream* pLog=NULL);

private:

	MODBUS_REQUEST request;

	SOCKET sock;
	sockaddr_in saModule;	
	unsigned short moduleID;

	static unsigned nReadTimeout;
	static unsigned nWriteTimeout;
	static unsigned nMulfunctionTimeout;
	static std::string strVirtualServer;

	LPFNTCPFAILURE lpfnErrorCallback;
	LPVOID lpPar;


	CRITICAL_SECTION m_cs;

	static SOCKET CreateSocket();
};