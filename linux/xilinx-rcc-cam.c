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

typedef struct xrcc_cam_dev_s {
    // number of frame buffers, must be set only once at the beginning
    // TODO: Check if not better to have size/bpp/buf_size info only in
    //       v4l2_pix_format
    uint32_t                   frm_cnt;
    uint32_t                   width, height, bpp;
    uint32_t                   buf_size; // automatically calculated when allocating buffers

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

    /* old things to be removed??! */
    enum dma_transaction_type  type;
    bool                       done;
    uint32_t                 **buf; // Buffers in memory
    dma_addr_t                *phy_addr; // Buffers mapped to DMA structures
} xrcc_cam_dev_t;

typedef struct xrcc_dma_buffer_s {
    struct vb2_v4l2_buffer buf;
    struct list_head       queue;
    xrcc_cam_dev_t *xrcc_dev;
} xrcc_dma_buffer_t;

#define to_xrcc_cam_dev(vdev) container_of(vdev, xrcc_cam_dev_t, video)
#define to_xrcc_dma_buffer(vb)	container_of(vb, xrcc_dma_buffer_t, buf)

static struct device *xrcc_dev_to_dev(xrcc_cam_dev_t *dev)
{
    return dev->dev;
}

static int xrcc_cam_cleanup_buf(xrcc_cam_dev_t *dev)
{
    uint32_t i;

    if(dev->buf)
    {
        for(i = 0; i < dev->frm_cnt; i++)
        {
            if(dev->buf[i])
            {
                kfree(dev->buf[i]);
            }

        }

        kfree(dev->buf);
        dev->buf = NULL;
    }

    if(dev->phy_addr)
    {
        kfree(dev->phy_addr);
        dev->phy_addr = NULL;
    }

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "DMA buffers released");

    return 0;
}

static int xrcc_cam_init_buf(xrcc_cam_dev_t *dev)
{
    uint32_t i;

    dev->buf_size = dev->width * dev->height * dev->bpp;

    // First cleanup if needed!
    xrcc_cam_cleanup_buf(dev);

    dev->buf = kcalloc(dev->frm_cnt, sizeof(uint32_t *), GFP_KERNEL);
    if(!dev->buf)
    {
        goto mem_err_exit;
    }
    for(i = 0; i < dev->frm_cnt; i++)
    {
        dev->buf[i] = kmalloc(dev->buf_size, GFP_KERNEL);
        if(!dev->buf[i])
        {
            goto mem_err_exit;
        }

        memset(dev->buf[i], 0, dev->buf_size);
    }

    dev->phy_addr = kmalloc(sizeof(dma_addr_t) * dev->frm_cnt, GFP_KERNEL);
    if(!dev->phy_addr)
    {
        goto mem_err_exit;
    }

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "DMA buffers initialized "
               "(%d buffers of %d x %d x %d)",  dev->frm_cnt, dev->width,
               dev->height, dev->bpp);

    return 0;

mem_err_exit:
    xrcc_cam_cleanup_buf(dev);
    return -ENOMEM;
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

static void xrcc_cam_slave_rx_callback(void *param)
{
    xrcc_cam_dev_t *dev = (xrcc_cam_dev_t *)param;
    XRCC_DEBUG(xrcc_dev_to_dev(dev), "xrcc_cam_slave_rx_callback()");
    complete(&dev->rx_cmp);

}

static int xrcc_cam_queue_setup(struct vb2_queue *vq,
                                unsigned int *nbuffers, unsigned int *nplanes,
                                unsigned int sizes[],
                                struct device *alloc_devs[])
{
    xrcc_cam_dev_t *dev = vb2_get_drv_priv(vq);

    /* Make sure the image size is large enough. */
    if (*nplanes)
        return sizes[0] < dev->format.sizeimage ? -EINVAL : 0;

    *nplanes = 1;
    sizes[0] = dev->format.sizeimage;

    return 0;
}

static int xrcc_cam_buffer_prepare(struct vb2_buffer *vb)
{
    struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
    xrcc_cam_dev_t *dev = vb2_get_drv_priv(vb->vb2_queue);
    xrcc_dma_buffer_t *buf = to_xrcc_dma_buffer(vbuf);

    buf->xrcc_dev = dev;

    return 0;
}

static void xrcc_cam_buffer_queue(struct vb2_buffer *vb)
{
    struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
    xrcc_cam_dev_t *dev = vb2_get_drv_priv(vb->vb2_queue);
    xrcc_dma_buffer_t *buf = to_xrcc_dma_buffer(vbuf);
    struct dma_async_tx_descriptor *desc;
    dma_addr_t addr = vb2_dma_contig_plane_dma_addr(vb, 0);
    u32 flags;

    /* TODO: configure the DMA! */
    /* dev->xt... */
    /* prep_interleaved_dma ... */
    /* callback .. */

    spin_lock_irq(&dev->queued_lock);
    list_add_tail(&buf->queue, &dev->queued_bufs);
    spin_unlock_irq(&dev->queued_lock);

    // TODO: tx_submit()

    if(vb2_is_streaming(&dev->queue))
    {
        dma_async_issue_pending(dev->dma_chan);
    }
}

