#include <rcci_server.h>
#include <rcc_logger.h>
#include <rcc_sys_ctrl.h>

static rcciServer *myServer = NULL;
static rccSysCtrl *mySysCtrl = NULL;

// TODO: Try to add call-backs a little bit nicer
static void pushLogToClients(std::string &str)
{
    if(myServer)
    {
        myServer->writeServiceLog(str);
    }
}

static void pushDataToDrvCtrl(rcci_msg_drv_ctrl_t drvCtrlData)
{
    if(mySysCtrl)
    {
        mySysCtrl->pushDriveData(drvCtrlData);
    }
}

int main(int argc, char *argv[])
{
    int port = 1025;

    myServer = new rcciServer();
    mySysCtrl = new rccSysCtrl();

    if(argc == 2)
    {
        port = atoi(argv[1]);
    }

    if(!mySysCtrl->isInitialized())
    {
        std::cerr << "Can not initialize drive control class!" << std::endl;
        return -1;
    }

    // setup logging
//    getLogger().setFilename("log.txt");
    getLogger().setLogging(rccLogger::rccLoggerDebug|rccLogger::rccLoggerError,
                           rccLogger::rccLoggerRam|rccLogger::rccLoggerFile|
                           rccLogger::rccLoggerOut|rccLogger::rccLoggerErr |
                           rccLogger::rccLoggerCB);

    // set up the connection between various parts
    getLogger().setCallback((rccLogger::logFuncCb)&pushLogToClients);

    // TODO: Change the structure and rather pass mySysCtrl pointer
    // to rcciServer class directly - we need to control more things
    // (for example PWM output mux, enable/disable PWM, ...)
    myServer->setDriveDataCb((rccSysCtrl::driveFuncCb)&pushDataToDrvCtrl);

    // setup server
    if(myServer->openServer(port) < 0)
    {
        return -1;
    }


    // TODO: Check comment above - rccSysCtrl should be passed directly
    // to rcciServer and instead of callbacks just control directly
    mySysCtrl->pwmEnable(true);


    while(true)
    {

    }

    myServer->closeServer();

    delete mySysCtrl;
    delete myServer;

    return 0;
}
