# Chapter 5: TCP Client/Server Example   

Function `connect` returns when the second segment of the three-way handshake received by the client; `accept` returns when the thrid segment received by the server.   

We name a system call like `accept` the `slow system call`, because this kind of system calls may never return.   

When a slow system call was interrupted (by a signal or somthing alike), it may return `EINTR` on some systems. And some systems may restart the interrupted slow system calls automatically. To avoid the dependency, we shall handle the interrupts manually when using slow system calls.     

Usually we can re-call a slow system call after it is interrupted, but it is not okay for all system calls. For example, if we re-call `connect` after it's interrupted, it will return an error immediately. For a interrupted `connect`, we must use `select` to wait for the connection to complete.     

``` c
// From sys/wait.h

/* Blocks until the first child process terminates, returns its pid and passes back its termination status. 
 *
 * Parameter statloc indicates the position to store the termination status of the child process.
 * Here are some important macros in this value for Linux:
 * WIFEXITED(*statloc) returns a non zero value if this process terminates normally.
 * WEXITSTATUS(*statloc) returns the exiting code if the child process returns with function exit. This is applicable only if WIFEXITED(*statloc) is not false (0).
 * WIFSIGNALED(*statloc) returns a non-zero value if it is killed for not capturing a signal.
 * WTERMSIG(*statloc) returns the signal that killed this process. Only applicable if WIFSIGNALED(*statloc) is not false (0).
 * WIFSTOPPED(*statloc) returns a non-zero value if this process is stopped.
 * WSTOPSIG(*statloc) returns the signal that stops this process if WIFSTOPPED(*statloc) is not false (0).
 *
 * Returns -1 or 0 on error.
 */
pid_t wait(int *statloc); 

/* Blocks until the specified child proess terminates. 
 * 
 * If the argument pid is set to -1, it waits until the first child process terminates;
 * 0 on Linux means any process in the same process group of this process;
 * any positive value means the specific child process's pid;
 * any value less than -1 on Linux means its opposite value.
 *
 * Parameter statloc is of the same usage as the one in wait. 
 * 
 * Parameter options can is usually set to 0. Here are two important alternatives for this argument:
 * WNOHANG: Immediately returns 0 if the specified process(es) are not terminating or terminated.
 * WUNTRACED: Immediately returns if the child process(es) are stopped.
 */

pid_t waitpid(pid_t pid, int *statloc, int options);
```

--------------
Sometimes the connection may be reseted by the client, before the `accept` off the server returns. In this case, some implementations, like the Berkeley-derived ones, will automatically discard it. Some systems, like SVR4, returns an errno of `EPROTO` (protocal error). POSIX requires the errno shall be `ECONNABORTED`. So for most Unix server programs, it may just ignore the `ECONNABORTED` error caused by `accept`.   

------------

Though it is okay to write to a socket that received a `FIN`, it is not allowed to write to a socket received a `RST`. If a `RST`ed socket is written, a `SIGPIPE` is sent to the process. Unless the process handles it or set itself to ignore it, the proess will be killed. If it is handled or ignored, the write operation returns `EPIPE`.    

---------------

When server host is down, whatever the client sends will not bring back any response. If client writes to a server is down, it will block in the write operation, TCP will keep retransmitting the message. After enough tries, the write operation responds an error `ETIMEDOUT`. But if the failure is detected by the network, and an ICMP "destination unreachable" message is sent back, the error will be `EHOSTUNREACH` or `ENETUNREACH`.   

----------------

