#include "clusterCache.h"

#include <cstring>

ClusterCache::ClusterCache(Partition* partition_) : partition(partition_)
{}

ClusterCache::~ClusterCache()
{
	for (CacheEntry& entry : cache)
	{
		if (entry.cluster != 0)
		{
			partition->writeCluster(entry.cluster, entry.data);
		}
	}
}

char* ClusterCache::getClusterBuffer(ClusterNo cluster)
{
	CacheEntry* targetEntry = nullptr;

	CacheEntry* replaceableEntry = &cache[0];

	for (CacheEntry& entry : cache)
	{
		if (entry.cluster == cluster)
		{
			entry.age = 0;
			targetEntry = &entry;
		}
		else
		{
			if ((entry.cluster == 0) || (entry.age > replaceableEntry->age))
			{
				replaceableEntry = &entry;
			}

			entry.age++;
		}
	}

	if (targetEntry == nullptr)
	{
		if (replaceableEntry->cluster != 0)
		{
			partition->writeCluster(replaceableEntry->cluster, replaceableEntry->data);
		}

		partition->readCluster(cluster, replaceableEntry->data);
		replaceableEntry->cluster = cluster;
		replaceableEntry->age = 0;

		targetEntry = replaceableEntry;
	}

	return targetEntry->data;
}