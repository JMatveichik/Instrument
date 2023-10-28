#pragma once

#include <fstream>
#include <string>

enum class LogLevel {
    Error,
    Warning,
    Info
};

//-------------------------------------------------------------------------------
class ILogger
{
public:
	
    virtual void Log(LogLevel level, const std::string& message) = 0;
    virtual ~ILogger() {}

protected:
	
    virtual std::string GetLogLevelString(LogLevel level);
};

//-------------------------------------------------------------------------------
class FileLogger : public ILogger
{
public:
    FileLogger(const std::string& filename);
    ~FileLogger();
	
    void Log(LogLevel level, const std::string& message) override;
	
protected:

    std::string FormatLogEntry(const std::string& levelStr, const std::string& message);	

protected:
    std::ofstream logFile;
};


//-------------------------------------------------------------------------------
class XmlLogger : public FileLogger
{
public:
    XmlLogger(const std::string& filename);
    ~XmlLogger();

    void Log(LogLevel level, const std::string& message) override;
};