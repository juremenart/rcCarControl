#ifndef __RCC_VDMA_CTRL_H
#define __RCC_VDMA_CTRL_H

extern "C" {
#include "rcci_type.h"
}


// System control module class
class rccVdmaCtrl {
private:
    const uint32_t cVdmaCtrlAddr   = 0x43000000;
    const uint32_t cAxiClock       = 50e6;       // 50 [MHz]

    typedef struct axiVdmaCtrlRegs_s {
        uint32_t dmaCtrl;        // 0x00 - R/W  - DMA Control Register
        uint32_t dmaStat;        // 0x04 -  RO  - DMA Status Register
        uint32_t curDesc;        // 0x08 - R/W  - Current Description
        uint32_t reserved1;      // 0x0C - R/W  - Reserved
        uint32_t tailDesc;       // 0x10 - R/W  - Tail Description
        uint32_t regIndex;       // 0x14 -  RC  - Reg Index
        uint32_t frmStore;       // 0x18 -  RO  - Frame Store
        uint32_t threshold;      // 0x1C -  RO  - Threshold
        uint32_t reserved2;      // 0x20 -  RO  - Reserved
        uint32_t frmPtr;         // 0x24 - R/W  - Frame pointer
        uint32_t parkPtr;        // 0x28 - R/W  - Park Pointer
        uint32_t vdmaVersion;    // 0x2C -  RO  - VDMA Version
        uint32_t s2mmVdmaCtrl;   // 0x30 - R/W  - S2MM VDMA Control Register
        uint32_t s2mmVdmaStat;   // 0x34 -  RO  - S2MM VDMA Status Register
        uint32_t reserved3;      // 0x38 -  RO  - Reserved
        uint32_t s2mmVdmaIrqMsk; // 0x3C - R/W  - S2MM VDMA IRQ Mask
        uint32_t reserved4;      // 0x40 -  RO  - Reserved
        uint32_t s2mmRegIndex;   // 0x44 - R/W  - S2MM Register Index
        uint32_t reserved5[2];   // 0x48 - 0x4C - Reserved
        uint32_t mm2sVSize;      // 0x50 - R/W  - MM2S Vertical Size Register
        uint32_t mm2sHSize;      // 0x54 - R/W  - MM2S Horizontal Size Register
        uint32_t mm2sFrmDlyStrd; // 0x58 - R/W  - MM2S Frame Delay and Stride
        uint32_t mm2sStAddr[16]; // 0x5C - 0x98 - R/W - MM2S Start Addresses (1-16)
        uint32_t reserve6;       // 0x9C -  RO  - Reserved
        uint32_t s2mmVSize;      // 0xA0 - R/W  - S2MM Vertical Size
        uint32_t s2mmHSize;      // 0xA4 - R/W  - S2MM Horizontal Size
        uint32_t s2mmFrmDlyStrd; // 0xA8 - R/W  - S2MM Frame Delay and Stride
        uint32_t s2mmStAddr[16]; // 0xAC - 0xE8 - R/W - S2MM Start Address (1-16)
        uint32_t reserved7;      // 0xEC -  RO  - Reserved
        uint32_t s2mmHSizeStat;  // 0xF0 -  RO  - S2MM Horizontal Size Status
        uint32_t s2mmVSizeStat;  // 0xF4 -  RO  - S2MM Vertical Size Status
    } axiVdmaCtrlRegs_t;

public:
    rccVdmaCtrl(void);
    ~rccVdmaCtrl(void);

    bool isInitialized(void) { return (mRegs != NULL); };

    int version(void);

    int      writeReg(uint8_t regOffset, uint32_t regValue);
    uint32_t readReg(uint8_t regOffset);
    void     dumpRegs(void);

    bool     vdmaRunning(void);
    void     vdmaReset(void);
    void     vdmaStop(void);
    void     vdmaStart(void);
    int      acqNumFrames(int width, int height, int num_frames,
                          uint32_t *phy_addr);

private:
    int      cleanup(void);

    int                         mMemFd;
    volatile axiVdmaCtrlRegs_t *mRegs;
};

#endif // __RCC_VDMA_CTRL_H
