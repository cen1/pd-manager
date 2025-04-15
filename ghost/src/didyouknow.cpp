#include "didyouknow.h"
#include <algorithm>
#include <fstream>
#include "util.h"

DidYouKnow::DidYouKnow() : engine(rd()) {

	ifstream in;
	in.open("didyouknow.txt");

	if (in.fail()) {
		CONSOLE_Print("[DYK] warning - unable to read file didyouknow.txt");
	}
	else {
		string line;

		while (!in.eof()) {
			std::getline(in, line);
			if (!line.empty()) {
				m_phrases.push_back(line);
			}
		}
	}

	size_t max = m_phrases.size();
	if (max!=0) {
		max = max - 1;
		dist = make_unique<uniform_int_distribution<int>>(0, max);
	}	
}

DidYouKnow::~DidYouKnow() {}

string DidYouKnow::getRandomPhrase() {
	if (m_phrases.size() == 0) {
		return "Contact an admin to stop this spam, something is misconfigured.";
	}
	int r = dist.get()->operator()(this->engine);
	return m_phrases[r];
}