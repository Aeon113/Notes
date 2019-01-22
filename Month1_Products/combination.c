#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "common.h"


static int __init virmouse_register(void) {
    int result;
    PKN("%s: %s: started...", PROJ_NAME, __PRETTY_FUNCTION__);

    result = vm_dev_register();
    if(result) {
        PKW("%s: %s(): Cannot register the input device, "
            "with return value as %d, terminating...",
            PROJ_NAME, __func__, result);
            goto vm_registration_failed;
    }

    result = sysnode_register();
    if(result) {
        PKW("%s: %s(): Cannot register the sysnode, "
            "with return value as %d, unregister the input device "
            "and terminating...",
            PROJ_NAME, __func__, result);
            goto sysnode_registration_failed;
    }

    PKN("%s: %s() success, terminating...",
        PROJ_NAME, __func__);
    goto virmouse_registration_final;

sysnode_registration_failed:
    vm_dev_unregister();

vm_registration_failed:
virmouse_registration_final:
    return result;
}

static void __exit virmouse_unregister(void) {
    PKN("%s: %s: started...", PROJ_NAME, __PRETTY_FUNCTION__);
    sysnode_unregister();
    vm_dev_unregister();
    PKN("%s: %s() terminating...", PROJ_NAME, __func__);
}

MODULE_AUTHOR("Hui");
MODULE_DESCRIPTION("Virtual Mouse Driver");
MODULE_LICENSE("GPL");
module_init(virmouse_register);
module_exit(virmouse_unregister);