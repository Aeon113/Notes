# C++ Coroutines: Understanding the Compiler Transform

_翻译自: [C++ Coroutines: Understanding the Compiler Transform | Asymmetric Transfer](https://lewissbaker.github.io/2022/08/27/understanding-the-compiler-transform#setting-the-scene)_

_文中使用了一个词 'lower' 用来表示编译器将一个协程函数转换成一个非协程的普通函数的过程。对这个词目前没有找到合适的翻译，下文只能将其译作"降级"_

  在本文中，我将更深入地展示前几篇文章中的所有概念是如何结合在一起的。我将通过将协程降级为等效的非协程、命令式的C++代码来展示当协程达到挂起点时会发生什么。


# 介绍

在“理解C++协程”系列的先前博客中，我们讨论了编译器对协程及其`co_await`、`co_yield`和`co_return`表达式执行的不同转换类型。这些文章描述了编译器如何将每个表达式翻译为对用户定义类型上的各种自定义点/方法的调用。

1. [Coroutine Theory](https://lewissbaker.github.io/2017/09/25/coroutine-theory)
2. [C++ Coroutines: Understanding operator co_await](https://lewissbaker.github.io/2017/11/17/understanding-operator-co-await)
3. [C++ Coroutines: Understanding the promise type](https://lewissbaker.github.io/2018/09/05/understanding-the-promise-type)
4. [C++ Coroutines: Understanding Symmetric Transfer](https://lewissbaker.github.io/2020/05/11/understanding_symmetric_transfer)

然而，这些描述中有一个部分可能让您感到不满意。它们对“挂起点”这个概念只是粗略地带过，模糊地说“协程在这里挂起”和“协程在这里恢复”，但并没有详细说明这实际上意味着什么，以及编译器如何实现它。

在本文中，我将更深入地展示前几篇文章中的所有概念是如何结合在一起的。我将通过将协程降级为等效的非协程、命令式的C++代码来展示当协程达到挂起点时会发生什么。

请注意，我不会详细描述特定编译器如何将协程转换为机器代码（编译器在这方面有额外的技巧），而只是展示一种可能的将协程降级为可移植C++代码的方式。

警告：这将是一个相当深入的探讨！

# 设置场景

首先，让我们假设我们有一个基本的`task`类型，它既可以作为可等待对象，也可以作为协程的返回类型。为了简单起见，让我们假设这个协程类型可以异步地产生一个`int`类型的结果。

在本文中，我们将逐步介绍如何将以下协程函数转换为不包含任何协程关键字`co_await`、`co_return`的C++代码，以便更好地理解这意味着什么。

```c++
// Forward declaration of some other function. Its implementation is not relevant.
task f(int x);

// A simple coroutine that we are going to translate to non-C++ code
task g(int x) {
    int fx = co_await f(x);
    co_return fx * fx;
}
```

# 定义 `task` 类型

首先，让我们声明我们将要使用的 `task` 类。

为了理解协程是如何被降级的，我们不需要知道这个类型的方法的定义。降级只是插入对它们的调用。

这些方法的定义并不复杂，我将把它们留给读者作为练习，以便更好地理解之前的文章。

```c++
class task {
public:
    struct awaiter;

    class promise_type {
    public:
        promise_type() noexcept;
        ~promise_type();

        struct final_awaiter {
            bool await_ready() noexcept;
            std::coroutine_handle<> await_suspend(
                std::coroutine_handle<promise_type> h) noexcept;
            void await_resume() noexcept;
        };

        task get_return_object() noexcept;
        std::suspend_always initial_suspend() noexcept;
        final_awaiter final_suspend() noexcept;
        void unhandled_exception() noexcept;
        void return_value(int result) noexcept;

    private:
        friend task::awaiter;
        std::coroutine_handle<> continuation_;
        std::variant<std::monostate, int, std::exception_ptr> result_;
    };

    task(task&& t) noexcept;
    ~task();
    task& operator=(task&& t) noexcept;

    struct awaiter {
        explicit awaiter(std::coroutine_handle<promise_type> h) noexcept;
        bool await_ready() noexcept;
        std::coroutine_handle<promise_type> await_suspend(
            std::coroutine_handle<> h) noexcept;
        int await_resume();
    private:
        std::coroutine_handle<promise_type> coro_;
    };

    awaiter operator co_await() && noexcept;

private:
    explicit task(std::coroutine_handle<promise_type> h) noexcept;

    std::coroutine_handle<promise_type> coro_;
};
```

这个任务类型的结构对于那些阅读过[C++ Coroutines: Understanding Symmetric Transfer](https://lewissbaker.github.io/2020/05/11/understanding_symmetric_transfer)文章的人来说应该是熟悉的。

## 第一步：确定 promise 类型

```c++
task g(int x) {
    int fx = co_await f(x);
    co_return fx * fx;
}
```

当编译器发现这个函数包含三个协程关键字之一（`co_await`、`co_yield`或`co_return`）时，它会开始进行协程转换过程。

第一步是确定该协程的`promise_type`。

这是通过将函数的返回类型和参数类型作为模板参数替换到`std::coroutine_traits`类型中来确定的。

例如，对于我们的函数`g`，它的返回类型是`task`，一个参数类型是`int`，编译器将使用`std::coroutine_traits<task, int>::promise_type`来查找。

让我们定义一个别名，以便稍后可以引用这个类型：
```c++
using __g_promise_t = std::coroutine_traits<task, int>::promise_type;
```

**注意：我在这里使用前导双下划线来表示编译器内部生成的符号。这些符号由实现保留，不应在您自己的代码中使用。**

现在，由于我们没有为`std::coroutine_traits`进行特化，这将实例化主模板，该模板将`promise_type`作为嵌套的`promise_type`名称的别名定义。在我们的示例中，这应该解析为类型`task::promise_type`。

## 第二步：创建协程状态 _(协程状态 coroutine state就是前文所述的协程帧 coroutine frame, 在堆上分配)_

协程函数在暂停时需要保留协程的状态、参数和局部变量，以便在协程稍后恢复时仍然可用。

在C++标准中，这个状态被称为“协程状态”，通常在堆上分配。

让我们先定义一个用于协程`g`的协程状态的结构体。

我们还不知道这个类型的内容将是什么样的，所以暂时将其保持为空。

```c++
struct __g_state {
  // to be filled out
};
```

协程状态包含多个不同的内容：

* promise 对象
* 函数参数的副本
* 关于协程当前挂起点的信息以及如何恢复/销毁它
* 用于存储在挂起点跨越的任何局部变量/临时变量的空间

让我们首先为 promise 对象和参数副本添加存储空间。

```c++
struct __g_state {
    int x;
    __g_promise_t __promise;

    // to be filled out
};
```

接下来，我们应该添加一个构造函数来初始化这些数据成员。

请记住，编译器首先尝试使用参数副本的左值引用调用 promise 构造函数，
如果该调用有效，则调用 promise 类型的默认构造函数。

让我们创建一个简单的辅助函数来帮助处理这个问题：
```c++
template<typename Promise, typename... Params>
Promise construct_promise([[maybe_unused]] Params&... params) {
    if constexpr (std::constructible_from<Promise, Params&...>) {
        return Promise(params...);
    } else {
        return Promise();
    }
}
```

因此，协程状态的构造函数可能如下所示：

```c++
struct __g_state {
    __g_state(int&& x)
    : x(static_cast<int&&>(x))
    , __promise(construct_promise<__g_promise_t>(x))
    {}

    int x;
    __g_promise_t __promise;
    // to be filled out
};
```

我们已经开始构建表示协程状态的类型，现在我们开始构建`g()`的降级实现的框架。它将通过堆分配一个`__g_state`类型的实例，并将函数参数传递给它，以便将它们复制/移动到协程状态中。

一些术语 - 我使用术语“ramp函数”来指代协程实现的一部分，其中包含初始化协程状态并使其准备好开始执行协程的逻辑。
也就是说，它类似于进入协程体执行的入口。

```c++
task g(int x) {
    auto* state = new __g_state(static_cast<int&&>(x));
    // ... implement rest of the ramp function
}
```

请注意，我们的 promise 类型没有定义自己的自定义 `operator new` 重载，
因此我们在这里只是调用全局的 `::operator new`。


如果 promise 类型定义了自定义的 `operator new`，那么我们将调用它而不是全局的 `::operator new`。我们首先检查是否存在可以接受参数 `(size, paramLvalues...)`的自定义 `operator new` ，如果存在，则使用该参数列表调用它 _(注意 `paramLvalues...`是可变参数模板，表示一个或多个参数)_。否则，我们将使用 `(size)` 参数列表。_(因为 `operator new` 至少要接受一个 `size` 参数)_ `operator new` 能够访问协程函数的参数列表的能力有时被称为 "参数预览"，在需要用自定义分配器为部分特殊协程的协程状态分配空间的情况下非常有用。

如果编译器找到了 `__g_promise_t::operator new` 的任何定义，那么我们将降级到以下逻辑：
```c++
template<typename Promise, typename... Args>
void* __promise_allocate(std::size_t size, [[maybe_unused]] Args&... args) {
  if constexpr (requires { Promise::operator new(size, args...); }) {
    return Promise::operator new(size, args...);
  } else {
    return Promise::operator new(size);
  }
}

task g(int x) {
    void* state_mem = __promise_allocate<__g_promise_t>(sizeof(__g_state), x);
    __g_state* state;
    try {
        state = ::new (state_mem) __g_state(static_cast<int&&>(x));
    } catch (...) {
        __g_promise_t::operator delete(state_mem);
        throw;
    }
    // ... implement rest of the ramp function
}
```

此外，这个 promise 类型没有定义 `get_return_object_on_allocation_failure()` 静态成员函数。
如果在 promise 类型上定义了这个函数，那么在这里分配内存时将使用 `std::nothrow_t` 形式的 `operator new`，
并且在返回 `nullptr` 后将会 `return __g_promise_t::get_return_object_on_allocation_failure();`。

也就是说，它看起来会像这样：
```c++
task g(int x) {
    auto* state = ::new (std::nothrow) __g_state(static_cast<int&&>(x));
    if (state == nullptr) {
        return __g_promise_t::get_return_object_on_allocation_failure();
    }
    // ... implement rest of the ramp function
}
```

为了简化示例，我们将只使用最简形式 --- 调用全局的 `::operator new` 内存分配函数。

## 第三步：调用 `get_return_object()`

接下来，ramp函数调用promise对象的`get_return_object()`方法来获取ramp函数的返回值。

返回值被存储为一个局部变量，并在其他步骤完成后，于ramp函数的末尾返回。

```c++
task g(int x) {
    auto* state = new __g_state(static_cast<int&&>(x));
    decltype(auto) return_value = state->__promise.get_return_object();
    // ... implement rest of ramp function
    return return_value;
}
```

然而，现在调用 `get_return_object()` 可能会抛出异常，如果是这样的话，我们希望释放已分配的协程状态。为了保险起见，让我们将状态的所有权交给一个 `std::unique_ptr`，这样在后续的操作中如果抛出异常，它会被释放：

```c++
task g(int x) {
    std::unique_ptr<__g_state> state(new __g_state(static_cast<int&&>(x)));
    decltype(auto) return_value = state->__promise.get_return_object();
    // ... implement rest of ramp function
    return return_value;
}
```

_注意实际生成的代码中没有`std::unique_ptr`，这里只是为了简化实现_

## 第四步：初始挂起点 (initial-suspend point)

在调用 `get_return_object()` 后，ramp函数的下一步是开始执行协程体，而协程体中的第一件事就是判定初始挂起点。也就是我们要处理 `co_await promise.initial_suspend()`。

理想情况下，我们将把协程视为初始挂起状态，并将协程的启动实现为对这个初始挂起的协程的恢复。然而，初始挂起点的规范在处理异常和协程状态的生命周期方面有一些特殊之处。这是在C++20发布之前对初始挂起点语义进行的一次微调，以修复一些问题。

在初始挂起点的评估过程中，如果在以下位置抛出了异常：
*  `initial_suspend()` 中，
* 在对 `operator co_await()` 的调用中（如果定义了该函数），
* 在对awaiter的 `await_ready()` 的调用中，
* 在对awaiter的 `await_suspend()` 的调用中，

那么异常将传播回到 ramp 函数的调用者，并且协程状态将自动销毁。

如果在以下位置抛出了异常：
*  `await_resume()` 中，
*  `operator co_await()` 返回的对象的析构函数中（如果适用）_(Effective C++ 建议不要在析构函数里抛异常)_，
*  `initial_suspend()` 返回的对象的析构函数中

那么这个异常会被协程体捕获，并调用 `promise.unhandled_exception()`。

这意味着我们在处理这部分时需要小心，因为一些部分需要放在ramp函数中，而其他部分需要放在协程体中。

另外，由于从`initial_suspend()`和（可选的）`operator co_await()`返回的对象的生命周期跨越了挂起点（它们在协程挂起之前创建，在恢复之后销毁），因此这些对象的存储需要放在协程状态中。

在我们的示例中，从 `initial_suspend()` 返回的类型是 `std::suspend_always`，好是一个空的、可平凡构造的 (trivial-constructable) 类型。然而，从逻辑上讲，我们仍然需要在协程状态中存储一个该类型的实例，所以了展示协程的工作原理，我们将在协程状态中添加对这个`std::suspend_always`的存储。

这个对象只会在调用 `initial_suspend()` 时被构造，因此我们需要添加一个特定类型的数据成员，以便能够显式控制它的生命周期。

为了支持这一点，让我们首先定义一个辅助类 `manual_lifetime`，它是可平凡构造和可平凡析构的，但允许我们在需要时显式地构造/析构存储在其中的值。

```c++
template<typename T>
struct manual_lifetime {
    manual_lifetime() noexcept = default;
    ~manual_lifetime() = default;

    // Not copyable/movable
    manual_lifetime(const manual_lifetime&) = delete;
    manual_lifetime(manual_lifetime&&) = delete;
    manual_lifetime& operator=(const manual_lifetime&) = delete;
    manual_lifetime& operator=(manual_lifetime&&) = delete;

    template<typename Factory>
        requires
            std::invocable<Factory&> &&
            std::same_as<std::invoke_result_t<Factory&>, T>
    T& construct_from(Factory factory) noexcept(std::is_nothrow_invocable_v<Factory&>) {
        return *::new (static_cast<void*>(&storage)) T(factory());
    }

    void destroy() noexcept(std::is_nothrow_destructible_v<T>) {
        std::destroy_at(std::launder(reinterpret_cast<T*>(&storage)));
    }

    T& get() & noexcept {
        return *std::launder(reinterpret_cast<T*>(&storage));
    }

private:
    alignas(T) std::byte storage[sizeof(T)];
};
```

注意，这里的 `construct_from()` 方法设计为接受一个 lambda 表达式，而不是构造函数参数。这样可以利用函数调用的结果来直接在变量初始化时进行对象的原地构造，从而保证了拷贝省略 (copy-elision)。如果它接受构造函数参数，那么我们将不必要地调用额外的移动构造函数。

现在，我们可以使用这个 `manual_lifetime` 结构为 `promise.initial_suspend()` 返回的临时对象声明一个数据成员。

```c++
struct __g_state {
    __g_state(int&& x);

    int x;
    __g_promise_t __promise;
    manual_lifetime<std::suspend_always> __tmp1;
    // to be filled out
};
```

`std::suspend_always` 类型没有 `operator co_await()`，因此我们不需要为该调用结果预留额外的临时存储空间。

在通过调用 `intial_suspend()` 构造了这个对象之后，我们需要调用一系列方法来实现 `co_await` 表达式：`await_ready()`、`await_suspend()` 和 `await_resume()`。

在调用 `await_suspend()` 时，我们需要传递当前协程的句柄。
目前，我们可以简单地调用 `std::coroutine_handle<__g_promise_t>::from_promise()` 并传递对该 promise 的引用。稍后我们将更详细地了解它的内部工作原理。

此外，调用 `.await_suspend(handle)` 的结果类型为 `void` _(因为 `std::always_suspend`的`await_suspend()`的返回值类型就是`void`)_，因此我们不需要像我们对返回 `bool` 和 `coroutine_handle` 的情况所做的那样考虑在调用 `await_suspend()` 后是恢复当前协程还是其他协程。

最后，由于 `std::suspend_always` awaiter 上的所有方法调用都声明为 `noexcept`，我们不需要担心异常。如果它们可能抛出异常，我们需要添加额外的代码，以确保在异常传播到 ramp 函数之前销毁临时的 `std::suspend_always` 对象。

一旦 `await_suspend()` 成功返回，或者我们即将开始执行协程体，我们就进入了一个阶段，在这个阶段我们不再需要在抛出异常时自动销毁协程状态。因此，我们可以调用拥有协程状态的 `std::unique_ptr` 上的 `release()` 方法，以防止在函数返回时销毁它。

那么现在我们可以按照以下方式实现initial-suspend表达式的第一部分：

```c++
task g(int x) {
    std::unique_ptr<__g_state> state(new __g_state(static_cast<int&&>(x)));
    decltype(auto) return_value = state->__promise.get_return_object();

    state->__tmp1.construct_from([&]() -> decltype(auto) {
        return state->__promise.initial_suspend();
    });
    if (!state->__tmp1.get().await_ready()) {
        //
        // ... suspend-coroutine here
        //
        state->__tmp1.get().await_suspend(
            std::coroutine_handle<__g_promise_t>::from_promise(state->__promise));

        state.release();

        // fall through to return statement below.
    } else {
        // Coroutine did not suspend.

        state.release();

        //
        // ... start executing the coroutine body
        //
    }
    return __return_val;
}
```

对 `await_resume()` 和 `__tmp1` 的析构函数的调用将出现在协程体中，因此不需要在 ramp 函数中编写这些逻辑。

现在，我们的初始挂起点已经实现了（大部分）功能，但是在这个 ramp 函数中仍然有一些待办事项。为了能够解决这些问题，我们需要先了解如何实现协程的挂起和恢复。

## 第五步：记录挂起点

当协程挂起时，它需要确保未来这个协程恢复执行时，控制流还会回到挂起时的位置。

而且，不是所有被挂起的协程都一定会被恢复，它们也可能直接就被销毁了，再也不会被恢复。因此，我们还需要跟踪每个挂起点上还存活着的对象，以便在协程被销毁时将这些对象也销毁 _(调用这些对象的析构函数)_。

一种实现方法是为协程中的每个挂起点分配一个唯一的编号，并在协程状态中添加一个整数数据成员存储此编号。

每当协程挂起时，它将挂起的挂起点编号写入协程状态，并在恢复/销毁时检查该整数编号以确定挂起点的位置。

请注意，这不是存储挂起点的唯一方法，然而，截至本文撰写时（约2022年），三个主流编译器（MSVC、Clang、GCC）都使用了这种方法。

另一个可行的方案是为每个挂起点使用单独的恢复/销毁函数，在每次挂起时使用函数指针指向不同的恢复/销毁函数，从而为每个挂起点实现不同的恢复/销毁策略，但本文不会探讨这种方案。

因此，让我们在协程状态中扩展一个整数数据成员来存储挂起点索引，并将其初始化为零（我们将始终将其作为初始挂起点的值）。

```c++
struct __g_state {
    __g_state(int&& x);

    int x;
    __g_promise_t __promise;
    int __suspend_point = 0;  // <-- add the suspend-point index
    manual_lifetime<std::suspend_always> __tmp1;
    // to be filled out
};
```

## 第六步：实现 `coroutine_handle::resume()` 和 `coroutine_handle::destroy()`

当通过调用 `coroutine_handle::resume()` 来恢复协程时，我们需要确保调用的函数能够执行挂起的协程的剩余部分。
被调用的函数可以查找挂起点索引，并跳转到控制流中适当的位置。

我们还需要实现 `coroutine_handle::destroy()` 函数，以便在协程挂起后调用适当的逻辑来销毁任何在作用域内的对象。_(销毁尚未挂起的协程是未定义行为)_

我们还需要实现 `coroutine_handle::done()` 函数来查询当前挂起点是否为最终挂起点。

`coroutine_handle` 方法的接口不知道具体的协程状态类型 - `coroutine_handle<void>` 类型可以指向任何协程实例。
这意味着我们需要以一种类型擦除的方式 (a way that type-erases the coroutine state type)来实现它们 _(意思是我们可以使用`coroutine_handle<void>`来指代任意协程，而不用关注其具体的类型)_。

我们可以通过存储指向该协程类型的 resume/destroy 函数的函数指针，并使 `coroutine_handle::resume/destroy()` 调用这些函数指针来实现类型擦除。

`coroutine_handle` 类型和 `void *`还需要能够使用 `coroutine_handle::address()` 和 `coroutine_handle::from_address()` 方法实现互转。

此外，协程可以从 _任何_ 对该协程的句柄进行恢复/销毁，而不仅仅是传递给最近的 `await_suspend()` 调用的句柄。_(标准要求 `std::coroutine_handle<Promise>` 是 non-owning的，也就是说我们可以同时拥有多个 `coroutine_handle`指向同一个协程/协程状态/协程帧)_

这些要求导致我们的 `coroutine_handle` 类型仅包含指向协程状态的指针，并将恢复/销毁函数指针存储为协程状态的数据成员，而不是将它们存储在 `coroutine_handle` 中。

此外，由于我们需要 `coroutine_handle` 能够指向任意协程状态对象，我们需要函数指针数据成员的布局在所有协程状态类型中保持一致。

一种直接的方法是让每个协程状态类型继承自一个包含这些数据成员的基类。

例如，我们可以定义以下类型作为所有协程状态类型的基类
```c++
struct __coroutine_state {
    using __resume_fn = void(__coroutine_state*);
    using __destroy_fn = void(__coroutine_state*);

    __resume_fn* __resume;
    __destroy_fn* __destroy;
};
```

然后，`coroutine_handle::resume()` 方法可以简单地调用 `__resume()`，传递一个指向 `__coroutine_state` 对象的指针。
类似地，我们可以为 `coroutine_handle::destroy()` 方法和 `__destroy` 函数指针做同样的处理。

对于 `coroutine_handle::done()` 方法，我们选择将空的 `__resume` 函数指针视为处于最终挂起点的指示。这很方便，因为最终挂起点不支持 `resume()`，只支持 `destroy()`。如果有人尝试在最终挂起点挂起的协程上调用 `resume()`（这是未定义行为），那么他们最终会调用一个空的函数指针，这应该很快失败并指出他们的错误。

根据此，我们可以实现 `coroutine_handle<void>` 类型如下：
```c++
namespace std
{
    template<typename Promise = void>
    class coroutine_handle;

    template<>
    class coroutine_handle<void> {
    public:
        coroutine_handle() noexcept = default;
        coroutine_handle(const coroutine_handle&) noexcept = default;
        coroutine_handle& operator=(const coroutine_handle&) noexcept = default;

        void* address() const {
            return static_cast<void*>(state_);
        }

        static coroutine_handle from_address(void* ptr) {
            coroutine_handle h;
            h.state_ = static_cast<__coroutine_state*>(ptr);
            return h;
        }

        explicit operator bool() noexcept {
            return state_ != nullptr;
        }
        
        friend bool operator==(coroutine_handle a, coroutine_handle b) noexcept {
            return a.state_ == b.state_;
        }

        void resume() const {
            state_->__resume(state_);
        }
        void destroy() const {
            state_->__destroy(state_);
        }

        bool done() const {
            return state_->__resume == nullptr;
        }

    private:
        __coroutine_state* state_ = nullptr;
    };
}
```

## 第7步：实现 `coroutine_handle<Promise>::promise()` 和 `from_promise()`

对于更一般的 `coroutine_handle<Promise>` 特化，大部分实现可以重用 `coroutine_handle<void>` 的实现。然而，我们还需要能够访问协程状态返回的 promise 对象的 `promise()` 方法，并且能够根据 promise 对象的引用构造一个 `coroutine_handle`。

然而，我们不能简单地指向具体的协程状态类型，因为 `coroutine_handle<Promise>` 类型必须能够引用任何 promise 类型为 `Promise` 的协程状态。

我们需要定义一个新的协程状态基类，它继承自 `__coroutine_state`，并包含 promise 对象，以便我们可以定义所有使用特定 promise 类型的协程状态类型都继承自这个基类。

```c++
template<typename Promise>
struct __coroutine_state_with_promise : __coroutine_state {
    __coroutine_state_with_promise() noexcept {}
    ~__coroutine_state_with_promise() {}

    union {
        Promise __promise;
    };
};
```

你可能会想为什么我们在这里将`__promise`成员声明在一个匿名union里...

这是因为为特定协程函数创建的派生类包含了参数副本数据成员的定义。派生类的数据成员默认在基类的数据成员之后初始化，因此将promise对象声明为普通数据成员意味着promise对象将在参数副本数据成员之前被构造。

然而，我们需要在参数副本的构造函数之后调用promise的构造函数 - 因为我们可能需要将参数副本的引用传递给promise构造函数。

因此，我们在这个基类中为promise对象保留存储空间，以便它与协程状态的起始位置具有一致的偏移量，但是将派生类负责在参数副本初始化后的适当时机调用构造函数/析构函数。将`__promise`声明为union成员提供了这种控制。

让我们更新`__g_state`类，使其继承这个新的基类。

```c++
struct __g_state : __coroutine_state_with_promise<__g_promise_t> {
    __g_state(int&& __x)
    : x(static_cast<int&&>(__x)) {
        // Use placement-new to initialise the promise object in the base-class
        ::new ((void*)std::addressof(this->__promise))
            __g_promise_t(construct_promise<__g_promise_t>(x));
    }

    ~__g_state() {
        // Also need to manually call the promise destructor before the
        // argument objects are destroyed.
        this->__promise.~__g_promise_t();
    }

    int __suspend_point = 0;
    int x;
    manual_lifetime<std::suspend_always> __tmp1;
    // to be filled out
};
```

现在我们已经定义了promise基类，我们可以实现`std::coroutine_handle<Promise>`类模板。

大部分实现应该与`coroutine_handle<void>`中的等效方法基本相同，只是使用`__coroutine_state_with_promise<Promise>`指针代替`__coroutine_state`指针。

唯一的新部分是添加了`promise()`和`from_promise()`函数。

* `promise()`方法很简单 - 它只是返回协程状态的`__promise`成员的引用。
* `from_promise()`方法需要我们根据promise对象的地址计算出协程状态的地址。我们可以通过从promise对象的地址减去`__promise`成员的偏移量来实现这一点。

`coroutine_handle<Promise>`的实现：

```c++
namespace std
{
    template<typename Promise>
    class coroutine_handle {
        using state_t = __coroutine_state_with_promise<Promise>;
    public:
        coroutine_handle() noexcept = default;
        coroutine_handle(const coroutine_handle&) noexcept = default;
        coroutine_handle& operator=(const coroutine_handle&) noexcept = default;

        operator coroutine_handle<void>() const noexcept {
            return coroutine_handle<void>::from_address(address());
        }

        explicit operator bool() const noexcept {
            return state_ != nullptr;
        }

        friend bool operator==(coroutine_handle a, coroutine_handle b) noexcept {
            return a.state_ == b.state_;
        }

        void* address() const {
            return static_cast<void*>(static_cast<__coroutine_state*>(state_));
        }

        static coroutine_handle from_address(void* ptr) {
            coroutine_handle h;
            h.state_ = static_cast<state_t*>(static_cast<__coroutine_state*>(ptr));
            return h;
        }

        Promise& promise() const {
            return state_->__promise;
        }

        static coroutine_handle from_promise(Promise& promise) {
            coroutine_handle h;

            // We know the address of the __promise member, so calculate the
            // address of the coroutine-state by subtracting the offset of
            // the __promise field from this address.
            h.state_ = reinterpret_cast<state_t*>(
                reinterpret_cast<unsigned char*>(std::addressof(promise)) -
                offsetof(state_t, __promise));

            return h;
        }

        // Define these in terms of their `coroutine_handle<void>` implementations

        void resume() const {
            static_cast<coroutine_handle<void>>(*this).resume();
        }

        void destroy() const {
            static_cast<coroutine_handle<void>>(*this).destroy();
        }

        bool done() const {
            return static_cast<coroutine_handle<void>>(*this).done();
        }

    private:
        state_t* state_;
    };
}
```

现在我们已经定义了协程恢复的机制，我们可以回到我们的 "ramp" 函数并更新它，以初始化我们添加到协程状态的新函数指针数据成员。

## 第8步：协程体的开始

让我们现在提前声明正确的resume/destroy函数的签名，并更新`__g_state`构造函数，以初始化协程状态，使得resume/destroy函数指针指向它们：

```c++
void __g_resume(__coroutine_state* s);
void __g_destroy(__coroutine_state* s);

struct __g_state : __coroutine_state_with_promise<__g_promise_t> {
    __g_state(int&& __x)
    : x(static_cast<int&&>(__x)) {
        // Initialise the function-pointers used by coroutine_handle methods.
        this->__resume = &__g_resume;
        this->__destroy = &__g_destroy;

        // Use placement-new to initialise the promise object in the base-class
        ::new ((void*)std::addressof(this->__promise))
            __g_promise_t(construct_promise<__g_promise_t>(x));
    }

    // ... rest omitted for brevity
};


task g(int x) {
    std::unique_ptr<__g_state> state(new __g_state(static_cast<int&&>(x)));
    decltype(auto) return_value = state->__promise.get_return_object();

    state->__tmp1.construct_from([&]() -> decltype(auto) {
        return state->__promise.initial_suspend();
    });
    if (!state->__tmp1.get().await_ready()) {
        state->__tmp1.get().await_suspend(
            std::coroutine_handle<__g_promise_t>::from_promise(state->__promise));
        state.release();
        // fall through to return statement below.
    } else {
        // Coroutine did not suspend. Start executing the body immediately.
        __g_resume(state.release());
    }
    return return_value;
}
```

这样ramp函数就写完了，现在我们可以专注于`g()`函数的resume/destroy函数。

让我们开始完成initial-suspend表达式的降级。

当调用`__g_resume()`并且`__suspend_point`索引为0时，我们需要通过调用`__tmp1`的`await_resume()`方法来恢复执行，然后调用`__tmp1`的析构函数。

```c++
void __g_resume(__coroutine_state* s) {
    // We know that 's' points to a __g_state.
    auto* state = static_cast<__g_state*>(s);

    // Generate a jump-table to jump to the correct place in the code based
    // on the value of the suspend-point index.
    switch (state->__suspend_point) {
    case 0: goto suspend_point_0;
    default: std::unreachable();
    }

suspend_point_0:
    state->__tmp1.get().await_resume();
    state->__tmp1.destroy();

    // TODO: Implement rest of coroutine body.
    //
    //  int fx = co_await f(x);
    //  co_return fx * fx;
}
```

当调用`__g_destroy()`并且`__suspend_point`索引为0时，我们需要先销毁`__tmp1`，然后再销毁和释放协程状态。

```c++
void __g_destroy(__coroutine_state* s) {
    auto* state = static_cast<__g_state*>(s);

    switch (state->__suspend_point) {
    case 0: goto suspend_point_0;
    default: std::unreachable();
    }

suspend_point_0:
    state->__tmp1.destroy();
    goto destroy_state;

    // TODO: Add extra logic for other suspend-points here.

destroy_state:
    delete state;
}
```

## 第9步：降级`co_await`表达式

接下来，让我们来看看如何降级`co_await f(x)`表达式。

首先，我们需要评估`f(x)`，它返回一个临时的`task`对象。

由于临时的`task`对象在语句的末尾分号之前不会被销毁，并且该语句包含一个`co_await`表达式，因此`task`的生命周期跨越了一个挂起点，所以它必须存储在协程状态中。

当在这个临时的`task`对象上评估`co_await`表达式时，我们需要调用`operator co_await()`方法，该方法返回一个临时的`awaiter`对象。这个对象的生命周期也跨越了挂起点，所以必须存储在协程状态中。

让我们向`__g_state`类型添加必要的成员：

```c++
struct __g_state : __coroutine_state_with_promise<__g_promise_t> {
    __g_state(int&& __x);
    ~__g_state();

    int __suspend_point = 0;
    int x;
    manual_lifetime<std::suspend_always> __tmp1;
    manual_lifetime<task> __tmp2;
    manual_lifetime<task::awaiter> __tmp3;
};
```

然后，我们可以更新`__g_resume()`函数来初始化这些临时变量，并评估由`co_await`表达式的`await_ready`、`await_suspend`和`await_resume`调用组成的剩余部分。

注意，`task::awaiter::await_suspend()`方法返回一个协程句柄，因此我们需要生成代码来恢复返回的句柄。

在调用`await_suspend()`之前，我们还需要更新挂起点编号（我们将使用编号1代表此挂起点），然后在跳转表中添加一个额外的条目，以确保我们恢复到正确的位置。

```c++
void __g_resume(__coroutine_state* s) {
    // We know that 's' points to a __g_state.
    auto* state = static_cast<__g_state*>(s);

    // Generate a jump-table to jump to the correct place in the code based
    // on the value of the suspend-point index.
    switch (state->__suspend_point) {
    case 0: goto suspend_point_0;
    case 1: goto suspend_point_1; // <-- add new jump-table entry
    default: std::unreachable();
    }

suspend_point_0:
    state->__tmp1.get().await_resume();
    state->__tmp1.destroy();

    //  int fx = co_await f(x);
    state->__tmp2.construct_from([&] {
        return f(state->x);
    });
    state->__tmp3.construct_from([&] {
        return static_cast<task&&>(state->__tmp2.get()).operator co_await();
    });
    if (!state->__tmp3.get().await_ready()) {
        // mark the suspend-point
        state->__suspend_point = 1;

        auto h = state->__tmp3.get().await_suspend(
            std::coroutine_handle<__g_promise_t>::from_promise(state->__promise));
        
        // Resume the returned coroutine-handle before returning.
        h.resume();
        return;
    }

suspend_point_1:
    int fx = state->__tmp3.get().await_resume();
    state->__tmp3.destroy();
    state->__tmp2.destroy();

    // TODO: Implement
    //  co_return fx * fx;
}
```

注意，`int fx` 局部变量的生命周期不跨越挂起点，因此不需要将其存储在协程状态中。我们可以将其作为普通的局部变量存储在 `__g_resume` 函数中。

我们还需要在 `__g_destroy()` 函数中添加必要的条目，以处理在此挂起点处销毁协程的情况。

```c++
void __g_destroy(__coroutine_state* s) {
    auto* state = static_cast<__g_state*>(s);

    switch (state->__suspend_point) {
    case 0: goto suspend_point_0;
    case 1: goto suspend_point_1; // <-- add new jump-table entry
    default: std::unreachable();
    }

suspend_point_0:
    state->__tmp1.destroy();
    goto destroy_state;

suspend_point_1:
    state->__tmp3.destroy();
    state->__tmp2.destroy();
    goto destroy_state;

    // TODO: Add extra logic for other suspend-points here.

destroy_state:
    delete state;
}
```

所以现在我们已经完成了该语句的实现：

```c++
int fx = co_await f(x);
```

然而，函数 `f(x)` 没有标记为 `noexcept`，因此它可能会抛出异常。
此外，`awaiter::await_resume()` 方法也没有标记为 `noexcept`，也可能会抛出异常。

当从协程体中抛出异常时，编译器会生成代码来捕获异常，然后调用 `promise.unhandled_exception()` 来给 promise 一个机会处理异常。让我们接下来实现这个功能。

## 第10步：实现 `unhandled_exception()`

协程定义的规范 [`[dcl.fct.def.coroutine]`](https://eel.is/c++draft/dcl.fct.def.coroutine)
指出，协程的行为就好像它的函数体被替换为：

```c++
{
    promise-type promise promise-constructor-arguments ;
    try {
        co_await promise.initial_suspend() ;
        function-body
    } catch ( ... ) {
        if (!initial-await-resume-called)
            throw ;
        promise.unhandled_exception() ;
    }
final-suspend :
    co_await promise.final_suspend() ;
}
```

我们已经在 ramp 函数中单独处理了 `initial-await_resume-called` 分支，因此在这里我们不需要担心它。

让我们调整 `__g_resume()` 函数，在函数体周围插入 try/catch 块。

请注意，我们需要小心将跳转到正确位置的 `switch` 放在 try 块内，因为我们不允许使用 `goto` 进入 try 块。

此外，我们需要小心在 try/catch 块之外调用从 `await_suspend()` 返回的协程句柄的 `.resume()` 方法。
如果从调用 `.resume()` 的协程中抛出异常，则不应该被当前协程捕获，而是应该在调用 `resume()` 的地方传播出去。
因此，我们将协程句柄存储在函数顶部声明的变量中，然后使用 `goto` 跳转到 try/catch 块之外的位置，在那里执行调用 `.resume()` 的操作。

```c++
void __g_resume(__coroutine_state* s) {
    auto* state = static_cast<__g_state*>(s);

    std::coroutine_handle<void> coro_to_resume;

    try {
        switch (state->__suspend_point) {
        case 0: goto suspend_point_0;
        case 1: goto suspend_point_1; // <-- add new jump-table entry
        default: std::unreachable();
        }

suspend_point_0:
        state->__tmp1.get().await_resume();
        state->__tmp1.destroy();

        //  int fx = co_await f(x);
        state->__tmp2.construct_from([&] {
            return f(state->x);
        });
        state->__tmp3.construct_from([&] {
            return static_cast<task&&>(state->__tmp2.get()).operator co_await();
        });
        
        if (!state->__tmp3.get().await_ready()) {
            state->__suspend_point = 1;
            coro_to_resume = state->__tmp3.get().await_suspend(
                std::coroutine_handle<__g_promise_t>::from_promise(state->__promise));
            goto resume_coro;
        }

suspend_point_1:
        int fx = state->__tmp3.get().await_resume();
        state->__tmp3.destroy();
        state->__tmp2.destroy();

        // TODO: Implement
        //  co_return fx * fx;
    } catch (...) {
        state->__promise.unhandled_exception();
        goto final_suspend;
    }

final_suspend:
    // TODO: Implement
    // co_await promise.final_suspend();

resume_coro:
    coro_to_resume.resume();
    return;
}
```

上述代码中存在一个bug。在`__tmp3.get().await_resume()`调用引发异常的情况下，我们将无法在捕获异常之前调用`__tmp3`和`__tmp2`的析构函数。

请注意，我们不能简单地捕获异常，调用析构函数，然后重新抛出异常，因为这样会改变这些析构函数的行为，如果它们在调用`std::unhandled_exceptions()`时会发生异常，那么异常将被"处理"。然而，如果析构函数在异常展开期间调用它，那么调用`std::unhandled_exceptions()`应该返回非零值。

相反，我们可以定义一个RAII辅助类，在异常抛出时确保析构函数被调用。

```c++
template<typename T>
struct destructor_guard {
    explicit destructor_guard(manual_lifetime<T>& obj) noexcept
    : ptr_(std::addressof(obj))
    {}

    // non-movable
    destructor_guard(destructor_guard&&) = delete;
    destructor_guard& operator=(destructor_guard&&) = delete;

    ~destructor_guard() noexcept(std::is_nothrow_destructible_v<T>) {
        if (ptr_ != nullptr) {
            ptr_->destroy();
        }
    }

    void cancel() noexcept { ptr_ = nullptr; }

private:
    manual_lifetime<T>* ptr_;
};

// Partial specialisation for types that don't need their destructors called.
template<typename T>
    requires std::is_trivially_destructible_v<T>
struct destructor_guard<T> {
    explicit destructor_guard(manual_lifetime<T>&) noexcept {}
    void cancel() noexcept {}
};

// Class-template argument deduction to simplify usage
template<typename T>
destructor_guard(manual_lifetime<T>& obj) -> destructor_guard<T>;
```

使用这个实用工具，我们现在可以使用这个类型来确保在抛出异常时销毁存储在协程状态中的变量。

让我们还使用这个类来调用现有变量的析构函数，以便在它们离开作用域时也调用它们的析构函数。

```c++
void __g_resume(__coroutine_state* s) {
    auto* state = static_cast<__g_state*>(s);

    std::coroutine_handle<void> coro_to_resume;

    try {
        switch (state->__suspend_point) {
        case 0: goto suspend_point_0;
        case 1: goto suspend_point_1; // <-- add new jump-table entry
        default: std::unreachable();
        }

suspend_point_0:
        {
            destructor_guard tmp1_dtor{state->__tmp1};
            state->__tmp1.get().await_resume();
        }

        //  int fx = co_await f(x);
        {
            state->__tmp2.construct_from([&] {
                return f(state->x);
            });
            destructor_guard tmp2_dtor{state->__tmp2};

            state->__tmp3.construct_from([&] {
                return static_cast<task&&>(state->__tmp2.get()).operator co_await();
            });
            destructor_guard tmp3_dtor{state->__tmp3};

            if (!state->__tmp3.get().await_ready()) {
                state->__suspend_point = 1;

                coro_to_resume = state->__tmp3.get().await_suspend(
                    std::coroutine_handle<__g_promise_t>::from_promise(state->__promise));

                // A coroutine suspends without exiting scopes.
                // So cancel the destructor-guards.
                tmp3_dtor.cancel();
                tmp2_dtor.cancel();

                goto resume_coro;
            }

            // Don't exit the scope here.
            //
            // We can't 'goto' a label that enters the scope of a variable with a
            // non-trivial destructor. So we have to exit the scope of the destructor
            // guards here without calling the destructors and then recreate them after
            // the `suspend_point_1` label.
            tmp3_dtor.cancel();
            tmp2_dtor.cancel();
        }

suspend_point_1:
        int fx = [&]() -> decltype(auto) {
            destructor_guard tmp2_dtor{state->__tmp2};
            destructor_guard tmp3_dtor{state->__tmp3};
            return state->__tmp3.get().await_resume();
        }();

        // TODO: Implement
        //  co_return fx * fx;
    } catch (...) {
        state->__promise.unhandled_exception();
        goto final_suspend;
    }

final_suspend:
    // TODO: Implement
    // co_await promise.final_suspend();

resume_coro:
    coro_to_resume.resume();
    return;
}
```

现在，在存在异常的情况下，我们的协程体将正确销毁局部变量，并且如果这些异常传播出协程体，将正确调用`promise.unhandled_exception()`方法。

值得注意的是，对于`promise.unhandled_exception()`方法本身引发异常的情况，可能还需要特殊处理（例如，如果它重新抛出当前异常）。

在这种情况下，协程需要捕获异常，将协程标记为在最终挂起点处挂起，然后重新抛出异常。

例如：`__g_resume()`函数的catch块应该如下所示：
```c++
try {
  // ...
} catch (...) {
    try {
        state->__promise.unhandled_exception();
    } catch (...) {
        state->__suspend_point = 2;
        state->__resume = nullptr; // mark as final-suspend-point
        throw;
    }
}
```
并且我们需要在`__g_destroy`函数的跳转表中添加一个额外的条目：
```c++
switch (state->__suspend_point) {
case 0: goto suspend_point_0;
case 1: goto suspend_point_1;
case 2: goto destroy_state; // no variables in scope that need to be destroyed
                            // just destroy the coroutine-state object.
}
```

请注意，在这种情况下，最终挂起点不一定与`co_await promise.final_suspend()`的挂起点相同。

这是因为`promise.final_suspend()`的挂起点通常会有一些与`co_await`表达式相关的额外临时对象，当调用`coroutine_handle::destroy()`时需要销毁这些对象。而如果`promise.unhandled_exception()`引发异常，则这些临时对象将不存在，因此不需要由`coroutine_handle::destroy()`销毁。

## 第11步：实现`co_return`

下一步是实现`co_return fx * fx;`语句。

与之前的步骤相比，这相对简单。

`co_return <expr>`语句被映射为：
```c++
promise.return_value(<expr>);
goto final-suspend-point;
```

所以我们可以简单地将TODO注释替换为：
```c++
state->__promise.return_value(fx * fx);
goto final_suspend;
```

简单。

## 第12步：实现`final_suspend()`

现在代码中的最后一个TODO是实现`co_await promise.final_suspend()`语句。

`final_suspend()`方法返回一个临时的`task::promise_type::final_awaiter`类型，需要将其存储在协程状态中，并在`__g_destroy`中销毁。

这个类型没有自己的`operator co_await()`，所以我们不需要为该调用的结果创建额外的临时对象。

与`task::awaiter`类型一样，它也使用返回协程句柄的形式的`await_suspend()`。因此，我们需要确保在返回的句柄上调用`resume()`。

如果协程在final-suspend-point处不挂起，则协程状态会被隐式销毁。因此，如果执行到协程的末尾，我们需要删除状态对象。

另外，由于所有的最终挂起逻辑都要求是noexcept的，我们不需要担心这里的任何子表达式引发异常。

让我们首先将数据成员添加到`__g_state`类型中。
```c++
struct __g_state : __coroutine_state_with_promise<__g_promise_t> {
    __g_state(int&& __x);
    ~__g_state();

    int __suspend_point = 0;
    int x;
    manual_lifetime<std::suspend_always> __tmp1;
    manual_lifetime<task> __tmp2;
    manual_lifetime<task::awaiter> __tmp3;
    manual_lifetime<task::promise_type::final_awaiter> __tmp4; // <---
};
```

然后，我们可以按照以下方式实现final-suspend表达式的主体：
```c++
final_suspend:
    // co_await promise.final_suspend
    {
        state->__tmp4.construct_from([&]() noexcept {
            return state->__promise.final_suspend();
        });
        destructor_guard tmp4_dtor{state->__tmp4};

        if (!state->__tmp4.get().await_ready()) {
            state->__suspend_point = 2;
            state->__resume = nullptr; // mark as final suspend-point

            coro_to_resume = state->__tmp4.get().await_suspend(
                std::coroutine_handle<__g_promise_t>::from_promise(state->__promise));

            tmp4_dtor.cancel();
            goto resume_coro;
        }

        state->__tmp4.get().await_resume();
    }

    //  Destroy coroutine-state if execution flows off end of coroutine
    delete state;
    return;
```
 
现在我们还需要更新`__g_destroy`函数来处理这个新的挂起点。
```c++
void __g_destroy(__coroutine_state* state) {
    auto* state = static_cast<__g_state*>(s);

    switch (state->__suspend_point) {
    case 0: goto suspend_point_0;
    case 1: goto suspend_point_1;
    case 2: goto suspend_point_2;
    default: std::unreachable();
    }

suspend_point_0:
    state->__tmp1.destroy();
    goto destroy_state;

suspend_point_1:
    state->__tmp3.destroy();
    state->__tmp2.destroy();
    goto destroy_state;

suspend_point_2:
    state->__tmp4.destroy();
    goto destroy_state;

destroy_state:
    delete state;
}
```

我们现在已经完全实现了`g()`协程函数的降级。

我们完成了！就是这样！

或者还有其他的事情吗....

## 第13步：实现对称传输 (symmetric transfer) 和空操作协程 (noop-coroutine)

事实证明，我们上面实现的`__g_resume()`函数存在问题。

关于这些问题的详细讨论可以在之前的博文中找到，如果你想更深入地了解问题，请查看博文[C++ Coroutines: Understanding Symmetric Transfer](https://lewissbaker.github.io/2020/05/11/understanding_symmetric_transfer)。

[expr.await]的规范对于我们应该如何处理返回协程句柄的`await_suspend`提供了一点提示：
> 如果_await-suspend_的类型是`std::coroutine_handle<Z>`，则会评估 (evaluate) _await-suspend_`.resume()`。
> 
> [_注_ 1：这会恢复由 _await-suspend_ 引用的协程。
> 可以按照这种方式连续恢复任意数量的协程，但最终将控制流将返回给当前协程的调用者或恢复者（[\[dcl.fct.def.coroutine\]](https://eel.is/c++draft/dcl.fct.def.coroutine)）。 —- ]

上面注释中提到的文档，虽然它是非规范性的，不具有约束力，但它强烈鼓励编译器以一种执行尾调用来恢复下一个协程的方式实现它，而不是递归地恢复下一个协程。这是因为如果协程在循环中相互恢复，递归地恢复下一个协程很容易导致无限堆栈增长。

问题在于我们在`__g_resume()`函数的主体内部调用了下一个协程的`.resume()`方法，然后返回，因此在下一个协程挂起并返回之前，`__g_resume()`帧使用的堆栈空间不会被释放。

编译器通过将下一个协程的恢复操作实现为尾调用来实现这一点。这样，编译器生成的代码首先弹出当前的栈帧，保留返回地址，然后执行`jmp`指令跳转到下一个协程的恢复函数。

由于在C++中没有机制可以指定尾位置的函数调用应该是尾调用，因此我们需要实际从恢复函数中返回，以便可以释放其堆栈空间，然后让调用者恢复下一个协程。

由于下一个协程在挂起时可能还需要恢复另一个协程，而这可能会无限循环进行，所以调用者需要在循环中恢复协程。

这样的循环通常被称为“跳板循环” (trampoline loop)，因为我们从一个协程返回到循环，然后再次从循环“跳”到下一个协程。

如果我们修改恢复函数的签名，使其返回指向下一个协程的协程状态的指针，而不是返回void，那么`coroutine_handle::resume()`函数就可以立即调用下一个协程的`__resume()`函数指针来恢复它。

让我们修改`__coroutine_state`的`__resume_fn`的签名：
```c++
struct __coroutine_state {
    using __resume_fn = __coroutine_state* (__coroutine_state*);
    using __destroy_fn = void (__coroutine_state*);

    __resume_fn* __resume;
    __destroy_fn* __destroy;
};
```

然后我们可以这样编写`coroutine_handle::resume()`函数：
```c++
void std::coroutine_handle<void>::resume() const {
    __coroutine_state* s = state_;
    do {
        s = s->__resume(s);
    } while (/* some condition */);
}
```

接下来的问题是：“条件应该是什么？”

这就是`std::noop_coroutine()`辅助函数发挥作用的地方。

`std::noop_coroutine()`是一个工厂函数，返回一个特殊的协程句柄，该句柄具有一个空操作的`resume()`和`destroy()`方法。如果一个协程在`await_suspend()`方法中挂起并返回noop-coroutine句柄，那么这表示没有更多的协程需要恢复，并且调用`coroutine_handle::resume()`来恢复该协程的操作应该返回给其调用者。

因此，我们需要实现`std::noop_coroutine()`和`coroutine_handle::resume()`中的条件，使得当`__coroutine_state`指针指向noop-coroutine-state时，条件返回false，循环退出。

我们可以使用一种策略，在这里定义一个被指定为noop-coroutine-state的静态实例。
`std::noop_coroutine()`函数可以返回一个指向该对象的协程句柄，我们可以将`__coroutine_state`指针与该对象的地址进行比较，以确定特定的协程句柄是否为noop-coroutine。

首先，让我们定义这个特殊的noop-coroutine-state对象：
```c++
struct __coroutine_state {
    using __resume_fn = __coroutine_state* (__coroutine_state*);
    using __destroy_fn = void (__coroutine_state*);

    __resume_fn* __resume;
    __destroy_fn* __destroy;

    static __coroutine_state* __noop_resume(__coroutine_state* state) noexcept {
        return state;
    }

    static void __noop_destroy(__coroutine_state*) noexcept {}

    static const __coroutine_state __noop_coroutine;
};

inline const __coroutine_state __coroutine_state::__noop_coroutine{
    &__coroutine_state::__noop_resume,
    &__coroutine_state::__noop_destroy
};
```

然后我们可以实现`std::coroutine_handle<noop_coroutine_promise>`的特化。
```c++
namespace std
{
    struct noop_coroutine_promise {};

    using noop_coroutine_handle = coroutine_handle<noop_coroutine_promise>;

    noop_coroutine_handle noop_coroutine() noexcept;

    template<>
    class coroutine_handle<noop_coroutine_promise> {
    public:
        constexpr coroutine_handle(const coroutine_handle&) noexcept = default;
        constexpr coroutine_handle& operator=(const coroutine_handle&) noexcept = default;

        constexpr explicit operator bool() noexcept { return true; }

        constexpr friend bool operator==(coroutine_handle, coroutine_handle) noexcept {
            return true;
        }

        operator coroutine_handle<void>() const noexcept {
            return coroutine_handle<void>::from_address(address());
        }

        noop_coroutine_promise& promise() const noexcept {
            static noop_coroutine_promise promise;
            return promise;
        }

        constexpr void resume() const noexcept {}
        constexpr void destroy() const noexcept {}
        constexpr bool done() const noexcept { return false; }

        constexpr void* address() const noexcept {
            return const_cast<__coroutine_state*>(&__coroutine_state::__noop_coroutine);
        }
    private:
        constexpr coroutine_handle() noexcept = default;

        friend noop_coroutine_handle noop_coroutine() noexcept {
            return {};
        }
    };
}
```

我们可以更新`coroutine_handle::resume()`函数，使其在返回noop-coroutine-state时退出。

```c++
void std::coroutine_handle<void>::resume() const {
    __coroutine_state* s = state_;
    do {
        s = s->__resume(s);
    } while (s != &__coroutine_state::__noop_coroutine);
}
```

最后，我们可以更新`__g_resume()`函数，使其返回`__coroutine_state*`。

这只需要修改签名，并且使用这个:

```c++
coro_to_resume = ...;
goto resume_coro;
```

来替换这个:

```c++
auto h = ...;
return static_cast<__coroutine_state*>(h.address());
```

然后在函数的最后（在`delete state;`语句之后）添加以下代码：
```c++
return static_cast<__coroutine_state*>(std::noop_coroutine().address());
```

# 最后一件事

那些注意力集中的人可能已经注意到，协程状态类型 `__g_state` 实际上比它需要的要大。

这4个临时值的数据成员都为它们的值保留了存储空间。然而，某些临时值的生命周期并不重叠，因此理论上我们可以通过在一个对象的生命周期结束后重用其存储空间来节省协程状态的空间。

为了能够利用这一点，我们可以在适当的地方将数据成员定义在一个匿名联合体中。

观察临时变量的生命周期，我们发现：
- `__tmp1` - 仅在 `co_await promise.initial_suspend();` 语句中存在
- `__tmp2` - 仅在 `int fx = co_await f(x);` 语句中存在
- `__tmp3` - 仅在 `int fx = co_await f(x);` 语句中存在 - 在 `__tmp2` 的生命周期内嵌套
- `__tmp4` - 仅在 `co_await promise.final_suspend();` 语句中存在

由于 `__tmp2` 和 `__tmp3` 变量的生命周期重叠，我们需要将它们放在一个结构体中，
以便它们可以同时存在。

然而，`__tmp1` 和 `__tmp4` 成员的生命周期不重叠，因此它们可以放在一个匿名的联合体中。

因此，我们可以将数据成员的定义更改为：
```c++
struct __g_state : __coroutine_state_with_promise<__g_promise_t> {
    __g_state(int&& x);
    ~__g_state();

    int __suspend_point = 0;
    int x;

    struct __scope1 {
        manual_lifetime<task> __tmp2;
        manual_lifetime<task::awaiter> __tmp3;
    };

    union {
        manual_lifetime<std::suspend_always> __tmp1;
        __scope1 __s1;
        manual_lifetime<task::promise_type::final_awaiter> __tmp4;
    };
};
```

然后，由于`__tmp2`和`__tmp3`变量现在嵌套在`__s1`对象内部，
我们需要更新对它们的引用，现在应该是`state->__s1.tmp2`。但其他代码保持不变。

这样做可以节省额外的16字节的协程状态大小，因为我们不再需要为`__tmp1`和`__tmp4`数据成员的额外存储空间和填充进行内存分配，尽管它们是空类型也会被填充到指针的大小。

# 将所有内容综合起来

好，所以我们最终为这个协程函数:
```c++
task g(int x) {
    int fx = co_await f(x);
    co_return fx * fx;
}
```

所生成的代码如下:
```c++
/////
// The coroutine promise-type

using __g_promise_t = std::coroutine_traits<task, int>::promise_type;

__coroutine_state* __g_resume(__coroutine_state* s);
void __g_destroy(__coroutine_state* s);

/////
// The coroutine-state definition

struct __g_state : __coroutine_state_with_promise<__g_promise_t> {
    __g_state(int&& x)
    : x(static_cast<int&&>(x)) {
        // Initialise the function-pointers used by coroutine_handle methods.
        this->__resume = &__g_resume;
        this->__destroy = &__g_destroy;

        // Use placement-new to initialise the promise object in the base-class
        // after we've initialised the argument copies.
        ::new ((void*)std::addressof(this->__promise))
            __g_promise_t(construct_promise<__g_promise_t>(this->x));
    }

    ~__g_state() {
        this->__promise.~__g_promise_t();
    }

    int __suspend_point = 0;

    // Argument copies
    int x;

    // Local variables/temporaries
    struct __scope1 {
        manual_lifetime<task> __tmp2;
        manual_lifetime<task::awaiter> __tmp3;
    };

    union {
        manual_lifetime<std::suspend_always> __tmp1;
        __scope1 __s1;
        manual_lifetime<task::promise_type::final_awaiter> __tmp4;
    };
};

/////
// The "ramp" function

task g(int x) {
    std::unique_ptr<__g_state> state(new __g_state(static_cast<int&&>(x)));
    decltype(auto) return_value = state->__promise.get_return_object();

    state->__tmp1.construct_from([&]() -> decltype(auto) {
        return state->__promise.initial_suspend();
    });
    if (!state->__tmp1.get().await_ready()) {
        state->__tmp1.get().await_suspend(
            std::coroutine_handle<__g_promise_t>::from_promise(state->__promise));
        state.release();
        // fall through to return statement below.
    } else {
        // Coroutine did not suspend. Start executing the body immediately.
        __g_resume(state.release());
    }
    return return_value;
}

/////
//  The "resume" function

__coroutine_state* __g_resume(__coroutine_state* s) {
    auto* state = static_cast<__g_state*>(s);

    try {
        switch (state->__suspend_point) {
        case 0: goto suspend_point_0;
        case 1: goto suspend_point_1; // <-- add new jump-table entry
        default: std::unreachable();
        }

suspend_point_0:
        {
            destructor_guard tmp1_dtor{state->__tmp1};
            state->__tmp1.get().await_resume();
        }

        //  int fx = co_await f(x);
        {
            state->__s1.__tmp2.construct_from([&] {
                return f(state->x);
            });
            destructor_guard tmp2_dtor{state->__s1.__tmp2};

            state->__s1.__tmp3.construct_from([&] {
                return static_cast<task&&>(state->__s1.__tmp2.get()).operator co_await();
            });
            destructor_guard tmp3_dtor{state->__s1.__tmp3};

            if (!state->__s1.__tmp3.get().await_ready()) {
                state->__suspend_point = 1;

                auto h = state->__s1.__tmp3.get().await_suspend(
                    std::coroutine_handle<__g_promise_t>::from_promise(state->__promise));

                // A coroutine suspends without exiting scopes.
                // So cancel the destructor-guards.
                tmp3_dtor.cancel();
                tmp2_dtor.cancel();

                return static_cast<__coroutine_state*>(h.address());
            }

            // Don't exit the scope here.
            // We can't 'goto' a label that enters the scope of a variable with a
            // non-trivial destructor. So we have to exit the scope of the destructor
            // guards here without calling the destructors and then recreate them after
            // the `suspend_point_1` label.
            tmp3_dtor.cancel();
            tmp2_dtor.cancel();
        }

suspend_point_1:
        int fx = [&]() -> decltype(auto) {
            destructor_guard tmp2_dtor{state->__s1.__tmp2};
            destructor_guard tmp3_dtor{state->__s1.__tmp3};
            return state->__s1.__tmp3.get().await_resume();
        }();

        //  co_return fx * fx;
        state->__promise.return_value(fx * fx);
        goto final_suspend;
    } catch (...) {
        state->__promise.unhandled_exception();
        goto final_suspend;
    }

final_suspend:
    // co_await promise.final_suspend
    {
        state->__tmp4.construct_from([&]() noexcept {
            return state->__promise.final_suspend();
        });
        destructor_guard tmp4_dtor{state->__tmp4};

        if (!state->__tmp4.get().await_ready()) {
            state->__suspend_point = 2;
            state->__resume = nullptr; // mark as final suspend-point

            auto h = state->__tmp4.get().await_suspend(
                std::coroutine_handle<__g_promise_t>::from_promise(state->__promise));

            tmp4_dtor.cancel();
            return static_cast<__coroutine_state*>(h.address());
        }

        state->__tmp4.get().await_resume();
    }

    //  Destroy coroutine-state if execution flows off end of coroutine
    delete state;

    return static_cast<__coroutine_state*>(std::noop_coroutine().address());
}

/////
// The "destroy" function

void __g_destroy(__coroutine_state* s) {
    auto* state = static_cast<__g_state*>(s);

    switch (state->__suspend_point) {
    case 0: goto suspend_point_0;
    case 1: goto suspend_point_1;
    case 2: goto suspend_point_2;
    default: std::unreachable();
    }

suspend_point_0:
    state->__tmp1.destroy();
    goto destroy_state;

suspend_point_1:
    state->__s1.__tmp3.destroy();
    state->__s1.__tmp2.destroy();
    goto destroy_state;

suspend_point_2:
    state->__tmp4.destroy();
    goto destroy_state;

destroy_state:
    delete state;
}

```

对于完全可编译的最终代码版本，请参见：
[https://godbolt.org/z/xaj3Yxabn](https://godbolt.org/z/xaj3Yxabn)

这结束了关于理解 C++ 协程机制的 5 部分系列。

这可能是关于协程的比你想知道的更多的信息，但希望它能帮助你理解底层发生的事情，并稍微揭开一些神秘感。

Thanks for making it through to the end!

Until next time, Lewis.