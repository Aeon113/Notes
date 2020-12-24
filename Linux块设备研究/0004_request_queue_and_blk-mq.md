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
3. 在第2步成功后，将注册好的tagset传入`blk_mq_init_queue()`，分配并获取`struct request_queue`。
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
*这里假设你已经了解了request queue, `bio`, `request`以及elevator的概念。网络上有很多相关的文章。*   

在早期，大多数计算机使用的存储设备都是HDD。HDD的磁头寻道相当耗时，因此随机访问的效率非常低。因此，传达给驱动的IO请求不能简单的依据时间上的FIFO来进行，而是必须将一段时间内的IO请求以地址顺序排序，尽量缩短寻道的次数和磁头移动的距离，从而使得IO效率最大化。为了达成这个目的，kernel引入了request queue、request和evevator。   

通常，`bio`在发出后，不会被立刻被送给驱动，而是会被放入request queue中暂存。   
在大多数情况下，驱动会为request queue指定一个elevator。这个elevator会对request queue中暂存的bio进行排序、merge等操作，将一个或多个`bio`组合成一个`request`(每个`request`中的`bio`都是连续的)。然后，在合适的时间点，将各个`request`发送给驱动。    
当然，除了提高IO效率外, request queue和elevator还能起到减少IO性能波动的作用。     
(还有一种类型的request queue，它没有这些复杂的功能，只会对暂存的bio做简单的处理，我们会在下一篇文章中探讨。)     

## 2. blk-mq
最早，与块设备层最常打交道的硬件只有HDD。虽然HDD很慢，但其向上暴露的接口也非常简单。 驱动在收到来自request queue的IO请求后，只需要将它翻译成硬件能够识别的命令，填入硬件的command queue (通常就是一段特定的内存空间)，然后等待硬件处理即可。     
但随着SSD的出现，以及NVMe协议的发展，块设备越来越快，结构也越来越复杂。IO的latency从十毫秒级降到了百微妙级，最大读写带宽也由10MB级提升到了GB级。并且，一个NVMe磁盘可以拥有多个command queues，同时接收IO命令。       
而request queue的结构一直没有变化，致使它从性能提升的机制变成了性能瓶颈。   
传统的request queue只有一个队列 (blk-sq, single-queue)，这个队列也只有一个锁。当有大量的进程向同一个磁盘发送请求时，竞争带来的CPU性能损耗将非常严重。同时，如果当前线程所在CPU和request queue处于不同的NUMA nodes时，性能损耗也变得无法忽视。    
因此，kernel 3.13引入了blk-mq (multi-queue)机制。并最终在v5.0彻底移除了旧的blk-sq。     

新的blk-mq机制中，`struct request_queue`拥有了多个队列。这些队列主要分为2大类: software staging queues (软件队列，`struct blk_mq_ctx`) 和 hardware dispatch queues (硬件队列，`struct blk_mq_hw_ctx`)。   
每一个CPU或NUMA node都拥有一个软件队列。所有的`bio`都会首先被发送给当前进程对应的软件队列。这个过程的锁损耗非常少，甚至可以完全没有锁损耗。    
而每一个软件队列都将拥有一个elevator (IO scheduler)。Elevators的所用与过去一样，只是它们不能跨软件队列工作，也就是说，一个软件队列上的elevator只能处理其所在软件队列上的`bio`。