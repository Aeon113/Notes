# CppReference: Coroutine

翻译自: [cppreference coroutine](https://en.cppreference.com/w/cpp/language/coroutines)，可以被认为是 C++ 20 coroutine标准的简化版文档

## 限制

协程不能使用可变参数、普通的 `return` 语句或占位符返回类型（`auto` 或 `Concept`）。

`consteval` 函数、`constexpr` 函数、`构造函数`、`析构函数`和 `main 函数`不能是协程。

## 执行

每个协程都与以下内容相关联：

promise 对象，从协程内部操作。协程通过此对象提交其结果或异常。

coroutine handle (`std::coroutine_handle<XXX>` 和 `std::coroutine_handle<>`)，从协程外部操作。这是一个非拥有的句柄，用于恢复协程的执行或销毁协程帧。

协程状态 (coroutine state, 或协程帧 coroutine frame)，这是一个内部的 (和lambda一样, 具体类型信息不对编码人暴露)、
动态分配的存储对象（除非优化掉了分配, 优化的方法通常是将多个协程帧打包在一起一同分配, 这样多个协程帧只需要分配一次），包含以下内容：

+ promise 对象
+ 参数（全部通过值复制）
+ 当前挂起点的某种表示，以便恢复知道从哪里继续，销毁知道哪些局部变量在作用域内
+ 生命周期跨越当前挂起点的局部变量和临时变量。

当协程开始执行时，它会执行以下操作：

使用 `operator new` 分配协程状态对象。

1. 将所有函数参数复制到协程状态：按值参数被移动或复制，按引用参数保持引用(因此，如果在引用对象的生命周期结束后恢复协程，可能会变成dangling reference)。
2. 调用 promise 对象的构造函数。如果 promise 类型有一个接受所有协程参数的构造函数，那么将调用该构造函数，参数为复制后的协程参数。否则调用默认构造函数。
3. 调用 `promise.get_return_object()` 并将结果保留在局部变量中, 该调用的结果将在协程首次挂起时返回给调用者。在此步骤及之前抛出的任何异常都会向调用者传播，而不是放在 promise 中。(`promise.get_return_object()` 的返回值类型通常是/包含 一个 `std::coroutine_handle<Promise>`, 指向当前coroutine, 这样其他逻辑可以在条件合适时使用 `coroutine_handle<Promise>::resume()`来恢复此协程)。
4. 调用 `promise.initial_suspend()` 并 `co_await` 其结果。标准库提供了2个预先定义的 Awaiter `std::suspend_always`和 `std::suspend_never`。典型的 Promise 类型要么返回 `std::suspend_always`，用于延迟启动的协程，要么返回 `std::suspend_never`，用于立即启动的协程。
5. 当 `co_await promise.initial_suspend()` 恢复时，开始执行协程的主体。

当协程达到一个挂起点时，先前获取的返回对象(此前上方的 `promise.get_return_object()`返回值) 在必要时隐式转换为协程的返回类型后，将返回给调用者/恢复者。

当协程达到 `co_return` 语句时，它会执行以下操作：

+ 调"用return"函数:
  + 对于 `co_return` 或 `co_return expr`（其中 `expr` 的类型为 `void`），调用 `promise.return_void()`
  + 对于 `co_return expr`（其中 `expr` 的类型为非 `void`），调用 `promise.return_value(expr)`
+ 按照创建它们的反序销毁所有具有自动存储期的变量。
+ 调用 `promise.final_suspend()` 并 `co_await` 其结果。

协程的结束等同于 `co_return`;，但是如果 `Promise` 类型没有 `Promise::return_void()` 成员函数，那么行为是未定义的。

如果函数体中没有协程相关的关键字出现(`co_await`, `co_yield` 或 `co_return`)，那么无论其返回类型如何，都不是协程，并且如果返回类型不是 `cv void` (我也不知道什么是 `cv void`, 我理解和"函数定义中有返回值但函数体中没有return任何值"是一回事)，那么结束时的结果将是未定义的。

```c++
// assuming that task is some coroutine task type
task<void> f()
{
    // not a coroutine, undefined behavior
}
 
task<void> g()
{
    co_return;  // OK
}
 
task<void> h()
{
    co_await g();
    // OK, implicit co_return;
}
```

如果协程以未捕获的异常结束，它将执行以下操作：

+ 在 catch-block 中捕获异常并调用 `promise.unhandled_exception()`
+ 调用 `promise.final_suspend()` 并 `co_await` 其结果 (例如, 恢复一个延续或发布一个结果)。从这一点恢复协程是未定义的行为。

当协程状态因通过 `co_return` 或未捕获的异常终止，或通过其句柄被销毁时，它会执行以下操作：

+ 调用 `promise` 对象的析构函数。
+ 调用函数参数副本的析构函数。
+ 调用 `operator delete` 释放协程状态使用的内存。
+ 将执行权转回给调用者/恢复者。

Some examples of a parameter becoming dangling:

```cpp
#include <coroutine>
#include <iostream>
 
struct promise;
 
struct coroutine : std::coroutine_handle<promise>
{
    using promise_type = ::promise;
};
 
struct promise
{
    coroutine get_return_object() { return {coroutine::from_promise(*this)}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
};
 
struct S
{
    int i;
    coroutine f()
    {
        std::cout << i;
        co_return;
    }
};
 
void bad1()
{
    coroutine h = S{0}.f();
    // S{0} destroyed
    h.resume(); // resumed coroutine executes std::cout << i, uses S::i after free
    h.destroy();
}
 
coroutine bad2()
{
    S s{0};
    return s.f(); // returned coroutine can't be resumed without committing use after free
}
 
void bad3()
{
    coroutine h = [i = 0]() -> coroutine // a lambda that's also a coroutine
    {
        std::cout << i;
        co_return;
    }(); // immediately invoked
    // lambda destroyed
    h.resume(); // uses (anonymous lambda type)::i after free
    h.destroy();
}
 
void good()
{
    coroutine h = [](int i) -> coroutine // make i a coroutine parameter
    {
        std::cout << i;
        co_return;
    }(0);
    // lambda destroyed
    h.resume(); // no problem, i has been copied to the coroutine
                // frame as a by-value parameter
    h.destroy();
}
```

## Dynamic allocation

协程状态通过non-array的 `operator new` 动态分配。

如果 `Promise` 类型定义了类级替换，它将被使用，否则将使用全局的 `operator new`。

如果 `Promise` 类型定义了一个接受额外参数的 `operator new` 的放置形式，并且它们匹配一个参数列表，其中第一个参数是请求的大小（类型为 `std::size_t`），其余的是协程函数参数，那么这些参数将被传递给 `operator new` (这使得可以使用领先的分配器约定用于协程)。

如果满足以下条件，可以优化掉对 `operator new` 的调用 (即使使用了自定义分配器):

协程状态的生命周期严格嵌套在调用者的生命周期内，以及在调用点知道协程帧的大小。 在这种情况下，协程状态被嵌入到调用者的堆栈帧中 (如果调用者是一个普通函数) 或协程状态中 (如果调用者是一个协程)。
如果分配失败，协程会抛出 `std::bad_alloc`，除非 `Promise` 类型定义了成员函数 `Promise::get_return_object_on_allocation_failure()`。如果定义了该成员函数，分配使用 `operator new` 的 `nothrow` 形式，在分配失败时，协程立即将从 `Promise::get_return_object_on_allocation_failure()` 获取的对象返回给调用者，例如:

```c++
struct Coroutine::promise_type
{
  /* ... */

  // ensure the use of non-throwing operator-new
  static Coroutine get_return_object_on_allocation_failure()
  {
    std::cerr << __func__ << '\n';
    throw std::bad_alloc(); // or, return Coroutine(nullptr);
  }

  // custom non-throwing overload of new
  void* operator new(std::size_t n) noexcept
  {
    if (void* mem = std::malloc(n))
      return mem;
    return nullptr; // allocation failure
  }
};
```

## Promise

`Promise` 类型是由编译器通过使用 std::coroutine_traits 从协程的返回类型中确定的。

正式地说，让 `R` 和 `Args...` 分别表示协程的返回类型和参数类型列表，`ClassT` 和 `cv-qual`（如果有的话）分别表示协程所属的类类型及其 `cv-qualification`，如果它被定义为非静态成员函数，那么它的 `Promise` 类型由以下方式确定：

+ 如果协程不是定义为非静态成员函数，那么由 `std::coroutine_traits<R, Args...>::promise_type` 确定，
+ 如果协程被定义为非静态成员函数且没有 `rvalue-reference-qualified`，那么由 `std::coroutine_traits<R, ClassT /*cv-qual*/&, Args...>::promise_type` 确定，
+ 如果协程被定义为非静态成员函数且是 `rvalue-reference-qualified`，那么由 `std::coroutine_traits<R, ClassT /*cv-qual*/&&, Args...>::promise_type` 确定。

For example:

| If the coroutine is defined as ...    | then its Promise type is ...                                         |
| ------------------------------------- | -------------------------------------------------------------------- |
| `task<void> foo(int x);`            | `std::coroutine_traits<task<void>, int>::promise_type`             |
| `task<void> Bar::foo(int x) const;` | `std::coroutine_traits<task<void>, const Bar&, int>::promise_type` |
| `task<void> Bar::foo(int x) &&;`    | `std::coroutine_traits<task<void>, Bar&&, int>::promise_type`      |

## co_await

一元运算符 `co_await` 挂起协程并将控制权返回给调用者。它的操作数是一个表达式，该表达式要么 (1) 是定义了成员运算符 `co_await` 的类类型，或者可以传递给非成员运算符 `co_await`，要么 (2) 可以通过当前协程的 `Promise::await_transform()` 转换为这样的类类型。

用法:

`co_await <expr>`

`co_await` 表达式只能出现在常规函数体内的可被评估表达式 (Potentially-evaluated expressions)中，不能出现在:

+ exception handler 中，
+ 声明语句中，除非它出现在该声明语句的initializer中 (也就是在声明变量时可以出现在等号右边, 作为右值使用)，
+ init-statement 的simple declaration中（参见 if，switch，for 和 range-for），除非它出现在该 init-statement 的初始化器中，
+ 函数的默认参数中，或者
+ 具有静态或线程级 storage duration 的 block-scoped 变量的initializer中。

首先，将 `expr` 转换为可等待对象 (awaitable)，具体如下：

+ 如果 `expr` 是由初始挂起点 (initial suspend point)、最终挂起点(final suspend point)或 yield 表达式产生的，则awaitable为 `expr`，保持不变。
+ 否则，如果当前协程的 Promise 类型具有成员函数 `await_transform`，则awaitable为 `promise.await_transform(expr)`。
+ 否则，awaitable为 `expr`，保持不变。

然后，获取等待器对象(awaiter object)，具体如下：

+ 如果对 `operator co_await` 的重载解析得到了单个最佳重载，awaiter object为该调用的结果：
  + 对于成员重载，为 `awaitable.operator co_await()`，
  + 对于非成员重载，为 `operator co_await(static_cast<Awaitable&&>(awaitable))`。
+ 否则，如果重载解析未找到 `operator co_await`, awaiter object为awaitable，保持不变。
+ 否则，如果重载解析存在二义性，程序将是非法的。

如果上述表达式是 prvalue，则awaiter object是从该表达式临时生成的临时对象。否则，如果上述表达式是 glvalue，则awaiter object是它所引用的对象。

然后，调用 `awaiter.await_ready()`（这是一个快捷方式，如果异步调用的结果已准备好或者同步操作的成本更低，在这里可以避免不必要的挂起）。如果它的结果在上下文中转换为 `bool` 为 `true`，则跳过以下挂起过程，否则：

  协程被挂起（当前挂起点信息和局部变量等数据会被写入协程帧）。调用 `awaiter.await_suspend(handle)`，其中 handle 是表示当前协程的 `coroutine_handle`。在该函数内部，可以通过该handle访问被挂起的协程，该函数的责任是将其安排在某个执行器(executor)上恢复，或者将其销毁（返回 `false` 标识调度）。

- 如果 `awaiter.await_suspend(handle)` 返回 `void`，则立即将控制权返回给当前协程的caller/resumer（此协程保持挂起状态），否则
- 如果 `awaiter.await_suspend(handle)` 返回 `bool`，
  - 值为 `true` 将控制权返回给当前协程的caller/resumer
  - 值为 `false` 恢复当前协程。
- 如果 `awaiter.await_suspend(handle)` 返回其他协程的协程句柄，则通过调用 `handle.resume()` 来恢复该句柄（请注意，这可能会链式触发最终导致当前协程恢复）。
- 如果 `awaiter.await_suspend(handle)` 抛出异常，则捕获异常，恢复协程，并立即重新抛出异常。

最后，调用 `awaiter.await_resume()`（无论协程是否被挂起），其结果是整个 `co_await expr` 表达式的结果。

如果协程在 `co_await 表达式` 中被挂起，则未来恢复时，恢复点位于调用 `awaiter.await_resume()` 之前。

请注意，因为协程在进入 `awaiter.await_suspend()`之前完全挂起，所以该函数可以自由地在线程之间传递coroutine_handle，无需额外的同步。例如，它可以将coroutine handle放在回调函数中，在异步I/O操作完成时在线程池上调度运行。在这种情况下，由于当前协程可能已经被恢复并执行了awaiter对象的析构函数，因此在 `await_suspend()`在当前线程上继续执行时，它应该将 `*this`视为已销毁，并且在coroutine handle 传递给其他线程后不再访问它。

---

Sample:

```cpp
#include <coroutine>
#include <iostream>
#include <stdexcept>
#include <thread>
 
auto switch_to_new_thread(std::jthread& out) {
  struct awaitable {
    std::jthread* p_out;
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<> h) {
      std::jthread& out = *p_out;
      if (out.joinable())
          throw std::runtime_error("Output jthread parameter not empty");
      out = std::jthread([h] { h.resume(); });
      // Potential undefined behavior: accessing potentially destroyed *this
      // std::cout << "New thread ID: " << p_out->get_id() << '\n';
      std::cout << "New thread ID: " << out.get_id() << '\n'; // this is OK
    }
    void await_resume() {}
  };
  return awaitable{&out};
}

struct task {
  struct promise_type {
    task get_return_object() { return {}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
  };
};

task resuming_on_new_thread(std::jthread& out) {
  std::cout << "Coroutine started on thread: " << std::this_thread::get_id() << '\n';
  co_await switch_to_new_thread(out);
  // awaiter destroyed here
  std::cout << "Coroutine resumed on thread: " << std::this_thread::get_id() << '\n';
}

int main() {
  std::jthread out;
  resuming_on_new_thread(out);
}
```

Possible output:

```text
Coroutine started on thread: 139972277602112
New thread ID: 139972267284224
Coroutine resumed on thread: 139972267284224
```

注意：awaiter对象是协程状态的一部分（作为一个临时对象，其生命周期跨越了挂起点），并且在 `co_await`表达式完成之前被销毁。它可以用于维护一些异步I/O API所需的每个操作状态，而无需进行额外的动态分配。

标准库定义了两个简单的可等待对象：`std::suspend_always`和 `std::suspend_never`。

## co_yield

`co_yield` 表达式将一个值返回给调用者并挂起当前协程：它是可恢复生成器函数的常见构建块。

用法：

`co_yield expr`

`co_yield braced-init-list`

它等价于 `co_await promise.yield_value(expr)`。

典型的生成器的 yield_value 会将其参数存储（复制/移动或仅存储地址，因为参数的生命周期跨越了 `co_await` 的挂起点）到生成器对象中，并返回 `std::suspend_always`，将控制权转移到调用者/恢复者。

Sample:

```cpp
#include <coroutine>
#include <cstdint>
#include <exception>
#include <iostream>
 
template<typename T>
struct Generator
{
    // The class name 'Generator' is our choice and it is not required for coroutine
    // magic. Compiler recognizes coroutine by the presence of 'co_yield' keyword.
    // You can use name 'MyGenerator' (or any other name) instead as long as you include
    // nested struct promise_type with 'MyGenerator get_return_object()' method.
 
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
 
    struct promise_type // required
    {
        T value_;
        std::exception_ptr exception_;
 
        Generator get_return_object()
        {
            return Generator(handle_type::from_promise(*this));
        }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void unhandled_exception() { exception_ = std::current_exception(); } // saving
                                                                              // exception
 
        template<std::convertible_to<T> From> // C++20 concept
        std::suspend_always yield_value(From&& from)
        {
            value_ = std::forward<From>(from); // caching the result in promise
            return {};
        }
        void return_void() {}
    };
 
    handle_type h_;
 
    Generator(handle_type h) : h_(h) {}
    ~Generator() { h_.destroy(); }
    explicit operator bool()
    {
        fill(); // The only way to reliably find out whether or not we finished coroutine,
                // whether or not there is going to be a next value generated (co_yield)
                // in coroutine via C++ getter (operator () below) is to execute/resume
                // coroutine until the next co_yield point (or let it fall off end).
                // Then we store/cache result in promise to allow getter (operator() below
                // to grab it without executing coroutine).
        return !h_.done();
    }
    T operator()()
    {
        fill();
        full_ = false; // we are going to move out previously cached
                       // result to make promise empty again
        return std::move(h_.promise().value_);
    }
 
private:
    bool full_ = false;
 
    void fill()
    {
        if (!full_)
        {
            h_();
            if (h_.promise().exception_)
                std::rethrow_exception(h_.promise().exception_);
            // propagate coroutine exception in called context
 
            full_ = true;
        }
    }
};
 
Generator<std::uint64_t>
fibonacci_sequence(unsigned n)
{
    if (n == 0)
        co_return;
 
    if (n > 94)
        throw std::runtime_error("Too big Fibonacci sequence. Elements would overflow.");
 
    co_yield 0;
 
    if (n == 1)
        co_return;
 
    co_yield 1;
 
    if (n == 2)
        co_return;
 
    std::uint64_t a = 0;
    std::uint64_t b = 1;
 
    for (unsigned i = 2; i < n; ++i)
    {
        std::uint64_t s = a + b;
        co_yield s;
        a = b;
        b = s;
    }
}
 
int main()
{
    try
    {
        auto gen = fibonacci_sequence(10); // max 94 before uint64_t overflows
 
        for (int j = 0; gen; ++j)
            std::cout << "fib(" << j << ")=" << gen() << '\n';
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "Unknown exception.\n";
    }
}
```
