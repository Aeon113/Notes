# Chapter 14 Block I/O Layer

## Buffer Head
The buffer head is used to describe the mapping between the on-disk block and the physical in-memory buffer (which is a sequence of bytes on a specific page). Acting as a descriptor of this buffer-to-block mapping is the data structure's only role in the kernel.    

```c
// From include/linux/buffer_head.h

/*
 * Historically, a buffer_head was used to map a single block
 * within a page, and of course as the unit of I/O through the
 * filesystem and block layers.  Nowadays the basic I/O unit
 * is the bio, and buffer_heads are used for extracting block
 * mappings (via a get_block_t call), for tracking state within
 * a page (via a page_mapping) and for wrapping bio submission
 * for backward compatibility reasons (e.g. submit_bh).
 */
struct buffer_head {
	unsigned long b_state;		/* buffer state bitmap (see above) */
	struct buffer_head *b_this_page;/* circular list of page's buffers */
	struct page *b_page;		/* the page this bh is mapped to */

	sector_t b_blocknr;		/* start block number */
	size_t b_size;			/* size of mapping */
	char *b_data;			/* pointer to data within the page */

	struct block_device *b_bdev;
	bh_end_io_t *b_end_io;		/* I/O completion */
 	void *b_private;		/* reserved for b_end_io */
	struct list_head b_assoc_buffers; /* associated with another mapping */
	struct address_space *b_assoc_map;	/* mapping this buffer is
						   associated with */
	atomic_t b_count;		/* users using this buffer_head */
};
```   

Here are the values for the `b_state` bitset:   

| Status Flag | Meaning |
|:------------|--------:|
| BH_Uptodate | Buffer contains valid data. |
| BH_Dirty | Buffer is dirty. (The contents of the buffer are newer than the contents of the block on disk and therefore the buffer must eventuallybe written back to disk.) |
| BH_Lock | Buffer is undergoing disk I/O and is locked to prevent concurrent access. |
| BH_Req | Buffer is involved in an I/O request. |
| BH_Mapped | Buffer is a valid buffer mapped to an on-disk block. |
| BH_New | Buffer is newly mapped via get_block() and not yet accessed. |
| BH_Async_Read | Buffer is undergoing asynchronous read I/O via end_buffer_async_read(). |
| BH_Async_Write | Buffer is undergoing asynchronous write I/O via end_buffer_async_write(). |
| BH_Delay | Buffer does not yet have an associated on-disk block (delayed allocation). |
| BH_Boundary | Buffer forms the boundary of contiguous blocks—the next block is discontinuous. |
| BH_Write_EIO | Buffer incurred an I/O error on write. |
| BH_Ordered | Ordered write. |
| BH_Eopnotsupp | Buffer incurred a “not supported” error. |
| BH_Unwritten | Space for the buffer has been allocated on disk but the actual data has not yet been written out. |
| BH_Quiet | Suppress errors for this buffer. |   

Here may also be a flag named `BH_PrivateStart`. It is not a valid state flag but instead corresponds to the first usable bit of which other code can make use. All bit values equal to and greater thant `BH_PrivateStart` are not used by the block I/O layer proper, so these bits are safe to use by individual drivers who want to store information in the `b_state` field.   

The `b_count` field is the buffer's usage count. The value is incremented and decremented by two inline functions, both of which are defined in `<linux/buffer_head.h>`:   

```c
static inline void get_bh(struct buffer_head *bh)
{
	atomic_inc(&bh->b_count);
}

static inline void put_bh(struct buffer_head *bh) {
	atomic_dec(&bh->b_count);
}
```   

__Before maniputating a buffer head, one must increment its reference count via `get_bh()` to ensure it will not be deallocated during usage.__   

When finished with the buffer head, one may decrement the reference count via `put_bh()`.   

------------------------

## `bio`
