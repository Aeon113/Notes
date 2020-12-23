TODO: `bd_get()`, `blkdev_get()`、`blkdev_put()`。

# `struct block_device`
每个`struct hd_struct`都拥有一个`struct block_device`，相对应的`struct hd_struct`和`struct block_device`拥有相同的`dev_t`(不是真正的设备号，它只被用来索引块设备和分区)，可以通过`bdget(part_devt(part))`来找到一个`struct hd_struct`对应的`struct block_device`(如果它不存在，那么这个语句也将会自动创建它)。   
`struct block_device`负责将`struct hd_struct`与其他部分，比如bdi和文件系统，链接起来，它们通过`struct block_device`来确定一个具体的块设备/分区。    

当前版本的`struct gendisk`、`struct hd_struct`、`struct block_device`耦合性过强，这个问题在5.9中得到了改善。因此，下方列出的`struct block_device`的fields中，部分在5.9中已被删除。     

## 1. `struct block_device`
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
| bd_write_holder | 部分设备在exclusive access时，如果指定了write权限，则需要block住此设备的uevent上报。这个field和此相关。 |
| bd_holder_disks | md/dm相关。用于保存当前分区/设备的holder。 |
| bd_contains | 指向"contain"当前块设备的块设备。如果当前`struct block_device`是一个完整的块设备，那么它的`bd_contains`将指向它自己；而如果当前`struct block_device`是一个分区，那么`bd_contains`将指向它所处的块设备对应的`struct block_device`。 |
| bd_block_size | 指当前设备的物理块大小，通常hdd为512，ssd为4096。 |
| bd_partno | 当前`struct block_device`对应的`struct hd_struct`在`struct gendisk`的分区表中的index。0代表此`struct block_device`代指整个磁盘，其他指代指分区。 |
| bd_part | 指向当前`struct block_device`对应的`struct hd_struct`。 |
| bd_part_count |  |
| bd_invalidated | 表示此block device在内存中的缓存、分区表是否都已经失效。通常在驱动检测到磁盘内容发生变化，或注册新块设备、删除块设备时，将此值设为1。 |
| bd_disk | 当前bdev对应的分区/磁盘的gendisk。 |
| bd_queue | 指向所在gendisk的request queue。 |
| bd_bdi | BDI相关，不了解它的作用。 |
| bd_head | 用于将当前bdev嵌入链表`all_bdevs`中。 |
| bd_private | Private data，用法已在注释中。 |
| bd_fsfreeze_count 和 bd_fsfreeze_mutex | 与Filesystem Freeze有关。 |

## 2. `struct bdev_inode`, `struct block_device`, `bdget()`
一般，`struct block_dev`的分配和获取都是通过函数`bdget()`来完成。   

``` c
struct block_device *bdget(dev_t dev)
{
	struct block_device *bdev;
	struct inode *inode;

	inode = iget5_locked(blockdev_superblock, hash(dev),
			bdev_test, bdev_set, &dev);

	if (!inode)
		return NULL;

	bdev = &BDEV_I(inode)->bdev;

	if (inode->i_state & I_NEW) {
		bdev->bd_contains = NULL;
		bdev->bd_super = NULL;
		bdev->bd_inode = inode;
		bdev->bd_block_size = i_blocksize(inode);
		bdev->bd_part_count = 0;
		bdev->bd_invalidated = 0;
		inode->i_mode = S_IFBLK;
		inode->i_rdev = dev;
		inode->i_bdev = bdev;
		inode->i_data.a_ops = &def_blk_aops;
		mapping_set_gfp_mask(&inode->i_data, GFP_USER);
		spin_lock(&bdev_lock);
		list_add(&bdev->bd_list, &all_bdevs);
		spin_unlock(&bdev_lock);
		unlock_new_inode(inode);
	}
	return bdev;
}

EXPORT_SYMBOL(bdget);
```

此函数的逻辑很简单，先是调用`iget5_locked()`来获取/创建`struct block_device`。对于已经存在的`struct block_device`，直接将其返回；对于原先不存在的`struct block_device`，则先创建此struct，然后再将其初始化。    

这里给出相应的函数实现:    

``` c
/**
 * iget5_locked - obtain an inode from a mounted file system
 * @sb:		super block of file system
 * @hashval:	hash value (usually inode number) to get
 * @test:	callback used for comparisons between inodes
 * @set:	callback used to initialize a new struct inode
 * @data:	opaque data pointer to pass to @test and @set
 *
 * Search for the inode specified by @hashval and @data in the inode cache,
 * and if present it is return it with an increased reference count. This is
 * a generalized version of iget_locked() for file systems where the inode
 * number is not sufficient for unique identification of an inode.
 *
 * If the inode is not in cache, allocate a new inode and return it locked,
 * hashed, and with the I_NEW flag set. The file system gets to fill it in
 * before unlocking it via unlock_new_inode().
 *
 * Note both @test and @set are called with the inode_hash_lock held, so can't
 * sleep.
 */
struct inode *iget5_locked(struct super_block *sb, unsigned long hashval,
		int (*test)(struct inode *, void *),
		int (*set)(struct inode *, void *), void *data)
{
	struct inode *inode = ilookup5(sb, hashval, test, data);

	if (!inode) {
		struct inode *new = alloc_inode(sb);

		if (new) {
			new->i_state = 0;
			inode = inode_insert5(new, hashval, test, set, data);
			if (unlikely(inode != new))
				destroy_inode(new);
		}
	}
	return inode;
}
EXPORT_SYMBOL(iget5_locked);
```

