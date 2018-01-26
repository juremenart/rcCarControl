#include <cstring>
#include <cerrno>
#include <iostream>
#include <unistd.h>

#include "rcci_client.h"


const int cLogMaxBufRead(1024);

rcciClient::rcciClient(void)
    : mPort(-1),mSockFd(-1),mServer(NULL),
      mMagic(0), mVersion(0), mLogPort(-1), mLogFd(-1),
      mDrvPort(-1), mDrvFd(-1)
{

}

rcciClient::~rcciClient(void)
{
    drvDisconnect();
    logDisconnect();
    disconnect();
}

bool rcciClient::isConnected(void)
{
    if((mPort > 0) && (mSockFd > 0) && mServer)
    {
        return true;
    }
    else
    {
        return false;
    }
}

int rcciClient::connect(const std::string hostname, const int port)
{
    int sockFd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockFd < 0)
    {
        std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
        return -1;
    }

    // TODO: implement client/server connect with getaddrinfo()!
    mServer = gethostbyname(hostname.c_str());
    if(!mServer)
    {
        std::cerr << "Error in gethostbyname(): " <<
            strerror(errno) << std::endl;
        return -1;
    }

    memset(&mServAddr, 0, sizeof(mServAddr));
    mServAddr.sin_family = AF_INET;
    memcpy((char *)&mServAddr.sin_addr,(char *)mServer->h_addr,
           mServer->h_length);
    mServAddr.sin_port = htons(port);
    std::cerr << "Connecting to " << mServer->h_name << std::endl;

    if(::connect(sockFd, (struct sockaddr *)&mServAddr, sizeof(mServAddr)) < 0)
    {
        std::cerr << "Error connecting to server " << hostname << ":" <<
            port << ": " << strerror(errno) <<std::endl;
        return -1;
    }

    mSockFd = sockFd;
    mPort = port;

    // also initialize connection with server
    return sendInitMsg();
}

int rcciClient::disconnect(void)
{
    if(!isConnected())
    {
        return 0;
    }

    if(mSockFd > 0)
    {
        close(mSockFd);
        mSockFd = -1;
    }

    if(mServer)
    {
        // TODO: how to clean/free this?
//        free(mServer);
        mServer = NULL;
    }

    mPort = -1;
    return 0;
}

int rcciClient::sendInitMsg(void)
{
    rcci_msg_init_t initMsg;

    ssize_t bytes;

    if(!isConnected())
    {
        std::cerr << "sendInitMsg() not connected to server" << std::endl;
        return -1;
    }

    initMsg.header.type  = rcci_msg_init;
    initMsg.header.magic = rcci_msg_init_magic;
    initMsg.header.ver   = rcci_msg_init_ver;
    initMsg.header.size  = sizeof(rcci_msg_init_t);

    bytes = write(mSockFd, &initMsg, sizeof(rcci_msg_init_t));
    if(bytes < 0)
    {
        std::cerr << "write() failed (" << bytes << "): " <<
            strerror(errno) << std::endl;
        return -1;
    }

    // wait for response and then close the session
    bytes = read(mSockFd, &initMsg, sizeof(rcci_msg_init_t));
    if(bytes < 0)
    {
        std::cerr << "read() failed (" << bytes << "): " <<
            strerror(errno) << std::endl;
        return -1;
    }

    if((initMsg.status != rcci_status_ack) ||
       (bytes != sizeof(rcci_msg_init_t)))
    {
        std::cerr << "read() failed (" << bytes << " != " <<
            sizeof(rcci_msg_init_t) << ") or wrong status: " <<
            initMsg.status << std::endl;
        return -1;
    }

    mMagic   = initMsg.header.magic;
    mVersion = initMsg.header.ver;
    mLogPort = initMsg.log_port;
    mDrvPort = initMsg.drv_port;

    return 0;
}

int rcciClient::serviceConnect(int aServPort, int &aServFd,
                               struct sockaddr_in &aServAddr)
{
    if(aServPort <= 0)
    {
        std::cerr << "drvConnect() unknown logging port: " <<
            mDrvPort << std::endl;
        return -1;
    }

