#include "kernelfs.h"

using std::pair;
#include "part.h"
#include "index.h"
#include "directoryEntryIterator.h"
#include <string>
using std::string;
#include "kernelFile.h"
#include "file.h"
#include "partitionData.h"
#include "openedFile.h"

LetterGenerator KernelFS::letterGenerator;
map<char, PartitionData*> KernelFS::partitions;
CRITICAL_SECTION KernelFS::mountMutex;

KernelFSInitializer KernelFS::initializer;

char KernelFS::mount(Partition* partition)
{
	EnterCriticalSection(&mountMutex);

	if (letterGenerator.empty())
	{
		LeaveCriticalSection(&mountMutex);

		return 0;
	}
	else
	{
		char c = letterGenerator.getLetter();

		partitions.insert(pair<char, PartitionData*>(c, new PartitionData(partition, c)));

		LeaveCriticalSection(&mountMutex);

		return c;
	}
}

char KernelFS::unmount(char part)
{
	EnterCriticalSection(&mountMutex);

	map<char, PartitionData*>::iterator it = partitions.find(part);
	if (it == partitions.end()) return 0;

	if (it->second->openedFiles.size() > 0) WaitForSingleObject(it->second->getFilesClosed(), INFINITE);

	delete it->second;
	partitions.erase(part);
	letterGenerator.returnLetter(part);

	LeaveCriticalSection(&mountMutex);

	return 1;
}

char KernelFS::format(char part)
{
	map<char, PartitionData*>::iterator it = partitions.find(part);
	if (it == partitions.end()) return 0;

	EnterCriticalSection(&(it->second->formatMutex));

	if (it->second->openedFiles.size() > 0) WaitForSingleObject(it->second->getFilesClosed(), INFINITE);

	it->second->beingFormatted = 1;

	Partition* partition = it->second->getPartition();

	//Initialize the bitvector

	vector<char>& bitvector = it->second->bitvector;
	bitvector.clear();

	ClusterNo bitVectorClusters = it->second->getBitVectorClusters();
	unsigned long
		totalBits = bitVectorClusters * ClusterSize * BITS_IN_BYTE,
		allocatedBits = bitVectorClusters,
		unavailableBits = totalBits - partition->getNumOfClusters();

	unsigned long byteCount;
	unsigned char currentByte, k;
	while (totalBits > 0)
	{
		byteCount = 0;
		while (allocatedBits > 0)
		{
			currentByte = ~0;
			k = BITS_IN_BYTE;
			while ((allocatedBits > 0) && (k > 0))
			{
				currentByte <<= 1;

				k--;
				allocatedBits--;
			}
			byteCount++;
			bitvector.push_back(currentByte);
		}
		totalBits -= byteCount * BITS_IN_BYTE;

		while (totalBits - BITS_IN_BYTE >= unavailableBits)
		{
			bitvector.push_back(~0);
			totalBits -= BITS_IN_BYTE;
		}

		if (unavailableBits % BITS_IN_BYTE != 0)
		{
			currentByte = ~0;
			while (unavailableBits % BITS_IN_BYTE > 0)
			{
				currentByte >>= 1;
				unavailableBits--;
			}
			bitvector.push_back(currentByte);

			totalBits -= BITS_IN_BYTE;
		}

		for (; totalBits > 0; totalBits -= BITS_IN_BYTE) bitvector.push_back(0);
	}

	it->second->cached = 1;

	//Initialize root directory index

	allocateCluster(part);
	if (!formatCluster(part, bitVectorClusters))
	{
		it->second->beingFormatted = 0;

		LeaveCriticalSection(&(it->second->formatMutex));

		return 0;
	}

	it->second->beingFormatted = 0;

	LeaveCriticalSection(&(it->second->formatMutex));

	return 1;
}

char KernelFS::readRootDir(char part, EntryNum n, Directory &d)
{
	map<char, PartitionData*>::iterator it = partitions.find(part);
	if (it == partitions.end()) return 0;

	if (it->second->beingFormatted) return 0;

	EnterCriticalSection(&(it->second->fileMutex));
	
	DirectoryEntryIterator iterator(it->second->getRootIndex(), 0);

	EntryNum entriesToRead = n;
	while ((entriesToRead > 0) && (iterator.hasNext()))
	{
		iterator.getNextEntry();
		entriesToRead--;
	}
	if (entriesToRead > 0)
	{
		LeaveCriticalSection(&(it->second->fileMutex));

		return 0;
	}

	entriesToRead = ENTRYCNT;
	while ((entriesToRead > 0) && (iterator.hasNext()))
	{
		d[ENTRYCNT - entriesToRead] = readRootEntry(it->second->getLetter(), iterator.getNextEntry());

		entriesToRead--;
	}

	LeaveCriticalSection(&(it->second->fileMutex));

	return (char)(ENTRYCNT - entriesToRead);
}

