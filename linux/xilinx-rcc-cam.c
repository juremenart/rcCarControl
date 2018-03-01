/*
 * RC Car Control Video driver
 *
 * Author: Jure Menart (juremenart@gmail.com)
 */
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_dma.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/dma/xilinx_dma.h>


#include <media/media-device.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>

#define XRCC_DEBUG(...) dev_info(__VA_ARGS__)
#define XRCC_INFO(...)  dev_info(__VA_ARGS__)
#define XRCC_ERR(...)   dev_err(__VA_ARGS__)

#define VIDEO_CTRL_INTEGRATED
#define VDMA_CTRL_INTEGRATED

#if defined(VIDEO_CTRL_INTEGRATED) && defined(VDMA_CTRL_INTEGRATED)
#define USE_VECTOR_BUFFERS
#endif

#ifdef USE_VECTOR_BUFFERS
typedef struct xrcc_cam_buf_s {
    struct vb2_v4l2_buffer *buf;
    dma_addr_t              addr;
    int                     registered;
} xrcc_cam_buf_t;
#endif

typedef struct xrcc_cam_dev_s {
    // number of frame buffers, must be set only once at the beginning
    // TODO: Check if not better to have size/bpp/buf_size info only in
    //       v4l2_pix_format
    uint32_t                   frm_cnt;
    uint32_t                   width, height, bpp;
    uint32_t                   buf_size; // automatically calculated when allocating buffers
    int                        port;

    /* Device structures */
    struct device             *dev;
    struct v4l2_device         v4l2_dev;
    struct media_device        media_dev;
    struct video_device        video;

    /* DMA channel & transaction */
    struct dma_chan                 *dma_chan;
    struct dma_interleaved_template  xt;
    struct completion                rx_cmp;

    /* Lock protection for format/queues/video */
    struct mutex               lock;
    struct v4l2_pix_format     format;
    struct media_pad           pad;

    struct vb2_queue           queue;
    unsigned int               sequence;

    struct list_head           queued_bufs;
    spinlock_t                 queued_lock;

#ifdef USE_VECTOR_BUFFERS
    xrcc_cam_buf_t            *vect_bufs;
#endif

#ifdef VIDEO_CTRL_INTEGRATED
    /* Video control registers */
    /* TODO: Do this properly with it's own driver! */
    void __iomem              *video_ctrl_mem;
    int                        frame_ptr_irq;
#endif

#ifdef VDMA_CTRL_INTEGRATED
    void __iomem              *vdma_ctrl_mem;
#endif
} xrcc_cam_dev_t;

typedef struct xrcc_dma_buffer_s {
    struct vb2_v4l2_buffer buf;
    struct list_head       queue;
    xrcc_cam_dev_t        *xrcc_dev;
} xrcc_dma_buffer_t;

#define to_xrcc_cam_dev(vdev) container_of(vdev, xrcc_cam_dev_t, video)
#define to_xrcc_dma_buffer(vb)	container_of(vb, xrcc_dma_buffer_t, buf)

static struct device *xrcc_dev_to_dev(xrcc_cam_dev_t *dev)
{
    return dev->dev;
}

#ifdef VIDEO_CTRL_INTEGRATED

#define VIDEO_CTRL_VERSION       0x00
#define VIDEO_CTRL_RX_CTRL       0x0C
#define VIDEO_CTRL_RX_SIZE_STAT  0x10
#define VIDEO_CTRL_RX_FRAME_CNTS 0x14
#define VIDEO_CTRL_RX_FRAME_LEN  0x18
#define VIDEO_CTRL_RX_FIFO_CTRL  0x1C

#define VIDEO_CTRL_RX_CTRL_RUN   0x01
#define VIDEO_CTRL_RX_CTRL_INTEN 0x10
#define VIDEO_CTRL_RX_CTRL_INT   0x20

// Write pointer - last pointer that was written by VDMA
#define VIDEO_CTRL_WR_PTR_SHIFT  24
// Read pointer - can be set by this driver to protect one buffer (which
// is used by userspace for example) - if set to 0 no buffer is protected
#define VIDEO_CTRL_RD_PTR_SHIFT  16
#define VIDEO_CTRL_PTR_MASK      0x3F // Mask for pointers - we have 6 bits for each

static inline u32 video_ctrl_read(xrcc_cam_dev_t *dev, u32 reg)
{
    return ioread32(dev->video_ctrl_mem + reg);
}

static inline void video_ctrl_write(xrcc_cam_dev_t *dev, u32 reg, u32 value)
{
    iowrite32(value, dev->video_ctrl_mem + reg);
}

static inline void video_ctrl_set_size(xrcc_cam_dev_t *dev, int width,
                                       int height, int bpp)
{
    int line_length = width * bpp;

    // control the FIFO (set line length + when video starts to stream)
    video_ctrl_write(dev, VIDEO_CTRL_RX_FIFO_CTRL,
                     (line_length << 16) | line_length);

    (void)(height);
}

static inline u32 video_ctrl_version(xrcc_cam_dev_t *dev)
{
    return video_ctrl_read(dev, VIDEO_CTRL_VERSION);
}

static inline void video_ctrl_inten(xrcc_cam_dev_t *dev, int int_enable)
{
    int ctrl = video_ctrl_read(dev, VIDEO_CTRL_RX_CTRL);

    if(int_enable)
    {
        video_ctrl_write(dev, VIDEO_CTRL_RX_CTRL,
                         ctrl | VIDEO_CTRL_RX_CTRL_INTEN);
    }
    else
    {
        ctrl &= ~VIDEO_CTRL_RX_CTRL_INTEN;
        video_ctrl_write(dev, VIDEO_CTRL_RX_CTRL, ctrl);

        // Clear also potential interrupts
        video_ctrl_write(dev, VIDEO_CTRL_RX_CTRL, ctrl | VIDEO_CTRL_RX_CTRL_INT);
    }
}

static inline void video_ctrl_run(xrcc_cam_dev_t *dev)
{
    int ctrl = video_ctrl_read(dev, VIDEO_CTRL_RX_CTRL);
    video_ctrl_write(dev, VIDEO_CTRL_RX_CTRL, ctrl | VIDEO_CTRL_RX_CTRL_RUN);
}

static inline void video_ctrl_stop(xrcc_cam_dev_t *dev)
{
    int ctrl = video_ctrl_read(dev, VIDEO_CTRL_RX_CTRL);
    video_ctrl_write(dev, VIDEO_CTRL_RX_CTRL, ctrl & ~VIDEO_CTRL_RX_CTRL_RUN);
}

