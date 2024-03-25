# XFS
*This part is mainly derived from wikipedia.*     

XFS is a high-performance 64-bit journaling file system created by Silicon Graphics, Inc (SGI) in 1993. It was the default file system in SGI's IRIX operating system starting with its version 5.3. XFS was ported to the Linux kernel in 2001; as of June 2014, XFS is supported by most Linux distributions, some of which use it as the default file system.    

## 01 Features

### Capacity     
XFS is a 64-bit file system and supports a maximum file system size of 8 exbibytes minus one byte (263 − 1 bytes), but limitations imposed by the host operating system can decrease this limit. 32-bit Linux systems limit the size of both the file and file system to 16 tebibytes.    

### Journaling      
Further information: Journaling file system
In modern computing, journaling is a capability which ensures consistency of data in the file system, despite any power outages or system crash that may occur. XFS provides journaling for file system metadata, where file system updates are first written to a serial journal before the actual disk blocks are updated. The journal is a circular buffer of disk blocks that is not read in normal file system operation.    

The XFS journal is limited to a maximum size of both 64 KB blocks and 128 MB,[verification needed] with the minimum size dependent upon a calculation of the file system block size and directory block size. Placing the journal on an external device larger than the maximum journal size will simply leave the extra space unused by the journal. It can be stored within the data section of the file system (as an internal log), or on a separate device to minimize disk contention.      

In XFS, the journal contains "logical" entries that describe, in a humanly understandable way, what operations are being performed (as opposed to a "physical" journal that stores a copy of the blocks modified during each operation). Journal updates are performed asynchronously to avoid a decrease in performance speed.    

In the event of a system crash, file system operations which occurred immediately prior to the crash can be reapplied and completed as recorded in the journal, which is how data stored in XFS file systems remain consistent. Recovery is performed automatically the first time the file system is mounted after the crash. The speed of recovery is independent of the size of the file system, instead depending on the amount of file system operations to be reapplied.     

Allocation groups     
XFS file systems are internally partitioned into allocation groups, which are equally sized linear regions within the file system. Files and directories can span allocation groups. Each allocation group manages its own inodes and free space separately, providing scalability and parallelism so multiple threads and processes can perform I/O operations on the same file system simultaneously.     

This architecture helps to optimize parallel I/O performance on systems with multiple processors and/or cores, as metadata updates can also be parallelized. The internal partitioning provided by allocation groups can be especially beneficial when the file system spans multiple physical devices, allowing optimal usage of throughput of the underlying storage components.     

Striped allocation     
If an XFS file system is to be created on a striped RAID array, a stripe unit can be specified when the file system is created. This maximizes throughput by ensuring that data allocations, inode allocations and the internal log (the journal) are aligned with the stripe unit.     

Extent based allocation     
Blocks used in files stored on XFS file systems are managed with variable length extents where one extent describes one or more contiguous blocks. This can shorten the list of blocks considerably, compared to file systems that list all blocks used by a file individually.     

Block-oriented file systems manage space allocation with one or more block-oriented bitmaps; in XFS, these structures are replaced with an extent oriented structure consisting of a pair of B+ trees for each file system allocation group. One of the B+ trees is indexed by the length of the free extents, while the other is indexed by the starting block of the free extents. This dual indexing scheme allows for the highly efficient allocation of free extents for file system operations.     

Variable block sizes     
The file system block size represents the minimum allocation unit. XFS allows file systems to be created with block sizes ranging between 512 bytes and 64 KB, allowing the file system to be tuned for the expected degree of usage. When many small files are expected, a small block size would typically maximize capacity, but for a system dealing mainly with large files, a larger block size can provide a performance efficiency advantage.     

Delayed allocation     
Main article: Delayed allocation
XFS makes use of lazy evaluation techniques for file allocation. When a file is written to the buffer cache, rather than allocating extents for the data, XFS simply reserves the appropriate number of file system blocks for the data held in memory. The actual block allocation occurs only when the data is finally flushed to disk. This improves the chance that the file will be written in a contiguous group of blocks, reducing fragmentation problems and increasing performance.     

Sparse files     
XFS provides a 64-bit sparse address space for each file, which allows both for very large file sizes, and for "holes" within files in which no disk space is allocated. As the file system uses an extent map for each file, the file allocation map size is kept small. Where the size of the allocation map is too large for it to be stored within the inode, the map is moved into a B+ tree which allows for rapid access to data anywhere in the 64-bit address space provided for the file.     

