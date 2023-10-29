#pragma once

class device_exception : public std::exception
{
    
public:
    // Конструктор класса, который принимает сообщение об ошибке
    device_exception(const std::string& message, const std::string& caption) :  _message(message), _caption(caption)
    {	    
    }
    
    // Переопределение метода what() для предоставления сообщения об ошибке
    const char* what() const noexcept override
	{
        return _message.c_str();
    }
	
    // Метод caption () для заголовка сообщения об ошибке
    const char* caption() const noexcept
    {
        return _caption.c_str();
    }

protected:
	
    mutable std::string _message;
    std::string _caption;
};

class device_connection_exception : public device_exception
{
public:
    // Конструктор класса, который принимает сообщение об ошибке
    device_connection_exception(const std::string& message, const std::string& caption, const std::string& ip, const int& port) : device_exception(message, caption), _ip(ip), _port(port)
    {
    }

    // Переопределение метода what() для предоставления сообщения об ошибке
    const char* what() const noexcept override
    {
        std::stringstream ss;

        ss << _message << std::endl;
        ss << "Ошибка соединения со  спректрографом " << "(" << ip() << ":" << port() << ")" << std::endl;
        ss << "Выполните  следующее :" << std::endl;
        ss << "  - Убедитесь что питание прибора включено" << std::endl;
        ss << "  - Убедитесь что прибор подключен к локальной сети" << std::endl;
        ss << "  - Отредактируйте файл <connect.txt> в соответствии с реальным адресом устройства (IP:PORT )" << std::endl;

        _message = ss.str();    	
        return _message.c_str();
    }

    // Метод ip () для предоставления об IP адресу устройства
    const char* ip() const noexcept
    {
        return _ip.c_str();
    }

    // Метод port () для предоставления об номере порта устройства
    const int port() const noexcept
    {
        return _port;
    }
	
protected:
	
    std::string _ip;
    int _port;
};