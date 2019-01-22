# Chapter 6: I/O Multiplexing: The `select` abd `poll` Functions
Five I/O Models:   
+   Blocking I/O Model
+   Nonblocking I/O Model
+   I/O Multiplexing Model
+   Signal-Driven I/O Model
+   Asynchronous I/O Model

--------------------
### `select` 函数:    
当某个或某些descriptors满足某些条件, 或者时限达到后返回.   
``` c
// From sys/select.h
int select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset,struct timeval *timeout); /* positive return value indicates the count of ready descriptors, 0 on timeout, -1 on error. */
```

``` c
struct timeval {
    long tv_sec;    /* seconds */
    long tv_usec;   /* microseconds */
};
```   

关于 `timeout`:   
1. 当 `timeout == NULL` 时, 永久等待直到descriiptor满足条件.   
2. 当 `timeout->tv_set == NULL && tv_usec == NULL` 时, 函数将立即返回.   
3. 其他条件下, 函数最多等待 `*timeout` 所指定的时长.     

前两种情况下, 函数可能被interrupt, 某些实现不会自动重启 `select`, 并会产生错误.   
The actual timeout may not be the same as we specified. The system may manage it in a more coarse form. For example, many Unix kernels round the timeout value up to a multiple of 10 ms.   

Some implementations, including Linux, may modify `*timeout`.    

The three middle arguments, `readset`, `writeset` and `exceptsert`, specify the descriptors that we want the kernel to test for reading, writing, and exception condiotions.   
We use the following macros (presented in the forms of functions) to set a fdset:
``` c
void FD_ZERO(fd_set *fdset);    /* clear all bits in fdset */
void FD_SET(int fd, fd_set *fdset); /* turn on the bit for fd in fdset */
void FD_CLR(int fd, fd_set *fdset); /* turn off the bit for fd in fdset */
void FD_ISSET(int fd, fd_set *fdset); /* is the bit for fd on in fdset? */
```

If we are not interested in any of the three set, we can set the pointer for them to NULL.   

The argument `maxfdp1` means "maximal fd to test plus 1". For example, if we need to test descriptors "1, 3, 5, 7", then we set `maxfdp1` to "8", since it's 1 larger than the maximal fd we are going to test, "7".      

The constant (maybe a macro) `FD_SETSIZE` in `<sys/select.h>` indicates the number of descriptors of a `fd_set`, usually 1024.   

`select` modifies `readset`, `writeset` and `exceptset`. Use `FD_ISSET` on them, if the corresponding bit is on, the descriptor is ready.   

