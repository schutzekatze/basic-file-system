#include "openedFile.h"

#include "kernelFile.h"
#include <algorithm>
using std::find;

OpenedFile::OpenedFile(KernelFile* file): filename(file->getName() + string(".") + file->getExt()) { files.push_back(file); }

void OpenedFile::addReference(KernelFile* file) { files.push_back(file); }

void OpenedFile::removeReference(KernelFile* file)
{
	vector<KernelFile*>::iterator it;
	if ((it = find(files.begin(), files.end(), file)) != files.end()) files.erase(it);
}