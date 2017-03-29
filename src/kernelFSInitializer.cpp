#include "kernelFSInitializer.h"

#include "Windows.h"
#include "kernelFS.h"

KernelFSInitializer::KernelFSInitializer()
{
	InitializeCriticalSection(&KernelFS::mountMutex);
}