    int sockFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sockFd < 0)
    {
        std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
        return -1;
    }

    // the same server but different port
    memcpy((void *)&aServAddr, (void *)&mServAddr, sizeof(mServAddr));
    aServAddr.sin_port = htons(aServPort);
    std::cerr << "Service connecting to port " << aServPort << std::endl;
    if(::connect(sockFd, (const struct sockaddr *)&aServAddr,
                 sizeof(aServAddr)) < 0)
    {
        std::cerr << "Error connecting to service on port " <<
            aServPort << std::endl;
        return -1;
    }

    aServFd = sockFd;

    return aServFd;
}

int rcciClient::logConnect(std::string &aStr, bool fullLog)
{
    if(serviceConnect(mLogPort, mLogFd, mLogAddr) < 0)
    {
        return -1;
    }

    if(registerLog(fullLog, aStr) < 0)
    {
        return -1;
    }

    return mLogFd;
}

int rcciClient::logDisconnect(void)
{
    if(mLogFd < 0)
    {
        return 0;
    }

    unregisterService(rcci_client_flag_log, mLogFd);

    close(mLogFd);
    mLogFd = -1;
    return 0;
}

int rcciClient::logReadData(std::string &aStr)
{
    ssize_t bytes;
    char buf[cLogMaxBufRead];

    if(!isConnected())
    {
        std::cerr << "logReadData() client not connected" << std::endl;
        return -1;
    }

    struct sockaddr_in sockAddr;
    socklen_t sockLen;
    memcpy(&sockAddr, &mLogAddr, sizeof(struct sockaddr_in));

    bytes = recvfrom(mLogFd, &buf, cLogMaxBufRead, 0,
                     (struct sockaddr *)&sockAddr, &sockLen);
    if(bytes < 0)
    {
        std::cerr << "logReadData() recvfrom() failed: " <<
            strerror(errno) << std::endl;
        return -1;
    }

    aStr = std::string(buf, bytes);

    return bytes;
}

int rcciClient::drvConnect(void)
{
    if(serviceConnect(mDrvPort, mDrvFd, mDrvAddr) < 0)
    {
        return -1;
    }

    if(registerService(rcci_client_flag_drive, mDrvFd, 0) < 0)
    {
        return -1;
    }

    memset(&mDrvMsg, 0, sizeof(mDrvMsg));

    return mDrvFd;
}

int rcciClient::drvDisconnect(void)
{
    if(mDrvFd < 0)
    {
        return 0;
    }

    unregisterService(rcci_client_flag_drive, mDrvFd);

    close(mDrvFd);
    mDrvFd = -1;
    return 0;
}

int rcciClient::drvSendData(int32_t drive, int32_t steer)
{
    mDrvMsg.count++;
    mDrvMsg.drive = drive;
    mDrvMsg.steer = steer;

    ssize_t bytes = sendto(mDrvFd, &mDrvMsg, sizeof(mDrvMsg), 0,
                           (struct sockaddr *)&mDrvAddr, sizeof(mDrvAddr));
    if(bytes < 0)
    {
        std::cerr << "drvSendData() problems with sendto(): " <<
            strerror(errno) << std::endl;
        return -1;
    }
    else if(bytes != sizeof(mDrvMsg))
    {
        std::cerr << "drvSendData() send bytes are not as expected: " << bytes
                  << " != " << sizeof(mDrvMsg) << std::endl;
    }

    return bytes;
}