static void video_ctrl_clear_err(xrcc_cam_dev_t *dev)
{
    video_ctrl_write(dev, VIDEO_CTRL_RX_SIZE_STAT, 1);
}

static void video_ctrl_get_stats(xrcc_cam_dev_t *dev,
                                 u32 *width, u32 *height,
                                 u32 *err_flags, u32 *frame_cnt,
                                 u32 *frame_len)
{
    u32 size_stat = video_ctrl_read(dev, VIDEO_CTRL_RX_SIZE_STAT);
    u32 f_len     = video_ctrl_read(dev, VIDEO_CTRL_RX_FRAME_LEN);
    u32 f_cnts    = video_ctrl_read(dev, VIDEO_CTRL_RX_FRAME_CNTS);

    *height     = (size_stat >> 16) & 0xFFF;
    *width      =  (size_stat >> 0)  & 0xFFF;
    *err_flags  = (size_stat & 0x80008000);
    *frame_cnt  = f_cnts;
    *frame_len  = f_len;
}

static void video_ctrl_dump_meas(xrcc_cam_dev_t *dev)
{
    u32 width, height, err_flags, frame_cnt, frame_len;

    video_ctrl_get_stats(dev, &width, &height, &err_flags, &frame_cnt, &frame_len);
    XRCC_DEBUG(xrcc_dev_to_dev(dev), "Video controller size stats:");
    XRCC_DEBUG(xrcc_dev_to_dev(dev), "\twidth=%d", width);
    XRCC_DEBUG(xrcc_dev_to_dev(dev), "\theight=%d", height);
    XRCC_DEBUG(xrcc_dev_to_dev(dev), "\terr_flags=0x%08x", err_flags);
    XRCC_DEBUG(xrcc_dev_to_dev(dev), "\tframe_cnt=%d", frame_cnt);
    XRCC_DEBUG(xrcc_dev_to_dev(dev), "\tframe_len=%d", frame_len);
}

/* Hardcoded 6-bit conversion & specific for VDMA */
static u32 video_gray2bin(const u32 gray, const u32 frm_cnts)
{
    u32 bin = gray & (1<<5);
    int i;

    for(i = 4; i >= 0; i--)
    {
        u32 bin_b = ((gray & (1<<i))>>i) ^ (bin>>(i+1));
        bin |= (bin_b << i);
    }

    bin = bin -1; // Gray reall starts with 1
    bin = bin % frm_cnts; // Gray in VDMA is always ^2 and we should wrap

    return bin;
}

static u32 video_bin2gray(const u32 bin)
{
    u32 binp = bin+1;
    u32 gray = (binp & 0x1f) ^ ((binp >> 1) & 0x1f);

    gray |= binp & 0x20;

    return gray;
}

/* Write pointer readout & gray to bin conversion to get index */
static u32 video_ctrl_get_idx(const u32 status_val, const u32 frm_cnts)
{
    int gray = (status_val >> VIDEO_CTRL_WR_PTR_SHIFT) & VIDEO_CTRL_PTR_MASK;

    return video_gray2bin(gray, frm_cnts);
}

/* Read pointer setting + conversion bin2gray */
static u32 video_ctrl_set_ptr(const u32 index)
{
    int bin = (index & VIDEO_CTRL_PTR_MASK) << VIDEO_CTRL_RD_PTR_SHIFT;

    return video_bin2gray(bin);
}

#endif

#ifdef VDMA_CTRL_INTEGRATED
/* TODO: This is really ugly hack - xilinx-vdma driver is not working as I'd
 * expect and would want for my system so instead of writting new full driver I
 * am adding this hack for now to configure the VDMA engine as I'd expect
 * In the end this should be separate xilinx-vdma driver and also for the video
 * controller (see VIDEO_CTRL_INTEGRATED)
 */
#define VDMA_CTRL_VERSION_REG 0x2C

/* Supporting only S2MM */
#define VDMA_CTRL_DMACR         0x30
#define VDMA_CTRL_DMASR         0x34
#define VDMA_CTRL_REG_INDEX     0x44
#define VDMA_CTRL_VSIZE         0xA0
#define VDMA_CTRL_HSIZE         0xA4
#define VDMA_CTRL_FRMDLY_STRIDE 0xA8
#define VDMA_CTRL_ST_ADDR       0xAC // Start address

#define VDMA_CTRL_DMACR_RUN          0x0001
#define VDMA_CTRL_DMACR_CIRCPARK     0x0002
#define VDMA_CTRL_DMACR_RESET        0x0004
#define VDMA_CTRL_DMACR_GENLOCK_EN   0x0008
#define VDMA_CTRL_DMACR_ERR_IRQEN    0x4000

#define VDMA_CTRL_DMASR_HALTED        0x0001
#define VDMA_CTRL_DMASR_INT_ERR       0x0010
#define VDMA_CTRL_DMASR_SLV_ERR       0x0020
#define VDMA_CTRL_DMASR_DEC_ERR       0x0040
#define VDMA_CTRL_DMASR_SOF_EARLY_ERR 0x0080
#define VDMA_CTRL_DMASR_EOL_EARLY_ERR 0x0100
#define VDMA_CTRL_DMASR_SOF_LATE_ERR  0x0800
#define VDMA_CTRL_DMASR_FRM_CNT_IRQ   0x1000
#define VDMA_CTRL_DMASR_DLY_CNT_IRQ   0x2000
#define VDMA_CTRL_DMASR_ERR_IRQ       0x4000
#define VDMA_CTRL_DMASR_EOL_LATE_ERR  0x8000

static inline u32 vdma_ctrl_read(xrcc_cam_dev_t *dev, u32 reg)
{
    return ioread32(dev->vdma_ctrl_mem + reg);
}

static inline void vdma_ctrl_write(xrcc_cam_dev_t *dev, u32 reg, u32 value)
{
    iowrite32(value, dev->vdma_ctrl_mem + reg);
}

static inline u32 vdma_ctrl_version(xrcc_cam_dev_t *dev)
{
    return vdma_ctrl_read(dev, VDMA_CTRL_VERSION_REG);
}

