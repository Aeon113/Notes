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

[DPDK 内存管理 - 1](https://www.intel.com/content/www/us/en/developer/articles/technical/memory-in-dpdk-part-1-general-concepts.html?wapkw=Memory%20in%20DPDK)



# 2021-12-20

[Memorypool Library - Data Plane Development Kit Documentation](https://doc.dpdk.org/guides/prog_guide/mempool_lib.html):

> In this way, each core has full access to its own cache (with locks) of free objects and only when the cache fills does the core need to shuffle some of the free objects back to the pools ring or obtain more objects when the cache is empty.

可以看出，只有当一个core的local memorypool cache被填光后，才会向memory pool归还或申请内存。这个可能导致DPDK所实际使用的内存空间大于其所需的内存量。

# 2022-01-24

SPDK NVMe 驱动组织架构:

## 1. Environment Abstraction:
SPDK/DPDK PCIe driver提供的抽象概念，使得一个PCIe设备(NVMe SSD)只能被一个进程使用

SPDK/DPDK 进程内的线程不能被频繁的申请和释放，通常在进程初始化早期，所需的线程就应被完全分配完毕。

SPDK/DPDK 进程依赖外部注册的hugepage，以防止频繁的memory pin。

SPDK/DPDK 要求目标设备已被从kernel mode driver中detach，并attach到了usermode driver framework，比如UIO, VFIO等。

## 2. NVMe Transport

就是NVMe spec内的transport，例如PCIe, RDMA等。

## 3. NVMe Probe and Attach

### [`spdk_nvme_connect()`](https://spdk.io/doc/nvme_8h.html#ae26375a74c2c935ec32f0c41a7ed93df)
链接NVMe controller。每次链接一个。此为同步调用。

不同于kernel中的NVMe driver，`spdk_nvme_connect()`可以使用`opts`参数，为每个所链接的NVMe controller设定不同的参数，比如IO queue数量等等。

### [`spdk_nvme_probe()`](https://spdk.io/doc/nvme_8h.html#a225bbc386ec518ae21bd5536f21db45d)
链接一个或多个NVMe controller。此为同步调用。

现在版本的spdk提供了异步版本的`spdk_nvme_connect()`和`spdk_nvme_probe()`。

## 4. [`spdk_nvme_ctrlr_alloc_io_qpair()`](https://spdk.io/doc/nvme_8h.html#a13f745d239dab9b8f934fae2ad4984a2)
通常kernel NVMe driver会为每个core/numa node 分配一个queue pair，但是SPDK不会自动创建IO queue pair。应用需要使用此API自行创建。通过这种方式用户可以实现更灵活的lcore-queue pair组合。

## 5. Submission命令
SPDK中的所有NVMe submission API均是异步API。

### 5.1 [`spdk_nvme_ns_cmd_read()`](https://spdk.io/doc/nvme_8h.html#a084c6ecb53bd810fbb5051100b79bec5)

注意这个函数的一次调用可能生成多个nvme cmd。


## 6. [`spdk_nvme_qpair_process_completions()`](https://spdk.io/doc/nvme_8h.html#aa331d140870e977722bfbb6826524782)
NVMe qpair polling函数。在submit了多个命令后，使用这个函数来poll completions。

注意如果submitting和polling不在同一个线程上，或者有多个线程在向同一个 qpair做submission, 或者有多个线程在向同一个qpair poll，用户需要自行维护线程安全。

Submit进目标qpair的命令，它们对应的`callback`s将在此函数的context中被调用。

## 7. `struct nvme_request`
+ Private, internal data structure of SPDK
+ Dedicated set of objects per qpair
  + Memory for objects allocated along with qpair
  + Count dictated by controller or qpair options
+ Transport agnostic
+ Can be queued
  + Depending on transport specifics

## 8. `I/O Splitting`
I/O may need to be split before sent to controller
  + Max Data Transfer Size (MDTS)
  + Namespace Optimal IO Boundary (NOIOB)

Two possible approaches
  1. User must split I/O before calling SPDK API
  2. SPDK split I/O internally

SPDK选择在内部自行分割IO请求，因为:
  1. Simplify SPDK usage and avoid code duplication
  2. Ignoring NOIOB is functionally OK, but will result in unexpected performance issues
  3. Minimal (if any) impact on applications that require no I/O splitting

## 9. Vectored I/O
SGEs fetched by SPDK NVMe driver using callback functions

## 10. VA-IOVA Translation
+ SPDK maintains a userspace "page table"
  + describes the pre-allocated pinned memory
+ Two-level page table
  + First level: 1GB granularity (0x0 => 256TB)
  + Second level: 2MB granularity
+ Similar registration/translation scheme used for RDMA
  + Translates to MR instead of IOVA

在DPDK community中，`IOVA`和`physical address`通常被混用，都用来指代非"virtual address`的内存地址，虽然它们本质上是不同的地质类型。

[Memory in DPDK 1](https://www.intel.com/content/www/us/en/developer/articles/technical/memory-in-dpdk-part-1-general-concepts.html?wapkw=Memory%20in%20DPDK)

[Memory in DPDK 2](https://www.intel.com/content/www/us/en/developer/articles/technical/memory-in-dpdk-part-2-deep-dive-into-iova.html)

[Memory in DPDK 3](https://www.intel.com/content/www/us/en/developer/articles/technical/memory-in-dpdk-part-3-1711-and-earlier-releases.html)

[Memory in DPDK 4](https://www.intel.com/content/www/us/en/developer/articles/technical/memory-in-dpdk-part-4-1811-and-beyond.html)

## 11. Hotplug

首先说"hot insertion", 这一部分和正常的probe没有区别，因为SPDK不会主动"hot insertion" 任何设备，这里需要用户逻辑自行probe/connect/attach。

再说"hot removal"。SPDK会通过监控uevent来检测设备的hot removal。但这里存在一个问题，设备移除和uevent产生存在一个时间差。在这个时间差内如果用户进程仍然在submit IO, 用户进程将会访问到这个设备的BAR空间，然后收到一个`SIGBUS`。

所以出了监控uevent来获取来自操作系统的removal信息外，SPDK还会注册`SIGBUS` handler, 这个 `SIGBUS` handler将会把BAR空间 remap 到一段别的内存上，防止再次出现`SIGBUS`。然后直到uevent到达，hot removal流程正式结束。

## 12. Timeout Callbacks
[spdk_nvme_ctrlr_register_timeout_callback()](https://spdk.io/doc/nvme_8h.html#ae2957853179526e6176cf7623b19552b)

+ Optional - can be set at controller level, we cannot set a timeout for each IO request
+ No interrupt in SPDK, how to check the timeouts?
  + Do expirations checks during completion polling
+ For timeouts, the driver does nothing (i.e. abort or reset) but notifies the application
  + Application is notified and can take action
+ Pending requests linked in submission order, so timeout checks do not take much cost

## 13. benchmarking
+ fio plugin
+ nvme perf

