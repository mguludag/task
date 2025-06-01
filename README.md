# task

A generic, header-only C++ task wrapper that supports cancellation, continuations, and result retrieval via futures.

## Features

- **Composable asynchronous tasks**: Chain tasks using `.then()`.
- **Cancellation support**: Cancel tasks before execution.
- **Continuation support**: Attach continuations to tasks for flexible workflows.
- **Future-based result access**: Retrieve results using `std::shared_future`.
- **Explicit invocation**: Control when tasks start via an invoker object.
- **Header-only**: Just include the headers, no linking required.

## Getting Started

### Installation

This library is header-only. You can use one of the following methods:

#### 1. Manual Copy

Copy the contents of the `include/` directory into your project.

#### 2. CMake (find_package)

```cmake
find_package(task CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE mgutility::task)
```

#### 3. CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    task
    GIT_REPOSITORY https://github.com/mguludag/task.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(task)
target_link_libraries(your_target PRIVATE mgutility::task)
```

### Usage

```cpp
#include <mgutility/task.hpp>
#include <iostream>

int main() {
    mgutility::task<int> t([] { return 42; });
    auto t2 = t.then([](std::shared_future<int> f) { return f.get() + 1; });

    t.get_invoker()(); // Start the chain

    int result = t2.get();
    std::cout << "Result: " << result << std::endl; // Output: Result: 43
}
```

#### With Thread Pools (e.g., Asio)

```cpp
#include <asio/thread_pool.hpp>
#include <asio/post.hpp>
#include <mgutility/task.hpp>

asio::thread_pool pool(4);
mgutility::task<int> t([] { return 42; });
auto t2 = t.then([](std::shared_future<int> f) { return f.get() + 1; });
asio::post(pool, t.get_invoker());
int result = t2.get();
pool.join();
```

## API Overview

See [include/task.hpp](include/mgutility/task.hpp) for full documentation.

- [`mgutility::task`](include/mgutility/task.hpp): Main task class template.
- `task::then()`: Attach a continuation.
- `task::cancel()`: Cancel the task.
- `task::get_future()`: Get the shared future.
- `task::get_invoker()`: Get an invoker to start the task.

## Requirements

- C++11 or newer

## License

MIT License. See [LICENSE](LICENSE) for details.


