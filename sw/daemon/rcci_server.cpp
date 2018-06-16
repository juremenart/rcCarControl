#include <unistd.h>
#include <netdb.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <chrono>

#include "rcci_server.h"
#include "rcc_logger.h"

const int cCommMagic(0xa5a5);
const int cCommVer(0x100);
const int cNumberOfConn(5);
const int cSelTimeoutMs(100);

const uint16_t cServMagic(0xbaba);
const uint16_t cServVer(0x100);

rcciServer::rcciServer(void)
    : mListenFd(-1), mPort(-1), mListenThread(NULL), mListenThreadRunning(false),
      mSelectThread(NULL), mSelectThreadRunning(NULL), mSelectThreadUpdate(false),
      mDriveCbFunc(NULL), mDriveReadThread(NULL), mDriveReadThreadRunning(false)
{
    mConnClients.clear();

    for(int i = 0; i < rcci_service_nonexisting; i++)
    {
        mServices.push_back(cServiceTable[i]);
    }
};

rcciServer::~rcciServer(void)
{
    closeServer();
};

int rcciServer::openServer(int port)
{
    std::ostringstream strStream;

    struct sockaddr_in sockAddr;

    mListenFd = socket(AF_INET, SOCK_STREAM, 0);
    if(mListenFd < 0)
    {
        strStream << "Can not open socket on port" << port <<
            ": " << strerror(errno) << std::endl;

        getLogger().error(strStream.str());

        return -1;
    }

    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = INADDR_ANY;
    sockAddr.sin_port = htons(port);

    if(bind(mListenFd, (struct sockaddr *)&sockAddr, sizeof(sockAddr)) < 0)
    {
        strStream << "Can not bind: " << strerror(errno) << std::endl;

        getLogger().error(strStream.str());
        return -1;
    }

    mPort = ntohs(port);

    listen(mListenFd, cNumberOfConn);

    strStream << "Server open on port: " << htons(mPort) << std::endl;
    getLogger().debug(strStream.str());

    for(auto it = mServices.begin(); it != mServices.end(); ++it)
        startServiceServer(*it);

    // TODO: Move this to DRV service register
    startDriveThread(); // start listening for DRV service

    return listenServer();
}

int rcciServer::listenServer(void)
{
    std::ostringstream strStream;
    mListenThread = new std::thread(&rcciServer::acceptThread, this);

    if(!mListenThread)
    {
        strStream << "Can not start accepting thread" << std::endl;
        getLogger().error(strStream.str());
        return -1;
    }

    strStream << "Started accepting thread" << std::endl;
    getLogger().debug(strStream.str());

    mSelectThread = new std::thread(&rcciServer::selectThread, this);
    if(!mSelectThread)
    {
        strStream << "Can not start select thread" << std::endl;
        getLogger().error(strStream.str());
        return -1;
    }

    return 0;
}

bool rcciServer::isServerRunning()
{
    if(mListenThread && (mPort > 0) && (mListenFd > 0))
    {
        return true;
    }
    else
    {
        return false;
    }
}

void rcciServer::writeServiceLog(std::string &str)
{
    ssize_t bytes;
    for(auto it = mServices[rcci_service_logging].clients.begin();
        it != mServices[rcci_service_logging].clients.end(); ++it)
    {
        sockaddr_in *sockAddr = (sockaddr_in *)&(*it);
        socklen_t adrlen = sizeof(*sockAddr);

        bytes = sendto(mServices[rcci_service_logging].fd, str.c_str(), str.length(), 0,
                       (sockaddr *)sockAddr, adrlen);
        if(bytes < 0)
        {
            char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
            getnameinfo((const sockaddr *)sockAddr, sizeof(*sockAddr),
                        hbuf, sizeof(hbuf), sbuf, sizeof(sbuf),
                        (NI_NUMERICHOST | NI_NUMERICSERV));

            std::cerr << "writeServiceLog() failed sendto() to " << hbuf << ":" <<
                sbuf << ": " << strerror(errno) << std::endl;
        }
        if(bytes != (ssize_t)str.length())
        {
            std::cerr << "writeServiceLog() failed to send all bytes: " << bytes <<
                " != " << str.length() << std::endl;
        }
    }
}

