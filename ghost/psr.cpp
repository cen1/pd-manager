
#include "ghost.h"
#include "psr.h"
#include "util.h"

#include <cmath>

//
// CPSR
//

CPSR::CPSR()
{
	m_BaseKFactor = 20.0;
	m_GammaCurveK = 18.0;
	m_GammaCurveRange = 200;
	m_GammaCurveTheta = 5.0000;
	m_KFactorScale = 8.0;
	m_LogisticPredictionScale = 80.0000;
	m_MaxKFactor = 40.0;
	m_MedianScalingRank = 1600.0;
	m_MinKFactor = 10.0;
	m_TeamRankWeighting = 6.5000;

	m_ConstWinGain = 5.0;
	m_ConstLoseGain = 5.0;

	m_team1avgpsr = 0.0;
	m_team2avgpsr = 0.0;
	m_team1winPerc = 0.0;
	m_team2winPerc = 0.0;

	m_team1kfactors = vector<double>(5, 0.0);
	m_team2kfactors = vector<double>(5, 0.0);
	m_team1gainLose = vector< pair<double, double> >(5, make_pair(0.0, 0.0));
	m_team2gainLose = vector< pair<double, double> >(5, make_pair(0.0, 0.0));
}

CPSR :: ~CPSR()
{

}

double CPSR::GetTeamAvgPSR(unsigned team)
{
	// team, sentinel = 0, scourge = 1

	if (team == 0)
		return m_team1avgpsr;
	else
		return m_team2avgpsr;
}

double CPSR::GetTeamWinPerc(unsigned team)
{
	// team, sentinel = 0, scourge = 1

	if (team == 0)
		return m_team1winPerc;
	else
		return m_team2winPerc;
}

double CPSR::GetPlayerKFactor(string name)
{
	for (unsigned int i = 0; i < m_team1.size(); ++i)
	{
		if (m_team1[i].first == name)
			return m_team1kfactors[i];
	}

	for (unsigned int i = 0; i < m_team2.size(); ++i)
	{
		if (m_team2[i].first == name)
			return m_team2kfactors[i];
	}

	return 0.0;
}

pair<double, double> CPSR::GetPlayerGainLoss(string name)
{
	for (unsigned int i = 0; i < m_team1.size(); ++i)
	{
		if (m_team1[i].first == name)
			return m_team1gainLose[i];
	}

	for (unsigned int i = 0; i < m_team2.size(); ++i)
	{
		if (m_team2[i].first == name)
			return m_team2gainLose[i];
	}

	return make_pair(0.0, 0.0);
}

