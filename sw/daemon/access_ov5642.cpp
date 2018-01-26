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

#include "rcc_ov5642_ctrl.h"

void usage(const char *name)
{
    std::cerr << "Usage: " << name <<
        " -[w|r] <regAddr> [<numOfRegs>|<val1> <val2> ...]" << std::endl;
}

int main(int argc, char *argv[])
{
    // OV5642
    rccOv5642Ctrl *ov5642Ctrl = new rccOv5642Ctrl(0);

    if(argc < 4)
    {
        usage(argv[0]);
        return -1;
    }

    if(!ov5642Ctrl->open())
        return -1;

    uint16_t regAddr = (uint16_t)strtod(argv[2], NULL);

    if(strncmp(argv[1], "-w", 2) == 0)
    {
        // Write operation - argv[2] is regAddr and all subsequent values
        // are values to be written
        std::vector<uint8_t> regVal;
        for(int i = 3; i < argc; i++)
        {
            regVal.push_back((uint8_t)strtod(argv[i]), NULL);
        }
        std::cout << "Write " << regVal.size() << " bytes starting with address"
                  << " 0x" << std::hex << (int)regAddr << std::endl;
        if(ov5642Ctrl->write(regAddr, regVal) < 0)
        {
            std::cerr << "write failed!" << std::endl;
            return -1;
        }
    }
    else if(strncmp(argv[1], "-r", 2) == 0)
    {
        // Read operation - argv[2] is regAddr and argv[3] is number of
        // register to be read-out
        std::vector<uint8_t> regVal;
        int numOfRegs = atoi(argv[3]);
        regVal.resize(numOfRegs);

        int bytes = ov5642Ctrl->read(regAddr, regVal);

        if(bytes < 0)
        {
            std::cerr << "read failed!" << std::endl;
            return -1;
        }
        if(bytes != (int)regVal.size())
        {
            std::cerr << "only " << bytes << " readout (expected "
                      << regVal.size() << ")" << std::endl;
        }

        std::cout << "Received data: " << std::endl;
        for(int i = 0; i < bytes; i++)
        {
            std::cout << "   Reg=0x" << std::hex << (int)(regAddr+i)
                      << "   Value=" << std::hex << (int)(regVal[i])
                      << std::endl;
        }
    } else
    {
        usage(argv[0]);
        return -1;
    }

    return 0;
}
