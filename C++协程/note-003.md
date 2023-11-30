# C++ Coroutines: Understanding the promise type

_翻译自: [ C++ Coroutines: Understanding the promise type |
 Asymmetric Transfer ](https://lewissbaker.github.io/2018/09/05/understanding-the-promise-type)_

  在本文中，我将介绍编译器将你编写的协程代码转换为编译代码的机制，以及如何通过定义自己的 promise 类型来自定义协程的行为。

  本文是关于 C++ 协程 TS（[N4736](http://wg21.link/N4736)）的系列文章中的第三篇。

  本系列文章的前几篇介绍了以下内容：
* [Coroutine Theory](https://lewissbaker.github.io/2017/09/25/coroutine-theory)
* [Understanding operator co_await](https://lewissbaker.github.io/2017/11/17/understanding-operator-co-await)

在本文中，我将介绍编译器将你编写的协程代码转换为编译代码的机制，以及如何通过定义自己的 **Promise** 类型来自定义协程的行为。

## 协程概念

协程 TS 引入了三个新的关键字：`co_await`、`co_yield` 和 `co_return`。当你在函数体中使用其中一个协程关键字时，编译器会将该函数编译为协程，而不是普通函数。

编译器对你编写的代码应用了一些合理的、机械的转换，将其转换为状态机，使其能够在函数内的特定点暂停执行，然后稍后恢复执行。

在上一篇文章中，我描述了协程 TS 引入的两个新接口之一：**Awaitable** 接口。协程 TS 引入的第二个接口对于这种代码转换非常重要，那就是 **Promise** 接口。

**Promise** 接口指定了自定义协程行为的方法。库编写者可以自定义协程被调用时的行为，协程返回时的行为（无论是通过正常方式还是通过未处理的异常），
以及协程内部的任何 `co_await` 或 `co_yield` 表达式的行为。

## Promise 对象

在协程执行过程中的特定位置会调用一些**Promise**对象所提供的特殊函数，通过**Promise** 对象实现这些函数，我们可以定义和控制协程的行为。

> 在我们继续之前，我希望你能摆脱对“promise”这个词的任何先入为主的观念。
> 虽然在某些情况下，协程的 promise 对象确实扮演了类似于 `std::promise`-`std::future`-pair 中的`std::promise` 角色，
> 但在其他情况下，这个类比有些牵强。更容易理解的方式是将协程的 promise 对象视为一个“协程状态控制器”对象 (它并不同于之前提到的coroutine_handle, 
> 因为每个协程帧只有一个 promise 对象)，它控制着协程的行为并可以用于跟踪其状态。

每次调用协程函数时，都会在协程帧内构造一个 promise 对象的实例。

编译器在协程执行过程中的关键点上生成对 promise 对象的某些方法的调用。

在下面的示例中，假设在特定协程调用的协程帧中创建的 promise 对象为 `promise`。

当你编写一个具有 `<body-statements>` 体的协程函数，该体包含协程关键字之一（`co_return`、`co_await`、`co_yield`），那么协程的体将被转换为类似以下内容的形式：

```c++
{
  co_await promise.initial_suspend();
  try
  {
    <body-statements>
  }
  catch (...)
  {
    promise.unhandled_exception();
  }
FinalSuspend:
  co_await promise.final_suspend();
}
```

当调用协程函数时，会在执行协程体源代码之前执行一些与常规函数略有不同的步骤。

以下是这些步骤的摘要（我将在下面详细介绍每个步骤）。

1. 使用 `operator new` 分配协程帧的内存（可选）。
2. 将任何函数参数复制到协程帧中。
3. 调用类型为 `P` 的 promise 对象的构造函数。
4. 调用 `promise.get_return_object()` 方法以获取在协程首次暂停时返回给调用者的结果。将结果保存为局部变量。
5. 调用 `promise.initial_suspend()` 方法并 `co_await` 结果。
6. 当 `co_await promise.initial_suspend()` 表达式恢复执行（立即执行或先挂起再执行），协程开始执行您编写的协程体语句。

当执行到 `co_return` 语句时，还会执行一些额外的步骤：
1. 调用 `promise.return_void()` 或 `promise.return_value(<expr>)`。
2. 按照创建的相反顺序销毁所有具有自动存储期的变量。
3. 调用 `promise.final_suspend()` 并 `co_await` 其结果。

如果执行权由于由于未处理的异常而离开 `<body-statements>` ，则：
1. 捕获异常并在 catch 块中调用 `promise.unhandled_exception()`。
2. 调用 `promise.final_suspend()` 并 `co_await` 结果。

一旦执行超出协程体，则协程帧将被销毁。销毁协程帧涉及以下几个步骤：
1. 调用 promise 对象的析构函数。
2. 调用函数参数副本的析构函数。
3. 调用 `operator delete` 释放协程帧使用的内存（可选）。
4. 将执行权转移回调用者/恢复者。

当执行首次到达 `co_await` 表达式中的 `<return-to-caller-or-resumer>` 点，或者如果协程在不触发 `<return-to-caller-or-resumer>` 点的情况下运行完成，
则协程将被暂停或销毁，并将之前从调用 `promise.get_return_object()` 返回的返回对象返回给协程的调用者。

### 分配协程帧

首先，编译器生成一个调用 `operator new` 来为协程帧分配内存。

如果 promise 类型 `P` 定义了自定义的 `operator new` 方法，则调用该方法，否则调用全局的 `operator new`。

这里有几个重要的事项需要注意：

传递给 `operator new` 的大小不是 `sizeof(P)`，而是整个协程帧的大小，由编译器根据参数的数量和大小、promise 对象的大小、局部变量的数量和大小以及其他编译器特定的用于管理协程状态的存储需求自动确定。

如果编译器能够确定协程帧的生命周期严格嵌套在调用者的生命周期内，并且编译器可以在调用点看到所需的协程帧大小，则编译器可以优化掉对 `operator new` 的调用。

在这些情况下，编译器可以在调用者的活动帧中为协程帧分配存储空间（可以是栈帧或协程帧部分）。

协程 TS 尚未指定任何能够确保分配被消除的情况，因此您编写代码时，仍然需要假设协程帧的分配可能失败并引发 `std::bad_alloc` 异常。这也意味着，除非您能够接受协程帧分配内存失败时调用 `std::terminate()` ，否则通常不应将协程函数声明为 `noexcept`。

然而，有一种备选方案可以替代异常来处理无法分配协程帧的情况。这在无法使用异常的环境中可能是必要的，例如嵌入式环境或高性能环境，其通常无法容忍异常的开销。

如果 promise 类型提供了静态的 `P::get_return_object_on_allocation_failure()` 成员函数，
那么编译器将生成对 `operator new(size_t, nothrow_t)` 重载的调用。
如果该调用返回 `nullptr`，则协程将立即调用 `P::get_return_object_on_allocation_failure()`，
并将结果返回给协程的调用者，而不是抛出异常。

#### 自定义协程帧内存分配

您的 promise 类型可以定义 `operator new()` 的重载，如果编译器需要为使用您的 promise 类型的协程帧分配内存，则会调用该重载，而不是全局范围的 `operator new`。

例如：
```c++
struct my_promise_type
{
  void* operator new(std::size_t size)
  {
    void* ptr = my_custom_allocate(size);
    if (!ptr) throw std::bad_alloc{};
    return ptr;
  }

  void operator delete(void* ptr, std::size_t size)
  {
    my_custom_free(ptr, size);
  }

  ...
};
```


你可能会问，自定义分配器怎么办呢？


您还可以提供一个重载的 `P::operator new()`，它以左值引用的形式将协程函数的参数作为额外的参数，这种重载的`P::operator new()`的优先级高于其他的`P::operator new()`，只要协程函数的参数类型匹配, 就会被调用。 
这可以用于将 `operator new` 链接到在作为参数传递给协程函数的分配器上调用 `allocate()` 方法。

您需要额外做一些工作，将分配器的副本存储在分配的内存中，以便在相应的 `operator delete` 调用中引用它，因为参数不会传递给相应的 `operator delete` 调用。这是因为参数存储在协程帧中，所以在调用 `operator delete` 时，它们已经被销毁。

例如，您可以实现 `operator new`，使其在协程帧之后分配额外的空间，并使用该空间存储分配器的副本，该副本可用于释放协程帧内存。

例如:
```c++
template<typename ALLOCATOR>
struct my_promise_type
{
  template<typename... ARGS>
  void* operator new(std::size_t sz, std::allocator_arg_t, ALLOCATOR& allocator, ARGS&... args)
  {
    // Round up sz to next multiple of ALLOCATOR alignment
    std::size_t allocatorOffset =
      (sz + alignof(ALLOCATOR) - 1u) & ~(alignof(ALLOCATOR) - 1u);

    // Call onto allocator to allocate space for coroutine frame.
    void* ptr = allocator.allocate(allocatorOffset + sizeof(ALLOCATOR));

    // Take a copy of the allocator (assuming noexcept copy constructor here)
    new (((char*)ptr) + allocatorOffset) ALLOCATOR(allocator);

    return ptr;
  }

  void operator delete(void* ptr, std::size_t sz)
  {
    std::size_t allocatorOffset =
      (sz + alignof(ALLOCATOR) - 1u) & ~(alignof(ALLOCATOR) - 1u);

    ALLOCATOR& allocator = *reinterpret_cast<ALLOCATOR*>(
      ((char*)ptr) + allocatorOffset);

    // Move allocator to local variable first so it isn't freeing its
    // own memory from underneath itself.
    // Assuming allocator move-constructor is noexcept here.
    ALLOCATOR allocatorCopy = std::move(allocator);

    // But don't forget to destruct allocator object in coroutine frame
    allocator.~ALLOCATOR();

    // Finally, free the memory using the allocator.
    allocatorCopy.deallocate(ptr, allocatorOffset + sizeof(ALLOCATOR));
  }
}
```

要将自定义的 `my_promise_type` 与将 `std::allocator_arg` 作为第一个参数传递的协程关联起来，您需要对 `coroutine_traits` 类进行特化（有关更多详细信息，请参阅下面关于 `coroutine_traits` 的部分）。

例如:
``` c++
namespace std::experimental
{
  template<typename ALLOCATOR, typename... ARGS>
  struct coroutine_traits<my_return_type, std::allocator_arg_t, ALLOCATOR, ARGS...>
  {
    using promise_type = my_promise_type<ALLOCATOR>;
  };
}
```

请注意，即使您自定义了协程的内存分配策略，**编译器仍然可以优化调对您的内存分配器的调用**。

### 将参数复制到协程帧

协程需要将原始调用者传递给协程函数的任何参数复制到协程帧中，以便在协程暂停后仍然保持有效。

如果通过值传递参数给协程，则通过调用类型的移动构造函数将这些参数复制到协程帧中。

如果通过引用（左值或右值）将参数传递给协程，则只会将引用复制到协程帧中，而不是引用指向的值。

请注意，对于具有trivial析构函数的类型，如果在协程中的可达的`<return-to-caller-or-resumer>`点之后从未引用参数，则编译器可以省略参数的复制。

当将参数通过引用传递给协程时，涉及许多陷阱，因为您不能保证引用在协程的整个生命周期内保持有效。许多常见的技术，如完美转发和通用引用，在与协程一起使用时可能导致代码具有未定义的行为。如果您想了解更多细节，Toby Allsopp在这个主题上写了一篇[很棒的文章](https://toby-allsopp.github.io/2017/04/22/coroutines-reference-params.html)。

如果任何参数的复制/移动构造函数抛出异常，则已经构造的参数将被析构，协程帧将被释放，并且异常将传播回调用者。

### 构造 promise 对象

一旦所有参数都被复制到协程帧中，协程就会构造 promise 对象。

之所以在构造 promise 对象之前复制参数，是为了让 promise 对象在其构造函数中能够访问到已复制的参数。

首先，编译器会检查是否存在一个重载的 promise 构造函数，可以接受每个复制参数的左值引用。如果编译器找到这样的重载，那么编译器会生成对该构造函数重载的调用。如果找不到这样的重载，那么编译器会回退到生成对 promise 类型的默认构造函数的调用。

请注意，promise 构造函数能够“窥视”参数的能力是 Coroutines TS 的一个相对较新的变化，在 2018 年 Jacksonville 会议上采纳了 [N4723](http://wg21.link/N4723)。有关提案，请参阅 [P0914R1](http://wg21.link/P0914R1)。因此，某些较旧版本的 Clang 或 MSVC 可能不支持此功能。

如果 promise 构造函数抛出异常，则在异常传播到调用者之前，在stack unwinding期间会析构参数副本并释放协程帧。

### 获取返回对象

协程首先要做的事情是通过调用 `promise.get_return_object()` 来获取 `return-object`。

`return-object` 是在协程首次暂停或在协程运行完成后，当执行返回到调用者时返回给调用者的值。

您可以将控制流大致想象为以下方式：
```c++
// Pretend there's a compiler-generated structure called 'coroutine_frame'
// that holds all of the state needed for the coroutine. It's constructor
// takes a copy of parameters and default-constructs a promise object.
struct coroutine_frame { ... };

T some_coroutine(P param)
{
  auto* f = new coroutine_frame(std::forward<P>(param));

  auto returnObject = f->promise.get_return_object();

  // Start execution of the coroutine body by resuming it.
  // This call will return when the coroutine gets to the first
  // suspend-point or when the coroutine runs to completion.
  coroutine_handle<decltype(f->promise)>::from_promise(f->promise).resume();

  // Then the return object is returned to the caller.
  return returnObject;
}
```

请注意，我们需要在开始执行协程体之前获取返回对象，因为协程帧（以及promise对象）可能会在调用`coroutine_handle::resume()`返回之前被销毁，销毁流程可能在当前线程或在其他线程上，因此在开始执行协程体后调用`get_return_object()`将不安全。

### 初始挂起点

一旦协程帧被初始化并获取到返回对象，协程执行的下一步就是执行语句 `co_await promise.initial_suspend();`。

这允许 `promise_type` 的作者控制协程在执行源代码中出现的协程体之前是否挂起，或者立即开始执行协程体。

如果协程在初始挂起点挂起，那么可以通过在协程的 `coroutine_handle` 上调用 `resume()` 或 `destroy()` 来在任意时间恢复或销毁它。

`co_await promise.initial_suspend()` 表达式的结果被丢弃，因此实现通常应该从等待器的 `await_resume()` 方法返回 `void`。

需要注意的是，此语句位于 `try`/`catch` 块之外，该块用于保护协程的其余部分（如果你忘记了协程体的定义，请看前文）。这意味着在达到 `<return-to-caller-or-resumer>` 之前，从 `co_await promise.initial_suspend()` 评估中抛出的任何异常都将在销毁协程帧和返回对象后抛回给协程的调用者。

如果你的 `return-object` 具有 RAII 语义，在销毁时会销毁协程帧，请注意这一点。如果是这种情况，你需要确保 `co_await promise.initial_suspend()` 是 `noexcept` 的，以避免协程帧的双重释放。

> 注意，有一个提案正在调整语义，使得 `co_await promise.initial_suspend()` 表达式的全部或部分位于协程体的 try/catch 块中，因此在协程最终确定之前，这里的确切语义可能会发生变化。

对于许多类型的协程，`initial_suspend()` 方法要么返回 `std::experimental::suspend_always`（如果操作是惰性启动(lazily started)的），要么返回 `std::experimental::suspend_never`（如果操作是急切启动(eagerly started)的），这两者都是 `noexcept` 的可等待对象，因此通常不会出现问题。

### 返回给调用者

当协程函数达到其第一个`<return-to-caller-or-resumer>`（或者如果没有达到这样的点，则当协程的执行完成时），`get_return_object()`调用返回的`return-object`将返回给协程的调用者。

请注意，`return-object`的类型不需要与协程函数的返回类型相同。如果需要，将执行从`return-object`到协程的返回类型的隐式转换。

> 请注意，Clang的协程实现（截至5.0版本）推迟执行此转换，直到从协程调用中返回`return-object`，而MSVC的实现（截至2017年更新3）在调用`get_return_object()`后立即执行转换。尽管Coroutines TS对预期行为没有明确规定，但我相信MSVC计划改变其实现，使其更像Clang的行为，因为这样可以实现一些[有趣的用例 (即用 `co_await`实现类似于rust中的`?`的用法)](https://github.com/toby-allsopp/coroutine_monad)。

### 使用 `co_return` 从协程中返回

当协程达到 `co_return` 语句时，它会被转换为调用 `promise.return_void()` 或 `promise.return_value(<expr>)`，然后跳转到 `FinalSuspend` 标签。

转换规则如下：
* `co_return;`  
  -> `promise.return_void();`
* `co_return <expr>;`  
  -> `<expr>; promise.return_void();` 如果 `<expr>` 的类型为 `void`  
  -> `promise.return_value(<expr>);` 如果 `<expr>` 的类型不是 `void`

随后的 `goto FinalSuspend;` 会导致所有具有自动存储期的局部变量按照构造的逆序进行析构，然后评估 `co_await promise.final_suspend();`。

请注意，如果协程在没有 `co_return` 语句的情况下执行到函数体的末尾，那也相当于在函数体末尾有一个 `co_return;`。在这种情况下，如果 `promise_type` 没有 `return_void()` 方法，则行为是未定义的。

如果 `<expr>` 的评估或调用 `promise.return_void()` 或 `promise.return_value()` 抛出异常，则异常仍会传播到 `promise.unhandled_exception()`（见下文）。

### 处理从协程体传播出的异常

如果异常从协程体传播出来，则会捕获该异常，并在`catch`块内调用`promise.unhandled_exception()`方法。

该方法的实现通常会调用`std::current_exception()`来捕获异常的副本，并将其存储起来以便在不同的上下文中重新抛出。

或者，实现也可以立即通过执行`throw;`语句来重新抛出异常。例如，参考[folly::Optional](https://github.com/facebook/folly/blob/4af3040b4c2192818a413bad35f7a6cc5846ed0b/folly/Optional.h#L587)。然而，这样做会（很可能 - 见下文）导致协程帧立即被销毁，并且异常会传播到调用者/恢复者。这可能会对一些假设/要求`coroutine_handle::resume()`调用为`noexcept`的抽象造成问题，因此通常只有在你可以控制`resume()`的调用点时才使用这种方法。

请注意，当前的[协程TS](http://wg21.link/N4736)的措辞对于`unhandled_exception()`调用重新抛出异常的预期行为（或者说在try块之外的任何逻辑抛出异常的行为）有一些不清楚。

我目前对措辞的解释是，如果控制流离开协程体，无论是通过异常从`co_await promise.initial_suspend()`传播出来，还是通过`promise.unhandled_exception()`或`co_await promise.final_suspend()`，或者通过`co_await p.final_suspend()`同步完成协程运行，那么在执行返回给调用者/恢复者之前，协程帧将自动被销毁。然而，这种解释也存在问题。

希望未来的协程规范版本能够澄清这种情况。然而，在那之前，最好避免从`initial_suspend()`、`final_suspend()`或`unhandled_exception()`中抛出异常。敬请关注！

### 最终挂起点

一旦执行退出协程体的用户定义部分，并通过调用`return_void()`、`return_value()`或`unhandled_exception()`捕获结果，并且任何局部变量已被析构，协程有机会在将执行返回给调用者/恢复者之前执行一些额外的逻辑。

协程执行`co_await promise.final_suspend();`语句。

这允许协程执行一些逻辑，例如发布结果、标记完成或恢复继续。它还允许协程在执行到完成并销毁协程帧之前选择立即挂起。

请注意，在`final_suspend`点挂起的协程上调用`resume()`是未定义行为。在此挂起的协程上，您只能执行`destroy()`操作。

根据Gor Nishanov的说法，这种限制的理由是，这为编译器提供了几个优化机会，因为需要表示协程需要挂起的状态数量减少了，并且可能减少了所需的分支数量。

请注意，虽然允许协程在`final_suspend`点不挂起，**但建议您结构化您的协程，使其在可能的情况下挂起在`final_suspend`点**。
这是因为这样可以强制您从协程外部（通常是从某个RAII对象的析构函数）调用`.destroy()`来销毁协程，这使得编译器更容易确定协程帧的生命周期范围是否嵌套在调用者内部。这反过来使得编译器更有可能省略协程帧的内存分配。

### 编译器如何选择 promise 类型

现在让我们看看编译器如何确定给定协程使用的 promise 对象的类型。

promise 对象的类型是根据协程的签名来确定的，使用 `std::experimental::coroutine_traits` 类。

如果你有一个具有以下签名的协程函数：
```c++
task<float> foo(std::string x, bool flag);
```

编译器将通过将返回类型和参数类型作为模板参数传递给`coroutine_traits`来推断协程的promise类型。
```c++
typename coroutine_traits<task<float>, std::string, bool>::promise_type;
```

如果函数是非静态成员函数，则类类型作为第二个模板参数传递给`coroutine_traits`。请注意，如果您的方法对右值引用进行了重载，则第二个模板参数将是右值引用。

例如，如果您有以下方法：
```c++
task<void> my_class::method1(int x) const;
task<foo> my_class::method2() &&;
```

然后编译器将使用以下的 promise 类型：
```c++
// method1 promise type
typename coroutine_traits<task<void>, const my_class&, int>::promise_type;

// method2 promise type
typename coroutine_traits<task<foo>, my_class&&>::promise_type;
```

`coroutine_traits` 模板的默认定义通过查找返回类型上定义的嵌套 `promise_type` 类型来定义 `promise_type`。
例如，类似于以下方式（但使用了一些额外的 SFINAE ，以便在未定义 `RET::promise_type` 的情况下不定义 `promise_type`）。
```c++
namespace std::experimental
{
  template<typename RET, typename... ARGS>
  struct coroutine_traits<RET, ARGS...>
  {
    using promise_type = typename RET::promise_type;
  };
}
```

对于你可以控制的协程返回类型，你可以在你的类中定义一个嵌套的 `promise_type`，以便编译器将该类型作为协程返回你的类的 promise 对象的类型。

For example:
```c++
template<typename T>
struct task
{
  using promise_type = task_promise<T>;
  ...
};
```

然而，对于您无法控制的协程返回类型，您可以专门化`coroutine_traits`来定义要使用的promise类型，而无需修改类型。

例如，要为返回`std::optional<T>`的协程定义要使用的promise类型：
```c++
namespace std::experimental
{
  template<typename T, typename... ARGS>
  struct coroutine_traits<std::optional<T>, ARGS...>
  {
    using promise_type = optional_promise<T>;
  };
}
```

### 协程帧

当您调用协程函数时，将创建一个协程帧。为了恢复关联的协程或销毁协程帧，您需要一种方式来找到并引用该特定的协程帧。

协程 TS 提供的机制是 `coroutine_handle` 类型。

该类型的（简化的）接口如下：
```c++
namespace std::experimental
{
  template<typename Promise = void>
  struct coroutine_handle;

  // Type-erased coroutine handle. Can refer to any kind of coroutine.
  // Doesn't allow access to the promise object.
  template<>
  struct coroutine_handle<void>
  {
    // Constructs to the null handle.
    constexpr coroutine_handle();

    // Convert to/from a void* for passing into C-style interop functions.
    constexpr void* address() const noexcept;
    static constexpr coroutine_handle from_address(void* addr);

    // Query if the handle is non-null.
    constexpr explicit operator bool() const noexcept;

    // Query if the coroutine is suspended at the final_suspend point.
    // Undefined behaviour if coroutine is not currently suspended.
    bool done() const;

    // Resume/Destroy the suspended coroutine
    void resume();
    void destroy();
  };

  // Coroutine handle for coroutines with a known promise type.
  // Template argument must exactly match coroutine's promise type.
  template<typename Promise>
  struct coroutine_handle : coroutine_handle<>
  {
    using coroutine_handle<>::coroutine_handle;

    static constexpr coroutine_handle from_address(void* addr);

    // Access to the coroutine's promise object.
    Promise& promise() const;

    // You can reconstruct the coroutine handle from the promise object.
    static coroutine_handle from_promise(Promise& promise);
  };
}
```

您可以通过两种方式获取协程的`coroutine_handle`：
1. 在`co_await`表达式期间，它会传递给`await_suspend()`方法。
2. 如果您有协程的promise对象的引用，可以使用`coroutine_handle<Promise>::from_promise()`来重建其`coroutine_handle`。

在`co_await`表达式的`suspend-point`处，协程暂停后，等待的协程的`coroutine_handle`将传递给awaiter的`await_suspend()`方法。您可以将这个`coroutine_handle`视为以[continuation-passing style](https://en.wikipedia.org/wiki/Continuation-passing_style)调用中协程的继续。

请注意，`coroutine_handle`**不是**RAII对象。您必须手动调用`.destroy()`来销毁协程帧并释放其资源。可以将其视为用于管理内存的`void*`的等效物。这是出于性能原因：将其设置为RAII对象会增加协程的额外开销，例如需要进行引用计数。

通常应尽量使用提供协程的RAII语义的更高级别类型，例如由[cppcoro](https://github.com/lewissbaker/cppcoro)（无耻的广告）提供的类型，或编写自己的封装协程帧生命周期的更高级别类型。

### 自定义`co_await`的行为

Promise类型可以选择自定义协程体中每个`co_await`表达式的行为。

只需在promise类型上定义一个名为`await_transform()`的方法，编译器将会将协程体中的每个`co_await <expr>`转换为`co_await promise.await_transform(<expr>)`。

这有一些重要且强大的用途：

**它允许您启用通常不可等待的类型的等待。**

例如，对于返回类型为`std::optional<T>`的协程，promise类型可以提供一个`await_transform()`重载，接受一个`std::optional<U>`并返回一个可等待类型，该类型要么返回类型为`U`的值，要么在等待的值包含`nullopt`时暂停协程。

```c++
template<typename T>
class optional_promise
{
  ...

  template<typename U>
  auto await_transform(std::optional<U>& value)
  {
    class awaiter
    {
      std::optional<U>& value;
    public:
      explicit awaiter(std::optional<U>& x) noexcept : value(x) {}
      bool await_ready() noexcept { return value.has_value(); }
      void await_suspend(std::experimental::coroutine_handle<>) noexcept {}
      U& await_resume() noexcept { return *value; }
    };
    return awaiter{ value };
  }
};
```

**它允许您通过声明`await_transform`的重载为删除来禁止等待某些类型。**

例如，`std::generator<T>`返回类型的promise类型可以声明一个已删除的`await_transform()`模板成员函数，该函数接受任何类型。这基本上禁用了在协程中使用`co_await`。

```c++
template<typename T>
class generator_promise
{
  ...

  // Disable any use of co_await within this type of coroutine.
  template<typename U>
  std::experimental::suspend_never await_transform(U&&) = delete;

};
```

**它允许您适应和更改通常可等待值的行为**

例如，您可以定义一种协程类型，通过在关联的执行器中包装可等待对象来确保协程始终从每个`co_await`表达式中恢复（参见`cppcoro::resume_on()`）。

```c++
template<typename T, typename Executor>
class executor_task_promise
{
  Executor executor;

public:

  template<typename Awaitable>
  auto await_transform(Awaitable&& awaitable)
  {
    using cppcoro::resume_on;
    return resume_on(this->executor, std::forward<Awaitable>(awaitable));
  }
};
```

关于`await_transform()`的最后一点需要注意的是，如果promise类型定义了*任何*`await_transform()`成员函数，那么编译器将会将*所有*的`co_await`表达式转换为调用`promise.await_transform()`。这意味着，如果您只想为某些类型自定义`co_await`的行为，您可能还需要提供一个更加泛用的`await_transform()`重载作为fallback，只需将参数转发即可，否则除了少数能够通过重载匹配的类型之外, 都不能再被用作`co_await`的参数。

### 自定义`co_yield`的行为

通过promise类型，您可以自定义`co_yield`关键字的行为。

如果在协程中出现`co_yield`关键字，编译器将将表达式`co_yield <expr>`转换为表达式`co_await promise.yield_value(<expr>)`。因此，promise类型可以通过在promise对象上定义一个或多个`yield_value()`方法来自定义`co_yield`关键字的行为。

请注意，与`await_transform`不同，如果promise类型未定义`yield_value()`方法，则`co_yield`没有默认行为。因此，虽然promise类型需要通过声明已删除的`await_transform()`来明确禁止`co_await`，但是promise类型需要通过声明`yield_value()`来明确支持`co_yield`。

具有`yield_value()`方法的promise类型的典型示例是`generator<T>`类型：
```c++
template<typename T>
class generator_promise
{
  T* valuePtr;
public:
  ...

  std::experimental::suspend_always yield_value(T& value) noexcept
  {
    // Stash the address of the yielded value and then return an awaitable
    // that will cause the coroutine to suspend at the co_yield expression.
    // Execution will then return from the call to coroutine_handle<>::resume()
    // inside either generator<T>::begin() or generator<T>::iterator::operator++().
    valuePtr = std::addressof(value);
    return {};
  }
};
```

# 摘要

在本文中，我介绍了编译器在将函数编译为协程时应用的各个转换。

希望本文能帮助您了解如何通过定义自己的promise类型来自定义不同类型协程的行为。
协程机制中有很多组成部分，因此您可以以多种方式自定义它们的行为。

然而，编译器还执行了一个更重要的转换，即将协程体转换为状态机。
然而，本文已经太长了，所以我将在下一篇文章中解释这个转换。
敬请关注！
