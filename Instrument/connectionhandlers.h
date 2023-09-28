#include <fstream>
#include <string>
#include <regex>
#include <stdexcept>
#include <utility> // Для std::pair

class ConnectionHandler
{

public:

	ConnectionHandler* setNext(ConnectionHandler* nextHandler)
	{
        nextHandler_ = nextHandler;
        return nextHandler_;
    }

    virtual std::pair<std::string, int> getConnectionData(const std::string& connectionString) = 0;

    virtual ~ConnectionHandler()
    {
        delete nextHandler_;
    }

protected:
	
    ConnectionHandler* nextHandler_ = nullptr;

    std::pair<std::string, int> ParseConnectionString(const std::string& connectionString)
	{
        size_t colonPos = connectionString.find(':');
    	
        if (colonPos != std::string::npos) 
        {
            std::string ipAddress = connectionString.substr(0, colonPos);
            std::string portStr = connectionString.substr(colonPos + 1);

            if (!ValidateIPAddress(ipAddress)) {
                throw std::invalid_argument("Invalid IP address format: " + ipAddress);
            }

            try 
            {
                int port = std::stoi(portStr);
                if (!ValidatePort(port)) 
                {
                    throw std::invalid_argument("Invalid port number: " + portStr);
                }
                return std::make_pair(ipAddress, port);
            }
            catch (const std::exception& e) 
            {
                throw std::invalid_argument("Invalid port number: " + portStr);
            }
        }
        throw std::invalid_argument("Invalid connectionString format: " + connectionString);
    }

private:

	bool ValidateIPAddress(const std::string& ipAddress)
	{
        std::regex ipRegex(R"(\b\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\b)");
        return std::regex_match(ipAddress, ipRegex);
    }

    bool ValidatePort(int port)
	{
        return (port >= 1 && port <= 65535);
    }
};

class FileStringHandler : public ConnectionHandler
{
protected:
	std::string getConnectionString(const std::string& filepath)
	{
        std::ifstream file(filepath);
        if (file.is_open())
        {
            std::string fileConnectionString;
            std::getline(file, fileConnectionString);

            file.close();
            return fileConnectionString;
        }
	}
	
public:
    virtual std::pair<std::string, int> getConnectionData(const std::string& filepath) override
    {
    	try {
			std::string fileConnectionString = getConnectionString(filepath);
            return ParseConnectionString(fileConnectionString);
        }
    	catch (...)
        {
            // Передача управления следующему обработчику в цепочке
            if (nextHandler_) {
                return nextHandler_->getConnectionData(filepath);
            }
    	}
    }
};

class EmptyConnectionStringHandler : public FileStringHandler
{
public:
    std::pair<std::string, int> getConnectionData(const std::string& connectionString) override
	{
        return FileStringHandler::getConnectionData("connect.txt");
    }
};

class FileConnectionStringHandler : public FileStringHandler
{

public:

	std::pair<std::string, int> getConnectionData(const std::string& connectionString) override
	{
        return FileStringHandler::getConnectionData(connectionString);
        
    }
};

class DirectConnectionStringHandler : public ConnectionHandler
{

public:
    std::pair<std::string, int> getConnectionData(const std::string& connectionString) override
	{
        try
    	{
            // Разбор connectionString
            return ParseConnectionString(connectionString);            
    	}
    	catch(...)
    	{
            // Передача управления следующему обработчику в цепочке
            if (nextHandler_) 
            {
                return nextHandler_->getConnectionData(connectionString);
            }
    	}
        return {};
    }
};

class ConnectionStringHandlers
{
public:
	
	static std::pair<std::string, int> getConnectionData(const std::string& connectionString)
	{
        std::unique_ptr<ConnectionHandler> handlerChain (new EmptyConnectionStringHandler());
        handlerChain->setNext(new FileConnectionStringHandler())
					->setNext(new DirectConnectionStringHandler());

        return handlerChain->getConnectionData(connectionString);
	}
};
