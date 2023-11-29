# C++ Coroutines: Understanding operator `co_await`

_翻译自: [C++ Coroutines: Understanding operator co_await | Asymmetric Transfer](https://lewissbaker.github.io/2017/11/17/understanding-operator-co-await)_

_这篇文章编写时, coroutine仍然处于TS阶段, 但其介绍的原理基本上和现行的C++ coroutine 标准一致_

  了解 `co_await` 运算符的工作原理可以帮助解开协程的行为以及它们如何被暂停和恢复的神秘面纱。
  在本文中，我将解释 `co_await` 运算符的机制，并介绍相关的“Awaitable”和“Awaiter”类型概念。

  在之前关于[协程理论](https://lewissbaker.github.io/2017/09/25/coroutine-theory)的文章中，
  我描述了函数和协程之间的高级差异，但没有详细介绍由C++协程TS（[N4680](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/n4680.pdf)）描述的协程的语法和语义。

  协程TS为C++语言添加的关键新功能是能够暂停协程，以便稍后恢复。TS提供的机制是通过新的 `co_await`运算符实现的。

  了解 `co_await` 运算符的工作原理可以帮助解开协程的行为以及它们如何被暂停和恢复的神秘面纱。在本文中，我将解释 `co_await` 运算符的机制，并介绍相关的**Awaitable**和**Awaiter**类型概念。

  但在深入讨论 `co_await` 之前，我想简要概述一下协程TS，以提供一些背景。

## Coroutines TS 给我们带来了什么？

* 三个新的语言关键字：`co_await`、`co_yield` 和 `co_return`
* `std::experimental` 命名空间中的几个新类型 (_因为已经合入标准, 所以这几个组件已经不在 `std::experimental`里, 被移进 `std`了_)：
  * `coroutine_handle<P>`
  * `coroutine_traits<Ts...>`
  * `suspend_always`
  * `suspend_never`
* 一个通用机制，供库编写者与协程交互并自定义其行为。
* 一种使编写异步代码变得更加容易的语言工具！

C++ 协程 TS 在语言中提供的功能可以被视为_协程的低级汇编语言_。直接使用这些功能可能很难以安全的方式使用，主要是为了供库编写者使用，以构建更高级的抽象，使应用程序开发者能够以安全的方式使用。

计划是将这些新的低级功能与一些附带的高级类型一起交付到即将发布的语言标准中（希望是 C++20, _但实际并未提供_），标准库中的这些高级类型将包装这些低级构建块，并以安全的方式使协程更易于应用程序开发者使用。

## 编译器 <-> 库的交互

有趣的是，协程 TS 实际上并没有定义协程的语义。它没有定义如何生成返回给调用者的值。它没有定义如何处理传递给 `co_return` 语句的返回值，也没有定义如何处理从协程中传播出来的异常。它没有定义协程应该在哪个线程上恢复。

相反，它指定了一个通用机制，供库代码通过实现符合特定接口的类型来自定义协程的行为。然后编译器会生成调用库提供的类型实例上的方法的代码。这种方法类似于库编写者通过定义 `begin()`/`end()` 方法和一个 `iterator` 类型来自定义基于范围的 for 循环的行为。

协程 TS 并没有对协程的机制规定任何特定的语义，这使得它成为一个强大的工具。它允许库编写者为各种不同的目的定义许多不同类型的协程。

例如，您可以定义一个异步产生单个值的协程，或者定义一个惰性产生值序列的协程，或者定义一个简化对 `optional<T>` 值进行控制流的协程，如果遇到 `nullopt` 值，则提前退出。

协程 TS 定义了两种接口：**Promise** 接口和 **Awaitable** 接口。

**Promise** 接口指定了自定义协程本身行为的方法。库编写者可以自定义协程被调用时的行为，协程返回时的行为（通过正常方式返回或通过未处理的异常返回），以及协程内部的任何 `co_await` 或 `co_yield` 表达式的行为。

**Awaitable** 接口指定了控制 `co_await` 表达式语义的方法。当一个值被 `co_await` 时，代码会被转换为对 awaitable 对象上的一系列方法的调用，这些方法允许它:

+ 判定是否挂起当前协程
+ 在挂起后执行一些逻辑以安排协程以便稍后恢复
+ 在协程恢复后执行一些逻辑以产生 `co_await` 表达式的结果。

我将在以后的文章中详细介绍 **Promise** 接口的细节，但现在让我们来看看 **Awaitable** 接口。

_原文下方几节的表述有很大问题, 这里我参考标准重写了_

## `operator co_await`

`co_await` 运算符是一个可以应用于表达式的新的一元运算符。

它只能在协程的上下文中使用，然而，这是一个自证的命题，因为任何包含 `co_await` 运算符的函数体都将被编译为协程(_不仅存在有协程函数, 协程lambda也是合法的_)。

`co_await <expr>`几乎可以出现在协程上下文中的任意位置:

+ `co_await someValue`
+ `co_await SomeFunction()`
+ `auto a = (co_await SomeFunction(b, c)) + (co_await AnotherFunction(d, e))`
+ `co_await SomeFunction(co_await AnotherFunction(a), b)`

`co_await <expr>`是控制协程挂起和恢复的重要表达式, 它依序做如下几步:

1. 获取 一个 **Awaitable** 类型的对象。
2. 从 **Awaitable** 对象得到一个 **Awaiter** 类型的对象
3. 使用这个 **Awaiter** 类型对象控制协程的挂起, 并在协程恢复后执行一些自定义代码

## **Awaitable** 类型

**Awaitable**类型不是某种特定的类型, 其是一组类型的总称。

这是因为协程使用的 **Promise** 类型可以通过其 `await_transform` 方法改变 `co_await` 表达式在协程内的含义（稍后会详细介绍 `await_transform`）。

为了更具体地描述，我将**Awaitable**类型分为2类:

+ **Normally Awaitable**: 在相应的 `Promise`类型 __没有__ `await_transfrom`函数的协程上下文中, 可以用做 `co_await`的参数的类型
+ **Contextually Awaitable**: 如果协程的 `Promise`类型 __拥有__ `await_transform`函数, 则其所有 `await_transform`函数的返回值类型都是 **Contextually Awaitable**

(对于这些名称，我欢迎更好的建议…)

可以看出 **Normally Awaitable** 和 **Contextually Awaitable** 具体指代哪些类型, 并不是固定的。它们由当前协程的**Promise**的 `await_transform`的定义所决定。

具体来说, **Awaitable** 类型对象可以通过如下逻辑得到:

```c++
template<typename P, typename T>
decltype(auto) get_awaitable(P& promise, T&& expr)
{
  if constexpr (has_any_await_transform_member_v<P>) // 处理 contextually awaitable
    return promise.await_transform(static_cast<T&&>(expr));
  else // 处理 normally awaitable
    return static_cast<T&&>(expr);
}
```

_注意上方 `get_awaitable()`中的 `has_any_await_transform_member_v<P>`, 这代表只要当前协程的 **Promise** 类型拥有 `await_transform`成员函数, 就会进入这个分支。而如果此 **Promise** 类型的所有 `await_transform`函数都不能接受 `expr`参数, 编译就会报错_

如果 Promise 类型 `P` 有一个名为 `await_transform` 的成员，那么首先将 `<expr>` 传递给 `promise.await_transform(<expr>)` 调用，以获取 **Awaitable** 值 `awaitable`。

否则，如果 Promise 类型没有 `await_transform` 成员，那么直接使用评估 `<expr>` 的结果作为 **Awaitable** 对象 `awaitable`。

## **Awaiter** 类型

**Awaiter** 类型也是一组不特定类型的总称。所有的 **Awaiter** 类型都拥有3个特殊的函数:

+ `awaiter_ready`
+ `await_suspend`
+ `await_resume`

请注意，我在这里无耻地“借用”了C# `async` 关键字的机制中的术语“Awaiter”，它是通过一个 `GetAwaiter()`方法实现的，该方法返回一个接口与C++中的**Awaiter**概念非常相似的对象。有关C# awaiter的更多详细信息，请参阅[此文章](https://weblogs.asp.net/dixin/understanding-c-sharp-async-await-2-awaitable-awaiter-pattern)。

**Awaiter** 类型对象是由上文的 **Awaitable** 对象转换得到的, 具体的逻辑可以参考下方代码:

```c++
template<typename Awaitable>
decltype(auto) get_awaiter(Awaitable&& awaitable)
{
  if constexpr (has_member_operator_co_await_v<Awaitable>)
    return static_cast<Awaitable&&>(awaitable).operator co_await();
  else if constexpr (has_non_member_operator_co_await_v<Awaitable&&>)
    return operator co_await(static_cast<Awaitable&&>(awaitable));
  else
    return static_cast<Awaitable&&>(awaitable);
}
```

这里和获取**Awaitable**时不一样, 如果**Awaitable**类型没有相应的 `operator co_await()`重载, 会进入下方的 else 逻辑, 直接把 `awaitable` 当成 `awaiter`, 而非报错。当然, 如果此时 `awaiter`没有上方所述的3个特殊函数, 也会编译报错, 也就是说一个类型可能既是**Awaitable**类型，也是**Awaiter**类型。

### `co_await <expr>`的具体工作

`co_await <expr>` 的语义可以（大致）翻译如下：

```c++
{
  // 当前处于coroutine context (也就是 coroutine function/lambda)中

  auto&& value = <expr>;
  auto&& awaitable = get_awaitable(promise, static_cast<decltype(value)>(value));
  auto&& awaiter = get_awaiter(static_cast<decltype(awaitable)>(awaitable));
  if (!awaiter.await_ready())
  {
    using handle_t = std::experimental::coroutine_handle<P>;

    using await_suspend_result_t =
      decltype(awaiter.await_suspend(handle_t::from_promise(p)));

    // 在这里保存当前coroutine的状态, 包括保存coroutine frame和设置 coroutine 恢复时需要执行
    // 的指令的地址(也就是下方的<resume-point>的位置)。
    // 注意当这里执行完成后, 协程就已经挂起了
    <suspend-coroutine>;

    // 无返回值的await_suspend()一定会挂起当前coroutine
    // bool 返回值版本的 await_suspend() 则只会在返回值为true时挂起。
    // 因此可以使用bool版本的await_suspend()来处理 "等待的条件在事实挂起前就已满足"的情况。
    //
    // 如果 await_suspend()抛异常, 则会直接退回到上级函数, 后续的 await_resume() 将不会执行。
    // 当前coroutine在<return-to-caller-or-resumer>处挂起, 此时会返回到当前coroutine的上级函数。

    if constexpr (std::is_void_v<await_suspend_result_t>)
    {
      awaiter.await_suspend(handle_t::from_promise(p));
      <return-to-caller-or-resumer>;
    }
    else
    {
      static_assert(
         std::is_same_v<await_suspend_result_t, bool>,
         "await_suspend() must return 'void' or 'bool'.");

      if (awaiter.await_suspend(handle_t::from_promise(p)))
      {
        <return-to-caller-or-resumer>;
      }
    }

    // 如上方所属, 当前coroutine的恢复点在这里。
    <resume-point>;
  }

  // 根据上方的代码可以看出, 事实上 co_await <expr> 在awaiter.await_ready()返回true时不会挂起。
  // 但不论是否挂起, 只要 await_suspend() 不抛异常, 下方的 await_resume()就一定会执行。
  // await_resume()的返回值就是本次 co_await <expr> 这个表达式整体的值。
  // 如果await_resume()抛异常, 则也会向上方的 await_suspend()一样, 被直接抛给当前coroutine。
  return awaiter.await_resume();
}
}
```

`await_suspend()` 的无返回值版本在调用 `await_suspend()` 后会无条件地将执行权转移回协程的调用者/恢复者，而 `bool` 返回值版本允许等待器对象有条件地立即恢复协程而不返回给调用者/恢复者。

`await_suspend()` 的 `bool` 返回值版本在等待器可能启动一个有时可以同步完成的异步操作时非常有用。在同步完成的情况下，`await_suspend()` 方法可以返回 `false`，表示协程应立即恢复并继续执行。

在 `<suspend-coroutine>` 点，编译器会生成一些代码来保存协程的当前状态以为未来的恢复做准备。这包括存储 `<resume-point>` 的位置，以及将当前寄存器中的任何值溢出到协程帧内存中。

当前协程在 `<suspend-coroutine>` 操作完成后被认为是挂起的。你可以在 `await_suspend()` 调用中观察到挂起的协程。一旦协程被挂起，它就可以被恢复或销毁。

为协程规划在将来恢复(或销毁)的职责落在 `await_suspend()`中。 注意，从 `await_suspend()` 返回 `false` 等同于将协程安排为立即在当前线程上恢复。

`await_ready()` 方法的目的是允许您在已知操作将同步完成且无需挂起的情况下避免 `<suspend-coroutine>` 操作的开销。

在 `<return-to-caller-or-resumer>` 点，执行权转移到调用者或恢复者，弹出本地栈帧但保持协程帧的存活。

当挂起的协程最终被恢复时(如果没有在挂起期间将其销毁的话)，此协程将在 `<resume-point>` 处，也就是在调用 `await_resume()` 方法之前，恢复执行。

`await_resume()` 方法调用的返回值成为 `co_await` 表达式的结果。`await_resume()` 方法也可以抛出异常，在这种情况下，异常会从 `co_await` 表达式中传播出去。

请注意，如果异常从 `await_suspend()` 调用中传播出去，则协程会自动恢复，并且异常会从 `co_await` 表达式中传播出去，而不调用 `await_resume()`。

## 协程句柄

你可能已经注意到在 `co_await` 表达式的 `await_suspend()` 调用中使用了 `coroutine_handle<P>` 类型。

这个类型表示对协程帧的非拥有句柄，可以用于恢复协程的执行或销毁协程帧。它还可以用于访问协程的 promise 对象。

`coroutine_handle` 类型具有以下（大致）接口：

```c++
namespace std::experimental
{
  template<typename Promise>
  struct coroutine_handle;

  template<>
  struct coroutine_handle<void>
  {
    bool done() const;

    void resume();
    void destroy();

    void* address() const;
    static coroutine_handle from_address(void* address);
  };

  template<typename Promise>
  struct coroutine_handle : coroutine_handle<void>
  {
    Promise& promise() const;
    static coroutine_handle from_promise(Promise& promise);

    static coroutine_handle from_address(void* address);
  };
}
```

在实现**Awaitable**类型时，你将会在 `coroutine_handle`上使用的关键方法是 `.resume()`，当操作完成并且你想要恢复等待的协程的执行时，应该调用该方法。调用 `.resume()`会重新激活被挂起的协程, 使其在 `<resume-point>`处恢复执行。对 `.resume()`的调用会在协程下次到达 `<return-to-caller-or-resumer>`点时返回。

`.destroy()`方法会销毁协程帧，调用任何在作用域内的变量的析构函数，并释放协程帧使用的内存。通常情况下，你不需要（实际上应该避免）调用 `.destroy()`，除非你是一个实现协程promise类型的库编写者。通常，协程帧的所有权属于对协程函数进行**调用**操作时(见 Coroutine Theory 文), 所返回的某个对象。这个对象可能是一个RAII类型。因此，如果没有与RAII对象的协作，调用 `.destroy()`可能会导致double-free错误。

`.promise()`方法返回对协程的promise对象的引用。然而，像 `.destroy()`一样，它通常只在你编写协程promise类型时有用。你应该将协程的promise对象视为协程的内部实现细节。对于大多数**Normally Awaitable**类型，你应该在 `await_suspend()`方法的参数类型中使用 `coroutine_handle<void>`，而不是 `coroutine_handle<Promise>`。

`coroutine_handle<P>::from_promise(P& promise)`函数允许从对协程的promise对象的引用重建协程句柄。请注意，你必须确保类型 `P`与用于协程帧的具体promise类型完全匹配；当具体promise类型为 `Derived`时，尝试构造 `coroutine_handle<Base>`可能会导致未定义行为。

`.address()` / `from_address()`函数允许将协程句柄转换为/从 `void*`指针转换。这主要用于允许将其作为“上下文”参数传递给现有的C风格API，因此在某些情况下，你可能会发现在实现**Awaitable**类型时它很有用。然而，在大多数情况下，我发现有必要通过这个“上下文”参数将额外的信息传递给回调函数，所以我通常会将 `coroutine_handle`存储在一个结构体中，并在“上下文”参数中传递指向该结构体的指针，而不是使用 `.address()`返回值。

## 无需同步的异步代码

`co_await` 运算符的一个强大设计特性是能够在协程被挂起后但在返回给调用者/恢复者之前执行代码。

这使得 Awaiter 对象可以在协程挂起后启动一些异步操作(比如IO)，将挂起的协程的 `coroutine_handle` 传递给该操作，在操作完成时（可能在另一个线程上）安全地恢复协程，而无需额外的同步。

例如，在 `await_suspend()` 中当协程已经挂起时启动异步读取操作，意味着当操作完成时我们只需恢复协程，而无需任何线程同步来协调启动操作的线程和完成操作的线程。

```
Time     Thread 1                           Thread 2
  |      --------                           --------
  |      ....                               Call OS - Wait for I/O event
  |      Call await_ready()                    |
  |      <supend-point>                        |
  |      Call await_suspend(handle)            |
  |        Store handle in operation           |
  V        Start AsyncFileRead ---+            V
                                  +----->   <AsyncFileRead Completion Event>
                                            Load coroutine_handle from operation
                                            Call handle.resume()
                                              <resume-point>
                                              Call to await_resume()
                                              execution continues....
           Call to AsyncFileRead returns
         Call to await_suspend() returns
         <return-to-caller/resumer>
```

当利用这种方法时，需要非常小心的一点是，一旦启动了将协程句柄发布给其他线程的操作，那么在 `await_suspend()`返回之前，另一个线程可能会在另一个线程上恢复协程，并可能与 `await_suspend()`方法并行执行。

协程在恢复时首先要做的是调用 `await_resume()`来获取结果，然后通常会立即销毁**Awaiter**对象（即 `await_suspend()`调用的 `this`指针）。
然后，协程可能会在 `await_suspend()`返回之前完全运行完成，销毁协程和promise对象。

因此，在 `await_suspend()`方法中，一旦协程可能在另一个线程上并发恢复，就需要确保避免访问 `this`或协程的 `.promise()`对象，因为两者都可能已经被销毁。
一般来说，在操作启动并且协程被安排恢复之后，只有 `await_suspend()`内的局部变量是安全访问的。

### 与有栈协程的比较

我想先稍微比较一下协程 TS 的无栈协程与一些常见的有栈协程框架（如 Win32 fibers 或 `boost::context`）在协程被挂起后执行逻辑的能力。

在许多有栈协程框架中，协程的挂起操作与另一个协程的恢复操作被合并为一个“上下文切换”操作。
在这个“上下文切换”操作中，通常没有机会在挂起当前协程之后、在将执行转移到另一个协程之前执行逻辑。

这意味着，如果我们想要在有栈协程上实现类似的异步文件读取操作，我们必须在挂起协程之前启动操作。
因此，操作可能在协程被挂起并准备恢复之前在另一个线程上完成。这种操作在另一个线程上完成和协程挂起之间的潜在竞争需要某种线程同步来决定胜者。

可能有一些方法可以通过使用一个跳板上下文，在协程被挂起后代表初始上下文启动操作。然而，这将需要额外的基础设施和额外的上下文切换来使其工作，而且这可能引入的开销可能大于它试图避免的同步开销。

## 避免内存分配

异步操作通常需要存储一些每个操作状态的信息，以跟踪操作的进度。这些状态通常需要在操作完成之前持续存在，并且只有在操作完成后才能释放。

例如，调用异步的Win32 I/O函数需要分配并传递一个指向 `OVERLAPPED`结构的指针。调用者负责确保该指针在操作完成之前保持有效。

在传统的基于回调的API中，这些状态通常需要在堆上分配，以确保其具有适当的生命周期。如果要执行多个操作，则可能需要为每个操作分配和释放这些状态。如果性能是一个问题，可以使用自定义分配器从池中分配这些状态对象。

然而，当我们使用协程时，可以通过利用协程帧中的局部变量在协程挂起时保持其有效性，避免为操作状态分配堆内存。

通过将每个操作状态放置在**Awaiter**对象中，我们可以有效地从协程帧中“借用”内存，用于存储 `co_await`表达式的持续时间内的每个操作状态。一旦操作完成，协程将恢复执行，并销毁**Awaiter**对象，释放协程帧中的内存，以供其他局部变量使用。

最终，协程帧可能仍然在堆上分配。然而，一旦分配了协程帧，就可以使用该协程帧来执行许多异步操作，只需要进行一次堆分配。

如果仔细思考，协程帧就像一个高性能的区域内存分配器。编译器在编译时计算出所有局部变量所需的总区域大小，然后能够根据需要将该内存分配给局部变量，而没有任何开销！试着用自定义分配器来超越这一点吧）


*下方是一个使用示例, 就不翻译了*

## An example: Implementing a simple thread-synchronisation primitive

Now that we've covered a lot of the mechanics of the `co_await` operator, I want to show
how to put some of this knowledge into practice by implementing a basic awaitable
synchronisation primitive: An asynchronous manual-reset event.

The basic requirements of this event is that it needs to be **Awaitable** by multiple
concurrently executing coroutines and when awaited needs to suspend the awaiting
coroutine until some thread calls the `.set()` method, at which point any awaiting
coroutines are resumed. If some thread has already called `.set()` then the coroutine
should continue without suspending.

Ideally we'd also like to make it `noexcept`, require no heap allocations and have a
lock-free implementation.

**Edit 2017/11/23: Added example usage for `async_manual_reset_event`**

Example usage should look something like this:

```c++
T value;
async_manual_reset_event event;

// A single call to produce a value
void producer()
{
  value = some_long_running_computation();

  // Publish the value by setting the event.
  event.set();
}

// Supports multiple concurrent consumers
task<> consumer()
{
  // Wait until the event is signalled by call to event.set()
  // in the producer() function.
  co_await event;

  // Now it's safe to consume 'value'
  // This is guaranteed to 'happen after' assignment to 'value'
  std::cout << value << std::endl;
}
```

Let's first think about the possible states this event can be in: 'not set' and 'set'.

When it's in the 'not set' state there is a (possibly empty) list of waiting coroutines
that are waiting for it to become 'set'.

When it's in the 'set' state there won't be any waiting coroutines as coroutines that
`co_await` the event in this state can continue without suspending.

This state can actually be represented in a single `std::atomic<void*>`.

- Reserve a special pointer value for the 'set' state.
  In this case we'll use the `this` pointer of the event since we know that can't be the same address as any of the list items.
- Otherwise the event is in the 'not set' state and the value is a pointer to the head of a singly linked-list of awaiting coroutine structures.

We can avoid extra calls to allocate nodes for the linked-list on the heap by storing the
nodes within an 'awaiter' object that is placed within the coroutine frame.

So let's start with a class interface that looks something like this:

```c++
class async_manual_reset_event
{
public:

  async_manual_reset_event(bool initiallySet = false) noexcept;

  // No copying/moving
  async_manual_reset_event(const async_manual_reset_event&) = delete;
  async_manual_reset_event(async_manual_reset_event&&) = delete;
  async_manual_reset_event& operator=(const async_manual_reset_event&) = delete;
  async_manual_reset_event& operator=(async_manual_reset_event&&) = delete;

  bool is_set() const noexcept;

  struct awaiter;
  awaiter operator co_await() const noexcept;

  void set() noexcept;
  void reset() noexcept;

private:

  friend struct awaiter;

  // - 'this' => set state
  // - otherwise => not set, head of linked list of awaiter*.
  mutable std::atomic<void*> m_state;

};
```

Here we have a fairly straight-forward and simple interface. The main thing to note
at this point is that it has an `operator co_await()` method that returns an, as yet,
undefined type, `awaiter`.

Let's define the `awaiter` type now.

### Defining the Awaiter

Firstly, it needs to know which `async_manual_reset_event` object it is going to be
awaiting, so it will need a reference to the event and a constructor to initialise it.

It also needs to act as a node in a linked-list of `awaiter` values so it will need
to hold a pointer to the next `awaiter` object in the list.

It also needs to store the `coroutine_handle` of the awaiting coroutine that is executing
the `co_await` expression so that the event can resume the coroutine when it becomes 'set'.
We don't care what the promise type of the coroutine is so we'll just use a
`coroutine_handle<>` (which is short-hand for `coroutine_handle<void>`).

Finally, it needs to implement the **Awaiter** interface, so it needs the three special
methods: `await_ready`, `await_suspend` and `await_resume`. We don't need to return a
value from the `co_await` expression so `await_resume` can return `void`.

Once we put all of that together, the basic class interface for `awaiter` looks like this:

```c++
struct async_manual_reset_event::awaiter
{
  awaiter(const async_manual_reset_event& event) noexcept
  : m_event(event)
  {}

  bool await_ready() const noexcept;
  bool await_suspend(std::experimental::coroutine_handle<> awaitingCoroutine) noexcept;
  void await_resume() noexcept {}

private:

  const async_manual_reset_event& m_event;
  std::experimental::coroutine_handle<> m_awaitingCoroutine;
  awaiter* m_next;
};
```

Now, when we `co_await` an event, we don't want the awaiting coroutine to suspend if
the event is already set. So we can define `await_ready()` to return `true` if the
event is already set.

```c++
bool async_manual_reset_event::awaiter::await_ready() const noexcept
{
  return m_event.is_set();
}
```

Next, let's look at the `await_suspend()` method. This is usually where most of the
magic happens in an awaitable type.

First it will need to stash the coroutine handle of the awaiting coroutine into the
`m_awaitingCoroutine` member so that the event can later call `.resume()` on it.

Then once we've done that we need to try and atomically enqueue the awaiter onto
the linked list of waiters. If we successfully enqueue it then we return `true`
to indicate that we don't want to resume the coroutine immediately, otherwise if
we find that the event has concurrently been changed to the 'set' state then we
return `false` to indicate that the coroutine should be resumed immediately.

```c++
bool async_manual_reset_event::awaiter::await_suspend(
  std::experimental::coroutine_handle<> awaitingCoroutine) noexcept
{
  // Special m_state value that indicates the event is in the 'set' state.
  const void* const setState = &m_event;

  // Remember the handle of the awaiting coroutine.
  m_awaitingCoroutine = awaitingCoroutine;

  // Try to atomically push this awaiter onto the front of the list.
  void* oldValue = m_event.m_state.load(std::memory_order_acquire);
  do
  {
    // Resume immediately if already in 'set' state.
    if (oldValue == setState) return false; 

    // Update linked list to point at current head.
    m_next = static_cast<awaiter*>(oldValue);

    // Finally, try to swap the old list head, inserting this awaiter
    // as the new list head.
  } while (!m_event.m_state.compare_exchange_weak(
             oldValue,
             this,
             std::memory_order_release,
             std::memory_order_acquire));

  // Successfully enqueued. Remain suspended.
  return true;
}
```

Note that we use 'acquire' memory order when loading the old state so that
if we read the special 'set' value then we have visibility of writes that
occurred prior to the call to 'set()'.

We require 'release' sematics if the compare-exchange succeeds so that a
subsequent call to 'set()' will see our writes to m_awaitingCoroutine and
prior writes to the coroutine state.

### Filling out the rest of the event class

Now that we have defined the `awaiter` type, let's go back and look at the
implementation of the `async_manual_reset_event` methods.

First, the constructor. It needs to initialise to either the 'not set' state
with the empty list of waiters (ie. `nullptr`) or initialise to the 'set' state
(ie. `this`).

```c++
async_manual_reset_event::async_manual_reset_event(
  bool initiallySet) noexcept
: m_state(initiallySet ? this : nullptr)
{}
```

Next, the `is_set()` method is pretty straight-forward - it's 'set' if it
has the special value `this`:

```c++
bool async_manual_reset_event::is_set() const noexcept
{
  return m_state.load(std::memory_order_acquire) == this;
}
```

Next, the `reset()` method. If it's in the 'set' state we want to transition back
to the empty-list 'not set' state, otherwise leave it as it is.

```c++
void async_manual_reset_event::reset() noexcept
{
  void* oldValue = this;
  m_state.compare_exchange_strong(oldValue, nullptr, std::memory_order_acquire);
}
```

With the `set()` method, we want to transition to the 'set' state by exchanging
the current state with the special 'set' value, `this`, and then examine what
the old value was. If there were any waiting coroutines then we want to resume
each of them sequentially in turn before returning.

```c++
void async_manual_reset_event::set() noexcept
{
  // Needs to be 'release' so that subsequent 'co_await' has
  // visibility of our prior writes.
  // Needs to be 'acquire' so that we have visibility of prior
  // writes by awaiting coroutines.
  void* oldValue = m_state.exchange(this, std::memory_order_acq_rel);
  if (oldValue != this)
  {
    // Wasn't already in 'set' state.
    // Treat old value as head of a linked-list of waiters
    // which we have now acquired and need to resume.
    auto* waiters = static_cast<awaiter*>(oldValue);
    while (waiters != nullptr)
    {
      // Read m_next before resuming the coroutine as resuming
      // the coroutine will likely destroy the awaiter object.
      auto* next = waiters->m_next;
      waiters->m_awaitingCoroutine.resume();
      waiters = next;
    }
  }
}
```

Finally, we need to implement the `operator co_await()` method.
This just needs to construct an `awaiter` object.

```c++
async_manual_reset_event::awaiter
async_manual_reset_event::operator co_await() const noexcept
{
  return awaiter{ *this };
}
```

And there we have it. An awaitable asynchronous manual-reset event that has
a lock-free, memory-allocation-free, `noexcept` implementation.

If you want to have a play with the code or check out what it compiles
down to under MSVC and Clang have a look at the [source on godbolt](https://godbolt.org/g/Ad47tH).

You can also find an implementation of this class available in the
[cppcoro](https://github.com/lewissbaker/cppcoro) library, along with a number
of other useful awaitable types such as `async_mutex` and `async_auto_reset_event`.

## Closing Off

This post has looked at how the `operator co_await` is implemented and defined
in terms of the **Awaitable** and **Awaiter** concepts.

It has also walked through how to implement an awaitable async thread-synchronisation
primitive that takes advantage of the fact that awaiter objects are allocated on the
coroutine frame to avoid additional heap allocations.

I hope this post has helped to demystify the new `co_await` operator for you.

In the next post I'll explore the **Promise** concept and how a coroutine-type author
can customise the behaviour of their coroutine.

## Thanks

I want to call out special thanks to Gor Nishanov for patiently and enthusiastically
answering my many questions on coroutines over the last couple of years.

And also to Eric Niebler for reviewing and providing feedback on an early draft of
this post.
