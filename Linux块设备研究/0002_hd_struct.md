hd_struct与disk_part_tbl
TODO:
disk_expand_part_tbl()
holder_dir
add_partition()
invalidate_partition()
delete_partition()
disk_part_iter_init()
disk_part_iter_next()
disk_part_iter_exit()
part_stat_set_all()

``` c
// __alloc_disk_node():
disk->part0.dkstats = alloc_percpu(struct disk_stats);
if (!disk->part0.dkstats) {
	kfree(disk);
	return NULL;
}

// ...

if (disk_expand_part_tbl(disk, 0)) {
	free_percpu(disk->part0.dkstats);
	kfree(disk);
	return NULL;
}
ptbl = rcu_dereference_protected(disk->part_tbl, 1);
rcu_assign_pointer(ptbl->part[0], &disk->part0);

// ...

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

// ...

disk_to_dev(disk)->class = &block_class;
disk_to_dev(disk)->type = &disk_type;
device_initialize(disk_to_dev(disk));

// ...

// __device_add_disk():
retval = blk_alloc_devt(&disk->part0, &devt);
if (retval) {
	WARN_ON(1);
	return;
}

// ...

// register_disk():
disk->part0.holder_dir = kobject_create_and_add("holders", &ddev->kobj);

// ...

// 通过blkdev_get()，从设备中读取分区表，并写入到了disk->part_tbl中；同时也读取设备的容量，通过set_capacity()写入了part0。
bdev->bd_invalidated = 1;
err = blkdev_get(bdev, FMODE_READ, NULL);
if (err < 0)
	goto exit;
blkdev_put(bdev, FMODE_READ);

// ...

/* announce possible partitions */
disk_part_iter_init(&piter, disk, 0);
while ((part = disk_part_iter_next(&piter)))
	kobject_uevent(&part_to_dev(part)->kobj, KOBJ_ADD);
disk_part_iter_exit(&piter);
```

``` c
del_gendisk():
down_write(&disk->lookup_sem);
// ...
disk_part_iter_init(&piter, disk,
		     DISK_PITER_INCL_EMPTY | DISK_PITER_REVERSE);
while ((part = disk_part_iter_next(&piter))) {
	invalidate_partition(disk, part->partno);
	delete_partition(disk, part);
}
disk_part_iter_exit(&piter);

// ...

invalidate_partition(disk, 0);
set_capacity(disk, 0);
// ...
up_write(&disk->lookup_sem);

// ...

kobject_put(disk->part0.holder_dir);
```

后续:
blkdev_get()     

# `hd_struct`与`disk_part_tbl`
`struct hd_struct`与`disk_part_tbl`也是块设备使用过程中的重要数据结构。块设备上的每一个分区，都对应一个`hd_struct`。     
此外，每一个块设备的管理数据结构(`struct gendisk`)，也都有一个名为`part0`的`struct hd_struct`类型的field，它并不是代表某一个分区，而是代表了整个块设备。   
`struct disk_part_tbl`是用来存储分区表的数据结构，`struct gendisk`中的field `part_tbl`即是这个类型，它被宏`__rcu`修饰，也就是说我们必须通过相应的RCU API来访问此field。    

## 1. `struct hd_struct`
这个结构体的定义如下:   
``` c
struct hd_struct {
	sector_t start_sect;
	/*
	 * nr_sects is protected by sequence counter. One might extend a
	 * partition while IO is happening to it and update of nr_sects
	 * can be non-atomic on 32bit machines with 64bit sector_t.
	 */
	sector_t nr_sects;
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	seqcount_t nr_sects_seq;
#endif
	unsigned long stamp;
	struct disk_stats __percpu *dkstats;
	struct percpu_ref ref;

	sector_t alignment_offset;
	unsigned int discard_alignment;
	struct device __dev;
	struct kobject *holder_dir;
	int policy, partno;
	struct partition_meta_info *info;
#ifdef CONFIG_FAIL_MAKE_REQUEST
	int make_it_fail;
#endif
	struct rcu_work rcu_work;
};
```

| Field | Comment |
|------:|:--------|
| start_sect | 当前分区的起始sector，对于part0，这个值自然是0。 |
| nr_sects | 当前分区中sectors的数量，由于每个sector是512Bytes，所以这个值也可以指代块设备的容量。对于part0，`nr_sects`自然是全盘的sectors数量。 |
| stamp | 统计IO的时间戳，具体作用会在后续的文章里详述。 |
| dkstats | 当前partition/disk的IO统计信息。 |
| ref | Reference count。 |
| alignment_offset | 指当前分区或块设备的起始地址相对于aligment boundary的位移量。例如，如果一个块设备的alignment是4KiB，但是一个分区的起始地址是Byte 8704(sector 17)，则alignment offset值为512 (8704 = 4096 * 2 + 512)。事实上内核中其他部分和驱动逻辑一般不参考这个值，它们一般参考request_queue的alignment_offset。 |
| discard_alignement | 和`alignemt_offset`类似，但这里描述的是discard命令的`alignment_offset` |
| __dev | 代表当前分区的dev，我理解应该是像 `/dev/sda1`、`/dev/sdb2`这样的设备。 |
| holder_dir | 此kobject内会存放指向当前块设备的slaves的链接。与gendisk的slave_dir相反。如果sda1和其他块设备组成了device mapper dm-1 (device mapper是LVM实现的基础机制)，那么dm-1的gendisk中的slaves_dir将有指向sda1的链接；而sda1的part0的holder_dir中将存在指向dm-1的链接；它们都可以在sysfs中看到。具体可见kernel中函数`bd_link_disk_holder()` (fs/block_dev.c)的说明文档。 |
| policy | 没有找到文档。从代码中看，这一项应该和readonly的设置有关。如果此项不为0，则代表当前分区/块设备 readonly。它一般通过helper函数 |
| partno | 分区号，part0的分区号是0；其他的分区从1开始编号。 |
| info | 存储当前分区的卷标（volume name)和UUID。 |
| rcu_work | 用于执行reclaimer部分的work。这个field目前只在分区删除时被使用。 |