static inline void vdma_ctrl_reset(xrcc_cam_dev_t *dev)
{
    u32 status;

    vdma_ctrl_write(dev, VDMA_CTRL_DMACR, VDMA_CTRL_DMACR_RESET);

    status = vdma_ctrl_read(dev, VDMA_CTRL_DMACR);
    while(status & VDMA_CTRL_DMACR_RESET)
    {
        status = vdma_ctrl_read(dev, VDMA_CTRL_DMACR);
    }
}

static inline void vdma_ctrl_run(xrcc_cam_dev_t *dev)
{
    u32 status = vdma_ctrl_read(dev, VDMA_CTRL_DMACR);

    vdma_ctrl_write(dev, VDMA_CTRL_DMACR, status | VDMA_CTRL_DMACR_RUN);

    status = vdma_ctrl_read(dev, VDMA_CTRL_DMASR);
    while(status & VDMA_CTRL_DMASR_HALTED)
    {
        status = vdma_ctrl_read(dev, VDMA_CTRL_DMASR);
    }
}

static int vdma_ctrl_configure(xrcc_cam_dev_t *dev)
{
    u32 i;

    XRCC_DEBUG(xrcc_dev_to_dev(dev),
               "vdma_ctrl_configure(): Resettting VDMA");

    /* Reset the engine */
    vdma_ctrl_reset(dev);

    XRCC_DEBUG(xrcc_dev_to_dev(dev),
               "vdma_ctrl_configure(): Setting the DMACR register");

    /* Start VDMA & wait for it to be started */
    vdma_ctrl_write(dev, VDMA_CTRL_DMACR,
                    VDMA_CTRL_DMACR_GENLOCK_EN |
                    VDMA_CTRL_DMACR_CIRCPARK | VDMA_CTRL_DMACR_ERR_IRQEN);
    vdma_ctrl_run(dev);

    /* Reset reg index */
    vdma_ctrl_write(dev, VDMA_CTRL_REG_INDEX, 0);

    /* Fill in all buffer addresses */
    for(i = 0; i < dev->frm_cnt; i++)
    {
        if((dev->vect_bufs[i].buf == NULL) ||
           (dev->vect_bufs[i].addr == 0))
        {
            XRCC_ERR(xrcc_dev_to_dev(dev),
                     "vdma_ctrl_configure(): Not all buffers are initialized");
            vdma_ctrl_reset(dev);
            return -EINVAL;
        }

        vdma_ctrl_write(dev, VDMA_CTRL_ST_ADDR + (i*4),
                        dev->vect_bufs[i].addr);
    }

    /* Set HSIZE & FRMDLY_STRIDE */
    vdma_ctrl_write(dev, VDMA_CTRL_HSIZE, (dev->width * dev->bpp));
    vdma_ctrl_write(dev, VDMA_CTRL_FRMDLY_STRIDE, (dev->width * dev->bpp));

    // Set VSIZE - MUST BE LAST! */
    vdma_ctrl_write(dev, VDMA_CTRL_VSIZE, dev->height);

    return 0;
}

#else // VDMA_CTRL_INTEGRATED

// use official xilinx_dma driver to configure VDMA HW

static int xrcc_cam_configure_vdma(xrcc_cam_dev_t *dev)
{
    struct xilinx_vdma_config xil_vdma_config;
    int err;

    /* Configure Xilinx VDMA */
    memset(&xil_vdma_config, 0, sizeof(struct xilinx_vdma_config));

    xil_vdma_config.frm_cnt_en = 1;
    xil_vdma_config.coalesc    = 1;//dev->frm_cnt;
    xil_vdma_config.park       = 0;
    xil_vdma_config.reset      = 1;

    err = xilinx_vdma_channel_set_config(dev->dma_chan, &xil_vdma_config);
    if(err)
    {
        XRCC_ERR(xrcc_dev_to_dev(dev),
                 "Configuring Xilinx VDMA channel failed: %d", err);
        return err;
    }

    return 0;
}
#endif // VDMA_CTRL_INTEGRATED

static void dump_num_buffers(xrcc_cam_dev_t *dev, const char *text)
{
    int i = 0;
    xrcc_dma_buffer_t *tmp, *ntmp;
    char out_text[256];

    list_for_each_entry_safe(tmp, ntmp, &dev->queued_bufs, queue) {
        i++;
    }

    sprintf(&out_text[0], "%s number of buffers: %d", text, i);
    XRCC_DEBUG(xrcc_dev_to_dev(dev), out_text);
}


static int xrcc_cam_add_channel(xrcc_cam_dev_t *dev,  const char *chan_name)
{
    struct dma_chan *chan;
    int err;

    // TODO: We are requesting axivdma1 (we define 2 in DTS eventhough
    // in HW we had only one connected. This should be fixed but when I
    // tried it I had problems and reverted to original DTS settings to
    // at least be able to request chan successfully.
    chan = dma_request_chan(xrcc_dev_to_dev(dev), "axivdma1");
    if (IS_ERR(chan) || (chan == NULL)) {
        err = PTR_ERR(chan);
        XRCC_ERR(xrcc_dev_to_dev(dev),
                "RX DMA channel not found, check dma-names in DT");
        return err;
    }

    dev->dma_chan = chan;

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "Adding DMA channel: %s",
               dma_chan_name(chan));

    return 0;
}

#ifndef VDMA_CTRL_INTEGRATED
static void xrcc_cam_rx_callback(void *param)
{
    xrcc_dma_buffer_t *buf = param;
    xrcc_cam_dev_t *dev = (xrcc_cam_dev_t *)buf->xrcc_dev;

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "xrcc_cam_rx_callback() start");

    /* List elements in incomming vb2_buffer */
    dump_num_buffers(dev, "xrcc_cam_rx_callback() start");

    spin_lock(&dev->queued_lock);
    list_del(&buf->queue);
    spin_unlock(&dev->queued_lock);

    buf->buf.field = V4L2_FIELD_NONE;
    buf->buf.sequence = dev->sequence++;
    buf->buf.vb2_buf.timestamp = ktime_get_ns();
    vb2_set_plane_payload(&buf->buf.vb2_buf, 0, dev->format.sizeimage);
    vb2_buffer_done(&buf->buf.vb2_buf, VB2_BUF_STATE_DONE);

    dump_num_buffers(dev, "xrcc_cam_rx_callback() end");
    XRCC_DEBUG(xrcc_dev_to_dev(dev), "xrcc_cam_rx_callback() end");
}
#endif // VDMA_CTRL_INTEGRATED

