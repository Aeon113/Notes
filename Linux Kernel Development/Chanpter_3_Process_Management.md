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