Extended attributes     
XFS provides multiple data streams for files; this is made possible by its implementation of extended attributes. These allow the storage of a number of name/value pairs attached to a file. Names are nul-terminated printable character strings which are up to 256 bytes in length, while their associated values can contain up to 64 KB of binary data.     

They are further subdivided into two namespaces: root and user. Extended attributes stored in the root namespace can be modified only by the superuser, while attributes in the user namespace can be modified by any user with permission to write to the file.     

Extended attributes can be attached to any kind of XFS inode, including symbolic links, device nodes, directories, etc. The attr utility can be used to manipulate extended attributes from the command line, and the xfsdump and xfsrestore utilities are aware of extended attributes, and will back up and restore their contents. Most other backup systems do not support working with extended attributes.     

Direct I/O     
For applications requiring high throughput to disk, XFS provides a direct I/O implementation that allows non-cached I/O operations to be applied directly to the userspace. Data is transferred between the buffer of the application and the disk using DMA, which allows access to the full I/O bandwidth of the underlying disk devices.     

Guaranteed-rate I/O      
The XFS guaranteed-rate I/O system provides an API that allows applications to reserve bandwidth to the filesystem. XFS dynamically calculates the performance available from the underlying storage devices, and will reserve bandwidth sufficient to meet the requested performance for a specified time. This is a feature unique to the XFS file system. Guaranteed rates can be "hard" or "soft", representing a trade off between reliability and performance; however, XFS will only allow "hard" guarantees if the underlying storage subsystem supports it. This facility is used mostly for real-time applications, such as video streaming.     

Guaranteed-rate I/O was only supported under IRIX, and required special hardware for that purpose.     

DMAPI     
XFS implemented the DMAPI interface to support Hierarchical Storage Management in IRIX. As of October 2010, the Linux implementation of XFS supported the required on-disk metadata for DMAPI implementation, but the kernel support was reportedly not usable. For some time, SGI hosted a kernel tree which included the DMAPI hooks, but this support has not been adequately maintained, although kernel developers have stated an intention to bring this support up to date.     

Snapshots     
XFS does not yet provide direct support for snapshots, as it currently expects the snapshot process to be implemented by the volume manager. Taking a snapshot of an XFS filesystem involves temporarily halting I/O to the filesystem using the xfs_freeze utility, having the volume manager perform the actual snapshot, and then resuming I/O to continue with normal operations. The snapshot can then be mounted read-only for backup purposes.     

Releases of XFS in IRIX incorporated an integrated volume manager called XLV. This volume manager has not been ported to Linux, and XFS works with standard LVM in Linux systems instead.     

In recent Linux kernels, the xfs_freeze functionality is implemented in the VFS layer, and is executed automatically when the Volume Manager's snapshot functionality is invoked. This was once a valuable advantage as the ext3 file system could not be suspended and the volume manager was unable to create a consistent "hot" snapshot to back up a heavily busy database. Fortunately this is no longer the case. Since Linux 2.6.29, the file systems ext3, ext4, GFS2 and JFS have the freeze feature as well.     

Online defragmentation     
Although the extent-based nature of XFS and the delayed allocation strategy it uses significantly improves the file system's resistance to fragmentation problems, XFS provides a filesystem defragmentation utility (xfs_fsr, short for XFS filesystem reorganizer) that can defragment the files on a mounted and active XFS filesystem.     

Online resizing      
XFS provides the xfs_growfs utility to perform online resizing of XFS file systems. XFS filesystems can be grown so long as there is remaining unallocated space on the device holding the filesystem. This feature is typically used in conjunction with volume management, as otherwise the partition holding the filesystem will need enlarging separately. XFS partitions cannot (as of August 2017) be shrunk in place, although several possible workarounds have been discussed.     

Native backup/restore utilities     
XFS provides the xfsdump and xfsrestore utilities to aid in the backup of data stored in XFS file systems. The xfsdump utility backs up an XFS filesystem in inode order, and in contrast to traditional UNIX file systems which must be unmounted before dumping to guarantee a consistent dump image, XFS file systems can be dumped while the file system is in use. This is not the same as a snapshot, since files are not frozen during the dump.     

XFS dumps and restores are also resumable and can be interrupted without difficulty. The multi-threaded operation of xfsdump provides high performance of backup operations by splitting the dump into multiple streams, which can be sent to different dump destinations. The multi-stream capabilities have not been fully ported to Linux yet, however.     

Atomic disk quotas     
Quotas for XFS filesystems are turned on when initially mounted; this fixes a race window that is present with most other filesystems that first require to be mounted and where no quotas are enforced until quotaon(8) is called.     