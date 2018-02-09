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

#define XRCC_DEBUG(...) dev_info(__VA_ARGS__)
#define XRCC_INFO(...)  dev_info(__VA_ARGS__)
#define XRCC_ERR(...)   dev_err(__VA_ARGS__)

typedef struct xrcc_cam_dev_s {
    // number of frame buffers, must be set only once at the beginning
    uint32_t                   frm_cnt;
    uint32_t                   width, height;
    uint32_t                   buf_size; // automatically calculated when allocating buffers
    struct device             *dev;
    struct v4l2_device        *v4l2_dev;
    struct media_device       *media_dev;

    enum dma_transaction_type  type;
    bool                       done;
    uint32_t                 **buf; // Buffers in memory
    dma_addr_t                *phy_addr; // Buffers mapped to DMA structures

    struct xilinx_vdma_config  dma_config;
    struct dma_chan           *dma_chan;

    struct dma_async_tx_descriptor *rxd;
    struct dma_interleaved_template xt;
    struct completion rx_cmp;

    enum dma_ctrl_flags flags;
    enum dma_status     status;
} xrcc_cam_dev_t;

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

    dev->buf_size = dev->width * dev->height * 2;

    // First cleanup if needed!
    xrcc_cam_cleanup_buf(dev);

    dev->buf = kcalloc(dev->frm_cnt, sizeof(uint32_t *), GFP_KERNEL);
    if(!dev->buf)
    {
        goto mem_err_exit;
    }
    for(i = 0; i < dev->frm_cnt; i++)
    {
        int k;
        dev->buf[i] = kmalloc(dev->buf_size, GFP_KERNEL);
        if(!dev->buf[i])
        {
            goto mem_err_exit;
        }

        memset(dev->buf[i], 0, dev->buf_size);

        printk("xrcc_cam_init_buf() buffer %d (buf_size=%d) data=", i, dev->buf_size);
        for(k = 0; k < 6; k++)
        {
            printk("%d ", dev->buf[i][k]);
        }
    }

    dev->phy_addr = kmalloc(sizeof(dma_addr_t) * dev->frm_cnt, GFP_KERNEL);
    if(!dev->phy_addr)
    {
        goto mem_err_exit;
    }

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "DMA buffers initialized "
               "(%d buffers of %d x %d x %d)",  dev->frm_cnt, dev->width,
               dev->height, sizeof(uint32_t *));

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

static int xrcc_cam_dma_config(xrcc_cam_dev_t *dev)
{
    int err, i;
    dma_cookie_t rx_cookie;

    // Cleanup the configuration
    memset(&dev->dma_config, 0, sizeof(struct xilinx_vdma_config));

    dev->dma_config.frm_cnt_en = 1;
    dev->dma_config.coalesc = dev->frm_cnt * 10;
    dev->dma_config.park = 0;

    err = xilinx_vdma_channel_set_config(dev->dma_chan, &dev->dma_config);
    if(err)
    {
        XRCC_ERR(xrcc_dev_to_dev(dev),
                 "Configuring Xilinx VDMA channel failed: %d", err);
        return err;
    }

    // Map allocated buffers to DMA addresses
    dev->xt.dir         = DMA_DEV_TO_MEM;
    dev->xt.numf        = dev->height;
    dev->xt.sgl[0].size = dev->width * 2; // x2 because we have still packed YCbCr format to 8 bits
    dev->xt.sgl[0].icg  = 0;
    dev->xt.frame_size  = 1;

    dev->flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;

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
        dev->rxd = dma_dev->device_prep_interleaved_dma(dev->dma_chan,
                                                        &dev->xt,
                                                        dev->flags);
        rx_cookie = dev->rxd->tx_submit(dev->rxd);
        XRCC_DEBUG(xrcc_dev_to_dev(dev), "rx_cookie=%d", rx_cookie);
        if(dma_submit_error(rx_cookie))
        {
            XRCC_ERR(xrcc_dev_to_dev(dev), "Submit error, rx_cookie: %d",
                     rx_cookie);
            return -1;
        }
    }

    init_completion(&dev->rx_cmp);
    dev->rxd->callback = xrcc_cam_slave_rx_callback;
    dev->rxd->callback_param = &dev;

    dma_async_issue_pending(dev->dma_chan);

    {
        // start & wait for compeltion
        unsigned long rx_tmo = msecs_to_jiffies(3000);
        dma_cookie_t last_cookie, used_cookie;
        int i;

        rx_tmo = wait_for_completion_timeout(&dev->rx_cmp, rx_tmo);

        dev->status = dma_async_is_tx_complete(dev->dma_chan, rx_cookie,
                                               &last_cookie, &used_cookie);

        for(i = 0; i < dev->frm_cnt; i++)
        {
            int k;
            printk("xrcc_cam_dma_config() buffer %d (phy_addr=0x%08x) data=", i,
                dev->phy_addr[i]);
            for(k = 0; k < 6; k++)
            {
                printk("%d ", dev->buf[i][k]);
            }
            printk("\n");
        }

        XRCC_DEBUG(xrcc_dev_to_dev(dev), "status=%d last_cookie=%d used_cookie=%d",
                   dev->status, last_cookie, used_cookie);
    }

    return 0;
}

