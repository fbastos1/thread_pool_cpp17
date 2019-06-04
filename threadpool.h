#pragma once

#include <vector>
#include <thread>
#include <future> //packaged_task
#include <queue>
#include <functional> //bind
#include <mutex> //unique_lock
#include <condition_variable>
#include <type_traits> //invoke_result

#include "globals.h"

class thread_pool {
public:
    thread_pool(size_t thread_count);
    ~thread_pool();
    
    //since std::thread objects are not copiable, it doesn't make sense for a thread_pool
    //  to be copiable.
    thread_pool(const thread_pool &) = delete;
    thread_pool &operator=(const thread_pool &) = delete;
    
    //F must be Callable, and invoking F with ...Args must be well-formed.
    template <typename F, typename ...Args>
    auto execute(F, Args&&...);
    
private:
    //_task_container_base and _task_container exist simply as a wrapper around a 
    //  MoveConstructible - but not CopyConstructible - Callable object. Since an
    //  std::function requires a given Callable to be CopyConstructible, we cannot
    //  construct one from a lambda function that captures a non-CopyConstructible
    //  object (such as the packaged_task declared in execute) - because a lambda
    //  capturing a non-CopyConstructible object is not CopyConstructible.
    
    //_task_container_base exists only to serve as an abstract base for _task_container.
    class _task_container_base {
    public:
        virtual ~_task_container_base() {};
        
        virtual void operator()() = 0;
    };
        
    //_task_container takes a typename F, which must be Callable and MoveConstructible.
    //  Furthermore, F must be callable with no arguments; it can, for example, be a
    //  bind object with no placeholders.
    //  F may or may not be CopyConstructible.
    template <typename F>
    class _task_container : public _task_container_base {
    public:
        //here, std::forward is needed because we need the construction of _f *not* to
        //  bind an lvalue reference - it is not a guarantee that an object of type F is
        //  CopyConstructible, only that it is MoveConstructible.
        _task_container(F &&func) : _f(std::forward<F>(func)) {}
        
        void operator()() override {
            _f();
        }

    private:
        F _f;
    };
    
    //returns a unique_ptr to a _task_container that wraps around a given function
    //  for details on _task_container_base and _task_container, see above
    //  This exists so that _Func may be inferred from f.
    template <typename _Func>
    static std::unique_ptr<_task_container_base> allocate_task_container(_Func &&f) {
        //in the construction of the _task_container, f must be std::forward'ed because
        //  it may not be CopyConstructible - the only requirement for an instantiation
        //  of a _task_container is that the parameter is of a MoveConstructible type.
        return std::unique_ptr<_task_container_base>(
            new _task_container<_Func>(std::forward<_Func>(f))
        );
    }
    
    std::vector<std::thread> _threads;
    std::queue<std::unique_ptr<_task_container_base>> _tasks;
    std::mutex _task_mutex;
    std::condition_variable _task_cv;
    bool _stop_threads = false;
};

template <typename F, typename ...Args>
auto thread_pool::execute(F function, Args &&...args) {
    std::unique_lock<std::mutex> queue_lock(_task_mutex, std::defer_lock);
    std::packaged_task<std::invoke_result_t<F, Args...>()> task_pkg(
        std::bind(function, args...)
    );
    std::future<std::invoke_result_t<F, Args...>> future = task_pkg.get_future();

    queue_lock.lock();
    //this lambda move-captures the packaged_task declared above. Since the packaged_task
    //  type is not CopyConstructible, the function is not CopyConstructible either -
    //  hence the need for a _task_container to wrap around it.
    _tasks.emplace(
        allocate_task_container([task(std::move(task_pkg))]() mutable { task(); })
    );
    queue_lock.unlock();

    _task_cv.notify_one();

    return std::move(future);
}
