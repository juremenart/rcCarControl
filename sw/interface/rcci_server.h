#ifndef __RCCI_SERVER_H
#define __RCCI_SERVER_H

#include <thread>
#include <mutex>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

extern "C" {
#include "rcci_type.h"
}

#include "rcc_drv_ctrl.h"

#define MAX_SERVICE_NAME 16

class rcciServer {
private:
    typedef struct rcci_client_info_s {
        struct sockaddr     sockAddr;
        int                 fd;
        uint32_t            flags; // collected from rcci_client_flags_t type
    } rcci_client_info_t;

    // service destinations
    typedef std::vector<struct sockaddr_in> rcci_client_vect_t;

    // must be the same order as entries in mServices
    typedef enum rcci_service_id_e {
        rcci_service_logging = 0,
        rcci_service_drive,
        rcci_service_nonexisting // must be last
    } rcci_service_id_t;

    // TODO: rcci_service should became independent class maybe? :)
    //       it should include also start/close server methods from
    //       rcciServer class.
    //       Maybe also include things such as writeLogCb() - so
    //       default write/read function interfaces?
    typedef struct rcci_service_s {
        rcci_service_id_t   serviceId;
        char                name[MAX_SERVICE_NAME];
        size_t              maxClients; // if 0 - unlimited
        // elements below are initialized during startup of server
        int                 port;
        int                 fd;
        rcci_client_vect_t  clients;

        // some helper functions
        bool canAddClient()
        {
            if((maxClients == 0) ||
               (clients.size() < maxClients))
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    } rcci_service_t;
    typedef std::vector<rcci_service_t> rcci_service_vect_t;

    const rcci_service_t cServiceTable[rcci_service_nonexisting] = {
        { rcci_service_logging, "logging",  0, -1, -1, rcci_client_vect_t() },
        {   rcci_service_drive,   "drive",  1, -1, -1, rcci_client_vect_t() }
    };


public:
    rcciServer(void);
    ~rcciServer(void);

    /* TCP server public interfaces */
    int     openServer(int port = 1025);
    int     closeServer(void);
    bool    isServerRunning();

    /* Service public interfaces */
    /* Logging service write support */
    void    writeServiceLog(std::string &str);
    int     setDriveDataCb(rccDrvCtrl::driveFuncCb cbFunc);

private:
    // used for logging, drive & video streams - should be put
    // to new class (together with rcci_service structure)
    int     startServiceServer(rcci_service_t &service);
    void    closeServiceServer(rcci_service_t &service);

    /* TCP server supporting methods */
    int     listenServer(void); // generates acceptThread()

    void    acceptThread(void);
    void    selectThread(void);

    void    addClient(rcci_client_info_t &cInfo);
    void    removeClient(rcci_client_info_t &cInfo);

    /* Internal service supporting methods */
    /* Support for drive data polling*/
    int     startDriveThread(void);
    int     stopDriveThread(void);
    void    driveReadThread(void);

    /* TCP server message processing */
    ssize_t processData(rcci_client_info_t &cInfo);
    int     processMsgInit(rcci_client_info_t &cInfo,
                           rcci_msg_header_t &header);
    int     processMsgRegService(rcci_client_info_t &cInfo,
                                 rcci_msg_header_t &header);
    int     processMsgUnregService(rcci_client_info_t &cInfo,
                                   rcci_msg_header_t &header);

    int                             mListenFd;
    int                             mPort;
    std::thread                    *mListenThread;
    bool                            mListenThreadRunning;
    std::thread                    *mSelectThread;
    bool                            mSelectThreadRunning;
    std::vector<rcci_client_info_t> mConnClients;
    bool                            mSelectThreadUpdate;
    uint16_t                        mMagic;
    uint16_t                        mProtVer;
    // mServices is initialized from mServiceTable in constructor
    rcci_service_vect_t             mServices;

    // drive callback function member
    rccDrvCtrl::driveFuncCb         mDriveCbFunc;

    // Thread for supporting the drive readback (so we don't block other
    // parts)
    std::thread                    *mDriveReadThread;
    bool                            mDriveReadThreadRunning;
    std::mutex mThreadMutex;

};

#endif // __RCCI_SERVER_H
