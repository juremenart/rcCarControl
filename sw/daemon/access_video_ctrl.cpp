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

#include "rcc_video_ctrl.h"

void usage(const char *name)
{
    std::cerr << "Usage: " << name <<
        " [<regAddr> [regValue]]" << std::endl;
}

int main(int argc, char *argv[])
{
    rccVideoCtrl *mVideoCtrl = new rccVideoCtrl();

    if(argc == 1)
    {
        mVideoCtrl->dumpRegs();
    }
    else if(argc == 2)
    {
        uint8_t regOffset = (uint8_t)strtod(argv[1], NULL);
        uint32_t regVal = mVideoCtrl->readReg(regOffset);
        std::cout << "Offset=0x" << std::hex << (int)regOffset
                  << " Value=0x" << regVal << std::endl;
    }
    else if(argc == 3)
    {
        uint8_t regOffset = (uint8_t)strtod(argv[1], NULL);
        uint32_t regValue = (uint32_t)strtod(argv[2], NULL);

        if(mVideoCtrl->writeReg(regOffset, regValue) < 0)
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
        usage(argv[0]);
        return -1;
    }

    return 0;
}