另外，两个条件编译的fields:   

| Field | Preprocessor Condition | Comment |
|------:|-----------------------:|:--------|
| nr_sects_seq | `BITS_PER_LONG==32 && defined(CONFIG_SMP)` | 用于保护`nr_sects`的顺序锁。在部分32位架构上，处理器无法对一个64 bits的数据进行原子操作，必须分2次进行。因此需要这个顺序锁来保护。 |
| make_it_fail | `CONFIG_FAIL_MAKE_REQUEST` | 启用编译选项`CONFIG_FAIL_MAKE_REQUEST`后，用户可以通过sysfs修改`make_it_fail`的值。在这个值不为0时，在当前分区/块设备上的IO都将报错。此项主要被用于debug。 |

## 2. `struct disk_part_tbl`
`struct disk_part_tbl`即为存储分区表的数据结构。每个`struct gendisk`都有一个此类型的field。注意这个表中有一项指向当前`struct gendisk`的part0。   

``` c
struct disk_part_tbl {
	struct rcu_head rcu_head;
	int len;
	struct hd_struct __rcu *last_lookup;
	struct hd_struct __rcu *part[];
};
```

| Field | Comment |
|------:|:--------|
| rcu_head | RCU Protection |
| len | 此表中的内容(`struct hd_struct`)的数量。因为其中一个`struct hd_struct`是part0，因此这个field也等于当前设备分区数+1。 |
| last_lookup | Lookup Buffer，用于缩短partition的查找时间。 |
| part | 存储`struct hd_struct`地址的数组。 |

## 3. `part0`和`part_tbl`的初始化
`part0`和`part_tbl`的初始化是`strcut gendisk`的初始化的一部分，因而它们也经历了`alloc_disk()`、`add_disk_node()`等函数。     
由于`struct gendisk`是通过`kzalloc()`分配的，因此`part0`和`part_tbl`的内容在开始初始化前全部是0。    
这里将这些函数中与这两个fields相关的部分摘出:   

``` c
// __alloc_disk_node():

// 分配part0中的dkstats。
disk->part0.dkstats = alloc_percpu(struct disk_stats);
if (!disk->part0.dkstats) {
	kfree(disk);
	return NULL;
}

// ...

// 向part_tbl中添加part0。
if (disk_expand_part_tbl(disk, 0)) {
	free_percpu(disk->part0.dkstats);
	kfree(disk);
	return NULL;
}
ptbl = rcu_dereference_protected(disk->part_tbl, 1);
rcu_assign_pointer(ptbl->part[0], &disk->part0);

// ...

// 设置part0中，设备的sectors数量为0。
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

// 设置part0的reference count。
if (hd_ref_init(&disk->part0)) {
	hd_free_part(&disk->part0);
	kfree(disk);
	return NULL;
}

// ...

// 初始化disk->part0.__dev。
disk_to_dev(disk)->class = &block_class;
disk_to_dev(disk)->type = &disk_type;
device_initialize(disk_to_dev(disk));

// ...
```

``` c
// __device_add_disk():

// 分配设备号。
retval = blk_alloc_devt(&disk->part0, &devt);
if (retval) {
	WARN_ON(1);
	return;
}
```

``` c
// register_disk():

// 创建holders kobject
disk->part0.holder_dir = kobject_create_and_add("holders", &ddev->kobj);

// ...

// 通过blkdev_get()，从设备中读取分区表，并写入到了disk->part_tbl中；同时也读取设备的容量，通过set_capacity()写入了part0。
bdev->bd_invalidated = 1;
err = blkdev_get(bdev, FMODE_READ, NULL);
if (err < 0)
	goto exit;
blkdev_put(bdev, FMODE_READ);

// ...

// 为所有分区推送 KOBJ_ADD uevent。
/* announce possible partitions */
disk_part_iter_init(&piter, disk, 0);
while ((part = disk_part_iter_next(&piter)))
	kobject_uevent(&part_to_dev(part)->kobj, KOBJ_ADD);
disk_part_iter_exit(&piter);
```

部分相关函数的定义如下，它们当中的部分不仅只在初始化块设备时被使用，还会在添加新分区时被使用:    

