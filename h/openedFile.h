#pragma once

#include "readWriteGuard.h"
#include <vector>
using std::vector;
#include <string>
using std::string;

class KernelFile;

class OpenedFile
{
public:

	OpenedFile(KernelFile* file);

	string getFilename() const { return filename; }
	ReadWriteGuard& getGuard() { return guard; }

	void addReference(KernelFile* file);
	void removeReference(KernelFile* file);
	unsigned getReferenceCount() { return files.size(); }

private:

	string filename;
	ReadWriteGuard guard;
	vector<KernelFile*> files;

};