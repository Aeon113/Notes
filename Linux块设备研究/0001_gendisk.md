# 块设备与gendisk

`struct gendisk`在Linux kernel中用于表示一个独立的磁盘设备。基本上Linux中的每一个块设备都对应一个`struct gendisk`。这里以5.8.9的代码为例研究这个struct，以及它在整个块设备子系统中的作用。   

## 1. 定义
`struct gendisk`的定义在`include/linux/genhd.h`中:   

``` c
struct gendisk {
	/* major, first_minor and minors are input parameters only,
	 * don't use directly.  Use disk_devt() and disk_max_parts().
	 */
	int major;			/* major number of driver */
	int first_minor;
	int minors;                     /* maximum number of minors, =1 for
                                         * disks that can't be partitioned. */

	char disk_name[DISK_NAME_LEN];	/* name of major driver */

	unsigned short events;		/* supported events */
	unsigned short event_flags;	/* flags related to event processing */

	/* Array of pointers to partitions indexed by partno.
	 * Protected with matching bdev lock but stat and other
	 * non-critical accesses use RCU.  Always access through
	 * helpers.
	 */
	struct disk_part_tbl __rcu *part_tbl;
	struct hd_struct part0;

	const struct block_device_operations *fops;
	struct request_queue *queue;
	void *private_data;

	int flags;
	struct rw_semaphore lookup_sem;
	struct kobject *slave_dir;

	struct timer_rand_state *random;
	atomic_t sync_io;		/* RAID */
	struct disk_events *ev;
#ifdef  CONFIG_BLK_DEV_INTEGRITY
	struct kobject integrity_kobj;
#endif	/* CONFIG_BLK_DEV_INTEGRITY */
#if IS_ENABLED(CONFIG_CDROM)
	struct cdrom_device_info *cdi;
#endif
	int node_id;
	struct badblocks *bb;
	struct lockdep_map lockdep_map;
};
```

逐一介绍一下这个结构体的各个field:    

