#include <iostream>
#include <cstdio>

#include "rcc_sys_ctrl.h"

static rccSysCtrl *mSysCtrl;

int main(int argc, char *argv[])
{
    mSysCtrl = new rccSysCtrl();

    std::cout << "System controller version: 0x" << std::hex
              << mSysCtrl->version() << " PWM MUX sel: "
              << mSysCtrl->pwmMuxSel() << std::endl;

    rcci_msg_drv_ctrl_t data;

    mSysCtrl->pwmDumpRegs();

    if(argc == 1)
    {
        mSysCtrl->pwmEnable(false);
        return 0;
    }
    if((argc < 3) || (argc > 4))
    {
        std::cerr << "Usage: " << argv[0] << " <drive> <steer> [<pwm_mux_sel>]"
                  << std::endl;
        mSysCtrl->pwmEnable(false);
        return -1;
    }

    data.count = 0;
    data.drive = atoi(argv[1]);
    data.steer = atoi(argv[2]);

    if(argc == 4)
    {
        mSysCtrl->setPwmMuxSel(atoi(argv[3]));
    }

    mSysCtrl->pwmEnable(true);
    mSysCtrl->pushDriveData(data);

    mSysCtrl->pwmDumpRegs();

    return 0;
}
