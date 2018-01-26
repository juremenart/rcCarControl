#include <unistd.h>
#include <netdb.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "rcc_logger.h"

rccLogger::rccLogger(void)
    : mLogStream(nullptr), mCbFunc(NULL)
{
    mLog.clear();
    mLevel = rccLoggerError;
    mOutputs = rccLoggerErr;
}

rccLogger::~rccLogger(void)
{
    if(mLogBuf.is_open())
    {
        mLogBuf.close();
    }
    mLogName.clear();
}

int rccLogger::setFilename(std::string fName)
{
    mLogName = fName;
    mLogBuf.open(mLogName.c_str(), std::ios::out|std::ios::trunc);

    if(!mLogBuf.is_open())
    {
        std::cerr << "Can not open logging file" << std::endl;
        return -1;
    }

    mLogStream.rdbuf(&mLogBuf);
    return 0;
}

int rccLogger::setCallback(logFuncCb cbFunc)
{
    mCbFunc = cbFunc;
    return 0;
}

int rccLogger::setLogging(int level, int outputs)
{
    mLevel = level;
    mOutputs = outputs;

    return 0;
}

int rccLogger::print(int level, std::string &str)
{
    // implement level checks
    if(level == rccLoggerErr)
    {
        std::cerr << str;
    }
    else
    {
        std::cout << str;
    }

    if(mLogBuf.is_open())
    {
        mLogStream << str << std::flush;
    }

    if((mOutputs & rccLoggerCB) && mCbFunc)
    {
        mCbFunc(str);
    }

    mLog.append(str);

    return 0;
}

void rccLogger::clearLog(void)
{
    mLog.clear();
}

void rccLogger::getLog(std::string &log)
{
    log = mLog;
}
