# Block Device Drivers

## Overview
The smallest logical unit of addressing is the block.   

Although the physical device can be addressed at sector level, the kernel performs all disk operations using blocks. A block is a constant number of consecutive sectors. The block size must be a power of 2 and can not exceed the size of a page.    

--------------------

## Register a block I/O device
We use `register_blkdev()` to register a block device, and `unregister_blkdev()` to unregister one.   

Since version 4.9, the call to `register_blkdev()` is optional. The only operations performed by this function are the dynamic allocation of a major (if the major argument is 0 when calling the function) and creating an entry in `/proc/devices`. It might be removed in future versions.   

```c
// Declared in include/linux/fs.h
// Implemented in block/genhd.c
// From version 4.20-rc3

/**
 * register_blkdev - register a new block device
 *
 * @major: the requested major device number [1..BLKDEV_MAJOR_MAX-1]. If
 *         @major = 0, try to allocate any unused major number.
 * @name: the name of the new block device as a zero terminated string
 *
 * The @name must be unique within the system.
 *
 * The return value depends on the @major input parameter:
 *
 *  - if a major device number was requested in range [1..BLKDEV_MAJOR_MAX-1]
 *    then the function returns zero on success, or a negative error code
 *  - if any unused major number was requested with @major = 0 parameter
 *    then the return value is the allocated major number in range
 *    [1..BLKDEV_MAJOR_MAX-1] or a negative error code otherwise
 *
 * See Documentation/admin-guide/devices.txt for the list of allocated
 * major numbers.
 */
int register_blkdev(unsigned int major, const char *name)
{
	struct blk_major_name **n, *p;
	int index, ret = 0;

	mutex_lock(&block_class_lock);

	/* temporary */
	if (major == 0) {
		for (index = ARRAY_SIZE(major_names)-1; index > 0; index--) {
			if (major_names[index] == NULL)
				break;
		}

		if (index == 0) {
			printk("register_blkdev: failed to get major for %s\n",
			       name);
			ret = -EBUSY;
			goto out;
		}
		major = index;
		ret = major;
	}

	if (major >= BLKDEV_MAJOR_MAX) {
		pr_err("register_blkdev: major requested (%u) is greater than the maximum (%u) for %s\n",
		       major, BLKDEV_MAJOR_MAX-1, name);

		ret = -EINVAL;
		goto out;
	}

	p = kmalloc(sizeof(struct blk_major_name), GFP_KERNEL);
	if (p == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	p->major = major;
	strlcpy(p->name, name, sizeof(p->name));
	p->next = NULL;
	index = major_to_index(major);

	for (n = &major_names[index]; *n; n = &(*n)->next) {
		if ((*n)->major == major)
			break;
	}
	if (!*n)
		*n = p;
	else
		ret = -EBUSY;

	if (ret < 0) {
		printk("register_blkdev: cannot get major %u for %s\n",
		       major, name);
		kfree(p);
	}
out:
	mutex_unlock(&block_class_lock);
	return ret;
}

EXPORT_SYMBOL(register_blkdev);

void unregister_blkdev(unsigned int major, const char *name)
{
	struct blk_major_name **n;
	struct blk_major_name *p = NULL;
	int index = major_to_index(major);

	mutex_lock(&block_class_lock);
	for (n = &major_names[index]; *n; n = &(*n)->next)
		if ((*n)->major == major)
			break;
	if (!*n || strcmp((*n)->name, name)) {
		WARN_ON(1);
	} else {
		p = *n;
		*n = p->next;
	}
	mutex_unlock(&block_class_lock);
	kfree(p);
}

EXPORT_SYMBOL(unregister_blkdev);
```

Typical usage:
``` c
#include <linux/fs.h>

#define MY_BLOCK_MAJOR           240
#define MY_BLKDEV_NAME          "mybdev"

static int my_block_init(void)
{
    int status;

    status = register_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
    if (status < 0) {
             printk(KERN_ERR "unable to register mybdev block device\n");
             return -EBUSY;
     }
     //...
}

static void my_block_exit(void)
{
     //...
     unregister_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
}
```

--------------

## Register a disk
`register_blkdev()` may obtain a major, however, it didn't expose the disk to the system.   

To create and use block devices (disks), a specialized interface defined in `linux/genhd.h` is used.   

`alloc_disk()` is used to allocate a disk, and the `del_gendisk()` is used to deallocate it. And we use `add_disk()` to add the disk to the system.   

```c
// v 4.20-rc3
// Declared in include/linux/genhd.h
#define alloc_disk(minors) alloc_disk_node(minors, NUMA_NO_NODE)

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

// block/genhd.c
struct gendisk *__alloc_disk_node(int minors, int node_id)
{
	struct gendisk *disk;
	struct disk_part_tbl *ptbl;

	if (minors > DISK_MAX_PARTS) {
		printk(KERN_ERR
			"block: can't allocate more than %d partitions\n",
			DISK_MAX_PARTS);
		minors = DISK_MAX_PARTS;
	}

	disk = kzalloc_node(sizeof(struct gendisk), GFP_KERNEL, node_id);
	if (disk) {
		if (!init_part_stats(&disk->part0)) {
			kfree(disk);
			return NULL;
		}
		init_rwsem(&disk->lookup_sem);
		disk->node_id = node_id;
		if (disk_expand_part_tbl(disk, 0)) {
			free_part_stats(&disk->part0);
			kfree(disk);
			return NULL;
		}
		ptbl = rcu_dereference_protected(disk->part_tbl, 1);
		rcu_assign_pointer(ptbl->part[0], &disk->part0);

		/*
		 * set_capacity() and get_capacity() currently don't use
		 * seqcounter to read/update the part0->nr_sects. Still init
		 * the counter as we can read the sectors in IO submission
		 * patch using seqence counters.
		 *
		 * TODO: Ideally set_capacity() and get_capacity() should be
		 * converted to make use of bd_mutex and sequence counters.
		 */
		seqcount_init(&disk->part0.nr_sects_seq);
		if (hd_ref_init(&disk->part0)) {
			hd_free_part(&disk->part0);
			kfree(disk);
			return NULL;
		}

		disk->minors = minors;
		rand_initialize_disk(disk);
		disk_to_dev(disk)->class = &block_class;
		disk_to_dev(disk)->type = &disk_type;
		device_initialize(disk_to_dev(disk));
	}
	return disk;
}
EXPORT_SYMBOL(__alloc_disk_node);
```   

