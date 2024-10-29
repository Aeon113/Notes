# 2024-10-29
1. Rocksdb 使用单链表来组成链表, 主要目的是为了支持并发插入。因为单链表在插入节点时, 可以使用CAS来进行, 但双链表不行。

2. 列族的概念类似于MySQL/MongoDB中的表/collection。它是一种逻辑上的分区。一个rocksdb实例中可以有多个列族, 每个列族都有自己的LSM结构。多个列族共享WAL, 但不共享memtable, immemtable, SST; rocksdb默认情况下只有一个列族, 名为default(列族id为0)。

3. RocksDB里的配置分诶ColumnFamilyOptions和DBOptions。前者为单个列族的配置, 后者是整个DB的配置。

4. RocksDB通过`Version`管理某一个时刻的DB的状态怒。任何读写都是对一个version的操作。

Version用于管理LSM中的SST的集合在每次compaction结束或者memtable被flush到磁盘时, 都将会创建一个新的version, 用来记录新的LSM结构。随着数据不断的写入以及compaction额执行, RocksDB中将会存在多个version。但在任一时刻都只会有一个 __当前__ 的version版本, 即current version。新的Get操作或者迭代器在其整个查询过程和迭代器的生命周期内都会使用current version。"过时" (没有被任何Get或者iterator迭代器使用)的version都需要被清除。任何version都未使用的SST则会被删除。