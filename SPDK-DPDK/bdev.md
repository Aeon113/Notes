SPDK: 添加新的bdev类型: SPDK_BDEV_MODULE_REGISTER

这个宏定义一个static函数，以 __attribute__(constructor)修饰:

```C
/*
 *  Macro used to register module for later initialization.
 */
#define SPDK_BDEV_MODULE_REGISTER(name, module) \
static void __attribute__((constructor)) _spdk_bdev_module_register_##name(void) \
{ \
  spdk_bdev_module_list_add(module); \
} \

```

其中的module:

```C
static struct spdk_bdev_module udisk_if = {
  .name = "udisk",
  .module_init = bdev_udisk_initialize,
  .module_fini = bdev_udisk_finish,
  .config_text = bdev_udisk_get_spdk_running_config,
  .get_ctx_size = bdev_udisk_get_ctx_size,
};

SPDK_BDEV_MODULE_REGISTER(udisk, &udisk_if)
```

-------------

bdev subsystem注册:

```C
static struct spdk_subsystem g_spdk_subsystem_bdev = {
  .name = "bdev",
  .init = bdev_subsystem_initialize,
  .fini = bdev_subsystem_finish,
  .write_config_json = bdev_subsystem_config_json,
};

SPDK_SUBSYSTEM_REGISTER(g_spdk_subsystem_bdev);
SPDK_SUBSYSTEM_DEPEND(bdev, accel)
SPDK_SUBSYSTEM_DEPEND(bdev, vmd)
SPDK_SUBSYSTEM_DEPEND(bdev, sock)

/**
 * \brief Register a new subsystem
 */
#define SPDK_SUBSYSTEM_REGISTER(_name) \
  __attribute__((constructor)) static void _name ## _register(void) \
  {                 \
    spdk_add_subsystem(&_name);         \
  }

void
spdk_add_subsystem(struct spdk_subsystem *subsystem)
{
  TAILQ_INSERT_TAIL(&g_subsystems, subsystem, tailq);
}
```

所以，subsystem注册其实是就是将`struct spdk_subsystem`放入 `g_subsystems` (`struct spdk_subsystem_list g_subsystems = TAILQ_HEAD_INITIALIZER(g_subsystems);`)。

-----------------

各subsystem初始化:

```
spdk_app_start() -> spdk_thread_send_msg(g_app_thread, bootstrap_fn, NULL);

bootstrap_fn()
  └ 在 g_delay_subsystem_init == false 时: spdk_subsystem_init(app_start_rpc, NULL)
      └  spdk_subsystem_init(app_start_rpc, NULL)
          └ 设置 g_subsystem_start_fn 和 g_subsystem_start_arg。本处即将它们设置为 app_start_rpc 和 NULL
          └ 检查各subsystem间的依赖关系。对全局变量g_subsystems中的子系统进行排序，排序的依据是子系统之间的依赖关系，这些依赖关系存储在全局变量g_subsystems_deps中。
          └ spdk_subsystem_init_next(0)
              └ 在 参数 (rc) 为0时: g_subsystem_start_fn(rc, g_subsystem_start_arg); (本处不会触发此逻辑)
              └ 在判定所有subsystem均初始化完成后: g_subsystem_start_fn(rc, g_subsystem_start_arg); (本处不会触发此逻辑)
              └ 从 g_subsystems中选择第一个未初始化的subsystem，调用其init回调。如果init不存在，就跳过，再次调用本函数(`spdk_subsystem_init_next(0)`)，尝试初始化下一个subsystem。
```

--------

bdev subsystem的初始化:

```
bdev_subsystem_initialize()
  └ spdk_bdev_initialize(bdev_initialize_complete, NULL);
      └ 设置 g_init_cb_fn 和 g_init_cb_arg，本处这两个值被设置为 bdev_initialize_complete 和 NULL
      └ Register new spdk notification types: "bdev_register" 和 "bdev_unregister"
      └ spdk_iobuf_register_module("bdev")
      └ spdk_io_device_register(&g_bdev_mgr, bdev_mgmt_channel_create,
				bdev_mgmt_channel_destroy,
				sizeof(struct spdk_bdev_mgmt_channel),
				"bdev_mgr"); // 现在不清楚这段代码的意义
      └ bdev_modules_init();
      └ g_bdev_mgr.module_init_complete = true;
      └ bdev_module_action_complete();
```

