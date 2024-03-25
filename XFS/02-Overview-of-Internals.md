# Overview of Internals

*This part is mainly derived from [XFS Filesystem Disk Structures](http://oss.sgi.com/projects/xfs/papers/xfs_filesystem_structure.pdf) (3rd Edition).*      

XFS is a high performance filesystem which was designed to maximize parallel throughput and to scale up to ex- tremely large 64-bit storage systems. Originally developed by SGI in October 1993 for IRIX, XFS can handle large files, large filesystems, many inodes, large directories, large file attributes, and large allocations. Filesystems are optimized for parallel access by splitting the storage device into semi-autonomous allocation groups. XFS employs branching trees (B+ trees) to facilitate fast searches of large lists; it also uses delayed extent-based allocation to improve data contiguity and IO performance.      

_All fields in XFS metadata structures are in big-endian byte order except for log items which are formatted in host order._       

