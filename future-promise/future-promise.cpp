// future-promise.cpp : Defines the entry point for the console application.

#include "stdafx.h"

#include <memory>
#include <functional>

template <typename T> class future;
template <typename T> class promise;

template <typename T> class future_core
{
    friend class future<T>;
    friend class promise<T>;

private:
    volatile bool finished_;
    volatile bool ready_;
    T result_;
    std::function<void(const T&)> continuation_;

    void do_continuation()
    {
        if (finished_ && ready_)
        {
            continuation_(result_);
        }
    }

    constexpr future_core()
        : finished_(false), ready_(false)
    {
    }

    constexpr void set_value(const T& result)
    {
        finished_ = false;
        result_ = result;
        finished_ = true;
        do_continuation();
    }

    constexpr void then(std::function<void(const T&)> continuation)
    {
        ready_ = false;
        continuation_ = continuation;
        ready_ = true;
        do_continuation();
    }
};

template <typename T> class promise;

template <typename T> class future
{
    friend class promise<T>;

private:
    std::shared_ptr<future_core<T>> futurePtr_;

    constexpr future(const std::shared_ptr<future_core<T>>& futurePtr)
        :futurePtr_(futurePtr)
    {
    }

public:
    constexpr future()
    {
    }

    constexpr future(const future<T>& rhs)
        : futurePtr_(rhs.futurePtr_)
    {
    }

    constexpr void then(std::function<void(const T&)> continuation)
    {
        futurePtr_->then(continuation);
    }

    constexpr future<T> &operator =(const future<T>& rhs)
    {
        futurePtr_ = rhs.futurePtr_;
        return *this;
    }
};

template <typename T> class promise
{
private:
    std::shared_ptr<future_core<T>> futurePtr_;

public:
    constexpr promise()
        : futurePtr_(new future_core<T>())
    {
    }

    constexpr void set_value(const T& result)
    {
        futurePtr_->set_value(result);
    }

    constexpr future<T> get_future()
    {
        return future<T>(futurePtr_);
    }
};

int main()
{
    {
        promise<int> p;
        auto f = p.get_future();
        f.then([](auto value) { printf("value1=%d\n", value); });

        p.set_value(123);
    }

    {
        promise<int> p;
        p.set_value(456);

        auto f = p.get_future();
        f.then([](auto value) { printf("value2=%d\n", value); });
    }

    return 0;
}

