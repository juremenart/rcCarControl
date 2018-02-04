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

typedef struct xrcc_cam_dev_s {
    uint32_t           frm_cnt; // number of frame buffers
    uint32_t           width, height;

    struct dma_chan   *dma_chan;
    struct device     *dev;

} xrcc_cam_dev_t;

static struct device *xrcc_dev_to_dev(xrcc_cam_dev_t *dev)
{
    return dev->dev;
}

static int xrcc_cam_add_channel(struct dma_chan *chan,
                                xrcc_cam_dev_t *dev)
{
    dev->dma_chan = chan;

    dev_info(xrcc_dev_to_dev(dev), "Adding DMA channel: %s",
             dma_chan_name(chan));
    return 0;
}

static int xrcc_cam_probe(struct platform_device *pdev)
{
    xrcc_cam_dev_t *xrcc_dev;

    struct dma_chan *rx_chan;
    int err;
    uint32_t frm_cnt = 3; // TODO: Get that info from rx_chan!

    dev_info(&pdev->dev, "Probe entry");

    // TODO: We are requesting axivdma1 (we define 2 in DTS eventhough
    // in HW we had only one connected. This should be fixed but when I
    // tried it I had problems and reverted to original DTS settings to
    // at least be able to request rx_chan successfully.
    rx_chan = dma_request_chan(&pdev->dev, "axivdma1");
    if (IS_ERR(rx_chan) || (rx_chan == NULL)) {
        err = PTR_ERR(rx_chan);
        dev_err(&pdev->dev, "RX DMA channel not found, check dma-names in DT");
        return err;
    }

    // Create internal structure
    xrcc_dev = devm_kzalloc(&pdev->dev, sizeof(xrcc_cam_dev_t), GFP_ATOMIC);
    if(xrcc_dev == NULL)
    {
        dev_err(&pdev->dev, "Can not allocate memory for private structure");
        return -ENOMEM;
    }

    xrcc_dev->dev = &pdev->dev;
    dev_set_drvdata(&pdev->dev, xrcc_dev);

    // TODO: Get the number of buffers from DMA driver 'somehow'
//    dev_info(&pdev->dev, "Reading num-fstores");
//    err = of_property_read_u32(rx_chan->dev->device.of_node,
//                               "xlnx,num-fstores", &frm_cnt);
//    if(err < 0)
//    {
//        dev_err(&pdev->dev, "Number of frame buffers not found, check num-fstores in DT");
//        return err;
//    }

    xrcc_dev->frm_cnt = frm_cnt;

    err = xrcc_cam_add_channel(rx_chan, xrcc_dev);
    if (err) {
        dev_err(&pdev->dev, "Unable to add channels\n");
        goto free_rx;
    }

    dev_info(xrcc_dev_to_dev(xrcc_dev), "RCC Camera Driver Probed");

    return 0;

free_rx:
    dma_release_channel(rx_chan);

    return err;
}

static int xrcc_cam_remove(struct platform_device *pdev)
{
    xrcc_cam_dev_t *xrcc_dev = dev_get_drvdata(&pdev->dev);

    if(xrcc_dev->dma_chan)
    {
        dev_info(xrcc_dev_to_dev(xrcc_dev), "Releasing channel %s",
                 dma_chan_name(xrcc_dev->dma_chan));
        dma_release_channel(xrcc_dev->dma_chan);
    }
    else
    {
        dev_err(xrcc_dev_to_dev(xrcc_dev), "DMA channel not set, a problem?!");
    }

    dev_info(xrcc_dev_to_dev(xrcc_dev), "RCC Camera Driver Removed");

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