``` c
// v 4.20-rc3

// Declared in include/linux/genhd.h
// Defined in block/genhd.c

void del_gendisk(struct gendisk *disk)
{
	struct disk_part_iter piter;
	struct hd_struct *part;

	blk_integrity_del(disk);
	disk_del_events(disk);

	/*
	 * Block lookups of the disk until all bdevs are unhashed and the
	 * disk is marked as dead (GENHD_FL_UP cleared).
	 */
	down_write(&disk->lookup_sem);
	/* invalidate stuff */
	disk_part_iter_init(&piter, disk,
			     DISK_PITER_INCL_EMPTY | DISK_PITER_REVERSE);
	while ((part = disk_part_iter_next(&piter))) {
		invalidate_partition(disk, part->partno);
		bdev_unhash_inode(part_devt(part));
		delete_partition(disk, part->partno);
	}
	disk_part_iter_exit(&piter);

	invalidate_partition(disk, 0);
	bdev_unhash_inode(disk_devt(disk));
	set_capacity(disk, 0);
	disk->flags &= ~GENHD_FL_UP;
	up_write(&disk->lookup_sem);

	if (!(disk->flags & GENHD_FL_HIDDEN))
		sysfs_remove_link(&disk_to_dev(disk)->kobj, "bdi");
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

	if (!(disk->flags & GENHD_FL_HIDDEN))
		blk_unregister_region(disk_devt(disk), disk->minors);

	kobject_put(disk->part0.holder_dir);
	kobject_put(disk->slave_dir);

	part_stat_set_all(&disk->part0, 0);
	disk->part0.stamp = 0;
	if (!sysfs_deprecated)
		sysfs_remove_link(block_depr, dev_name(disk_to_dev(disk)));
	pm_runtime_set_memalloc_noio(disk_to_dev(disk), false);
	device_del(disk_to_dev(disk));
}
EXPORT_SYMBOL(del_gendisk);
```

```c
// v 4.20-rc3
// In include/linux/genhd.h
static inline void add_disk(struct gendisk *disk)
{
	device_add_disk(NULL, disk, NULL);
}

void device_add_disk(struct device *parent, struct gendisk *disk,
		     const struct attribute_group **groups)

{
	__device_add_disk(parent, disk, groups, true);
}

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

	/* minors == 0 indicates to use ext devt from part0 and should
	 * be accompanied with EXT_DEVT flag.  Make sure all
	 * parameters make sense.
	 */
	WARN_ON(disk->minors && !(disk->major || disk->first_minor));
	WARN_ON(!disk->minors &&
		!(disk->flags & (GENHD_FL_EXT_DEVT | GENHD_FL_HIDDEN)));

	disk->flags |= GENHD_FL_UP;

	retval = blk_alloc_devt(&disk->part0, &devt);
	if (retval) {
		WARN_ON(1);
		return;
	}
	disk->major = MAJOR(devt);
	disk->first_minor = MINOR(devt);

	disk_alloc_events(disk);

	if (disk->flags & GENHD_FL_HIDDEN) {
		/*
		 * Don't let hidden disks show up in /proc/partitions,
		 * and don't bother scanning for partitions either.
		 */
		disk->flags |= GENHD_FL_SUPPRESS_PARTITION_INFO;
		disk->flags |= GENHD_FL_NO_PART_SCAN;
	} else {
		int ret;

		/* Register BDI before referencing it from bdev */
		disk_to_dev(disk)->devt = devt;
		ret = bdi_register_owner(disk->queue->backing_dev_info,
						disk_to_dev(disk));
		WARN_ON(ret);
		blk_register_region(disk_devt(disk), disk->minors, NULL,
				    exact_match, exact_lock, disk);
	}
	register_disk(parent, disk, groups);
	if (register_queue)
		blk_register_queue(disk);

	/*
	 * Take an extra ref on queue which will be put on disk_release()
	 * so that it sticks around as long as @disk is there.
	 */
	WARN_ON_ONCE(!blk_get_queue(disk->queue));

	disk_add_events(disk);
	blk_integrity_add(disk);
}
```

Typical usage:
``` c
#include <linux/fs.h>
#include <linux/genhd.h>

#define MY_BLOCK_MINORS       1

static struct my_block_dev {
    struct gendisk *gd;
    //...
} dev;

static int create_block_device(struct my_block_dev *dev)
{
    dev->gd = alloc_disk(MY_BLOCK_MINORS);
    //...
    add_disk(dev->gd);
}

static int my_block_init(void)
{
    //...
    create_block_device(&dev);
}

static void delete_block_device(struct my_block_dev *dev)
{
    if (dev->gd)
        del_gendisk(dev->gd);
    //...
}

static void my_block_exit(void)
{
    delete_block_device(&dev);
    //...
}
```

It shall be noticed that immediately after calling `add_disk()` (even before `add_disk()` returns), the disk is active and the kernel may start operations on it. So this function should not be called before the driver is fully initialized and ready to respond to requests for the registered disk.    

The basic structure in working with block devices (disks) is the `struct gendisk` structure.   

After a call to `del_gendisk()`, the `struct gendisk` may still exists, because it's reference counted (If an open operation was called on the device but the associated releases operation has not been called).   

----------------------

## `struct gendisk` structure

As stated above, a `struct gendisk` stores information about a disk. It is obtained from the `alloc_disk()` call and its fields must be filled before sending it to `add_disk()` function.   