void CPSR::CalculatePSR(vector<PairedPlayerRating> &team1, vector<PairedPlayerRating> &team2, uint32_t maxWinChanceDiffGainConstant)
{
	m_team1 = team1;
	m_team2 = team2;

	m_team1avgpsr = 0.0;
	m_team2avgpsr = 0.0;
	m_team1winPerc = 0.0;
	m_team2winPerc = 0.0;

	//m_team1kfactors.clear( );
	//m_team2kfactors.clear( );

	//m_team1gainLose.clear( );
	//m_team2gainLose.clear( );

	vector<double> team1weights(5, 0.0);
	vector<double> team2weights(5, 0.0);

	double team1weight = 0;
	double team2weight = 0;
	double team1totalpsr = 0;
	double team2totalpsr = 0;

	for (unsigned int i = 0; i < m_team1.size(); ++i)
	{
		team1weights[i] = pow(m_team1[i].second, m_TeamRankWeighting);
		team1weight += team1weights[i];
		team1totalpsr += m_team1[i].second;
	}

	for (unsigned int i = 0; i < m_team2.size(); ++i)
	{
		team2weights[i] = pow(m_team2[i].second, m_TeamRankWeighting);
		team2weight += team2weights[i];
		team2totalpsr += m_team2[i].second;
	}

	m_team1avgpsr = team1totalpsr / m_team1.size();
	m_team2avgpsr = team2totalpsr / m_team2.size();

	double team1rating = pow(team1weight, (double)1.0 / m_TeamRankWeighting);
	double team2rating = pow(team2weight, (double)1.0 / m_TeamRankWeighting);

	double diff = team1rating - team2rating;

	double E = 2.7182;

	double team1winprobability = 1.0 / (1.0 + pow(E, ((-1.0 * diff) / m_LogisticPredictionScale)));
	double team2winprobability = 1.0 - team1winprobability;

	for (unsigned int i = 0; i < m_team1.size(); ++i)
	{
		m_team1kfactors[i] = ((m_MedianScalingRank - m_team1[i].second) / m_KFactorScale) + m_BaseKFactor;
		m_team1kfactors[i] = m_team1kfactors[i] > m_MaxKFactor ? m_MaxKFactor : m_team1kfactors[i];
		m_team1kfactors[i] = m_team1kfactors[i] > m_MinKFactor ? m_team1kfactors[i] : m_MinKFactor;

		double distancefromteamavgpsr = m_team1[i].second - 50 - m_team1avgpsr;

		if (distancefromteamavgpsr > 100)
			distancefromteamavgpsr = 100;

		if (distancefromteamavgpsr > 0 && distancefromteamavgpsr <= 100)
		{
			double implayingwithnewbiesfactor = (100.0 - distancefromteamavgpsr) / 100;
			m_team1kfactors[i] = implayingwithnewbiesfactor * m_team1kfactors[i];
		}
	}

	for (unsigned int i = 0; i < m_team2.size(); ++i)
	{
		m_team2kfactors[i] = ((m_MedianScalingRank - m_team2[i].second) / m_KFactorScale) + m_BaseKFactor;
		m_team2kfactors[i] = m_team2kfactors[i] > m_MaxKFactor ? m_MaxKFactor : m_team2kfactors[i];
		m_team2kfactors[i] = m_team2kfactors[i] > m_MinKFactor ? m_team2kfactors[i] : m_MinKFactor;

		double distancefromteamavgpsr = m_team2[i].second - 50 - m_team2avgpsr;

		if (distancefromteamavgpsr > 100)
			distancefromteamavgpsr = 100;

		if (distancefromteamavgpsr > 0 && distancefromteamavgpsr <= 100)
		{
			double implayingwithnewbiesfactor = (100.0 - distancefromteamavgpsr) / 100;
			m_team2kfactors[i] = implayingwithnewbiesfactor * m_team2kfactors[i];
		}
	}

	//WinP
	m_team1winPerc = (team1winprobability * 100);
	m_team2winPerc = 100 - m_team1winPerc;

	double winPercDiff = m_team1winPerc - m_team2winPerc;
	bool rejectLowerGainLimitForT1 = winPercDiff>0 && maxWinChanceDiffGainConstant > 0 && abs(winPercDiff) > maxWinChanceDiffGainConstant;
	bool rejectLowerGainLimitForT2 = winPercDiff<0 && maxWinChanceDiffGainConstant > 0 && abs(winPercDiff) > maxWinChanceDiffGainConstant;

	//Final +/-
	for (unsigned int i = 0; i < m_team1.size(); ++i) {
		double im_win = ceil(team2winprobability * m_team1kfactors[i]);
		double im_lose = floor(team1winprobability * m_team1kfactors[i]);

		//lose points are also positive
		if (!rejectLowerGainLimitForT1) {
			if (im_win < 1.0) {
				im_win = 1.0;
			}
			if (im_lose < 1.0) {
				im_lose = 1.0;
			}
		}

		m_team1gainLose[i] = make_pair(im_win, im_lose);

		CONSOLE_Print("[" + UTIL_ToString(i) + "] (" + UTIL_ToString(m_team1[i].second, 0) + ") "
			+ UTIL_ToString(im_win, 0) + "/-" + UTIL_ToString(im_lose, 0));
	}

	for (unsigned int i = 0; i < m_team2.size(); ++i) {
		double im_win = ceil(team1winprobability * m_team2kfactors[i]);
		double im_lose = floor(team2winprobability * m_team2kfactors[i]);

		//lose points are also positive
		if (!rejectLowerGainLimitForT2) {
			if (im_win < 1.0) {
				im_win = 1.0;
			}
			if (im_lose < 1.0) {
				im_lose = 1.0;
			}
		}
		//Lose fix
		if (im_win == 1.0 && im_lose == 0.0) im_lose = 1.0;

		m_team2gainLose[i] = make_pair(im_win, im_lose);

		CONSOLE_Print("[" + UTIL_ToString(i) + "] (" + UTIL_ToString(m_team2[i].second, 0) + ") "
			+ UTIL_ToString(im_win, 0) + "/-" + UTIL_ToString(im_lose, 0));
	}

	m_NeedRecalculate = false;
}

vector<pair<double, double> > CPSR::getTeam1gainLose() {
	return m_team1gainLose;
}