int rcciServer::setDriveDataCb(rccSysCtrl::driveFuncCb cbFunc)
{
    mDriveCbFunc = cbFunc;

    return 0;
}

int rcciServer::closeServer(void)
{
    for(auto it = mServices.begin(); it != mServices.end(); ++it)
    {
        closeServiceServer(*it);
    }

    mSelectThreadRunning = false;
    if(mSelectThread)
    {
        mSelectThread->join();
        delete [] mSelectThread;
        mSelectThread = NULL;
    }

    mListenThreadRunning = false;
    if(mListenThread)
    {
        mListenThread->join();
        delete [] mListenThread;
        mListenThread = NULL;
    }

    for(auto it = mConnClients.begin(); it != mConnClients.end(); ++it)
    {
        close(it->fd);
    }
    mConnClients.clear();

    if(mListenFd > 0)
    {
        close(mListenFd);
        mListenFd = -1;
    }

    mPort = -1;

    return 0;
}

int rcciServer::startServiceServer(rcci_service_t &service)
{
    std::ostringstream strStream;
    struct sockaddr_in sockAddr;
    socklen_t sockLen = sizeof(struct sockaddr_in);

    service.fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(service.fd < 0)
    {
        strStream << "openServiceServer(): Can not open socket: " <<
            strerror(errno) << std::endl;
        getLogger().error(strStream.str());
        return -1;
    }

    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = INADDR_ANY;
    sockAddr.sin_port = 0;

    if(bind(service.fd, (struct sockaddr *)&sockAddr, sizeof(sockAddr)) < 0)
    {
        strStream << "openServiceServer(): Can not bind: " << strerror(errno) << std::endl;

        getLogger().error(strStream.str());
        return -1;
    }

    if(getsockname(service.fd, (struct sockaddr *)&sockAddr, &sockLen) < 0)
    {
        strStream << "openServiceServer(): getsockname() failed: " <<
            strerror(errno) << std::endl;

        getLogger().error(strStream.str());
        return -1;
    }

    service.port = ntohs(sockAddr.sin_port);

    service.clients.clear();

    strStream.str(std::string());
    strStream << service.name << " service started at port: " << service.port <<
        " fd: " << service.fd << std::endl;
    getLogger().debug(strStream.str());

    return service.port;
}

void rcciServer::closeServiceServer(rcci_service_t &service)
{
    service.clients.clear();
    if(service.fd > 0)
    {
        close(service.fd);
        service.fd = -1;
    }
    if(service.port > 0)
    {
        service.port = -1;
    }
}

void rcciServer::addClient(rcci_client_info_t &cInfo)
{
    std::ostringstream strStream;
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

    cInfo.flags = rcci_client_flag_none;

    if(getnameinfo((const sockaddr *)&cInfo.sockAddr, sizeof(cInfo.sockAddr),
                   hbuf, sizeof(hbuf), sbuf, sizeof(sbuf),
                   NI_NUMERICHOST | NI_NUMERICSERV) < 0)
    {
        strStream << "Can not resolve client hostname" << std::endl;
        getLogger().error(strStream.str());
    }

    // protect by mutex
    mConnClients.push_back(cInfo);

    // inform listening thread to include it (must also be protected)
    // use signals/slots - would be nice :)
    mSelectThreadUpdate = true;

    strStream.str(std::string());
    strStream << "Client connected: " << hbuf << ":" << sbuf <<
        " (num of clients: " << mConnClients.size() << ")" <<std::endl;

    getLogger().debug(strStream.str());
}