-------

```c
// lib/bdev/bdev.c

struct spdk_bdev_mgr {
	struct spdk_mempool *bdev_io_pool; // 

	void *zero_buffer;

	TAILQ_HEAD(bdev_module_list, spdk_bdev_module) bdev_modules;

	struct spdk_bdev_list bdevs;
	struct bdev_name_tree bdev_names;

	bool init_complete;
	bool module_init_complete;

	struct spdk_spinlock spinlock;

	TAILQ_HEAD(, spdk_bdev_open_async_ctx) async_bdev_opens;

#ifdef SPDK_CONFIG_VTUNE
	__itt_domain	*domain;
#endif
};

static struct spdk_bdev_mgr g_bdev_mgr = {
	.bdev_modules = TAILQ_HEAD_INITIALIZER(g_bdev_mgr.bdev_modules),
	.bdevs = TAILQ_HEAD_INITIALIZER(g_bdev_mgr.bdevs),
	.bdev_names = RB_INITIALIZER(g_bdev_mgr.bdev_names),
	.init_complete = false,
	.module_init_complete = false,
	.async_bdev_opens = TAILQ_HEAD_INITIALIZER(g_bdev_mgr.async_bdev_opens),
};
```

在 SPDK (Storage Performance Development Kit) 中，`g_bdev_mgr.bdev_io_pool` 是一个全局变量，用于管理 `struct spdk_bdev_io` 类型的内存池。

`struct spdk_bdev_io` 是一个结构体，用于表示一个块设备 I/O 操作。每当 SPDK 需要执行一个新的块设备 I/O 操作时，它会从 `g_bdev_mgr.bdev_io_pool` 中分配一个 `struct spdk_bdev_io`。

使用内存池的好处是，当频繁地需要分配和释放同一类型的对象时，内存池可以提供更高效的内存使用。它通过预先分配一大块内存，并将其划分为多个小的对象，来避免频繁的内存分配和释放操作。

总的来说，`g_bdev_mgr.bdev_io_pool` 的作用是管理 `struct spdk_bdev_io` 类型的内存池，以高效地支持块设备 I/O 操作。

------

在 SPDK (Storage Performance Development Kit) 中，`g_bdev_mgr.zero_buffer` 是一个全局变量，它是一个预先分配的、填充了零值的内存缓冲区。

这个零值缓冲区在一些情况下非常有用。例如，当 SPDK 需要向一个块设备写入一块全零的数据时，它可以直接使用 `g_bdev_mgr.zero_buffer`，而不需要额外分配一个新的缓冲区并将其填充为零。

总的来说，`g_bdev_mgr.zero_buffer` 的作用是提供一个预先分配的、填充了零值的内存缓冲区，以便在需要全零数据时使用。

------

`spdk_io_device_register` 是 SPDK (Storage Performance Development Kit) 中的一个函数，用于注册一个 I/O 设备。注册设备后，SPDK 可以为该设备创建和管理 I/O 通道。

函数的原型如下：

```c
void spdk_io_device_register(void *io_device, spdk_io_channel_create_cb create_cb,
                             spdk_io_channel_destroy_cb destroy_cb, uint32_t ctx_size,
                             const char *name);
```

其中，`io_device` 是要注册的 I/O 设备，`create_cb` 和 `destroy_cb` 是创建和销毁 I/O 通道时的回调函数，`ctx_size` 是每个 I/O 通道的上下文大小，`name` 是设备的名称。

在 SPDK 中，I/O 通道（channel）是一个重要的概念。每个 I/O 通道都关联一个 I/O 设备和一个线程。当线程需要对设备进行 I/O 操作时，它会通过关联的 I/O 通道进行。

I/O 通道的目的是将 I/O 设备的访问局部化到特定的线程，从而避免多线程访问同一设备时的竞争条件。每个线程都有自己的 I/O 通道，可以独立地对设备进行操作，而不需要担心其他线程的干扰。

总的来说，`spdk_io_device_register` 的作用是注册一个 I/O 设备，以便 SPDK 可以为该设备创建和管理 I/O 通道。

------