``` c
/**
 * inode_insert5 - obtain an inode from a mounted file system
 * @inode:	pre-allocated inode to use for insert to cache
 * @hashval:	hash value (usually inode number) to get
 * @test:	callback used for comparisons between inodes
 * @set:	callback used to initialize a new struct inode
 * @data:	opaque data pointer to pass to @test and @set
 *
 * Search for the inode specified by @hashval and @data in the inode cache,
 * and if present it is return it with an increased reference count. This is
 * a variant of iget5_locked() for callers that don't want to fail on memory
 * allocation of inode.
 *
 * If the inode is not in cache, insert the pre-allocated inode to cache and
 * return it locked, hashed, and with the I_NEW flag set. The file system gets
 * to fill it in before unlocking it via unlock_new_inode().
 *
 * Note both @test and @set are called with the inode_hash_lock held, so can't
 * sleep.
 */
struct inode *inode_insert5(struct inode *inode, unsigned long hashval,
			    int (*test)(struct inode *, void *),
			    int (*set)(struct inode *, void *), void *data)
{
	struct hlist_head *head = inode_hashtable + hash(inode->i_sb, hashval);
	struct inode *old;
	bool creating = inode->i_state & I_CREATING;

again:
	spin_lock(&inode_hash_lock);
	old = find_inode(inode->i_sb, head, test, data);
	if (unlikely(old)) {
		/*
		 * Uhhuh, somebody else created the same inode under us.
		 * Use the old inode instead of the preallocated one.
		 */
		spin_unlock(&inode_hash_lock);
		if (IS_ERR(old))
			return NULL;
		wait_on_inode(old);
		if (unlikely(inode_unhashed(old))) {
			iput(old);
			goto again;
		}
		return old;
	}

	if (set && unlikely(set(inode, data))) {
		inode = NULL;
		goto unlock;
	}

	/*
	 * Return the locked inode with I_NEW set, the
	 * caller is responsible for filling in the contents
	 */
	spin_lock(&inode->i_lock);
	/* 注意这里，新创建的inode会在i_state中置上I_NEW，然后在bdget()中再将其初始化。 */
	inode->i_state |= I_NEW;
	hlist_add_head_rcu(&inode->i_hash, head);
	spin_unlock(&inode->i_lock);
	if (!creating)
		inode_sb_list_add(inode);
unlock:
	spin_unlock(&inode_hash_lock);

	return inode;
}
EXPORT_SYMBOL(inode_insert5);
```

``` c
static int bdev_test(struct inode *inode, void *data)
{
	return BDEV_I(inode)->bdev.bd_dev == *(dev_t *)data;
}
```

``` c
static int bdev_set(struct inode *inode, void *data)
{
	BDEV_I(inode)->bdev.bd_dev = *(dev_t *)data;
	return 0;
}
```

### 2.2. `bdget_disk()`
`bdget()`函数通过`bd_dev`来查找/创建`struct block_device`。    
很多时候，对于已经完成创建和初始化的`struct block_device`，也可以通过`struct gendisk`和分区号来调用`bdget()`。     

``` c
/**
 * bdget_disk - do bdget() by gendisk and partition number
 * @disk: gendisk of interest
 * @partno: partition number
 *
 * Find partition @partno from @disk, do bdget() on it.
 *
 * CONTEXT:
 * Don't care.
 *
 * RETURNS:
 * Resulting block_device on success, NULL on failure.
 */
struct block_device *bdget_disk(struct gendisk *disk, int partno)
{
	struct hd_struct *part;
	struct block_device *bdev = NULL;

	part = disk_get_part(disk, partno);
	if (part)
		bdev = bdget(part_devt(part));
	disk_put_part(part);

	return bdev;
}
EXPORT_SYMBOL(bdget_disk);
```

### 2.3 `struct bdev_inode`
在`iget5_locked()`中，分配inode的语句是`alloc_inode(sb)`:    

``` c
if (!inode) {
		struct inode *new = alloc_inode(sb);
		// ...
}
```

而`alloc_inode()`的定义为:   

``` c
static struct inode *alloc_inode(struct super_block *sb)
{
	const struct super_operations *ops = sb->s_op;
	struct inode *inode;

	if (ops->alloc_inode)
		inode = ops->alloc_inode(sb);
	else
		inode = kmem_cache_alloc(inode_cachep, GFP_KERNEL);

	if (!inode)
		return NULL;

	if (unlikely(inode_init_always(sb, inode))) {
		if (ops->destroy_inode) {
			ops->destroy_inode(inode);
			if (!ops->free_inode)
				return NULL;
		}
		inode->free_inode = ops->free_inode;
		i_callback(&inode->i_rcu);
		return NULL;
	}

	return inode;
}
```

可以看到，事实上对`struct block_device`进行分配的函数是`blockdev_superblock.s_op.alloc_inode()`，也就是`bdev_alloc_inode()`:    

``` c
static struct inode *bdev_alloc_inode(struct super_block *sb)
{
	struct bdev_inode *ei = kmem_cache_alloc(bdev_cachep, GFP_KERNEL);
	if (!ei)
		return NULL;
	return &ei->vfs_inode;
}
```
可见，`struct block_device`和它的对应inode，事实上是一同分配的。在得到`struct inode`和`struct block_device`中的一个之后可以通过一下函数得到另一个:   

``` c
struct bdev_inode {
	struct block_device bdev;
	struct inode vfs_inode;
};
```

``` c
static inline struct bdev_inode *BDEV_I(struct inode *inode)
{
	return container_of(inode, struct bdev_inode, vfs_inode);
}

struct block_device *I_BDEV(struct inode *inode)
{
	return &BDEV_I(inode)->bdev;
}
EXPORT_SYMBOL(I_BDEV);
```
需要注意的是`BDEV_I()`没有被export。如果驱动需要类似的逻辑，则可能需要自行实现。   

事实上，由于每一个`struct block_device`在分配时一定会伴随一个`struct inode`的分配，我们可以认为`struct block_device`将磁盘和分区与VFS连接了起来。   

## 3. `blkdev_get()`与`blkdev_put()`
在操作`struct block_device`的函数中，最重要的2个就是`blkdev_get()`和`blkdev_put()`，分别被用于disk/分区的打开/初始化和关闭/销毁。此外，直接对块设备写入raw数据、挂载块设备等等操作，也都需要先打开此块设备。    
在打开过程中，`blkdev_get()`的调用发生在`blkdev_open()`中。而在初始化块设备的过程中，`blkdev_get()`的调用发生在`add_disk()`->`device_add_disk()`->`__device_add_disk()`->`register_disk()`里。需要注意的是，在初始化时，目标`struct block_device`对应的是整个块设备而非分区，且此时，`struct block_device`并未完成初始化，且其中的`bd_invalidated`为1，表明此`struct block_device`内的数据是无效的。     

