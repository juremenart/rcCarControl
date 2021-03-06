#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#include <iostream>
#include <sstream>
#include <chrono>
#include <thread> // for this_thread::sleep_for()

#include "rcc_ov5642_ctrl.h"

#include "ov5642_720p_init.h"
#include "ov5642_vga_yuv_init.h"
#include "ov5642_vga_rgb_init.h"

// TODO: Add logger() here instead of std::cerr

const uint8_t cOv5642SlaveAddr   = 0x3c;

const rccOv5642Ctrl::ov5642_mode_table_t cOv5642ModeTable =
{
    {  true, &ov5642_720p_init    , std::string("720p video") },
    {  true, &ov5642_vga_yuv_init , std::string("   VGA YUV") },
    {  true, &ov5642_vga_rgb_init , std::string("   VGA RGB") }
};



rccOv5642Ctrl::rccOv5642Ctrl(uint8_t devNum)
    : rccI2cCtrl(devNum, cOv5642SlaveAddr)
{
}

rccOv5642Ctrl::~rccOv5642Ctrl(void)
{
    close();
}

bool rccOv5642Ctrl::init(ov5642_mode_t mode)
{
    if(mode >= ov5642_mode_nonexisting)
    {
        std::cerr << "Unknown mode: " << mode << " (max valid is: "
                  << (ov5642_mode_nonexisting-1) << std::endl;
        return false;
    }

    // open i2c connection
    if(open() < 0)
    {
        std::cerr << "Can not open I2C connection to device" << std::endl;
        return false;
    }

    std::vector<uint8_t> chipId;
    chipId.resize(2);

    // Read out the ChipID for this chip
    int bytes = read(cChipIdAddr, chipId);
    if(bytes < 0)
    {
        close();
        std::cerr << "Reading out chip ID failed" << std::endl;
        return false;
    }

    if((chipId[0] != cChipIdMsb) || (chipId[1] != cChipIdLsb))
    {
        close();
        std::cerr << "Chip ID does not match 0x" << std::hex <<
            (int)chipId[0] << (int)chipId[1] << " != 0x" <<
            (int)cChipIdMsb << (int)cChipIdLsb << std::endl;
        return false;
    }

    std::cout << "OV5642 opened and chip ID correct: 0x" << std::hex
              << (int)chipId[0] << (int)chipId[1] << std::endl;
    std::cout << "Initializing mode: " << cOv5642ModeTable[mode].shortDesc
              << std::endl;

    reset();

    if(!configure(mode, true))
    {
        return false;
    }

    std::cout << "Initialization successful" << std::endl;

    // Initialize the init struct
    return true;
}

bool rccOv5642Ctrl::reset(void)
{
    if(!isOpen())
    {
        return false;
    }

    write(cSysCtrlAddr, cSysCtrl_SwRst | cSysCtrl_Rsvd);

    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    return true;
}

bool rccOv5642Ctrl::configure(ov5642_mode_t mode, bool verify)
{
    if(mode >= ov5642_mode_nonexisting)
    {
        std::cerr << "Unknown mode: " << mode << " (max valid is: "
                  << (ov5642_mode_nonexisting-1) << std::endl;
        return false;
    }

    ov5642_init_vect_t *pTable = cOv5642ModeTable[mode].pInitTable;
    for(int i = 0; i < (int)pTable->size(); i++)
    {
        uint8_t retVal;

        if(write(pTable->at(i).regAddr, pTable->at(i).regValue) < 0)
        {
            std::cerr << "Initialization failure at " << i << ", quitting"
                      << std::endl;
            close();
            return false;
        }

        if(verify)
        {
            if(read(pTable->at(i).regAddr, retVal) < 0)
            {
                std::cerr << "Initialization failure at reading " << i
                          << std::endl;
                close();
                return false;
            }
            if(retVal != pTable->at(i).regValue)
            {
                std::cerr << "Written and verified data don't agree for 0x"
                          << std::hex << (int)pTable->at(i).regAddr
                          << ": 0x" << (int)retVal << " != 0x"
                          << (int)pTable->at(i).regValue << std::endl;
            }
        }
    }

    return true;
}
