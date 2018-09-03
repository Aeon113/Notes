#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include "common.h"

// For virtual mouse input device.
static struct input_dev *vms_dev;

static void __init set_vms_dev_fields(void) {
    vms_dev->name = "Virtual Mouse";
    vms_dev->id.bustype = BUS_VIRTUAL;
    
    set_bit(EV_KEY, vms_dev->evbit);
    set_bit(EV_REL, vms_dev->evbit);
    set_bit(EV_REP, vms_dev->evbit);
    set_bit(BTN_MOUSE, vms_dev->keybit);
    set_bit(BTN_LEFT, vms_dev->keybit);
    set_bit(BTN_RIGHT, vms_dev->keybit);
    set_bit(BTN_MIDDLE, vms_dev->keybit);
    set_bit(REL_X, vms_dev->relbit);
    set_bit(REL_Y, vms_dev->relbit);
}

int __init vm_dev_register(void) {
    int result;
    PKN("%s: %s started...", PROJ_NAME, __PRETTY_FUNCTION__);

    // Allocate input device
    vms_dev = input_allocate_device();
    if(NULL == vms_dev) {
        PKW("%s: %s(): Cannot allocate input device, terminating...", PROJ_NAME, __func__);
        return -ENOMEM;
    }

    set_vms_dev_fields();

    // register input device
    result = input_register_device(vms_dev);
    if(result) {
        PKW("%s: %s(): input_register_device() failed, with return value as %d, "
            "free the device and terminating...", 
            PROJ_NAME, __func__, result);
        input_free_device(vms_dev);
    } else
        PKN("%s: %s(): input_register_device() success, terminating...",
            PROJ_NAME, __func__);
    
    return result;
}

void vm_dev_unregister (void) {
    PKN("%s: %s started...", PROJ_NAME, __PRETTY_FUNCTION__);
    input_unregister_device(vms_dev);
    PKN("%s: %s(): input_register_device() success, terminating...",
            PROJ_NAME, __func__);
}

// For event report and translation
static void left_click(void) {
    input_report_key(vms_dev, BTN_LEFT, 1);
    input_sync(vms_dev);
    input_report_key(vms_dev, BTN_LEFT, 0);
    input_sync(vms_dev);

    PKN("%s: LEFT CLICK.", PROJ_NAME);
}

static void right_click(void) {
    input_report_key(vms_dev, BTN_RIGHT, 1);
    input_sync(vms_dev);
    input_report_key(vms_dev, BTN_RIGHT, 0);
    input_sync(vms_dev);

    PKN("%s: RIGHT CLICK.", PROJ_NAME);
}

static void middle_click(void) {
    input_report_key(vms_dev, BTN_MIDDLE, 1);
    input_sync(vms_dev);
    input_report_key(vms_dev, BTN_MIDDLE, 0);
    input_sync(vms_dev);

    PKN("%s: MIDDLE CLICK.", PROJ_NAME);
}

static void rel_move(int x, int y) {
    input_report_rel(vms_dev, REL_X, x);
    input_report_rel(vms_dev, REL_Y, y);
    input_sync(vms_dev);

    PKN("%s: RELATIVE MOVE:\tx = %d\ty = %d.", PROJ_NAME, x, y);
}

int translate(const char *command) {
    int x, y;
    switch(command[0]) {
    case 'c':
    case 'C':
        // click
        switch(command[1]) {
        case 'l':
        case 'L':
            left_click();
            break;
        
        case 'r':
        case 'R':
            right_click();
            break;

        case 'm':
        case 'M':
            middle_click();
            break;

        default:
            goto wrong_command;
        }
    case 'm':
    case 'M':
        if(2 != sscanf(command + 1, "%d%d", &x, &y))
            goto wrong_command;
        rel_move(x, y);
        break;

    default:
        goto wrong_command;
    }

    return 0;

wrong_command:
    PKW("%s: %s(): Cannot recognize this command: %s", 
        PROJ_NAME, __func__, command);
    return -EBADF;
}