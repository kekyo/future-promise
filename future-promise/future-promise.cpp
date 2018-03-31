// future-promise.cpp : Defines the entry point for the console application.

#include "stdafx.h"

#include <memory>
#include <functional>
#include <queue>

class synch_context
{
private:
    std::queue<std::function<void()>> queue_;

public:
    synch_context() {}

    void post(const std::function<void()>& continuation)
    {
        queue_.push(continuation);
    }

    void consume()
    {
        while (!queue_.empty())
        {
            auto& continuation = queue_.front();

            continuation();

            queue_.pop();
        }
    }
};

template <typename T> class future;
template <typename T> class promise;

template <typename T> class future_core
{
    friend class future<T>;
    friend class promise<T>;

private:
    volatile mutable int count_;
    volatile bool finished_;
    volatile bool ready_;
    T result_;
    std::function<void(const T&)> continuation_;
    std::shared_ptr<synch_context> scPtr_;

    explicit future_core(const std::shared_ptr<synch_context>& scPtr)
        : count_(1), finished_(false), ready_(false), scPtr_(scPtr)
    {
    }

    future_core* add_ref() const
    {
        count_++;
        //printf("add_ref: %d\n", count_);
        return const_cast<future_core*>(this);
    }

    void release() const
    {
        auto c = --count_;
        if (c <= 0)
        {
            delete this;
        }
        //printf("release: %d\n", c);
    }

    void do_continuation() const
    {
        if (finished_ && ready_)
        {
            add_ref();
            scPtr_->post([this]() { continuation_(result_); release(); });
        }
    }

    void set_value(const T& result)
    {
        finished_ = false;
        result_ = result;
        finished_ = true;

        do_continuation();
    }

    void then(std::function<void(const T&)> continuation)
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
    future_core<T>* pFuture_;

    future(future_core<T>* pFuture)
        :pFuture_(pFuture->add_ref())
    {
    }

public:
    future()
        :pFuture_(nullptr)
    {
    }

    ~future()
    {
        if (pFuture_ != nullptr)
        {
            pFuture_->release();
        }
    }

    future(const future<T>& rhs)
        : pFuture_((rhs.pFuture_ != nullptr) ? rhs.pFuture_->add_ref() : nullptr)
    {
    }

    void then(std::function<void(const T&)> continuation)
    {
        pFuture_->then(continuation);
    }

    future<T> &operator =(const future<T>& rhs)
    {
        pFuture_ = rhs.pFuture_->add_ref();
        return *this;
    }
};

template <typename T> class promise
{
private:
    future_core<T>* pFuture_;

public:
    promise(const std::shared_ptr<synch_context> scPtr)
        : pFuture_(new future_core<T>(scPtr))
    {
    }

    ~promise()
    {
        pFuture_->release();
    }

    void set_value(const T& result)
    {
        pFuture_->set_value(result);
    }

    future<T> get_future()
    {
        return future<T>(pFuture_);
    }
};

static void test()
{
    auto scPtr = std::make_shared<synch_context>();

    {
        promise<int> p(scPtr);
        auto f = p.get_future();
        f.then([](auto value) { printf("value1=%d\n", value); });

        p.set_value(123);
    }

    {
        promise<int> p(scPtr);
        p.set_value(456);

        auto f = p.get_future();
        f.then([](auto value) { printf("value2=%d\n", value); });
    }

    scPtr->consume();
}

int main()
{
#ifdef _CRTDBG_ALLOC_MEM_DF
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
#endif

    test();

#ifdef _CRTDBG_ALLOC_MEM_DF
    _CrtDumpMemoryLeaks();
#endif

    return 0;
}