在 SPDK (Storage Performance Development Kit) 中，I/O 设备（IO Device）是一个抽象的概念，它代表任何可以进行 I/O 操作的设备。

一个 I/O 设备可以是一个物理设备，例如一个硬盘或者一个网络接口卡。它也可以是一个虚拟设备，例如一个文件或者一个内存区域。

在 SPDK 中，每个 I/O 设备都可以关联一个或多个 I/O 通道（IO Channel）。每个 I/O 通道都关联一个线程，当线程需要对设备进行 I/O 操作时，它会通过关联的 I/O 通道进行。

通过这种方式，SPDK 可以将 I/O 设备的访问局部化到特定的线程，从而避免多线程访问同一设备时的竞争条件。每个线程都有自己的 I/O 通道，可以独立地对设备进行操作，而不需要担心其他线程的干扰。

总的来说，I/O 设备是 SPDK 中的一个抽象概念，它代表任何可以进行 I/O 操作的设备。

------------

```c
static int
bdev_modules_init(void)
{
	struct spdk_bdev_module *module;
	int rc = 0;

	TAILQ_FOREACH(module, &g_bdev_mgr.bdev_modules, internal.tailq) {
		g_resume_bdev_module = module;
		if (module->async_init) {
			spdk_spin_lock(&module->internal.spinlock);
			module->internal.action_in_progress = 1;
			spdk_spin_unlock(&module->internal.spinlock);
		}
		rc = module->module_init();
		if (rc != 0) {
			/* Bump action_in_progress to prevent other modules from completion of modules_init
			 * Send message to defer application shutdown until resources are cleaned up */
			spdk_spin_lock(&module->internal.spinlock);
			module->internal.action_in_progress = 1;
			spdk_spin_unlock(&module->internal.spinlock);
			spdk_thread_send_msg(spdk_get_thread(), bdev_init_failed, module);
			return rc;
		}
	}

	g_resume_bdev_module = NULL;
	return 0;
}
```

```c
static void
bdev_module_action_complete(void)
{
	/*
	 * Don't finish bdev subsystem initialization if
	 * module pre-initialization is still in progress, or
	 * the subsystem been already initialized.
	 */
	if (!g_bdev_mgr.module_init_complete || g_bdev_mgr.init_complete) {
		return;
	}

	/*
	 * Check all bdev modules for inits/examinations in progress. If any
	 * exist, return immediately since we cannot finish bdev subsystem
	 * initialization until all are completed.
	 */
	if (!bdev_module_all_actions_completed()) {
		return;
	}

	/*
	 * Modules already finished initialization - now that all
	 * the bdev modules have finished their asynchronous I/O
	 * processing, the entire bdev layer can be marked as complete.
	 */
	bdev_init_complete(0);
}

static bool
bdev_module_all_actions_completed(void)
{
	struct spdk_bdev_module *m;

	TAILQ_FOREACH(m, &g_bdev_mgr.bdev_modules, internal.tailq) {
		if (m->internal.action_in_progress > 0) {
			return false;
		}
	}
	return true;
}

static void
bdev_init_complete(int rc)
{
	spdk_bdev_init_cb cb_fn = g_init_cb_fn;
	void *cb_arg = g_init_cb_arg;
	struct spdk_bdev_module *m;

	g_bdev_mgr.init_complete = true;
	g_init_cb_fn = NULL;
	g_init_cb_arg = NULL;

	/*
	 * For modules that need to know when subsystem init is complete,
	 * inform them now.
	 */
	if (rc == 0) {
		TAILQ_FOREACH(m, &g_bdev_mgr.bdev_modules, internal.tailq) {
			if (m->init_complete) {
				m->init_complete();
			}
		}
	}

	cb_fn(cb_arg, rc);
}
```

------------

研究 bdev aio module init:

```c
static struct spdk_bdev_module aio_if = {
	.name		= "aio",
	.module_init	= bdev_aio_initialize,
	.module_fini	= bdev_aio_fini,
	.get_ctx_size	= bdev_aio_get_ctx_size,
};

SPDK_BDEV_MODULE_REGISTER(aio, &aio_if)
```

因此 module init 的起始函数是 `bdev_aio_initialize()`

```c
static int
bdev_aio_initialize(void)
{
	spdk_io_device_register(&aio_if, bdev_aio_group_create_cb, bdev_aio_group_destroy_cb,
				sizeof(struct bdev_aio_group_channel), "aio_module");

	return 0;
}
```

