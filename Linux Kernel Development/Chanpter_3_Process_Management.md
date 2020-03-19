# Process Management

The `state` field of tge oricess descriptor describes the currecnt condition of the process. Each process on the system is in exactly one of five different states. This value is represented by one of five flags:      

| State | Comment |
|:------|--------:|
| `TASK_RUNNING` | The process is either currently running or on a run-queu waiting to run. This is the only possible state for a process executing in user-space; it can also apply to a process in kernel-space that is actively running. |
| `TASK_INTERRUPTIBLE` | The process is sleeping (blocked), waiting for some condition to exist. When this condition exists, the kernel sets the process's state to `TASK_RUNNING`. The process also awakes prematurely and becomes runnable if it receives a signal. |
| `TASK_UNINTERRUPTABLE` | This is identical to `TASK_INTERRUPTABLE` except it dies not wake up and become runnable if it receives a signal. |
| `__TASK_TRACED` | This process is being traced by another process, such as a debugger, via ptrace. |
| `__TASK_STOPPED` | Process execution has stopped; the task is not running nor is it eligible to run. This occurs if the task receives the `SIGSTOP`, `SIGTSTP`, `SIGTTIN`, or `STGTTOU` signal or if it receives any signal whi;e it is being debugged. |

----------------------

The states of a process can be set via `set_task_state(task, state)`. And if the `task` is `current`, another function named `set_current_state(state)` is provided, which calls `set_task_state()` as well.    
`set_task_state()` also sets a mem ory barrier to force ordering on other processors.     

----------------------

The kernel can be in two contexts: `process context` and `interrupt context`. When in `process context`, the macro `current` becomes available.     

----------------------

All Linux (Unix) process are the descendants of the process `init`, whose PID is `1`.     
Processes that are all direct children of the same parent are called `siblings`.    

-------------------------

