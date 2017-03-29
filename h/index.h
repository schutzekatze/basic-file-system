#pragma once

#include "part.h"
class PartitionData;

class PartitionData;

class Index
{
public:

	Index(PartitionData* partitionData, ClusterNo mainCluster);
	~Index();

	char addCluster();
	char removeCluster();
	ClusterNo getNumOfClusters();
	ClusterNo getCluster(ClusterNo);

	PartitionData* getPartitionData() const { return partitionData; }
	ClusterNo getMainCluster() const { return mainCluster; }

	static const ClusterNo MAX_ENTRIES = ClusterSize / sizeof(ClusterNo);
	static const ClusterNo MAX_CLUSTERS = MAX_ENTRIES / 2 + MAX_ENTRIES / 2 * MAX_ENTRIES;

private:

	char update();

	PartitionData* partitionData;
	ClusterNo mainCluster;

	ClusterNo cachedMainCluster[MAX_ENTRIES];
	char cached = 0;

	ClusterNo cachedIndirectCluster[MAX_ENTRIES];
	ClusterNo cachedIndirectClusterNo = 0;

	ClusterNo numOfClusters = -1;
};