``` c
/**
 * disk_expand_part_tbl - expand disk->part_tbl
 * @disk: disk to expand part_tbl for
 * @partno: expand such that this partno can fit in
 *
 * Expand disk->part_tbl such that @partno can fit in.  disk->part_tbl
 * uses RCU to allow unlocked dereferencing for stats and other stuff.
 *
 * LOCKING:
 * Matching bd_mutex locked or the caller is the only user of @disk.
 * Might sleep.
 *
 * RETURNS:
 * 0 on success, -errno on failure.
 */
int disk_expand_part_tbl(struct gendisk *disk, int partno)
{
	struct disk_part_tbl *old_ptbl =
		rcu_dereference_protected(disk->part_tbl, 1);
	struct disk_part_tbl *new_ptbl;
	int len = old_ptbl ? old_ptbl->len : 0;
	int i, target;

	/*
	 * check for int overflow, since we can get here from blkpg_ioctl()
	 * with a user passed 'partno'.
	 */
	target = partno + 1;
	if (target < 0)
		return -EINVAL;

	/* disk_max_parts() is zero during initialization, ignore if so */
	if (disk_max_parts(disk) && target > disk_max_parts(disk))
		return -EINVAL;

	if (target <= len)
		return 0;

	new_ptbl = kzalloc_node(struct_size(new_ptbl, part, target), GFP_KERNEL,
				disk->node_id);
	if (!new_ptbl)
		return -ENOMEM;

	new_ptbl->len = target;

	for (i = 0; i < len; i++)
		rcu_assign_pointer(new_ptbl->part[i], old_ptbl->part[i]);

	disk_replace_part_tbl(disk, new_ptbl);
	return 0;
}
```

``` c
/**
 * disk_replace_part_tbl - replace disk->part_tbl in RCU-safe way
 * @disk: disk to replace part_tbl for
 * @new_ptbl: new part_tbl to install
 *
 * Replace disk->part_tbl with @new_ptbl in RCU-safe way.  The
 * original ptbl is freed using RCU callback.
 *
 * LOCKING:
 * Matching bd_mutex locked or the caller is the only user of @disk.
 */
static void disk_replace_part_tbl(struct gendisk *disk,
				  struct disk_part_tbl *new_ptbl)
{
	struct disk_part_tbl *old_ptbl =
		rcu_dereference_protected(disk->part_tbl, 1);

	rcu_assign_pointer(disk->part_tbl, new_ptbl);

	if (old_ptbl) {
		rcu_assign_pointer(old_ptbl->last_lookup, NULL);
		kfree_rcu(old_ptbl, rcu_head);
	}
}
```

``` c
static inline void hd_sects_seq_init(struct hd_struct *p)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	seqcount_init(&p->nr_sects_seq);
#endif
}
```

``` c
int hd_ref_init(struct hd_struct *part)
{
	if (percpu_ref_init(&part->ref, hd_struct_free, 0, GFP_KERNEL))
		return -ENOMEM;
	return 0;
}
```

``` c
// 此函数为partition/part0分配设备号。
// 通常，每个块设备的最大子设备号数量都已被设置(disk->minors)，这个值减去1即为最大分区数量。
// 当分区号大于这个最大分区数量时，将自动使用"extended device numbers"。
// 具体可以见commit bcce3de1be61e424deef35d1e86e86a35c4b6e65。
/**
 * blk_alloc_devt - allocate a dev_t for a partition
 * @part: partition to allocate dev_t for
 * @devt: out parameter for resulting dev_t
 *
 * Allocate a dev_t for block device.
 *
 * RETURNS:
 * 0 on success, allocated dev_t is returned in *@devt.  -errno on
 * failure.
 *
 * CONTEXT:
 * Might sleep.
 */
int blk_alloc_devt(struct hd_struct *part, dev_t *devt)
{
	struct gendisk *disk = part_to_disk(part);
	int idx;

	/* in consecutive minor range? */
	if (part->partno < disk->minors) {
		*devt = MKDEV(disk->major, disk->first_minor + part->partno);
		return 0;
	}

	/* allocate ext devt */
	idr_preload(GFP_KERNEL);

	spin_lock_bh(&ext_devt_lock);
	idx = idr_alloc(&ext_devt_idr, part, 0, NR_EXT_DEVT, GFP_NOWAIT);
	spin_unlock_bh(&ext_devt_lock);

	idr_preload_end();
	if (idx < 0)
		return idx == -ENOSPC ? -EBUSY : idx;

	*devt = MKDEV(BLOCK_EXT_MAJOR, blk_mangle_minor(idx));
	return 0;
}
```

`blkdev_get()`和`blkdev_put()`将在后续的文章中介绍；`blkdev_get()`中对分区的操作将在下方详述。    

