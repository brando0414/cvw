/*
    This driver will locate a predetermined place in memory to do frame buffering.
    It uses the Direct Rendering Manager (DRM) rather than fbdev since fbdev is deprecated.
    The ioremap function looks for a physical place in memory rather than the indexed kernel memory.
    However, we will be using fb_helper to emulate the framebuffer and access fbcon for console.

*/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/drm/drm_drv.h>
#include <linux/drm/drm_device.h>
#include <linux/drm/drm_modeset_helper_vtables.h>
#include <linux/drm/drm_fb_helper.h>
#include <linux/drm/drm_framebuffer.h>
#include <linux/io.h>
#include <linux/drm/drm_fb_helper.h>//fb emulation
#include <linux/drm/drm_framebuffer.h>

/* Define video memory region (adjust as needed) */
#define VIDEO_MEM_START  0xA0000000  // Replace with actual framebuffer address
#define VIDEO_MEM_SIZE   (640 * 480 * 3)  // Example: 640x480 @ 24bpp

struct custom_drm_device {
    struct drm_device drm;
    struct drm_fb_helper fb_helper;//fb emulation
    struct drm_framebuffer *fb;//fb emulation
    void __iomem *video_mem;
};

/* DRM Driver Functions */
static int custom_drm_open(struct drm_device *dev, struct drm_file *file)
{
    return 0;
}

static struct drm_driver custom_drm_driver = {
    .driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_ATOMIC,
    .open = custom_drm_open,
    .name = "custom_drm",
    .desc = "Custom DRM/KMS Driver",
    .date = "20250213",
    .major = 1,
    .minor = 0,
};

/* Modesetting Functions */
static int custom_drm_modeset_init(struct drm_device *drm)
{
    /* Set up mode configurations */
    return 0;
}

/* Probe Function (Called When Device is Detected) */
static int custom_drm_probe(struct platform_device *pdev)
{
    struct custom_drm_device *custom_dev;
    struct drm_device *drm;
    int ret;

    custom_dev = devm_kzalloc(&pdev->dev, sizeof(*custom_dev), GFP_KERNEL);
    if (!custom_dev)
        return -ENOMEM;

    drm = &custom_dev->drm;
    ret = drm_dev_init(drm, &custom_drm_driver, &pdev->dev);
    if (ret)
        return ret;

    /* Map Video Memory */
    custom_dev->video_mem = ioremap(VIDEO_MEM_START, VIDEO_MEM_SIZE);
    if (!custom_dev->video_mem) {
        drm_dev_put(drm);
        return -ENOMEM;
    }

    /* Register DRM device */
    ret = drm_dev_register(drm, 0);
    if (ret) {
        iounmap(custom_dev->video_mem);
        drm_dev_put(drm);
        return ret;
    }

    /* Initialize modesetting */
    ret = custom_drm_modeset_init(drm);
    if (ret) {
        drm_dev_unregister(drm);
        iounmap(custom_dev->video_mem);
        drm_dev_put(drm);
        return ret;
    }

    /* Initialize framebuffer console */
    ret = custom_drm_fbdev_init(drm);
    if (ret) {
        drm_dev_unregister(drm);
        iounmap(custom_dev->video_mem);
        drm_dev_put(drm);
        return ret;
    }


    platform_set_drvdata(pdev, custom_dev);
    pr_info("Custom DRM driver initialized!(w/framebuffer emulation)\n");
    return 0;
}

//fb for linux console, emulated
static int custom_drm_fbdev_init(struct drm_device *drm)
{
    struct custom_drm_device *custom_dev = container_of(drm, struct custom_drm_device, drm);
    struct drm_fb_helper *fb_helper = &custom_dev->fb_helper;
    int ret;

    drm_fb_helper_prepare(drm, fb_helper, &drm_fbdev_generic_setup);
    ret = drm_fb_helper_init(drm, fb_helper);
    if (ret)
        return ret;

    ret = drm_fb_helper_initial_config(fb_helper, 32);  // 32-bit depth
    if (ret)
        return ret;

    return 0;
}


/* Remove Function (Called When Driver is Unloaded) */
static int custom_drm_remove(struct platform_device *pdev)
{
    struct custom_drm_device *custom_dev = platform_get_drvdata(pdev);
    drm_dev_unregister(&custom_dev->drm);
    iounmap(custom_dev->video_mem);
    drm_dev_put(&custom_dev->drm);
    drm_fb_helper_fini(&custom_dev->fb_helper);//fb emulation
    return 0;
}

/* Platform Driver Definition */
static struct platform_driver custom_drm_platform_driver = {
    .driver = {
        .name = "custom_drm",
    },
    .probe = custom_drm_probe,
    .remove = custom_drm_remove,
};

module_platform_driver(custom_drm_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Brandon Collings");
MODULE_DESCRIPTION("Custom DRM/KMS Driver Using ioremap");
