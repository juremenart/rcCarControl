#ifndef __RCCI_CLIENT_H
#define __RCCI_CLIENT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string>

extern "C" {
#include "rcci_type.h"
}

class rcciClient {
public:
    rcciClient(void);
    ~rcciClient(void);

    bool isConnected(void);
    int connect(const std::string hostname, const int port);
    int disconnect(void);

    // RCCI specific things
    int sendInitMsg(void);

    // logging
    int logConnect(std::string &aStr, bool full_log = true);
    int logDisconnect(void);
    int logReadData(std::string &aStr);

    // Drive data
    int drvConnect(void);
    int drvDisconnect(void);
    int drvSendData(int32_t drive, int32_t steer);

private:
    int serviceConnect(int mServPort, int &mServFd,
                       struct sockaddr_in &mServAddr);

    int registerService(rcci_client_flags_t service, int srvFd,
                        const int param);
    int unregisterService(rcci_client_flags_t service, int srvFd);

    // Logging service is 'special' because of full log readback
    int registerLog(bool fullLog, std::string &aStr);

    int registerDrv(const struct sockaddr_in &sockAddr);
    int unregisterDrv(const struct sockaddr_in &sockAddr);


    int                mPort;
    int                mSockFd;
    struct sockaddr_in mServAddr;
    struct hostent    *mServer;

    uint16_t           mMagic;
    uint16_t           mVersion;

    int                mLogPort;
    int                mLogFd;
    struct sockaddr_in mLogAddr;

    int                 mDrvPort;
    int                 mDrvFd;
    rcci_msg_drv_ctrl_t mDrvMsg;
    struct sockaddr_in  mDrvAddr;
};

#endif // __RCCI_CLIENT_H