``` c
static int blkdev_open(struct inode * inode, struct file * filp)
{
	struct block_device *bdev;

	/*
	 * Preserve backwards compatibility and allow large file access
	 * even if userspace doesn't ask for it explicitly. Some mkfs
	 * binary needs it. We might want to drop this workaround
	 * during an unstable branch.
	 */
	filp->f_flags |= O_LARGEFILE;

	filp->f_mode |= FMODE_NOWAIT;

	if (filp->f_flags & O_NDELAY)
		filp->f_mode |= FMODE_NDELAY;
	if (filp->f_flags & O_EXCL)
		filp->f_mode |= FMODE_EXCL;
	if ((filp->f_flags & O_ACCMODE) == 3)
		filp->f_mode |= FMODE_WRITE_IOCTL;

	bdev = bd_acquire(inode);
	if (bdev == NULL)
		return -ENOMEM;

	filp->f_mapping = bdev->bd_inode->i_mapping;
	filp->f_wb_err = filemap_sample_wb_err(filp->f_mapping);

	return blkdev_get(bdev, filp->f_mode, filp);
}
```

``` c
static void register_disk(struct device *parent, struct gendisk *disk,
			  const struct attribute_group **groups)
{
	// ...
	bdev->bd_invalidated = 1;
	err = blkdev_get(bdev, FMODE_READ, NULL);
	if (err < 0)
		goto exit;
	blkdev_put(bdev, FMODE_READ);
	// ...
}
```

### 3.1 `blkdev_get()`

这个函数有3个参数:

| Parameter | Comment |
|----------:|:--------|
| bdev | 目标块设备 |
| mode | 打开mode。通常由上层逻辑，例如文件系统，用户程序等，来指定。在初始化时，这里的值为`FMODE_READ`。 |
| holder | 用于exclusive access。在exclusive access时，需要给这个参数一个non-NULL值，用于标识当前opener。 |
``` c
/**
 * blkdev_get - open a block device
 * @bdev: block_device to open
 * @mode: FMODE_* mask
 * @holder: exclusive holder identifier
 *
 * Open @bdev with @mode.  If @mode includes %FMODE_EXCL, @bdev is
 * open with exclusive access.  Specifying %FMODE_EXCL with %NULL
 * @holder is invalid.  Exclusive opens may nest for the same @holder.
 *
 * On success, the reference count of @bdev is unchanged.  On failure,
 * @bdev is put.
 *
 * CONTEXT:
 * Might sleep.
 *
 * RETURNS:
 * 0 on success, -errno on failure.
 */
int blkdev_get(struct block_device *bdev, fmode_t mode, void *holder)
{
	struct block_device *whole = NULL;
	int res;

// 1. 如果请求exclusive access，则holder必须不为NULL。
	WARN_ON_ONCE((mode & FMODE_EXCL) && !holder);

// 2. 对于exclusive access，先执行bd_start_claiming()。
//    Claiming 相关的部分会在下一节详述。
	if ((mode & FMODE_EXCL) && holder) {
		whole = bd_start_claiming(bdev, holder);
		if (IS_ERR(whole)) {
			bdput(bdev);
			return PTR_ERR(whole);
		}
	}

// 3. 主要逻辑。后面会详述这里。
	res = __blkdev_get(bdev, mode, 0);

// 4. 继续exclusive access相关的处理。
	if (whole) {
		struct gendisk *disk = whole->bd_disk;

		/* finish claiming */
		mutex_lock(&bdev->bd_mutex);
		if (!res)
			bd_finish_claiming(bdev, whole, holder);
		else
			bd_abort_claiming(bdev, whole, holder);
		/*
		 * Block event polling for write claims if requested.  Any
		 * write holder makes the write_holder state stick until
		 * all are released.  This is good enough and tracking
		 * individual writeable reference is too fragile given the
		 * way @mode is used in blkdev_get/put().
		 */
		if (!res && (mode & FMODE_WRITE) && !bdev->bd_write_holder &&
		    (disk->flags & GENHD_FL_BLOCK_EVENTS_ON_EXCL_WRITE)) {
			bdev->bd_write_holder = true;
			disk_block_events(disk);
		}

		mutex_unlock(&bdev->bd_mutex);
		bdput(whole);
	}

// 5. 如果失败，直接调用bdput()，其将执行iput(bdev->bd_inode)。
	if (res)
		bdput(bdev);

// 6. 返回__blkdev_get()的结果。
	return res;
}
EXPORT_SYMBOL(blkdev_get);
```

这个功能的主要逻辑在`__blkdev_get()`中。    

`__blkdev_get()`有3个参数:   

| Parameter | Comment |
|----------:|:--------|
| bdev | 目标`struct block_device` |
| mode | 打开mode |
| for_part | 这个值只有在此函数递归时才有概率为1，其他任何时候caller都会传0。具体最用我们将在下方分析。 |