## 4. `del_gendisk()`中对`part0`和`part_tbl`的处理
块设备的卸载逻辑，`del_gendisk()`中，对这2个部分的处理逻辑：    
``` c
// del_gendisk():

down_write(&disk->lookup_sem);
// ...
disk_part_iter_init(&piter, disk,
		     DISK_PITER_INCL_EMPTY | DISK_PITER_REVERSE);
while ((part = disk_part_iter_next(&piter))) {
	invalidate_partition(disk, part->partno);
	delete_partition(disk, part);
}
disk_part_iter_exit(&piter);

// ...

invalidate_partition(disk, 0);
set_capacity(disk, 0);
// ...
up_write(&disk->lookup_sem);

// ...

kobject_put(disk->part0.holder_dir);

// ...

part_stat_set_all(&disk->part0, 0);
disk->part0.stamp = 0;
if (!sysfs_deprecated)
	sysfs_remove_link(block_depr, dev_name(disk_to_dev(disk)));
pm_runtime_set_memalloc_noio(disk_to_dev(disk), false);
device_del(disk_to_dev(disk));
```

主要函数的实现如下: 
### 4.1 使`struct hd_struct`无效化
这一步主要是通过`invalidate_partition()`实现的:    
``` c
static void invalidate_partition(struct gendisk *disk, int partno)
{
	struct block_device *bdev;

	bdev = bdget_disk(disk, partno);
	if (!bdev)
		return;

	fsync_bdev(bdev);
	__invalidate_device(bdev, true);

	/*
	 * Unhash the bdev inode for this device so that it gets evicted as soon
	 * as last inode reference is dropped.
	 */
	remove_inode_hash(bdev->bd_inode);
	bdput(bdev);
}
```
它主要调用了3个函数: `fsync_bdev()`、`__invalidate_device()`和`remove_inode_hash()`。   
``` c
// 刷脏数据
/*
 * Write out and wait upon all dirty data associated with this
 * device.   Filesystem data as well as the underlying block
 * device.  Takes the superblock lock.
 */
int fsync_bdev(struct block_device *bdev)
{
	struct super_block *sb = get_super(bdev);
	if (sb) {
		int res = sync_filesystem(sb);
		drop_super(sb);
		return res;
	}
	return sync_blockdev(bdev);
}
EXPORT_SYMBOL(fsync_bdev);
```

``` c
int __invalidate_device(struct block_device *bdev, bool kill_dirty)
{
	struct super_block *sb = get_super(bdev);
	int res = 0;

	// 1. 清理当前分区上的文件系统。
	if (sb) {
		/*
		 * no need to lock the super, get_super holds the
		 * read mutex so the filesystem cannot go away
		 * under us (->put_super runs with the write lock
		 * hold).
		 */
		shrink_dcache_sb(sb);
		res = invalidate_inodes(sb, kill_dirty);
		drop_super(sb);
	}
	// 2. 清理此partition上的clean数据。
	invalidate_bdev(bdev);
	return res;
}
EXPORT_SYMBOL(__invalidate_device);
```

``` c
/* Invalidate clean unused buffers and pagecache. */
void invalidate_bdev(struct block_device *bdev)
{
	struct address_space *mapping = bdev->bd_inode->i_mapping;

	if (mapping->nrpages) {
		invalidate_bh_lrus();
		lru_add_drain_all();	/* make sure all lru add caches are flushed */
		invalidate_mapping_pages(mapping, 0, -1);
	}
	/* 99% of the time, we don't need to flush the cleancache on the bdev.
	 * But, for the strange corners, lets be cautious
	 */
	cleancache_invalidate_inode(mapping);
}
EXPORT_SYMBOL(invalidate_bdev);
```

``` c
// 我的理解，这个函数是将当前分区从/dev中移除。
static inline void remove_inode_hash(struct inode *inode)
{
	if (!inode_unhashed(inode) && !hlist_fake(&inode->i_hash))
		__remove_inode_hash(inode);
}
```
### 4.2 删除分区数据结构
这一部分通过`delete_partition()`实现。但是注意，此函数没有对`part0`调用。   

``` c
/*
 * Must be called either with bd_mutex held, before a disk can be opened or
 * after all disk users are gone.
 */
void delete_partition(struct gendisk *disk, struct hd_struct *part)
{
	struct disk_part_tbl *ptbl =
		rcu_dereference_protected(disk->part_tbl, 1);

	/*
	 * ->part_tbl is referenced in this part's release handler, so
	 *  we have to hold the disk device
	 */
	get_device(disk_to_dev(part_to_disk(part)));
	rcu_assign_pointer(ptbl->part[part->partno], NULL);
	kobject_put(part->holder_dir);
	device_del(part_to_dev(part));

	/*
	 * Remove gendisk pointer from idr so that it cannot be looked up
	 * while RCU period before freeing gendisk is running to prevent
	 * use-after-free issues. Note that the device number stays
	 * "in-use" until we really free the gendisk.
	 */
	blk_invalidate_devt(part_devt(part));
	percpu_ref_kill(&part->ref);
}
```
这个函数主要4件事:    
1. 将`part_tbl`中的相应分区指针赋成`NULL`。
2. 从kernel中注销与删除当前分区对应的device (field `__dev`)。
3. 如果当前分区的分区号为`blk ext dev_t`，则释放它；如果不是，则这里不做任何操作，等待`del_gendisk()`的后续逻辑做释放操作。
4. Kill当前分区的reference count。


