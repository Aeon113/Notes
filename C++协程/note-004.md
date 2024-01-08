# C++ Coroutines: Understanding Symmetric Transfer

_翻译自: [C++ Coroutines: Understanding Symmetric Transfer | Asymmetric Transfer](https://lewissbaker.github.io/2020/05/11/understanding_symmetric_transfer)_

  一个小调整在2018年对协程的设计中加入了一项功能，称为“对称转换”，它允许您暂停一个协程并恢复另一个协程，而不会消耗任何额外栈空间。这个功能的添加消除了Coroutines TS的一个关键限制，使得异步协程类型的实现更简单、更高效，而不会牺牲任何用于防止栈溢出的安全性方面的需求。

Coroutines TS 提供了一种非常好的方式来编写异步代码，就像编写同步代码一样。您只需要在适当的位置添加 `co_await`，编译器会负责暂停协程，在暂停点保留状态，并在操作完成后恢复协程的执行。

然而，最初指定的协程 TS 存在一个令人讨厌的限制，如果不小心的话很容易导致堆栈溢出。如果要避免堆栈溢出，就必须在 `task<T>` 类型中引入额外的同步开销来安全地防止这种情况。

幸运的是，2018 年对协程的设计进行了调整，添加了一项名为“对称转换”的功能，允许您暂停一个协程并恢复另一个协程，而不会消耗任何额外的堆栈空间。这个功能的添加消除了协程 TS 的一个关键限制，使得异步协程类型的实现更加简单、高效，而不会牺牲任何用于防止堆栈溢出的安全性方面的需求。

在本文中，我将尝试解释堆栈溢出问题以及这个关键的“对称转换”功能是如何帮助我们解决这个问题的。

## 首先了解一下任务协程的工作原理

考虑以下协程：
```c++
task foo() {
  co_return;
}

task bar() {
  co_await foo();
}
```

假设我们有一个简单的`task`类型，当另一个协程等待它时 ，它会延迟执行其主体。
这个特定的`task`类型不支持返回值(_也就是说不能在协程函数里写`co_return <expr>;`, 只能`co_return;`或者使函数执行到最后一个大括号处自然结束_)。

让我们解析一下当`bar()`评估`co_await foo()`时发生了什么。

* `bar()`协程调用`foo()`函数。
  注意，从调用者的角度来看，协程只是一个普通的函数。
* `foo()`的调用执行了几个步骤：
  * 为协程帧分配存储空间（通常在堆上）
  * 将参数复制到协程帧中（在本例中，没有参数，所以这是一个空操作）。
  * 在协程帧中构造promise对象
  * 调用`promise.get_return_object()`获取`foo()`的返回值。
    这会产生一个`task`对象，该对象将被返回，并使用引用刚刚创建的协程帧的`std::coroutine_handle`进行初始化。
  * 在初始挂起点（即左大括号处）暂停协程的执行
  * 将`task`对象返回给`bar()`。
* 接下来，`bar()`协程对从`foo()`返回的`task`上的`co_await`表达式进行评估 (evaluate)。
  * `bar()`协程暂停，然后在返回的任务上调用`await_suspend()`方法，
    将`std::coroutine_handle`传递给它，这个`std::coroutine_handle`引用了本次`bar()`的协程帧。
  * `await_suspend()`方法然后将`bar()`的`std::coroutine_handle`存储在`foo()`的promise对象中，
    然后通过调用`foo()`的`std::coroutine_handle`上的`.resume()`方法恢复`foo()`协程的执行。
* `foo()`协程同步执行并运行到完成。
* `foo()`协程在最终挂起点（即右大括号处）暂停，然后通过在刚才在其promise对象中存储的`std::coroutine_handle`来恢复协程执行，即`bar()`的协程。
* `bar()`协程恢复执行并继续执行，最终到达包含`co_await`表达式的语句的末尾，此时调用从`foo()`返回的临时`task`对象的析构函数。
* `task`析构函数然后在`foo()`的协程句柄上调用`.destroy()`方法，这将销毁协程帧以及promise对象和任何参数的副本。

好的，对于一个简单的调用来说，这步骤似乎很多。

为了更深入地理解这一点，让我们看看当使用不支持对称转换的协程 TS 设计时，一个简单的 `task` 类的实现会是什么样子。

## `task` 实现的概要

该类的概要可能如下：
```c++
class task {
public:
  class promise_type { /* see below */ };

  task(task&& t) noexcept
  : coro_(std::exchange(t.coro_, {}))
  {}

  ~task() {
    if (coro_)
      coro_.destroy();
  }

  class awaiter { /* see below */ };

  awaiter operator co_await() && noexcept;

private:
  explicit task(std::coroutine_handle<promise_type> h) noexcept
  : coro_(h)
  {}

  std::coroutine_handle<promise_type> coro_;
};
```

一个 `task` 对象独占了在协程调用期间创建的对应协程帧的 `std::coroutine_handle`。
`task` 对象是一个 RAII 对象，确保在 `task` 对象超出作用域时调用 `.destroy()` 方法来销毁 `std::coroutine_handle`。

现在让我们实现`promise_type`。

## 实现 `task::promise_type`

从[上一篇文章](https://lewissbaker.github.io/2018/09/05/understanding-the-promise-type)中我们知道，`promise_type` 成员定义了在协程帧内创建的**Promise**对象的类型，并控制协程的行为。

首先，我们需要实现 `get_return_object()` 方法来构造在调用协程时返回的 `task` 对象。这个方法只需要使用新创建的协程帧的 `std::coroutine_handle` 来初始化任务。

我们可以使用 `std::coroutine_handle::from_promise()` 方法来从 promise 对象中制造一个这样的句柄。

```c++
class task::promise_type {
public:
  task get_return_object() noexcept {
    return task{std::coroutine_handle<promise_type>::from_promise(*this)};
  }
```

接下来，我们希望协程在左大括号处挂起，以便在等待返回的`task`时可以从此处恢复协程的执行。

延迟启动协程有几个好处：
1. 这意味着我们可以在启动协程之前附加继续的`std::coroutine_handle`。这意味着我们不需要使用线程同步来解决附加继续和协程完成执行之间的竞争问题。
2. 这意味着`task`析构函数可以无条件地销毁协程帧 - 我们不需要担心协程是否在另一个线程上执行，因为协程在我们等待它之前不会开始执行，并且在执行时，调用协程被挂起，因此不会尝试调用任务析构函数，直到协程完成执行。
  这使得编译器有更好的机会将协程帧的分配内联到调用者的帧中。
  有关堆分配省略优化（HALO）的更多信息，请参见[P0981R0](https://wg21.link/P0981R0)。
3. 它还提高了协程代码的异常安全性。如果您不立即`co_await`返回的`task`，而是做一些可能引发异常的其他操作，导致堆栈展开和`task`析构函数运行，那么我们可以安全地销毁协程，因为我们知道它还没有开始执行。我们不必在可能留下悬空引用、可能在析构函数中阻塞、终止程序或产生未定义行为之间做出困难的选择。
  这是我在我的[CppCon 2019关于结构化并发的演讲](https://www.youtube.com/watch?v=1Wy5sq3s2rg)中更详细地介绍的内容。

为了让协程在左大括号处初始挂起，我们只需要定义一个返回内置的`suspend_always`类型的`initial_suspend()`方法。

```c++
  std::suspend_always initial_suspend() noexcept {
    return {};
  }
```

接下来，我们需要定义 `return_void()` 方法，其将在执行 `co_return;` 或者协程执行到末尾时被自动调用。这个方法实际上不需要做任何事情，只是必须存在，以便编译器知道在这个协程类型中 `co_return;` 是有效的。

```c++
  void return_void() noexcept {}
```

我们还需要添加一个`unhandled_exception()`方法，用于处理协程体中逃逸的异常。对于本例，我们可以将任务协程体视为`noexcept`，并在发生异常时调用`std::terminate()`。

```c++
  void unhandled_exception() noexcept {
    std::terminate();
  }
```

最后，当协程执行到右大括号时，我们希望协程在最终挂起点挂起，然后恢复正在等待此协程完成的协程。

为了支持这一点，我们需要在 promise 中添加一个数据成员来保存继续执行的 `std::coroutine_handle`。我们还需要定义 `final_suspend()` 方法，该方法返回一个可等待对象，在当前协程在最终挂起点挂起后恢复这个正在等待当前协程的协程的执行。

延迟恢复等待已完成的协程的协程非常重要，直到被等待的协程挂起，因为继续执行可能会立即调用`task`析构函数，该析构函数将在协程帧上调用`.destroy()`方法。
只有在协程挂起时才能调用`.destroy()`方法，在当前协程挂起之前恢复继续执行将导致未定义行为。

编译器会在闭合大括号处插入代码来评估语句 `co_await promise.final_suspend();`。

需要注意的是，在调用 `final_suspend()` 方法时，协程还没有处于挂起状态。
在其返回的可等待对象的 `await_suspend()` 方法被调用后，协程才会被挂起。

```c++
  struct final_awaiter {
    bool await_ready() noexcept {
      return false;
    }

    void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
      // The coroutine is now suspended at the final-suspend point.
      // Lookup its continuation in the promise and resume it.
      h.promise().continuation.resume();
    }

    void await_resume() noexcept {}
  };

  final_awaiter final_suspend() noexcept {
    return {};
  }

  std::coroutine_handle<> continuation;
};
```

好的，这就是完整的`promise_type`。我们需要实现的最后一部分是`task::operator co_await()`。

## 实现 `task::operator co_await()`

你可能还记得[理解 operator co_await() 文章](https://lewissbaker.github.io/2017/11/17/understanding-operator-co-await)中提到的，当评估 `co_await` 表达式时，如果定义了 `operator co_await()`，编译器将生成对该方法的调用，然后返回的对象必须定义 `await_ready()`、`await_suspend()` 和 `await_resume()` 方法。

当协程等待一个 `task` 时，我们希望等待的协程(也就是当前协程)总是挂起，然后一旦它挂起，将等待的协程的句柄存储在即将恢复的协程(也就是新创建的协程)的 promise 中，并调用 `.resume()` 方法来启动执行任务的协程。

Thus the relatively straight forward code:
```c++
class task::awaiter {
public:
  bool await_ready() noexcept {
    return false;
  }

  void await_suspend(std::coroutine_handle<> continuation) noexcept {
    // Store the continuation in the task's promise so that the final_suspend()
    // knows to resume this coroutine when the task completes.
    coro_.promise().continuation = continuation;

    // Then we resume the task's coroutine, which is currently suspended
    // at the initial-suspend-point (ie. at the open curly brace).
    coro_.resume();
  }

  void await_resume() noexcept {}

private:
  explicit awaiter(std::coroutine_handle<task::promise_type> h) noexcept
  : coro_(h)
  {}

  std::coroutine_handle<task::promise_type> coro_;
};

task::awaiter task::operator co_await() && noexcept {
  return awaiter{coro_};
}
```

这就是一个功能完整的 `task` 类型所需的全部代码。

您可以在这里查看完整的代码集合：[https://godbolt.org/z/-Kw6Nf](https://godbolt.org/z/-Kw6Nf)

## 栈溢出问题

然而，当您在协程中编写循环并且在该循环体内`co_await`可能会同步完成的任务时，这种实现就会出现问题。

For example:
```c++
task completes_synchronously() {
  co_return;
}

task loop_synchronously(int count) {
  for (int i = 0; i < count; ++i) {
    co_await completes_synchronously();
  }
}
```

使用上述简单的`task`实现，`loop_synchronously()`函数在`count`为10、1000甚至100'000时可能会正常工作。但是，当这个值足够大时，该协程最终还是会崩溃。

例如，查看：[https://godbolt.org/z/gy5Q8q](https://godbolt.org/z/gy5Q8q)，当`count`为1'000'000时会崩溃。

导致崩溃的原因是栈溢出。

为了理解为什么这段代码会导致栈溢出，我们需要查看代码执行时发生了什么。特别是，栈帧发生了什么变化。

`loop_synchronously()`协程将在其他某个协程`co_await`其返回的`task`时开始执行时。这将暂停等待的协程，并调用`task::awaiter::await_suspend()`，该方法将在任务的`std::coroutine_handle`上调用`resume()`。

因此，当`loop_synchronously()`开始时，堆栈大致如下所示：
```
           Stack                                                   Heap
+------------------------------+  <-- top of stack   +--------------------------+
| loop_synchronously$resume    | active coroutine -> | loop_synchronously frame |
+------------------------------+                     | +----------------------+ |
| coroutine_handle::resume     |                     | | task::promise        | |
+------------------------------+                     | | - continuation --.   | |
| task::awaiter::await_suspend |                     | +------------------|---+ |
+------------------------------+                     | ...                |     |
| awaiting_coroutine$resume    |                     +--------------------|-----+
+------------------------------+                                          V
|  ....                        |                     +--------------------------+
+------------------------------+                     | awaiting_coroutine frame |
                                                     |                          |
                                                     +--------------------------+
```

> 注意：当编译协程函数时，编译器通常将其分为两部分：
> 1. "ramp function" 处理协程帧的构建、参数复制、promise 构造和生成返回值，
> 2. "coroutine body" 包含协程体中用户编写的逻辑。
>
> 我使用 `$resume` 后缀来指代协程的 "coroutine body" 部分。
>
> 后续的博文将详细介绍这种分割方式。

当 `loop_synchronously()` 等待从 `completes_synchronously()` 返回的 `task` 时，
当前协程(也就是 `loop_synchronously()`)被暂停，并调用 `task::awaiter::await_suspend()`。
`await_suspend()` 方法然后调用 `completes_synchronously()` 协程对应的协程句柄的 `.resume()`。

这将恢复 `completes_synchronously()` 协程，然后同步运行并在最终挂起点暂停。
然后它调用 `task::promise::final_awaiter::await_suspend()`，该方法调用 `loop_synchronously()` 对应的协程句柄的 `.resume()`。

所有这些的最终结果是，如果我们在 `loop_synchronously()` 协程恢复之后，在临时 `task` 在分号处被销毁之前查看程序的状态，堆栈/堆应该如下所示：
```
           Stack                                                   Heap
+-------------------------------+ <-- top of stack
| loop_synchronously$resume     | active coroutine -.
+-------------------------------+                   |
| coroutine_handle::resume      |            .------'
+-------------------------------+            |
| final_awaiter::await_suspend  |            |
+-------------------------------+            |  +--------------------------+ <-.
| completes_synchronously$resume|            |  | completes_synchronously  |   |
+-------------------------------+            |  | frame                    |   |
| coroutine_handle::resume      |            |  +--------------------------+   |
+-------------------------------+            '---.                             |
| task::awaiter::await_suspend  |                V                             |
+-------------------------------+ <-- prev top  +--------------------------+   |
| loop_synchronously$resume     |     of stack  | loop_synchronously frame |   |
+-------------------------------+               | +----------------------+ |   |
| coroutine_handle::resume      |               | | task::promise        | |   |
+-------------------------------+               | | - continuation --.   | |   |
| task::awaiter::await_suspend  |               | +------------------|---+ |   |
+-------------------------------+               | - task temporary --|---------'
| awaiting_coroutine$resume     |               +--------------------|-----+
+-------------------------------+                                    V
|  ....                         |               +--------------------------+
+-------------------------------+               | awaiting_coroutine frame |
                                                |                          |
                                                +--------------------------+
```

然后，接下来要做的是调用`task`的析构函数，它将销毁`completes_synchronously()`的帧。然后它会增加`count`变量并再次循环，创建一个新的`completes_synchronously()`帧并恢复它。

实际上，这里发生的是`loop_synchronously()`和`completes_synchronously()`最终会互相递归调用。每次发生这种情况时，我们都会消耗更多的堆栈空间，直到最终，在足够多次的迭代之后，堆栈溢出并进入未定义行为的领域，通常导致程序立即崩溃。

使用这种方式构建协程的循环使得编写执行无限递归的函数变得非常容易，而看起来又不像是在进行任何递归。

那么，在最初的Coroutines TS设计下，解决方案会是什么样子呢？

## 协程 TS 的解决方案

好的，那么我们该如何解决这种无限递归的问题呢？

在上述实现中，我们使用了返回 `void` 的 `await_suspend()` 变体。
在协程 TS 中，还有一种返回 `bool` 的 `await_suspend()` 变体 -
如果返回 `true`，则协程被暂停，执行返回到 `resume()` 的调用者；
否则，如果返回 `false`，则协程立即恢复，但这次不会消耗额外的堆栈空间。

因此，为了避免无限的相互递归，我们希望利用返回 `bool` 的 `await_suspend()` 变体，
通过在 `task::awaiter::await_suspend()` 方法中返回 `false` 来恢复当前协程，
而不是使用 `std::coroutine_handle::resume()` 递归地恢复协程。

为了实现这个通用解决方案，有两个部分需要考虑。

1. 在 `task::awaiter::await_suspend()` 方法内，可以通过调用 `.resume()` 来开始执行协程。然后，在调用 `.resume()` 返回后，检查协程是否已经执行完成。如果协程已经执行完成，则可以返回 `false`，表示等待的协程应立即恢复；或者返回 `true`，表示执行应返回给 `std::coroutine_handle::resume()` 的调用者。

2. 在 `task::promise_type::final_awaiter::await_suspend()` 方法内（当协程执行完成时运行），我们需要检查等待的协程是否已经（或将要）从 `task::awaiter::await_suspend()` 返回 `true`，如果是，则通过调用 `.resume()` 来恢复它。否则，我们需要避免恢复协程，并通知 `task::awaiter::await_suspend()` 需要返回 `false`。

然而，存在一个额外的可能，即协程可能在当前线程上开始执行，然后暂停，并在调用 `.resume()` 返回之前在不同的线程上运行并执行完成。
因此，我们需要能够解决上述第1部分和第2部分同时发生的潜在竞争。

我们需要使用一个 `std::atomic` 值来决定竞争的胜者。


现在来看代码。我们可以进行以下修改：
```c++
class task::promise_type {
  ...

  std::coroutine_handle<> continuation;
  std::atomic<bool> ready = false;
};

bool task::awaiter::await_suspend(
    std::coroutine_handle<> continuation) noexcept {
  promise_type& promise = coro_.promise();
  promise.continuation = continuation;
  coro_.resume();
  return !promise.ready.exchange(true, std::memory_order_acq_rel);
}

void task::promise_type::final_awaiter::await_suspend(
    std::coroutine_handle<promise_type> h) noexcept {
  promise_type& promise = h.promise();
  if (promise.ready.exchange(true, std::memory_order_acq_rel)) {
    // The coroutine did not complete synchronously, resume it here.
    promise.continuation.resume();
  }
}
```

在Compiler Explorer上查看更新后的示例代码：[https://godbolt.org/z/7fm8Za](https://godbolt.org/z/7fm8Za)
注意当执行`count == 1'000'000`的情况时，不再崩溃。

这实际上就是`cppcoro::task<T>`的实现采用的方法，用于避免无限递归问题（对于某些平台仍然有效），并且效果还不错。

喔耶！问题解决了，对吧？可以发布了！对吧...？

## 问题

虽然上述解决方案解决了递归问题，但它有几个缺点。

首先，它引入了对`std::atomic`操作的需求，这可能非常昂贵。
在挂起等待的协程时，调用者需要进行原子交换操作，而在协程运行完成时，被调用者也需要进行原子交换操作。
如果您的应用程序只在单个线程上执行，那么您将为同步线程支付原子操作的成本，尽管实际上并不需要。

其次，它引入了额外的分支。调用者需要决定是挂起协程还是立即恢复协程，而被调用者需要决定是恢复继续执行还是挂起。

请注意，这个额外分支的成本，甚至可能是原子操作的成本，通常会被协程中存在的业务逻辑的成本所掩盖。
然而，协程被宣传为零成本的抽象，甚至有人使用协程来挂起函数的执行，以避免等待L1缓存未命中（有关更多详细信息，请参见Gor的精彩的[CppCon讲座](https://www.youtube.com/watch?v=j9tlJAqMV7U)）。

第三，可能也是最重要的，它在等待的协程恢复时引入了一些非确定性的执行上下文。

假设我有以下代码：
```c++
cppcoro::static_thread_pool tp;

task foo()
{
  std::cout << "foo1 " << std::this_thread::get_id() << "\n";
  // Suspend coroutine and reschedule onto thread-pool thread.
  co_await tp.schedule();
  std::cout << "foo2 " << std::this_thread::get_id() << "\n";
}

task bar()
{
  std::cout << "bar1 " << std::this_thread::get_id() << "\n";
  co_await foo();
  std::cout << "bar2" << std::this_thread::get_id() << "\n";
}
```

使用之前的实现，我们可以确保在`co_await foo()`之后运行的代码将在与`foo()`完成的相同线程上内联运行。

例如，可能的输出之一是：
```
bar1 1234
foo1 1234
foo2 3456
bar2 3456
```

然而，使用原子操作进行更改后，`foo()`的完成可能与`bar()`的挂起竞争，这在某些情况下可能意味着`co_await foo()`之后的代码可能在`bar()`开始执行的原始线程上运行。

例如，以下输出现在也可能发生：
```
bar1 1234
foo1 1234
foo2 3456
bar2 1234
```

对于许多用例，这种行为可能没有区别。
然而，对于目的就是转换执行上下文的算法来说，这可能是有问题的。

例如，`via()` 算法等待某个 Awaitable，然后在指定的调度器的执行上下文中生成它。
下面是该算法的简化版本。
```c++
template<typename Awaitable, typename Scheduler>
task<await_result_t<Awaitable>> via(Awaitable a, Scheduler s)
{
  auto result = co_await std::move(a);
  co_await s.schedule();
  co_return result;
}

task<T> get_value();
void consume(const T&);

task<void> consumer(static_thread_pool::scheduler s)
{
  T result = co_await via(get_value(), s);
  consume(result);
}
```

使用原来的版本，对`consume()`的调用总是保证在线程池`s`上执行。然而，使用使用原子操作的修订版本，`consume()`可能会在与调度器`s`关联的线程上执行，也可能会在`consumer()`协程所在的线程上执行。

那么，如何在没有原子操作、额外分支和非确定性恢复上下文的情况下解决堆栈溢出问题呢？

## 进入“对称传输 (symmetric transfer)”！

2018年，Gor Nishanov提出的论文[P0913R0](https://wg21.link/P0913R0)“添加对称协程控制传输”提出了一种解决方案，通过提供一种机制，允许在不消耗额外堆栈空间的情况下暂停一个协程，然后对另一个协程进行对称恢复。

该论文提出了两个关键改变：
* 允许从`await_suspend()`返回一个`std::coroutine_handle<T>`，作为指示应将执行对称传输到返回句柄所标识的协程的方式。
* 添加一个`std::experimental::noop_coroutine()`函数，返回一个特殊的`std::coroutine_handle`，可以从`await_suspend()`返回，以暂停当前协程并从调用`.resume()`的位置返回，而不是将执行传输到另一个协程。

那么，“对称传输”是什么意思？

当你通过在`std::coroutine_handle`上调用`.resume()`来恢复一个协程时，调用`.resume()`的调用者将保持在堆栈上活动，而恢复的协程执行。

当这个协程下次挂起并且`await_suspend()`的调用返回`void`（表示无条件挂起）或`true`（表示有条件挂起）时，对`.resume()`的调用将返回。

这可以被看作是执行 (execution)的“非对称传输”，并且行为就像普通的函数调用一样。`.resume()`的调用者可以是任何函数（可以是协程，也可以不是协程）。当该协程挂起并且从`await_suspend()`返回`true`或`void`时，执行将从对`.resume()`的调用返回。

每次我们通过调用`.resume()`来恢复一个协程时，都会为该协程的执行创建一个新的堆栈帧。

然而，使用“对称传输”时，我们只是暂停一个协程并恢复另一个协程。两个协程之间没有隐式的调用者/被调用者关系 - 当一个协程挂起时，它可以将执行传输给任何挂起的协程（包括自身），并且在下次挂起或完成时不一定要将执行传输回先前的协程。

让我们看看当awaiter使用对称传输时，编译器会将`co_await`表达翻译成什么样：

```c++
{
  decltype(auto) value = <expr>;
  decltype(auto) awaitable =
      get_awaitable(promise, static_cast<decltype(value)&&>(value));
  decltype(auto) awaiter =
      get_awaiter(static_cast<decltype(awaitable)&&>(awaitable));
  if (!awaiter.await_ready())
  {
    using handle_t = std::coroutine_handle<P>;

    //<suspend-coroutine>

    auto h = awaiter.await_suspend(handle_t::from_promise(p));
    h.resume();
    //<return-to-caller-or-resumer>
    
    //<resume-point>
  }

  return awaiter.await_resume();
}
```

让我们重点关注与其他`co_await`形式不同的关键部分：
```c++
auto h = awaiter.await_suspend(handle_t::from_promise(p));
h.resume();
//<return-to-caller-or-resumer>
```

一旦协程状态机被降级 (lowered, 应该是指协程被编译器翻译为机器码)（这是另一篇文章的主题），`<return-to-caller-or-resumer>` 部分基本上变成了一个 `return;` 语句，它导致最后一次恢复协程的 `.resume()` 调用返回到它的调用者。

这意味着我们有了这样一种情况：我们调用了另一个具有相同签名的函数 `std::coroutine_handle::resume()`，然后在当前函数中的 `std::coroutine_handle::resume()` 调用的主体之后是一个 `return;`。

一些启用了优化的编译器能够应用一种优化，将位于尾部位置（即在返回之前）的对其他函数的调用转换为尾调用，只要满足一些条件。

碰巧的是，这种尾调用优化正是我们想要做的，以避免之前遇到的堆栈溢出问题。但是，我们不想完全依赖于优化器是否执行尾调用转换，我们希望能够保证尾调用转换发生，即使在未启用优化的情况下。

但首先，让我们深入了解什么是尾调用。

### 尾调用

尾调用是指在调用之前弹出当前栈帧，并且当前函数的返回地址成为被调用函数的返回地址。也就是说，被调用函数将直接返回给调用者。

_简单来说，如果我们有3个函数，且有调用关系 `A()`->`B()`->`C()`。如果`B()`->`C()`满足下述条件，则在开始调用`C()`前，会直接将`B()`的栈帧弹出，然后为`C()`创建栈帧。在`C()`执行完成后，其不会返回`B()`，而是直接返回到`B()`的上级函数`A()`_

在X86/X64架构中，通常情况下编译器会生成这样的代码：首先弹出当前堆栈帧，然后使用`jmp`指令跳转到被调用函数的入口点，而不是使用`call`指令，在`call`返回后再弹出当前堆栈帧。

然而，这种优化通常只在有限的情况下才能实现。

特别是，它需要满足以下条件：
* 调用约定 (calling convension) 支持尾调用，并且对调用者和被调用者相同；
* 返回类型相同；
* 没有需要在调用之后在返回给调用者之前运行的非平凡析构函数 (Non-trivial destructor)；
* 调用不在try/catch块内。

对称传输形式的`co_await`的设计实际上是为了满足所有这些要求。让我们逐个看一下。

**调用约定 (Calling convention)**
当编译器将协程降级为机器码时，实际上将协程分为两部分：ramp（分配和初始化协程帧）和body（包含用户编写的协程体的状态机）。

协程的函数签名（以及任何用户指定的调用约定）仅影响ramp部分，而body部分由编译器控制，从不直接由任何用户代码调用 - 仅由ramp函数和`std::coroutine_handle::resume()`调用。

协程体部分的调用约定对用户不可见，完全由编译器决定，因此它可以选择一个支持尾调用的适当调用约定，并且该调用约定由所有协程体使用。

**返回类型相同**
源协程和目标协程的`.resume()`方法的返回类型都是`void`，因此这个要求是显而易见地满足的。

**没有非平凡析构函数**
当进行尾调用时，我们需要在调用目标函数之前释放当前的栈帧，这要求所有在调用之前的栈上分配的对象的生命周期已经结束。

通常情况下，只要有任何具有非平凡析构函数的对象在作用域内，这将成为一个问题，因为这些对象的生命周期尚未结束，并且这些对象将在栈上分配。

然而，当协程挂起时，它会在不退出任何作用域的情况下挂起，它实现这一点的方式是将生命周期跨越挂起点的任何对象放置在协程帧中，而不是在栈上分配它们。

生命周期不跨越挂起点的局部变量可以在栈上分配，但是这些对象的生命周期已经结束，并且它们的析构函数在协程下次挂起之前已经被调用。

因此，在尾调用返回之后不应该有需要运行的非平凡析构函数的栈上分配对象。

**不在 try/catch 块中的调用**
这个稍微复杂一些，因为在每个协程中，都有一个隐式的 try/catch 块包围着用户编写的协程体。

从规范中我们可以看到，协程的定义如下：
```c++
{
  promise_type promise;
  co_await promise.initial_suspend();
  try { F; }
  catch (...) { promise.unhandled_exception(); }
final_suspend:
  co_await promise.final_suspend();
}
```

其中 `F` 是协程体的用户编写部分。

因此，除了 initial_suspend/final_suspend 之外，每个用户编写的 `co_await` 表达式都存在于 try/catch 块的上下文中。

然而，实现通过在 try 块的上下文之外执行 `.resume()` 调用来解决这个问题。

我希望能够在另一篇博文中更详细地介绍这个方面，该博文将深入探讨协程降级为机器码的细节（本文已经足够长了）。

> 注意，然而，C++规范中的当前措辞并不明确要求实现这样做，这只是一个非规范性的注释，暗示这可能是必需的。希望我们能够在将来修复规范。

因此，我们可以看到执行对称传输的协程通常满足执行尾调用的所有要求。编译器保证这将始终是一个尾调用，无论是否启用了优化。

这意味着通过使用返回`std::coroutine_handle`的`await_suspend()`方法，我们可以暂停当前协程并将执行转移到另一个协程，而不会消耗额外的堆栈空间。

这使我们能够编写相互和递归地恢复彼此的协程，而不用担心堆栈溢出的问题。

这正是我们需要修复`task`实现的地方。

## 重新审视`task`

所以，有了新的“对称传输”能力，让我们回过头来修复我们的`task`类型实现。

为了做到这一点，我们需要对实现中的两个`await_suspend()`方法进行更改：
* 首先，当我们等待任务时，我们需要执行对称传输来恢复任务的协程。
* 其次，当任务的协程完成时，它需要执行对称传输来恢复等待的协程。

为了解决等待方向的问题，我们需要将`task::awaiter`方法从这样的形式：
```c++
void task::awaiter::await_suspend(
    std::coroutine_handle<> continuation) noexcept {
  // Store the continuation in the task's promise so that the final_suspend()
  // knows to resume this coroutine when the task completes.
  coro_.promise().continuation = continuation;

  // Then we resume the task's coroutine, which is currently suspended
  // at the initial-suspend-point (ie. at the open curly brace).
  coro_.resume();
}
```
改成这样:
```c++
std::coroutine_handle<> task::awaiter::await_suspend(
    std::coroutine_handle<> continuation) noexcept {
  // Store the continuation in the task's promise so that the final_suspend()
  // knows to resume this coroutine when the task completes.
  coro_.promise().continuation = continuation;

  // Then we tail-resume the task's coroutine, which is currently suspended
  // at the initial-suspend-point (ie. at the open curly brace), by returning
  // its handle from await_suspend().
  return coro_;
}
```

为了解决返回路径的问题，我们需要将`task::promise_type::final_awaiter`方法从这样的形式从这样：
```c++
void task::promise_type::final_awaiter::await_suspend(
    std::coroutine_handle<promise_type> h) noexcept {
  // The coroutine is now suspended at the final-suspend point.
  // Lookup its continuation in the promise and resume it.
  h.promise().continuation.resume();
}
```
改成这样:
```c++
std::coroutine_handle<> task::promise_type::final_awaiter::await_suspend(
    std::coroutine_handle<promise_type> h) noexcept {
  // The coroutine is now suspended at the final-suspend point.
  // Lookup its continuation in the promise and resume it symmetrically.
  return h.promise().continuation;
}
```

现在我们有了一个`task`的实现，它不再遭受`void`返回的`await_suspend`版本的堆栈溢出问题，也不再有`bool`返回的`await_suspend`版本的非确定性恢复上下文问题。


### 可视化堆栈

现在让我们回过头来看一下我们的原始示例：
```c++
task completes_synchronously() {
  co_return;
}

task loop_synchronously(int count) {
  for (int i = 0; i < count; ++i) {
    co_await completes_synchronously();
  }
}
```

假设其他某个协程`co_await`了`loop_synchronously()`返回的`task`，使得当前协程(也就是 `loop_synchronously()`对应的协程)开始执行。而当前协程是通过对称传输启动的，而这个"其他协程"会在未来调用`std::coroutine_handle::resume()`时被恢复。

因此，当`loop_synchronously()`开始时，堆栈的情况可能如下所示：
```
           Stack                                                Heap
+---------------------------+  <-- top of stack   +--------------------------+
| loop_synchronously$resume | active coroutine -> | loop_synchronously frame |
+---------------------------+                     | +----------------------+ |
| coroutine_handle::resume  |                     | | task::promise        | |
+---------------------------+                     | | - continuation --.   | |
|     ...                   |                     | +------------------|---+ |
+---------------------------+                     | ...                |     |
                                                  +--------------------|-----+
                                                                       V
                                                  +--------------------------+
                                                  | awaiting_coroutine frame |
                                                  |                          |
                                                  +--------------------------+
```

当执行 `co_await completes_synchronously()` 时，它将执行一个对称传输，切换到 `completes_synchronously` 协程。

它通过以下方式实现：
* 调用 `task::operator co_await()`，然后返回 `task::awaiter` 对象
* 然后暂停并调用 `task::awaiter::await_suspend()`，然后返回 `completes_synchronously` 协程的 `coroutine_handle`
* 然后执行尾调用/跳转到 `completes_synchronously` 协程。
  这会在激活 `completes_synchronously` 帧之前弹出 `loop_synchronously` 帧。

如果我们现在查看在`completes_synchronously`被恢复后的堆栈，它将如下所示：
```
              Stack                                          Heap
                                            .-> +--------------------------+ <-.
                                            |   | completes_synchronously  |   |
                                            |   | frame                    |   |
                                            |   | +----------------------+ |   |
                                            |   | | task::promise        | |   |
                                            |   | | - continuation --.   | |   |
                                            |   | +------------------|---+ |   |
                                            `-, +--------------------|-----+   |
                                              |                      V         |
+-------------------------------+ <-- top of  | +--------------------------+   |
| completes_synchronously$resume|     stack   | | loop_synchronously frame |   |
+-------------------------------+ active -----' | +----------------------+ |   |
| coroutine_handle::resume      | coroutine     | | task::promise        | |   |
+-------------------------------+               | | - continuation --.   | |   |
|     ...                       |               | +------------------|---+ |   |
+-------------------------------+               | task temporary     |     |   |
                                                | - coro_       -----|---------`
                                                +--------------------|-----+
                                                                     V
                                                +--------------------------+
                                                | awaiting_coroutine frame |
                                                |                          |
                                                +--------------------------+
```

请注意，这里的栈帧数量没有增加。

在`completes_synchronously`协程完成并执行到闭合大括号时，它将执行`co_await promise.final_suspend()`。

这将暂停协程并调用`final_awaiter::await_suspend()`，它会返回他的continuation中保存的`std::coroutine_handle`（即指向`loop_synchronously`协程的句柄）。然后，它将进行对称传输/尾调用以恢复`loop_synchronously`协程。

如果我们在`loop_synchronously`恢复执行后查看堆栈，它将类似于以下情况：
```
           Stack                                                   Heap
                                                   +--------------------------+ <-.
                                                   | completes_synchronously  |   |
                                                   | frame                    |   |
                                                   | +----------------------+ |   |
                                                   | | task::promise        | |   |
                                                   | | - continuation --.   | |   |
                                                   | +------------------|---+ |   |
                                                   +--------------------|-----+   |
                                                                        V         |
+----------------------------+  <-- top of stack   +--------------------------+   |
| loop_synchronously$resume  | active coroutine -> | loop_synchronously frame |   |
+----------------------------+                     | +----------------------+ |   |
| coroutine_handle::resume() |                     | | task::promise        | |   |
+----------------------------+                     | | - continuation --.   | |   |
|     ...                    |                     | +------------------|---+ |   |
+----------------------------+                     | task temporary     |     |   |
                                                   | - coro_       -----|---------`
                                                   +--------------------|-----+
                                                                        V
                                                   +--------------------------+
                                                   | awaiting_coroutine frame |
                                                   |                          |
                                                   +--------------------------+
```

`loop_synchronously` 协程在恢复执行后，首先要做的事情是在执行到分号时调用从调用 `completes_synchronously` 返回的临时 `task` 的析构函数。
这将销毁协程帧，释放其内存，并使我们处于以下情况：
```
           Stack                                                   Heap
+---------------------------+  <-- top of stack   +--------------------------+
| loop_synchronously$resume | active coroutine -> | loop_synchronously frame |
+---------------------------+                     | +----------------------+ |
| coroutine_handle::resume  |                     | | task::promise        | |
+---------------------------+                     | | - continuation --.   | |
|     ...                   |                     | +------------------|---+ |
+---------------------------+                     | ...                |     |
                                                  +--------------------|-----+
                                                                       V
                                                  +--------------------------+
                                                  | awaiting_coroutine frame |
                                                  |                          |
                                                  +--------------------------+
```

我们现在回到了 `loop_synchronously` 中，我们现在有了与开始时相同数量的栈帧和协程帧，并且每次循环都会保持这样。

因此，我们只使用恒定的存储空间，就可以执行任意多次循环迭代。

关于 `task` 类型的对称传输版本的完整示例，请参考以下 Compiler Explorer 链接：[https://godbolt.org/z/9baieF](https://godbolt.org/z/9baieF)。

## 将对称传输用作 await_suspend 的通用形式

现在我们已经看到了对称传输形式的 awaitable 概念的强大和重要性，我想向您展示这种形式实际上是通用形式，可以理论上替代 `void` 和 `bool` 返回形式的 `await_suspend()`。

但首先，我们需要看一下 [P0913R0](https://wg21.link/P0913R0) 提案为协程设计添加的另一个部分：`std::noop_coroutine()`。

### 终止递归

使用对称传输形式的协程，每当协程暂停时，它就会对称地恢复另一个协程。只要有另一个协程可以恢复，这是很好的，但有时我们没有另一个协程可以执行，只需要暂停并让执行返回到`std::coroutine_handle::resume()`的调用者。

`void` 返回形式和 `bool` 返回形式的 `await_suspend()` 都允许协程暂停并从 `std::coroutine_handle::resume()` 返回，那么如何在对称传输形式中实现这一点呢？

答案是使用特殊的内置 `std::coroutine_handle`，称为“noop协程句柄”，它由函数 `std::noop_coroutine()` 生成。

“noop协程句柄”之所以被称为如此，是因为它的 `.resume()` 实现只是立即返回。也就是说，恢复协程是一个空操作。通常，它的实现包含一条 `ret` 指令。

如果 `await_suspend()` 方法返回 `std::noop_coroutine()` 句柄，那么它不会将执行转移到下一个协程，而是将执行转移到 `std::coroutine_handle::resume()` 的调用者。

### 表示其他 `await_suspend()` 形式

有了这些信息，我们现在可以展示如何使用对称传输形式来表示其他 `await_suspend()` 形式。

`void` 返回形式
```c++
void my_awaiter::await_suspend(std::coroutine_handle<> h) {
  this->coro = h;
  enqueue(this);
}
```
也可以使用`bool`返回形式来编写：
```c++
bool my_awaiter::await_suspend(std::coroutine_handle<> h) {
  this->coro = h;
  enqueue(this);
  return true;
}
```
同时也可以使用对称传输形式来编写：
```c++
std::noop_coroutine_handle my_awaiter::await_suspend(
    std::coroutine_handle<> h) {
  this->coro = h;
  enqueue(this);
  return std::noop_coroutine();
}
```

`bool` 返回形式：
```c++
bool my_awaiter::await_suspend(std::coroutine_handle<> h) {
  this->coro = h;
  if (try_start(this)) {
    // Operation will complete asynchronously.
    // Return true to transfer execution to caller of
    // coroutine_handle::resume().
    return true;
  }

  // Operation completed synchronously.
  // Return false to immediately resume the current coroutine.
  return false;
}
```
也可以使用对称传输形式来编写：
```c++
std::coroutine_handle<> my_awaiter::await_suspend(std::coroutine_handle<> h) {
  this->coro = h;
  if (try_start(this)) {
    // Operation will complete asynchronously.
    // Return std::noop_coroutine() to transfer execution to caller of
    // coroutine_handle::resume().
    return std::noop_coroutine();
  }

  // Operation completed synchronously.
  // Return current coroutine's handle to immediately resume
  // the current coroutine.
  return h;
}
```

### 为什么存在这三种形式？

那么为什么我们在引入了对称传输形式之后，仍然保留了`void`和`bool`返回形式的`await_suspend()`呢？

原因部分是历史原因，部分是实用原因，部分是性能原因。

`void`返回形式可以完全被从`await_suspend()`返回`std::noop_coroutine_handle`类型来替代，因为这两种形式是等价的，都是告诉编译器协程无条件地将执行转移到`std::coroutine_handle::resume()`的调用者。

之所以保留它，我个人认为部分是因为在引入对称传输形式之前它已经在使用中，部分是因为`void`形式在无条件暂停的情况下可以减少代码量和输入量。

`bool` 返回形式相比对称传输形式在某些情况下可以被稍微更好的优化。

考虑一个在另一个翻译单元（翻译单元就是指.cc/.cpp文件）中定义的 `bool` 返回形式的 `await_suspend()` 方法。在这种情况下，编译器可以生成代码，在等待的协程中暂停当前协程，然后在 `await_suspend()` 返回后根据条件执行下一段代码来恢复协程。它知道如果 `await_suspend()` 返回 `false`，应该执行的下一段代码是什么。

对于对称传输形式，我们仍然需要表示相同的逻辑：要么返回给调用者/恢复协程，要么恢复当前协程。
我们需要将 `std::noop_coroutine()` 或当前协程的句柄转换为 `std::coroutine_handle<void>` 类型并返回。

然而，现在，由于`await_suspend()`方法在另一个翻译单元中定义，
编译器无法看到返回的句柄所引用的协程，因此在恢复协程时，
相比于`bool`返回形式，它现在需要执行一些更昂贵的间接调用和可能的分支。

不过，我们也可以将对称传输形式的性能优化与`bool`返回形式等效。例如，
我们可以以这样的方式编写代码，把`await_suspend()`inline化，
但调用在外部定义的返回`bool`的方法，然后有条件地返回适当的句柄。

例如：
```c++
struct my_awaiter {
  bool await_ready();

  // Compilers should in-theory be able to optimise this to the same
  // as the bool-returning version, but currently don't do this optimisation.
  std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) {
    if (try_start(h)) {
      return std::noop_coroutine();
    } else {
      return h;
    }
  }

  void await_resume();

private:
  // This method is defined out-of-line in a separate translation unit.
  bool try_start(std::coroutine_handle<> h);
}
```

然而，目前的编译器（例如Clang 10）无法将其优化为与等效的`bool`返回形式一样高效的代码。尽管如此，除非在非常紧密的循环中等待此代码，否则您可能不会注意到差异。


所以，目前的一般规则是：
* 如果你需要无条件返回到`.resume()`的调用者，使用返回`void`的形式。
* 如果你需要有条件地返回到`.resume()`的调用者或者恢复当前协程，使用返回`bool`的形式。
* 如果你需要恢复另一个协程，使用对称传输的形式。

# 完善

为C++20添加的新的对称传输能力使得编写互相递归恢复的协程变得更加容易，而不用担心堆栈溢出的问题。这个能力对于创建高效且安全的异步协程类型（如本文介绍的`task`）非常关键。

这篇关于对称传输的文章比预期的要长得多。如果你能坚持到这里，非常感谢你！希望你觉得有用。

在下一篇文章中，我将深入了解编译器如何将协程函数转换为状态机。

# Thanks

Thanks to Eric Niebler and Corentin Jabot for providing feedback on drafts of this post.