-----------

- `io_device`：这是一个抽象的概念，代表任何可以进行I/O操作的设备。这可以是一个物理设备（如NVMe设备），也可以是一个虚拟设备（如bdev层的一个虚拟块设备）。`io_device`是通过`spdk_io_device_register`函数注册到SPDK的I/O设备管理系统中的。

- `io_channel`：这是一个抽象的通道，用于在特定的`io_device`和`spdk_thread`之间进行I/O操作。每个`io_channel`都有一个与之关联的`io_device`和`spdk_thread`。`io_channel`是通过`spdk_get_io_channel`函数获取的。

- `reactor`：这是SPDK的事件驱动框架的核心组件。每个`reactor`都运行在一个单独的lcore上，并且管理一个或多个`spdk_thread`。

- `spdk_thread`：这是SPDK的线程抽象。每个`spdk_thread`都运行在一个`reactor`中，并且可以处理一个或多个`io_channel`。

- `lcore`：这是DPDK（SPDK的底层库）的一个概念，代表一个逻辑核心。每个`reactor`都运行在一个单独的lcore上。

这些概念之间的关系可以简单地总结为：每个`reactor`运行在一个lcore上，并管理一个或多个`spdk_thread`。每个`spdk_thread`可以处理一个或多个`io_channel`，每个`io_channel`都与一个`io_device`关联。

---------

注意以下几个函数的签名:

```c
void
spdk_io_device_register(void *io_device, spdk_io_channel_create_cb create_cb,
			spdk_io_channel_destroy_cb destroy_cb, uint32_t ctx_size,
			const char *name);

void
spdk_io_device_unregister(void *io_device, spdk_io_device_unregister_cb unregister_cb);

struct spdk_io_channel *
spdk_get_io_channel(void *io_device);
```

可以看出, 这里的`void *io_device` (不是`struct io_device`)非常重要。它被用来定位"io_device"。

在 `spdk_io_device_register()`中，此函数会将参数`void *io_device`, `spdk_io_channel_create_cb create_cb`, `spdk_io_channel_destroy_cb destroy_cb`等包装进 `struct io_device`。

而在`spdk_get_io_channel()`中，此函数会根据传入的`io_device`，查找到对应的 `struct io_device`。然后再调用其`create_cb`回调，也就是刚才`spdk_io_device_register()`调用时传入的`create_cb`。

-------------

每个`spdk_thread`都有独立的 `io_channels`队列、各种pollers队列(`active_pollers`, `timed_pollers`, `paused_pollers`), `messages` ring/cache。

(spdk 也支持 interrupt模式，这个没研究过)

-----------------

`spdk_msg`会被放入`spdk_thread`，再由reactor执行；`spdk_event`则会直接由目标reactor执行。

-------------------

下发io请求时，除了需要一个`io_channel`，同时还需要相应块设备的`spdk_bdev_desc`。`spdk_bdev_desc`可以通过 `spdk_bdev_open()`或`spdk_bdev_open_ext()`得到。相应的，还有`spdk_bdev_close()`用来回收一个 `spdk_bdev_desc`。

----------------------

UCloud XClient SpdkGate 的初始化流程中的主要步骤:

这里的vhost 协商和建链和IO发送流程都是UCloud修改过的

首先启动spdk app, 获取各个配置信息

然后读取restore json文件。

文件读取完成后，再构建各udisk bdev，方法是对自己建立一个rpc 链接，然后发请求`construct_udisk_bdev`。

RPC请求使得某个reactor随后执行函数`spdk_rpc_construct_udisk_bdev`。

