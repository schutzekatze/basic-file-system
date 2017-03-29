#pragma once

#include "part.h"

const unsigned INDEX_CACHE_SIZE = 16;

struct CacheEntry
{
	char data[ClusterSize];
	ClusterNo cluster = 0;
	unsigned age = 0;
};

class ClusterCache
{
public:

	ClusterCache(Partition*);
	~ClusterCache();

	char* getClusterBuffer(ClusterNo);

private:

	Partition* partition;
	CacheEntry cache[INDEX_CACHE_SIZE];
};