``` c
/*
 * bd_mutex locking:
 *
 *  mutex_lock(part->bd_mutex)
 *    mutex_lock_nested(whole->bd_mutex, 1)
 */

static int __blkdev_get(struct block_device *bdev, fmode_t mode, int for_part)
{
	struct gendisk *disk;
	int ret;
	int partno;
	int perm = 0;
	bool first_open = false;

// 1. 设置perm。
	if (mode & FMODE_READ)
		perm |= MAY_READ;
	if (mode & FMODE_WRITE)
		perm |= MAY_WRITE;

// 2. 对于for_part为0的情况，进行cgroup相关的操作。
//    这里是perm这个变量唯一需要被用到的地方。
	/*
	 * hooks: /n/, see "layering violations".
	 */
	if (!for_part) {
		ret = devcgroup_inode_permission(bdev->bd_inode, perm);
		if (ret != 0)
			return ret;
	}

// 3. restart标号，主要逻辑开始的地方。
 restart:

	ret = -ENXIO;

// 4. 获取当前bdev所在的struct gendisk，以及分区号。
	disk = bdev_get_gendisk(bdev, &partno);
	if (!disk)
		goto out;

// 5. 暂时组织当前gendisk报告uevent。
	disk_block_events(disk);

// 6. 锁。
	mutex_lock_nested(&bdev->bd_mutex, for_part);

// 7. 以下是一长串if-else嵌套，主要判断2个条件的组合: 目标是整个gendisk还是一个分区；此次是否是第一次打开此块设备。

// 7.I. 第一次打开此块设备的情况。
//      对于gendisk，因为在注册块设备时就会调用此函数，所以此时会执行此分支。
//      但对于分区，似乎只会在有其他外部逻辑尝试打开时才会执行这里。我没有发现注册gendisk时为它们调用blkdev_get()或bdget()的代码，也就是说不会在此时初始化这些分区对应的struct block_device。
	if (!bdev->bd_openers) {

	// 7.I.A 第一次打开，所以设置局部变量first_open为true。
		first_open = true;

	// 7.I.B 简单初始化。注意bd_contains，这个值暂时赋成了当前bdev。如果当前bdev是一个分区，那么后面会把这个量修改为其所在gendisk对应的bdev。
		bdev->bd_disk = disk;
		bdev->bd_queue = disk->queue;
		bdev->bd_contains = bdev;
		bdev->bd_partno = partno;

	// 7.I.C 如果是第一次打开此块设备，且当前bdev对应的是整个gendisk。
		if (!partno) {
			ret = -ENXIO;

		// 7.I.C.i 获取当前bdev对应的struct hd_struct。
			bdev->bd_part = disk_get_part(disk, partno);
			if (!bdev->bd_part)
				goto out_clear;

		// 7.I.C.ii 如果当前块设备的驱动提供了open函数，则调用此函数。
		//          如果函数返回了-ERESTARTSYS (代表open函数被中断或睡眠打断，consistency不能保证，需要重新执行)，则undo此前对bdev的初始化操作，回到第3步重新开始。
			ret = 0;
			if (disk->fops->open) {
				ret = disk->fops->open(bdev, mode);
				if (ret == -ERESTARTSYS) {
					/* Lost a race with 'disk' being
					 * deleted, try again.
					 * See md.c
					 */
					disk_put_part(bdev->bd_part);
					bdev->bd_part = NULL;
					bdev->bd_disk = NULL;
					bdev->bd_queue = NULL;
					mutex_unlock(&bdev->bd_mutex);
					disk_unblock_events(disk);
					put_disk_and_module(disk);
					goto restart;
				}
			}

		// 7.I.C.iii 在第7.I.C.ii没有出错的情况下，调用bd_set_size()和set_init_blocksize()。
		//           get_capacity()获取当前gendisk的sectors数量，bd_set_size()设置
		//           当前块设备对应的inode中存储的size信息，set_init_blocksize()设置
		//           设置当前设备的block size。
		//
		//           值得注意的是，可以看到，set_init_blocksize()的代码中，有尝试扩大
		//           block size的逻辑，最大可以扩大到一个page size的大小。
		//           在后续的内核版本中，这段代码被删掉了，可能是因为没有带来什么性能提升。
			if (!ret) {
				bd_set_size(bdev,(loff_t)get_capacity(disk)<<9);
				set_init_blocksize(bdev);
			}

		// 7.I.C.iv 这里看注释就能理解了。
		//          需要注意的是，bd_invalidated只有在在块设备注册时、销毁时，或其他逻辑，
		//          例如驱动，需要重新扫描磁盘前，都会将此值置1，表示在内存中的缓存、分区表都已经失效。
		//          bdev_disk_changed()将在接下来介绍。注意其第二个参数，可以参考
		//          https://stackoverflow.com/questions/39209485/meaning-of-errno-constant-emediumtype ，一般此值都为false。
			/*
			 * If the device is invalidated, rescan partition
			 * if open succeeded or failed with -ENOMEDIUM.
			 * The latter is necessary to prevent ghost
			 * partitions on a removed medium.
			 */
			if (bdev->bd_invalidated &&
			    (!ret || ret == -ENOMEDIUM))
				bdev_disk_changed(bdev, ret == -ENOMEDIUM);

			if (ret)
				goto out_clear;
		}

	// 7.I.D 如果是第一次打开此块设备，且此块设备对应的不是整个gendisk而是一个分区。
		else {
			struct block_device *whole;

		// 7.I.D.i 获取当前分区所在的gendisk对应的struct block_device，放入局部变量whole。
		//         注意此时whole已经完全初始化完成。
			whole = bdget_disk(disk, 0);
			ret = -ENOMEM;
			if (!whole)
				goto out_clear;

			BUG_ON(for_part);

		// 7.I.D.ii 对whole递归一次__blkdev_get()，注意此时for_part已经是1。
			ret = __blkdev_get(whole, mode, 1);
			if (ret) {
				bdput(whole);
				goto out_clear;
			}

		// 7.I.D.iii 赋值流程。
		//
		//           注意bd_contains，和第7.I.B对比，我们可以总结出bd_contains在初始化结束后，一定
		//           指向其所在gendisk对应的struct hd_struct。
			bdev->bd_contains = whole;
			bdev->bd_part = disk_get_part(disk, partno);

		// 7.I.D.iv 如果当前gendisk不可用，或者当前struct block_device没有对应的struct hd_struct，
		//          或者当前分区的大小为0，则准备报错退出。
			if (!(disk->flags & GENHD_FL_UP) ||
			    !bdev->bd_part || !bdev->bd_part->nr_sects) {
				ret = -ENXIO;
				goto out_clear;
			}

		// 7.I.D.v 和7.I.C.iii一样，设置size和block size。
			bd_set_size(bdev, (loff_t)bdev->bd_part->nr_sects << 9);
			set_init_blocksize(bdev);
		}

	// 7.I.E BDI相关，这里不做介绍。
		if (bdev->bd_bdi == &noop_backing_dev_info)
			bdev->bd_bdi = bdi_get(disk->queue->backing_dev_info);
	}
// 7.II 不是第一次打开此块设备的情况。
	else {
	// 7.II.A 此块设备不是第一次打开，且其并非指向分区，而是指向整个块设备。
		if (bdev->bd_contains == bdev) {
			ret = 0;

		// 7.II.B 与7.I.C.ii一样，如果驱动提供了open函数，则为此块设备调用一次。
			if (bdev->bd_disk->fops->open)
				ret = bdev->bd_disk->fops->open(bdev, mode);

		// 7.II.C 与7.I.C.iv一样，符合相应条件时，调用一次bdev_disk_changed()。
			/* the same as first opener case, read comment there */
			if (bdev->bd_invalidated &&
			    (!ret || ret == -ENOMEDIUM))
				bdev_disk_changed(bdev, ret == -ENOMEDIUM);
			if (ret)
				goto out_unlock_bdev;
		}
	}

// 8. 递增bd_openers，所以这里可以总结出，bd_openers指当前块设备被打开的次数减去被关闭的次数。
	bdev->bd_openers++;

// 9. 如果传入的参数for_part不为0，则递增bd_part_count。
//    所以，这里可以总结出，bd_part_count指当前块设备中，已经被打开的分区的数量。
	if (for_part)
		bdev->bd_part_count++;

// 10. 释放锁。
	mutex_unlock(&bdev->bd_mutex);

// 11. 停止events blocking。
	disk_unblock_events(disk);

// 12. 如果不是第一次打开此gendisk/分区，则调用put_disk_and_module()，减少引用计数。
	/* only one opener holds refs to the module and disk */
	if (!first_open)
		put_disk_and_module(disk);
	return 0;

 out_clear:
	disk_put_part(bdev->bd_part);
	bdev->bd_disk = NULL;
	bdev->bd_part = NULL;
	bdev->bd_queue = NULL;
	if (bdev != bdev->bd_contains)
		__blkdev_put(bdev->bd_contains, mode, 1);
	bdev->bd_contains = NULL;
 out_unlock_bdev:
	mutex_unlock(&bdev->bd_mutex);
	disk_unblock_events(disk);
	put_disk_and_module(disk);
 out:

	return ret;
}
```

