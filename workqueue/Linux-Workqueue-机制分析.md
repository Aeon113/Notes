Linux Workqueue 机制分析

**2018-08-12 03:08**

本文发自 [http://www.binss.me/blog/analysis-of-linux-workqueue/](https://www.binss.me/blog/analysis-of-linux-workqueue/)，转载请注明出处。

## 定义

workqueue 是自 kernel 2.6 引入的一种任务执行机制，和 softirq 、 tasklet 并称下半部 (bottom half) 三剑客。比起这两者，workqueue 在进程上下文异步执行任务，能够进行睡眠，很快就受到内核开发者们的追捧。

workqueue 最核心的思想是分离了任务 (work) 的发布者和执行者。当需要执行任务的时候，构造一个 work，塞进相应的 workqueue，由 workqueue 所绑定的 worker (thread) 去执行。如果任务不重要，我们可以让多种任务共享一个 worker，而无需每种任务都开一个 thread 去处理(n -> 1)；相反如果任务很多很重要，那么我们可以开多个 worker 加速处理(1 -> n)，类似于生产者消费者模型。

在传统的实现中，workqueue 和 worker 的对应关系是二元化的：要么使用 Multi Threaded (MT) workqueue 每个 CPU 一个 worker，要么使用 Single Threaded (ST) workqueue 整个系统只有一个 worker。但即使是通过 MT 每个 CPU 都开一个 worker ，它们相互之间是独立的，在哪个 CPU 上挂入的 work 就只能被那个 CPU 上的 worker 处理，这样当某个 worker 因为处理 work 而阻塞时，位于其他 CPU 上的 worker 只能干着急，这并不是我们所期待的并行。更麻烦的是，它很容易导致死锁，比如有 A 和 B 两个 work，B 依赖于 A 的执行结果，而此时如果 A 和 B 被安排由同一个 worker 来做，而 B 恰好在 A 前面，于是形成死锁。

为了解决这个问题，Tejun Heo 在 2009 年提出了 CMWQ(Concurrency Managed Workqueue) ，于 2.6.36 进入 kernel 。

### CMWQ

相比传统实现，CMWQ 做了一个很重要的改进就是 workqueue 和 worker 进行解耦，提出了 worker pool 的概念。worker 的创建不再与 workqueue 相关，而是由 worker pool 统一管理。不同 workqueue 共享全局的 worker pool，但 workqueue 可以根据需要 (flags) 选择使用特定的 worker pool 。

整理一下 workqueue 中出现的角色：

* work ：工作。被加到 workqueue 中
* workqueue ：工作队列。逻辑上是 work 排成的队列
* worker：工人。本质上是内核线程(kthread)，负责真正执行 work
* worker pool：worker 的集合。负责管理 worker。

#### worker pool

按照运行特性，主要 CPU bound 和 unbound 分为两类。

##### CPU bound worker pool

绑定特定 CPU，其管理的 worker 都运行在该 CPU 上。

根据优先级分为 normal pool 和 high priority pool，后者管理高优先级的 worker。

Linux 会为每个 online CPU 都创建 1 个 normal pool 和 1 个 high priority pool，并在命名上进行区分。

比如 `[kworker/1:1]` 表示 CPU 1 上 normal pool 的第 1 个 worker ，而 `[kworker/2:0H]` 表示 CPU 2 上 high priority pool 的第 0 个 worker

##### unbound

其管理的 worker 可以运行在任意的 CPU 上。

比如 `[kworker/u32:2]` 表示 unbound pool 32 的第 2 个 worker 进程

## 实现

### 数据结构

#### work

```
struct work_struct {
    atomic_long_t data;
    struct list_head entry;
    work_func_t func;
#ifdef CONFIG_LOCKDEP
    struct lockdep_map lockdep_map;
#endif
};
```

work 的定义很简单，func 是相应的函数指针，data 是 work 的数据，包括 flags、所处 pool_workqueue 等。entry 用来加入到 workqueue (实际上是 worker pool，后详) 中的 work 链表 。

#### workqueue

``` c
/*
 * The externally visible workqueue.  It relays the issued work items to
 * the appropriate worker_pool through its pool_workqueues.
 */
struct workqueue_struct {
    struct list_head    pwqs;       /* WR: all pwqs of this wq */
    struct list_head    list;       /* PR: list of all workqueues */

    struct mutex        mutex;      /* protects this wq */
    int         work_color; /* WQ: current work color */
    int         flush_color;    /* WQ: current flush color */
    atomic_t        nr_pwqs_to_flush; /* flush in progress */
    struct wq_flusher   *first_flusher; /* WQ: first flusher */
    struct list_head    flusher_queue;  /* WQ: flush waiters */
    struct list_head    flusher_overflow; /* WQ: flush overflow list */

    struct list_head    maydays;    /* MD: pwqs requesting rescue */
    struct worker       *rescuer;   /* MD: rescue worker */

    int         nr_drainers;    /* WQ: drain in progress */
    int         saved_max_active; /* WQ: saved pwq max_active */

    struct workqueue_attrs  *unbound_attrs; /* PW: only for unbound wqs */
    struct pool_workqueue   *dfl_pwq;   /* PW: only for unbound wqs */

#ifdef CONFIG_SYSFS
    struct wq_device    *wq_dev;    /* I: for sysfs interface */
#endif
#ifdef CONFIG_LOCKDEP
    char            *lock_name;
    struct lock_class_key   key;
    struct lockdep_map  lockdep_map;
#endif
    char            name[WQ_NAME_LEN]; /* I: workqueue name */
```

和想象中的不一样，workqueue 并不是真正的 queue，其没有维护任何存放 work 的结构。但维护了两个 pool_workqueue 类型的成员。

从前面我们可以知道，workqueue 和 worker pool 是多对多的关系：一个 workqueue 可以对应多个 worker pool，同时一个 worker pool 也可以对应多个 workqueue。为了将它们关联起来，引入了 pool_workqueue 这个结构：

``` c
/*
 * The per-pool workqueue.  While queued, the lower WORK_STRUCT_FLAG_BITS
 * of work_struct->data are used for flags and the remaining high bits
 * point to the pwq; thus, pwqs need to be aligned at two's power of the
 * number of flag bits.
 */
struct pool_workqueue {
    struct worker_pool  *pool;      /* I: the associated pool */
    struct workqueue_struct *wq;        /* I: the owning workqueue */
    int         work_color; /* L: current color */
    int         flush_color;    /* L: flushing color */
    int         refcnt;     /* L: reference count */
    int         nr_in_flight[WORK_NR_COLORS];
                        /* L: nr of in_flight works */

    /*
     * nr_active management and WORK_STRUCT_INACTIVE:
     *
     * When pwq->nr_active >= max_active, new work item is queued to
     * pwq->inactive_works instead of pool->worklist and marked with
     * WORK_STRUCT_INACTIVE.
     *
     * All work items marked with WORK_STRUCT_INACTIVE do not participate
     * in pwq->nr_active and all work items in pwq->inactive_works are
     * marked with WORK_STRUCT_INACTIVE.  But not all WORK_STRUCT_INACTIVE
     * work items are in pwq->inactive_works.  Some of them are ready to
     * run in pool->worklist or worker->scheduled.  Those work itmes are
     * only struct wq_barrier which is used for flush_work() and should
     * not participate in pwq->nr_active.  For non-barrier work item, it
     * is marked with WORK_STRUCT_INACTIVE iff it is in pwq->inactive_works.
     */
    int         nr_active;  /* L: nr of active works */
    int         max_active; /* L: max active works */
    struct list_head    inactive_works; /* L: inactive works */
    struct list_head    pwqs_node;  /* WR: node on wq->pwqs */
    struct list_head    mayday_node;    /* MD: node on wq->maydays */

    /*
     * Release of unbound pwq is punted to system_wq.  See put_pwq()
     * and pwq_unbound_release_workfn() for details.  pool_workqueue
     * itself is also RCU protected so that the first pwq can be
     * determined without grabbing wq->mutex.
     */
    struct work_struct  unbound_release_work;
    struct rcu_head     rcu;
} __aligned(1 << WORK_STRUCT_FLAG_BITS);
```


pool_workqueue 维护了 workqueue 和 worker pool 的指针，起到关联作用。

在 workqueue 看来，pool_workqueue 代表了一个 worker pool。worker pool 分为 CPU bound 和 unbound 。对于前者，只需通过 per-CPU 变量指向即可，后者则通过一个数组来维护。这就是 workqueue_struct 中 cpu_pwqs 和 numa_pwq_tbl 成员的由来。

这里还有个细节： workqueue 应该和多少个 worker pool 进行关联，即应该创建多少个 pool_workqueue 。从理论上来说，普通的 workqueue 和所有 CPU worker 都建立关联，而 unbound workqueue 取决于是否设置了 `__WQ_ORDERED` flag，如果是，则要求严格串行执行，只能关联一个 unbound worker pool 。否则，为了 NUMA 亲和性，我们每个 NUMA node 关联一个 unbound worker pool。而具体实现上有细微不同，细节请看下文对 apply_workqueue_attrs 函数的分析。

#### worker_pool

``` c
/*
 * Structure fields follow one of the following exclusion rules.
 *
 * I: Modifiable by initialization/destruction paths and read-only for
 *    everyone else.
 *
 * P: Preemption protected.  Disabling preemption is enough and should
 *    only be modified and accessed from the local cpu.
 *
 * L: pool->lock protected.  Access with pool->lock held.
 *
 * X: During normal operation, modification requires pool->lock and should
 *    be done only from local cpu.  Either disabling preemption on local
 *    cpu or grabbing pool->lock is enough for read access.  If
 *    POOL_DISASSOCIATED is set, it's identical to L.
 *
 * A: wq_pool_attach_mutex protected.
 *
 * PL: wq_pool_mutex protected.
 *
 * PR: wq_pool_mutex protected for writes.  RCU protected for reads.
 *
 * PW: wq_pool_mutex and wq->mutex protected for writes.  Either for reads.
 *
 * PWR: wq_pool_mutex and wq->mutex protected for writes.  Either or
 *      RCU for reads.
 *
 * WQ: wq->mutex protected.
 *
 * WR: wq->mutex protected for writes.  RCU protected for reads.
 *
 * MD: wq_mayday_lock protected.
 */

/* struct worker is defined in workqueue_internal.h */

struct worker_pool {
    raw_spinlock_t      lock;       /* the pool lock */
    int         cpu;        /* I: the associated cpu */
    int         node;       /* I: the associated node ID */
    int         id;     /* I: pool ID */
    unsigned int        flags;      /* X: flags */

    unsigned long       watchdog_ts;    /* L: watchdog timestamp */

    /*
     * The counter is incremented in a process context on the associated CPU
     * w/ preemption disabled, and decremented or reset in the same context
     * but w/ pool->lock held. The readers grab pool->lock and are
     * guaranteed to see if the counter reached zero.
     */
    int         nr_running;

    struct list_head    worklist;   /* L: list of pending works */

    int         nr_workers; /* L: total number of workers */
    int         nr_idle;    /* L: currently idle workers */

    struct list_head    idle_list;  /* L: list of idle workers */
    struct timer_list   idle_timer; /* L: worker idle timeout */
    struct timer_list   mayday_timer;   /* L: SOS timer for workers */

    /* a workers is either on busy_hash or idle_list, or the manager */
    DECLARE_HASHTABLE(busy_hash, BUSY_WORKER_HASH_ORDER);
                        /* L: hash of busy workers */

    struct worker       *manager;   /* L: purely informational */
    struct list_head    workers;    /* A: attached workers */
    struct completion   *detach_completion; /* all workers detached */

    struct ida      worker_ida; /* worker IDs for task name */

    struct workqueue_attrs  *attrs;     /* I: worker attributes */
    struct hlist_node   hash_node;  /* PL: unbound_pool_hash node */
    int         refcnt;     /* PL: refcnt for unbound pools */

    /*
     * Destruction of pool is RCU protected to allow dereferences
     * from get_work_pool().
     */
    struct rcu_head     rcu;
};
```

前文提到，worker pool 是所有 workqueue 共用的。它不用关心 work 到底来自哪个 workqueue，只需要机械地从 worklist 中取出 work ，再从 workers 中取出 worker，将 worker 交给 worker 执行即可。

#### worker

``` c
/*
 * The poor guys doing the actual heavy lifting.  All on-duty workers are
 * either serving the manager role, on idle list or on busy hash.  For
 * details on the locking annotation (L, I, X...), refer to workqueue.c.
 *
 * Only to be used in workqueue and async.
 */
struct worker {
    /* on idle list while idle, on busy hash table while busy */
    union {
        struct list_head    entry;  /* L: while idle */
        struct hlist_node   hentry; /* L: while busy */
    };

    struct work_struct  *current_work;  /* L: work being processed */
    work_func_t     current_func;   /* L: current_work's fn */
    struct pool_workqueue   *current_pwq;   /* L: current_work's pwq */
    unsigned int        current_color;  /* L: current_work's color */
    struct list_head    scheduled;  /* L: scheduled works */

    /* 64 bytes boundary on 64bit, 32 on 32bit */

    struct task_struct  *task;      /* I: worker task */
    struct worker_pool  *pool;      /* A: the associated pool */
                        /* L: for rescuers */
    struct list_head    node;       /* A: anchored at pool->workers */
                        /* A: runs through worker->node */

    unsigned long       last_active;    /* L: last active timestamp */
    unsigned int        flags;      /* X: flags */
    int         id;     /* I: worker id */
    int         sleeping;   /* None */

    /*
     * Opaque string set with work_set_desc().  Printed out with task
     * dump for debugging - WARN, BUG, panic or sysrq.
     */
    char            desc[WORKER_DESC_LEN];

    /* used only by rescuers to point to the target workqueue */
    struct workqueue_struct *rescue_wq; /* I: the workqueue to rescue */

    /* used by the scheduler to determine a worker's last known identity */
    work_func_t     last_func;
};
```

本质上是一个内核线程，通过 task 成员指向。通过 current_work 指向当前正在处理的 work，current_func 指向 work 相应的函数。

根据当前的状态，加入到 worker_pool 的 idle_list 或 busy list 中。

### API

#### 创建 work

``` c
#define DECLARE_WORK(n, f)                      \
    struct work_struct n = __WORK_INITIALIZER(n, f)

#define INIT_WORK(_work, _func)                     \
    __INIT_WORK((_work), (_func), 0)
```

kernel 提供了 DECLARE_WORK 来创建 work，当然也可以手动定义 work 然后通过 INIT_WORK 来初始化。

#### 创建 workqueue

系统默认提供了一些 workqueue，比如 system_wq 等，直接加入即可。但用户也可以通过 alloc_workqueue / alloc_ordered_workqueue 自己创建：

``` c
#define alloc_workqueue(fmt, flags, max_active, args...)        \
    __alloc_workqueue_key((fmt), (flags), (max_active),     \
                  NULL, NULL, ##args)

#define alloc_ordered_workqueue(fmt, flags, args...)            \
    alloc_workqueue(fmt, WQ_UNBOUND | __WQ_ORDERED |        \
            __WQ_ORDERED_EXPLICIT | (flags), 1, ##args)
```

可见 alloc_ordered_workqueue 多加了 WQ_UNBOUND | __WQ_ORDERED | __WQ_ORDERED_EXPLICIT 三个 flag 。

为了保持兼容，CMWQ 也对以前的接口提供了支持：

``` c
#define create_workqueue(name)                      \
    alloc_workqueue("%s", __WQ_LEGACY | WQ_MEM_RECLAIM, 1, (name))

#define create_singlethread_workqueue(name)             \
    alloc_ordered_workqueue("%s", __WQ_LEGACY | WQ_MEM_RECLAIM, name)

#define create_freezable_workqueue(name)                \
    alloc_workqueue("%s", __WQ_LEGACY | WQ_FREEZABLE | WQ_UNBOUND | \
            WQ_MEM_RECLAIM, 1, (name))
```

本质上也是调用 alloc_workqueue，只是多了 `__WQ_LEGACY` flag 。

我们重点来看 alloc_workqueue => `__alloc_workqueue_key`

##### __alloc_workqueue_key

创建 workqueue 的核心逻辑。

> => 如果设置了 WQ_UNBOUND 且 max_active == 1，设置 __WQ_ORDERED(严格串行执行)
> => 如果设置了 WQ_POWER_EFFICIENT 且开启了 CONFIG_WQ_POWER_EFFICIENT_DEFAULT ，设置 WQ_UNBOUND(不绑定 CPU)
> => alloc_workqueue_attrs     如果是 unbound workqueue，由于属性较多，专门使用 workqueue_attrs 来存放，这里进行初始化
> => alloc_and_link_pwqs       创建相应数目的 pool_workqueue ，用作连接 worker pool 的桥梁
>     => 如果未设置 WQ_UNBOUND，需要为每个 CPU 都创建一个 pool_workqueue，设置到 per CPU 变量中，并通过 init_pwq 绑定 per CPU 的 worker_pool
>     => 否则如果设置了 __WQ_ORDERED ，通过 apply_workqueue_attrs 设置属性为 ordered_wq_attrs[highpri]，其中 highpri 由 WQ_HIGHPRI 决定
>     => 否则通过 apply_workqueue_attrs 设置属性为 unbound_std_wq_attrs[highpri]
> => 如果设置了 WQ_MEM_RECLAIM，为了保证在内存回收时还能干活，避免因为内存不足无法创建新 worker 导致阻塞，提前额外创建名为 rescuer 的 worker
> => 如果设置了 WQ_SYSFS，需要在 /sys/bus/workqueue/devices/ 下创建相应的文件，如果文件已存在，报错退出
> => list_add_tail_rcu(&wq->list, &workqueues)         将当前的 wq 加入到全局链表 workqueues 中


max_active 参数用于指定 workqueue 在一个 worker pool 上能同时运行上下文数目，换句话说就是最多能有多少个 worker 服务于该 workqueue。但如果用户指定 WQ_UNBOUND flag 且 max_active 为 1，这说明了同一个 node 上，添加到该 workqueue 中的 work 是串行执行的，因此设置 `__WQ_ORDERED` 。

除了运行效率，节能省电也被 workqueue 体系考虑在内。按照常理，使用 unbound pool 会比 bound pool 更省电。因为对于 unbound pool，调度器可以调度它的 worker 到任意的 CPU 上执行，而 bound pool 要求 worker 必须在特定 CPU 上执行。这意味者在某些 CPU idle 的情况下，使用 unbound pool 可以避免唤醒它们，从而实现省电的目的。

##### workqueue_attrs

``` c
	struct workqueue_attrs {
    int         nice;       /* nice level */
    cpumask_var_t       cpumask;    /* allowed CPUs */
    bool            no_numa;    /* disable NUMA affinity */
};
```

workqueue 的属性。这是 unbound workqueue 所独有的。属性不同的 workqueue 使用不同的 worker pool ，因为 worker pool 的行为将和属性保持一致。

##### apply_workqueue_attrs

理论上，由于 NUMA 亲和性，对于一种特定属性的 unbound workqueue，会为它在每一个 NUMA node 上创建一个 worker pool，worker pool 上的 worker 绑定为该 node 上的 CPU。在执行 task 时，会把 work 放到当前 CPU 所在 node 的 worker pool 上去做。

而实际上，workqueue 关联的 worker pool 的数目实际上受四个因素的约束：全局 cpumask(wq_unbound_cpumask)，属性中的 cpumask (workqueue_attrs.cpumask)，属性中的 no_numa (workqueue_attrs.no_numa) 和 CPU 是否 offline 。

* wq_unbound_cpumask : 默认为 cpu_possible_mask 。但可通过 /sys/devices/virtual/workqueue/cpumask 进行修改。影响所有的 workqueue，所有的 workqueue 中的 work 只能在该 mask 指定的 CPU 上执行
* workqueue_attrs.cpumask : 影响单个 workqueue ，它上面的 work 只能在该 mask 指定的 CPU 上执行
* workqueue_attrs.no_numa : 影响单个 workqueue ，它上面的 work 执行不受 node 的限制
* CPU offline : 当 CPU offline时，work 自然不能在上面执行

apply_workqueue_attrs 实现的就是这些逻辑。它负责为 unbound workqueue 设置属性，采用了先创建再提交的方式，如果 prepare 失败，则直接返回：

> => apply_workqueue_attrs_locked
>     => apply_wqattrs_prepare                                                创建新的上下文(ctx)
>         => 创建属性上下文 apply_wqattrs_ctx
>         => ctx->dfl_pwq = alloc_unbound_pwq(wq, new_attrs)                  根据属性创建默认 pool_workqueue
>         => 对于每个 node
>             => wq_calc_node_cpumask                                         更新属性中的 cpumask
>             => ctx->pwq_tbl[node] = alloc_unbound_pwq(wq, tmp_attrs)        根据属性创建对应的 pool_workqueue
>         => 返回上下文(ctx)
>     => apply_wqattrs_commit => numa_pwq_tbl_install     将新的上下文(ctx)中的 pool_workqueue 设置到 workqueue 的 numa_pwq_tbl 中，将旧的存回 ctx
>     => apply_wqattrs_cleanup                            操作已经成功提交，清除该上下文(ctx)


受上述因素的影响，这里考虑了以下情况：

* 如果属性中设置了 no_numa，说明不再考虑亲和性，此时只需要关联一个 worker pool
* 对于某个 node，如果算出来的 cpumask 和 node 的 cpumask 无相交，无需关联 worker pool
* 对于某个 node，如果算出来的 cpumask 就等于 node 的 cpumask，则说明它不会和其他 node 的 cpumask 有相交，此时只需要关联一个 worker pool

##### alloc_unbound_pwq

对于 bound workqueue，其 worker pool (per CPU worker pool)早就创建好了，只需根据优先级进行绑定即可。但对于 unbound workqueue，其对应的 worker pool 是动态创建的。为了统一，不同属性的 workqueue 使用不同的 worker pool，如果有合适的 worker pool，直接绑定即可，否则需要进行创建：

> => get_unbound_pool                                                 获取一个符合要求的 worker pool
>     => wqattrs_hash                                                 计算属性的 hash 值
>     => 遍历 unbound_pool_hash 中 hash 值相同的 pool 链表，如果属性相同，返回该 pool
>     => 创建新的 worker pool
>         => 如果属性中 cpumask 都在同一个 node 上，设置 pool->node 为该 node，否则为 NUMA_NO_NODE
>         => worker_pool_assign_id                                    为 pool 分配 id，在为 worker 内核线程起名时会用到
>         => create_worker                                            为 pool 创建 worker
>         => hash_add(unbound_pool_hash, &pool->hash_node, hash)      将 pool 加入到 unbound_pool_hash 中，key 为先前计算出的 hash 值
> => kmem_cache_alloc_node                                            从对应 node 上的 slab 中(NUMA 亲和性)分配 pool_workqueue
> => init_pwq                                                         初始化，绑定 worker_pool


#### 创建 worker pool

对于 per CPU worker pool，它们是静态定义的：


```
NR_STD_WORKER_POOLS = 2,
static DEFINE_PER_CPU_SHARED_ALIGNED(struct worker_pool [NR_STD_WORKER_POOLS], cpu_worker_pools);
```

而对于 unbound worker pool ，创建将推迟到和 workqueue 关联时，发现相应属性(hash 值)的 worker pool 不存在，于是进行创建：

apply_workqueue_attrs => alloc_unbound_pwq => get_unbound_pool =>


> ...
>     /* nope, create a new one */
>     pool = kzalloc_node(sizeof(*pool), GFP_KERNEL, target_node);
>     if (!pool || init_worker_pool(pool) < 0)
>         goto fail;

#### 销毁关联结构 pool_workqueue 和 worker pool

当 unbound workqueue 的属性发生变化时，需要为其关联新的 worker pool，因此会创建新的 pool_workqueue ，如果该属性的 pool 不存在，同样进行创建。

那对于原来的 pool_workqueue 和 worker pool，应该如何处理呢？这需要查看 apply_wqattrs_cleanup ，它会对每个 node 调用 put_pwq_unlocked，如果 node 存在相应的 pool_workqueue，则调用 put_pwq，将 pwq->unbound_release_work 加入到 system_wq 。通过 workqueue 来实现 workqueue 的功能，有点自举的味道在里面。

unbound_release_work 绑定的函数为 pwq_unbound_release_workfn ，首先它会通过 rcu_free_pwq 将 pool_workqueue 销毁掉，然后调用 put_unbound_pool 减少 worker pool 的引用计数，如果降到 0，则通过 rcu_free_pool 销毁 worker pool 。

#### 创建 worker

每个 worker pool 至少应该有一个 worker 在那里等活干：

对于 per CPU worker pool 的 worker ，第一个 worker 早在 CPU prepare 阶段就通过 workqueue_prepare_cpu => create_worker 创建 。

对于 unbound worker pool 的 worker ，在 apply_workqueue_attrs => alloc_unbound_pwq => get_unbound_pool 创建 worker pool 时会通过 create_worker 创建。

当 worker pool 中的 worker 不够时，也会创建 worker ，这点在后文会提到。

##### create_worker

创建 worker 。


> => alloc_worker                 为 worker 结构分配内存并初始化
> => 根据 worker 所属 pool 设置其内核线程的名称 (ps 和 top 打印出来的名字)
> => kthread_create_on_node       创建内核线程，指定相关数据结构的内存从 worker pool 所在的 node 上分配(为了 NUMA 亲和性)
> => set_user_nice                根据 pool 的属性设置进程优先级
> => worker_attach_to_pool        将 worker 加入到 worker pool 中
> => worker->pool->nr_workers++   增加 pool 的 worker 计数
> => worker_enter_idle            尝试让 worker 进入 idle 状态，因为刚创建暂时还没 work 可以干
> => wake_up_process              唤醒 worker 对应的内核线程


#### 加入 workqueue

将一个 work 加入到 workqueue 有多个 API ，它们的功能不同：


> // 将 work 加入到特定 workqueue，要求在特定 CPU 上运行
> bool queue_work_on(int cpu, struct workqueue_struct *wq, struct work_struct *work);
> 
> // 将 work 加入到特定 workqueue ，CPU 无所谓
> static inline bool queue_work(struct workqueue_struct *wq, struct work_struct *work);
> 
> // 在一段时间后将 work 加入到特定 workqueue
> bool queue_delayed_work_on(int cpu, struct workqueue_struct *wq, struct delayed_work *dwork, unsigned long delay);
> 
> // 将 work 加入到全局 workqueue ，即 system_wq
> static inline bool schedule_work(struct work_struct *work);

后几个最后都会调用到第一个最基本的接口 ：


```
bool queue_work_on(int cpu, struct workqueue_struct *wq,
           struct work_struct *work)
{
    bool ret = false;
    unsigned long flags;

    local_irq_save(flags);

    if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))) {
        __queue_work(cpu, wq, work);
        ret = true;
    }

    local_irq_restore(flags);
    return ret;
}
```

一个 work 只会被挂入到一个 workqueue 中，为此检查它是否当前已经挂在 workqueue 中，如果没有，才调用 `__queue_work` 进行挂入。

##### __queue_work


> => 如果 workqueue 设置了 __WQ_DRAINING，表示 workqueue 当前进行清理工作，准备销毁，此时不允许再挂入新 work，返回 (唯一例外是 work 是被该 queue 中的 work 所添加的)
> => 如果 work 加入时未指定要运行的 CPU，通过 wq_select_unbound_cpu 进行选择，默认使用当前 CPU 。如果该 CPU 不在 wq_unbound_cpumask (全局 cpumask)内，则从 wq_unbound_cpumask 中通过 round robin 方式选择
> => 对于 bound workqueue ，取出当前 per CPU 变量中的 pool_workqueue 。对于 unbound workqueue，取出当前 CPU 所在 node 对应的 pool_workqueue
> => last_pool = get_work_pool                            获取 work 上次所在的 worker poll
> => 如果有 last_pool 但不是当前选择 pool_workqueue 所对应的 worker pool，则其当前可能正在别的 worker pool 上的 worker 执行
>     => find_worker_executing_work                       寻找正在执行它的 worker
>     => 如果它确实在某个 worker 上执行，为了保证不会发生重入，只能选择该 worker 所在的 pool_workqueue，即把 work 加到其当前正在运行所在的 worker pool 中
> => 对于 unbound pool_workqueue ，可能此时恰好被销毁，因此检查引用计数，如果为 0，则重新选择 pool_workqueue
> => 新加入 work 的颜色为要加入 pool_workqueue 当前的颜色，增加该颜色在 nr_in_flight 的计数
> => 如果当前活跃的 worker 数大于设置的 max_active ，则不能执行，只能加入到 delayed_works 链表里等着，否则可以执行，加入到 pool 的 worklist 链表
> => insert_work                                          将 work 挂入到 pool_workqueue 所指向的 worker pool 中
>     => set_work_pwq(work, pwq, extra_flags)             将 pool_workqueue 更新到 work 的 data 中
>     => list_add_tail(&work->entry, head)                将 work 加入到 worker pool 的 worklist 链表中
>     => 如果 worker pool 当前处于可运行状态的 worker 数为 0，通过 wake_up_worker 唤醒处于 idle 状态的 worker


往 workqueue 里塞 work 时，会比较关联结构 pool_workqueue 维护的 nr_active 和 max_active ，判断该 workqueue 当前被加到该 worker pool 中的 work 数是否已经超过了 max_active 的限制。如果 nr_active >= max_active，表示达到了限制，于是当前 work 不能被加入到后端，而是放到 pool_workqueue 的 delayed_works 链表中进行等待。否则在增加 nr_active 计数后，加入到 worker pool 的 worklist 中，被 worker 所执行。

需要注意的是，唯一能够减少 nr_active 的地方 `pwq_dec_nr_in_flight => nr_active--` 位于 process_one_work 的最后一行，也就是说，只有当 work 执行完成后，先前达到限制的 workqueue 才能把后续的 work 放到该 worker pool 中。这意味着 max_active 限制的其实是 workqueue 在每个 worker pool 中创建的上下文数，比如 max_active 为 3，那么该 workqueue 把 3 个 work 放到一个 worker pool 中后，就不能再放了，即使这三个 work 可能处于阻塞状态，但此时不会影响 worker pool 的正常工作，其他 workqueue 依然可以把 work 放到该 worker pool 中，并根据需要创建 worker 进行执行。这样就实现了对 workqueue 并发度的限制。

紧接在 `nr_active--` 后的是 pwq_activate_first_delayed，因此一旦有空位，会把先前在 delayed_works 中排队等待的 work 依次取出进行处理。

#### worker 执行 work

在 create_worker 中创建 worker kthread 时，指定的函数为 worker_thread ，它本质上是一个通过 goto 实现的循环，在每轮循环中执行以下逻辑：


> => 被唤醒，开始执行代码
> => 如果被设置了 WORKER_DIE flags，表示 worker 已死亡，进行清理工作，从 pool 中移除，然后退出循环
> => worker_leave_idle                    声明 worker 已经离开了 idle 状态
> => need_more_worker                     检查是否需要干活，主要是判断所属 pool 的 worklist 不为空且当前可运行的 worker 数为 0 (没有 worker 或它们当前被阻塞)
>     => 如果不需要，跳转到 sleep 标签
>         => worker_enter_idle            重新进入 idle 状态
>         => schedule
> => manage_workers                       如果当前没有 idle worker，则对 worker pool 执行管理操作
>     => maybe_create_worker              创建 worker 直到够用为止
> => 从 pool 的 worklist 中取出第一个 work 来执行
>     => 如果 work 设置了 WORK_STRUCT_LINKED flag，表示和其他 work 相关联，将其挂到 worker 的 scheduled 链表中，然后调用 process_scheduled_works 进行处理
>         => process_scheduled_works 会不断取出 scheduled 链表中的第一个 work，通过 process_one_work 执行
>     => 否则直接通过 process_one_work 执行
>     => 隐含的逻辑是：对于没有关联的 work ，优先执行，否则请再到另外一个队伍(scheduled 链表)上排队执行


worker pool 需要保证存在 idle worker ，用来执行随时可能到来的 work 。但执行这种行为的不是 worker pool 本身，因为 worker pool 本身并没有执行实体。因此它的工作由 worker 来代劳。当 worker 被唤醒时，发现没有 idle worker，则化身为 manager，执行管理操作：创建 worker ，直到够用为止(有 idle worker / 有可以执行的 worker / worklist 为空)。一个 worker pool 同一时刻只允许有一个 manager，这通过 POOL_MANAGER_ACTIVE flag 控制，如果 worker 发现 pool 设置该 flag 则不能成为 manager。

在过了一把 manager 瘾后，worker 才开始干自己正事：处理 work。

为什么要这样做呢？前文提到，当 worklist 中有 work 要做时，idle worker 被唤醒处理 work，这时该 worker 不能再算是 idle worker，如果一上来就去执行 work ，万一被阻塞，岂不是再也没 worker 可用了？要知道 worker pool 本身可不会自己创建 worker 。因此 worker 脱离 idle 状态后会先尝试成为 manager 去创建 worker，以保证始终存在 idle worker，如果还有 work 要处理，则新的 idle worker 同样会在处理前创建新的 worker 。

此外，worker_thread 在开头有一行只执行一次的逻辑：`worker->task->flags |= PF_WQ_WORKER;` ，它通过为 worker 内核线程打上标记，告诉调度器：我是一个 worker 内核线程，当该内核线程进入睡眠时，为了避免阻塞其他 work，应该唤醒其他 worker 来处理，这体现在 `__schedule` 中：


``` c
	    /*
     * If a worker went to sleep, notify and ask workqueue
     * whether it wants to wake up a task to maintain
     * concurrency.
     */
    if (prev->flags & PF_WQ_WORKER) {
        struct task_struct *to_wakeup;

        to_wakeup = wq_worker_sleeping(prev);
        if (to_wakeup)
            try_to_wake_up_local(to_wakeup, &rf);
    }
```

wq_worker_sleeping 首先减少 pool->nr_running 的计数，如果为 0 ，表示当前 pool 没有 worker 在运行，如果此时 worklist 非空，则需要唤醒 worker 来处理 work。于是返回第一个 idle worker，通过 try_to_wake_up_local 唤醒之。

#### 销毁 worker

worker pool 为了保证存在 idle worker ，当 worklist 上有很多 work 要做，而 worker 们却由于阻塞导致没有空闲 worker 时，会通过上述机制疯狂创建 worker 。一旦高峰期过去，worker 们不阻塞并把 work 做完了，而 worklist 上又没有那么多 work 要做了，于是 worker 们纷纷进入 idle 状态。此时需要裁员，干掉一些无所事事的 worker 。

worker pool 在初始化(init_worker_pool) 时设置了 timer ，回调函数为 idle_worker_timeout 。

前文提到，当 worker 进入 idle 状态时，会调用 worker_enter_idle ，除了将自身加入 idle_list 外，还会通过 too_many_workers 判断是否有太多 worker 。如果是且 pool 的 timer 未设置，则设置 timer ，在 IDLE_WORKER_TIMEOUT (默认为 300 * HZ = 5 min) 后触发。触发时，回调函数 idle_worker_timeout 被调用。

它会从 idle_list 中依次取出 worker，通过 destroy_worker 进行销毁，直到非 too_many_workers 为止。同时将 timer 的超时时间设置为 最后一个删除 worker 进入 idle 状态的时间 + IDLE_WORKER_TIMEOUT 。

##### too_many_workers

怎么判定有太多 worker 呢？ `nr_idle > 2 && (nr_idle - 2) * MAX_IDLE_WORKERS_RATIO >= nr_busy` 。

即 idle worker 数大于 2 或 (idle worker 数 - 2) 是 busy worker 数的 MAX_IDLE_WORKERS_RATIO(4) 倍。

这里 idle worker 包括 manager，而 busy worker 数等于总 worker 数减去 idle worker 数。

#### rescuer

当内存不足(比如正在进行内存回收)时，动态创建 worker 可能失败，导致 work 无法得到执行，为此可通过为 workqueue 指定 WQ_MEM_RECLAIM 来避免这种情况。

在实现上，会在创建 workqueue (`__alloc_workqueue_key`) 时提前创建名为 rescuer 的 worker 。rescuer ，顾名思义就是拯救者，负责拯救那些因为因无法创建 worker 可用而无法执行的 work 。

那 rescuer 是如何感知到哪个 pool 需要拯救的呢？前面提到 worker 会尝试成为 manager，然后在 maybe_create_worker 中创建 worker 直到够用为止。但如果一直不够用，则一直循环出不来。因此 worker pool 维护了 mayday_timer 。在开始创建 worker 前启动 timer，在创建完成退出循环时取消 timer ，如果循环时间超过了 MAYDAY_INITIAL_TIMEOUT，则 pool_mayday_timeout 会被调用。

pool_mayday_timeout 会遍历 pool worklist 中的 work ，调用 send_mayday 。其找出 work 所在 workqueue，如果 workqueue 有 rescuer，则将其加入到 workqueue 的 maydays 链表中，然后唤醒 workqueue 的 rescuer 。

rescuer 线程执行的函数为 rescuer_thread 。

##### rescuer_thread

rescuer 线程不断执行以下循环，直到要退出为止：


> => 遍历所在 wq 的 maydays 链表，得到连接到那些无法创建 worker 的 pool 的 pool_workqueue
>     => 取出 maydays 链表中的第一个 pool_workqueue，将其从 wq 的 maydays 链表中移除
>     => __set_current_state(TASK_RUNNING)        rescuer 进程进入运行状态
>     => worker_attach_to_pool                    将 rescuer 加入到要拯救的 pool 中
>     => 遍历 worker pool 的 worklist，将其中的 work 移到 rescuer 的 scheduled 链表中
>     => 遍历 scheduled 链表
>         => process_scheduled_works              处理 work
>         => 如果处理后，pool 依然 need_to_create_worker，则其依然处于无法创建 worker 的状态，将其加回 wq 的 maydays 链表
>     => 如果 need_more_worker ，通过 wake_up_worker 唤醒 pool 上的普通 worker
>     => worker_detach_from_pool                  rescuer 从要拯救的 pool 中脱离
> => schedule

rescuer 并不是大公无私的 worker，它只会处理属于它所属 workqueue 的 work。同时由于 rescuer 只有一个，因此当它阻塞后，该 workqueue 上的 work 依然无法得到执行。

#### flush_workqueue

清空 workqueue ，确保其中的 work 都被执行完。


> => wq_flusher                                                               创建新 flusher
> => next_color = work_next_color                                             选择下一种 work color
> => 如果新选择的 work_color 和 workqueue 当前的 flush_color 不同
>     => this_flusher.flush_color = wq->work_color                            将当前的 work_color 设置到 flusher
>     => wq->work_color = next_color                                          并更新 workqueue.work_color 为新选择的颜色
>     => 如果 workqueue 当前没有 flusher，成为 first flusher
>         => flush_workqueue_prep_pwqs(wq, wq->flush_color, wq->work_color)   为 workqueue 所有的 pool_workqueue 更新 flush_color 和 work_color
>     => 否则表示有 flusher 在工作，将 flusher 加入到 flusher_queue 链表中
>         => flush_workqueue_prep_pwqs(wq, -1, wq->work_color)                -1 表示不更新，只更新 work_color
> => 否则表示所有颜色都被选过了，只能先将 flusher 加入到 flusher_overflow 链表中
> => wait_for_completion(&this_flusher.done)                                  等待轮到当前 flusher 执行
> => 如果当前 flusher 不是第一个，返回
> => 循环至 flusher_queue 为空
>     => 上一轮 flush 已做完，将 flusher_queue 中所有和当前 flush_color(上一轮 flush 的 color) 相同的 flusher 移除
>     => wq->flush_color = work_next_color(wq->flush_color)
>     => 如果 flusher_overflow 非空
>         => 修改它们的 flush_color 为当前 work_color，将它们挪到 flusher_queue 链表中
>         => flush_workqueue_prep_pwqs(wq, -1, wq->work_color)
>     => 如果 flusher_queue 为空，返回
>     => wq->first_flusher = next              设置下一个 flusher 为第一 flusher
>     => flush_workqueue_prep_pwqs(wq, wq->flush_color, -1)      如果还有下一种颜色要 flush，退出，让下一个 flusher 来

定义了 WORK_STRUCT_COLOR_BITS 个 bit 用于着色，即一共有 WORK_NR_COLORS = (1 << WORK_STRUCT_COLOR_BITS) - 1 种颜色(最后一种表示没颜色)。

每个 workqueue 有自己的颜色，每次启动 flush，颜色都会变化。而 work 的颜色取决于加入到 workqueue 时的颜色 (work_color)。这样就形成了区分： 如果 work 是在某次 flush 启动之后加入的，那么其颜色必然不同于 flusher 的 flush_color (等于之前的 work_color)，于是不会在这轮中被清除。

允许有多个 flusher 同时存在，但需要排队执行，同一时刻只有一个 flusher 能得到执行，这是通过 completion 来控制的：flush_workqueue 函数会阻塞在 `wait_for_completion(&this_flusher.done)` 等待。在 flush_workqueue_prep_pwqs 中会判断是否有新加入的 work，如果有，通过 `complete(&wq->first_flusher->done)` 让排在最前面的第一个 flusher 开始执行。

##### flush_workqueue_prep_pwqs

更新 workqueue_struct 的 flush_color 和 work_color，为 flush workqueue 做好准备


> => 如果 flush_color 非负，则 nr_pwqs_to_flush 必须为 0
>     => atomic_set(&wq->nr_pwqs_to_flush, 1)
> => 遍历 workqueue 所有的 pool_workqueue
>     => 如果参数 flush_color 非负，则 pool_workqueue.flush_color 必须为 -1，如果 pool_workqueue 在该颜色上还有命令未执行，则设置其 flush_color 为参数 flush_color，增加 nr_pwqs_to_flush 计数
>     => 如果参数 work_color 非负，则 work_color 必须为 pool_workqueue.work_color 的下一个颜色，设置其 work_color 为参数 work_color
> => 如果 flush_color 非负，且 nr_pwqs_to_flush 大于 1
>     => complete(&wq->first_flusher->done)                   让第一个 flusher 开始执行


#### 取消准备 / 已经加入到 workqueue 的 work

取消一个 work，让其不要执行。

对于普通 work，调用 cancel_work ，而对于 delayed work，调用 cancel_delayed_work。它们都会调用 `__cancel_work`，只是 is_dwork 参数不同

##### __cancel_work


> => try_to_grab_pending                  尝试将 work 从 worklist 中取出来
>     => del_timer                        对于 delayed work，清除其绑定的定时器即可，返回
>     => 如果 work 不属于 pending 状态（work data 未设置 WORK_STRUCT_PENDING_BIT)，直接返回
>     => get_work_pool                    根据 work data 获取所在的 worker pool
>     => 将 work 从 worklist 上删除
>     => set_work_pool_and_keep_pending   设置 work data 中在所在 pool bit，并设置 pending bit
> => set_work_pool_and_clear_pending      清除 work data 中在所在 pool bit，并清除 pending bit

