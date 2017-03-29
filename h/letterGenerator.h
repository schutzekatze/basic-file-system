#pragma once

#include <vector>
using std::vector;

class LetterGenerator
{
public:

	LetterGenerator();
	char getLetter();
	void returnLetter(char c);
	bool empty();

private:

	vector<char> letters;

};