EntryNum KernelFS::findEntry(char* fname)
{
	map<char, PartitionData*>::iterator it = partitions.find(fname[0]);
	if (it == partitions.end()) return INVALID_ENTRY_NUM;

	if (it->second->beingFormatted) return INVALID_ENTRY_NUM;

	string filename = fname;
	string name = filename.substr(3);

	DirectoryEntryIterator iterator(it->second->getRootIndex(), 0);
	while (iterator.hasNext())
	{
		EntryNum e = iterator.getNextEntry();
		Entry entry = readRootEntry(it->second->getLetter(), e);

		if (name == entryGetFullName(entry)) return e;
	}
	return INVALID_ENTRY_NUM;
}

char KernelFS::doesExist(char* fname)
{
	map<char, PartitionData*>::iterator it = partitions.find(fname[0]);
	if (it == partitions.end()) return 0;

	EnterCriticalSection(&(it->second->fileMutex));

	if (findEntry(fname) != INVALID_ENTRY_NUM)
	{
		LeaveCriticalSection(&(it->second->fileMutex));

		return 1;
	}

	LeaveCriticalSection(&(it->second->fileMutex));

	return 0;
}

File* KernelFS::open(char* fname, char mode)
{
	map<char, PartitionData*>::iterator it = partitions.find(fname[0]);
	if (it == partitions.end()) return nullptr;

	if (it->second->beingFormatted) return nullptr;

	EnterCriticalSection(&(it->second->fileMutex));

	EntryNum entryNum = findEntry(fname);
	Entry entry;

	KernelFile* kernelFile;
	File *file = nullptr;

	OpenedFile* workingFile = nullptr;
	
	if (mode == 'r')
	{
		if (entryNum != INVALID_ENTRY_NUM)
		{
			entry = readRootEntry(it->second->getLetter(), entryNum);

			kernelFile = new KernelFile(it->second, entry.indexCluster, 0, entryName2String(entry.name), entryExt2String(entry.ext), entry.size, entryNum);

			file = new File();
			file->myImpl = kernelFile;

			for (OpenedFile* openedFile : it->second->openedFiles) if (openedFile->getFilename() == entryGetFullName(entry)) workingFile = openedFile;

			LeaveCriticalSection(&(it->second->fileMutex));

			workingFile->getGuard().startRead();

			kernelFile->updateSize();

			return file;
		}
	}
	else if (mode == 'w')
	{
		char alreadyExists = 0;

		if (entryNum != INVALID_ENTRY_NUM)
		{
			entry = readRootEntry(it->second->getLetter(), entryNum);

			alreadyExists = 1;
		}
		else
		{
			DirectoryEntryIterator iterator(it->second->getRootIndex(), 1);
			if (iterator.hasNext())
			{
				entryNum = iterator.getNextEntry();

				string filename(fname);
				string name = filename.substr(3, filename.length() - 3 - 4);
				string ext = filename.substr(filename.length() - 3);

				strcpy(entry.name, name.c_str());
				strcpy(entry.ext, ext.c_str());

				ClusterNo cluster = allocateCluster(it->second->getLetter());
				if (cluster == 0) return nullptr;
				formatCluster(it->second->getLetter(), cluster);

				entry.indexCluster = cluster;
			}
			else
			{
				LeaveCriticalSection(&(it->second->fileMutex));

				return nullptr;
			}
		}

		entry.size = 0;
		kernelFile = new KernelFile(it->second, entry.indexCluster, 1, entryName2String(entry.name), entryExt2String(entry.ext), 0, entryNum);

		file = new File();
		file->myImpl = kernelFile;

		for (OpenedFile* openedFile : it->second->openedFiles) if (openedFile->getFilename() == entryGetFullName(entry)) workingFile = openedFile;

		LeaveCriticalSection(&(it->second->fileMutex));

		workingFile->getGuard().startWrite();

		if (alreadyExists)
		{
			Index index(it->second, entry.indexCluster);

			for (ClusterNo i = index.getNumOfClusters(); i > 0; i--) index.removeCluster();
		}

		return file;
	}
	else if (mode == 'a')
	{
		if (entryNum != INVALID_ENTRY_NUM)
		{
			entry = readRootEntry(it->second->getLetter(), entryNum);

			kernelFile = new KernelFile(it->second, entry.indexCluster, 1, entryName2String(entry.name), entryExt2String(entry.ext), entry.size, entryNum);
			kernelFile->seek(kernelFile->getFileSize());

			file = new File();
			file->myImpl = kernelFile;

			for (OpenedFile* openedFile : it->second->openedFiles) if (openedFile->getFilename() == entryGetFullName(entry)) workingFile = openedFile;

			LeaveCriticalSection(&(it->second->fileMutex));

			workingFile->getGuard().startWrite();

			kernelFile->updateSize();

			return file;
		}
	}

	LeaveCriticalSection(&(it->second->fileMutex));

	return nullptr;
}

