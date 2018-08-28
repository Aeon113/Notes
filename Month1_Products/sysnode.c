#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/device.h>

// For device
#define sysnodename "vm_sysnode";
static device* sysdev = NULL;

// For device
static int __init sysfolder_register(void) {
    PKN("%s: %s started...", PROJ_NAME, __PRETTY_FUNCTION__);

    // Allocate root device
    sysdev = root_device_register(device_name);
    if(NULL == sysdev) {
        PKW("%s: root_device_register() failed, terminating...",
        PROJ_NAME);
        return -ENOMEM;
    }

    // Now, there should be "/sys/devices/vm_sysnode/".
    PKN("%s: device allocated, at /sys/devices/%s/, device pointer: %p.",
        PROJ_NAME, sysnodename, sysdev);
    return 0;
}

static void __exit sysfolder_unregister(void) {
    PKN("%s: %s started...", PROJ_NAME, __PRETTY_FUNCTION__);
    root_device_unregister(dev_ptr);
    PKN("%s: device unregistered, %s() terminating...",
     PROJ_NAME, __func__);
}



// For attributes
static device_attribute sd_attr;

static static ssize_t do_write(
    struct device *dev,
    struct device_attribute *attr,
    const char *buf,
    size_t count);
static void __init set_sd_attr(void) {
    sd_attr.attr.name = "mattr";
    sd_attr.attr.mode = 0222; // Write-only
    sd_attr.attr.show = NULL;
    sd_attr.attr.store = do_write;
}

static int __init mattr_register(void) {
    int result = 0;
    PKN("%s: %s started...", VIRMOUSE, __PRETTY_FUNCTION__);
    set_sd_attr();
    
    result = sysfs_create_file(&sysdev->kobj, &sd_attr.attr);
    if(result)
        PKW("%s: %s(): Cannot add the file.", PROJ_NAME, __func__);
    else
        PKN("%s: %s(): file created.", PROJ_NAME, __func__);
    return result;
}

static int __exit mattr_unregister(void) {
    PKN("%s: %s started...", PROJ_NAME, __PRETTY_FUNCTION__);
    sysfs_remove_file(&sysdev->kobj, &sd_attr.attr);
    PKN("%s: %s() terminating...", PROJ_NAME, __func__);
}

static static ssize_t do_write(
    struct device *dev,
    struct device_attribute *attr,
    const char *buf,
    size_t count) {
        //TODO: Not implemented yet.
        
    }

// Combination
int __init sysnode_register(void) {
    int result;
    PKN("%s: %s started...", PROJ_NAME, __PRETTY_FUNCTION__);
    result = sysfolder_register();
    if(result)
        goto sysfolder_registration_failed;
    
    result = mattr_register();
    if(result)
        goto mattr_registration_failed;
    
    PKN("%s: %s(): sysnode registration success.", PROJ_NAME, __func__);
    goto sysnode_registration_final;
    
mattr_registration_failed:
    PKW("%s: %s(): mattr cannot be registered, result is %d, unregistering sysfolder...", PROJ_NAME, __func__, result);
    sysfolder_unregister();
sysfolder_registration_failed:
    PKW("%s: %s(): sysfolder cannot be registered, result is %d.", PROJ_NAME, __func__, result);

sysnode_registration_final:
    PKN("%s: %s() terminating...", PROJ_NAME, __func__);
    return result;
}

void __exit sysnode_unregister(void) {
    PKN("%s: %s started...", PROJ_NAME, __PRETTY_FUNCTION__);
    mattr_unregister();
    sysfolder_unregister();
    PKN("%s: %s() terminating...",
        PROJ_NAME, __func__);
}