``` c
void blk_invalidate_devt(dev_t devt)
{
	if (MAJOR(devt) == BLOCK_EXT_MAJOR) {
		spin_lock_bh(&ext_devt_lock);
		idr_replace(&ext_devt_idr, NULL, blk_mangle_minor(MINOR(devt)));
		spin_unlock_bh(&ext_devt_lock);
	}
}
```

### 4.3 设置sectors数量为0
``` c
static inline void set_capacity(struct gendisk *disk, sector_t size)
{
	disk->part0.nr_sects = size;
}
```

### 4.4 清除IO统计信息
``` c
static inline void part_stat_set_all(struct hd_struct *part, int value)
{
	int i;

	for_each_possible_cpu(i)
		memset(per_cpu_ptr(part->dkstats, i), value,
				sizeof(struct disk_stats));
}
```

## 5. 分区迭代器
分区迭代器是遍历分区表时常用的工具。迭代器的数据类型是`struct disk_part_iter`。   

Kernel中有多处对分区迭代器的使用，例如`del_gendisk()`中的一段:   
``` c
disk_part_iter_init(&piter, disk,
		     DISK_PITER_INCL_EMPTY | DISK_PITER_REVERSE);
while ((part = disk_part_iter_next(&piter))) {
	invalidate_partition(disk, part->partno);
	delete_partition(disk, part);
}
disk_part_iter_exit(&piter);
```

### 5.1 分区迭代器初始化
`disk_part_iter_init()`的实现如下:    
``` c
/**
 * disk_part_iter_init - initialize partition iterator
 * @piter: iterator to initialize
 * @disk: disk to iterate over
 * @flags: DISK_PITER_* flags
 *
 * Initialize @piter so that it iterates over partitions of @disk.
 *
 * CONTEXT:
 * Don't care.
 */
void disk_part_iter_init(struct disk_part_iter *piter, struct gendisk *disk,
			  unsigned int flags)
{
	struct disk_part_tbl *ptbl;

	rcu_read_lock();
	ptbl = rcu_dereference(disk->part_tbl);

	piter->disk = disk;
	piter->part = NULL;

	if (flags & DISK_PITER_REVERSE)
		piter->idx = ptbl->len - 1;
	else if (flags & (DISK_PITER_INCL_PART0 | DISK_PITER_INCL_EMPTY_PART0))
		piter->idx = 0;
	else
		piter->idx = 1;

	piter->flags = flags;

	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(disk_part_iter_init);
```

这个函数生成的迭代器，默认会升序遍历当前`struct gendisk`中的所有分区，但是会跳过part0；同时也会跳过sectors数量为0的分区。   
可以将如下值组合，传入flags以改变迭代器默认的行为:     

``` c
/*
 * Smarter partition iterator without context limits.
 */
#define DISK_PITER_REVERSE	(1 << 0) /* iterate in the reverse direction */
#define DISK_PITER_INCL_EMPTY	(1 << 1) /* include 0-sized parts */
#define DISK_PITER_INCL_PART0	(1 << 2) /* include partition 0 */
#define DISK_PITER_INCL_EMPTY_PART0 (1 << 3) /* include empty partition 0 */
```

| Flag | Comment |
|-----:|:--------|
| DISK_PITER_REVERSE | 倒序遍历。 |
| DISK_PITER_INCL_EMPTY | 遍历包括sectors数量为0的分区。如果没有设置此flag，则sectors为0的分区会被跳过。 |
| DISK_PITER_INCL_PART0 | 遍历包括part0，但如果part0的sectors数量为0则仍会被跳过。 |
| DISK_PITER_INCL_EMPTY_PART0 | 强制要求遍历包括part0，即使它的sectors数量为0。 |

### 5.2 迭代器的使用

迭代器通过`disk_part_iter_next()`来移动以及返回当前分区。    
此函数返回NULL代表遍历已结束。    

``` c
/**
 * disk_part_iter_next - proceed iterator to the next partition and return it
 * @piter: iterator of interest
 *
 * Proceed @piter to the next partition and return it.
 *
 * CONTEXT:
 * Don't care.
 */
struct hd_struct *disk_part_iter_next(struct disk_part_iter *piter)
{
	struct disk_part_tbl *ptbl;
	int inc, end;

	/* put the last partition */
	disk_put_part(piter->part);
	piter->part = NULL;

	/* get part_tbl */
	rcu_read_lock();
	ptbl = rcu_dereference(piter->disk->part_tbl);

	/* determine iteration parameters */
	if (piter->flags & DISK_PITER_REVERSE) {
		inc = -1;
		if (piter->flags & (DISK_PITER_INCL_PART0 |
				    DISK_PITER_INCL_EMPTY_PART0))
			end = -1;
		else
			end = 0;
	} else {
		inc = 1;
		end = ptbl->len;
	}

	/* iterate to the next partition */
	for (; piter->idx != end; piter->idx += inc) {
		struct hd_struct *part;

		part = rcu_dereference(ptbl->part[piter->idx]);
		if (!part)
			continue;
		if (!part_nr_sects_read(part) &&
		    !(piter->flags & DISK_PITER_INCL_EMPTY) &&
		    !(piter->flags & DISK_PITER_INCL_EMPTY_PART0 &&
		      piter->idx == 0))
			continue;

		get_device(part_to_dev(part));
		piter->part = part;
		piter->idx += inc;
		break;
	}

	rcu_read_unlock();

	return piter->part;
}
EXPORT_SYMBOL_GPL(disk_part_iter_next);
```