int rcciClient::registerService(rcci_client_flags_t service, int srvFd,
                                const int params)
{
    rcci_msg_reg_service_t msg;
    ssize_t bytes;

    struct sockaddr_in sockAddr;
    socklen_t sockLen = sizeof(struct sockaddr_in);
    if(getsockname(srvFd, (struct sockaddr *)&sockAddr,
                   &sockLen) < 0)
    {
        std::cerr << "Error getting socket name: " <<
            strerror(errno) << std::endl;
        return -1;
    }

    msg.header.type  = rcci_msg_reg_service;
    msg.header.magic = mMagic;
    msg.header.ver   = mVersion;
    msg.header.size  = sizeof(rcci_msg_reg_service_t);
    msg.service      = service;
    memcpy(&msg.sockaddr, &sockAddr, sizeof(struct sockaddr_in));
    msg.params       = params;

    bytes = write(mSockFd, &msg, sizeof(rcci_msg_reg_service_t));
    if(bytes < 0)
    {
        std::cerr << "write() failed (" << bytes << "): " <<
            strerror(errno) << std::endl;
        return -1;
    }

    // wait for response
    bytes = read(mSockFd, &msg, sizeof(rcci_msg_reg_service_t));
    if(bytes < 0)
    {
        std::cerr << "read() failed (" << bytes << "): " <<
            strerror(errno) << std::endl;
        return -1;
    }

    if(msg.status != rcci_status_ack)
    {
        std::cerr << "Service registering denied by server!" << std::endl;
        return -1;
    }

    if((msg.header.magic != mMagic)               ||
       (msg.header.ver   != mVersion)             ||
       (msg.header.type  != rcci_msg_reg_service) ||
       (bytes != sizeof(rcci_msg_reg_service_t)))
    {
        std::cerr << "read() failed (" << bytes << " != " <<
            sizeof(rcci_msg_reg_service_t) <<
            ") or wrong reply (like status: " << msg.status << ", magic: 0x" <<
            std::hex << msg.header.magic << ", ver: 0x" << msg.header.ver <<
            ")" << std::endl;
        return -1;
    }

    return msg.header.size;
}

int rcciClient::unregisterService(rcci_client_flags_t service, int srvFd)
{
    rcci_msg_reg_service_t msg;
    ssize_t bytes;

    struct sockaddr_in sockAddr;
    socklen_t sockLen = sizeof(struct sockaddr_in);
    if(getsockname(srvFd, (struct sockaddr *)&sockAddr,
                   &sockLen) < 0)
    {
        std::cerr << "Error getting socket name: " <<
            strerror(errno) << std::endl;
        return -1;
    }

    msg.header.type  = rcci_msg_unreg_service;
    msg.header.magic = mMagic;
    msg.header.ver   = mVersion;
    msg.header.size  = sizeof(rcci_msg_reg_service_t);
    msg.service      = service;
    memcpy(&msg.sockaddr, &sockAddr, sizeof(struct sockaddr_in));
    msg.params       = 0;

    bytes = write(mSockFd, &msg, sizeof(rcci_msg_reg_service_t));
    if(bytes < 0)
    {
        std::cerr << "write() failed (" << bytes << "): " <<
            strerror(errno) << std::endl;
        return -1;
    }

    // wait for response
    bytes = read(mSockFd, &msg, sizeof(rcci_msg_reg_service_t));
    if(bytes < 0)
    {
        std::cerr << "read() failed (" << bytes << "): " <<
            strerror(errno) << std::endl;
        return -1;
    }

    if(msg.status != rcci_status_ack)
    {
        std::cerr << "Service unregister denied by server!" << std::endl;
        return -1;
    }

    std::cout << "Service number " << (int)service <<
        " unregistered for this client" << std::endl;

    return 0;
}

int rcciClient::registerLog(bool fullLog, std::string &aStr)
{
    int msg_size = -1;
    ssize_t bytes;

    msg_size = registerService(rcci_client_flag_log, mLogFd,
                               (fullLog ? 1 : 0));
    if(msg_size < 0)
    {
        std::cerr << "registerService() for logging failed!" << std::endl;
        return -1;
    }

    // Readout also remaining stuff - it is the log file
    if(fullLog && ((msg_size - sizeof(rcci_msg_reg_service_t)) > 0))
    {
        char *log;
        int remBytes = msg_size - sizeof(rcci_msg_reg_service_t);

        log = new char [remBytes];

        bytes = read(mSockFd, log, remBytes);
        if(bytes != remBytes)
        {
            std::cerr << "read() failed, expected " << remBytes <<
                " receiver " <<  bytes << std::endl;
            return -1;
        }

        aStr = std::string(log); // clear

        delete [] log;
    }

    return 0;
}
