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

    explicit future_core(const T& result, const std::shared_ptr<synch_context>& scPtr)
        : count_(1), finished_(true), ready_(false), result_(result), scPtr_(scPtr)
    {
    }

private:
    future_core* add_ref() const
    {
        count_++;
#ifdef _DEBUG
        printf("add_ref: %d\n", count_);
#endif
        return const_cast<future_core*>(this);
    }

    void release() const
    {
        auto c = --count_;
        if (c <= 0)
        {
            delete this;
        }
#ifdef _DEBUG
        printf("release: %d\n", c);
#endif
    }

    void do_continuation() const
    {
        if (finished_ && ready_)
        {
            add_ref();
            scPtr_->post([this]()
            {
                continuation_(result_);
                release();
            });
        }
    }

    void set_value(const T& result)
    {
        finished_ = false;
        result_ = result;
        finished_ = true;

        do_continuation();
    }

    void then(const std::function<void(const T&)>& continuation)
    {
        ready_ = false;
        continuation_ = continuation;
        ready_ = true;

        do_continuation();
    }

    template <typename U> future<U> map(const std::function<U(const T&)>& mapper)
    {
        auto pf = new future_core<U>(scPtr_);
        std::function<U(const T&)> m(mapper);

        then([pf, m](auto value)
        {
            pf->set_value(m(value));
            pf->release();
        });

        return future<U>(pf);
    }

    template <typename U> future<U> bind(const std::function<future<U>(const T&)>& binder)
    {
        auto pf = new future_core<U>(scPtr_);
        std::function<future<U>(const T&)> b(binder);

        then([pf, b](auto value)
        {
            auto pf2 = b(value);
            pf2.then([pf](auto value2)
            {
                pf->set_value(value2);
                pf->release();
            });
        });

        return future<U>(pf);
    }
};

template <typename T> class promise;

template <typename T> class future
{
    friend class promise<T>;
    friend class future_core<T>;

private:
    future_core<T>* pFuture_;

    future(future_core<T>* pFuture)
        :pFuture_(pFuture->add_ref())
    {
    }

    future(const T& value, const std::shared_ptr<synch_context>& scPtr)
        :pFuture_(new future_core<T>(value, scPtr))
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

    future<T> &operator =(const future<T>& rhs)
    {
        pFuture_ = rhs.pFuture_->add_ref();
        return *this;
    }

    void then(const std::function<void(const T&)>& continuation)
    {
        pFuture_->then(continuation);
    }

    template <typename U> future<U> map(const std::function<U(const T&)>& mapper)
    {
        return pFuture_->map(mapper);
    }

    template <typename U> future<U> bind(const std::function<future<U>(const T&)>& binder)
    {
        return pFuture_->bind(binder);
    }

    static future<T> result(const T& value, const std::shared_ptr<synch_context>& scPtr)
    {
        return future<T>(value, scPtr);
    }
};

template <typename T> class promise
{
private:
    future_core<T>* pFuture_;

public:
    promise(const std::shared_ptr<synch_context>& scPtr)
        : pFuture_(new future_core<T>(scPtr))
    {
    }

    promise(const promise<T>& rhs)
        : pFuture_(rhs.pFuture_->add_ref())
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

    {
        promise<int> p(scPtr);
        auto f1 = p.get_future();
        auto f2 = f1.map<int>([](auto value) { return value + 1; });
        f2.then([](auto value) { printf("value3=%d\n", value); });

        p.set_value(123);
    }

    {
        promise<int> p(scPtr);
        auto f1 = p.get_future();
        auto f2 = f1.bind<int>([scPtr](auto value) { return future<int>::result(value + 2, scPtr); });
        f2.then([](auto value) { printf("value4=%d\n", value); });

        p.set_value(123);
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