注意这里使用了`get_device(part_to_dev(part))`和`disk_put_part(piter->part)`来维护对partition的reference count。所以，虽然迭代到分区会在rcu read critical sections之外被访问或者修改，但也是安全的。    
但是必须注意的是，某些情况下，在进行其他对分区表以及其中的分区(包括part0)的操作前，必须先获取`struct gendisk`的`lookup_sem`。   

### 5.3 结束迭代
在完成迭代(包括正常的完成迭代，即`disk_part_iter_next()`返回`NULL`；也包括在没有返回`NULL`时提前结束迭代)后，需要调用`disk_part_iter_exit()`以结束对最后一个分区的引用。   
``` c
/**
 * disk_part_iter_exit - finish up partition iteration
 * @piter: iter of interest
 *
 * Called when iteration is over.  Cleans up @piter.
 *
 * CONTEXT:
 * Don't care.
 */
void disk_part_iter_exit(struct disk_part_iter *piter)
{
	disk_put_part(piter->part);
	piter->part = NULL;
}
EXPORT_SYMBOL_GPL(disk_part_iter_exit);
```
## 6. 添加分区
在`blkdev_get()`中，会读取设备，进行设置设备容量，添加分区等操作。这里关注一下添加分区的流程。    
第一次添加块设备时，会依次调用全局数组`check_part`中的各个函数。每一个函数都对应一种分区表类型的扫描逻辑。当某一个函数在当前块设备上发现了与之对应的分区表后，它将读取它们并为这些分区创建数据结构、设置`part_tbl`。        
这个逻辑在函数`blk_add_partitions()`中(`blkdev_get()`->`__blkdev_get()`->`bdev_disk_changed()`->`blk_add_partitions()`)。    

// TODO: Finish This.
``` c
int blk_add_partitions(struct gendisk *disk, struct block_device *bdev)
{
	struct parsed_partitions *state;
	int ret = -EAGAIN, p, highest;

	// 1. 检查此块设备上是否可以/需要扫描分区表。
	// static inline bool disk_part_scan_enabled(struct gendisk *disk)
	// {
	// 	return disk_max_parts(disk) > 1 &&
	// 		!(disk->flags & GENHD_FL_NO_PART_SCAN);
	// }
	if (!disk_part_scan_enabled(disk))
		return 0;

	// 2. 检查分区表。
	// 对check_part中各个函数的调用即发生在这个函数里。
	// 检查的结果将以 struct parsed_partitions *的形式返回。
	state = check_partition(disk, bdev);
	if (!state)
		return 0;
	if (IS_ERR(state)) {
		/*
		 * I/O error reading the partition table.  If we tried to read
		 * beyond EOD, retry after unlocking the native capacity.
		 */
		if (PTR_ERR(state) == -ENOSPC) {
			printk(KERN_WARNING "%s: partition table beyond EOD, ",
			       disk->disk_name);
			if (disk_unlock_native_capacity(disk))
				return -EAGAIN;
		}
		return -EIO;
	}

	// 3. 如果当前块设备是host managed zoned block devices，则不应该对此设备检查分区。
	/*
	 * Partitions are not supported on host managed zoned block devices.
	 */
	if (disk->queue->limits.zoned == BLK_ZONED_HM) {
		pr_warn("%s: ignoring partition table on host managed zoned block device\n",
			disk->disk_name);
		ret = 0;
		goto out_free_state;
	}

	// 4. 这一步不是很理解。
	// 事实上如果access_beyound_eod为true，那么check_partition()会直接返回-ENOSPC，所以在第2步直接就会退出函数。
	/*
	 * If we read beyond EOD, try unlocking native capacity even if the
	 * partition table was successfully read as we could be missing some
	 * partitions.
	 */
	if (state->access_beyond_eod) {
		printk(KERN_WARNING
		       "%s: partition table partially beyond EOD, ",
		       disk->disk_name);
		if (disk_unlock_native_capacity(disk))
			goto out_free_state;
	}

	// 5. 对当前块设备发送 KOBJ_CHANGE uevent。
	/* tell userspace that the media / partition table may have changed */
	kobject_uevent(&disk_to_dev(disk)->kobj, KOBJ_CHANGE);

	// 6. 调用disk_expand_part_tbl()，在part_tbl中为各个分区分配空间。
	// 注意这里忽略了size为0的分区。
	/*
	 * Detect the highest partition number and preallocate disk->part_tbl.
	 * This is an optimization and not strictly necessary.
	 */
	for (p = 1, highest = 0; p < state->limit; p++)
		if (state->parts[p].size)
			highest = p;
	disk_expand_part_tbl(disk, highest);

	// 7. 为各个新分区调用`blk_add_partition()`。
	for (p = 1; p < state->limit; p++)
		if (!blk_add_partition(disk, bdev, state, p))
			goto out_free_state;

	// 8. 正常退出的返回值为0。
	ret = 0;
out_free_state:

	// 9. 释放state以及其占用的部分资源，这些资源是check_partition()分配的。
	// static void free_partitions(struct parsed_partitions *state)
	// {
	// 	vfree(state->parts);
	// 	kfree(state);
	// }
	free_partitions(state);

	// 10. 返回。
	return ret;
}
```

