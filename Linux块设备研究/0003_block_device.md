TODO: `bd_get()`, `blkdev_get()`、`blkdev_put()`。

# `struct block_device`
每个`struct hd_struct`都拥有一个`struct block_device`，它负责将`struct hd_struct`与其他部分，比如bdi和文件系统，链接起来，它们通过`struct block_device`来确定一个具体的块设备/分区。    

``` c
struct block_device {
	dev_t			bd_dev;  /* not a kdev_t - it's a search key */
	int			bd_openers;
	struct inode *		bd_inode;	/* will die */
	struct super_block *	bd_super;
	struct mutex		bd_mutex;	/* open/close mutex */
	void *			bd_claiming;
	void *			bd_holder;
	int			bd_holders;
	bool			bd_write_holder;
#ifdef CONFIG_SYSFS
	struct list_head	bd_holder_disks;
#endif
	struct block_device *	bd_contains;
	unsigned		bd_block_size;
	u8			bd_partno;
	struct hd_struct *	bd_part;
	/* number of times partitions within this device have been opened. */
	unsigned		bd_part_count;
	int			bd_invalidated;
	struct gendisk *	bd_disk;
	struct request_queue *  bd_queue;
	struct backing_dev_info *bd_bdi;
	struct list_head	bd_list;
	/*
	 * Private data.  You must have bd_claim'ed the block_device
	 * to use this.  NOTE:  bd_claim allows an owner to claim
	 * the same device multiple times, the owner must take special
	 * care to not mess up bd_private for that case.
	 */
	unsigned long		bd_private;

	/* The counter of freeze processes */
	int			bd_fsfreeze_count;
	/* Mutex for freeze */
	struct mutex		bd_fsfreeze_mutex;
} __randomize_layout;
```

| Field | Comment |
|------:|:--------|
| bd_dev | 代码注释里说这里其实并非是设备号，而是一个search key。具体怎么用的，没有查到。 |
| bd_openers | 统计当前已打开此设备的用户数量。一般每次执行`blkdev_get()`，此值会加1；每次执行`blkdev_put()`时减1。在这个值增加到1或减少到0时这两个函数会做一些初始化和销毁操作，这种情况只会发生在块设备的注册和销毁逻辑中。 |
| bd_inode | 当前分区/块设备对应的inode。这个field被注释成"will die"至少有15年了，但是仍然在被使用。 |
| bd_super | 存放当前分区/块设备上的super block信息。如果没有文件系统，那这里应该就是NULL。 |
| bd_mutex | 这个没什么好解释的。 |
| bd_claiming / bd_holder | 这两个量被用于exclusively open/close和块设备初始化销毁(主要就是指`blkdev_get()`与`blkdev_put()`)。注意这两个量的类型都是`void *`，即可以被赋成任意类型。具体操作可以参考下方的`blkdev_get()`和`blkdev_put()`的简介。 |
| bd_write_holder | 在当前设备处于被exclusively open的状态时用于指示opener是否在fmode中要求并获取了write权限。 |
| bd_holder_disks | md/dm相关。用于保存当前分区/设备的holder。 |
| bd_contains | 指向"contain"当前块设备的块设备。如果当前`struct block_device`是一个完整的块设备，那么它的`bd_contains`将指向它自己；而如果当前`struct block_device`是一个分区，那么`bd_contains`将指向它所处的块设备对应的`struct block_device`。 |
| bd_block_size | 指当前设备的物理块大小，通常hdd为512，ssd为4096。 |
| bd_partno | 当前`struct block_device`对应的`struct hd_struct`在`struct gendisk`的分区表中的index。0代表此`struct block_device`代指整个磁盘，其他指代指分区。 |
| bd_part | 指向当前`struct block_device`对应的`struct hd_struct`。 |
| 