如果一个 work 已经被执行，表现为它的 pending bit 被清除，那么已经无可挽回，取消失败。

## 总结

在 CMWQ 中，通过引入 worker pool，实现了对 workqueue 机制中 生产者(workqueue) 和 消费者(worker) 的解耦。worker 的生命周期不再受 workqueue 的控制，而是由相应的 worker pool 来管理。而 workqueue 在创建时会和特定的 workpool 建立关联 (pool_workqueue)，work 在添加时会顺着该关联由相应 worker pool 管理的 worker 来做。

当用户把 work 添加到 workqueue 后，会确定被放到哪个 worker pool 中执行，但无法确定被哪个 worker 执行。实际上 work 有两个去向：如果 workqueue 在选中 worker pool 上正在运行的 work 数目未达到设定的并发上限 max_active，则会放到对应 worker pool 的 worklist 中；如果达到了 max_active，则会暂时放到关联结构 pool_workqueue 的 delayed_works 中，等稍后再加入 worker pool。而 worker pool 只要 worklist 中有 work，就会唤醒 worker 去执行上面的 work。worker pool 始终会保持至少有一个 worker 处于空闲状态，随时应对新添加的 work。如果 worker 由于 work 的某些操作被阻塞了，则 worker pool 唤醒那个 idle 的 worker 去执行下一个 work，当然在此之前会创建一个新的 idle worker 以满足保持至少有一个 idle worker 的要求。

虽然 CMWQ 在设计上更加合理，理论开销也更小，但代价是增加了代码的复杂度，仅 workqueue.c 中的代码，就达到了 5500+ 行。但考虑越来越多的内核模块(如 driver)依赖于 workqueue 来完成异步任务，这些付出都是值得的。

最后特别感谢 wowotech 四篇对 workqueue 机制深入浅出的分析，对我理解 CMWQ 的代码帮助很大，地址在参考中已经列出。

## 参考

Documentation/workqueue.txt

Understanding Linux kernel

https://lwn.net/Articles/355700/

http://www.wowotech.net/irq_subsystem/workqueue.html

http://www.wowotech.net/irq_subsystem/cmwq-intro.html

http://www.wowotech.net/irq_subsystem/alloc_workqueue.html

http://www.wowotech.net/irq_subsystem/queue_and_handle_work.html

http://kernel.meizu.com/linux-workqueue.html

https://stackoverflow.com/questions/14965513/what-happens-when-kernel-delayed-work-is-rescheduled
