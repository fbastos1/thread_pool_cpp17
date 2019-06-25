# thread_pool_cpp17
C++17 implementation of a thread pool. Requires some C++17 features (see *Required language features*).

### Sample usage
```
#include <iostream>
#include <vector>

int multiply(int x, int y) {
    return x * y;
}

int main() {
    thread_pool pool(4);
    std::vector<std::future<int>> futures;

    for (const int &x : { 2, 4, 7, 13 }) {
        futures.push_back(pool.execute(multiply, x, 2));
    }

    for (auto &fut : futures) {
        std::cout << fut.get() << std::endl;
    }

    return 0;
}
```

### Required language features (C++14 and newer only)

*[std::is_invocable, std::is_invocable_v](https://en.cppreference.com/w/cpp/types/is_invocable) (C++17 feature)*  
*[std::invoke_result](https://en.cppreference.com/w/cpp/types/result_of) (C++17 feature)*  
[move-only lambda capture](https://en.cppreference.com/w/cpp/language/lambda#Lambda_capture) (C++14 feature)  


### Compilation flags
###### This may not compile without an explicit C++17 flag (depending on your compiler). If there are errors, double check the flags you pass to your compiler.
**clang:** -std=c++17  
**gcc:** -std=c++17  
**msvc:** /std:c++17