A socket is ready to read from if any of the following conditions is true:       
1. Number of bytes of data in the socket receive buffer is greater than or equal to the current size of the low-water mark for the socket receive buffer. We can use option `SO_RCVLOWAT` with function `setsockopt` (from `<sys/socket.h>) to set this value, it defaults to 1 for TCP and UDP sockets.     
2. The read half of the connection is closed. (i.e., a TCP connection that has revceived a FIN). A read operation on this socket will not block and will return 0 (i.e., EOF).   
3. The socket is a listening socket and the number of completed connections is nonzero. An `accept` on the listening socket will normally not block, althoogh we can make `accept` blockable.   
4. A socket error is pending. A read operation on this socket will not block but returns an -1 and set `errno` to the error code. These `pending errors` can also be fetched and cleared by calling `getsockopt` and specifying the `SO_ERROR` socket option.    

A socket is ready to write from if any of the following conditions is true:    
1. If the socket is connected, or it doesn't require a connection (i.e., a UDP socket), and in the mean time the number of bytes of available space in the socket send buffer is greatere than or equal to the size of the low-water mark for the socket send buffer.   
2. The write half of the connection is closed. A write operation on thesocket will generate `SIGPIPE`.   
3. A socket using a non-blocking `connect` has completed the connection, or the `connect` has failed.   
4. A socket error is pending. A write operation on this socket will not block but returns an -1 and set `errno` to the error code. These `pending errors` can also be fetched and cleared by calling `getsockopt` and specifying the `SO_ERROR` socket option.     

If there is out-of-band data for one of the specified socket or one of them is still at the out-of-band mark, `select` sets the corresponding bit in `exceptset` on and returns.   

A socket is marked both readable and writable if an error occurs on it.   

A summary for these conditions:   
![Summary of conditions that cause a spclet to be ready for `select`](pics/6.7.png)   

Previously we use `close` to terminate a TCP connection. But `close` only terminates a connection when its reference count reaches 0, and after it, normally we cannot read from or write to the socket anymore. We can use `shutdown` function to bypass the reference count machanism and only shut one half of the read-write:   

``` c
// From sys/socket.h
int shutdown(int sockfd, int howto); // Returns 0 if OK, -1 on error.
```   

The behaviour of this function depends on the argument `howto`:   

| `howto`         |         description         |
|-----------------|-----------------------------|
|    `SHUT_RD`    |    The read half of the connection is closed, no more data can be read. Any data received after this calling will simply be discarded. |
|    `SHUT_WR`    |    It makes the TCP connection into the `half-closed` connection. The remaining data in the write buffer will be sent, and then an `FIN` will be sent. After this we cannot write more to this socket. |
|    `SHUT_RDWR`  |    After this, we can neither read from or write to the socket. This is equivilent to calling `shutdown` twice: first with `SHUT_RD` and then with `SHUT_WR`. |   

POSIX provides a function `pselect` to replace `select`.    

``` c
int pselect(int maxfdp1, fd_set *readset, df_set *writeset, fd_set *excceptset, const struct timespec *timeout, const sigset_t *sigmask); // Positive return value indicates the count of the ready descriptors; 0 on timeout; -1 on error.
```   

The structure of `timespec` is different from `timeval`:   

``` c
struct timespec {
    time_t tv_sec;  /* seconds */
    long tv_nsec;   /* nanoseconds */
};
```   

The `sigmask` may block the delivery of certain signals.   

The reason that pselect() is needed is that if one wants to wait for either a signal or for a file descriptor to become ready, then an atomic test is needed to prevent race conditions.  (Suppose the signal handler sets a global flag and returns.  Then a test of this global flag followed by a call of select() could hang indefinitely if the signal arrived just after the test but just before the call.  By contrast, pselect() allows one to first block signals, handle the signals that have come in, then call pselect() with the desired sigmask, avoiding the race.)     

A typical use of `pselect`:

``` c
sigset_t newmask, oldmask, zeromask;

sigemptyset(&zeromask);
sigemptyset(&newmask);
sigaddset(&newmask, SIGINT);

sigprocmask(SIG_BLOCK, &newmask, &oldmask); /* block SIGINT */
if(intr_flag)
    handle_intr(); /* handle the signal */
if((nready = pselect(.../ &zeromask)) < 0) {
    if(errno == EINTR) {
        if(intr_flag)
        handle_intr();
    }
    ...
}
```   

----------------
### `poll` 函数
``` c
// From poll.h
int poll(struct pollfd *fdarray, unsigned long nfds, int timeout); // returns a positive integer to indicate the count of the ready descriptors, 0 on timeout, -1 on error.
```   

The defination for struct `pollfd`:   

``` c
struct pollfd {
    int fd;             /* descriptor to check */
    short events;       /* Events on this descriptor we will wait for */
    short revents;      /* returned events. If any event specified in the previous member variable occcurrs, we set the corresponding bit on in this member variable. */
};
```

| Constants | Input to `events` | Result from `revents` | Description |
|:---------:|:-----------------:|:---------------------:|:-----------:|
| POLLIN | True | True | Normal or priority band data can be read |
| POLLRDNOM | True | True | Normal data can be read |
| POLLRDBAND | True | True | Priority band data can be read |
| POLLPRI | True | True | High-priority data can be read |
| POLLOUT | True | True | Normal data can be written |
| POLLWRNORM | True | True | Normal data can be written |
| POLLWRBAND | True | True | Priority band data can be written |
| POLLERR | False | True | Error has occurred |
| POLLHUP | False | True | Hangup has occurred |
| POLLNVAL | False | True | Descriptor is not an open file |   

For the normal data, priority band data and high-priority band data.   
+ All regular TCP data and all UDP data is considered normal.
+ TCP's out-of-band data is considered priority band.
+ When the read half of a TCP connection is closed, (e.g., a FIN is received) This is also considered normal data and a subsequent read operation will return 0.
+ The presence of an error for a TCP connection can be considered either normal data or an error (POLLERR). In either case, a subsequent read will return -1 with `errno` set to the approriate value. This handles conditions such as the receipt of an RST or a timeout.
+ The availability of a new connection on a listening socket can be considered either normal data or priority data. Most implementations consider this normal data.
+ The completion of a nonblocking `connect` is considered to make a socket writable.    

The `nfds` is the size of the descriptors array, and `timeout` is of milliseconds. If we want to make it returns immediately, we set it to 0; use INFTIM to wait forever. If the system does not support millisecond accuracy, this value will be rounded up to the nearest supported value.   

`select` is limited by `FD_SETSIZE`, so the largest descriptor it supports may not satisfy us. `poll` is not limited by it because it uses the array `fdarray`. The negative descriptors in this array is ignored. If we want to stop tracking one descriptor in this array, we can set the corresponding descriptor to a negative value.   

