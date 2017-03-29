#include "letterGenerator.h"

LetterGenerator::LetterGenerator()
{
	for (char c = 'Z'; c >= 'A'; c--)
	{
		letters.push_back(c);
	}
}

char LetterGenerator::getLetter()
{
	char c = letters.back();
	letters.pop_back();
	return c;
}

void LetterGenerator::returnLetter(char c)
{
	letters.push_back(c);
}

bool LetterGenerator::empty() { return letters.empty(); }