In `do_fork()`, after `copy_process()` returned, it runs the parent and child processes. Deliberately, it runs the child process first (Though it doesn't function correctly right now).      

-------------------------------

The `vfork()`has the same effect as `fork()`, except that the page table entries of the parent process are not copied.  Instead, the child executes as the sole thread in the parent's address space, and the parent is blocked until the child either calls `exec()` or exits.    

This function is merely useless in Linux, because Linux is impletemted with page-entries-copy-on-write and child-run-first semantics.      

------------------------------

To the Linux kernel, there is no concept of a thread. Linux implements all threads as standard processes. A thread is a process sharing specific resources (such as address space, filesystem resources, file descriptors and signal handlers) with other processes.         

-------------------------------------------

To create a "thread", one may call:    
``` c
clone(CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND, 0);
```

In contrast, `fork()` can be impletemted as:    
``` c
clone(SIGCHLD, 0);
```

And `vfork()` can be implemented as:    
``` c
clone(CLONE_VFORK | CLONE_VM | SIGCHLD, 0);
```

------------------------

`fork()`, `vfork()` and `clone()` are all the APIs provided by glibc.  Each of them calls the system calls `sys_fork()`, `sys_vfork()` or `sys_clone()`. In the kernel, these system calls will call `do_fork()`.         

Here are several clone flags can be passed to `clone()`:          

| Flag | Meaning |
|:------|------------:|
| `CLONE_FILES` | Parent and child share open files. |
| `CLONE_FS` | Parent and child share filesystem information. |
| `CLONE_IDLETASK` | Set PID to zero (used only by the idle tasks). |
| `CLONE_NEWNS` | Create a new namespace for the child. |
| `CLONE_PARENT` | Child is to have same parent as its parent. |
| `CLONE_PTRACE` | Continue tracing child. |
| `CLONE_SETTID` | Write the TID back to user-space. |
| `CLONE_SETTLS` | Create a new TLS for the child. |
| `CLONE_SIGHAND` | Parent and child share signal handlers and blocked signals. |
| `CLONE_SYSVSEM` | Parent and child share System V SEM_UNDO semantics. |
| `CLONE_THREAD` | Parent and child are in the same thread group. |
| `CLONE_VFORK` | vfork() was used and the parent will sleep until the child wakes it. |
| `CLONE_UNTRACED` | Do not let the tracing process force CLONE_PTRACE on the child. |
| `CLONE_STOP` | Start process in the TASK_STOPPED state. |
| `CLONE_SETTLS` | Create a new TLS (thread-local storage) for the child. |
| `CLONE_CHILD_CLEARTID` | Clear the TID in the child. |
| `CLONE_CHILD_SETTID` | Set the TID in the child. |
| `CLONE_PARENT_SETTID` | Set the TID in the parent. |
| `CLONE_VM` | Parent and child share address space. |

---------------------------------

Kernel threads are standard processes that exist solely in kernel-space. They are often used to perform some operations in the background in the kernel.     

The significant difference between kernel threads and normal processes is that kernel threads do not have an address space. (Their `mm` pointer, which points at their address space, is `NULL`.) They operate only in kernel-space and do not context switch into user-space. Kernel threads, however are schedulable and preemptable, the same as normal processes.       

The linux kernel itself delegates some tasks,  such as the *flush* task and the *ksoftirqd* task, to kernel threads as well.       

-------------------------------------

All new kernel threads are forked off of the *kthreadd* process.    

The interface for spawning a new kernel thread from an existing one is:     
``` c
struct task_struct *kthread_create(int (*threadfn) (void *data),
                                            void *data,
                                            const char namefmt[],
                                            ...)
```

It creates and returns a kernel thread which will execute the `threadfn`. This thread is created in an unrunnable state; it will not start running until explicitly woken up via `wake_up_process()`. A macro named `kthread_run()` combines the creation and waking up.     

``` c
#define kthread_run(threadfn, data, namefmt, ...)			   \
({									   \
	struct task_struct *__k						   \
		= kthread_create(threadfn, data, namefmt, ## __VA_ARGS__); \
	if (!IS_ERR(__k))						   \
		wake_up_process(__k);					   \
	__k;								   \
})
```

-------------------

A process may terminates voluntarily or involuntarily.     

+ Voluntarily: It occurs when the process calls the `exit()` system call, either explicitly or implicitly (That is, the C compiler places a call to exit() after main() returns).     
+ Involuntarily: This occurs when the process receives a signal or exception it cannot handle or ignore.     

--------------------

Regardless of how a process terminates, the bulk of the work is handled by `do_exit()`, defined in `kernel/exit.c`, which completes a number of chores:    
1. It sets the `PF_EXITING` flag in the flags member of the `task_struct`.
2. It calls `del_timer_sync()` to remove any kernel timers. Upon return, it is guaranteed that no timer is queued and that no timer handler is running.
3. If BSD process accounting is enabled, `do_exit()` calls `acct_update_integrals()` to write out accounting information.
4. It calls `exit_mm()` to release the `mm_struct` held by this process. If no other process is using this address space — that it, if the address space is not shared — the kernel then destroys it.
5. It calls `exit_sem()`. If the process is queued waiting for an IPC semaphore, it is dequeued here.
6. It then calls `exit_files()` and `exit_fs()` to decrement the usage count of objects related to file descriptors and filesystem data, respectively. If either usage counts reach zero, the object is no longer in use by any process, and it is destroyed.
7. It sets the task’s exit code, stored in the `exit_code` member of the `task_struct`, to the code provided by `exit()` or whatever kernel mechanism forced the termination. The exit code is stored here for optional retrieval by the parent.
8. It calls `exit_notify()` to send signals to the task’s parent, reparents any of the task’s children to another thread in their thread group or the init process, and sets the task’s exit state, stored in `exit_state` in the `task_struct` structure, to `EXIT_ZOMBIE`.
9. `do_exit()` calls `schedule()` to switch to a new process (see Chapter 4). Because the process is now not schedulable, this is the last code the task will ever execute. `do_exit()` never returns.

At this point, the task is not runnable, the only memory it occupies is its kernel stack, the `thread_info` structure, and the `task_struct` structure. The task exists solely to provide information to its parent. After the parent retrieves the information, or notifies the kernel that it is uninterested, the remaining memory held by the process is freed and returned to the system for use.

---------------------

After do_exit() completes, the process descriptor for the terminated process still exists, which enables the system to obtain information about a child process after it has terminated. Consequently, the acts of cleaning up after a process and removing its process descriptor are separate.     

After the parent has obtained information on its terminated child, or signified to the kernel that it does not care, the child’s task_struct is deallocated.    

When it is time to finally deallocate the process descriptor, `release_task()` is invoked. It does the following:     

1. It calls `__exit_signal()`, which calls `__unhash_process()`, which in turns calls `detach_pid()` to remove the process from the pidhash and remove the process from the task list.     
2. `__exit_signal()` releases any remaining resources used by the now dead process and finalizes statistics and bookkeeping.      
3. If the task was the last member of a thread group, and the leader is a zombie, then `release_task()` notifies the zombie leader’s parent.
4. `release_task()` calls `put_task_struct()` to free the pages containing the process’s kernel stack and `thread_info` structure and deallocate the slab cache containing the `task_struct`.     

At this point, the process descriptor and all resources belonging solely to the process have been freed.    

-----------------------

If a parent exits before its children, the kernel needs to reparent them. It tries to reparent them to one living thread of the same thread group. If that fails, they are reparented to `init`.     

`init` routinely calls `wait()` on its children, cleaning up any zombies assigned to it.     

---------------------

For the `ptraced` children, when their parent exits, the kernel does the same thing.     