``` c
// v 4.20-rc3
// From include/linux/genhd.h
struct gendisk {
	/* major, first_minor and minors are input parameters only,
	 * don't use directly.  Use disk_devt() and disk_max_parts().
	 */
	int major;			/* major number of driver */
	int first_minor;
	int minors;                     /* maximum number of minors, =1 for
                                         * disks that can't be partitioned. */

	char disk_name[DISK_NAME_LEN];	/* name of major driver */
	char *(*devnode)(struct gendisk *gd, umode_t *mode);

	unsigned int events;		/* supported events */
	unsigned int async_events;	/* async events, subset of all */

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
	int node_id;
	struct badblocks *bb;
	struct lockdep_map lockdep_map;
};
```

| Fields | Meanings |
|:-------|---------:|
| `major`, `first_minor` and `minor` | Describe the identifiers used by the disk; a disk must have at least one minor; if the disk allows the partitioning operation, a minor must be allocated for each possible partition. |
| `disk_name` | Represents the disk name as it appears in /proc/partitions and in sysfs (/sys/block). |
| `fops` | Operations associated with the disk. |
| `queue` | Represents the queue of requests |
| `capacity` | Means the disk capacity in 512-byte sectors; it shall be initialized using the `set_capacity()` function. |
| `private_data` | Points to the private data. |


An example of filling a `struct gendisk`:   

``` c
#include <linux/genhd.h>
#include <linux/fs.h>
#include <linux/blkdev.h>

#define NR_SECTORS                   1024

#define KERNEL_SECTOR_SIZE           512

static struct my_block_dev {
    //...
    spinlock_t lock;                /* For mutual exclusion */
    struct request_queue *queue;    /* The device request queue */
    struct gendisk *gd;             /* The gendisk structure */
    //...
} dev;

static int create_block_device(struct my_block_dev *dev)
{
    ...
    /* Initialize the gendisk structure */
    dev->gd = alloc_disk(MY_BLOCK_MINORS);
    if (!dev->gd) {
        printk (KERN_NOTICE "alloc_disk failure\n");
        return -ENOMEM;
    }

    dev->gd->major = MY_BLOCK_MAJOR;
    dev->gd->first_minor = 0;
    dev->gd->fops = &my_block_ops;
    dev->gd->queue = dev->queue;
    dev->gd->private_data = dev;
    snprintf (dev->gd->disk_name, 32, "myblock");
    set_capacity(dev->gd, NR_SECTORS);

    add_disk(dev->gd);

    return 0;
}

static int my_block_init(void)
{
    int status;
    //...
    status = create_block_device(&dev);
    if (status < 0)
        return status;
    //...
}

static void delete_block_device(struct my_block_dev *dev)
{
    if (dev->gd) {
        del_gendisk(dev->gd);
    }
    //...
}

static void my_block_exit(void)
{
    delete_block_device(&dev);
    //...
}
```   

If the sector size of the device is not 512 bytes, the driver may use `blk_queue_logical_block_size()` (declared in `include/linux/blkdev.h`, implemented in `block/blk-settings.c`) to inform the kernel about this. However, the communication between the device and the deriver will still be performed in sectores of 512 bytes in size, so conversion should be done each time (including calling `set_capacity()`).   

--------------------

## `struct block_device_operations` structure

As for a character device, a block device shall be associated with an "operations list" as well. For block device, this list is `struct block_device_operations`. It is pointed by the field `fops` in `struct gendisk`.   

``` c
// v 4.20-rc3
// From include/linux/blkdev.h
truct block_device_operations {
	int (*open) (struct block_device *, fmode_t);
	void (*release) (struct gendisk *, fmode_t);
	int (*rw_page)(struct block_device *, sector_t, struct page *, unsigned int);
	int (*ioctl) (struct block_device *, fmode_t, unsigned, unsigned long);
	int (*compat_ioctl) (struct block_device *, fmode_t, unsigned, unsigned long);
	unsigned int (*check_events) (struct gendisk *disk,
				      unsigned int clearing);
	/* ->media_changed() is DEPRECATED, use ->check_events() instead */
	int (*media_changed) (struct gendisk *);
	void (*unlock_native_capacity) (struct gendisk *);
	int (*revalidate_disk) (struct gendisk *);
	int (*getgeo)(struct block_device *, struct hd_geometry *);
	/* this callback is with swap_lock and sometimes page table lock held */
	void (*swap_slot_free_notify) (struct block_device *, unsigned long);
	int (*report_zones)(struct gendisk *, sector_t sector,
			    struct blk_zone *zones, unsigned int *nr_zones,
			    gfp_t gfp_mask);
	struct module *owner;
	const struct pr_ops *pr_ops;
};
```   

`open()` and `release()` operations are called directly from user space by utilities that may perform the following tasks: partitioning, file system creation, file system verification. In a `mount()` operation, the `open()` function is called directly from the kernel space, the file descriptor being stored by the kernel. A driver for a block device can not differentiate between `open()` calls performed from user space and kernel space.    

An example of setting `open` and `release`:
``` c
#include <linux/fs.h>
#include <linux/genhd.h>

static struct my_block_dev {
    //...
    struct gendisk * gd;
    //...
} dev;

static int my_block_open(struct block_device *bdev, fmode_t mode)
{
    //...

    return 0;
}

static int my_block_release(struct gendisk *gd, fmode_t mode)
{
    //...

    return 0;
}

struct block_device_operations my_block_ops = {
    .owner = THIS_MODULE,
    .open = my_block_open,
    .release = my_block_release
};

static int create_block_device(struct my_block_dev *dev)
{
    //....
    dev->gd->fops = &my_block_ops;
    dev->gd->private_data = dev;
    //...
}
```   

Unlike character devices, there is no `read` or `write` operation for a block device. These operations are performed by `request()` associated with the request queue of the disk.   

--------------------

## Request Queues
Since most block device I/O operations are high time-consuming, the perform of the block device I/Os may be asynchronous.    
For some block devices, HDDs for example, if the I/Os performed on them are totally random, the performance may be poor. So it is necessary to place a mechanism, to schedule the I/O requests (by merging several requests into one and sorting requests).   
So block devices' I/Os are more completed that the character devices'.    

