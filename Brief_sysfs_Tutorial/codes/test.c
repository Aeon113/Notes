#include <linux/printk.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/module.h>

#define PKN(fmt, args...) \
    printk(KERN_NOTICE fmt, ##args)

#define PKW(fmt, args...) \
    printk(KERN_WARNING fmt, ##args)

static const char * device_name = "my_test";

static struct device *dev_ptr = NULL;

// For operations
static const char *to_read = "Hey!";
static const int to_read_size = 5;
static ssize_t do_read(
    struct device *dev,
    struct device_attribute *attr,
    char *buf) {
        memcpy(buf, to_read, to_read_size);
        return 5;
    }
static ssize_t do_write(
    struct device *dev,
    struct device_attribute *attr,
    const char *buf,
    size_t count) {
        PKN("%s: %s", device_name, buf);
        return count;
    }

// For files
static struct device_attribute dattr;
static int __init create_file(void) {
    int result = 0;
    PKN("%s: %s started...", device_name, __PRETTY_FUNCTION__);

    dattr.attr.name = "my_test_attr";
    dattr.attr.mode = 0666;
    dattr.show = do_read;
    dattr.store = do_write;

    result = sysfs_create_file(&dev_ptr->kobj, &dattr.attr);
    if(result)
        PKW("%s: %s(): Cannot add the file.", device_name, __func__);
    else
        PKN("%s: %s(): file created.", device_name, __func__);
    return result;
}

static void __exit remove_file(void) {
    PKN("%s: %s started...", device_name, __PRETTY_FUNCTION__);
    sysfs_remove_file(&dev_ptr->kobj, &dattr.attr);
    PKN("%s: %s() terminating...", device_name, __func__);
}

// For device
static int __init test_device_init(void) {
    PKN("%s: %s started...", device_name, __PRETTY_FUNCTION__);

    // Allocate root device
    dev_ptr = root_device_register(device_name);
    if(NULL == dev_ptr) {
        PKW("%s: root_device_register() failed, terminating...",
        device_name);
        return -ENOMEM;
    }

    // Now, there should be "/sys/devices/my_test".
    PKN("%s: device allocated, at /sys/devices/%s, device pointer: %p.",
        device_name, device_name, dev_ptr);
    return 0;
}

static void __exit test_device_remove(void) {
    PKN("%s: %s started...", device_name, __PRETTY_FUNCTION__);
    root_device_unregister(dev_ptr);
    PKN("%s: device unregistered, %s() terminating...",
     device_name, __func__);
}

static int __init test_init(void) {
    int result;
    PKN("%s: %s started...", device_name, __PRETTY_FUNCTION__);

    // Create device.
    result = test_device_init();
    if(result) {
        PKW("%s: %s(): Cannot create device, with return value as %d, terminating...",
            device_name, __func__, result);
        return -EACCES;
    }

    // Create files.
    result = create_file();
    if(result) {
        PKW("%s: %s(): Cannot create the file, with return value as %d, unregistering dev...",
            device_name, __func__, result);
        root_device_unregister(dev_ptr);
        return -EACCES;    
    }

    PKN("%s: %s() terminating...",
        device_name, __func__);
    return 0;
}

static void __exit test_remove(void) {
    PKN("%s: %s started...", device_name, __PRETTY_FUNCTION__);
    remove_file();
    test_device_remove();
    PKN("%s: %s() terminating...",
        device_name, __func__);
}


MODULE_DESCRIPTION("Brief sysfs Tutorial by Ben Marks at https://www.cs.swarthmore.edu/~newhall/sysfstutorial.pdf");
MODULE_AUTHOR("Me");
MODULE_LICENSE("Dual BSD/GPL");
module_init(test_init);
module_exit(test_remove);