From e2fsprogs/lib/ext2fs

``` c
// from ext2_fs.h

/*
 * Data structures used by the directory indexing feature
 *
 * Note: all of the multibyte integer fields are little endian.
 */

/*
 * Note: dx_root_info is laid out so that if it should somehow get
 * overlaid by a dirent the two low bits of the hash version will be
 * zero.  Therefore, the hash version mod 4 should never be 0.
 * Sincerely, the paranoia department.
 */
/*
 * In a indexed directory, this struct appears at offset 24.
 * The first half of the prefix is for directory ".", and the other one is
 * for "..".
 */
struct ext2_dx_root_info
{
    __u32 reserved_zero;
    __u8 hash_version; /* 0 now, 1 at release */
    __u8 info_length;  /* 8 */
    __u8 indirect_levels;
    __u8 unused_flags;
};

#define EXT2_HASH_LEGACY 0
#define EXT2_HASH_HALF_MD4 1
#define EXT2_HASH_TEA 2
#define EXT2_HASH_LEGACY_UNSIGNED 3   /* reserved for userspace lib */
#define EXT2_HASH_HALF_MD4_UNSIGNED 4 /* reserved for userspace lib */
#define EXT2_HASH_TEA_UNSIGNED 5      /* reserved for userspace lib */

#define EXT2_HASH_FLAG_INCOMPAT 0x1

#define EXT4_DX_BLOCK_MASK 0x0fffffff

struct ext2_dx_entry
{
    __le32 hash;
    __le32 block;
};

struct ext2_dx_countlimit
{
    __le16 limit; /* how many entries can be stored in this block */
    __le16 count; /* how many entries is stored in this block */
};

/*
 * This goes at the end of each htree block.
 */
struct ext2_dx_tail
{
    __le32 dt_reserved;
    __le32 dt_checksum; /* crc32c(uuid+inum+dxblock) */
};
```   

Still Dont Know how is ext2_dx_countlimit determined.   

It seems Ext2 does not have metadata csum protection.   

Physical structure of the first blocks of a indexed directory:   

| Offset (bytes) | Length (bytes) | Structure |
|:------:|:------:|:---------:|
| 0 | 12 | dirent for directory "." |
| 12 | 12 | dirent for directory ".." |
| 24 | ext2_dx_root_info::info_length (usually 8) | ext2_dx_root_info |
| 24 + ext2_dx_root_info::info_length, usually 32 | 4 | ext2_dx_countlimit |
| 28 + ext2_dx_root_info::info_length, usually 36 | ... | "ext2_dx_entry"s |   

The rec_len of the ".." dirent shall be the length of the remaining of the whole block.   

For other non-leaf blocks, the first two dirents are replaced by one 8-bytes empty-dirent, its rec_len shall be the length of the whole block length.   

Every time we seek a block down the hash chain, the level shall be decreased by 1. When it reaches 0, the children blocks of the current one are leaf blocks.   

Linux ACL: 块地址公式:   

``` c
inode->i_file_acl = blk;
if(fs && ext2fs_has_feature_64bit(fs->super))
    inode->osd2.linux.l_i_file_acl_high = (__u64) blk >> 32;
    /* blk is of type blk64_t */
```   

-------------------------------------
# 预备实现列表：   

### Super Block:

s_state:   

| 宏 | 值 | 行为 |
|:----|:--:|-----:|
| EXT2_VALID_FS | 0x0001 | 支持 |
| EXT2_ERROR_FS | 0x0002 | 支持 |
| EXT3_ORPHAN_FS | 0x0004 | 不支持 |

s_errors:   

| 宏 |  值   | 行为  |
|:----|:-----:|-----:|
| EXT2_ERRORS_CONTINUE | 1 | 支持 |
| EXT2_ERRORS_RO | 2 | 支持 |
| EXT2_ERRORS_PANIC | 3 | 支持 |

s_creator_os:   

| 宏 |  值   | 行为  |
|:----|:-----:|-----:|
| EXT2_OS_LINUX | 0 | 支持 |
| EXT2_OS_HURD | 1 | 支持 |
| EXT2_OS_MASIX | 2 | 支持 |
| EXT2_OS_FREEBSD | 3 | 支持 |
| EXT2_OS_LITES | 4 | 支持 |

s_rev_level:

| 宏 |  值   | 行为  |
|:----|:-----:|-----:|
| EXT2_GOOD_OLD_REV | 0 | 支持 |
| EXT2_DYNAMIC_REV | 1 | 支持 |

s_feature_compat:   