``` c
int bdev_disk_changed(struct block_device *bdev, bool invalidate)
{
	struct gendisk *disk = bdev->bd_disk;
	int ret;

// 1. 锁debug相关。
	lockdep_assert_held(&bdev->bd_mutex);

// 2. 标号rescan，具体逻辑开始的位置。
rescan:

// 3. blk_drop_partitions()，在disk_part_scan_enabled(bdev->bd_disk)为true和
//    bdev->bd_part_count为0的情况下，会做2件事情: 刷入并释放此bdev上的所有cache，然后对其所在gendisk
//    上的所有hd_struct (不包括part0)，调用delete_partition()。
	ret = blk_drop_partitions(bdev);
	if (ret)
		return ret;

// 4. 这里我实在找不到有什么条件会导致传入的参数invalidate为true。
//    所以我们假设这个值不会为true吧。这样这里只会调用驱动指定的revalidate_disk回调函数。
//    revalidate_disk可以参考LDD(https://static.lwn.net/images/pdf/LDD3/ch16.pdf)。
	/*
	 * Historically we only set the capacity to zero for devices that
	 * support partitions (independ of actually having partitions created).
	 * Doing that is rather inconsistent, but changing it broke legacy
	 * udisks polling for legacy ide-cdrom devices.  Use the crude check
	 * below to get the sane behavior for most device while not breaking
	 * userspace for this particular setup.
	 */
	if (invalidate) {
		if (disk_part_scan_enabled(disk) ||
		    !(disk->flags & GENHD_FL_REMOVABLE))
			set_capacity(disk, 0);
	} else {
		if (disk->fops->revalidate_disk)
			disk->fops->revalidate_disk(disk);
	}

// 5. 这个函数的定义会放在后面。
	check_disk_size_change(disk, bdev, !invalidate);

// 6. 如果此gendisk的容量不为0，则为其调用blk_add_partitions()，此函数已在hd_struct的文章中介绍。
//    因为没找到invalidate什么情况下会为true，所以这里不介绍else if中的情况了。
	if (get_capacity(disk)) {
		ret = blk_add_partitions(disk, bdev);
		if (ret == -EAGAIN)
			goto rescan;
	} else if (invalidate) {
		/*
		 * Tell userspace that the media / partition table may have
		 * changed.
		 */
		kobject_uevent(&disk_to_dev(disk)->kobj, KOBJ_CHANGE);
	}

	return ret;
}
/*
 * Only exported for for loop and dasd for historic reasons.  Don't use in new
 * code!
 */
EXPORT_SYMBOL_GPL(bdev_disk_changed);
```

``` c
/**
 * check_disk_size_change - checks for disk size change and adjusts bdev size.
 * @disk: struct gendisk to check
 * @bdev: struct bdev to adjust.
 * @verbose: if %true log a message about a size change if there is any
 *
 * This routine checks to see if the bdev size does not match the disk size
 * and adjusts it if it differs. When shrinking the bdev size, its all caches
 * are freed.
 */
static void check_disk_size_change(struct gendisk *disk,
		struct block_device *bdev, bool verbose)
{
	loff_t disk_size, bdev_size;

	disk_size = (loff_t)get_capacity(disk) << 9;
	bdev_size = i_size_read(bdev->bd_inode);
	if (disk_size != bdev_size) {
		if (verbose) {
			printk(KERN_INFO
			       "%s: detected capacity change from %lld to %lld\n",
			       disk->disk_name, bdev_size, disk_size);
		}
		i_size_write(bdev->bd_inode, disk_size);
		if (bdev_size > disk_size)
			flush_disk(bdev, false);
	}
	bdev->bd_invalidated = 0;
}
```