Drivers for block devices use queues to store the block requests I/O that will be processed.   
A request queue is represented by the `struct request_queue` structure. The request queue is made up of a doubly-linked list of requests arnd their associated control information.    

As long as the request queue is not empty, the queue's associated driver will have to retrieve the first request from the queue and pass it to the asociated block device. Each item in the request queue is a request represented by the `struct request` structure.   

``` c
// v 4.20-rc3
// From include/linux/blkdev.h
struct request_queue {
	/*
	 * Together with queue_head for cacheline sharing
	 */
	struct list_head	queue_head;
	struct request		*last_merge;
	struct elevator_queue	*elevator;
	int			nr_rqs[2];	/* # allocated [a]sync rqs */
	int			nr_rqs_elvpriv;	/* # allocated rqs w/ elvpriv */

	struct blk_queue_stats	*stats;
	struct rq_qos		*rq_qos;

	/*
	 * If blkcg is not used, @q->root_rl serves all requests.  If blkcg
	 * is used, root blkg allocates from @q->root_rl and all other
	 * blkgs from their own blkg->rl.  Which one to use should be
	 * determined using bio_request_list().
	 */
	struct request_list	root_rl;

	request_fn_proc		*request_fn;
	make_request_fn		*make_request_fn;
	poll_q_fn		*poll_fn;
	prep_rq_fn		*prep_rq_fn;
	unprep_rq_fn		*unprep_rq_fn;
	softirq_done_fn		*softirq_done_fn;
	rq_timed_out_fn		*rq_timed_out_fn;
	dma_drain_needed_fn	*dma_drain_needed;
	lld_busy_fn		*lld_busy_fn;
	/* Called just after a request is allocated */
	init_rq_fn		*init_rq_fn;
	/* Called just before a request is freed */
	exit_rq_fn		*exit_rq_fn;
	/* Called from inside blk_get_request() */
	void (*initialize_rq_fn)(struct request *rq);

	const struct blk_mq_ops	*mq_ops;

	unsigned int		*mq_map;

	/* sw queues */
	struct blk_mq_ctx __percpu	*queue_ctx;
	unsigned int		nr_queues;

	unsigned int		queue_depth;

	/* hw dispatch queues */
	struct blk_mq_hw_ctx	**queue_hw_ctx;
	unsigned int		nr_hw_queues;

	/*
	 * Dispatch queue sorting
	 */
	sector_t		end_sector;
	struct request		*boundary_rq;

	/*
	 * Delayed queue handling
	 */
	struct delayed_work	delay_work;

	struct backing_dev_info	*backing_dev_info;

	/*
	 * The queue owner gets to use this for whatever they like.
	 * ll_rw_blk doesn't touch it.
	 */
	void			*queuedata;

	/*
	 * various queue flags, see QUEUE_* below
	 */
	unsigned long		queue_flags;
	/*
	 * Number of contexts that have called blk_set_pm_only(). If this
	 * counter is above zero then only RQF_PM and RQF_PREEMPT requests are
	 * processed.
	 */
	atomic_t		pm_only;

	/*
	 * ida allocated id for this queue.  Used to index queues from
	 * ioctx.
	 */
	int			id;

	/*
	 * queue needs bounce pages for pages above this limit
	 */
	gfp_t			bounce_gfp;

	/*
	 * protects queue structures from reentrancy. ->__queue_lock should
	 * _never_ be used directly, it is queue private. always use
	 * ->queue_lock.
	 */
	spinlock_t		__queue_lock;
	spinlock_t		*queue_lock;

	/*
	 * queue kobject
	 */
	struct kobject kobj;

	/*
	 * mq queue kobject
	 */
	struct kobject mq_kobj;

#ifdef  CONFIG_BLK_DEV_INTEGRITY
	struct blk_integrity integrity;
#endif	/* CONFIG_BLK_DEV_INTEGRITY */

#ifdef CONFIG_PM
	struct device		*dev;
	int			rpm_status;
	unsigned int		nr_pending;
#endif

	/*
	 * queue settings
	 */
	unsigned long		nr_requests;	/* Max # of requests */
	unsigned int		nr_congestion_on;
	unsigned int		nr_congestion_off;
	unsigned int		nr_batching;

	unsigned int		dma_drain_size;
	void			*dma_drain_buffer;
	unsigned int		dma_pad_mask;
	unsigned int		dma_alignment;

	struct blk_queue_tag	*queue_tags;

	unsigned int		nr_sorted;
	unsigned int		in_flight[2];

	/*
	 * Number of active block driver functions for which blk_drain_queue()
	 * must wait. Must be incremented around functions that unlock the
	 * queue_lock internally, e.g. scsi_request_fn().
	 */
	unsigned int		request_fn_active;

	unsigned int		rq_timeout;
	int			poll_nsec;

	struct blk_stat_callback	*poll_cb;
	struct blk_rq_stat	poll_stat[BLK_MQ_POLL_STATS_BKTS];

	struct timer_list	timeout;
	struct work_struct	timeout_work;
	struct list_head	timeout_list;

	struct list_head	icq_list;
#ifdef CONFIG_BLK_CGROUP
	DECLARE_BITMAP		(blkcg_pols, BLKCG_MAX_POLS);
	struct blkcg_gq		*root_blkg;
	struct list_head	blkg_list;
#endif

	struct queue_limits	limits;

#ifdef CONFIG_BLK_DEV_ZONED
	/*
	 * Zoned block device information for request dispatch control.
	 * nr_zones is the total number of zones of the device. This is always
	 * 0 for regular block devices. seq_zones_bitmap is a bitmap of nr_zones
	 * bits which indicates if a zone is conventional (bit clear) or
	 * sequential (bit set). seq_zones_wlock is a bitmap of nr_zones
	 * bits which indicates if a zone is write locked, that is, if a write
	 * request targeting the zone was dispatched. All three fields are
	 * initialized by the low level device driver (e.g. scsi/sd.c).
	 * Stacking drivers (device mappers) may or may not initialize
	 * these fields.
	 *
	 * Reads of this information must be protected with blk_queue_enter() /
	 * blk_queue_exit(). Modifying this information is only allowed while
	 * no requests are being processed. See also blk_mq_freeze_queue() and
	 * blk_mq_unfreeze_queue().
	 */
	unsigned int		nr_zones;
	unsigned long		*seq_zones_bitmap;
	unsigned long		*seq_zones_wlock;
#endif /* CONFIG_BLK_DEV_ZONED */

	/*
	 * sg stuff
	 */
	unsigned int		sg_timeout;
	unsigned int		sg_reserved_size;
	int			node;
#ifdef CONFIG_BLK_DEV_IO_TRACE
	struct blk_trace	*blk_trace;
	struct mutex		blk_trace_mutex;
#endif
	/*
	 * for flush operations
	 */
	struct blk_flush_queue	*fq;

	struct list_head	requeue_list;
	spinlock_t		requeue_lock;
	struct delayed_work	requeue_work;

	struct mutex		sysfs_lock;

	int			bypass_depth;
	atomic_t		mq_freeze_depth;

#if defined(CONFIG_BLK_DEV_BSG)
	bsg_job_fn		*bsg_job_fn;
	struct bsg_class_device bsg_dev;
#endif

#ifdef CONFIG_BLK_DEV_THROTTLING
	/* Throttle data */
	struct throtl_data *td;
#endif
	struct rcu_head		rcu_head;
	wait_queue_head_t	mq_freeze_wq;
	struct percpu_ref	q_usage_counter;
	struct list_head	all_q_node;

	struct blk_mq_tag_set	*tag_set;
	struct list_head	tag_set_list;
	struct bio_set		bio_split;

#ifdef CONFIG_BLK_DEBUG_FS
	struct dentry		*debugfs_dir;
	struct dentry		*sched_debugfs_dir;
#endif

	bool			mq_sysfs_init_done;

	size_t			cmd_size;
	void			*rq_alloc_data;

	struct work_struct	release_work;

#define BLK_MAX_WRITE_HINTS	5
	u64			write_hints[BLK_MAX_WRITE_HINTS];
};
```