static int xrcc_cam_queue_setup(struct vb2_queue *vq,
                                unsigned int *nbuffers, unsigned int *nplanes,
                                unsigned int sizes[],
                                struct device *alloc_devs[])
{
    xrcc_cam_dev_t *dev = vb2_get_drv_priv(vq);

    XRCC_DEBUG(xrcc_dev_to_dev(dev),
               "xrcc_cam_queue_setup() start, num_buffers=%d nbuffers=%d",
               vq->num_buffers, *nbuffers);

    *nbuffers = dev->frm_cnt;

    /* Make sure the image size is large enough. */
    if(*nplanes)
    {
        return sizes[0] < dev->format.sizeimage ? -EINVAL : 0;
    }

    *nplanes = 1;
    sizes[0] = dev->format.sizeimage;

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "xrcc_cam_queue_setup() end");

    return 0;
}

static int xrcc_cam_buffer_prepare(struct vb2_buffer *vb)
{
    struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
    xrcc_cam_dev_t *dev = vb2_get_drv_priv(vb->vb2_queue);
    xrcc_dma_buffer_t *buf = to_xrcc_dma_buffer(vbuf);
    unsigned long size = dev->format.sizeimage;

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "xrcc_cam_buffer_prepare() start");

    buf->xrcc_dev = dev;

    if(vb2_plane_size(vb, 0) < size)
    {
        XRCC_ERR(xrcc_dev_to_dev(dev),
                 "Plane size too small for data (%lu < %lu)",
                 vb2_plane_size(vb, 0), size);
        return -EINVAL;
    }

    vb2_set_plane_payload(&buf->buf.vb2_buf, 0, size);

    dump_num_buffers(dev, "xrcc_cam_buffer_prepare()");

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "xrcc_cam_buffer_prepare() end");

    return 0;
}

static void xrcc_cam_buffer_queue(struct vb2_buffer *vb)
{
    struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
    xrcc_cam_dev_t *dev = vb2_get_drv_priv(vb->vb2_queue);
    dma_addr_t addr = vb2_dma_contig_plane_dma_addr(vb, 0);
    xrcc_dma_buffer_t *buf = to_xrcc_dma_buffer(vbuf);

#ifndef VDMA_CTRL_INTEGRATED
    struct dma_async_tx_descriptor *desc;
    dma_cookie_t rx_cookie;
    u32 flags;
    struct dma_device *dma_dev = dev->dma_chan->device;
    int err;
#endif // !VDMA_CTRL_INTEGRATED

    XRCC_DEBUG(xrcc_dev_to_dev(dev),
               "xrcc_cam_buffer_queue() start, buffer address=0x%08x",
               (uint32_t)addr);

#ifdef USE_VECTOR_BUFFERS
    {
        // find the next free slot
        int idx, found = 0;
        spin_lock_irq(&dev->queued_lock);
        for(idx = 0; idx < dev->frm_cnt; idx++)
        {
            if((dev->vect_bufs[idx].buf == NULL) ||
               (dev->vect_bufs[idx].addr == addr))
            {
                found = 1;
                break;
            }
        }

        if(!found)
        {
            /* TODO: We should actually free all buffers */
            XRCC_ERR(xrcc_dev_to_dev(dev),
                     "xrcc_cam_buffer_queue(): Too many buffers?");
            vb2_buffer_done(&buf->buf.vb2_buf, VB2_BUF_STATE_ERROR);
            spin_unlock_irq(&dev->queued_lock);
            return;
        }

        dev->vect_bufs[idx].buf        = vbuf;
        dev->vect_bufs[idx].addr       = addr;
        dev->vect_bufs[idx].registered = 1;

        spin_unlock_irq(&dev->queued_lock);
        XRCC_DEBUG(xrcc_dev_to_dev(dev),
                   "xrcc_cam_buffer_queue() start, address=0x%08x added to %d index",
                   (uint32_t)addr, idx);
    }
#endif

#ifndef VDMA_CTRL_INTEGRATED
    err = xrcc_cam_configure_vdma(dev);
    if(err)
    {
        XRCC_ERR(xrcc_dev_to_dev(dev),
                 "xrcc_cam_buffer_queue(): Configuring VDMA failed");
        vb2_buffer_done(&buf->buf.vb2_buf, VB2_BUF_STATE_ERROR);
        return;
    }

        // Map allocated buffers to DMA addresses
    dev->xt.dir         = DMA_DEV_TO_MEM;
    dev->xt.sgl[0].size = dev->format.width * dev->bpp;
    dev->xt.sgl[0].icg  = 0;
//    dev->xt.sgl[0].icg  = dev->format.bytesperline - dev->xt.sgl[0].size;
    dev->xt.frame_size  = 1;
    dev->xt.numf        = dev->format.height;

    flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;

    // TODO: Add dst_start address - but we would like to have tripple buffer?!
    dev->xt.dst_start = addr;

    desc = dma_dev->device_prep_interleaved_dma(dev->dma_chan, &dev->xt, flags);
    if(!desc)
    {
        XRCC_ERR(xrcc_dev_to_dev(dev), "Failed to prepare DMA transfer");
        vb2_buffer_done(&buf->buf.vb2_buf, VB2_BUF_STATE_ERROR);
        return;
    }

    desc->callback = xrcc_cam_rx_callback;
    desc->callback_param = buf;

    spin_lock_irq(&dev->queued_lock);
    list_add_tail(&buf->queue, &dev->queued_bufs);
    spin_unlock_irq(&dev->queued_lock);

    rx_cookie = desc->tx_submit(desc);
    if(dma_submit_error(rx_cookie))
    {
        XRCC_ERR(xrcc_dev_to_dev(dev), "Submit error, rx_cookie: %d",
                 rx_cookie);
        return;
    }

    if(vb2_is_streaming(&dev->queue))
    {
        XRCC_DEBUG(xrcc_dev_to_dev(dev), "xrcc_cam_buffer_queue()"
                   " still streaming request pending transactions");
        dma_async_issue_pending(dev->dma_chan);
    }
#endif // !VDMA_CTRL_INTEGRATED


    /* List elements in incomming vb2_buffer */
    dump_num_buffers(dev, "xrcc_cam_buffer_queue() end");

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "xrcc_cam_buffer_queue() end");
}

