#include <time.h>

#include "ThreadLifecircleListener.h"
#include "ParallelUtils.h"

parallel_core::ThreadLifecircleListener::ThreadLifecircleListener(ThreadLifecircleEvents* agent) :
	_agent(agent),
	_id(generateId())
{
}

bool parallel_core::ThreadLifecircleListener::onThreadCreated(int threadId)
{
	if (LIKELY(_agent))
		_agent->onThreadCreated(threadId);
	else
		return false;

	return true;
}

bool parallel_core::ThreadLifecircleListener::onThreadClear(int threadId)
{
	if (LIKELY(_agent))
		_agent->onThreadClear(threadId);
	else
		return false;

	return true;
}

inline unsigned long parallel_core::ThreadLifecircleListener::id() const
{
	return _id;
}


unsigned long parallel_core::ThreadLifecircleListener::generateId()
{
	return clock();
}
