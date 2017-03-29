#include "directoryEntryIterator.h"

#include "kernelFS.h"
#include "index.h"
#include "partitionData.h"

Entry EMPTY_ENTRY = { 0 };

char entryEmpty(const Entry& entry)
{
	for (unsigned b = 0; b < sizeof(Entry); b++) if ((((char*)&entry)[b]) != 0) return 0;
	return 1;
}

void DirectoryEntryIterator::advanceIterator()
{
	found = 0;
	unsigned numOfEntries = directoryIndex->getNumOfClusters()*ENTRIES_PER_CLUSTER;
	char part = directoryIndex->getPartitionData()->getLetter();

	while ((!found) && (nextEntry < numOfEntries))
	{
		if (emptyEntries == entryEmpty(KernelFS::readRootEntry(part, nextEntry)))
			found = 1;
		else
			nextEntry++;
	}

	if (!found && emptyEntries && directoryIndex->addCluster())
	{
		KernelFS::formatCluster(part, directoryIndex->getCluster(directoryIndex->getNumOfClusters() - 1));

		found = 1;
	}
}

DirectoryEntryIterator::DirectoryEntryIterator(Index* directoryIndex_, char emptyEntries_)
	: directoryIndex(directoryIndex_), emptyEntries(emptyEntries_)
{
	advanceIterator();
}

EntryNum DirectoryEntryIterator::getNextEntry()
{
	EntryNum e = nextEntry++;

	advanceIterator();

	return e;
}

char DirectoryEntryIterator::hasNext()
{
	return found;
}