static int xrcc_cam_start_streaming(struct vb2_queue *vq, unsigned int count)
{
    xrcc_cam_dev_t *dev = vb2_get_drv_priv(vq);
//    xrcc_dma_buffer_t *buf, *nbuf;

    if(!dev)
    {
        printk("Don't have any private structure?!");
        return -1;
    }

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "xrcc_cam_start_streaming()");


    dev->sequence = 0;

#ifndef VDMA_CTRL_INTEGRATED
    /* Start the DMA engine. This must be done before starting the blocks
     * in the pipeline to avoid DMA synchronization issues.
     */
    dma_async_issue_pending(dev->dma_chan);
#endif // VDMA_CTRL_INTEGRATED

#ifdef USE_VECTOR_BUFFERS
    // We have everything we need - we have all buffers reserved
    // so we should configure VDMA engine and started the stream
    vdma_ctrl_configure(dev);
#endif
#ifdef VIDEO_CTRL_INTEGRATED
    video_ctrl_set_size(dev, dev->width, dev->height, dev->bpp);
    video_ctrl_run(dev);
    video_ctrl_clear_err(dev);
    video_ctrl_inten(dev, 1);

    video_ctrl_dump_meas(dev);
#endif

    return 0;
}

static void xrcc_cam_stop_streaming(struct vb2_queue *vq)
{
    xrcc_cam_dev_t *dev = vb2_get_drv_priv(vq);

#ifdef USE_VECTOR_BUFFERS
    int i;
    for(i = 0; i < dev->frm_cnt; i++)
    {
        spin_lock_irq(&dev->queued_lock);
        if((dev->vect_bufs[i].addr != 0) &&
           (dev->vect_bufs[i].buf != NULL) &&
           (dev->vect_bufs[i].registered))
        {
            vb2_buffer_done(&dev->vect_bufs[i].buf->vb2_buf, VB2_BUF_STATE_ERROR);
            dev->vect_bufs[i].registered = 0;
            dev->vect_bufs[i].buf = NULL;
            dev->vect_bufs[i].addr = 0;
        }
        spin_unlock_irq(&dev->queued_lock);
    }
    vdma_ctrl_reset(dev);
#endif

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "xrcc_cam_stop_streaming()");


#ifdef VIDEO_CTRL_INTEGRATED
    video_ctrl_inten(dev, 0);
    video_ctrl_dump_meas(dev);
    video_ctrl_stop(dev);
#endif

#ifndef USE_VECTOR_BUFFERS
    {
        xrcc_dma_buffer_t *buf, *nbuf;

        spin_lock_irq(&dev->queued_lock);
        list_for_each_entry_safe(buf, nbuf, &dev->queued_bufs, queue) {
            vb2_buffer_done(&buf->buf.vb2_buf, VB2_BUF_STATE_ERROR);
            list_del(&buf->queue);
        }
        spin_unlock_irq(&dev->queued_lock);
    }
#endif // USE_VECTOR_BUFFERS
}

static const struct vb2_ops xrcc_cam_queue_qops = {
    .queue_setup = xrcc_cam_queue_setup,
    .buf_prepare = xrcc_cam_buffer_prepare,
    .buf_queue = xrcc_cam_buffer_queue,
    .wait_prepare = vb2_ops_wait_prepare,
    .wait_finish = vb2_ops_wait_finish,
    .start_streaming = xrcc_cam_start_streaming,
    .stop_streaming = xrcc_cam_stop_streaming,
};


static int xrcc_cam_querycap(struct file *file, void *fh,
                             struct v4l2_capability *cap)
{
    struct v4l2_fh *vfh = file->private_data;
    xrcc_cam_dev_t *dev = to_xrcc_cam_dev(vfh->vdev);

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "xrcc_cam_querycap() start");
    cap->capabilities =
        V4L2_CAP_DEVICE_CAPS | V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE;

    cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;

    /* TODO: Which driver? */
    strlcpy(cap->driver, "xrcc-cam", sizeof(cap->driver));
    strlcpy(cap->card, dev->video.name, sizeof(cap->card));
    snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s:%u",
             dev->dev->of_node->name, dev->port);

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "xrcc_cam_querycap() end");
    return 0;
}

/* TODO: Add more formats */
static int xrcc_cam_enum_format(struct file *file, void *fh,
                                struct v4l2_fmtdesc *f)
{
    struct v4l2_fh *vfh = file->private_data;
    xrcc_cam_dev_t *dev = to_xrcc_cam_dev(vfh->vdev);

    const char description[] = "4:2:2, packed, YUYV";
    if(f->index > 0)
    {
        return -EINVAL;
    }

    f->pixelformat = dev->format.pixelformat;

    // pixelformat = V4L2_PIX_FMT_YUYV, description: "4:2:2, packed, YUYV"
    strlcpy(f->description, (char *)&description[0], sizeof(f->description));

    return 0;
}

static int xrcc_cam_get_format(struct file *file, void *fh,
                               struct v4l2_format *format)
{
    struct v4l2_fh *vfh = file->private_data;
    xrcc_cam_dev_t *dev = to_xrcc_cam_dev(vfh->vdev);

    format->fmt.pix = dev->format;

    return 0;
}

static int xrcc_cam_try_format(struct file *file, void *fh,
                               struct v4l2_format *format)
{
    struct v4l2_fh *vfh = file->private_data;
    xrcc_cam_dev_t *dev = to_xrcc_cam_dev(vfh->vdev);
    struct v4l2_pix_format *pix = &format->fmt.pix;

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "xrcc_cam_try_format() start");

    // TODO: ??? Is this fine?
    memcpy(pix, &dev->format, sizeof(struct v4l2_pix_format));

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "xrcc_cam_try_format() end");

    return 0;
}

static int xrcc_cam_g_input(struct file *file, void *priv, unsigned int *i)
{
    *i = 0;

    return 0;
}

static int xrcc_cam_s_input(struct file *file, void *priv, unsigned int i)
{
    if (i > 0)
        return -EINVAL;

    return 0;
}

static int xrcc_cam_enum_input(struct file *file, void *priv,
                               struct v4l2_input *inp)
{
    xrcc_cam_dev_t *dev = file->private_data;

    if (inp->index != 0)
        return -EINVAL;

    /* default is camera */
    inp->type = V4L2_INPUT_TYPE_CAMERA;
    inp->std = dev->video.tvnorms;
    strcpy(inp->name, "Camera");

    return 0;
}

static int xrcc_cam_g_selection(struct file *file, void *fh,
                                struct v4l2_selection *s)
{
    xrcc_cam_dev_t *dev = file->private_data;

