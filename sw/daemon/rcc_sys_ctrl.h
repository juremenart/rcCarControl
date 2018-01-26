#ifndef __RCC_SYS_CTRL_H
#define __RCC_SYS_CTRL_H

extern "C" {
#include "rcci_type.h"
}


// System control module class
class rccSysCtrl {
private:
    const uint32_t cSysCtrlAddr   = 0x43C00000;
    const uint32_t cSysCtrlClock  = 50e6;       // 50 [MHz]

    // For GT2 Evo RF receiver I measured the following PWM parameters (values
    // in driver changed a little bit for simplification):
    //  - Period:     16.600 [ms]
    //  - NOM active:  1.379 [ms]
    //  - MIN active:  1.000 [ms]
    //  - MAX active:  1.889 [ms]
    //
    // rcci_msg_drv_ctrl_t data for drive and steer is sort of percentage
    // going from -1000 (-100.0%) to +1000 (+100.0%) transfored in
    // pushDriveData() to get:
    // is transformed:
    //  - MIN value: -1000 transformed to 1000 [us]
    //  - NOM value:     0 transformed to 1400 [us]
    //  - MAX value: +1000 trasnformed to 1800 [us]
    // This is valid for both - drive and steer
    const int cDefaultPwmPeriod = 16600;   // in [us]
    const int cPwmPulseNom      = 1400;    // in [us]
    const int cPwmPulseMin      = 1000;    // in [us]
    const int cPwmPulseMax      = 1800;    // in [us]

    // We assume (maybe wrongly?) that + / - sides are symetrical
    const int cPwmPulseRange    = (cPwmPulseMax - cPwmPulseNom);

    const int cPwmPulseLimit    = 1000;    // input value -/+ 100.0%
    // Timer0 defines period, Timer1 defines duty-cycle or period-high
    typedef struct axiSysCtrlRegs_s {
        uint32_t version;     // 0x00 -  RO  - Version register
        uint32_t ioCtrl;      // 0x04 - R/W  - IO control
        uint32_t pwmCtrlStat; // 0x08 - R/W  - PWM Control/Status
        uint32_t pwmPeriod;   // 0x0C - R/W  - PWM Period (for both channels)
        uint32_t pwmActive0;  // 0x10 - R/W  - PWM Active0
        uint32_t pwmActive1;  // 0x14 - R/W  - PWM Active1
    } axiSysCtrlRegs_t;

public:
    rccSysCtrl(void);
    ~rccSysCtrl(void);

    typedef int (*driveFuncCb)(rcci_msg_drv_ctrl_t);


    bool isInitialized(void) { return (mRegs != NULL); };

    int version(void);

    int pwmMuxSel(void);
    int setPwmMuxSel(int muxSel);

    bool pwmRunning(void);
    // All times in [us]
    int pwmEnable(bool enable);
    int pwmSetPeriod(int period);
    int pwmSetActive(int active0, int active1);
    void pwmDumpRegs(void);

    void pushDriveData(rcci_msg_drv_ctrl_t aData); // driveFuncCb() really
private:
    int      cleanup(void);
    uint32_t convertUsToCnt(int timeUs);

    int                        mMemFd;
    volatile axiSysCtrlRegs_t *mRegs;
    int                        mDrivePeriod, mSteerPeriod;

};

#endif // __RCC_SYS_CTRL_H
