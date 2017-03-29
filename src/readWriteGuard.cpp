#include "readWriteGuard.h"

ReadWriteGuard::ReadWriteGuard()
{
	writeSemaphore = CreateSemaphore(NULL, 1, 1, NULL);
	InitializeCriticalSection(&mutex);
}

ReadWriteGuard::~ReadWriteGuard()
{
	CloseHandle(writeSemaphore);
	DeleteCriticalSection(&mutex);
}

void ReadWriteGuard::startWrite()
{
	WaitForSingleObject(writeSemaphore, INFINITE);
}

void ReadWriteGuard::stopWrite()
{
	ReleaseSemaphore(writeSemaphore, 1, NULL);
}

void ReadWriteGuard::startRead()
{
	EnterCriticalSection(&mutex);

	readerCount++;
	if (readerCount == 1) WaitForSingleObject(writeSemaphore, INFINITE);

	LeaveCriticalSection(&mutex);
}

void ReadWriteGuard::stopRead()
{
	EnterCriticalSection(&mutex);

	readerCount--;
	if (readerCount == 0) ReleaseSemaphore(writeSemaphore, 1, NULL);

	LeaveCriticalSection(&mutex);
}