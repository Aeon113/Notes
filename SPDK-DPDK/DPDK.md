# 2021-11-25

DPDK中，socket的概念等同于NUMA?

`malloc`-like API中的管理:
+ `struct malloc_heap` - used to track free space on a per-socket basis
+ `struct malloc_elem` - the basic element of allocation and free-space tracking inside the library.

每个`malloc_heap`中有多个`malloc_elem`。

每个`malloc_elem`内部的内存都是virtually contiguous，不一定phisically contiguous。一个`malloc_elem`内部的内存也不一定IOVA-contiguous。

一个表中，相邻的`malloc_elem`所代表的内存不一定是相邻的。

代表free memory表的list，其中的`malloc_elem`会merge。

# 2021-12-03

DPDK service cores / service: DPDK service其实就是函数。DPDK service cores指可以运行dpdk services的执行单元(CPU core)。

DPDK service可以被动态的分配到多个DPDK service cores上。当DPDK service处于enable和active状态时，DPDK service cores将不停调用此DPDK service。

DPDK service core可以是特定的、只用来执行DPDK service的core，也可以是lcore。
