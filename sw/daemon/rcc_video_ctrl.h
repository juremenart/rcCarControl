#ifndef __RCC_VIDEO_CTRL_H
#define __RCC_VIDEO_CTRL_H

extern "C" {
#include "rcci_type.h"
}


// System control module class
class rccVideoCtrl {
private:
    const uint32_t cVideoCtrlAddr   = 0x43C10000;
    const uint32_t cAxiClock        = 50e6;       // 50 [MHz]

    typedef struct axiVideoCtrlRegs_s {
        uint32_t version;     // 0x00 -  RO  - Version register
        uint32_t tpCtrl;      // 0x04 - R/W  - Test Pattern Control
        uint32_t tpSize;      // 0x08 - R/W  - Test Pattern Size
        uint32_t rxCtrl;      // 0x0C - R/W  - RX / Image Receiver Control
        uint32_t rxSizeStat;  // 0x10 -  RC  - RX / Image Receiver Size Status
        uint32_t rxFrameCnts; // 0x14 -  RO  - RX / Image Receiver Frame Counter
        uint32_t rxFrameLen;  // 0x18 -  RO  - RX / Image Receiver Frame Length (can be used for framerate calculation)
        uint32_t rxFifoCtrl;  // 0x1C - R/W  - RX FIFO control (when to start FLUSH to AXI-Stream, the line length)
    } axiVideoCtrlRegs_t;

public:
    rccVideoCtrl(void);
    ~rccVideoCtrl(void);

    bool isInitialized(void) { return (mRegs != NULL); };

    int version(void);

    int      writeReg(uint8_t regOffset, uint32_t regValue);
    uint32_t readReg(uint8_t regOffset);
    void     dumpRegs(void);

private:
    int      cleanup(void);

    int                         mMemFd;
    volatile axiVideoCtrlRegs_t *mRegs;
};

#endif // __RCC_VIDEO_CTRL_H
