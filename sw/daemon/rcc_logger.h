#ifndef __RCC_LOGGER_H
#define __RCC_LOGGER_H

#include <string>
#include <iostream>
#include <fstream>


// Singleton class

class rccLogger {
public:
    enum rccLoggerLevel {
        rccLoggerError = 1,
        rccLoggerDebug = 2
    };
    enum rccLoggerOutputs {
        rccLoggerRam  = 1,
        rccLoggerFile = 2,
        rccLoggerOut  = 4,
        rccLoggerErr  = 8,
        rccLoggerCB   = 16 // callback
    };

    typedef void (*logFuncCb)(std::string &);

    static rccLogger& getInstance()
    {
        static rccLogger instance;
        return instance;
    }

    int setFilename(std::string fName);
    int setCallback(logFuncCb cbFunc);

    int setLogging(int level, int outputs);
    int print(int level, std::string &str);
    int error(std::string str) { return print(rccLoggerErr, str); };
    int debug(std::string str) { return print(rccLoggerOut, str); };

    void clearLog(void);
    void getLog(std::string &log);

private:
    rccLogger(void);

    rccLogger(rccLogger const &) = delete;
    void operator=(rccLogger const &) = delete;

    ~rccLogger(void);

    // TODO: log rotation or at least protection we don't grow too much
    std::string   mLog;
    std::string   mLogName;
    std::filebuf  mLogBuf;
    std::ostream  mLogStream;
    int           mLevel, mOutputs;
    logFuncCb     mCbFunc;
};

#define getLogger() rccLogger::getInstance()

#endif // __RCC_LOGGER_H