可以看到，此函数中主要的逻辑在对每个分区的`blk_add_partition()`调用中。     
`blk_add_partition()`的返回值类型是`bool`，但它并非代表添加成功或失败，而是被用于通知上层函数是否需要继续进行分区添加的操作。上方的`blk_add_partitions()`的实现如下:     
``` c
static bool blk_add_partition(struct gendisk *disk, struct block_device *bdev,
		struct parsed_partitions *state, int p)
{
	// 1. 获取当前分区的sectors数量。
	sector_t size = state->parts[p].size;
	// 2. 获取当前分区的起始sector编号。
	sector_t from = state->parts[p].from;
	struct hd_struct *part;

	// 2. 如果当前分区sectors数量为0，则退出。
	if (!size)
		return true;

	// 3. 检查分区起始sectors号是否在设备的sectors总量内。
	// 如果在范围内，则继续。
	// 否则，则先打印一个警告，然后调用disk_unlock_native_capacity()，最后直接return。
	// 如果disk_unlock_native_capacity()返回true，则此函数返回false，通知上层函数(这里是blk_add_partitions())停止对分区的添加；否则此函数会返回true，上层函数会继续添加后续的分区。
	//
	// disk_unlock_native_capacity()会在设备的block_device_opetations中存在unlock_native_capacity回调时调用此函数，并设置gendisk的flags，再返回true；如果当前disk的block_device_operations中不存在unlock_native_capacity回调，则直接返回false。
	// 目前没有找到对unlock_native_capacity的解释，但是似乎这个功能只有IDE设备才有，其他块设备都不会注册这个回调，也就是说对于这些设备，disk_unlock_native_capacity()会返回false。
	if (from >= get_capacity(disk)) {
		printk(KERN_WARNING
		       "%s: p%d start %llu is beyond EOD, ",
		       disk->disk_name, p, (unsigned long long) from);
		if (disk_unlock_native_capacity(disk))
			return false;
		return true;
	}

	// 4. 检查分区末尾sectors号是否在设备的sectors总量内。
	// 如果在范围内，则继续。
	// 否则，则先调用disk_unlock_native_capacity()，如果返回true，则此函数直接返回false，通知上层函数不再继续添加后续分区。
	// 如果disk_unlock_native_capacity()返回false，则将当前分区truncate到有效范围内，再继续。
	if (from + size > get_capacity(disk)) {
		printk(KERN_WARNING
		       "%s: p%d size %llu extends beyond EOD, ",
		       disk->disk_name, p, (unsigned long long) size);

		if (disk_unlock_native_capacity(disk))
			return false;

		/*
		 * We can not ignore partitions of broken tables created by for
		 * example camera firmware, but we limit them to the end of the
		 * disk to avoid creating invalid block devices.
		 */
		size = get_capacity(disk) - from;
	}

	// 5. 调用add_partition()添加分区。
	// 如果add_partition()返回错误，且错误值不是-ENXIO(返回-ENXIO的情况在下方可以看到，这里不会出现这种情况)，则打印信息并返回true。
	part = add_partition(disk, p, from, size, state->parts[p].flags,
			     &state->parts[p].info);
	if (IS_ERR(part) && PTR_ERR(part) != -ENXIO) {
		printk(KERN_ERR " %s: p%d could not be added: %ld\n",
		       disk->disk_name, p, -PTR_ERR(part));
		return true;
	}

	// 6. RAID支持，与LVM/MD等功能有关。
	// 具体可见 https://www.oreilly.com/library/view/managing-raid-on/9780596802035/ch03s01s02.html 。
	if (IS_BUILTIN(CONFIG_BLK_DEV_MD) &&
	    (state->parts[p].flags & ADDPART_FLAG_RAID))
		md_autodetect_dev(part_to_dev(part)->devt);

	return true;
}
```