```
# construct udisk bdev 流程
- spdk_rpc_construct_udisk_bdev(), RPC 参数中带有udisk的extern_id等等信息
    - create_udisk_bdev()
      - 分配并初始化 bdev (struct udisk_bdev *), 以及 bdev->bdev (struct spdk_bdev) 中的部分字段，包括iov最大大小、每笔io最大大小等等
      - 调用 g_api.get_device(), 也就是 xclient::GetUDiskDevice(), 进入login流程。同时传入回调 create_udisk_bdev_continue
      - 进入 UdiskController线程，开始login流程(异步)。
- ...
- Login完成，进入回调cb, 也就是 create_udisk_bdev_continue()。
  - 向刚才处理rpc的reactor发送spdk_event, 执行create_udisk_bdev_done
- create_udisk_bdev_done()
  - 继续设置刚才提到的bdev和bdev_bdev, 包括设备大小等信息
  - 执行 spdk_io_device_register()，此后各spdk_thread将可以为此设备创建io channel。此函数的参数 create_cb = bdev_udisk_channel_create_cb, destroy_cb = bdev_udisk_channel_destroy_cb
  - spdk_bdev_register()
    - 调用 bdev_init() 和 bdev_start()，再次设置 struct spdk_bdev *bdev 中的各字段，并将其加入 TAILQ g_bdev_mgr.bdevs，随后进行 bdev examine。
  - 执行 bdev_ctx->create_cb，这是udisk module的私有结构的字段，在create_udisk_bdev中被设置，值为 spdk_rpc_construct_udisk_bdev_done
    - 发送RPC调用结果，表明此bdev创建成功
```

RPC发送后通过不断`spdk_jsonrpc_client_poll()`或重发 RPC 来确保块设备成功创建。

在restore json 文件中的所有块设备均初始化完成后，开始读取其中的各target记录，来创建target。

这里简介 vhost target 的创建:

```
# vhost target 创建流程:
rpc_vhost_create_blk_controller(), RPC 参数中带有相应vhost controller name, 块设备名(就是上方contruct出来的udisk bdev 的设备名, 每个vhost target都关联一个udisk bdev), cpu core mask
  - spdk_vhost_blk_construct()
    - 分配，初始化，设置 struct spdk_vhost_blk_dev *bvdev 和 struct spdk_vhost_dev *vdev，调用 spdk_bdev_open_ext() 获取 udisk bdev 的 desc, 并通过 spdk_bdev_desc_get_bdev() 获取相应的 struct spdk_bdev *
    - vhost_dev_register()
      - vhost_dev_thread_start()
        - 为cpu core mask中所提到的每一个core (目前只会传一个core)都创建一个spdk_thread。这些spdk_thread中的每一个都只能运行在指定的一个core上。每个spdk_thread 都对应一个新分配的 struct spdk_vhost_session_group *sgroup。
        - 设置 vdev->groups，把刚才分配出来的这些 sgroup添加到这个TAILQ中。
      - vhost_dev_set_coalescing()，不知道干啥的
      - vhost_register_unix_socket(), 创建相应的socket。SpdkGate和Guest应该是通过这个socket通信
      - 将当前vdev (struct spdk_vhost_dev *) 添加到 g_vhost_devices
  - 发送RPC调用结果，表明创建成功
```

然后是vhost connection 创建流程:

