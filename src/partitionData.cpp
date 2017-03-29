#include "partitionData.h"

#include "kernelFS.h"
#include "openedFile.h"

PartitionData::PartitionData(Partition* partition_, char letter_) : partition(partition_), letter(letter_)
{
	bitVectorClusters = (ClusterNo)ceil((double)partition->getNumOfClusters() / ClusterSize / BITS_IN_BYTE);

	rootIndex = new Index(this, bitVectorClusters);

	InitializeCriticalSection(&allocationMutex);
	InitializeCriticalSection(&cacheMutex);
	InitializeCriticalSection(&formatMutex);
	InitializeCriticalSection(&fileMutex);

	filesClosed = CreateSemaphore(NULL, 1, 1, NULL);
}

PartitionData::~PartitionData()
{
	DeleteCriticalSection(&allocationMutex);
	DeleteCriticalSection(&cacheMutex);
	DeleteCriticalSection(&formatMutex);
	DeleteCriticalSection(&fileMutex);

	CloseHandle(filesClosed);

	if (cached)
	{
		char buffer[ClusterSize];
		for (ClusterNo i = 0; i < bitVectorClusters; i++)
		{
			for (unsigned long b = 0; b < ClusterSize; b++)
				buffer[b] = bitvector[b];

			partition->writeCluster(i, buffer);
		}
	}

	if (cachedRootClusterNo)
	{
		partition->writeCluster(cachedRootClusterNo, (char*)cachedRootCluster);
	}

	delete rootIndex;
}

void PartitionData::cache()
{
	char buffer[ClusterSize];

	for (ClusterNo i = 0; i < bitVectorClusters; i++)
	{
		partition->readCluster(i, buffer);

		for (unsigned long b = 0; b < ClusterSize; b++)
			bitvector.push_back(buffer[b]);
	}

	cached = 1;
}

vector<char>& PartitionData::getBitVector()
{
	if (!cached) cache();

	return bitvector;
}

void PartitionData::cacheRootCluster(ClusterNo cluster)
{
	if ((cachedRootClusterNo) && (cachedRootClusterNo != cluster))
	{
		partition->writeCluster(cachedRootClusterNo, (char*)cachedRootCluster);
	}

	partition->readCluster(cluster, (char*)cachedRootCluster);
	cachedRootClusterNo = cluster;
}