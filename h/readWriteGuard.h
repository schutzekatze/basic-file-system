#pragma once

#include "windows.h"

class ReadWriteGuard
{
public:

	ReadWriteGuard();
	~ReadWriteGuard();

	void startWrite();
	void stopWrite();

	void startRead();
	void stopRead();

private:

	unsigned readerCount = 0;
	HANDLE writeSemaphore;
	CRITICAL_SECTION mutex;
};