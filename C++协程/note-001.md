# 协程01

## 1. 协程定义

包含有 `co_await`, `co_yield`和 `co_return`中任意一者的函数, 即为协程函数。

coroutine consists of

+ A promise object: User defined `promise_type` object, manipulated inside coroutine, return result via this object
+ Handle: Non owning handle which use to resume or destroy coroutine from outside
+ Coroutine state: Heap allocated, contains promise object, arguments to coroutine and local variables

```cpp
// 一个示例:
#include <coroutine>
#include <iostream>
#include <cassert>

class resumable {
public:
struct promise_type;
using coro_handle = std::coroutine_handle<promise_type>;
resumable(coro_handle handle) : handle_(handle) { assert(handle); }
resumable(resumable&) = delete;
resumable(resumable&&) = delete;
bool resume() {
    if (not handle_.done())
        handle_.resume();
    return handle_.done();
}
~resumable() { handle_.destroy(); }
private:
    coro_handle handle_;
};

struct resumable::promise_type {
    using coro_handle = std::coroutine_handle<promise_type>;
    auto get_return_object() {
        return coro_handle::from_promise(*this);
    }
    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {
        std::terminate();
    }
};

resumable foo() {
    std::cout << "a" << std::endl;
    co_await std::suspend_always();
    std::cout << "b" << std::endl;
    co_await std::suspend_always();
    std::cout << "c" << std::endl;
}

int main() {
    auto res = foo();
    res.resume();
    res.resume();
    res.resume();

    return 0;
} 
```

如果一个 `noexcept`函数抛出一个exception, 则程序会调用 `std::terminate()`。