    /* With a wrong type no need to try to fall back to cropping */
    if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
        return -EINVAL;

    // TODO: selection not supported yet
    s->r.left = 0;
    s->r.top = 0;
    s->r.width = dev->width;
    s->r.height = dev->height;
    return 0;
}

static int xrcc_cam_s_selection(struct file *file, void *fh,
                                struct v4l2_selection *s)
{
//    xrcc_cam_dev_t *dev = file->private_data;

    /* TODO: selection not supported yet - ignore it */
    return 0;
}

static int xrcc_cam_g_parm(struct file *file, void *fh,
                           struct v4l2_streamparm *a)
{
//    xrcc_cam_dev_t *dev = file->private_data;

    /* TODO: Not supported yet */
    return 0;
}

static int xrcc_cam_s_parm(struct file *file, void *fh,
                           struct v4l2_streamparm *a)
{
//    xrcc_cam_dev_t *dev = file->private_data;

    /* TODO: Not supported yet */

    return 0;
}

static int xrcc_cam_set_format(struct file *file, void *fh,
                               struct v4l2_format *format)
{
    struct v4l2_fh *vfh = file->private_data;
    xrcc_cam_dev_t *dev = to_xrcc_cam_dev(vfh->vdev);
    struct v4l2_pix_format *pix = &format->fmt.pix;
    int memcmp_res = memcmp(pix, &dev->format, sizeof(struct v4l2_pix_format));

    // TODO: xrcc_cam_set_format() currently supports only 1 format!

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "xrcc_cam_set_format() start, memcmp=%d",
               memcmp_res);

    if(vb2_is_busy(&dev->queue))
    {
        XRCC_ERR(xrcc_dev_to_dev(dev),
                 "xrcc_cam_set_format() vb2_is_busy()");
        return -EBUSY;
    }

    /* TODO: If want to still support only 1 format - use memcmp */
    if((pix->pixelformat != dev->format.pixelformat) ||
       (pix->width       != dev->format.width)       ||
       (pix->height      != dev->format.height))
    {

        XRCC_DEBUG(xrcc_dev_to_dev(dev), "pixelformat: %d != %d",
                   pix->pixelformat, dev->format.pixelformat);
        XRCC_DEBUG(xrcc_dev_to_dev(dev), "width: %d != %d",
                   pix->width, dev->format.width);
        XRCC_DEBUG(xrcc_dev_to_dev(dev), "height: %d != %d",
                   pix->height, dev->format.height);
        XRCC_DEBUG(xrcc_dev_to_dev(dev), "field: %d != %d",
                   pix->field, dev->format.field);
        XRCC_ERR(xrcc_dev_to_dev(dev),
                 "xrcc_cam_set_format() incorrect parameters");
        return -EINVAL;
    }

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "xrcc_cam_set_format() end");

    return 0;
}

static const struct v4l2_ioctl_ops xrcc_cam_ioctl_ops = {
    .vidioc_querycap         = xrcc_cam_querycap,
    .vidioc_enum_fmt_vid_cap = xrcc_cam_enum_format,
    .vidioc_g_fmt_vid_cap    = xrcc_cam_get_format,
    .vidioc_g_fmt_vid_out    = xrcc_cam_get_format,
    .vidioc_s_fmt_vid_cap    = xrcc_cam_set_format,
    .vidioc_s_fmt_vid_out    = xrcc_cam_set_format,
    .vidioc_try_fmt_vid_cap  = xrcc_cam_try_format,
    .vidioc_try_fmt_vid_out  = xrcc_cam_try_format,
    .vidioc_reqbufs          = vb2_ioctl_reqbufs,
    .vidioc_querybuf         = vb2_ioctl_querybuf,
    .vidioc_qbuf             = vb2_ioctl_qbuf,
    .vidioc_dqbuf            = vb2_ioctl_dqbuf,
    .vidioc_create_bufs      = vb2_ioctl_create_bufs,
    .vidioc_expbuf           = vb2_ioctl_expbuf,
    .vidioc_streamon         = vb2_ioctl_streamon,
    .vidioc_streamoff        = vb2_ioctl_streamoff,
    // dummy ones (needed by ffmpeg)
    .vidioc_g_input          = xrcc_cam_g_input,
    .vidioc_s_input          = xrcc_cam_s_input,
    .vidioc_enum_input       = xrcc_cam_enum_input,
    .vidioc_g_selection      = xrcc_cam_g_selection,
    .vidioc_s_selection      = xrcc_cam_s_selection,
    .vidioc_g_parm           = xrcc_cam_g_parm,
    .vidioc_s_parm           = xrcc_cam_s_parm,
};

static const struct v4l2_file_operations xrcc_cam_dma_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = video_ioctl2,
    .open           = v4l2_fh_open,
    .release        = vb2_fop_release,
    .poll           = vb2_fop_poll,
    .mmap           = vb2_fop_mmap,
};

#ifdef VIDEO_CTRL_INTEGRATED
static irqreturn_t xrcc_cam_irq_handler(int irq, void *data)
{
    xrcc_cam_dev_t *dev = data;
    u32 rx_ctrl_reg = video_ctrl_read(dev, VIDEO_CTRL_RX_CTRL);
    int idx;

    //TODO: bug fixing - sometimes it crashes because unmap() was called
    if(!dev->video_ctrl_mem || !dev->vdma_ctrl_mem)
    {
        XRCC_ERR(xrcc_dev_to_dev(dev),
                 "xrcc_cam_irq_handler() called before video or VDMA registers mapped");
        return IRQ_HANDLED;
    }

    XRCC_DEBUG(xrcc_dev_to_dev(dev),
               "xrcc_cam_irq_handler() rx_ctrl_reg=0x%08x",
               rx_ctrl_reg);

    idx = video_ctrl_get_idx(rx_ctrl_reg, dev->frm_cnt);

    XRCC_DEBUG(xrcc_dev_to_dev(dev),
               "New frame index=%d in buffer address=0x%08x available (reg=%d)",
               idx, dev->vect_bufs[idx].addr, dev->vect_bufs[idx].registered);

    spin_lock(&dev->queued_lock);
    if(dev->vect_bufs[idx].registered)
    {
        dev->vect_bufs[idx].registered = 0;
        dev->vect_bufs[idx].buf->field = V4L2_FIELD_NONE;
        dev->vect_bufs[idx].buf->sequence = dev->sequence++;
        dev->vect_bufs[idx].buf->vb2_buf.timestamp = ktime_get_ns();

        vb2_buffer_done(&dev->vect_bufs[idx].buf->vb2_buf, VB2_BUF_STATE_DONE);
    }
    spin_unlock(&dev->queued_lock);

    // Clear interrupt
    video_ctrl_write(dev, VIDEO_CTRL_RX_CTRL, rx_ctrl_reg);

    return IRQ_HANDLED;
}
#endif

