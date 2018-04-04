////////////////////////////////////////////////////////////////////////// 
// 
// future-promise implements at C++ from scratch 
// Copyright (c) 2018 Kouji Matsui (@kekyo2) 
// 
// Licensed under the Apache License, Version 2.0 (the "License"); 
// you may not use this file except in compliance with the License. 
// You may obtain a copy of the License at 
// 
//  http://www.apache.org/licenses/LICENSE-2.0 
// 
// Unless required by applicable law or agreed to in writing, software 
// distributed under the License is distributed on an "AS IS" BASIS, 
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
// See the License for the specific language governing permissions and 
// limitations under the License. 
// 
////////////////////////////////////////////////////////////////////////// 

#ifndef FUTURE_PROMISE_H
#define FUTURE_PROMISE_H

#include <memory>
#include <functional>

//////////////////////////////////////////////////////////////////////////
// Future / Promise

namespace std
{
    // If your environment has not std::atomic and not required atomicity:
    //typedef int atomic_int;
};

class synch_context
{
public:
    virtual ~synch_context() {}
    virtual void post(const std::function<void()>& continuation) = 0;

    static std::shared_ptr<synch_context> default_synch_context;
};

#define DECLARE_DEFAULE_SYNCH_CONTEXT(scPtr) \
    static auto default_synch_context = scPtr; \
    std::shared_ptr<synch_context> synch_context::default_synch_context = default_synch_context

template <typename T> class future;
template <typename T> class promise;

template <typename T> class future_core
{
    friend class future<T>;
    friend class promise<T>;

private:
    mutable std::atomic_int count_;
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
        auto c = ++count_;
#ifdef _DEBUG
        printf("add_ref: %d\n", c);
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

        then([pf, m](const T& value)
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

        then([pf, b](const T& value)
        {
            auto pf2 = b(value);
            pf2.then([pf](const U& value2)
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

    static future<T> result(const T& value, const std::shared_ptr<synch_context>& scPtr = synch_context::default_synch_context)
    {
        return future<T>(value, scPtr);
    }
};

template <typename T> class promise
{
private:
    future_core<T>* pFuture_;

public:
    promise(const std::shared_ptr<synch_context>& scPtr = synch_context::default_synch_context)
        : pFuture_(new future_core<T>(scPtr))
    {
    }

    explicit promise(const promise<T>& rhs)
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

//////////////////////////////////////////////////////////////////////////
// Synchronization context

class dummy_synch_context : public synch_context
{
public:
    dummy_synch_context() {}

    void post(const std::function<void()>& continuation) override
    {
        continuation();
    }
};

#include <queue>

class queued_synch_context : public synch_context
{
private:
    std::queue<std::function<void()>> queue_;

public:
    queued_synch_context() {}

    void post(const std::function<void()>& continuation) override
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

#ifdef WIN32

#include <stack>
#include <mutex>
#include <thread>

class threaded_synch_context : public synch_context
{
private:
    std::queue<std::function<void()>> queue_;
    std::stack<std::thread> threads_;
    std::mutex lock_;
    std::condition_variable cv_;
    volatile bool abort_;

    void entry()
    {
        while (true)
        {
            std::unique_lock<std::mutex> lock(lock_);

        loop:
            if (queue_.empty())
            {
                if (abort_)
                {
                    break;
                }

                cv_.wait(lock);
                goto loop;
            }

            auto continuation = queue_.front();
            queue_.pop();

            lock.unlock();

            continuation();
        }
    }

    static void trampoline(threaded_synch_context* pt)
    {
        pt->entry();
    }

public:
    threaded_synch_context(int threads)
        :abort_(false)
    {
        for (auto i = 0; i < threads; i++)
        {
            threads_.push(std::thread(trampoline, this));
        }
    }

    ~threaded_synch_context()
    {
        join();
    }

    void post(const std::function<void()>& continuation) override
    {
        std::lock_guard<std::mutex> lock(lock_);
        queue_.push(continuation);

        if (queue_.size() == 1)
        {
            cv_.notify_one();
        }
    }

    void join()
    {
        abort_ = true;
        cv_.notify_all();

        while (!threads_.empty())
        {
            auto& t = threads_.top();
            t.join();

            threads_.pop();
        }
    }
};
#endif

#endif