|  Field | Comment |
|-------:|:--------|
| major | 块设备的major设备号，可以通过`register_blkdev()`得到。 |
| first_minor | 通常，每有一个分区，都会占用一个此`gendisk`的minor。此项表示第一个可用的minor编号，通常是0。 |
| minors | 通常，每有一个分区，都会占用一个此`gendisk`的minor。此项表示此`gendisk`中可用的minors的数量，即分区数量。 |
| disk_name | 块设备名称，将被用于注册`/dev`和`/sys`下的各项。 |
| events | 事件相关，参考此表中的`ev`项。 |
| async_events | 事件相关，参考此表中的`ev`项。  |
| part_tbl | 分区表。注意此项必须使用相关的helpers函数来访问。此表中的每个分区都使用`struct hd_struct`来表示。此外，此表的第0项，即`part_tbl->part[0]`，指代的并非一个分区，而是整个当前的块设备，它的值即为此表中下一项的part0的地址。这个赋值的过程在`__alloc_disk_node()`中。 |
| part0 | 指代当前块设备的`struct hd_struct`。如此`part_tbl`条目所述，它的地址也被赋给了`part_tbl->part[0]`。 |
| fops | 块设备的file_operations。其中的回调由驱动提供。它比字符设备的`struct file_operations`简单许多，两者最大的不同是块设备的没有read/write相关的回调，因为块设备的IO相对较复杂，它主要需要一系列的机制协作来实现。 |
| queue | 请求队列。所有对此块设备的IO(`struct bio`和`struct request`)最终都会汇集到这个队列中，它们在被相应的函数处理后，发送给驱动注册在此queue中的回调。 |
| private_data | 驱动private data。 |
| flags | GenHD capability flags，用于设置此gendisk支持的特性。此项是bitset，值可以参考[Generic Block Device Capability](https://www.kernel.org/doc/html/v5.8/block/capability.html) |
| lookup_sem | 用于防止gendisk的remove/recreation/open等过程出现竞态。可见kernel commit [56c0908c855afbb2bdda17c15d2879949a091ad3](https://patchwork.kernel.org/patch/10242121/) |
| slave_dir | 没有找到相关文档。从代码中看，与device mapper有关。关于device mapper可以参考[Wikipedia](https://en.wikipedia.org/wiki/Device_mapper)，许多重要的块设备机制，例如LVM，都基于此功能实现。由于device mapper的存在，一个higher level的块设备可能由一个或多个lower level的块设备组成，这些lower level的块设备即为它的slaves。这里的slave_dir将存有指向这些slaves的kobjects的连接。 |
| random | 用于为kernel的random pool贡献entropy，kernel使用此部分生成随机数。与块设备本身的功能无较大关联。 |
| sync_io | 没有找到相关文档，查看代码，推测和md (multiple devices, 多用于RAID)有关。 |
| ev | 和`events`以及`async_events`一同，负责将事件推送到userland，具体事件包括attach、eject。具体可见kernel commit [77ea887e433ad8389d416826936c110fa7910f80](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=77ea887e433ad8389d416826936c110fa7910f80)，以及文章[Reworking disk events handling](https://lwn.net/Articles/423619/)。 |
| node_id | 当前`struct gendisk`以及分区表等信息所在的NUMA node，由驱动可以在alloc disk时指定(`alloc_disk_node()`和`alloc_disk()`)。 |
| bb | 没有找到相关文档。查询资料，判断与NVDIMM设备有关。此项负责提供为NVDIMM设备提供坏块表支持，可见[block: Add badblock management for gendisks](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=99e6608c9e7414ae4f2168df8bf8fae3eb49e41f)。除NVDIMM设备外，绝大多数块设备的坏块管理都不是kernel或驱动的职责，因此不会使用此项。 |
| lockdep_map | 与死锁监测有关。可见 [commit e319e1fbd9d42420ab6eec0bfd75eb9ad7ca63b1](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=e319e1fbd9d42420ab6eec0bfd75eb9ad7ca63b1)。 |

另外两个条件编译项相关的fields:   

| Field | Preprocessor Condition | Comment |
|-------:|--------|:--------|
| integrity_kobj | CONFIG_BLK_DEV_INTEGRITY | 与块设备数据完整性检查有关。可见[Data Integrity](https://www.kernel.org/doc/html/v5.8/block/data-integrity.html)和[Block layer: integrity checking and lots of partitions](https://lwn.net/Articles/290141/)。 |
| cdi | IS_ENABLED(CONFIG_CDROM) | 与cdrom相关。 |

_关于rcu (`__rcu`)相关的代码，可以参考kernel 文档[What is RCU? - "Read, Copy, Update"](https://www.kernel.org/doc/html/latest/RCU/whatisRCU.html)。_    

## 2. 使用
通常，驱动注册一个完整可用的块设备，需要依次执行如下几步(__这里不包括与request_queue相关的操作__):   
+ 调用register_blkdev()，注册一个块设备设备号，或者动态获取一个新的块设备设备号 
+ alloc gendisk (`alloc_disk()`或`alloc_disk_node()`)
+ 设置gendisk内的各个fields，包括设备号、fops、private data、request queue、capacity等等等等
+ add gendisk (`add_disk()`)

而卸载一个块设备则是:    
+ del gendisk (`del_gendisk()`)
+ 调用`unregister_blkdev()`，注销块设备

具体可见[Block Device Drivers](https://linux-kernel-labs.github.io/refs/heads/master/labs/block_device_drivers.html)。
我们这里关注一下gendisk相关的函数。    

## 3. `alloc_disk()`和`alloc_disk_node()`
`alloc_disk()`是`alloc_disk_node()`的一个简写形式:   
``` c
#define alloc_disk(minors) alloc_disk_node(minors, NUMA_NO_NODE)
```
可以看到，`alloc_disk()`就是不指定NUMA node的`alloc_disk_node()`。   

而`alloc_disk_node()`的实现如下:   

``` c
#define alloc_disk_node(minors, node_id)				\
({									\
	static struct lock_class_key __key;				\
	const char *__name;						\
	struct gendisk *__disk;						\
									\
	__name = "(gendisk_completion)"#minors"("#node_id")";		\
									\
	__disk = __alloc_disk_node(minors, node_id);			\
									\
	if (__disk)							\
		lockdep_init_map(&__disk->lockdep_map, __name, &__key, 0); \
									\
	__disk;								\
})
```
这个函数主要做2件事情:   
+ 调用`__alloc_disk_node()`，获取`struct gendisk`
+ 利用`__name`和`__key`初始化`struct gendisk`的`lockdep_map`

由此可知，主要的逻辑在`__alloc_disk_node()`中:    

``` c
struct gendisk *__alloc_disk_node(int minors, int node_id)
{
	struct gendisk *disk;
	struct disk_part_tbl *ptbl;

	// 1. 限定minors，即分区的最大数量。
	if (minors > DISK_MAX_PARTS) {
		printk(KERN_ERR
			"block: can't allocate more than %d partitions\n",
			DISK_MAX_PARTS);
		minors = DISK_MAX_PARTS;
	}

	// 2. kzalloc一个gendisk。
	disk = kzalloc_node(sizeof(struct gendisk), GFP_KERNEL, node_id);

	// 3. 上一步成功分配后，初始化gendisk。
	if (disk) {
		// 3.A. part0初始化的第一部分。具体会在下一篇文章详述。
		disk->part0.dkstats = alloc_percpu(struct disk_stats);
		if (!disk->part0.dkstats) {
			kfree(disk);
			return NULL;
		}

		// 3.B. 初始化lookup_sem和NUMA nde_id。
		init_rwsem(&disk->lookup_sem);
		disk->node_id = node_id;

		// 3.C. 向disk->part_tbl添加第0项，然后将第0项指向disk->part0。
		if (disk_expand_part_tbl(disk, 0)) {
			free_percpu(disk->part0.dkstats);
			kfree(disk);
			return NULL;
		}
		ptbl = rcu_dereference_protected(disk->part_tbl, 1);
		rcu_assign_pointer(ptbl->part[0], &disk->part0);

		// 3.D. part0初始化的第二部分。具体会在下一篇文章详述。
		/*
		 * set_capacity() and get_capacity() currently don't use
		 * seqcounter to read/update the part0->nr_sects. Still init
		 * the counter as we can read the sectors in IO submission
		 * patch using seqence counters.
		 *
		 * TODO: Ideally set_capacity() and get_capacity() should be
		 * converted to make use of bd_mutex and sequence counters.
		 */
		hd_sects_seq_init(&disk->part0);
		if (hd_ref_init(&disk->part0)) {
			hd_free_part(&disk->part0);
			kfree(disk);
			return NULL;
		}

		// 3.E. 设置minors。
		disk->minors = minors;

		// 3.F. 初始化disk->rand。
		rand_initialize_disk(disk);

		// 3.G. part0初始化第3部分，设置disk->part0.__dev，这个会在后面详述。
		disk_to_dev(disk)->class = &block_class;
		disk_to_dev(disk)->type = &disk_type;
		device_initialize(disk_to_dev(disk));
	}

	// 4. 返回。
	return disk;
}
EXPORT_SYMBOL(__alloc_disk_node);
```

## 4. `add_disk()`
在`alloc_disk()`，以及相应的设置完成后，驱动将调用`add_disk()`以注册此disk。   

``` c
static inline void add_disk(struct gendisk *disk)
{
	device_add_disk(NULL, disk, NULL);
}
```

可以看到主要实现在函数`device_add_disk()`里。   

`device_add_disk()`有3个参数:   

| Parameter | Comment |
|----------:|:--------|
| parent | 设置此`struct gendisk`对应的`struct device`的`parent` |
| disk | 目标`struct gendisk` |
| groups | 设置此`struct gendisk`对应的`struct device`的`group` |

而`device_add_disk()`又会调用`__device_add_disk()`。    

``` c
void device_add_disk(struct device *parent, struct gendisk *disk,
		     const struct attribute_group **groups)

{
	__device_add_disk(parent, disk, groups, true);
}
```

``` c
/**
 * __device_add_disk - add disk information to kernel list
 * @parent: parent device for the disk
 * @disk: per-device partitioning information
 * @groups: Additional per-device sysfs groups
 * @register_queue: register the queue if set to true
 *
 * This function registers the partitioning information in @disk
 * with the kernel.
 *
 * FIXME: error handling
 */
static void __device_add_disk(struct device *parent, struct gendisk *disk,
			      const struct attribute_group **groups,
			      bool register_queue)
{
	dev_t devt;
	int retval;

	// 1. device_add_disk()向此函数传入的register_queue是true，因此这里设置disk->queue的调度器。
	/*
	 * The disk queue should now be all set with enough information about
	 * the device for the elevator code to pick an adequate default
	 * elevator if one is needed, that is, for devices requesting queue
	 * registration.
	 */
	if (register_queue)
		elevator_init_mq(disk->queue);

	// 2. 检查参数。
	/* minors == 0 indicates to use ext devt from part0 and should
	 * be accompanied with EXT_DEVT flag.  Make sure all
	 * parameters make sense.
	 */
	WARN_ON(disk->minors && !(disk->major || disk->first_minor));
	WARN_ON(!disk->minors &&
		!(disk->flags & (GENHD_FL_EXT_DEVT | GENHD_FL_HIDDEN)));

	// 3. 设置disk->flags中的GENHD_FL_UP对应位，表示此gendisk已经启用。
	disk->flags |= GENHD_FL_UP;

	// 4. 为part0分配dev_t。
	retval = blk_alloc_devt(&disk->part0, &devt);
	if (retval) {
		WARN_ON(1);
		return;
	}

	//5. 重设disk->major和disk->first_minor。这个我认为不是必须的。
	disk->major = MAJOR(devt);
	disk->first_minor = MINOR(devt);

	// 6. 设置与初始化disk->ev。
	disk_alloc_events(disk);

	// 7. 检查是驱动否设置了此gendisk的GENHD_FL_HIDDEN，关于这个flag，可以查看 https://patchwork.kernel.org/patch/10015051/ ，这个flag可以让使得一个gendisk不向用户态暴露，只用于kernel的IO。
	if (disk->flags & GENHD_FL_HIDDEN) {
		/*
		 * Don't let hidden disks show up in /proc/partitions,
		 * and don't bother scanning for partitions either.
		 */
		disk->flags |= GENHD_FL_SUPPRESS_PARTITION_INFO;
		disk->flags |= GENHD_FL_NO_PART_SCAN;
	} else {
		// 7.A 如果没有设置GEGHD_FL_HIDDEN，则此disk将暴露给用户。
		struct backing_dev_info *bdi = disk->queue->backing_dev_info;
		struct device *dev = disk_to_dev(disk);
		int ret;

		// 7.B 向BDI系统注册此disk，后面会介绍BDI系统。
		/* Register BDI before referencing it from bdev */
		dev->devt = devt;
		ret = bdi_register(bdi, "%u:%u", MAJOR(devt), MINOR(devt));
		WARN_ON(ret);
		bdi_set_owner(bdi, dev);
		blk_register_region(disk_devt(disk), disk->minors, NULL,
				    exact_match, exact_lock, disk);
	}

	// 8. 注册设备和相关kobjects。
	register_disk(parent, disk, groups);
	
	// 9. 注册request queue和相关kobjects。
	if (register_queue)
		blk_register_queue(disk);

	// 10. 令disk->queue->kobj的reference count加1。
	/*
	 * Take an extra ref on queue which will be put on disk_release()
	 * so that it sticks around as long as @disk is there.
	 */
	WARN_ON_ONCE(!blk_get_queue(disk->queue));

	// 11. 注册disk->ev。
	disk_add_events(disk);

	// 12. 注册disk->integrity_kobj。
	blk_integrity_add(disk);
}
```

我们再来看看第8步的`register_disk()`:    
``` c
static void register_disk(struct device *parent, struct gendisk *disk,
			  const struct attribute_group **groups)
{
	struct device *ddev = disk_to_dev(disk);
	struct block_device *bdev;
	struct disk_part_iter piter;
	struct hd_struct *part;
	int err;

	// 1. 设置disk的parent设备。
	ddev->parent = parent;

	// 2. 设置disk的device的名称。
	dev_set_name(ddev, "%s", disk->disk_name);

	// 3. 暂时阻止此设备发出uevent。
	/* delay uevents, until we scanned partition table */
	dev_set_uevent_suppress(ddev, 1);

	// 4. 挂载device_add_disk()调用者指定的attribute groups。
	if (groups) {
		WARN_ON(ddev->groups);
		ddev->groups = groups;
	}

	// 5. 调用device_add()，向kernel正式添加设备。
	if (device_add(ddev))
		return;

	// 6. 如果sysfs_deprecated为false，则向kobject block_depr注册此设备。block_depr即 /sys/block。
	// 在commit edfaa7c36574f1bf09c65ad602412db9da5f96bf 之前，块设备的kobjects都会放在 /sys/block中，这个
	// commit 将它们移到了 /sys/class/block中。
	// 如果sysfs_deprecated为false，则块设备的kobjects将被同时放置在/sys/block和/sys/class/block中。
	// 具体可见commit e52eec13cd6b7f30ab19081b387813e03e592ae5，以及kernel config CONFIG_SYSFS_DEPRECATED和CONFIG_SYSFS_DEPRECATED_V2。
	if (!sysfs_deprecated) {
		err = sysfs_create_link(block_depr, &ddev->kobj,
					kobject_name(&ddev->kobj));
		if (err) {
			device_del(ddev);
			return;
		}
	}

	// 7. 由于块设备在相关代码在申请内存时必须指定GFP_NOIO，因此这里设置相关的flags，防止在对
	// 此设备进行PM相关操作，例如resume/suspend时，其中的内存请求引起IO。
	/*
	 * avoid probable deadlock caused by allocating memory with
	 * GFP_KERNEL in runtime_resume callback of its all ancestor
	 * devices
	 */
	pm_runtime_set_memalloc_noio(ddev, true);

	// 8. 在当前设备的kobject中创建holders和slaves 子kobjects。
	// Slave的概念已在上方详述，holder将在hd_struct的部分说明。
	disk->part0.holder_dir = kobject_create_and_add("holders", &ddev->kobj);
	disk->slave_dir = kobject_create_and_add("slaves", &ddev->kobj);

	// 9. 如果设备不向用户暴露，则取消对uevents的supress，然后直接返回。
	if (disk->flags & GENHD_FL_HIDDEN) {
		dev_set_uevent_suppress(ddev, 0);
		return;
	}

	// 10. 如果此块设备无法或不需要检查分区表，则直接goto exit。注意退出时，uevent还是处于supressed的状态。
	// static inline bool disk_part_scan_enabled(struct gendisk *disk)
	// {
	//	 return disk_max_parts(disk) > 1 &&
	//		 !(disk->flags & GENHD_FL_NO_PART_SCAN);
	// }
	//
	// 一些设备，例如loop设备，大多数情况下在刚被创建时是不需要扫描分区表的，因为此时设备内根本没有分区表。
	/* No minors to use for partitions */
	if (!disk_part_scan_enabled(disk))
		goto exit;

	// 11. 如果设备容量为0，则也直接goto exit。
	/* No such device (e.g., media were just removed) */
	if (!get_capacity(disk))
		goto exit;

	// 12. 获取disk->part0对应的struct block_device。
	// 注意是block_device，不是device。
	// 失败则直接goto exit。
	bdev = bdget_disk(disk, 0);
	if (!bdev)
		goto exit;

	// 13. 调用blkdev_get()。
	// 在调用前，先将bd_invalidated置1，这样，在blkdev_get()->__blkdev_get()中，会扫描分区表。
	// blkdev_get()是block device open逻辑的重要组成部分，将在未来详述。
	bdev->bd_invalidated = 1;
	err = blkdev_get(bdev, FMODE_READ, NULL);
	if (err < 0)
		goto exit;
	// 14. 调用blkdev_put()，undo blkdev_get()的其他副作用。
	// blkdev_put()是block device close逻辑的重要组成部分，将在未来详述。
	blkdev_put(bdev, FMODE_READ);

	// 14. exit标号。大多数块设备的register流程都会执行到此处。
exit:
	// 15. 停止对此设备的uevents的suppress。
	/* announce disk after possible partitions are created */
	dev_set_uevent_suppress(ddev, 0);
	// 16. 报告uevent，指示新kobject被添加到kernel。
	kobject_uevent(&ddev->kobj, KOBJ_ADD);

	// 17. 对当前块设备中的所有分区，都报告uevent，指示新kobject被添加到kernel。
	/* announce possible partitions */
	disk_part_iter_init(&piter, disk, 0);
	while ((part = disk_part_iter_next(&piter)))
		kobject_uevent(&part_to_dev(part)->kobj, KOBJ_ADD);
	disk_part_iter_exit(&piter);

	// 18. 将当前设备的request queue中的BDI的kobject添加到sysfs tree中。
	if (disk->queue->backing_dev_info->dev) {
		err = sysfs_create_link(&ddev->kobj,
			  &disk->queue->backing_dev_info->dev->kobj,
			  "bdi");
		WARN_ON(err);
	}
}
```

## 5. `del_gendisk()`
再来看删除`struct gendisk`的流程:   
``` c
void del_gendisk(struct gendisk *disk)
{
	struct disk_part_iter piter;
	struct hd_struct *part;

	// 1. 从sysfstree中移除integrity检查的相关kobjects。
	blk_integrity_del(disk);
	// 2. 从设备events链表(disk_events)中移除当前设备的事件(disk->ev)，从sysfs tree中移除当前设备的事件attrs。
	disk_del_events(disk);

	// 3. 停止其他对此gendisk的访问。
	/*
	 * Block lookups of the disk until all bdevs are unhashed and the
	 * disk is marked as dead (GENHD_FL_UP cleared).
	 */
	down_write(&disk->lookup_sem);
	/* invalidate stuff */

	// 4. 对此设备上，除了part0以外的所有分区，都调用一次invalidate_partition()和delete_partition()。
	// invalidate_partition()将把分区上的脏数据刷入分区，然后将设备上的文件系统的super blocks、所有dentries和所有inodes删除，最后将此设备上的所有clean caches invalidate掉。
	// delete_partition负责将当前partition从disk->part_tbl中移除，然后清除此分区对应的kobject，从kernel中移除它的struct device(即undo device_add())，清除此分区占用的设备号，decrement percpu reference count。
	disk_part_iter_init(&piter, disk,
			     DISK_PITER_INCL_EMPTY | DISK_PITER_REVERSE);
	while ((part = disk_part_iter_next(&piter))) {
		invalidate_partition(disk, part->partno);
		delete_partition(disk, part);
	}
	disk_part_iter_exit(&piter);

	// 5. 对part0调用invalidate_partition()。
	// 注意没有对part0调用delete_partition()。
	invalidate_partition(disk, 0);
	// 6. 将设备的容量设置为0。
	set_capacity(disk, 0);
	// 7. Unset GENHD_FL_UP，代表此gendisk已经不再可以被使用。
	disk->flags &= ~GENHD_FL_UP;
	// 8. 释放lookup_sem，undo第3步。
	up_write(&disk->lookup_sem);

	// 9. 如果此disk被暴露给用户，则remove此设备指向bdi的sysfs link。
	if (!(disk->flags & GENHD_FL_HIDDEN))
		sysfs_remove_link(&disk_to_dev(disk)->kobj, "bdi");
	// 10. 如果设备有request queue，则判断此设备是否暴露给用户，如果暴露则unregister 此queue对应的bdi设，然后unregister此request queue。
	// 如果没有request queue，则打印警告。
	if (disk->queue) {
		/*
		 * Unregister bdi before releasing device numbers (as they can
		 * get reused and we'd get clashes in sysfs).
		 */
		if (!(disk->flags & GENHD_FL_HIDDEN))
			bdi_unregister(disk->queue->backing_dev_info);
		blk_unregister_queue(disk);
	} else {
		WARN_ON(1);
	}

	// 11. 如果设备向用户暴露，则释放此gendisk占用的块设备region。
	if (!(disk->flags & GENHD_FL_HIDDEN))
		blk_unregister_region(disk_devt(disk), disk->minors);
	// 12. 释放当前gendisk占用的devt。
	/*
	 * Remove gendisk pointer from idr so that it cannot be looked up
	 * while RCU period before freeing gendisk is running to prevent
	 * use-after-free issues. Note that the device number stays
	 * "in-use" until we really free the gendisk.
	 */
	blk_invalidate_devt(disk_devt(disk));

	// 13. 移除holers和slaves这两个kobjects。
	kobject_put(disk->part0.holder_dir);
	kobject_put(disk->slave_dir);

	// 14. 一般一个gendisk的所有IO统计信息都会放在disk->part0.dkstats中。这一步将其内的统计信息全部置0。
	part_stat_set_all(&disk->part0, 0);
	// 15. 将disk->part0.stamp (IO统计的时间戳)置0。
	disk->part0.stamp = 0;
	// 16. 在sysfs_deprecated为false的情况下，remove /sys/block中对此块设备的link。
	if (!sysfs_deprecated)
		sysfs_remove_link(block_depr, dev_name(disk_to_dev(disk)));
	// 17. 取消强制对此设备做PM相关操作时，要求memory alloc必须符合GFP_NOIO的限制。
	pm_runtime_set_memalloc_noio(disk_to_dev(disk), false);
	// 18. 从系统中移除当前gendisk对应的struct device。
	device_del(disk_to_dev(disk));
}
EXPORT_SYMBOL(del_gendisk);
```