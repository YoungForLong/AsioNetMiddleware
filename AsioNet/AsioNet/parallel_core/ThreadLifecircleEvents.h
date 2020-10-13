#pragma once

class ThreadLifecircleEvents
{
public:
	virtual void onThreadCreated(int pid) = 0;
	virtual void onThreadClear(int pid) = 0;
};