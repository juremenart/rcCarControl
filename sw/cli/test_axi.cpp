#include <cstdio>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>

// REMOVE THIS FILE - it's just sandbox for first bringup of AXI Slave RTL
typedef struct rccSysCtrl_s {
    uint32_t ver;
    uint32_t io_ctrl;
    uint32_t dummy;
} rccSysCtrl_t;


int main(int argc, char *argv[])
{
    void *pagePtr;
    long pageAddr, pageOff, pageSize = sysconf(_SC_PAGESIZE);
//    uint32_t mPhyAddr = 0x43C00000;
    uint32_t mPhyAddr = 0x43C10000;
    rccSysCtrl_t *mRegs;
    int mMemFd;

    mMemFd = open("/dev/mem", O_RDWR | O_SYNC);
    if(mMemFd < 0)
    {
        fprintf(stderr, "error in opening /dev/mem\n");
        return -1;
    }

    pageAddr   = mPhyAddr & (~(pageSize-1));
    pageOff    = mPhyAddr - pageAddr;
    pagePtr    = mmap(NULL, sizeof(rccSysCtrl_t),
                      PROT_READ | PROT_WRITE, MAP_SHARED,
                      mMemFd, pageAddr);
    if((void*)pagePtr == MAP_FAILED)
    {
        fprintf(stderr, "error in mmap()\n");
        return -1;
    }

    mRegs = (rccSysCtrl_t *)pagePtr + pageOff;

    printf("Version=0x%08x\n", mRegs->ver);
    printf("Dummy=0x%08x\n", mRegs->dummy);
    printf("IOCtrl=0x%08x\n", mRegs->io_ctrl);

    if(argc >= 2)
        mRegs->io_ctrl = atoi(argv[1]);

    printf("IOCtrl=0x%08x\n", mRegs->io_ctrl);

    return 0;
}
