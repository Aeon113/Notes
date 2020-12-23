Maybe TODO:    
elevator_init_mq()
blk_register_queue()
blk_get_queue()
bdi_register()
bdi_unregister()



Simple Notes:

`struct blk_mq_ctx`: Software staging queue, where IO scheduler works on. Usually allocated for every-CPU or every-node. After processed by software staging queues, the requests will be placed at the hardware queues.

`struct blk_mq_hw_ctx`: Hardware queue. Used by the device drivers to map the device submission queues (most modern NVMe devices owns more than one submission queues, the kernel is permitted to send IO requests simultaneously on these queues). The number of hardware queues depends on the number of hardware contexts supported by the hardware and its device driver, but it will not be more than the number of cores of the system. There is no reordering at this stage, and each software queue has a set of hardware queues to send requests for.

驱动工作步骤：
blk-mq:   
初始化:
1. 分配并设置`struct blk_mq_tag_set`。通常`kmalloc()`或静态分配；然后设置其中的fields。
2. 对第1步设置好的`strut blk_mq_tag_set`调用`blk_mq_alloc_tag_set()`，其将检查第1步设置的fields，并注册此tagset。
3. 在第2步成功后，调用`blk_mq_init_queue()`，分配并获取`struct request_queue`。
4. 设置第3步中`struct request_queue`的`queuedata`。它将被驱动使用。
5. 设置`struct request_queue`的其他fields，比如trim支持情况、blocksize、是否为机械磁盘设备等等。
6. 继续其他初始化流程。

销毁:     
1. 调用`blk_mq_free_tag_set()`注销tagset。
2. 调用`blk_cleanup_queue()`注销并释放request queue。

---------------

# struct request_queue 与 blk-mq
在文章0001`struct gendisk`中，我们曾介绍过一个块设备初始化所需的步骤:

> + 调用register_blkdev()，注册一个块设备设备号，或者动态获取一个新的块设备设备号    
> + alloc gendisk (`alloc_disk()`或`alloc_disk_node()`)   
> + 设置gendisk内的各个fields，包括设备号、fops、private data、capacity等等等等   
> + 分配并设置一个request queue   
> + 将此request queue挂到刚才分配的gendisk上   
> + add gendisk (`add_disk()`)   

整个过程主要牵涉到了4个结构体的初始化:    
+ `struct gendisk`: 用于指代整个磁盘(也包括md/dm等形成的虚拟磁盘)。
+ `struct hd_struct`: 用于负责IO统计和指代分区。
+ `struct block_device`: 负责将块设备(包括磁盘和分区)与VFS以及整个"device hierarchy"连接起来。
+ `struct request_queue`: 负责具体的IO操作。

前3篇文章分别研究了`struct gendisk`、`struct hd_struct`和`struct block_device`，这里将简单介绍`struct request_queue`，并研究其在blk-mq模式下的工作原理。    
本文参考Linux kernel 5.8.9的代码。    

## 1. `struct request_queue`
