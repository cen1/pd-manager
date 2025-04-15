#ifndef TEST_H
#define TEST_H

#include "psr.h"
#include <iostream>

using std::cout;
using std::endl;

class PsrTestSample
{
private:
	vector<vector<pair<string, CDBDiv1DPS*>>> m_Samples;
	vector<string> descriptions;

public:
	PsrTestSample() {}
	~PsrTestSample() {}

	void addSample(vector<pair<string, CDBDiv1DPS*>> players, string description) {
		m_Samples.push_back(players);
		descriptions.push_back(description);
	}

	string getSampleDescription(int i) {
		if (i > 0 && i < descriptions.size()) {
			return descriptions[i];
		}
		else {
			return "";
		}
	}

	vector<pair<string, CDBDiv1DPS*>> getPlayerList(int index) {
		vector<pair<string, CDBDiv1DPS*>> players;
		for (pair<string, CDBDiv1DPS*> player : m_Samples[index]) {
			players.push_back(make_pair(player.first + " (" + std::to_string(index) + ")", player.second));
		}
		return players;
	}

	int size() {
		return m_Samples.size();
	}

	//Returns a newly created cpsr object using the test sample i 
	CPSR generateCPSRFromSample(int i) {
		CPSR cpsr1;

		vector<unsigned char> pids = { 1, 2, 3, 4, 5, 7, 8, 9, 10, 11 };

		vector<PairedPlayerRating> team1;
		vector<PairedPlayerRating> team2;
		vector<CDIV1DotAPlayer *> dotaPlayers;

		for (int j = 0; j < 10; j++) {
			//*nGame, unsigned char nPID, string nName, uint32_t nServerID, double nRating, bool nLocked
			CDIV1DotAPlayer *p = new CDIV1DotAPlayer(NULL, pids[j], m_Samples[i][j].first, 0, m_Samples[i][j].second->GetRating(), false);

			//Hardcode : player 1 is locked (host)
			if (j == 0) {
				p->SetLocked(true);
			}

			if (j<=4)
				team1.push_back(make_pair(p->GetName(), p->GetRating()));
			else
				team2.push_back(make_pair(p->GetName(), p->GetRating()));

			dotaPlayers.push_back(p);
		}

		cpsr1.CalculatePSR(team1, team2, 0);

		for (auto it = dotaPlayers.begin(); it != dotaPlayers.end(); ++it) {

			double gain = cpsr1.GetPlayerGainLoss((*it)->GetName()).first;
			double loss = cpsr1.GetPlayerGainLoss((*it)->GetName()).second;

			cout << (*it)->GetName() << ": " << UTIL_ToString((*it)->GetRating(), 0) + ", +" 
				+ UTIL_ToString(gain, 0) + "/-" 
				+ UTIL_ToString(loss, 0)
				<< std::endl;
		}

		return cpsr1;
	}

	vector<vector<pair<string, CDBDiv1DPS*>>> getSamples() {
		return m_Samples;
	}

};

#endif