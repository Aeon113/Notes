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

One can check the kernel threads on the system by `ps -ef`.     

-------------------------------------

All new kernel threads are forked off of the *kthreadd* process.    

The interface for spawning a new kernel thread from an existing one is:     
``` c
struct task_struct *kthread_create(int (*threadfn) (void *data),
                                            void *data,
                                            const char namefmt[],
                                            ...)
```

