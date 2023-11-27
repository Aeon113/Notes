# 协程 03

## 1. awaiter 和 `co_await`

An Awaiter type is a type that implements the three special methods that are called as part of a co_await expression:
  await_ready()
  await_suspend()
  await_resume()

---

"co_await `<expr>`" 在执行过程中可以挂起当前协程, 其会被大致翻译成如下代码:

```cpp
template<typename P, typename T>
decltype(auto) get_awaitable(P& promise, T&& expr)
{
  if constexpr (has_any_await_transform_member_v<P>)
    return promise.await_transform(static_cast<T&&>(expr));
  else
    return static_cast<T&&>(expr);
}

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

{
  // 当前处于coroutine function中

  auto&& value = <expr>;
  // 如果当前promise有 await_transfrom(), 则先将<expr>传入此函数
  auto&& awaitable = get_awaitable(promise, static_cast<decltype(value)>(value));
  // 如果上方得到的 awaitable 有 operator co_await(), 则将其传入此重载运算符函数
  auto&& awaiter = get_awaiter(static_cast<decltype(awaitable)>(awaitable));
  if (!awaiter.await_ready())
  {
    using handle_t = std::experimental::coroutine_handle<P>;

    using await_suspend_result_t =
      decltype(awaiter.await_suspend(handle_t::from_promise(p)));

    // 在这里保存当前coroutine的状态, 包括保存coroutine frame和设置 coroutine 恢复时需要执行
    // 的指令的地址(也就是下方的<resume-point>的位置)。
    // 真正挂起当前coroutine的位置是下方的<return-to-caller-or-resumer>
    <prepare-coroutine-suspending>;

    // 无返回值的await_suspend()一定会挂起当前coroutine
    // bool 返回值版本的 await_suspend() 则只会在返回值为true时挂起。
    // 因此可以使用bool版本的await_suspend()来处理 "等待的条件在事实挂起前就已满足"的情况。
    //
    // 如果 await_suspend()抛异常, 则会直接退回到上级函数, 后续的 await_resume() 将不会执行。
    // 当前coroutine在<return-to-caller-or-resumer>处挂起, 此时会返回到当前coroutine的上级函数。

    if constexpr (std::is_void_v<await_suspend_result_t>) {
      awaiter.await_suspend(handle_t::from_promise(p));
      <return-to-caller-or-resumer>;
    } else {
      static_assert(
         std::is_same_v<await_suspend_result_t, bool>,
         "await_suspend() must return 'void' or 'bool'.");

    if (awaiter.await_suspend(handle_t::from_promise(p))) {
        <return-to-caller-or-resumer>;
      }
    }

    // 如上方所说, 当前coroutine的恢复点在这里。
    <resume-point>;
  }

  // 根据上方的代码可以看出, 事实上 co_await <expr> 不一定会挂起 (await_ready()返回true时不挂起)。
  // 不论是否挂起, 只要 await_suspend() 不抛异常, 下方的 await_resume()就一定会执行。
  // await_resume()的返回值就是本次 co_await <expr> 操作给当前coroutine 的返回值。
  // 如果await_resume()抛异常, 则也会向上方的 await_suspend()一样, 被直接抛给当前coroutine。
  return awaiter.await_resume();
}
```

## 2. Coroutine Handle

上方的 `await_suspend()`就接受一个 `coroutine_handle<P>`。

这种类型代表对协程帧的non-owning handle。其可以类比成裸指针, 裸指针指向一段内存, 多个裸指针可以指向同一块内存, 因此说 "non-owning"; `coroutine_handle<p>`也是如此, 其只是指向一块协程帧的"类裸指针"。

Coroutine Handle可以用来恢复协程的执行或销毁协程帧。它也可以用来获取协程的 promise 对象的访问权限。

`coroutine_handle<p>` 的具体实现由标准库提供, 其接口定义大致如下:

```cpp
namespace std
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

其中的 `.resume()`即是用于恢复一个已挂起的协程的函数。这个协程将会回到上方的 `<resume-point>`处。随后, 直到这个协程再次挂起 (也就是再次执行了 `co_await <expr>`, 并进入到了其中的 `<return-to-caller-or-resumer>`处或抛异常), 或者退出(执行完其中所有代码或者遇到 `co_return`)后, `.resume()`才会返回。

`.destroy()` 方法销毁协程帧，调用任何在作用域内的变量的析构函数，并释放协程帧使用的内存。一般来说，你不需要（实际上应该避免）调用 `.destroy()`，除非你是一个库编写者，正在实现协程 promise 类型。通常，协程帧将由从协程调用返回的某种 RAII 类型拥有。因此，未经 RAII 对象协作就调用 `.destroy()` 可能会导致双重销毁错误。

这里补充一下, 如果一个协程在 刚才的 `.resume()`过程中结束, 这个协程帧会自动销毁, 不需要调用 `.destroy()`。

`.promise()` 方法返回对协程的 promise 对象的引用。然而，就像 `.destroy()` 一样，它通常只在你编写协程 promise 类型时有用。你应该把协程的 promise 对象视为协程的内部实现细节。

For most **Normally Awaitable** types you should use `coroutine_handle<void>` as the parameter type to the `await_suspend()` method instead of `coroutine_handle<Promise>`.

`coroutine_handle<P>::from_promise(P& promise)` 函数允许从对协程的 promise 对象的引用构建 coroutine handle, 对同一个promise 调用多次这个函数会生成多个 coroutine handle, 但它们所指向的都是同一个协程帧。注意，你必须确保类型 P 与用于协程帧的具体 promise 类型完全匹配；试图在具体 promise 类型为 `Derived` 时构造一个 `coroutine_handle<Base>` 可能会导致未定义的行为。

`.address()` / `from_address()` 函数允许从coroutine handle得到 `void*` 指针, 或者从 `void *`得到一个coroutine handle。如果你通过对一个 coroutine handle 的 `.address()`得到了一个 `void *`, 然后再把它传给 `coroutine_handle<P>::from_address()` 得到一个新的 coroutine handle, 则这2个coroutine handle 其实指向的是同一个协程帧。这主要是为了允许将其作为 'context' 参数传入现有的 C 风格 API，所以在某些情况下，你可能会发现它在实现 Awaitable 类型时很有用。然而，在大多数情况下，我发现有必要通过这个 'context' 参数向回调传递额外的信息，所以我通常会把协程句柄存储在一个结构体中，并把指向该结构体的指针作为 'context' 参数传递，而不是使用 `.address()` 的返回值。
