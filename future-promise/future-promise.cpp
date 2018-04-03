//////////////////////////////////////////////////////////////////////////
//
// future-promise implements at C++ from scratch
// Copyright (c) 2018 Kouji Matsui (@kekyo2)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "future-promise.h"

//////////////////////////////////////////////////////////////////////////
// Test code

static unsigned int get_current_thread_id()
{
#ifdef WIN32
    return ::GetCurrentThreadId();
#else
    return 0;
#endif
}

static void test_core(const std::shared_ptr<synch_context>& scPtr)
{
    {
        promise<int> p(scPtr);
        auto f = p.get_future();
        f.then([](auto value) { printf("value1=%d, %u\n", value, get_current_thread_id()); });

        p.set_value(123);
    }

    {
        promise<int> p(scPtr);
        p.set_value(456);

        auto f = p.get_future();
        f.then([](auto value) { printf("value2=%d, %u\n", value, get_current_thread_id()); });
    }

    {
        promise<int> p(scPtr);
        auto f1 = p.get_future();
        auto f2 = f1.map<int>([](auto value) { return value + 1; });
        f2.then([](auto value) { printf("value3=%d, %u\n", value, get_current_thread_id()); });

        p.set_value(123);
    }

    {
        promise<int> p(scPtr);
        auto f1 = p.get_future();
        auto f2 = f1.bind<int>([scPtr](auto value) { return future<int>::result(value + 2, scPtr); });
        f2.then([](auto value) { printf("value4=%d, %u\n", value, get_current_thread_id()); });

        p.set_value(123);
    }
}

static void test_dummy()
{
    auto scPtr = std::make_shared<dummy_synch_context>();
    test_core(scPtr);
}

static void test_queued()
{
    auto scPtr = std::make_shared<queued_synch_context>();
    test_core(scPtr);
    scPtr->consume();
}

static void test_threaded()
{
    auto scPtr = std::make_shared<threaded_synch_context>(10);
    test_core(scPtr);
    scPtr->join();
}

int main()
{
#ifdef _CRTDBG_ALLOC_MEM_DF
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
#endif

    test_dummy();
    test_queued();
    test_threaded();

#ifdef _CRTDBG_ALLOC_MEM_DF
    _CrtDumpMemoryLeaks();
#endif

    return 0;
}