static int xrcc_cam_v4l2_cleanup(xrcc_cam_dev_t *dev)
{
    XRCC_DEBUG(xrcc_dev_to_dev(dev), "About to remove V4L2");
    if(dev->v4l2_dev)
    {
        v4l2_device_unregister(dev->v4l2_dev);
        kfree(dev->v4l2_dev);
        dev->v4l2_dev = NULL;
    }

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "About to remove Media device");
    if(dev->media_dev)
    {
        media_device_unregister(dev->media_dev);
        media_device_cleanup(dev->media_dev);
        kfree(dev->media_dev);
        dev->media_dev = NULL;
    }

    XRCC_DEBUG(xrcc_dev_to_dev(dev), "Cleaned!");
    return 0;
}

static int xrcc_cam_cleanup(xrcc_cam_dev_t *dev)
{
    if(!dev)
    {
        return 0;
    }

//    xrcc_cam_v4l2_cleanup(dev);

    if(dev->v4l2_dev)
    {
        v4l2_device_unregister(dev->v4l2_dev);
        kfree(dev->v4l2_dev);
        dev->v4l2_dev = NULL;
    }
    if(dev->media_dev)
    {
        media_device_unregister(dev->media_dev);
        media_device_cleanup(dev->media_dev);
        kfree(dev->media_dev);
        dev->media_dev = NULL;
    }

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

    dev->media_dev = kmalloc(sizeof(struct media_device), GFP_KERNEL);
    if(!dev->media_dev)
    {
        XRCC_ERR(xrcc_dev_to_dev(dev),
                "Can not allocate memory for media device");
        return -ENOMEM;
    }
    dev->media_dev->dev = xrcc_dev_to_dev(dev);
    strlcpy(dev->media_dev->model, "RC Car Control Video Device",
            sizeof(dev->media_dev->model));
    dev->media_dev->hw_revision = 0;

    dev->v4l2_dev = kmalloc(sizeof(struct v4l2_device), GFP_KERNEL);
    if(!dev->v4l2_dev)
    {
        XRCC_ERR(xrcc_dev_to_dev(dev),
                 "Can not allocate memory for V4L2 device");
        return -ENOMEM;
    }

    dev->v4l2_dev->mdev = dev->media_dev;

    retVal = v4l2_device_register(xrcc_dev_to_dev(dev), dev->v4l2_dev);
    if(retVal < 0)
    {
        XRCC_ERR(xrcc_dev_to_dev(dev), "V4L2 device registraion failed (%d)",
                 retVal);
        media_device_cleanup(dev->media_dev);
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

    err = xrcc_cam_dma_config(xrcc_dev);
    if(err)
    {
        XRCC_ERR(xrcc_dev_to_dev(xrcc_dev), "Problem configuring the DMA");
        goto probe_err_exit;
    }

//    err = xrcc_cam_v4l2_init(xrcc_dev);
//    if(err)
//    {
//        XRCC_ERR(xrcc_dev_to_dev(xrcc_dev), "Problem setting up V4L2 device");
//        goto probe_err_exit;
//    }

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
