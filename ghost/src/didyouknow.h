#pragma once
#include <string>
#include <vector>
#include <memory>
#include <random>

using namespace std;

class DidYouKnow {

private:
	vector<string> m_phrases;

	std::random_device rd;
	std::mt19937 engine;
	unique_ptr<uniform_int_distribution<int>> dist;
public:
	DidYouKnow();
	~DidYouKnow();

	string getRandomPhrase();
};