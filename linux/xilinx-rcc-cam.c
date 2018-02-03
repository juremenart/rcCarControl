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

typedef struct xrcc_vdma_chan_s {
    struct dma_chan *chan;
    struct list_head list;
} xrcc_vdma_chan_t;

xrcc_vdma_chan_t xrcc_vdma_channels;

struct platform_device *xrcc_vdma_dev = NULL;

static int xrcc_cam_add_channel(struct dma_chan *chan)
{
    xrcc_vdma_chan_t *new_chan;

    dev_info(&xrcc_vdma_dev->dev,
             "xrcc_cam_add_channel() adding: %s", dma_chan_name(chan));

    new_chan = kmalloc(sizeof(xrcc_vdma_chan_t), GFP_KERNEL);
    if(!new_chan)
    {
        return -ENOMEM;
    }

    new_chan->chan = chan;
    dev_info(&xrcc_vdma_dev->dev, "new xrcc_vdma_chan_t structure initialized");

    INIT_LIST_HEAD(&new_chan->list);

    dev_info(&xrcc_vdma_dev->dev, "new xrcc_vdma_chan_t list head initialized, adding it to structure");
    list_add_tail(&(new_chan->list), &(xrcc_vdma_channels.list));

    dev_info(&xrcc_vdma_dev->dev, "new xrcc_vdma_chan_t added successfully");
    return 0;
}

static int xrcc_cam_probe(struct platform_device *pdev)
{
    struct dma_chan *rx_chan;
    int err;

    INIT_LIST_HEAD(&xrcc_vdma_channels.list);

    xrcc_vdma_dev = pdev;

    dev_info(&pdev->dev, "LIST_HEAD called, let's add new channel");
    rx_chan = dma_request_slave_channel(&pdev->dev, "axivdma0");
    if (IS_ERR(rx_chan)) {
        err = PTR_ERR(rx_chan);
        pr_err("xilinx_dmatest: No Rx channel\n");
        return err;
    }

    err = xrcc_cam_add_channel(rx_chan);
    if (err) {
        pr_err("xilinx_dmatest: Unable to add channels\n");
        goto free_rx;
    }

    dev_info(&pdev->dev, "RCC Camera Driver Probed");

    return 0;

free_rx:
    dma_release_channel(rx_chan);

    return err;
}

static int xrcc_cam_remove(struct platform_device *pdev)
{
    xrcc_vdma_chan_t *vdma_chan;

    list_for_each_entry(vdma_chan, &xrcc_vdma_channels.list, list) {
        list_del(&vdma_chan->list);
        dev_info(&xrcc_vdma_dev->dev, "xrcc_cam: Releasing channel %s\n",
                dma_chan_name(vdma_chan->chan));
        dma_release_channel(vdma_chan->chan);

        kfree(vdma_chan);
    }

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

static int __init xrcc_cam_init(void)
{
    return platform_driver_register(&xrcc_cam_driver);

}
late_initcall(xrcc_cam_init);

static void __exit xrcc_cam_exit(void)
{
    platform_driver_unregister(&xrcc_cam_driver);
}
module_exit(xrcc_cam_exit)

MODULE_AUTHOR("Jure Menart (juremenart@gmail.com)");
MODULE_DESCRIPTION("RC Car Control Camera Driver");
MODULE_LICENSE("GPL v2");