### 3.2 `blkdev_put()`
`blkdev_put()`是`blkdev_get()`的反操作。这里可以对照`blkdev_get()`来理解。   

``` c
void blkdev_put(struct block_device *bdev, fmode_t mode)
{
	mutex_lock(&bdev->bd_mutex);

// 1. Claiming相关。
	if (mode & FMODE_EXCL) {
		bool bdev_free;

		/*
		 * Release a claim on the device.  The holder fields
		 * are protected with bdev_lock.  bd_mutex is to
		 * synchronize disk_holder unlinking.
		 */
		spin_lock(&bdev_lock);

		WARN_ON_ONCE(--bdev->bd_holders < 0);
		WARN_ON_ONCE(--bdev->bd_contains->bd_holders < 0);

		/* bd_contains might point to self, check in a separate step */
		if ((bdev_free = !bdev->bd_holders))
			bdev->bd_holder = NULL;
		if (!bdev->bd_contains->bd_holders)
			bdev->bd_contains->bd_holder = NULL;

		spin_unlock(&bdev_lock);

		/*
		 * If this was the last claim, remove holder link and
		 * unblock evpoll if it was a write holder.
		 */
		if (bdev_free && bdev->bd_write_holder) {
			disk_unblock_events(bdev->bd_disk);
			bdev->bd_write_holder = false;
		}
	}

// 2. 报告uevent。
	/*
	 * Trigger event checking and tell drivers to flush MEDIA_CHANGE
	 * event.  This is to ensure detection of media removal commanded
	 * from userland - e.g. eject(1).
	 */
	disk_flush_events(bdev->bd_disk, DISK_EVENT_MEDIA_CHANGE);

	mutex_unlock(&bdev->bd_mutex);

// 3. 主要逻辑。
	__blkdev_put(bdev, mode, 0);
}
EXPORT_SYMBOL(blkdev_put);
```

``` c
static void __blkdev_put(struct block_device *bdev, fmode_t mode, int for_part)
{
	struct gendisk *disk = bdev->bd_disk;
	struct block_device *victim = NULL;

// 1. 如果当前设备只有一个opener，则本次put后将一个都不剩了(即在第3步bd_openers将减至0)。
//    因此sync一次数据。
	/*
	 * Sync early if it looks like we're the last one.  If someone else
	 * opens the block device between now and the decrement of bd_openers
	 * then we did a sync that we didn't need to, but that's not the end
	 * of the world and we want to avoid long (could be several minute)
	 * syncs while holding the mutex.
	 */
	if (bdev->bd_openers == 1)
		sync_blockdev(bdev);

	mutex_lock_nested(&bdev->bd_mutex, for_part);

// 2. for_part相关，可以参考__blkdev_get()中for_part相关的处理逻辑。
	if (for_part)
		bdev->bd_part_count--;

// 3. 令bd_openers递减。
//    如果bd_openers减为了0，则再sync一次数据，然后调用kill_bdev()，清除此块设备
//    上的所有cache。
//    至于bdev_write_inode()，不知道作用是什么。
	if (!--bdev->bd_openers) {
		WARN_ON_ONCE(bdev->bd_holders);
		sync_blockdev(bdev);
		kill_bdev(bdev);

		bdev_write_inode(bdev);
	}

// 4. 如果当前bdev不是分区而是整个块设备，则调用驱动指定的release回调。
	if (bdev->bd_contains == bdev) {
		if (disk->fops->release)
			disk->fops->release(disk, mode);
	}

// 5. 继续销毁bdev。
//    注意，当当前bdev指向的块设备为分区时(即bdev != bdev->bd_contains)时，会设置victim，起降在第7步被使用。
	if (!bdev->bd_openers) {
		disk_put_part(bdev->bd_part);
		bdev->bd_part = NULL;
		bdev->bd_disk = NULL;
		if (bdev != bdev->bd_contains)
			victim = bdev->bd_contains;
		bdev->bd_contains = NULL;

		put_disk_and_module(disk);
	}
	mutex_unlock(&bdev->bd_mutex);

// 6. bdput()。
	bdput(bdev);

// 7. 如果第5步获得了victim，则为victim也调用一次__blkdev_put()。
//    而此次调用，for_part将被置1。
	if (victim)
		__blkdev_put(victim, mode, 1);
}
```

## 4. Claiming
Claiming (exclusive access)是Linux提供的一种块设备访问机制。一个块设备在正被exclusive access的期间，其他试图exclusive access此块设备的尝试都将失败(但non-exclusive access会成功)。    

尝试exclusive access的逻辑，需要在open函数的fmode中指定`FMOVE_EXCL`。   

Claiming的主要代码逻辑在`blkdev_get()`和`blkdev_put()`。    