---------------------

## Create and delete a request queue
A request queue is created with the `blk_init_queue()` function and is deleted using the `blk_cleanup_queue()` function.   

``` c
// v 4.20-rc3
// Declared in include/linux/blkdev.h
// Defined in block/blk-core.c

/**
 * blk_init_queue  - prepare a request queue for use with a block device
 * @rfn:  The function to be called to process requests that have been
 *        placed on the queue.
 * @lock: Request queue spin lock
 *
 * Description:
 *    If a block device wishes to use the standard request handling procedures,
 *    which sorts requests and coalesces adjacent requests, then it must
 *    call blk_init_queue().  The function @rfn will be called when there
 *    are requests on the queue that need to be processed.  If the device
 *    supports plugging, then @rfn may not be called immediately when requests
 *    are available on the queue, but may be called at some time later instead.
 *    Plugged queues are generally unplugged when a buffer belonging to one
 *    of the requests on the queue is needed, or due to memory pressure.
 *
 *    @rfn is not required, or even expected, to remove all requests off the
 *    queue, but only as many as it can handle at a time.  If it does leave
 *    requests on the queue, it is responsible for arranging that the requests
 *    get dealt with eventually.
 *
 *    The queue spin lock must be held while manipulating the requests on the
 *    request queue; this lock will be taken also from interrupt context, so irq
 *    disabling is needed for it.
 *
 *    Function returns a pointer to the initialized request queue, or %NULL if
 *    it didn't succeed.
 *
 * Note:
 *    blk_init_queue() must be paired with a blk_cleanup_queue() call
 *    when the block device is deactivated (such as at module unload).
 **/

struct request_queue *blk_init_queue(request_fn_proc *rfn, spinlock_t *lock)
{
	return blk_init_queue_node(rfn, lock, NUMA_NO_NODE);
}
EXPORT_SYMBOL(blk_init_queue);   

struct request_queue *
blk_init_queue_node(request_fn_proc *rfn, spinlock_t *lock, int node_id)
{
	struct request_queue *q;

	q = blk_alloc_queue_node(GFP_KERNEL, node_id, lock);
	if (!q)
		return NULL;

	q->request_fn = rfn;
	if (blk_init_allocated_queue(q) < 0) {
		blk_cleanup_queue(q);
		return NULL;
	}

	return q;
}
EXPORT_SYMBOL(blk_init_queue_node);

/**
 * blk_cleanup_queue - shutdown a request queue
 * @q: request queue to shutdown
 *
 * Mark @q DYING, drain all pending requests, mark @q DEAD, destroy and
 * put it.  All future requests will be failed immediately with -ENODEV.
 */
void blk_cleanup_queue(struct request_queue *q)
{
	spinlock_t *lock = q->queue_lock;

	/* mark @q DYING, no new request or merges will be allowed afterwards */
	mutex_lock(&q->sysfs_lock);
	blk_set_queue_dying(q);
	spin_lock_irq(lock);

	/*
	 * A dying queue is permanently in bypass mode till released.  Note
	 * that, unlike blk_queue_bypass_start(), we aren't performing
	 * synchronize_rcu() after entering bypass mode to avoid the delay
	 * as some drivers create and destroy a lot of queues while
	 * probing.  This is still safe because blk_release_queue() will be
	 * called only after the queue refcnt drops to zero and nothing,
	 * RCU or not, would be traversing the queue by then.
	 */
	q->bypass_depth++;
	queue_flag_set(QUEUE_FLAG_BYPASS, q);

	queue_flag_set(QUEUE_FLAG_NOMERGES, q);
	queue_flag_set(QUEUE_FLAG_NOXMERGES, q);
	queue_flag_set(QUEUE_FLAG_DYING, q);
	spin_unlock_irq(lock);
	mutex_unlock(&q->sysfs_lock);

	/*
	 * Drain all requests queued before DYING marking. Set DEAD flag to
	 * prevent that q->request_fn() gets invoked after draining finished.
	 */
	blk_freeze_queue(q);

	rq_qos_exit(q);

	spin_lock_irq(lock);
	queue_flag_set(QUEUE_FLAG_DEAD, q);
	spin_unlock_irq(lock);

	/*
	 * make sure all in-progress dispatch are completed because
	 * blk_freeze_queue() can only complete all requests, and
	 * dispatch may still be in-progress since we dispatch requests
	 * from more than one contexts.
	 *
	 * We rely on driver to deal with the race in case that queue
	 * initialization isn't done.
	 */
	if (q->mq_ops && blk_queue_init_done(q))
		blk_mq_quiesce_queue(q);

	/* for synchronous bio-based driver finish in-flight integrity i/o */
	blk_flush_integrity();

	/* @q won't process any more request, flush async actions */
	del_timer_sync(&q->backing_dev_info->laptop_mode_wb_timer);
	blk_sync_queue(q);

	/*
	 * I/O scheduler exit is only safe after the sysfs scheduler attribute
	 * has been removed.
	 */
	WARN_ON_ONCE(q->kobj.state_in_sysfs);

	blk_exit_queue(q);

	if (q->mq_ops)
		blk_mq_free_queue(q);
	percpu_ref_exit(&q->q_usage_counter);

	spin_lock_irq(lock);
	if (q->queue_lock != &q->__queue_lock)
		q->queue_lock = &q->__queue_lock;
	spin_unlock_irq(lock);

	/* @q is and will stay empty, shutdown and put */
	blk_put_queue(q);
}
EXPORT_SYMBOL(blk_cleanup_queue);
```   

