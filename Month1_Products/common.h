#ifndef COMMON_H_
#define COMMON_H_

#define PROJ_NAME "virmouse"

#define PKN(fmt, args...) \
    printk(KERN_NOTICE fmt, ##args)

#define PKW(fmt, args...) \
    printk(KERN_WARNING fmt, ##args)

#include <linux/module.h>
#include <linux/init.h>

int __init sysnode_register(void);
void __exit sysnode_unregister(void)

#endif //COMMON_H_