#pragma once

#include "part.h"
#include <vector>
using std::vector;
#include "index.h"
#include "fs.h"
#include "kernelFS.h"
#include "Windows.h"

class Index;
class OpenedFile;

class PartitionData
{
public:

	PartitionData::PartitionData(Partition* partition, char letter);
	~PartitionData();

	Partition* getPartition() const { return partition; }
	char getLetter() const { return letter; }

	ClusterNo getBitVectorClusters() const { return bitVectorClusters; }
	vector<char>& getBitVector();

	Index* getRootIndex() const { return rootIndex; }

	vector<OpenedFile*>& getOpenedFiles() { return openedFiles; }
	HANDLE getFilesClosed() const { return filesClosed; }

	CRITICAL_SECTION* getFileMutex() const { return (CRITICAL_SECTION*)&fileMutex; }

private:

	friend class KernelFS;

	void cacheRootCluster(ClusterNo);
	char cached = 0;
	void cache();

	Partition* partition;
	char letter;

	ClusterNo bitVectorClusters;
	vector<char> bitvector;

	ClusterNo cachedRootClusterNo = 0;
	Entry cachedRootCluster[ENTRIES_PER_CLUSTER];
	char reserved[ClusterSize - ClusterSize / sizeof(Entry) * sizeof(Entry)];

	Index *rootIndex;

	CRITICAL_SECTION allocationMutex;
	CRITICAL_SECTION cacheMutex;
	CRITICAL_SECTION formatMutex;
	CRITICAL_SECTION fileMutex;

	HANDLE filesClosed;

	char beingFormatted = 0;

	vector<OpenedFile*> openedFiles;

};