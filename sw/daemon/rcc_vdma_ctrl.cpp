#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <sstream>
#include <chrono>
#include <thread> // for this_thread::sleep_for()


#include "rcc_vdma_ctrl.h"
#include "rcc_logger.h"

// Only selected bits
#define VDMA_S2MM_DMACR_START     0x01
#define VDMA_S2MM_DMACR_CIRCULAR  0x02
#define VDMA_S2MM_DMACR_RESET     0x04
#define VDMA_S2MM_DMACR_GL_EN     0x08  // GenLock Enable

#define VDMA_S2MM_DMASR_HALT           0x00000001
#define VDMA_S2MM_DMASR_INT_ERR        0x00000010 // Internal Error
#define VDMA_S2MM_DMASR_SLV_ERR        0x00000020 // Slave Error
#define VDMA_S2MM_DMASR_DEC_ERR        0x00000040 // Decode Error
#define VDMA_S2MM_DMASR_SOF_EARLY_ERR  0x00000080 // Start-Of-Frame Early Error
#define VDMA_S2MM_DMASR_EOL_EARLY_ERR  0x00000100 // End-Of-Line Early Error
#define VDMA_S2MM_DMASR_SOF_LATE_ERR   0x00000800 // SoF Late Error
#define VDMA_S2MM_DMASR_FRM_CNT_INT    0x00001000 // Frame Counter Interrupt
#define VDMA_S2MM_DMASR_DLY_CNT_INT    0x00002000 // Delay Counter Interrupt
#define VDMA_S2MM_DMASR_ERR_INT        0x00004000 // Error Interrupt
#define VDMA_S2MM_DMASR_EOL_LATE_ERR   0x00008000 // EoL Late Error
#define VDMA_S2MM_DMASR_FRM_CNT        0x00FF0000 // Frame Count
#define VDMA_S2MM_DMASR_DLY_CNT        0xFF000000 // Delay Count

#define VDMA_S2MM_DMASR_ERR_MASK       0x0000CFF0

rccVdmaCtrl::rccVdmaCtrl(void) :
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

    pageAddr   = cVdmaCtrlAddr & (~(pageSize-1));
    pageOff    = cVdmaCtrlAddr - pageAddr;
    pagePtr    = mmap(NULL, sizeof(axiVdmaCtrlRegs_t),
                      PROT_READ | PROT_WRITE, MAP_SHARED,
                      mMemFd, pageAddr);
    if((void*)pagePtr == MAP_FAILED)
    {
        strStream.str(std::string());
        strStream << "mmap() of /dev/mem failed: " << strerror(errno) << std::endl;
        getLogger().error(strStream.str());
        return;
    }

    mRegs = (axiVdmaCtrlRegs_t *)pagePtr + pageOff;
}

rccVdmaCtrl::~rccVdmaCtrl(void)
{
    cleanup();
}