```
具体怎么创建vhost connection的，我也不知道。
但下方这个流程一定会走到:

vhost_new_connection_cb()
  - 获取 struct spdk_vhost_dev *vdev，分配并初始化 struct spdk_vhost_session *vsession，然后将 vsession 加入到 TAILQ vdev->vsessions
  - 向 g_vhost_init_thread 发送 spdk_msg: spdk_thread_send_msg(g_vhost_init_thread, vhost_register_vsession, vsession)
  - vhost_session_install_rte_compat_hooks(vsession), 这不知道是干啥的

vhost_register_vsession()
  - spdk_io_device_register(vsession, vhost_init_thread_session, vhost_fini_thread_session, sizeof(*vsession) + vdev->backend->session_ctx_size, vsession->name); 看起来像是注册了一个 spdk_io_device, 目的还不知道
  - 之前在 vhost_dev_thread_start()中提到了 vdev->groups 和里面的一批 spdk_thread。这里会给这些spdk_thread发送 spdk_msg: spdk_thread_send_msg(sgroup->thread, vhost_get_dummy_bdev_channel, vdev)

vhost_get_dummy_bdev_channel() 这个函数会在 vdev->groups 中的所有sgroup的spdk_thread中调用
  - spdk_get_io_channel(vdev->bdev)，获取此vhost controller对应的bdev的io channel。在xclient spdk gate中，这里会调用上方create_udisk_bdev_done()里提到的create_cb，也就是 bdev_udisk_channel_create_cb，但是不会直接调用，具体可以参考上方 spdk_get_io_channel() 的介绍。

bdev_udisk_channel_create_cb()
  - g_api.register_channel(bdev->udisk, ch->unique_id, bdev_udisk_register_channel_continue, channel_ctx), 也就是 xclient::RegisterChannel()
    - udisk_device->RegisterChannel(unique_id, cnt_cb) (xclient::UDiskDevice::RegisterChannel())
      - 对当前udisk device 的 所有extenthandle调用 RegisterChannel (eh->RegisterChannel(id, std::bind(&UDiskDevice::RegisterChannelDone, this, unique_id)))
        - io_submitter_->RegisterChannel(id, done_cb) (这里调用的函数即为 xclient::PollingIOSubmitter::RegisterChannel())
          - 创建2个ring: submit_ring_[id] = spdk_ring_create(SPDK_RING_TYPE_SP_SC, 8192, SPDK_ENV_SOCKET_ID_ANY); done_ring_[id] = spdk_ring_create(SPDK_RING_TYPE_SP_SC, 8192, SPDK_ENV_SOCKET_ID_ANY); 这里的id是在 UDiskDevice::RegisterChannel()中得到的，依据此函数执行时所在的lcore计算得到: uint32_t id = spdk_env_get_current_core() - spdk_env_get_first_core();
          - done_cb(), 也就是 void UDiskDevice::RegisterChannelDone()
            - ctx.completed_cb()
              - 执行 xclient::RegisterChannel()中的参数 cb, 也就是 bdev_udisk_register_channel_continue()
              - 向 channel 的 所在spdk_thread发送 spdk_msg: spdk_thread_send_msg(channel_ctx->thread_ctx, bdev_udisk_register_channel_done, ctx);
    - static_cast<xclient::SpdkArkController*>(g_ark_controller.get())->RegisterChannel(udisk_device, channel_id, cnt_cb)，这和方舟有关，这里不详述

bdev_udisk_register_channel_done()
  - 在当前channel 注册一个poller: udisk_io_ch->poller = SPDK_POLLER_REGISTER(udisk_io_poll, udisk_io_ch, 0)
```

然后注意一下 `int vhost_blk_start(struct spdk_vhost_session *vsession)`:

```
vhost_blk_start() // 应该会调用这个函数
  - vhost_session_send_event(vsession, vhost_blk_start_cb, vhost_session_start_done, 3, "start session")
    - 对当前vsession中的每一个spdk_vhost_session_group的spdk_thread, 发送spdk_msg: spdk_thread_send_msg(sgroup->thread, vhost_event_cb, &ev_ctx);

vhost_event_cb()
  - ctx->cb_fn(ctx->vdev, vsession, ctx->user_ctx); 也就是 vhost_blk_start_cb()
    - 注册poller, 函数为 vdev_worker: bvsession->requestq_poller = SPDK_POLLER_REGISTER(bvdev->bdev ? vdev_worker : no_bdev_vdev_worker, bvsession, 0)

vdev_worker()
  - 有时间接这些
```

有了 vhost connection之后，就可以开始收发IO了:

TODO: 接着写

-----------------

reactor初始化、启动、工作流程

reactor的初始化和启动:

```
这里参考spdk 24.01, 且只考虑polling模式而非interrupt模式

spdk_app_start()
  - spdk_reactors_init()
    - 初始化 g_spdk_event_mempool, 所有的spdk_event都将通过此mempool 分配 (通过 spdk_event_allocate())
    - 所有的reactor的管理结构 spdk_reactor 均以 数组的形式存在于 static struct spdk_reactor *g_reactors中。这里分配此数组。reactor的数量等于lcore的数量
    - 初始化 struct spdk_scheduler_core_info *g_core_infos;
    - spdk_thread_lib_init_ext()
      - 设置 g_thread_op_fn 和 g_thread_op_supported_fn, 分别设置为 reactor_thread_op 和 reactor_thread_op_supported。
    - 对每个lcore执行 reactor_construct()
      - 初始化此reactor, 主要包括:
        其lcore和 is_valid flag
        notify_cpuset (不知道干嘛的)
        TAILQ threads
        ring events
        interrupt机制
    - 设置 g_scheduling_reactor 为本lcore上的reactor


```
