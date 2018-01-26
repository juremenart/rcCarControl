#ifndef __RCC_OV7670_CTRL_H
#define __RCC_OV7670_CTRL_H

#include <vector>
#include <string>

#include "rcc_i2c_ctrl.h"

class rccOv7670Ctrl : public rccI2cCtrl {
public:
    rccOv7670Ctrl(uint8_t devNum = 0);
    ~rccOv7670Ctrl(void);

private:
};

#endif // __RCC_PWM_CTRL_H