An example for using these functions:
``` c
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>

static struct my_block_dev {
    //...
    struct request_queue *queue;
    //...
} dev;

static void my_block_request(struct request_queue *q);
//...

static int create_block_device(struct my_block_dev *dev)
{
    /* Initialize the I/O queue */
    spin_lock_init(&dev->lock);
    dev->queue = blk_init_queue(my_block_request, &dev->lock);
    if (dev->queue == NULL)
        goto out_err;
    blk_queue_logical_block_size(dev->queue, KERNEL_SECTOR_SIZE);
    dev->queue->queuedata = dev;
    //...

out_err:
    return -ENOMEM;
}

static int my_block_init(void)
{
    int status;
    //...
    status = create_block_device(&dev);
    if (status < 0)
        return status;
    //...
}

static void delete_block_device(struct block_dev *dev)
{
    //...
    if (dev->queue)
        blk_cleanup_queue(dev->queue);
}

static void my_block_exit(void)
{
    delete_block_device(&dev);
    //...
}
```   

The `blk_init_queue()`'s first argument is a function pointer, any request in this queue is processed by that function.   

The lock parameter is a spinlock, and shall be manually initialized (not by the kernel or some other mechanisms). This lock is held by the kernel to ensure the request handler's exclusive access to the queue. This spinlock can also be used in other driver functions to protect access to shared data with the `request()` function.   

As part of the request queue initialization, you can conficure the `queuedata` field, which is equivalent to the `private_data` field in other structures.   

-----------------------

## Useful functions for processing request queues
When request handlers called, they are passed with the request queues. It is the handler's job to retrieve the requests from the queue, since the amount of requests processed by the handler is not determined.   

The following functions are used for dealing with the request queue:   

+   `blk_peek_request()`: Retrieves a reference to the first request from the queue; the respective request must be started using `blk_start_request()`.
+   `blk_start_request()`: Extracts the request from the queue and starts it for processing; in general, the function receives as a reference a pointer to a request returned by `blk_peek_request()`;
+   `blk_fetch_request()`: Retrieves the first request from the queue (using `blk_peek_request()`) and starts it (using `blk_start_request()`);
+   `blk_requeue_request()`: To re-enter queue.   

The spinlock associated with the queue must be acquired before calling any function above. If a funciton is called by the funciton of type `request_fn_proc` (all the functions above are of this type), the spinlock is already held.   

------------------------

## Requests for block devices
The internal of a `struct request`:
``` c
// v 4.20-rc3
// From include/linux/blkdev.h
/*
 * Try to put the fields that are referenced together in the same cacheline.
 *
 * If you modify this structure, make sure to update blk_rq_init() and
 * especially blk_mq_rq_ctx_init() to take care of the added fields.
 */
struct request {
	struct request_queue *q;
	struct blk_mq_ctx *mq_ctx;

	int cpu;
	unsigned int cmd_flags;		/* op and common flags */
	req_flags_t rq_flags;

	int internal_tag;

	/* the following two fields are internal, NEVER access directly */
	unsigned int __data_len;	/* total data len */
	int tag;
	sector_t __sector;		/* sector cursor */

	struct bio *bio;
	struct bio *biotail;

	struct list_head queuelist;

	/*
	 * The hash is used inside the scheduler, and killed once the
	 * request reaches the dispatch list. The ipi_list is only used
	 * to queue the request for softirq completion, which is long
	 * after the request has been unhashed (and even removed from
	 * the dispatch list).
	 */
	union {
		struct hlist_node hash;	/* merge hash */
		struct list_head ipi_list;
	};

	/*
	 * The rb_node is only used inside the io scheduler, requests
	 * are pruned when moved to the dispatch queue. So let the
	 * completion_data share space with the rb_node.
	 */
	union {
		struct rb_node rb_node;	/* sort/lookup */
		struct bio_vec special_vec;
		void *completion_data;
		int error_count; /* for legacy drivers, don't use */
	};

	/*
	 * Three pointers are available for the IO schedulers, if they need
	 * more they have to dynamically allocate it.  Flush requests are
	 * never put on the IO scheduler. So let the flush fields share
	 * space with the elevator data.
	 */
	union {
		struct {
			struct io_cq		*icq;
			void			*priv[2];
		} elv;

		struct {
			unsigned int		seq;
			struct list_head	list;
			rq_end_io_fn		*saved_end_io;
		} flush;
	};

	struct gendisk *rq_disk;
	struct hd_struct *part;
	/* Time that I/O was submitted to the kernel. */
	u64 start_time_ns;
	/* Time that I/O was submitted to the device. */
	u64 io_start_time_ns;

#ifdef CONFIG_BLK_WBT
	unsigned short wbt_flags;
#endif
#ifdef CONFIG_BLK_DEV_THROTTLING_LOW
	unsigned short throtl_size;
#endif

	/*
	 * Number of scatter-gather DMA addr+len pairs after
	 * physical address coalescing is performed.
	 */
	unsigned short nr_phys_segments;

#if defined(CONFIG_BLK_DEV_INTEGRITY)
	unsigned short nr_integrity_segments;
#endif

	unsigned short write_hint;
	unsigned short ioprio;

	void *special;		/* opaque pointer available for LLD use */

	unsigned int extra_len;	/* length of alignment and padding */

	enum mq_rq_state state;
	refcount_t ref;

	unsigned int timeout;

	/* access through blk_rq_set_deadline, blk_rq_deadline */
	unsigned long __deadline;

	struct list_head timeout_list;

	union {
		struct __call_single_data csd;
		u64 fifo_time;
	};

	/*
	 * completion callback.
	 */
	rq_end_io_fn *end_io;
	void *end_io_data;

	/* for bidi */
	struct request *next_rq;

#ifdef CONFIG_BLK_CGROUP
	struct request_list *rl;		/* rl this rq is alloced from */
#endif
};
```   