vector<pair<double, double> > CPSR::getTeam2gainLose() {
	return m_team2gainLose;
}

void CPSR::CalculatePSR_New(vector<PairedPlayerRating>& team1, vector<PairedPlayerRating>& team2)
{
	if (team1.size() > 5 || team2.size() > 5) {
		return;
	}

	m_team1 = team1;
	m_team2 = team2;

	m_team1avgpsr = 0.0;
	m_team2avgpsr = 0.0;
	m_team1winPerc = 0.0;
	m_team2winPerc = 0.0;

	m_team1gainLose.clear();
	m_team2gainLose.clear();

	vector<double> team1weights(5, 0.0);
	vector<double> team2weights(5, 0.0);
	double team1weight = 0;
	double team2weight = 0;
	double team1totalpsr = 0;
	double team2totalpsr = 0;

	//Team 1 weight + total PSR
	for (unsigned int i = 0; i < m_team1.size(); ++i)
	{
		team1weights[i] = pow(m_team1[i].second, m_TeamRankWeighting);
		team1weight += team1weights[i];
		team1totalpsr += m_team1[i].second;
	}
	//Team 2 weight + total PSR
	for (unsigned int i = 0; i < m_team2.size(); ++i)
	{
		team2weights[i] = pow(m_team2[i].second, m_TeamRankWeighting);
		team2weight += team2weights[i];
		team2totalpsr += m_team2[i].second;
	}

	//Team 1 & 2 average PSR
	m_team1avgpsr = m_team1.size() > 0 ? team1totalpsr / m_team1.size() : 0;
	m_team2avgpsr = m_team2.size() > 0 ? team2totalpsr / m_team2.size() : 0;

	//Team 1 & 2 rating
	double team1rating = pow(team1weight, (double)1.0 / m_TeamRankWeighting);
	double team2rating = pow(team2weight, (double)1.0 / m_TeamRankWeighting);

	double diff = team1rating - team2rating;

	double E = 2.7182;

	//Team 1 & 2 win probabilities
	double team1winprobability = 1.0 / (1.0 + pow(E, ((-1.0 * diff) / m_LogisticPredictionScale)));
	double team2winprobability = 1.0 - team1winprobability;

	//Add both teams and their corresponding variables each to its own vector for simplicity
	vector<double> playerPSR;
	vector<pair<double, double>> allPlayersGainLose;
	vector<double> ownTeamWinProbabilities;
	vector<double> enemyTeamWinProbabilities;
	vector<double> ownTeamAveragePSRValues;
	vector<double> playerWinPools(10, 0);
	vector<double> playerLosePools(10, 0);
	vector<double> WinPenaltyFactors(10, 1);
	vector<double> LoseReductionFactors(10, 1);

	for (unsigned int i = 0; i < m_team1.size(); ++i) {
		playerPSR.push_back(m_team1[i].second);
		ownTeamWinProbabilities.push_back(team1winprobability);
		enemyTeamWinProbabilities.push_back(team2winprobability);
		ownTeamAveragePSRValues.push_back(m_team1avgpsr);
	}
	for (unsigned int i = 0; i < m_team2.size(); ++i) {
		playerPSR.push_back(m_team2[i].second);
		ownTeamWinProbabilities.push_back(team2winprobability);
		enemyTeamWinProbabilities.push_back(team1winprobability);
		ownTeamAveragePSRValues.push_back(m_team2avgpsr);
	}

	//Distance within which no reduction can happen
	double safeDistance;

	//Temporal variable to hold a reduction factor
	double distanceReductionFactor;

	//Temporal variable to hold max distance to work with considerationDistance
	double maxDistance;

	//Calculate base PSR for both teams
	for (unsigned int i = 0; i < playerPSR.size(); ++i)
	{
		//Calculate basic PSR Pool, based on distance between player's PSR and expected ladder median (Preset)
		double basePSR = ((m_MedianScalingRank - playerPSR[i]) / m_KFactorScale) + m_BaseKFactor;

		//Retain kFactor within [10, 40] (Preset in constants). This way PSR gained/lost is normally in [5, 20] interval
		basePSR = basePSR > m_MaxKFactor ? m_MaxKFactor : basePSR;
		basePSR = basePSR > m_MinKFactor ? basePSR : m_MinKFactor;

		//Basic win pool of the player
		playerWinPools[i] = basePSR;

		//Distance between teamPSR and player's PSR
		double distanceFromAverageTeamPSR = ownTeamAveragePSRValues[i] - playerPSR[i];

		if (distanceFromAverageTeamPSR < 0) {
			//If player has higher PSR than average in his team (TeamPSR), apply gain && loss reduction factors

			//Basic lose pool
			playerLosePools[i] = basePSR;

			//Calculate player's PSR distance to teamPSR
			//If PSR within 50 range of average teamPSR, no reduction

			//For Win Factor
			//PSR gained in case of victory will be reduced, due to player's own high rating (related to teamPSR)
			safeDistance = playerPSR[i] - 50 - ownTeamAveragePSRValues[i];

			//Sets the rate of the reduction, the lower maxDistance the bigger the rate, faster reduction
			maxDistance = 100;

			safeDistance = safeDistance > maxDistance
				? maxDistance : safeDistance;

			if (safeDistance > 0 && safeDistance <= maxDistance) {
				//Factor in (0, 1], the lower factor, the less PSR will be won in case of victory
				//Bigger distance means lower factor
				WinPenaltyFactors[i] = (maxDistance - safeDistance) / maxDistance;
			}

			//Lose Factor
			//PSR lost in case of defeat will be reduced, due to player's own high rating (related to teamPSR)
			safeDistance = playerPSR[i] - 50 - ownTeamAveragePSRValues[i];

			//Sets the "Rate" of reduction, the lower maxDistance the bigger the Rate, faster reduction
			maxDistance = 150;

			safeDistance = safeDistance > maxDistance
				? maxDistance : safeDistance;

			if (safeDistance > 0 && safeDistance <= maxDistance) {
				//Factor in (0, 1], the lower factor, the less PSR will be lost in case of defeat
				//Rate is lower which means smaller reduction "over given distance"
				//Player can lose more PSR than gain if his rating is significantly higher than teamPSR due
				LoseReductionFactors[i] = (maxDistance - safeDistance) / maxDistance;
			}

		}
		else {
			//If player's PSR is lower than teamPSR

			//Construct the mirrored graph which intersects the original graph in teamPSR point
			//to form a "triangle"
			basePSR = ((m_MedianScalingRank - 2 * ownTeamAveragePSRValues[i] + playerPSR[i]) / m_KFactorScale) + m_BaseKFactor;

			//Retain kFactor within [10, 40] (Preset in constants). This way PSR gained/lost is normally in [5, 20] interval
			basePSR = basePSR > m_MaxKFactor ? m_MaxKFactor : basePSR;
			basePSR = basePSR > m_MinKFactor ? basePSR : m_MinKFactor;

			//player's lose pool is calculated using the mirrored graph
			playerLosePools[i] = basePSR;

			//Reduction factor for PSR loss to mirror the original reduction factor
			safeDistance = ownTeamAveragePSRValues[i] - playerPSR[i] - 50;

			maxDistance = 150;

			safeDistance = safeDistance > maxDistance
				? maxDistance : safeDistance;

			if (safeDistance > 0 && safeDistance <= maxDistance) {
				//Set mirrored reduction factor for PSR loss
				//Win pool isn't reduced which makes the player win more PSR as his rating is lower than team's average
				LoseReductionFactors[i] = (maxDistance - safeDistance) / maxDistance;
			}
		}
	}

	//Calculate both teams Win/Lose points
	for (unsigned int i = 0; i < playerPSR.size(); ++i) {
		double im_win = ceil(enemyTeamWinProbabilities[i] * playerWinPools[i] * WinPenaltyFactors[i]);
		double im_lose = ceil(ownTeamWinProbabilities[i] * playerLosePools[i] * LoseReductionFactors[i]);

		//lose points are also positive
		if (im_win < 1.0) {
			im_win = 1.0;
		}
		if (im_lose < 1.0) {
			im_lose = 1.0;
		}

		allPlayersGainLose.push_back(make_pair(im_win, im_lose));
	}

	//Set values for team 1
	for (unsigned int i = 0; i < team1.size(); i++) {
		m_team1gainLose.push_back(allPlayersGainLose[i]);
	}

	//Set values for team 2
	for (unsigned int i = team1.size(); i < team1.size() + team2.size(); i++) {
		m_team2gainLose.push_back(allPlayersGainLose[i]);
	}

	m_team1winPerc = (team1winprobability * 100);
	m_team2winPerc = 100 - m_team1winPerc;

	m_NeedRecalculate = false;
}