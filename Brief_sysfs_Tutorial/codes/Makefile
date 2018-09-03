# 更改modulename为需要生成的模块名
# 更改 "file1.o", "file2.o", ... 为 需要的文件列表

KVER=$(shell uname -r)
PWD=$(shell pwd)
KBUILD_DIR=/lib/modules/$(KVER)/build

# Kernel modules
TARGET_MODULE:=sysfs_test_2
obj-m := $(TARGET_MODULE).o
# 这里，file1.c会生成file1.o, file2.c会生成file2.o，以此类推。
# $(TARGET_MODULE)-objs := file1.o file2.o file3.o
$(TARGET_MODULE)-objs := test.o

# 将下列宏打开后得到的模块会包含调试信息
#EXTRA_CFLAGS=-g -O0

all:
	$(MAKE) -C $(KBUILD_DIR) M=$(PWD) modules
clean: 
	$(MAKE) -C $(KBUILD_DIR) M=$(PWD) clean
load:
	insmod $(TARGET_MODULE).ko
unload:
	rmmod $(TARGET_MODULE).ko