#include "index.h"

#include "kernelFS.h"
#include "partitionData.h"

#define SECOND_LAYER_INDEX(clusterNo) (MAX_ENTRIES / 2 + (clusterNo-MAX_ENTRIES / 2 ) / MAX_ENTRIES)
#define SECOND_LAYER_ENTRY(clusterNo) ((clusterNo - MAX_ENTRIES / 2) % MAX_ENTRIES)

Index::Index(PartitionData* partitionData_, ClusterNo mainCluster_)
	: partitionData(partitionData_), mainCluster(mainCluster_)
{}

Index::~Index()
{
	if (cached)
	{
		partitionData->getPartition()->writeCluster(mainCluster, (char*)cachedMainCluster);
	}

	if (cachedIndirectClusterNo != 0)
		partitionData->getPartition()->writeCluster(cachedIndirectClusterNo, (char*)cachedIndirectCluster);
}

char Index::update()
{
	Partition* partition = partitionData->getPartition();

	if (!cached)
	{
		if (!partition->readCluster(mainCluster, (char*)cachedMainCluster)) return 0;

		cached = 1;
	}

	ClusterNo i = 0, e = 0;
	while ((cachedMainCluster[e] != 0) && (e < MAX_ENTRIES / 2)) e++;
	i = e;
	
	if (e >= MAX_ENTRIES / 2)
	{
		ClusterNo entries[MAX_ENTRIES];

		while ((cachedMainCluster[e] != 0) && (e < MAX_ENTRIES))
		{
			ClusterNo c = 0;

			if (cachedMainCluster[e] == cachedIndirectClusterNo)
			{
				while ((cachedIndirectCluster[c] != 0) && (c < MAX_ENTRIES)) c++;
			}
			else
			{
				if (!partition->readCluster(cachedMainCluster[e], (char*)entries)) return 0;

				while ((entries[c] != 0) && (c < MAX_ENTRIES)) c++;
			}

			i += c;
			e++;
		}
	}

	numOfClusters = i;
	return 1;
}

char Index::addCluster()
{
	if (numOfClusters == -1) update();

	if (numOfClusters == MAX_CLUSTERS) return 0;

	Partition* partition = partitionData->getPartition();

	if (!cached)
	{
		if (!partition->readCluster(mainCluster, (char*)cachedMainCluster)) return 0;

		cached = 1;
	}

	ClusterNo clusterNo = numOfClusters, newCluster;
	if (clusterNo < MAX_ENTRIES / 2)
	{
		newCluster = KernelFS::allocateCluster(partitionData->getLetter());
		if (newCluster == 0) return 0;

		cachedMainCluster[clusterNo] = newCluster;
	}
	else
	{
		ClusterNo *adr;
		ClusterNo entries[MAX_ENTRIES];

		if (SECOND_LAYER_ENTRY(clusterNo) == 0)
		{
			adr = cachedMainCluster + SECOND_LAYER_INDEX(clusterNo);

			newCluster = KernelFS::allocateCluster(partitionData->getLetter());
			if (newCluster == 0) return 0;

			ClusterNo newIndirectCluster = *adr = newCluster;

			newCluster = KernelFS::allocateCluster(partitionData->getLetter());
			if (newCluster == 0) return 0;

			for (unsigned long i = 0; i < MAX_ENTRIES; i++) entries[i] = 0;
			*entries = newCluster;

			if (!partition->writeCluster(newIndirectCluster, (char*)entries)) return 0;
		}
		else
		{
			ClusterNo i = *(cachedMainCluster + SECOND_LAYER_INDEX(clusterNo));

			if (i == cachedIndirectClusterNo)
			{
				adr = cachedIndirectCluster + SECOND_LAYER_ENTRY(clusterNo);
			}
			else
			{
				if (!partition->readCluster(i, (char*)entries)) return 0;

				adr = entries + SECOND_LAYER_ENTRY(clusterNo);
			}

			newCluster = KernelFS::allocateCluster(partitionData->getLetter());
			if (newCluster == 0) return 0;

			*adr = newCluster;

			if (i != cachedIndirectClusterNo)
				if (!partition->writeCluster(i, (char*)entries)) return 0;
		}
	}

	numOfClusters++;
	return 1;
}

char Index::removeCluster()
{
	if (numOfClusters == -1) update();

	if (numOfClusters == 0) return 0;

	Partition* partition = partitionData->getPartition();

	if (!cached)
	{
		if (!partition->readCluster(mainCluster, (char*)cachedMainCluster)) return 0;

		cached = 1;
	}

	ClusterNo clusterNo = numOfClusters - 1, *adr;
	if (clusterNo < MAX_ENTRIES / 2)
	{
		adr = cachedMainCluster + clusterNo;
		KernelFS::deallocateCluster(partitionData->getLetter(), *adr);
		*adr = 0;
	}
	else
	{
		adr = cachedMainCluster + SECOND_LAYER_INDEX(clusterNo);

		ClusterNo entries[MAX_ENTRIES], i = SECOND_LAYER_ENTRY(clusterNo), *adr2;

		if (*adr == cachedIndirectClusterNo)
		{
			adr2 = cachedIndirectCluster + i;
		}
		else
		{
			if (!partition->readCluster(*adr, (char*)entries)) return 0;

			adr2 = entries + i;
		}

		KernelFS::deallocateCluster(partitionData->getLetter(), *adr2);
			
		if (i == 0)
		{
			KernelFS::deallocateCluster(partitionData->getLetter(), *adr);
			*adr = 0;
		}
		else
		{
			*adr2 = 0;
			if (*adr != cachedIndirectClusterNo)
				if (!partition->writeCluster(*adr, (char*)entries)) return 0;
		}
	}

	numOfClusters--;
	return 1;
}

ClusterNo Index::getNumOfClusters()
{
	if (numOfClusters == -1) update();

	return numOfClusters;
}

ClusterNo Index::getCluster(ClusterNo clusterNo)
{
	if (clusterNo >= getNumOfClusters()) return 0;

	Partition* partition = partitionData->getPartition();

	if (!cached)
	{
		if (!partition->readCluster(mainCluster, (char*)cachedMainCluster)) return 0;

		cached = 1;
	}

	if (clusterNo < MAX_ENTRIES / 2)
	{
		return *(cachedMainCluster + clusterNo);
	}
	else
	{
		ClusterNo i = *(cachedMainCluster + SECOND_LAYER_INDEX(clusterNo));

		if (i != cachedIndirectClusterNo)
		{
			if (cachedIndirectClusterNo != 0)
				if (!partition->writeCluster(cachedIndirectClusterNo, (char*)cachedIndirectCluster)) return 0;

			if (!partition->readCluster(i, (char*)cachedIndirectCluster)) return 0;
			cachedIndirectClusterNo = i;
		}

		return *(cachedIndirectCluster + SECOND_LAYER_ENTRY(clusterNo));
	}
}