int rccVdmaCtrl::cleanup(void)
{
    std::ostringstream strStream;

    if(mRegs)
    {
        if(munmap((void *)mRegs, cVdmaCtrlAddr) < 0)
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

int rccVdmaCtrl::version(void)
{
    if(isInitialized())
    {
        return mRegs->vdmaVersion;
    }
    return -1;
}

int rccVdmaCtrl::writeReg(uint8_t regOffset, uint32_t regValue)
{
    uint32_t *addr = (uint32_t *)mRegs;

    if(!isInitialized())
    {
        return -1;
    }

    *(addr+(regOffset>>2)) = regValue;
    return 0;
}

uint32_t rccVdmaCtrl::readReg(uint8_t regOffset)
{
    uint32_t val = 0xdeadbeef;
    uint32_t *addr = (uint32_t *)mRegs;

    if(!isInitialized())
    {
        return val;
    }

    return *(addr+(regOffset>>2));
}

void rccVdmaCtrl::dumpRegs(void)
{
    uint32_t *addr = (uint32_t *)mRegs;

    for(int i = 0; i < (int)(sizeof(axiVdmaCtrlRegs_t)>>2); i++)
    {
        std::cout << "Offset=0x" << std::hex << (i*4)
                  << " Value=0x" << *(addr+i) << std::endl;
    }

    return;
}

void rccVdmaCtrl::vdmaReset(void)
{
    if(isInitialized())
    {
        mRegs->s2mmVdmaCtrl |= VDMA_S2MM_DMACR_RESET;

        // Wait until RESET bit is cleared
        while(mRegs->s2mmVdmaCtrl & VDMA_S2MM_DMACR_RESET)
        {
            /* Empty body */
        }
    }
}

void rccVdmaCtrl::vdmaStop(void)
{
    if(isInitialized())
    {
        mRegs->s2mmVdmaCtrl &= ~VDMA_S2MM_DMACR_START;

        // Wait until HALT bit is set
        while(vdmaRunning()) { /* Empty body */ }
    }
}

void rccVdmaCtrl::vdmaStart(void)
{
    if(isInitialized())
    {
        mRegs->s2mmVdmaCtrl |= VDMA_S2MM_DMACR_START;

        // Wait until not running
        while(!vdmaRunning()) { /* Empty body */ }
    }
}

bool rccVdmaCtrl::vdmaRunning(void)
{
    return(!(mRegs->s2mmVdmaStat & VDMA_S2MM_DMASR_HALT));
}

int rccVdmaCtrl::acqNumFrames(int width, int height, int num_frames,
                              uint32_t *phy_addr)
{
    int retVal = 0;
    int buf_size = width * height * 2;
    uint8_t  **virt_addr;
    int cur_frm_cnt;

    if(!isInitialized() || (mMemFd == -1))
    {
        std::cerr << "acqNumFrames() system not initialized" << std::endl;
        return -1;
    }

    /* Allocate memory */
    virt_addr = (uint8_t **)malloc(num_frames * sizeof(uint8_t *));
    if(!virt_addr)
    {
        std::cerr << "acqNumFrames() can not allocate memory for buffers"
                  << std::endl;
        return -1;
    }
    for(int i = 0; i < num_frames; i++)
    {
        virt_addr[i] = NULL;
    }

    /* Map all physical addresses */
//    for(int i = 0; i < num_frames; i++)
//    {
//        virt_addr[i] = (uint8_t *)mmap(NULL, buf_size, PROT_READ | PROT_WRITE,
//                                       MAP_SHARED, mMemFd, (off_t)phy_addr[i]);
//        if(virt_addr[i] == MAP_FAILED)
//        {
//            std::cerr << "acqNumFrames() mapping of phy_addr (0x" << std::hex
//                      << phy_addr[i] << ") failed: "  << strerror(errno) << std::endl;
//            retVal = -1;
//            goto free_and_exit;
//        }
//        std::cout << "Mapped buffer address 0x" << std::hex << phy_addr[i]
//                  << " to virtual address" << std::endl;
//        memset(virt_addr[i], 0, buf_size);
//    }

    /* Reset */
    vdmaReset();

    /* Clear status */
    mRegs->s2mmVdmaStat = 0xffffffff;

    /* Mask out all interrupts */
    mRegs->s2mmVdmaIrqMsk = 0xF;

    mRegs->s2mmVdmaCtrl = (num_frames << 16) | VDMA_S2MM_DMACR_CIRCULAR;

    /* Enable also start bit and wait */
    vdmaStart();

    mRegs->s2mmRegIndex = 0;

    /* Set addresses */
    for(int i = 0; i < num_frames; i++)
    {
        mRegs->s2mmStAddr[i] = phy_addr[i];
    }

    mRegs->parkPtr = 0;

    /* x2 because currently we are providing YCbCr directly (1 pixel 2 bytes) */
    mRegs->s2mmFrmDlyStrd = (width * 2);
    mRegs->s2mmHSize = (width * 2);

    /* Read original frame counter before starting the engine */
    cur_frm_cnt = (mRegs->s2mmVdmaStat & VDMA_S2MM_DMASR_FRM_CNT) >> 16;
    /* Vertical Size must be last - this actually starts the transfer */
    mRegs->s2mmVSize = height;

    std::cout << "VDMA S2MM configured, registers:" << std::endl;
    dumpRegs();

    std::cout << "Waiting for " << cur_frm_cnt << " frames." << std::endl;
    // Waiting for frames...

    int frm_idx = 0;
    while(cur_frm_cnt > 0)
    {
        int frm_cnt = (mRegs->s2mmVdmaStat & VDMA_S2MM_DMASR_FRM_CNT) >> 16;

        if(mRegs->s2mmVdmaStat & VDMA_S2MM_DMASR_ERR_MASK)
        {
            std::cerr << "VDMA S2MM Status: 0x" << std::hex
                      << mRegs->s2mmVdmaStat << std::endl;
            retVal = -1;
            break;
        }

        if(frm_cnt != cur_frm_cnt)
        {
            std::cout << "FRAME" << frm_idx << " data=";
//            for(int i = 0; i < buf_size; i++)
//            {
//                std::cout << " 0x" << (uint8_t)virt_addr[frm_idx][i];
//            }
            std::cout << std::endl;

            /* Frame counter decreased - we have new frame! */
            std::cout << "cur_frm_cnt=" << cur_frm_cnt << " frm_cnt=" << frm_cnt << " num_frames=" << num_frames << " frm_idx=" << frm_idx << std::endl;
            cur_frm_cnt = frm_cnt;
            frm_idx++;
            if(frm_idx == num_frames)
                break;
        }

        // sleep a little bit
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

free_and_exit:
//    if(virt_addr)
//    {
//        for(int i = 0; i < num_frames; i++)
//        {
//            if(virt_addr[i] != MAP_FAILED)
//            {
//                munmap(virt_addr[i], buf_size);
//            }
//        }
//        delete virt_addr;
//    }

    return retVal;
}