| 宏 |  值   | 行为  |
|:----|:-----:|-----:|
| EXT2_FEATURE_COMPAT_DIR_PREALLOC | 0x0001 | 支持，读取superblock内的s_prealloc_blocks和s_repalloc_dir_blocks来获取理应预先分配的block数量 |
| EXT2_FEATURE_COMPAT_IMAGIC_INODES | 0x0002 | 支持，无操作 |
| EXT3_FEATURE_COMPAT_HAS_JOURNAL | 0x0004 | 支持，无操作 |
| EXT2_FEATURE_COMPAT_EXT_ATTR | 0x0008 | 部分支持。软件不支持ACL, 无操作。仅在删除INODE时，同时变更ACL的引用数。详见[The second Extended File System](http://www.nongnu.org/ext2-doc/ext2.html#CONTRIB-EXTENDED-ATTRIBUTES), 并在必要时回收ACL块。|
| EXT2_FEATURE_COMPAT_RESIZE_INO | 0x0010 | 支持，方式应该是在结尾截断，或延长。 |
| EXT2_FEATURE_COMPAT_DIR_INDEX | 0x0020 | 部分支持。在写INDEXED_DIR时，将其inode内的相应bit置0，及将其转化为传统DIR |   

s_feature_incompat:   

| 宏 |  值   | 行为  |
|:----|:-----:|-----:|
| EXT2_FEATURE_INCOMPAT_COMPRESSION | 0x0001 | 压缩，不支持 |
| EXT2_FEATURE_INCOMPAT_FILTYPE | 0x0002 | 支持，不知有什么用，应该是和指dirent里的file type。Revision 1内应为enable, revision 0内未知。|
| EXT3_FEATURE_INCOMPAT_RECOVER | 0x0004 | 不支持 |
| EXT3_FEATURE_INCOMPAT_JOURNAL_DEV | 0x0008 | 不支持 |
| EXT2_FEATURE_INCOMPAT_META_BG | 0x0010 | 不支持 |   

s_feature_ro_compat:   

| 宏 |  值   | 行为  |
|:----|:-----:|-----:|
| EXT2_FEATURE_RO_COMPAT_SUPER | 0x0001 | 稀疏superblock, 支持。set时，在block group 0, 1, 以及 3、5、7的幂内存储super block 以及 group description table; unset时在所有block group内存储上述信息。 |
| EXT2_FEATURE_RO_COMPAT_LARGE_FILE | 0x0002 | 64-bit 文件大小，支持。 |
| EXT2_FEATURE_RO_BTREE_DIR | 0x0004 | 不支持，只读挂载 |   

s_default_mount_options:   

| 宏 |  值   | 行为  |
|:----|:-----:|-----:|
| EXT2_DEFM_DEBUG | 0x0001 | 输出调试信息，支持，行为待定 |
| EXT2_BSDGROUPS | 0x0002 | 不理解，忽略 |
| EXT2_XATTR_USER | 0x0004 | 支持，行为同EXT2_FEATURE_COMPAT_EXT_ATTR |
| EXT2_DEFM_ACL | 0x0008 | 支持，行为同EXT2_XATTR_USER |
| EXT2_DEFM_UID16 | 0x0010 | 16-bit UID，支持 |    


### Inode:   

i_mode:   

| 宏 |  值   | 行为  |
|:----|:-----:|-----:|
| EXT2_S_IFSOCK | 0xC000 | socket, 支持，不可读取，不可写入，不可创建，删除时提示 |
| EXT2_S_IFLINK | 0xA000 | symbolic link, 符号连接即软连接，支持，以文件系统根为根目录，搜索。不可创建。 |
| EXT2_S_IFREG | 0x8000 | regular file，常规文件，支持CURD |
| EXT2_S_IFBLK | 0x6000 | block device, 支持，不可读取，不可写入，不可创建，删除时提示 |
| EXT2_S_IFDIR | 0x4000 | directory, 支持CURD, 删除时对entrys进行DFS，并提示 |
| EXT2_S_IFCHR | 0x2000 | character device，支持，不可读取，不可写入，不可创建，删除时提示 |
| EXT2_S_IFIFO | 0x1000 | fifo，支持，不可读取，不可写入，不可创建，删除时提示 |
| EXT2_S_ISUID | 0x0800 | Set process User ID, 支持，忽略 |
| EXT2_S_ISGID | 0x0400 | Set process Group ID, 支持，忽略 |
| EXT2_S_ISVTX | 0x0200 | sticky bit，忽略 |
| EXT2_S_IRUSR | 0x0100 | user read, 支持，忽略 |
| EXT2_S_IWUSR | 0x0080 | user write, 支持，忽略 |
| EXT2_S_IXUSR | 0x0040 | user execute, 支持，忽略 |
| EXT2_S_IRGRP | 0x0020 | group read, 支持，忽略 |
| EXT2_S_IWGRP | 0x0010 | group write, 支持，忽略 |
| EXT2_S_IXGRP | 0x0008 | group execute, 支持，忽略 |
| EXT2_S_IROTH | 0x0004 | user read, 支持，忽略 |
| EXT2_S_IWOTH | 0x0002 | user write, 支持，忽略 |
| EXT2_S_IXOTH | 0x0001 | user execute, 支持，忽略 |   

i_flags:   

| 宏 |  值   | 行为  |
|:----|:-----:|-----:|
| EXT2_SECRM_FL | 0x00000001 | secure deletion，支持：inode删除后，向其blocks内填充随机数据 |
| EXT2_UNRM_FL | 0x00000002 | record for undelete，删除此节点时，将其内容移至其它区域以便恢复：忽略 |
| EXT2_COMPR_FL | 0x00000004 | compressed file，不应出现，不支持，相应文件只支持移动、复制、删除，无法读写 |
| EXT2_SYNC_FL | 0x00000008 | The file's content in memory will be constantly synchronized with the content on disk, 支持，行为：在写入、移动、删除此object后立即执行sync |
| EXT2_IMMUTABLE_FL | 0x00000010 | 任何用户，包括super user，都无法编辑，重命名，移动，删除此文件。支持。用户可以为object增加此属性，也可移除此属性 |
| EXT2_APPEND_FL | 0x00000020 | 只能在文件尾部追加，不能修改。支持，视作只读文件。 |
| EXT2_NODUMP_FL | 0x00000040 | 不能抹除object content，即使 i_links_count == 0，支持。 |
| EXT2_NOATIME_FL | 0x00000080 | 不update atime，支持 |
| EXT2_DIRTY_FL | 0x00000100 | Dirty标记，表明此inode内信息未完全写入。支持。行为：在每次修改inode信息前都会set此标记，在inode信息写入完成后unset。在读取inode时，若发现dirty标记，则尝试恢复：恢复行为待定无法恢复就在当前dirent内删除此inode的link，直到inode内links为0后删除此inode。|
| EXT2_COMPRBLK_FL | 0x00000200 | 指仅有部分data block被压缩。不应出现，若发现，则不能读取和写入，只能移动，重命名和删除。 |
| EXT2_NOCOMPR_FL | 0x00000400 | 指虽然数据被压缩，但应原样传递给应用程序而非解压后再传递。不应出现，但支持。 |
| EXT2_ECOMPR_FL | 0x00000800 | compression error，指在uncompressing时发生了错误。不应出现，行为：不能读取、修改、复制，可以移动和删除。|
| EXT2_INDEX_FL/EXT2_BTREE_FL | 0x00001000 | B-Tree Format Directory，后改为Hash Indexed Directory，部分支持。行为：读取、复制、移动、删除时与普通object相同。写入时，unset此bit，之后作为普通directory操作。 |
| EXT2_IMAGIC_FL | 0x00002000 | 无资料，不能理解，似乎与AFS/OpenAFS有关。不能读写，不能复制、移动，仅可删除。 |
| EXT2_JOURNAL_DATA_FL/EXT3_JOURNAL_DATA_FL | 0x00004000 | File data should be journaled。不应出现，忽略。 |
| EXT2_NOTAIL_FL | 0x00008000 | file tail should not be merged. 不理解，无资料。忽略。 |
| EXT2_DIRSYNC_FL | 0x00010000 | Synchronous directory modifications. 支持，在操作其内dirent后立即sync。 |
| EXT2_TOPDIR_FL | 0x00020000 | Top of directory hierarchies。不理解，无资料，忽略。 |
| EXT2_RESERVED_FL |  0x80000000| Reserved for ext2 lib，支持：忽略。 |   

### Reserved Inodes:   

| 宏 | 值 | 行为 |
|:---|:--:|----:|
| EXT2_BAD_INO	| 1	| Bad blocks inode，全置零 |
| EXT2_ROOT_INO	| 2	| Root inode |
| EXT4_USR_QUOTA_INO | 3	| User quota inode，全置零 |
| EXT4_GRP_QUOTA_INO | 4	| Group quota inode，全置零 |
| EXT2_BOOT_LOADER_INO | 5	| Boot loader inode，全置零 |
| EXT2_UNDEL_DIR_INO | 6	| Undelete directory inode，全置零 |
| EXT2_RESIZE_INO | 7	| Reserved group descriptors inode，保存 Reserved GDT Blocks |
| EXT2_JOURNAL_INO | 8	| Journal inode，全置零 |
| EXT2_EXCLUDE_INO | 9	| The "exclude" inode, for snapshots，全置零 |
| EXT4_REPLICA_INO | 10	| Used by non-upstream feature，一般为lost+found |   


### Reserved GDT Blocks:   
保存用于扩充GDT的blocks，他们均跟在GDT之后。每个GDT之后的数量均相同。一般大小为，可以将GDT扩充为当前大小的1024倍。   

### 补充
每次建立新inode，应对i_generation赋予随机值，并在每次写入inode或其内文件时，将其自增。