``` c
/*
 * Must be called either with bd_mutex held, before a disk can be opened or
 * after all disk users are gone.
 */
static struct hd_struct *add_partition(struct gendisk *disk, int partno,
				sector_t start, sector_t len, int flags,
				struct partition_meta_info *info)
{
	struct hd_struct *p;
	dev_t devt = MKDEV(0, 0);
	struct device *ddev = disk_to_dev(disk);
	struct device *pdev;
	struct disk_part_tbl *ptbl;
	const char *dname;
	int err;

	// 1. Zoned Block Device (Zoned Namespace, ZNS)支持。ZNS是一种将SSD内部部分结构信息暴露给上层应用的机制，用户态的上层应用可以利用这些信息设计存储相关的逻辑，从而最大化SSD的性能和寿命。
	/*
	 * Partitions are not supported on zoned block devices that are used as
	 * such.
	 */
	switch (disk->queue->limits.zoned) {
	case BLK_ZONED_HM:
		pr_warn("%s: partitions not supported on host managed zoned block device\n",
			disk->disk_name);
		return ERR_PTR(-ENXIO);
	case BLK_ZONED_HA:
		pr_info("%s: disabling host aware zoned block device support due to partitions\n",
			disk->disk_name);
		disk->queue->limits.zoned = BLK_ZONED_NONE;
		break;
	case BLK_ZONED_NONE:
		break;
	}

	// 2. 扩张分区表。
	err = disk_expand_part_tbl(disk, partno);
	if (err)
		return ERR_PTR(err);
	ptbl = rcu_dereference_protected(disk->part_tbl, 1);

	// 3. 如果本次期望添加的分区的目标分区号已经被使用，则报错。
	if (ptbl->part[partno])
		return ERR_PTR(-EBUSY);

	// 4. 为当前分区分配struct hd_struct。
	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return ERR_PTR(-EBUSY);

	// 5. Disk统计信息的内存分配。
	p->dkstats = alloc_percpu(struct disk_stats);
	if (!p->dkstats) {
		err = -ENOMEM;
		goto out_free;
	}

	// 6. 初始化nr_sects_seq。
	// static inline void hd_sects_seq_init(struct hd_struct *p)
	// {
	// #if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	// 	seqcount_init(&p->nr_sects_seq);
	// #endif
	// }
	hd_sects_seq_init(p);

	// 7. 获取当前hd_struct对应的dev
	// #define part_to_dev(part)	(&((part)->__dev))
	pdev = part_to_dev(p);

	// 8. 设置一些信息，这里比较直观，不过多介绍。
	p->start_sect = start;
	p->alignment_offset =
		queue_limit_alignment_offset(&disk->queue->limits, start);
	p->discard_alignment =
		queue_limit_discard_alignment(&disk->queue->limits, start);
	p->nr_sects = len;
	p->partno = partno;
	p->policy = get_disk_ro(disk);

	// 9. 复制上层函数传入的分区meta info，其实就是UUID和卷标。
	if (info) {
		struct partition_meta_info *pinfo;

		pinfo = kzalloc_node(sizeof(*pinfo), GFP_KERNEL, disk->node_id);
		if (!pinfo) {
			err = -ENOMEM;
			goto out_free_stats;
		}
		memcpy(pinfo, info, sizeof(*info));
		p->info = pinfo;
	}

	// 10. 设置当前hd_struct对应的设备的名称。
	dname = dev_name(ddev);
	if (isdigit(dname[strlen(dname) - 1]))
		dev_set_name(pdev, "%sp%d", dname, partno);
	else
		dev_set_name(pdev, "%s%d", dname, partno);

	// 11. 初始化当前hd_struct对应的设备。
	device_initialize(pdev);
	pdev->class = &block_class;
	pdev->type = &part_type;
	pdev->parent = ddev;

	// 12. 分配设备号。
	err = blk_alloc_devt(p, &devt);
	if (err)
		goto out_free_info;
	pdev->devt = devt;

	// 13. 暂时停止对此hd_struct对应设备的uevent报告。
	/* delay uevent until 'holders' subdir is created */
	dev_set_uevent_suppress(pdev, 1);

	// 14. 将当前设备添加的到device hierarchy中。
	err = device_add(pdev);
	if (err)
		goto out_put;

	// 15. 添加holders。
	err = -ENOMEM;
	p->holder_dir = kobject_create_and_add("holders", &pdev->kobj);
	if (!p->holder_dir)
		goto out_del;

	// 16. 回复对hd_struct对应设备的uevent报告。
	dev_set_uevent_suppress(pdev, 0);

	// 17. ADDPART_FLAG_WHOLEDISK相关，没有查到与此相关的文档，但是大多数块设备和分区在初始化时不会设定此flag。
	if (flags & ADDPART_FLAG_WHOLEDISK) {
		err = device_create_file(pdev, &dev_attr_whole_disk);
		if (err)
			goto out_del;
	}

	// 18. 初始化hd_struct的reference count。
	err = hd_ref_init(p);
	if (err) {
		if (flags & ADDPART_FLAG_WHOLEDISK)
			goto out_remove_file;
		goto out_del;
	}

	// 19. 将当前的hd_struct加入到gendisk的分区表中。
	/* everything is up and running, commence */
	rcu_assign_pointer(ptbl->part[partno], p);

	// 20. 报告当前hd_struct对应的设备的kobject的KOBJ_ADD事件。
	/* suppress uevent if the disk suppresses it */
	if (!dev_get_uevent_suppress(ddev))
		kobject_uevent(&pdev->kobj, KOBJ_ADD);

	// 21. 返回。
	return p;

out_free_info:
	kfree(p->info);
out_free_stats:
	free_percpu(p->dkstats);
out_free:
	kfree(p);
	return ERR_PTR(err);
out_remove_file:
	device_remove_file(pdev, &dev_attr_whole_disk);
out_del:
	kobject_put(p->holder_dir);
	device_del(pdev);
out_put:
	put_device(pdev);
	return ERR_PTR(err);
}
```

## 8. IO统计
这一部分主要是通过访问`struct hd_struct`中的`dkstats`实现的。它会在后续的文章中详述。    