### 4.1 开始claiming
这个过程发生在`blkdev_get()`中。
``` c
int blkdev_get(struct block_device *bdev, fmode_t mode, void *holder)
{

// 1. 如果需要exclusive access，则必须指定一个非NULL的holder。
	WARN_ON_ONCE((mode & FMODE_EXCL) && !holder);

// 2. 调用bd_start_claiming()，并将其返回值存为whole。
	if ((mode & FMODE_EXCL) && holder) {
		whole = bd_start_claiming(bdev, holder);
		if (IS_ERR(whole)) {
			bdput(bdev);
			return PTR_ERR(whole);
		}
	}

// 3. 继续打开过程的其他逻辑。
	res = __blkdev_get(bdev, mode, 0);

// 4. 如果第3步失败，则
//      I. 获取目标的bd_mutex。
//      II. 调用bd_abort_claiming()。
//      III. 释放bd_mutex。
//      IV. bdput(whole)(对应的bdget_disk()/bdgrab()在bd_start_claiming()中)。
//    如果成功，则:
//      I. 获取目标的bd_mutex。
//      II. 调用bd_finish_claiming()。
//      III. 在exclusive access时，如果指定了write权限，则需要block住此设备的uevent报告。这里的bd_write_holder和disk_block_events()的调用和这个有关。
//      IV. 释放bd_mutex。
//      V. bdput(whole)。
	if (whole) {
		struct gendisk *disk = whole->bd_disk;

		/* finish claiming */
		mutex_lock(&bdev->bd_mutex);
		if (!res)
			bd_finish_claiming(bdev, whole, holder);
		else
			bd_abort_claiming(bdev, whole, holder);
		/*
		 * Block event polling for write claims if requested.  Any
		 * write holder makes the write_holder state stick until
		 * all are released.  This is good enough and tracking
		 * individual writeable reference is too fragile given the
		 * way @mode is used in blkdev_get/put().
		 */
		if (!res && (mode & FMODE_WRITE) && !bdev->bd_write_holder &&
		    (disk->flags & GENHD_FL_BLOCK_EVENTS_ON_EXCL_WRITE)) {
			bdev->bd_write_holder = true;
			disk_block_events(disk);
		}

		mutex_unlock(&bdev->bd_mutex);
		bdput(whole);
	}

	if (res)
		bdput(bdev);

	return res;
}
```

这里牵涉到3个函数，`bd_start_claiming()`，`bd_finish_claiming()`和`bd_abort_claiming()`。    
`bd_start_claiming()`检查是否可以exclusive open，如果不能则返回失败；否则则设置目标bdev的`bd_claiming`，阻止后续其他线程/逻辑的claiming。
然后，`blkdev_get()`将继续其他的打开逻辑(即调用`__blkdev_get()`)。
如果其他的打开逻辑失败(`__blkdev_get()`返回失败)，则必须调用`bd_abort_claiming()`；如果成功，则调用`bd_finish_claiming()`完成claiming过程。

``` c
/**
 * bd_start_claiming - start claiming a block device
 * @bdev: block device of interest
 * @holder: holder trying to claim @bdev
 *
 * @bdev is about to be opened exclusively.  Check @bdev can be opened
 * exclusively and mark that an exclusive open is in progress.  Each
 * successful call to this function must be matched with a call to
 * either bd_finish_claiming() or bd_abort_claiming() (which do not
 * fail).
 *
 * This function is used to gain exclusive access to the block device
 * without actually causing other exclusive open attempts to fail. It
 * should be used when the open sequence itself requires exclusive
 * access but may subsequently fail.
 *
 * CONTEXT:
 * Might sleep.
 *
 * RETURNS:
 * Pointer to the block device containing @bdev on success, ERR_PTR()
 * value on failure.
 */
struct block_device *bd_start_claiming(struct block_device *bdev, void *holder)
{
	struct gendisk *disk;
	struct block_device *whole;
	int partno, err;

	might_sleep();

// 1. 获取当前bdev所在的gendisk和对应的分区号。
	/*
	 * @bdev might not have been initialized properly yet, look up
	 * and grab the outer block device the hard way.
	 */
	disk = bdev_get_gendisk(bdev, &partno);
	if (!disk)
		return ERR_PTR(-ENXIO);

// 2. 调用bdget_disk()/bdgrab()，获取当前bdev所在gendisk的struct block_device。
	/*
	 * Normally, @bdev should equal what's returned from bdget_disk()
	 * if partno is 0; however, some drivers (floppy) use multiple
	 * bdev's for the same physical device and @bdev may be one of the
	 * aliases.  Keep @bdev if partno is 0.  This means claimer
	 * tracking is broken for those devices but it has always been that
	 * way.
	 */
	if (partno)
		whole = bdget_disk(disk, 0);
	else
		whole = bdgrab(bdev);

// 3. 对gendisk的put操作。
	put_disk_and_module(disk);
	if (!whole)
		return ERR_PTR(-ENOMEM);

// 4. 获取bdev_lock，开始准备claiming。
	/* prepare to claim, if successful, mark claiming in progress */
	spin_lock(&bdev_lock);

// 5. Claiming的第一步，bd_prepare_to_claim()。
//    注意虽然第4步开始进入了critical area，但这个函数可能会把锁解开然后sleep。
	err = bd_prepare_to_claim(bdev, whole, holder);

// 6. 如果第5步成功，代表可以正常继续claiming过程，这里会将whole (注意，是whole，不是bdev)的bd_claiming置5为holder，然后释放bdev_lock，再返回whole。
//    如果第5步失败，则直接释放锁，然后bdput(whole，然后返回错误。
	if (err == 0) {
		whole->bd_claiming = holder;
		spin_unlock(&bdev_lock);
		return whole;
	} else {
		spin_unlock(&bdev_lock);
		bdput(whole);
		return ERR_PTR(err);
	}
}
EXPORT_SYMBOL(bd_start_claiming);
```

可以看到，`bd_start_claiming()`中有一个`bd_prepare_to_claim()`:   

