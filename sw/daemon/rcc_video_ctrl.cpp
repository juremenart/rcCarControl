#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <sstream>

#include "rcc_video_ctrl.h"
#include "rcc_logger.h"

rccVideoCtrl::rccVideoCtrl(void) :
    mMemFd(-1), mRegs(NULL)
{
    void *pagePtr;
    long pageAddr, pageOff, pageSize = sysconf(_SC_PAGESIZE);

    std::ostringstream strStream;
    mMemFd = open("/dev/mem", O_RDWR | O_SYNC);
    if(mMemFd < 0)
    {
        strStream.str(std::string());
        strStream << "open() of /dev/mem failed: " << strerror(errno) << std::endl;
        getLogger().error(strStream.str());
        return;
    }

    pageAddr   = cVideoCtrlAddr & (~(pageSize-1));
    pageOff    = cVideoCtrlAddr - pageAddr;
    pagePtr    = mmap(NULL, sizeof(axiVideoCtrlRegs_t),
                      PROT_READ | PROT_WRITE, MAP_SHARED,
                      mMemFd, pageAddr);
    if((void*)pagePtr == MAP_FAILED)
    {
        strStream.str(std::string());
        strStream << "mmap() of /dev/mem failed: " << strerror(errno) << std::endl;
        getLogger().error(strStream.str());
        return;
    }

    mRegs = (axiVideoCtrlRegs_t *)pagePtr + pageOff;
}

rccVideoCtrl::~rccVideoCtrl(void)
{
    cleanup();
}

int rccVideoCtrl::cleanup(void)
{
    std::ostringstream strStream;

    if(mRegs)
    {
        if(munmap((void *)mRegs, cVideoCtrlAddr) < 0)
        {
            strStream.str(std::string());
            strStream << "munmap() failed: " << strerror(errno) << std::endl;
            getLogger().error(strStream.str());
            return -1;
        }
        mRegs = NULL;
    }

    if(mMemFd)
    {
        close(mMemFd);
        mMemFd = -1;
    }

    return 0;
}

int rccVideoCtrl::version(void)
{
    if(isInitialized())
    {
        return mRegs->version;
    }
    return -1;
}

int rccVideoCtrl::writeReg(uint8_t regOffset, uint32_t regValue)
{
    uint32_t *addr = (uint32_t *)mRegs;

    if(!isInitialized())
    {
        return -1;
    }

    *(addr+(regOffset>>2)) = regValue;
    return 0;
}

uint32_t rccVideoCtrl::readReg(uint8_t regOffset)
{
    uint32_t val = 0xdeadbeef;
    uint32_t *addr = (uint32_t *)mRegs;

    if(!isInitialized())
    {
        return val;
    }

    return *(addr+(regOffset>>2));
}

void rccVideoCtrl::dumpRegs(void)
{
    uint32_t *addr = (uint32_t *)mRegs;

    for(int i = 0; i < (int)(sizeof(axiVideoCtrlRegs_t)>>2); i++)
    {
        std::cout << "Offset=0x" << std::hex << (i*4)
                  << " Value=0x" << *(addr+i) << std::endl;
    }

    return;
}