static int xrcc_cam_start_streaming(struct vb2_queue *vq, unsigned int count)
{
    xrcc_cam_dev_t *dev = vb2_get_drv_priv(vq);
    xrcc_dma_buffer_t *buf, *nbuf;

    // TODO...
}

static void xrcc_cam_stop_streaming(struct vb2_queue *vq)
{
    xrcc_cam_dev_t *dev = vb2_get_drv_priv(vq);
    xrcc_dma_buffer_t *buf, *nbuf;

    // TODO...
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

    // TODO
    return 0;
}

static int xrcc_cam_enum_format(struct file *file, void *fh,
                                struct v4l2_fmtdesc *f)
{
    struct v4l2_fh *vfh = file->private_data;
    xrcc_cam_dev_t *dev = to_xrcc_cam_dev(vfh->vdev);

    // TODO
    return 0;
}

static int xrcc_cam_get_format(struct file *file, void *fh,
                               struct v4l2_format *format)
{
    struct v4l2_fh *vfh = file->private_data;
    xrcc_cam_dev_t *dev = to_xrcc_cam_dev(vfh->vdev);

    // TODO
    return 0;
}

static int xrcc_cam_try_format(struct file *file, void *fh,
                               struct v4l2_format *format)
{
    struct v4l2_fh *vfh = file->private_data;
    xrcc_cam_dev_t *dev = to_xrcc_cam_dev(vfh->vdev);

    // TODO
    return 0;
}

static int xrcc_cam_set_format(struct file *file, void *fh,
                               struct v4l2_format *format)
{
    struct v4l2_fh *vfh = file->private_data;
    xrcc_cam_dev_t *dev = to_xrcc_cam_dev(vfh->vdev);

    // TODO
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
};

static const struct v4l2_file_operations xrcc_cam_dma_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = video_ioctl2,
    .open           = v4l2_fh_open,
    .release        = vb2_fop_release,
    .poll           = vb2_fop_poll,
    .mmap           = vb2_fop_mmap,
};

