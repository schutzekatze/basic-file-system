#pragma once

class Index;
#include "fs.h"

extern Entry EMPTY_ENTRY;

class DirectoryEntryIterator
{
public:

	DirectoryEntryIterator(Index* directoryIndex, char emptyEntries);

	EntryNum getNextEntry();
	char hasNext();

private:

	void advanceIterator();

	EntryNum nextEntry = 0;

	Index* directoryIndex;

	char emptyEntries;
	char found;
};
