#include "kernelFile.h"

#include "kernelFS.h"
#include "partitionData.h"
#include "openedFile.h"

KernelFile::KernelFile(PartitionData* partitionData, ClusterNo indexCluster, char writable_, string name_, string ext_, BytesCnt size_, EntryNum directoryEntryNumber_)
	: index(partitionData, indexCluster), name(name_), ext(ext_), size(size_), writable(writable_), clusterCache(partitionData->getPartition()), directoryEntryNumber(directoryEntryNumber_)
{
	Entry entry;
	strncpy(entry.name, name.c_str(), 8);
	strncpy(entry.ext, ext.c_str(), 3);
	entry.size = size;
	entry.indexCluster = index.getMainCluster();

	KernelFS::writeRootEntry(index.getPartitionData()->getLetter(), directoryEntryNumber, entry);

	vector<OpenedFile*>& openedFiles = partitionData->getOpenedFiles();

	if (openedFiles.size() == 0) WaitForSingleObject(partitionData->getFilesClosed(), 0);

	char exists = 0;
	for (OpenedFile* openedFile : openedFiles)
	{
		if (openedFile->getFilename() == (name + string(".") + ext))
		{
			openedFile->addReference(this);
			exists = 1;

			break;
		}
	}

	if (!exists)
	{
		openedFiles.push_back(new OpenedFile(this));
	}
}

KernelFile::~KernelFile()
{
	EnterCriticalSection(index.getPartitionData()->getFileMutex());

	Entry entry;
	strncpy(entry.name, name.c_str(), 8);
	strncpy(entry.ext, ext.c_str(), 3);
	entry.size = size;
	entry.indexCluster = index.getMainCluster();

	KernelFS::writeRootEntry(index.getPartitionData()->getLetter(), directoryEntryNumber, entry);

	vector<OpenedFile*>& openedFiles = index.getPartitionData()->getOpenedFiles();
	for (vector<OpenedFile*>::iterator it = openedFiles.begin(); it != openedFiles.end(); it++)
	{
		OpenedFile* openedFile = *it;
		if (openedFile->getFilename() == (name + string(".") + ext))
		{
			openedFile->removeReference(this);

			if (writable)
				openedFile->getGuard().stopWrite();
			else
				openedFile->getGuard().stopRead();

			if (openedFile->getReferenceCount() == 0)
			{
				openedFiles.erase(it);
				delete openedFile;
			}

			break;
		}
	}

	if (openedFiles.size() == 0) while(ReleaseSemaphore(index.getPartitionData()->getFilesClosed(), 1, NULL));

	LeaveCriticalSection(index.getPartitionData()->getFileMutex());
}

char KernelFile::write(BytesCnt byteCount, char* buffer)
{
	if ((!writable) || (cursor + byteCount > MAX_SIZE)) return 0;

	BytesCnt totalSize = index.getNumOfClusters() * ClusterSize;
	for (; totalSize < cursor + byteCount; totalSize += ClusterSize) index.addCluster();
	
	if (cursor + byteCount > size) size = cursor + byteCount;

	char* buffer2;

	ClusterNo cluster;

	BytesCnt bytesToWriteTotal = byteCount;
	BytesCnt bytesToWriteInCluster;
	BytesCnt bufferCounter = 0;

	while (bytesToWriteTotal > 0)
	{
		bytesToWriteInCluster = ClusterSize - cursor % ClusterSize;
		if (bytesToWriteInCluster > bytesToWriteTotal) bytesToWriteInCluster = bytesToWriteTotal;

		cluster = index.getCluster(cursor / ClusterSize);

		buffer2 = clusterCache.getClusterBuffer(cluster);

		bytesToWriteTotal -= bytesToWriteInCluster;
		while (bytesToWriteInCluster > 0)
		{
			buffer2[cursor % ClusterSize] = buffer[bufferCounter];

			cursor++;
			bufferCounter++;
			bytesToWriteInCluster--;
		}
	}

	return 1;
}

BytesCnt KernelFile::read(BytesCnt byteCount, char* buffer)
{
	char* buffer2;

	ClusterNo cluster;

	BytesCnt bytesToReadTotal = byteCount;
	BytesCnt bytesToReadInCluster;
	BytesCnt bufferCounter = 0;

	while ((bytesToReadTotal > 0) && (cursor < size))
	{
		bytesToReadInCluster = ClusterSize - cursor % ClusterSize;
		if (bytesToReadInCluster > bytesToReadTotal) bytesToReadInCluster = bytesToReadTotal;

		cluster = index.getCluster(cursor / ClusterSize);

		buffer2 = clusterCache.getClusterBuffer(cluster);

		bytesToReadTotal -= bytesToReadInCluster;
		while ((bytesToReadInCluster > 0) && (cursor < size))
		{
			buffer[bufferCounter] = buffer2[cursor % ClusterSize];

			cursor++;
			bufferCounter++;
			bytesToReadInCluster--;
		}
	}

	return bufferCounter;
}

char KernelFile::seek(BytesCnt cursor_)
{
	if (cursor_ > size) return 0;

	cursor = cursor_;

	return 1;
}

char KernelFile::eof()
{
	return cursor == size;
}

char KernelFile::truncate()
{
	BytesCnt totalSize = index.getNumOfClusters() * ClusterSize;
	for (; totalSize - cursor > ClusterSize ; totalSize -= ClusterSize) index.removeCluster();

	size = cursor;

	return 1;
}

void KernelFile::updateSize()
{
	size = KernelFS::readRootEntry(index.getPartitionData()->getLetter(), directoryEntryNumber).size;
}