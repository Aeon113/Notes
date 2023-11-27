# 协程 004



(以下翻译自 cppreference):

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

3. 调用 `promise.get_return_object()` 并将结果保留在局部变量中, 该调用的结果将在协程首次挂起时返回给调用者。在此步骤及之前抛出的任何异常都会向调用者传播，而不是放在 promise 中。(`promise.get_return_object()` 的返回值通常是/包含 一个 `std::coroutine_handle<Promise>`, 指向当前coroutine, 这样其他逻辑可以在条件合适时使用`coroutine_handle<Promise>::resume()`来恢复此协程)。

4. 调用 `promise.initial_suspend()` 并 `co_await` 其结果。标准库提供了2个预先定义的 Awaiter `std::suspend_always`和`std::suspend_never`。典型的 Promise 类型要么返回 `std::suspend_always`，用于延迟启动的协程，要么返回 `std::suspend_never`，用于立即启动的协程。

5. 当 `co_await promise.initial_suspend()` 恢复时，开始执行协程的主体。


当协程达到一个挂起点时，先前获取的返回对象(此前上方的`promise.get_return_object()`返回值) 在必要时隐式转换为协程的返回类型后，将返回给调用者/恢复者。

当协程达到 `co_return` 语句时，它会执行以下操作：

+ 调"用return"函数:
  + 对于 `co_return` 或 `co_return expr`（其中 `expr` 的类型为 `void`），调用 `promise.return_void()`
  + 对于 `co_return expr`（其中 `expr` 的类型为非 `void`），调用 `promise.return_value(expr)`
+ 按照创建它们的反序销毁所有具有自动存储期的变量。
+ 调用 `promise.final_suspend()` 并 `co_await` 其结果。

协程的结束等同于 `co_return`;，但是如果 `Promise` 类型没有 `Promise::return_void()` 成员函数，那么行为是未定义的。

如果函数体中没有协程相关的关键字出现(`co_await`, `co_yield` 或 `co_return`)，那么无论其返回类型如何，都不是协程，并且如果返回类型不是 `cv void` (我也不知道什么是`cv void`, 我理解和"函数定义中有返回值但函数体中没有return任何值"是一回事)，那么结束时的结果将是未定义的。

``` c++
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

## Dynamic allocation

协程状态通过non-array的 `operator new` 动态分配。

如果 `Promise` 类型定义了类级替换，它将被使用，否则将使用全局的 `operator new`。

如果 `Promise` 类型定义了一个接受额外参数的 `operator new` 的放置形式，并且它们匹配一个参数列表，其中第一个参数是请求的大小（类型为 `std::size_t`），其余的是协程函数参数，那么这些参数将被传递给 `operator new` (这使得可以使用领先的分配器约定用于协程)。

如果满足以下条件，可以优化掉对 `operator new` 的调用 (即使使用了自定义分配器):

协程状态的生命周期严格嵌套在调用者的生命周期内，以及在调用点知道协程帧的大小。 在这种情况下，协程状态被嵌入到调用者的堆栈帧中 (如果调用者是一个普通函数) 或协程状态中 (如果调用者是一个协程)。
如果分配失败，协程会抛出 `std::bad_alloc`，除非 `Promise` 类型定义了成员函数 `Promise::get_return_object_on_allocation_failure()`。如果定义了该成员函数，分配使用 `operator new` 的 `nothrow` 形式，在分配失败时，协程立即将从 `Promise::get_return_object_on_allocation_failure()` 获取的对象返回给调用者，例如:

``` c++
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

| If the coroutine is defined as ... | then its Promise type is ... |
|------------------------------------|------------------------------|
| `task<void> foo(int x);` | `std::coroutine_traits<task<void>, int>::promise_type` |
| `task<void> Bar::foo(int x) const;` | `std::coroutine_traits<task<void>, const Bar&, int>::promise_type` |
| `task<void> Bar::foo(int x) &&;` | `std::coroutine_traits<task<void>, Bar&&, int>::promise_type` |

## co_await

一元运算符 `co_await` 挂起协程并将控制权返回给调用者。它的操作数是一个表达式，该表达式要么 (1) 是定义了成员运算符 `co_await` 的类类型，或者可以传递给非成员运算符 `co_await`，要么 (2) 可以通过当前协程的 `Promise::await_transform()` 转换为这样的类类型。

`co_await <expr>`

`co_await` 表达式只能出现在常规函数体内的可被评估表达式 (Potentially-evaluated expressions)中，不能出现在:

+ exception handler 中，
+ 声明语句中，除非它出现在该声明语句的initializer中 (也就是在声明变量时可以出现在等号右边, 作为右值使用)，
+ init-statement 的simple declaration中（参见 if，switch，for 和 range-for），除非它出现在该 init-statement 的初始化器中，
+ 函数的默认参数中，或者
+ 具有静态或线程级 storage duration 的 block-scoped 变量的initializer中。












-------------------

Some examples of a parameter becoming dangling:

``` cpp
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

