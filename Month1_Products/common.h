#ifndef COMMON_H_
#define COMMON_H_

#define PROJ_NAME "virmouse"

#define PKN(fmt, args...) \
    printk(KERN_NOTICE fmt, ##args)

#define PKW(fmt, args...) \
    printk(KERN_WARNING fmt, ##args)

#include <linux/module.h>
#include <linux/init.h>

// For sysnode
int __init sysnode_register(void);
void sysnode_unregister(void);

// For virtual mouse
int __init vm_dev_register(void);
void vm_dev_unregister(void);

int translate(const char *command);

#endif //COMMON_H_