void rcciServer::removeClient(rcci_client_info_t &cInfo)
{
    std::ostringstream strStream;

    for(auto it = mConnClients.begin(); it != mConnClients.end(); ++it)
    {
        if(it->fd == cInfo.fd)
        {
            close(it->fd);

            // protect by mutex
            mConnClients.erase(it);

            strStream << "Removing client " << cInfo.fd <<
                " (Num of clients: " << mConnClients.size() << ")" << std::endl;
            getLogger().debug(strStream.str());

            // inform listening thread to include it (must also be protected)
            // use signals/slots - would be nice :)
            mSelectThreadUpdate = true;

            return;
        }
    }

    return;
}

void rcciServer::acceptThread(void)
{
    std::ostringstream strStream;
    rcci_client_info_t cInfo;
    socklen_t len(sizeof(cInfo.sockAddr));

    if(!isServerRunning())
    {
        strStream << "Server not started, can not listen!" << std::endl;
        getLogger().error(strStream.str());
        return;
    }
    strStream << "Starting server" << std::endl;
    getLogger().debug(strStream.str());
    // protect by mutex
    mListenThreadRunning = true;
    while(mListenThreadRunning)
    {
        cInfo.fd = accept(mListenFd, (struct sockaddr *)&cInfo.sockAddr, &len);

        if(cInfo.fd < 0)
        {
            strStream << "Error while client connecting" << std::endl;
            getLogger().error(strStream.str());
        }

        addClient(cInfo);
    }
}

// TODO: Check if possible to use creation/killing of the thread instead of
// timeouts for select() to detect if change is needed (maybe less CPU power)
void rcciServer::selectThread(void)
{
    std::ostringstream strStream;
    fd_set fullSet, readSet;
    int maxFd, retVal;
    struct timeval selTimeout; // make it programable?

    if(!isServerRunning())
    {
        strStream << "Server not started, can not listen!" << std::endl;
        getLogger().error(strStream.str());
        return;
    }

    // protect by mutex mSelectThreadUpdate, mSelectThreadrunning & mConnClients

    /* Always update FD_SET structures */
    mSelectThreadUpdate = true;
    mSelectThreadRunning = true;

    while(mSelectThreadRunning)
    {
        /* Update FD_SET structures - new request */
        if(mSelectThreadUpdate)
        {
            maxFd = 0;
            FD_ZERO(&fullSet);
            for(auto it = mConnClients.begin(); it != mConnClients.end(); ++it)
            {
                FD_SET(it->fd, &fullSet);
                maxFd = (maxFd > it->fd) ? maxFd : it->fd;
            }
            mSelectThreadUpdate = false;
        }

        memcpy(&readSet, &fullSet, sizeof(fd_set));

        selTimeout.tv_sec = 0;
        selTimeout.tv_usec = cSelTimeoutMs*1e3;

        retVal = select(maxFd + 1, &readSet, NULL, NULL, &selTimeout);
        if(retVal < 0)
        {
            strStream.str(std::string());
            strStream << "select() failed: " << strerror(errno) << std::endl;
            getLogger().error(strStream.str());
        }
        else
        {
            if(retVal == 0)
            {
                continue;
            }

            for(auto it = mConnClients.begin(); it != mConnClients.end(); ++it)
            {
                if(FD_ISSET(it->fd, &readSet))
                {
                    // TODO: Think if it would be better to serve data
                    // in separate thread?

                    processData(*it);
                    // we break here and read next time around if something is
                    // missing - mainly because processData() might have changed
                    // mConnClients and we want to have it updated
                    break;
                }
            }
        }
    }
}