Some important fields in `struct request`:   

+   `cmd_flags`: a series of flags including direction (reading or writing); to find out the direction, the macrodefinition `rq_data_dir` is used, which returns 0 for a read request and 1 for a write request on the device.   
+   `__sector`: the first sector of the transfer request; if the device sector has a different size, the appropriate conversion should be done. To access this field, use the `blk_rq_pos` macro.   
+   `__data_len`: the total number of bytes to be transferred; to access this field the `blk_rq_bytes` macro is used.   
+   generally, data from the current `struct bio` will be transferred; the data size is obtained using the `blk_rq_cur_bytes` macro.
+   `bio`, a dynamic list of struct bio structures that is a set of buffers associated to the request; this field is accessed by macrodefinition. `rq_for_each_segment` if there are multiple buffers, or by `bio_data` macrodefinition in case there is only one associated buffer.  
+   `bio_data`: the address of the buffer associated to the request.

## Create a request
Read /write requests are created by code layers superior to the kernel I/O subsystem. Typically, the subsystem that creates requests for block devices is the file management subsystem. The I/O subsystem acts as an interface between the file management subsystem and the block device driver. The main operations under the responsibility of the I/O subsystem are adding requests to the queue of the specific block device and sorting and merging requests according to performance considerations.   

----------------------

## Finish a request
When the driver has finished transferring all the sectors of a request to /from the device, it must inform the I/O subsystem by calling the `blk_end_request()` function. If the lock associated to the request queue is already acquired, the `__blk_end_request()` function can be used.

If the driver wants to close the request even if it did not transfer all the related sectors, it can call the `blk_end_request_all()` or `__blk_end_request_all()` function. The `__blk_end_request_all()` function is called if the lock associated to the request queue is already acquired.

------------------

## Process a request
As stated before, the handler for dealing with the requests are the function whose address was passed to `blk_init_queue()`.   

With the aid of that spinlock, each time the request handler is executed, it is under an atomic context. And, during the execution, no more requests are inserted into that queue.       

The time to invoke this handler is determine by the kernel. So the I/O requests issued by the kernel or user processes may not be immediately performed. This is for the sake of performance. As stated above, the I/O on a block device, like HDD, is very slow, and there is an I/O scheduler who's responsible to organize the requests. So it is necessary to execute the handler asynchronously.   

Since the handler is executed asynchronousely, there is no guarantee that under which process's context the code is running. Which means, the buffer related to the request may belong to any user process. Furthermore, the address may not belong to user-space, it may comes from the kernel space. So every time we add some buffer into a request, it's best for us to transform it into physical address.      

Here is one example of a request handler:   

```c
static void my_block_request(struct request_queue *q)
{
    struct request *rq;
    struct my_block_dev *dev = q->queuedata;

    while (1) {
        rq = blk_fetch_request(q);
        if (rq == NULL)
            break;

        if (blk_rq_is_passthrough(rq)) {
            printk (KERN_NOTICE "Skip non-fs request\n");
            __blk_end_request_all(rq, -EIO);
           continue;
        }

        /* do work */
        ...

        __blk_end_request_all(rq, 0);
    }
}
```   

Here are some explanation about this example:   
+   Read the first request from the queue using `blk_fetch_request()`. As described in Useful functions for processing request queues section, the `blk_fetch_request()` function retrieves the first item from the request queue and starts the request.    
+   If the function returns NULL, it has reached the end of the request queue (there is no remaining request to be processed) and exits `my_block_request()`.   
+   A block device can receive calls which do not transfer data blocks (e.g. low level operations on the disk, instructions referring to special ways of accessing the device). Most drivers do not know how to handle these requests and return an error.   
+   To return an error, `__blk_end_request_all()` function is called, `-EIO` being the second argument.   
+   The request is processed according to the needs of the associated device.   
+   The request ends. In this case, `__blk_end_request_all()` function is called in order to complete the request entirely. If all request sectors have been processed, the `__blk_end_request()` function is used.   

------------------------

## `struct bio` structure
Each `struct request` represents an I/O request, which may come from combining more independent requests from a higher level. 

The sectors to be transferred for a request can be scattered into the main memory but they always correspond to a set of consecutive sectors on the device.   

The request is represented as a series of segment, each corresponding to a buffer in memory.   

The kernel can combine requests that refer to adjacent sectors but will not combine write requests with read requests into a single `struct request` structure.     

A `struct request` strucrure is implemented as a linked list of `struct bio` structures togethor with information that allows the driver to retain its current position while processing the request.   

So, the `struct bio` structure is a low-level description of a portion of a block I/O request.   