static int xrcc_cam_video_config(xrcc_cam_dev_t *dev)
{
    int err = 0;

    mutex_init(&dev->lock);
    INIT_LIST_HEAD(&dev->queued_bufs);
    spin_lock_init(&dev->queued_lock);

    /* TODO: Should be initialized together with size/BPP... */
    dev->format.pixelformat  = V4L2_PIX_FMT_YUYV;
    dev->format.colorspace   = V4L2_COLORSPACE_SRGB;
    dev->format.field        = V4L2_FIELD_ANY;
    dev->format.width        = dev->width;
    dev->format.height       = dev->height;
    dev->format.bytesperline = dev->width * dev->bpp;
    dev->format.sizeimage    = dev->width * dev->height * dev->bpp;

    dev->pad.flags = MEDIA_PAD_FL_SINK;
    err = media_entity_pads_init(&dev->video.entity, 1, &dev->pad);
    if(err < 0)
    {
        XRCC_ERR(xrcc_dev_to_dev(dev), "media_entity_pads_init() failed");
        return err;
    }

    // Video structure
    snprintf(dev->video.name, sizeof(dev->video.name), "%s %s %u",
             dev->dev->of_node->name,"output", dev->port);

    dev->video.fops = &xrcc_cam_dma_fops;
    dev->video.v4l2_dev = &dev->v4l2_dev;
    dev->video.queue    = &dev->queue;
    dev->video.vfl_type = VFL_TYPE_GRABBER;
    dev->video.vfl_dir  = VFL_DIR_RX;
    dev->video.release  = video_device_release_empty;
    dev->video.ioctl_ops = &xrcc_cam_ioctl_ops;
    dev->video.lock     = &dev->lock;

    video_set_drvdata(&dev->video, dev);

    // Buffer queues
    dev->queue.type               = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    dev->queue.io_modes           = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
    dev->queue.lock               = &dev->lock;
    dev->queue.drv_priv           = dev;
    dev->queue.buf_struct_size    = sizeof(xrcc_dma_buffer_t);
    dev->queue.ops                = &xrcc_cam_queue_qops;
    dev->queue.mem_ops            = &vb2_dma_contig_memops;
    dev->queue.min_buffers_needed = dev->frm_cnt;
    dev->queue.timestamp_flags    =
        V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC | V4L2_BUF_FLAG_TSTAMP_SRC_EOF;
    dev->queue.dev = xrcc_dev_to_dev(dev);

    err = vb2_queue_init(&dev->queue);
    if(err < 0)
    {
        XRCC_ERR(xrcc_dev_to_dev(dev), "vb2_queue_init() failed");
        return err;
    }

    err = video_register_device(&dev->video, VFL_TYPE_GRABBER, -1);
    if(err < 0)
    {
        XRCC_ERR(xrcc_dev_to_dev(dev), "video_register_device() failed");
        return err;
    }

    return err;
}

static int xrcc_cam_v4l2_cleanup(xrcc_cam_dev_t *dev)
{
    if(!IS_ERR(dev->dma_chan))
    {

    }

    v4l2_device_unregister(&dev->v4l2_dev);
    media_device_unregister(&dev->media_dev);
    media_device_cleanup(&dev->media_dev);

    return 0;
}

static int xrcc_cam_cleanup(xrcc_cam_dev_t *dev)
{
    if(!dev)
    {
        return 0;
    }

    if (dev->frame_ptr_irq > 0)
        free_irq(dev->frame_ptr_irq, dev);

    vb2_queue_release(&dev->queue);

#ifdef VIDEO_CTRL_INTEGRATED
    iounmap(dev->video_ctrl_mem);
#endif

#ifdef VDMA_CTRL_INTEGRATED
    iounmap(dev->vdma_ctrl_mem);
#endif

    if(video_is_registered(&dev->video))
    {
        video_unregister_device(&dev->video);
    }

    media_entity_cleanup(&dev->video.entity);

    mutex_destroy(&dev->lock);

    xrcc_cam_v4l2_cleanup(dev);

    if(dev->dma_chan)
    {
        XRCC_DEBUG(xrcc_dev_to_dev(dev), "Releasing DMA channel: %s",
                 dma_chan_name(dev->dma_chan));
        dma_release_channel(dev->dma_chan);
        dev->dma_chan = NULL;
    }

    return 0;
}

static int xrcc_cam_v4l2_init(xrcc_cam_dev_t *dev)
{
    int retVal;

    dev->media_dev.dev = xrcc_dev_to_dev(dev);
    strlcpy(dev->media_dev.model, "RC Car Control Video Device",
            sizeof(dev->media_dev.model));
    dev->media_dev.hw_revision = 0;

    media_device_init(&dev->media_dev);

    dev->v4l2_dev.mdev = &dev->media_dev;
    retVal = v4l2_device_register(xrcc_dev_to_dev(dev), &dev->v4l2_dev);
    if(retVal < 0)
    {
        XRCC_ERR(xrcc_dev_to_dev(dev), "V4L2 device registraion failed (%d)",
                 retVal);
        media_device_cleanup(&dev->media_dev);
        return retVal;
    }

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "V4L2 device registered");

    return 0;
}

static int xrcc_cam_vdma_reset(xrcc_cam_dev_t *dev)
{
    int err;
    struct xilinx_vdma_config xil_vdma_config;

    /* Configure Xilinx VDMA */
    memset(&xil_vdma_config, 0, sizeof(struct xilinx_vdma_config));

    xil_vdma_config.frm_cnt_en = 1;
    xil_vdma_config.coalesc    = 1;//dev->frm_cnt;
    xil_vdma_config.park       = 0;
    xil_vdma_config.reset      = 1;

    err = xilinx_vdma_channel_set_config(dev->dma_chan, &xil_vdma_config);
    if(err)
    {
        XRCC_ERR(xrcc_dev_to_dev(dev),
                 "Resetting Xilinx VDMA channel failed: %d", err);
        return err;
    }

    /* In theory we should sleep a little bit... :) */
    return 0;
}