char KernelFS::deleteFile(char* fname)
{
	map<char, PartitionData*>::iterator it = partitions.find(fname[0]);
	if (it == partitions.end()) return 0;

	if (it->second->beingFormatted) return 0;

	EnterCriticalSection(&(it->second->fileMutex));

	EntryNum entryNum = findEntry(fname);
	if (entryNum != INVALID_ENTRY_NUM)
	{
		for (OpenedFile* openedFile : it->second->openedFiles) if (openedFile->getFilename() == string(fname))
		{
			LeaveCriticalSection(&(it->second->fileMutex));

			return 0;
		}

		Entry entry = readRootEntry(it->second->getLetter(), entryNum);

		Index index(it->second, entry.indexCluster);

		for (ClusterNo i = index.getNumOfClusters(); i > 0; i--) index.removeCluster();

		deallocateCluster(it->second->getLetter(), entry.indexCluster);

		writeRootEntry(it->second->getLetter(), entryNum, EMPTY_ENTRY);

		LeaveCriticalSection(&(it->second->fileMutex));

		return 1;
	}

	LeaveCriticalSection(&(it->second->fileMutex));

	return 0;
}

ClusterNo KernelFS::allocateCluster(char part)
{
	map<char, PartitionData*>::iterator it = partitions.find(part);
	if (it == partitions.end()) return 0;

	EnterCriticalSection(&(it->second->allocationMutex));

	vector<char> &bitvector = it->second->getBitVector();

	unsigned long bitCount = 0;
	for (char &b : bitvector)
	{
		if (b != 0)
		{
			char bit = 1, bitPosition = 0;
			while (!(bit & b))
			{
				bit <<= 1;
				bitPosition++;
			}

			b &= ~bit;

			LeaveCriticalSection(&(it->second->allocationMutex));

			return (ClusterNo)(bitCount + bitPosition);
		}

		bitCount += BITS_IN_BYTE;
	}

	LeaveCriticalSection(&(it->second->allocationMutex));

	return 0;
}

void KernelFS::deallocateCluster(char part, ClusterNo cluster)
{
	map<char, PartitionData*>::iterator it = partitions.find(part);
	if (it == partitions.end()) return;

	EnterCriticalSection(&(it->second->allocationMutex));

	vector<char> &bitvector = it->second->getBitVector();

	char bit = 1, bitPosition = cluster % BITS_IN_BYTE;
	for (; bitPosition > 0; bitPosition--) bit <<= 1;

	bitvector[cluster / BITS_IN_BYTE] |= bit;

	LeaveCriticalSection(&(it->second->allocationMutex));
}

char KernelFS::formatCluster(char part, ClusterNo cluster)
{
	map<char, PartitionData*>::iterator it = partitions.find(part);
	if (it == partitions.end()) return 0;

	char buffer[ClusterSize];
	for (unsigned long i = 0; i < ClusterSize; i++) buffer[i] = 0;

	if (!(it->second->getPartition()->writeCluster(cluster, buffer))) return 0;

	return 1;
}

Entry KernelFS::readRootEntry(char part, EntryNum entryNum)
{
	map<char, PartitionData*>::iterator it = partitions.find(part);
	if (it == partitions.end()) return EMPTY_ENTRY;

	EnterCriticalSection(&(it->second->cacheMutex));

	ClusterNo cluster = it->second->getRootIndex()->getCluster(entryNum / ENTRIES_PER_CLUSTER);
	if (cluster != it->second->cachedRootClusterNo)
	{
		it->second->cacheRootCluster(cluster);
	}

	Entry e = it->second->cachedRootCluster[entryNum % ENTRIES_PER_CLUSTER];

	LeaveCriticalSection(&(it->second->cacheMutex));

	return e;
}

char KernelFS::writeRootEntry(char part, EntryNum entryNum, Entry entry)
{
	map<char, PartitionData*>::iterator it = partitions.find(part);
	if (it == partitions.end()) return 0;

	EnterCriticalSection(&(it->second->cacheMutex));

	ClusterNo cluster = it->second->getRootIndex()->getCluster(entryNum / ENTRIES_PER_CLUSTER);
	if (cluster != it->second->cachedRootClusterNo)
	{
		it->second->cacheRootCluster(cluster);
	}

	it->second->cachedRootCluster[entryNum % ENTRIES_PER_CLUSTER] = entry;

	LeaveCriticalSection(&(it->second->cacheMutex));

	return 1;
}

string KernelFS::entryName2String(char* name)
{
	string ret = "";
	for (int i = 0; i < FNAMELEN; i++)
	{
		if (name[i] == '\0') break;
		ret += name[i];
	}
	return ret;
}

string KernelFS::entryExt2String(char* ext)
{
	string ret = "";
	for (int i = 0; i < FEXTLEN; i++)
	{
		if (ext[i] == '\0') break;
		ret += ext[i];
	}
	return ret;
}

string KernelFS::entryGetFullName(Entry e)
{
	return entryName2String(e.name) + string(".") + entryExt2String(e.ext);
}