int rcciServer::startDriveThread(void)
{
    std::ostringstream strStream;

    if(mDriveReadThread || mDriveReadThreadRunning)
    {
        strStream << "Drive data thread already running" << std::endl;
        getLogger().error(strStream.str());
        return -1;
    }


    if((mServices[rcci_service_drive].fd <= 0) ||
       (mServices[rcci_service_drive].port <= 0))
    {
        strStream << "Drive service seems not to be running."
                  << " Not starting new thread." << std::endl;
        getLogger().error(strStream.str());
        return -1;
    }

    mDriveReadThread = new std::thread(&rcciServer::driveReadThread, this);
    if(!mDriveReadThread)
    {
        strStream << "Can not start drive reading thread" << std::endl;
        getLogger().error(strStream.str());
        return -1;
    }

    return 0;
}

int rcciServer::stopDriveThread(void)
{
// TODO: Re-enable the starting & stopping of drive thread from service registering!

    mThreadMutex.lock();
    mDriveReadThreadRunning = false;
    mThreadMutex.unlock();

    if(mDriveReadThread)
    {
        mDriveReadThread->join();
        delete mDriveReadThread;
        mDriveReadThread = NULL;
    }

    return 0;
}

void rcciServer::driveReadThread(void)
{
    // TODO: Make it non-blocking with either select() - current implementation
    // with DONTWAIT flag for recvfrom() or to send some bytes to the socket
    // from stopDriveThread() function to unblock for this perticular situation
    std::ostringstream strStream;
    fd_set readSet;
    struct timeval selTimeout;

    strStream.str(std::string());
    strStream << "driveReadThread(): Listening for drive data" << std::endl;
    getLogger().debug(strStream.str());
    mThreadMutex.lock();
    mDriveReadThreadRunning = true;
    mThreadMutex.unlock();
    while(true)
    {
        // basically just listen and receive data and push it to callback
        // function
        mThreadMutex.lock();
        if(mDriveReadThreadRunning == false)
        {
            mThreadMutex.unlock();
            break;
        }
        mThreadMutex.unlock();

        FD_ZERO(&readSet);
        FD_SET(mServices[rcci_service_drive].fd, &readSet);
        selTimeout.tv_sec = 0;
        selTimeout.tv_usec = cSelTimeoutMs*1e3;

        int retVal = select(mServices[rcci_service_drive].fd + 1,
                            &readSet, NULL, NULL, &selTimeout);
        if(retVal < 0)
        {
            strStream.str(std::string());
            strStream << "driveReadThread() select() failed: " <<
                strerror(errno) << std::endl;
            continue;
        }
        if((retVal == 0) ||
           !(FD_ISSET(mServices[rcci_service_drive].fd, &readSet)))
        {
            // timeout or false alarm
            continue;
        }

        if(mServices[rcci_service_drive].fd <= 0)
        {
            strStream.str(std::string());
            strStream << "Drive service fd not valid, "
                      << "quitting driveReadThread()" << std::endl;
            getLogger().error(strStream.str());
            mDriveReadThreadRunning = false;
            break;
        }

        struct sockaddr_in sockAddr;
        socklen_t   sockLen = sizeof(sockAddr);
        rcci_msg_drv_ctrl_t drvData;

        ssize_t bytes = recvfrom(mServices[rcci_service_drive].fd,
                                 &drvData, sizeof(rcci_msg_drv_ctrl_t), 0,
                                 (struct sockaddr *)&sockAddr, &sockLen);

        if((bytes == -1) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
        {
            // sleep for some time
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        if(mDriveCbFunc && (bytes == sizeof(rcci_msg_drv_ctrl_t)))
        {
            // Check if data really commes from correct client
            // (only first one is allowed)
            if(memcmp(&sockAddr, &mServices[rcci_service_drive].clients[0],
                      sizeof(struct sockaddr_in)) != 0)
            {
                strStream.str(std::string());
                strStream << "Drive command comming from unknown source!"
                          << " Ignored!" << std::endl;
                getLogger().error(strStream.str());
                continue;
            }
            mDriveCbFunc(drvData);
        }
        else
        {
            strStream.str(std::string());
            strStream << "driveReadThread() wrong bytes read: " << bytes <<
                      " != " << sizeof(rcci_msg_drv_ctrl_t);
            getLogger().error(strStream.str());
        }
    }

    mThreadMutex.unlock();
    return;
}

ssize_t rcciServer::processData(rcci_client_info_t &cInfo)
{
    rcci_msg_header_t header;
    ssize_t bytes;

    std::ostringstream strStream;

    bytes = read(cInfo.fd, &header, sizeof(rcci_msg_header_t));
    if(bytes < 0)
    {
        strStream << "read() failed: " << strerror(errno) << std::endl;
        getLogger().error(strStream.str());
        return bytes;
    }
    else if(bytes == 0)
    {
        /* end-of-file connection closed */
        removeClient(cInfo);
        return 0;
    }

    if(bytes != sizeof(rcci_msg_header_t))
    {
        // TODO: write back NACK message?
        strStream.str(std::string());
        strStream << "Init header size wrong: " << bytes << " != " <<
            sizeof(rcci_msg_header_t) << std::endl;
        getLogger().error(strStream.str());
        return -1;
    }

    if(!(cInfo.flags & rcci_client_flags_t::rcci_client_flag_init))
    {
        // if init flag is not there only valid action is to initialize
        // the connection
        return processMsgInit(cInfo, header);
    }

    /* else parse the messages */
    switch(header.type)
    {
    case rcci_msg_init:
        return processMsgInit(cInfo, header);
        break;
    case rcci_msg_reg_service:
        return processMsgRegService(cInfo, header);
    case rcci_msg_unreg_service:
        return processMsgUnregService(cInfo, header);
    default:
        strStream.str(std::string());
        strStream << "Unsupported header type: " << header.type << std::endl;
        return -1;
    }
    /* Otherwise finally call correct handler based on msg type */

    return 0;
}

int rcciServer::processMsgInit(rcci_client_info_t &cInfo,
                               rcci_msg_header_t &header)
{
    rcci_msg_init_t init_msg;

    ssize_t bytes;
    ssize_t remBytes = sizeof(rcci_msg_init_t)-sizeof(rcci_msg_header_t);

    std::ostringstream strStream;

    if((header.type  != rcci_msg_init)       ||
       (header.magic != rcci_msg_init_magic) ||
       (header.ver   != rcci_msg_init_ver)   ||
       (remBytes < 0))
    {
        // TODO: Send NACK message
        strStream << "processMsgInit() wrong message received, type: " <<
            header.type << " expected: " << rcci_msg_init <<
            " from: " << cInfo.fd << std::endl;
        getLogger().error(strStream.str());
        return -1;
    }

    /* Correct message - read other parts of the message */
    bytes = read(cInfo.fd,
                 ((uint8_t *)&init_msg + sizeof(rcci_msg_header_t)),
                 remBytes);

    if(bytes != remBytes)
    {
        strStream << "processMsgInit() received size incorrect: " <<
            bytes << " != " << remBytes << " from: " << cInfo.fd << std::endl;
        getLogger().error(strStream.str());
        // TODO: Send NACK message
        return -1;
    }

    // else parse the message and reply
    init_msg.header.magic = cServMagic;
    init_msg.header.ver   = cServVer;
    init_msg.header.type  = rcci_msg_init;
    init_msg.header.size  = sizeof(rcci_msg_init_t);
    init_msg.header.crc   = 0; // TODO: Ignored for now
    init_msg.status       = rcci_status_ack;
    init_msg.log_port     = mServices[rcci_service_logging].port;
    init_msg.drv_port     = mServices[rcci_service_drive].port;
    // other fields remains the same

    std::string serverLog;
    getLogger().getLog(serverLog);

    bytes = write(cInfo.fd, &init_msg, sizeof(rcci_msg_init_t));
    if(bytes != sizeof(rcci_msg_init_t))
    {
        strStream << "processMsgInit() write 1 size incorrect: " <<
            bytes << " != " << sizeof(rcci_msg_init_t) << " to: " <<
            cInfo.fd << std::endl;
        getLogger().error(strStream.str());
        return -1;
    }

    cInfo.flags |= rcci_client_flag_init;

    return 0;
}

int rcciServer::processMsgRegService(rcci_client_info_t &cInfo,
                                     rcci_msg_header_t &header)
{
    rcci_msg_reg_service_t msg;

    ssize_t bytes;
    ssize_t remBytes = sizeof(msg)-sizeof(rcci_msg_header_t);

    std::ostringstream strStream;

    if((header.type  != rcci_msg_reg_service) ||
       (header.magic != cServMagic)           ||
       (header.ver   != cServVer)             ||
       (remBytes < 0))
    {
        // TODO: Send NACK message
        strStream << "processMsgRegService() wrong message received, type: " <<
            header.type << " expected: " << rcci_msg_reg_service <<
            " from: " << cInfo.fd << std::endl;
        getLogger().error(strStream.str());
        return -1;
    }

    /* Correct message - read other parts of the message */
    bytes = read(cInfo.fd, ((uint8_t *)&msg + sizeof(rcci_msg_header_t)),
                 remBytes);

    if(bytes != remBytes)
    {
        strStream << "processMsgRegClient() received size incorrect: " <<
            bytes << " != " << remBytes << " from: " << cInfo.fd << std::endl;
        getLogger().error(strStream.str());
        // TODO: Send NACK message
        return -1;
    }

    // else parse the message and reply
    msg.header.magic = cServMagic;
    msg.header.ver   = cServVer;
    msg.header.type  = rcci_msg_reg_service;
    msg.header.size  = sizeof(rcci_msg_reg_service_t);
    msg.status       = rcci_status_ack;
    // other fields remains the same

    // now parse possible services - put it to separate methods, this is just ugly!
    std::string serverLog;
    getLogger().getLog(serverLog);

    std::cerr << "Registering service added:" << msg.service << std::endl;
    std::cerr << "DRV port:" << mServices[rcci_service_drive].port
              << " fd: " << mServices[rcci_service_drive].fd << std::endl;
    // Logging service
    if((msg.service == rcci_client_flag_log) &&
       (mServices[rcci_service_logging].port > 0) &&
       (mServices[rcci_service_logging].fd > 0))
    {
        // protect by mutex
        if(mServices[rcci_service_logging].canAddClient())
        {
            mServices[rcci_service_logging].clients.push_back(msg.sockaddr);
            cInfo.flags |= rcci_client_flag_log;
        }
        else
        {
            msg.status = rcci_status_nack;
        }

        if(msg.params != 0)
        {
            /* Full log is requested */
            msg.header.size += serverLog.length();
        }
    }
    // Driving service
    else if((msg.service == rcci_client_flag_drive) &&
            (mServices[rcci_service_drive].port > 0) &&
            (mServices[rcci_service_drive].fd > 0))
    {
        std::cerr << "DRV service added" << std::endl;
        if(mServices[rcci_service_drive].canAddClient())
        {
            mServices[rcci_service_drive].clients.push_back(msg.sockaddr);
            cInfo.flags |= rcci_client_flag_drive;

            // start the reading thread
            // TODO: startDriveThread(); // start listening for DRV service
        }
        else
        {
            /* denied */
            msg.status = rcci_status_nack;
        }
    }

    // send reply
    bytes = write(cInfo.fd, &msg, sizeof(rcci_msg_reg_service_t));
    if(bytes != sizeof(rcci_msg_reg_service_t))
    {
        strStream << "processMsgRegClient() write 1 size incorrect: " <<
            bytes << " != " << sizeof(rcci_msg_reg_service_t) << " to: " <<
            cInfo.fd << std::endl;
        getLogger().error(strStream.str());
        return -1;
    }

    /* Exception if full log was requested, send also that one */
    if((msg.service == rcci_client_flag_log) &&
       (mServices[rcci_service_logging].port > 0) &&
       (mServices[rcci_service_logging].fd > 0) &&
       (msg.params != 0) && (serverLog.length() > 0) &&
       (msg.status == rcci_status_ack))
    {
        // TODO: This is actually limited in size, should we make it better
        // (it is for sure not critical)
        bytes = write(cInfo.fd, serverLog.c_str(), serverLog.length());
        if(bytes != (ssize_t)serverLog.length())
        {
            strStream << "processMsgRegClient() log write incorrect: " <<
                bytes << " != " << serverLog.length() << " to: " <<
                cInfo.fd << std::endl;
            getLogger().error(strStream.str());
            return -1;
        }
    }

    return 0;
}

int rcciServer::processMsgUnregService(rcci_client_info_t &cInfo,
                                       rcci_msg_header_t &header)
{
    rcci_msg_reg_service_t msg;

    ssize_t bytes;
    ssize_t remBytes = sizeof(msg)-sizeof(rcci_msg_header_t);

    std::ostringstream strStream;

    if((header.type  != rcci_msg_unreg_service) ||
       (header.magic != cServMagic)             ||
       (header.ver   != cServVer)               ||
       (remBytes < 0))
    {
        // TODO: Send NACK message
        strStream << "processMsgUnregService() wrong message received, type: " <<
            header.type << " expected: " << rcci_msg_reg_service <<
            " from: " << cInfo.fd << std::endl;
        getLogger().error(strStream.str());
        return -1;
    }

    /* Correct message - read other parts of the message */
    bytes = read(cInfo.fd, ((uint8_t *)&msg + sizeof(rcci_msg_header_t)),
                 remBytes);

    if(bytes != remBytes)
    {
        strStream << "processMsgUnregClient() received size incorrect: " <<
            bytes << " != " << remBytes << " from: " << cInfo.fd << std::endl;
        getLogger().error(strStream.str());
        // TODO: Send NACK message
        return -1;
    }

    // parse the message and reply
    msg.header.magic = cServMagic;
    msg.header.ver   = cServVer;
    msg.header.type  = rcci_msg_reg_service;
    msg.header.size  = sizeof(rcci_msg_reg_service_t);
    msg.status       = rcci_status_ack;
    // other fields remains the same
    rcci_service_id_t id = rcci_service_nonexisting;
    switch(msg.service)
    {
    case rcci_client_flag_log:
        id = rcci_service_logging;
        break;
    case rcci_client_flag_drive:
        // TODFO: stopDriveThread();
        id = rcci_service_drive;
        break;
    default:
        id = rcci_service_nonexisting;
        break;
    }

    if(id < rcci_service_nonexisting)
    {
        // protect by mutex
        for(auto it = mServices[id].clients.begin();
            it != mServices[id].clients.end(); ++it)
        {
            if(memcmp(&(*it), &msg.sockaddr, sizeof(msg.sockaddr)) == 0)
            {
                /* Find a match - remove it */
                mServices[id].clients.erase(it);
                strStream << "Removing client for service " <<
                    mServices[id].name << std::endl;
                getLogger().debug(strStream.str());
                break;
            }
        }
    }
    else
    {
        strStream << "processMsgUnregClient() unknown service flag: " <<
            msg.service << std::endl;
        getLogger().error(strStream.str());
        msg.status = rcci_status_nack;
    }

    // send reply
    bytes = write(cInfo.fd, &msg, sizeof(rcci_msg_reg_service_t));
    if(bytes != sizeof(rcci_msg_reg_service_t))
    {
        strStream.str(std::string());
        strStream << "processMsgRegClient() write 1 size incorrect: " <<
            bytes << " != " << sizeof(rcci_msg_reg_service_t) << " to: " <<
            cInfo.fd << std::endl;
        getLogger().error(strStream.str());
        return -1;
    }

    return 0;
}

