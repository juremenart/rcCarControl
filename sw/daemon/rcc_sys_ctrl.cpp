#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <sstream>

#include "rcc_sys_ctrl.h"
#include "rcc_logger.h"

// Register map
// VERSION = 0x00, all bits RO
// IOCTRL = 0x04
#define IOCTRL_PWM_MUX_SEL    0x01    // bit0 - R/W - PWM output mux selector (0=sys, 1=ext)

// PWM_CTRLSTAT = 0x08
#define PWMCTRLSTAT_ENABLE        0x01     // bit0         - R/W - PWM Enable
#define PWMCTRLSTAT_CNT_WIDTH     0xFF000  // bits [23:16] - RO  - Counter width (limit also for counter input)

rccSysCtrl::rccSysCtrl(void) :
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

    pageAddr   = cSysCtrlAddr & (~(pageSize-1));
    pageOff    = cSysCtrlAddr - pageAddr;
    pagePtr    = mmap(NULL, sizeof(axiSysCtrlRegs_t),
                      PROT_READ | PROT_WRITE, MAP_SHARED,
                      mMemFd, pageAddr);
    if((void*)pagePtr == MAP_FAILED)
    {
        strStream.str(std::string());
        strStream << "mmap() of /dev/mem failed: " << strerror(errno) << std::endl;
        getLogger().error(strStream.str());
        return;
    }

    mRegs = (axiSysCtrlRegs_t *)pagePtr + pageOff;

    pwmSetPeriod(cDefaultPwmPeriod);
}

rccSysCtrl::~rccSysCtrl(void)
{
    cleanup();
}

int rccSysCtrl::cleanup(void)
{
    std::ostringstream strStream;

    if(mRegs)
    {
        if(munmap((void *)mRegs, cSysCtrlAddr) < 0)
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

int rccSysCtrl::version(void)
{
    if(isInitialized())
    {
        return mRegs->version;
    }
    return -1;
}

int rccSysCtrl::writeReg(uint8_t regOffset, uint32_t regValue)
{
    uint32_t *addr = (uint32_t *)mRegs;

    if(!isInitialized())
    {
        return -1;
    }

    *(addr+(regOffset>>2)) = regValue;
    return 0;
}

uint32_t rccSysCtrl::readReg(uint8_t regOffset)
{
    uint32_t val = 0xdeadbeef;
    uint32_t *addr = (uint32_t *)mRegs;

    if(!isInitialized())
    {
        return val;
    }

    return *(addr+(regOffset>>2));
}

void rccSysCtrl::dumpRegs(void)
{
    uint32_t *addr = (uint32_t *)mRegs;

    for(int i = 0; i < (int)(sizeof(axiSysCtrlRegs_t)>>2); i++)
    {
        std::cout << "Offset=0x" << std::hex << (i*4)
                  << " Value=0x" << *(addr+i) << std::endl;
    }

    return;
}

int rccSysCtrl::pwmMuxSel(void)
{
    if(isInitialized())
    {
        return mRegs->ioCtrl & IOCTRL_PWM_MUX_SEL;
    }

    return -1;
}

int rccSysCtrl::setPwmMuxSel(int muxSel)
{
    if(!isInitialized())
    {
        return -1;
    }

    if(muxSel)
    {
        mRegs->ioCtrl |= IOCTRL_PWM_MUX_SEL;
    }
    else
    {
        mRegs->ioCtrl &= ~IOCTRL_PWM_MUX_SEL;
    }

    return 0;
}

bool rccSysCtrl::pwmRunning(void)
{
    if(!isInitialized())
        return false;

    return mRegs->pwmCtrlStat & PWMCTRLSTAT_ENABLE;
}


// All times in [us]
int rccSysCtrl::pwmEnable(bool enable)
{
    if(!isInitialized())
        return -1;

    if(enable)
    {
        mRegs->pwmCtrlStat |= PWMCTRLSTAT_ENABLE;
    }
    else
    {
        mRegs->pwmCtrlStat &= ~PWMCTRLSTAT_ENABLE;
    }

    return 0;
}


int rccSysCtrl::pwmSetPeriod(int period)
{
    uint32_t periodCnts = convertUsToCnt(period);

    if(!isInitialized())
        return -1;

    // TODO: Add checsk
    mRegs->pwmPeriod = periodCnts;

    return 0;
}

int rccSysCtrl::pwmSetActive(int active0, int active1)
{
    uint32_t active0Cnts = convertUsToCnt(active0);
    uint32_t active1Cnts = convertUsToCnt(active1);

    if(!isInitialized())
        return -1;

    // TODO: Add checks
    mRegs->pwmActive0 = active0Cnts;
    mRegs->pwmActive1 = active1Cnts;

    return 0;
}

void rccSysCtrl::pwmDumpRegs(void)
{
    std::ostringstream strStream;
    strStream.str(std::string());
    strStream << "PWM registers: " << std::endl;
    strStream << "   CTRLSTAT=0x"  << std::hex << mRegs->pwmCtrlStat << std::endl;
    strStream << "   PERIOD=0x"    << std::hex << mRegs->pwmPeriod << std::endl;
    strStream << "   ACTIVE0=0x"   << std::hex << mRegs->pwmActive0 << std::endl;
    strStream << "   ACTIVE1=0x"   << std::hex << mRegs->pwmActive1 << std::endl;
    getLogger().debug(strStream.str());
}

void rccSysCtrl::pushDriveData(rcci_msg_drv_ctrl_t aData)
{
    if(!pwmRunning()) {
        return;
    }

    int driveTime, steerTime;

    if(aData.drive > cPwmPulseLimit)
        driveTime = cPwmPulseLimit;
    else if(aData.drive < -cPwmPulseLimit)
        driveTime = -cPwmPulseLimit;
    else
        driveTime = aData.drive;

    if(aData.steer > cPwmPulseLimit)
        steerTime = cPwmPulseLimit;
    else if(aData.steer < -cPwmPulseLimit)
        steerTime = -cPwmPulseLimit;
    else
        steerTime = aData.steer;

    driveTime = cPwmPulseNom + (driveTime * cPwmPulseRange) / cPwmPulseLimit;
    steerTime = cPwmPulseNom + (steerTime * cPwmPulseRange) / cPwmPulseLimit;

    pwmSetActive(driveTime, steerTime);
}

uint32_t rccSysCtrl::convertUsToCnt(int timeUs)
{
    return (timeUs * (cSysCtrlClock/1e6));
}
