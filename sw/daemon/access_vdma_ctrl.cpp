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

#include <iostream>

#include "rcc_vdma_ctrl.h"

void usage(const char *name)
{
    std::cerr << "Usage: " << name << std::endl <<
        " [<regAddr> [regValue]]" << std::endl<<
        " [-a <width> <height> <phy_addr1> [<phy_addr2> ...]]" <<
        std::endl << std::endl <<
        "If no arguments is provided full register dump is performed" <<
        ", if only register address then read is performed if address and" <<
        " value then write operation." << std::endl <<
        "Special usage is if -a is provided - then the VDMA engine is setup" <<
        " and waits for acquisition of data and dumps it when arrives." <<
        " Physical addresses needs to be provided as arguments (and number" <<
        " of addresses also provides number of frames that application will" <<
        " wait for." << std::endl;
}

int main(int argc, char *argv[])
{
    rccVdmaCtrl *mVdmaCtrl = new rccVdmaCtrl();

    if(argc == 1)
    {
        mVdmaCtrl->dumpRegs();
    }
    else if(argc == 2)
    {
        uint8_t regOffset = (uint8_t)strtod(argv[1], NULL);
        uint32_t regVal = mVdmaCtrl->readReg(regOffset);
        std::cout << "Offset=0x" << std::hex << (int)regOffset
                  << " Value=0x" << regVal << std::endl;
    }
    else if(argc == 3)
    {
        uint8_t regOffset = (uint8_t)strtod(argv[1], NULL);
        uint32_t regValue = (uint32_t)strtod(argv[2], NULL);

        if(mVdmaCtrl->writeReg(regOffset, regValue) < 0)
        {
            std::cout << "Write register failed" << std::endl;
        }
        else
        {
            std::cout << "Written 0x" << std::hex << regValue
                      << " to offset 0x" << (int)regOffset << std::endl;
        }
    }
    else
    {
        if(strncmp(argv[1], "-a", 2) == 0)
        {
            /* We have -a <width> <height> <phy addr1> <phy addr 2>... */
            int width, height, num_frames, retVal;
            uint32_t *phy_addr;
            num_frames = argc - 4;
            if(num_frames <= 0)
            {
                usage(argv[0]);
                return -1;
            }
            width = (int)(strtod(argv[2], NULL));
            height = (int)(strtod(argv[3], NULL));

            phy_addr = (uint32_t *)malloc(num_frames * sizeof(uint32_t));
            if(phy_addr == NULL)
            {
                std::cerr << "Can not allocate memory, quitting" << std::endl;
                return -1;
            }
            for(int i = 0; i < num_frames; i++)
            {
                phy_addr[i] = (uint32_t)(strtod(argv[4+i], NULL));
            }

            retVal = mVdmaCtrl->acqNumFrames(width, height, num_frames,
                                             phy_addr);

            return retVal;
        }
        usage(argv[0]);
        return -1;
    }

    return 0;
}