``` c
/**
 * bd_may_claim - test whether a block device can be claimed
 * @bdev: block device of interest
 * @whole: whole block device containing @bdev, may equal @bdev
 * @holder: holder trying to claim @bdev
 *
 * Test whether @bdev can be claimed by @holder.
 *
 * CONTEXT:
 * spin_lock(&bdev_lock).
 *
 * RETURNS:
 * %true if @bdev can be claimed, %false otherwise.
 */
static bool bd_may_claim(struct block_device *bdev, struct block_device *whole,
			 void *holder)
{
	if (bdev->bd_holder == holder)
		return true;	 /* already a holder */
	else if (bdev->bd_holder != NULL)
		return false; 	 /* held by someone else */
	else if (whole == bdev)
		return true;  	 /* is a whole device which isn't held */

	else if (whole->bd_holder == bd_may_claim)
		return true; 	 /* is a partition of a device that is being partitioned */
	else if (whole->bd_holder != NULL)
		return false;	 /* is a partition of a held device */
	else
		return true;	 /* is a partition of an un-held device */
}

/**
 * bd_prepare_to_claim - prepare to claim a block device
 * @bdev: block device of interest
 * @whole: the whole device containing @bdev, may equal @bdev
 * @holder: holder trying to claim @bdev
 *
 * Prepare to claim @bdev.  This function fails if @bdev is already
 * claimed by another holder and waits if another claiming is in
 * progress.  This function doesn't actually claim.  On successful
 * return, the caller has ownership of bd_claiming and bd_holder[s].
 *
 * CONTEXT:
 * spin_lock(&bdev_lock).  Might release bdev_lock, sleep and regrab
 * it multiple times.
 *
 * RETURNS:
 * 0 if @bdev can be claimed, -EBUSY otherwise.
 */
static int bd_prepare_to_claim(struct block_device *bdev,
			       struct block_device *whole, void *holder)
{
retry:
	/* if someone else claimed, fail */
	if (!bd_may_claim(bdev, whole, holder))
		return -EBUSY;

	/* if claiming is already in progress, wait for it to finish */
	if (whole->bd_claiming) {
		wait_queue_head_t *wq = bit_waitqueue(&whole->bd_claiming, 0);
		DEFINE_WAIT(wait);

		prepare_to_wait(wq, &wait, TASK_UNINTERRUPTIBLE);
		spin_unlock(&bdev_lock);
		schedule();
		finish_wait(wq, &wait);
		spin_lock(&bdev_lock);
		goto retry;
	}

	/* yay, all mine */
	return 0;
}
```
`bd_prepare_to_claim()`结束后，调用`__blkdev_get()`，执行真正的打开过程。如果`__blkdev_get()`返回成功，则调用`bd_finish_claiming()`完成claiming；否则则调用`bd_abort_claiming()`。    

``` c
static void bd_clear_claiming(struct block_device *whole, void *holder)
{
	lockdep_assert_held(&bdev_lock);
	/* tell others that we're done */
	BUG_ON(whole->bd_claiming != holder);
	whole->bd_claiming = NULL;
	wake_up_bit(&whole->bd_claiming, 0);
}

/**
 * bd_finish_claiming - finish claiming of a block device
 * @bdev: block device of interest
 * @whole: whole block device (returned from bd_start_claiming())
 * @holder: holder that has claimed @bdev
 *
 * Finish exclusive open of a block device. Mark the device as exlusively
 * open by the holder and wake up all waiters for exclusive open to finish.
 */
void bd_finish_claiming(struct block_device *bdev, struct block_device *whole,
			void *holder)
{
	spin_lock(&bdev_lock);
	BUG_ON(!bd_may_claim(bdev, whole, holder));
	/*
	 * Note that for a whole device bd_holders will be incremented twice,
	 * and bd_holder will be set to bd_may_claim before being set to holder
	 */
	whole->bd_holders++;
	whole->bd_holder = bd_may_claim;
	bdev->bd_holders++;
	bdev->bd_holder = holder;
	bd_clear_claiming(whole, holder);
	spin_unlock(&bdev_lock);
}
EXPORT_SYMBOL(bd_finish_claiming);

/**
 * bd_abort_claiming - abort claiming of a block device
 * @bdev: block device of interest
 * @whole: whole block device (returned from bd_start_claiming())
 * @holder: holder that has claimed @bdev
 *
 * Abort claiming of a block device when the exclusive open failed. This can be
 * also used when exclusive open is not actually desired and we just needed
 * to block other exclusive openers for a while.
 */
void bd_abort_claiming(struct block_device *bdev, struct block_device *whole,
                       void *holder)
{
	spin_lock(&bdev_lock);
	bd_clear_claiming(whole, holder);
	spin_unlock(&bdev_lock);
}
EXPORT_SYMBOL(bd_abort_claiming);
``` 

我们总结一下claim的逻辑:   
1. 检查是否可以claim，如果不能，返回错误。
2. 如果`whole`的`bd_claiming`不为`NULL`，代表有进程正在claim当前块设备。等待其结束后，回到第一步开始执行。
3. 将`whole`的`bd_claiming`设置为caller传入的`holder`，阻止其他进程尝试claim。
4. 调用`__blkdev_get()`，开始设备的打开流程。
5. 如果第5步失败，将`whole`的`bd_claiming`置为`NULL`，终止流程。而如果其成功，则除了将`whole`的`bd_claiming`置为`NULL`，`whole->bd_holder`设为`bd_may_claim`，还会令`bdev`和`whole`的`bd_holders`自增，并将`bdev->bd_holder`设为`holder`。(所以如果`bdev`本身是指向的`hd_struct`对应了整个`gendisk`，那么`whole`和`bdev`指向的是同一个结构体，所以`bd_holders`会自增2次，而`bd_holder`也会被置为`holder`)    

### 4.2 结束claiming
在关闭设备时，可以结束claiming状态。这个过程在`blkdev_put()`中。    

``` c
void blkdev_put(struct block_device *bdev, fmode_t mode)
{
	mutex_lock(&bdev->bd_mutex);

	if (mode & FMODE_EXCL) {
		bool bdev_free;

		/*
		 * Release a claim on the device.  The holder fields
		 * are protected with bdev_lock.  bd_mutex is to
		 * synchronize disk_holder unlinking.
		 */
		spin_lock(&bdev_lock);

		WARN_ON_ONCE(--bdev->bd_holders < 0);
		WARN_ON_ONCE(--bdev->bd_contains->bd_holders < 0);

		/* bd_contains might point to self, check in a separate step */
		if ((bdev_free = !bdev->bd_holders))
			bdev->bd_holder = NULL;
		if (!bdev->bd_contains->bd_holders)
			bdev->bd_contains->bd_holder = NULL;

		spin_unlock(&bdev_lock);

		/*
		 * If this was the last claim, remove holder link and
		 * unblock evpoll if it was a write holder.
		 */
		if (bdev_free && bdev->bd_write_holder) {
			disk_unblock_events(bdev->bd_disk);
			bdev->bd_write_holder = false;
		}
	}

	// ...
}
EXPORT_SYMBOL(blkdev_put);
```