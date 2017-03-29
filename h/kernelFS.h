#pragma once

#include "letterGenerator.h"
#include "part.h"
#include "fs.h"
#include <map>
using std::map;
#include "windows.h"
#include "kernelFSInitializer.h"
#include <string>
using std::string;

const unsigned short BITS_IN_BYTE = 8;
const EntryNum ENTRIES_PER_CLUSTER = ClusterSize / sizeof(Entry);

class Partition;
class PartitionData;
class File;
class KernelFile;

class KernelFS
{
public:
	static char mount(Partition* partition);

	static char unmount(char part);

	static char format(char part);

	static char readRootDir(char part, EntryNum n, Directory &d);

	static char doesExist(char* fname);

	static File* open(char* fname, char mode);

	static char deleteFile(char* fname);

	//Clusters:

	static ClusterNo allocateCluster(char part);
	static void deallocateCluster(char part, ClusterNo cluster);

	static char formatCluster(char part, ClusterNo);

	//Root entries:

	static const EntryNum INVALID_ENTRY_NUM = -1;

	static Entry readRootEntry(char, EntryNum);
	static char writeRootEntry(char, EntryNum, Entry);

	static string entryName2String(char*);
	static string entryExt2String(char*);
	static string entryGetFullName(Entry);

private:

	static EntryNum findEntry(char* fname);

	static LetterGenerator letterGenerator;
	static map<char, PartitionData*> partitions;

	static CRITICAL_SECTION mountMutex;

	friend class KernelFSInitializer;
	static KernelFSInitializer initializer;

};