static int xrcc_cam_probe(struct platform_device *pdev)
{
    const uint32_t frm_cnt = 3; // TODO: Get that info from rx_chan!
    const uint32_t width  = 640;
    const uint32_t height = 480;
    const uint32_t bpp = 2;

    xrcc_cam_dev_t *xrcc_dev;
    int err;

    XRCC_DEBUG(&pdev->dev, "Probe entry");

    // Create internal structure
    xrcc_dev = devm_kzalloc(&pdev->dev, sizeof(xrcc_cam_dev_t), GFP_ATOMIC);
    if(xrcc_dev == NULL)
    {
        XRCC_ERR(&pdev->dev, "Can not allocate memory for private structure");
        return -ENOMEM;
    }

    memset(xrcc_dev, 0, sizeof(xrcc_cam_dev_t));

    xrcc_dev->dev = &pdev->dev;
    dev_set_drvdata(&pdev->dev, xrcc_dev);

    // TODO: Get the number of buffers from DMA driver 'somehow'
//    XRCC_INFO(&pdev->dev, "Reading num-fstores");
//    err = of_property_read_u32(rx_chan->dev->device.of_node,
//                               "xlnx,num-fstores", &frm_cnt);
//    if(err < 0)
//    {
//        XRCC_ERR(&pdev->dev, "Number of frame buffers not found, "
//                             "check num-fstores in DT");
//        return err;
//    }

    // start filling the structure
    xrcc_dev->frm_cnt = frm_cnt;
    xrcc_dev->width   = width;
    xrcc_dev->height  = height;
    xrcc_dev->bpp     = bpp;

    err = xrcc_cam_add_channel(xrcc_dev, "axivdma1");
    if(err)
    {
        XRCC_ERR(&pdev->dev, "Unable to add channels");
        goto probe_err_exit;
    }

    err = xrcc_cam_v4l2_init(xrcc_dev);
    if(err)
    {
        XRCC_ERR(xrcc_dev_to_dev(xrcc_dev), "Problem setting up V4L2 device");
        goto probe_err_exit;
    }

    err = xrcc_cam_video_config(xrcc_dev);
    if(err)
    {
        XRCC_ERR(xrcc_dev_to_dev(xrcc_dev), "Problem configuring the video settings");
        goto probe_err_exit;
    }

    // Reset the VDMA
    err = xrcc_cam_vdma_reset(xrcc_dev);
    if(err)
    {
        XRCC_ERR(xrcc_dev_to_dev(xrcc_dev), "Problem reseting VDMA controller");
        goto probe_err_exit;
    }

#ifdef VIDEO_CTRL_INTEGRATED

    // Map also Video controller
    {
        const uint32_t cVideoCtrlAddr   = 0x43C10000;

        xrcc_dev->video_ctrl_mem = ioremap(cVideoCtrlAddr, 100);
        XRCC_INFO(xrcc_dev_to_dev(xrcc_dev), "Mapped video controller, version: 0x%08x",
                  video_ctrl_version(xrcc_dev));
    }

    // Request and map alos interrupt
    XRCC_DEBUG(xrcc_dev_to_dev(xrcc_dev), "RCC Camera requesting IRQ");
    xrcc_dev->frame_ptr_irq = platform_get_irq(pdev, 0);
    err = devm_request_irq(xrcc_dev->dev, xrcc_dev->frame_ptr_irq,
                           xrcc_cam_irq_handler,
                           IRQF_SHARED, "xrcc-cam", xrcc_dev);
    if(err)
    {
        XRCC_ERR(xrcc_dev_to_dev(xrcc_dev), "Can not request IRQ %d\n",
                 xrcc_dev->frame_ptr_irq);
        return err;
    }
#endif

#ifdef VDMA_CTRL_INTEGRATED
    // Map also VDMA controller
    {
        const uint32_t cVdmaCtrlAddr = 0x43000000;

        xrcc_dev->vdma_ctrl_mem = ioremap(cVdmaCtrlAddr, 0xFF);
        if(!xrcc_dev->vdma_ctrl_mem)
        {
            XRCC_ERR(xrcc_dev_to_dev(xrcc_dev),
                     "VDMA control ioremap() fails");
            return err;
        }

        XRCC_DEBUG(xrcc_dev_to_dev(xrcc_dev), "VDMA control version: 0x%08x",
                   vdma_ctrl_version(xrcc_dev));
    }
#endif

#ifdef USE_VECTOR_BUFFERS
    xrcc_dev->vect_bufs =
        (xrcc_cam_buf_t *)devm_kzalloc(&pdev->dev,
                                       sizeof(xrcc_cam_buf_t)*xrcc_dev->frm_cnt,
                                       GFP_ATOMIC);
    if(xrcc_dev->vect_bufs == NULL)
    {
        XRCC_ERR(xrcc_dev_to_dev(xrcc_dev),
                 "Can not locate vector buffers");
        return -ENOMEM;
    }
#endif
    memset(xrcc_dev->vect_bufs, 0, sizeof(xrcc_cam_buf_t)*xrcc_dev->frm_cnt);

    XRCC_INFO(xrcc_dev_to_dev(xrcc_dev), "RCC Camera Driver Probed");

    return 0;

probe_err_exit:
    xrcc_cam_cleanup(xrcc_dev);

    return err;
}

static int xrcc_cam_remove(struct platform_device *pdev)
{
    xrcc_cam_dev_t *xrcc_dev = dev_get_drvdata(&pdev->dev);


    xrcc_cam_cleanup(xrcc_dev);

    XRCC_INFO(xrcc_dev_to_dev(xrcc_dev), "RCC Camera Driver Removed");

    return 0;
}

static const struct of_device_id xrcc_cam_of_ids[] = {
    { .compatible = "xlnx,rcc-cam-1.0" },
    {}
};

static struct platform_driver xrcc_cam_driver = {
    .driver = {
        .name = "xrcc_cam",
        .owner = THIS_MODULE,
        .of_match_table = xrcc_cam_of_ids,
    },
    .probe = xrcc_cam_probe,
    .remove = xrcc_cam_remove,
};

module_platform_driver(xrcc_cam_driver);

MODULE_AUTHOR("Jure Menart (juremenart@gmail.com)");
MODULE_DESCRIPTION("RC Car Control Camera Driver");
MODULE_LICENSE("GPL v2");
