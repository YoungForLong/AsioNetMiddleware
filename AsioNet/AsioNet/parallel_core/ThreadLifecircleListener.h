#pragma once

#include <memory>

#include "ThreadLifecircleEvents.h"

namespace parallel_core
{
    class ThreadLifecircleListener : public std::enable_shared_from_this<ThreadLifecircleListener>
    {
    public:
        ThreadLifecircleListener(ThreadLifecircleEvents* agent);

        // @return if result is false, this listener should be destroyed
        bool onThreadCreated(int threadId);

        // @return if result is false, this listener should be destroyed
        bool onThreadClear(int threadId);

        inline unsigned long id() const;

    private:
        unsigned long generateId();

    private:
        ThreadLifecircleEvents* _agent;
        unsigned long _id;
    };
}