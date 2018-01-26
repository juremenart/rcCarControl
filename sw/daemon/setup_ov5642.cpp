#include <cstdio>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

#include <iostream>

#include "rcc_ov5642_ctrl.h"

int main(int argc, char *argv[])
{
    // OV5642
    rccOv5642Ctrl *ov5642Ctrl = new rccOv5642Ctrl(0);
    rccOv5642Ctrl::ov5642_mode_t mode = rccOv5642Ctrl::ov5642_720p_video;

    if(argc > 1)
    {
        mode = static_cast<rccOv5642Ctrl::ov5642_mode_t>(atoi(argv[1]));
        if(mode >= rccOv5642Ctrl::ov5642_mode_nonexisting)
        {
            mode = rccOv5642Ctrl::ov5642_720p_video;
        }
    }

    if(!ov5642Ctrl->init(mode))
        return -1;


    std::vector<uint8_t> chipId;
    chipId.resize(2);

    uint16_t chipIdAddr = 0x300a;

    int bytes = ov5642Ctrl->read(chipIdAddr, chipId);
    if(bytes < 0)
        return -1;

    std::cout << "ChipID = 0x" << std::hex << (int)chipId[0]
              << (int)chipId[1] << std::endl;
    return 0;
}