static int xrcc_cam_video_config(xrcc_cam_dev_t *dev)
{
    int err = 0;

    mutex_init(&dev->lock);
    INIT_LIST_HEAD(&dev->queued_bufs);
    spin_lock_init(&dev->queued_lock);

    /* TODO: Should be initialized together with size/BPP... */
    dev->format.pixelformat  = V4L2_PIX_FMT_YUYV;
    dev->format.colorspace   = V4L2_COLORSPACE_SRGB;
    dev->format.field        = V4L2_FIELD_NONE;
    dev->format.width        = dev->width;
    dev->format.height       = dev->height;
    dev->format.bytesperline = dev->width * dev->bpp;
    dev->format.sizeimage    = dev->width * dev->height * dev->bpp;

    dev->pad.flags = MEDIA_PAD_FL_SINK; // CAPT
    err = media_entity_pads_init(&dev->video.entity, 1, &dev->pad);
    if(err < 0)
    {
        XRCC_ERR(xrcc_dev_to_dev(dev), "media_entity_pads_init() failed");
        return err;
    }

    // Video structure
    dev->video.fops = &xrcc_cam_dma_fops; // JJJ
    snprintf(dev->video.name, sizeof(dev->video.name), "%s %s %u",
             dev->dev->of_node->name,"output", 0); // CAPT

    dev->video.v4l2_dev = &dev->v4l2_dev;
    dev->video.queue    = &dev->queue;
    dev->video.vfl_type = VFL_TYPE_GRABBER; // CAPT
    dev->video.vfl_dir  = VFL_DIR_RX; // CAPT
    dev->video.release  = video_device_release_empty;
    dev->video.ioctl_ops = &xrcc_cam_ioctl_ops;
    dev->video.lock     = &dev->lock;
    video_set_drvdata(&dev->video, dev);

    // Buffer queues
    dev->queue.type            = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    dev->queue.io_modes        = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
    dev->queue.lock            = &dev->lock;
    dev->queue.drv_priv        = dev;
    dev->queue.buf_struct_size = sizeof(xrcc_dma_buffer_t);
    dev->queue.ops             = &xrcc_cam_queue_qops;
    dev->queue.mem_ops         = &vb2_dma_contig_memops;
    dev->queue.timestamp_flags =
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

static int xrcc_cam_dma_config(xrcc_cam_dev_t *dev)
{
    int err, i;
    dma_cookie_t rx_cookie;
    struct dma_async_tx_descriptor *rxd;
    enum dma_ctrl_flags flags;
    enum dma_status status;
    struct xilinx_vdma_config  xil_vdma_config;

    // Cleanup the configuration
    memset(&xil_vdma_config, 0, sizeof(struct xilinx_vdma_config));

    xil_vdma_config.frm_cnt_en = 1;
    xil_vdma_config.coalesc = dev->frm_cnt;
    xil_vdma_config.park = 0;
    xil_vdma_config.reset = 1;

    err = xilinx_vdma_channel_set_config(dev->dma_chan, &xil_vdma_config);
    if(err)
    {
        XRCC_ERR(xrcc_dev_to_dev(dev),
                 "Configuring Xilinx VDMA channel failed: %d", err);
        return err;
    }

    // Map allocated buffers to DMA addresses
    dev->xt.dir         = DMA_DEV_TO_MEM;
    dev->xt.numf        = dev->height;
    dev->xt.sgl[0].size = dev->width * dev->bpp; // x2 because we have still packed YCbCr format to 8 bits
    dev->xt.sgl[0].icg  = 0;
    dev->xt.frame_size  = 1;

    flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;

    for(i = 0; i < (uint32_t)dev->frm_cnt; i++)
    {
        struct dma_device *dma_dev = dev->dma_chan->device;

        dev->phy_addr[i] = dma_map_single(dma_dev->dev, dev->buf[i],
                                          dev->buf_size, DMA_DEV_TO_MEM);

        err = dma_mapping_error(dma_dev->dev, dev->phy_addr[i]);
        if(err)
        {
            XRCC_ERR(xrcc_dev_to_dev(dev),
                     "Mapping of DMA address (%d) failed", i);
            return err;
        }

        dev->xt.dst_start   = dev->phy_addr[i];
        rxd = dma_dev->device_prep_interleaved_dma(dev->dma_chan,
                                                        &dev->xt,
                                                        flags);
        rx_cookie = rxd->tx_submit(rxd);
        XRCC_DEBUG(xrcc_dev_to_dev(dev), "rx_cookie=%d", rx_cookie);
        if(dma_submit_error(rx_cookie))
        {
            XRCC_ERR(xrcc_dev_to_dev(dev), "Submit error, rx_cookie: %d",
                     rx_cookie);
            return -1;
        }
    }

    init_completion(&dev->rx_cmp);
    rxd->callback = xrcc_cam_slave_rx_callback;
    rxd->callback_param = &dev;

    dma_async_issue_pending(dev->dma_chan);

    {
        // start & wait for compeltion
        unsigned long rx_tmo = msecs_to_jiffies(3000);
        dma_cookie_t last_cookie, used_cookie;

        rx_tmo = wait_for_completion_timeout(&dev->rx_cmp, rx_tmo);

        status = dma_async_is_tx_complete(dev->dma_chan, rx_cookie,
                                               &last_cookie, &used_cookie);

        {
            int i, k;

            for(i = 0; i < dev->frm_cnt; i++)
            {
                uint8_t *buf = (uint8_t*)dev->buf[i];

                printk("FB%d (0x%08x) = [", i, dev->phy_addr[i]);
                for(k = 0; k < 10; k++)
                {
                    printk(KERN_CONT " 0x%02x", buf[k]);
                }
                printk(KERN_CONT " ]\n");
            }
        }

        XRCC_DEBUG(xrcc_dev_to_dev(dev),
                   "status=%d last_cookie=%d used_cookie=%d",
                   status, last_cookie, used_cookie);
    }

    return 0;
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

    if(video_is_registered(&dev->video))
    {
        video_unregister_device(&dev->video);
    }

    media_entity_cleanup(&dev->video.entity);

    mutex_destroy(&dev->lock);

    xrcc_cam_v4l2_cleanup(dev);

    if(dev->phy_addr)
    {        int i;
        for(i = 0; i < dev->frm_cnt; i++)
        {
            struct dma_device *dma_dev = dev->dma_chan->device;
            dma_unmap_single(dma_dev->dev, dev->phy_addr[i],
                             dev->buf_size, DMA_DEV_TO_MEM);
        }
    }

    if(dev->dma_chan)
    {
        XRCC_DEBUG(xrcc_dev_to_dev(dev), "Releasing DMA channel: %s",
                 dma_chan_name(dev->dma_chan));
        dma_release_channel(dev->dma_chan);
        dev->dma_chan = NULL;
    }

    if(dev->buf)
    {
        xrcc_cam_cleanup_buf(dev);
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

    err = xrcc_cam_init_buf(xrcc_dev);
    if(err)
    {
        XRCC_ERR(xrcc_dev_to_dev(xrcc_dev), "Problem allocating DMA buffers");
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

    err = xrcc_cam_dma_config(xrcc_dev);
    if(err)
    {
        XRCC_ERR(xrcc_dev_to_dev(xrcc_dev), "Problem configuring the DMA");
        goto probe_err_exit;
    }


    XRCC_INFO(xrcc_dev_to_dev(xrcc_dev), "RCC Camera Driver Probed");

    return 0;

probe_err_exit:
    xrcc_cam_cleanup(xrcc_dev);

    return err;
}

static int xrcc_cam_remove(struct platform_device *pdev)
{
    xrcc_cam_dev_t *xrcc_dev = dev_get_drvdata(&pdev->dev);


    {
        int i, k;

        for(i = 0; i < xrcc_dev->frm_cnt; i++)
        {
            uint8_t *buf = (uint8_t*)xrcc_dev->buf[i];

            printk("FB%d (0x%08x) = [", i, xrcc_dev->phy_addr[i]);
            for(k = 0; k < 10; k++)
            {
                printk(KERN_CONT " 0x%02x", buf[k]);
            }
            printk(KERN_CONT " ]\n");
        }
    }

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