```c
// v 4.20-rc3
// From include/linux/blk_types.h

/*
 * main unit of I/O for the block layer and lower layers (ie drivers and
 * stacking drivers)
 */
struct bio {
	struct bio		*bi_next;	/* request queue link */
	struct gendisk		*bi_disk;
	unsigned int		bi_opf;		/* bottom bits req flags,
						 * top bits REQ_OP. Use
						 * accessors.
						 */
	unsigned short		bi_flags;	/* status, etc and bvec pool number */
	unsigned short		bi_ioprio;
	unsigned short		bi_write_hint;
	blk_status_t		bi_status;
	u8			bi_partno;

	/* Number of segments in this BIO after
	 * physical address coalescing is performed.
	 */
	unsigned int		bi_phys_segments;

	/*
	 * To keep track of the max segment size, we account for the
	 * sizes of the first and last mergeable segments in this bio.
	 */
	unsigned int		bi_seg_front_size;
	unsigned int		bi_seg_back_size;

	struct bvec_iter	bi_iter;

	atomic_t		__bi_remaining;
	bio_end_io_t		*bi_end_io;

	void			*bi_private;
#ifdef CONFIG_BLK_CGROUP
	/*
	 * Optional ioc and css associated with this bio.  Put on bio
	 * release.  Read comment on top of bio_associate_current().
	 */
	struct io_context	*bi_ioc;
	struct cgroup_subsys_state *bi_css;
	struct blkcg_gq		*bi_blkg;
	struct bio_issue	bi_issue;
#endif
	union {
#if defined(CONFIG_BLK_DEV_INTEGRITY)
		struct bio_integrity_payload *bi_integrity; /* data integrity */
#endif
	};

	unsigned short		bi_vcnt;	/* how many bio_vec's */

	/*
	 * Everything starting with bi_max_vecs will be preserved by bio_reset()
	 */

	unsigned short		bi_max_vecs;	/* max bvl_vecs we can hold */

	atomic_t		__bi_cnt;	/* pin count */

	struct bio_vec		*bi_io_vec;	/* the actual vec list */

	struct bio_set		*bi_pool;

	/*
	 * We can inline a number of vecs at the end of the bio, to avoid
	 * double allocations for a small number of bio_vecs. This member
	 * MUST obviously be kept at the very end of the bio.
	 */
	struct bio_vec		bi_inline_vecs[0];
};

#define BIO_RESET_BYTES		offsetof(struct bio, bi_max_vecs)
```   

In turn, the `struct bio` contains a `bi_io_vec` vector of `struct bio_vec` structures. It consists of the individual pages in the physical memory to be transferred, the offset within the page and the size of the buffer.    

To iterate through a `struct bio` structure, we need to iterate through the vector of `struct bio_vec` and transfer the data from every physical page.    

To simplfy the vector iteration, the `struct bvec_iter` is used. This structure maintains information about how many buffers and sectors were consumed during the iteration. The request type is encoded in the `bi_opf` field; to determine it, use the `bio_data_dir()` function.   

---------------------

## Create a `struct bio` structure
Use one of these two functions:   
+   `bio_alloc()`: allocates space for a new structure; the structure must be initialized;
+   `bio_clone()`: makes a copy of an existing `struct bio` structure; The new one is initialized with the same values of the old one, including the buffer address. So after cloning, the buffer becomed shared.   

--------------------------

## Submit a `struct bio` structure
Usually, a `struct bio` is created by the higher levels of the kernek (usually the file system). A structure thus created is then transmitted to the I/O subsystem that gathers more `struct bio` structures into a request.   

To submit a `struct bio` to the request queue, the function `submit_bio()` is used.   

----------------

## Wait for the completin of a `struct bio` structure
After `submit_bio()` returns, the insertion to the queue may not be finished. One can use `submit_bio_wait()` instead.   

----------------

## Initialize a `struct bio` structure
A `struct bio` must be initialized before being transmitted.   

The `bi_end_io` field is used to specify the function called when the processing of this structure is finished.   

The `bi_private` field is used to store the private data, it may be useful to `bi_end_io`.   

The `bi_opf` field specified the type of the operations, use the macro `bio_set_op_attrs` to set it.   

Here is an example:  

```c
struct bio *bio = bio_alloc(GFP_NOIO, 1);
//...
bio->bi_disk = bdev->bd_disk;
bio->bi_iter.bi_sector = sector;
bio_set_op_attrs(bio, REQ_OP_READ, 0);
bio_add_page(bio, page, size, offset);
//...
```   

------------------------

## How to use the content of a `struct bio` strcuture
If we can be sure that the addresses we inserted into the `struct bio` are physical, we can use `kmap_atomic()` and `kunmap_atomic()` to map and unmap them.   

``` c
static void my_block_transfer(struct my_block_dev *dev, size_t start,
                              size_t len, char *buffer, int dir);


static int my_xfer_bio(struct my_block_dev *dev, struct bio *bio)
{
    int i;
    struct bio_vec bvec;
    struct bvec_iter i;
    int dir = bio_data_dir(bio);

    /* Do each segment independently. */
    bio_for_each_segment(bvec, bio, i) {
        sector_t sector = i.bi_sector;
        char *buffer = kmap_atomic(bvec.bv_page);
        unsigned long offset = bvec.bv_offset;
        size_t len = bvec.bv_len;

        /* process mapped buffer */
        my_block_transfer(dev, sector, len, buffer + offset, dir);

        kunmap_atomic(buffer);
    }

    return 0;
}

```   

If you want to get all the `struct bio_vec`s with one `for`, you can use `rq_for_each_segment`:    

``` c
struct bio_vec bvec;
struct req_iterator iter;

rq_for_each_segment(bvec, req, iter) {
    sector_t sector = iter.iter.bi_sector;
    char *buffer = kmap_atomic(bvec.bv_page);
    unsigned long offset = bvec.bv_offset;
    size_t len = bvec.bv_len;
    int dir = bio_data_dir(iter.bio);

    my_block_transfer(dev, sector, len, buffer + offset, dir);

    kunmap_atomic(buffer);
}
```   

------------------

## Free a `struct bio` structure   
We can use `bio_put()` to free a `struct bio`.   

