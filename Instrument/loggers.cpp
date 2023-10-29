#include "pch.h"
#include "loggers.h"

#include <time.h>


std::string ILogger::GetLogLevelString(LogLevel level)
{
    switch (level) {
    case LogLevel::Error:
        return "Error";
    case LogLevel::Warning:
        return "Warning";
    case LogLevel::Info:
        return "Info";
    default:
        return "Unknown";
    }
}


FileLogger::FileLogger(const std::string& filename)
{
    logFile.open(filename, std::ios::out | std::ios::app);
    if (!logFile.is_open())
    {
        throw std::exception("Failed to open the log file.");
    }
	
    logFile << "-------------- START LOG --------------" << std::endl;
}

FileLogger::~FileLogger()
{
    if (logFile.is_open())
    {
        logFile.close();
    }
}

void FileLogger::Log(LogLevel level, const std::string& message)
{
    if (!logFile.is_open())
    {
        throw std::exception("Log file is not open.");        
    }

    std::string logLevelStr = GetLogLevelString(level);
    std::string formattedLog = FormatLogEntry(logLevelStr, message);
	
    logFile << formattedLog << std::endl;
    logFile.flush();
}


std::string FileLogger::FormatLogEntry(const std::string& levelStr, const std::string& message)
{
    time_t now = time(0);
    struct tm currentTime;
    localtime_s(&currentTime, &now);

    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &currentTime);

    std::ostringstream formattedLog;
    formattedLog << "[" << timestamp << "] [" << levelStr << "] " << message;

    return formattedLog.str();
}

//--------------------------------------------------------------------------------

XmlLogger::XmlLogger(const std::string& filename) : FileLogger(filename)
{
    if (logFile.is_open())
    {
        logFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
        logFile << "<log>" << std::endl;
    }
}

XmlLogger::~XmlLogger()
{
    if (logFile.is_open())
    {
        logFile << "</log>" << std::endl;
    }
}

void XmlLogger::Log(LogLevel level, const std::string& message)
{
    if (!logFile.is_open())
    {
        throw std::exception("XML log file is not open.");        
    }

    std::string logLevelStr = GetLogLevelString(level);
    std::string formattedLog = FormatLogEntry(logLevelStr, message);
	
    logFile << "  <logEntry>" << std::endl;
    logFile << "    <timestamp>" << formattedLog << "</timestamp>" << std::endl;
    logFile << "  </logEntry>" << std::endl;
}
