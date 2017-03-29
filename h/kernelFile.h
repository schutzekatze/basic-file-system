#pragma once

#include "fs.h"
#include "index.h"
#include "clusterCache.h"
#include <string>
using std::string;

class KernelFile
{
public:

	KernelFile(PartitionData* partitionData, ClusterNo indexCluster, char writable, string name, string ext, BytesCnt size, EntryNum directoryEntryNumber);
	~KernelFile();
	char write(BytesCnt, char* buffer);
	BytesCnt read(BytesCnt, char* buffer);
	char seek(BytesCnt);
	BytesCnt filePos() const { return cursor; }
	char eof();
	BytesCnt getFileSize() const { return size; }
	char truncate();

	string getName() const { return name; }
	string getExt() const { return ext; }

	void updateSize();

	const BytesCnt MAX_SIZE = Index::MAX_CLUSTERS*ClusterSize;

private:

	string name;
	string ext;
	BytesCnt cursor = 0;
	BytesCnt size;

	char writable;

	Index index;

	ClusterCache clusterCache;

	EntryNum directoryEntryNumber;
};