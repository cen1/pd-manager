
#ifndef PSR_H
#define PSR_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using std::pair;
using std::string;
using std::vector;

typedef pair<string, double> PairedPlayerRating;

//
// CPSR
//

class CPSR {
  private:
	/*
	var psf_baseKFactor = 20;
	var psf_gammaCurveK = 18;
	var psf_gammaCurveRange = 200;
	var psf_gammaCurveTheta = 5.0000;
	var psf_KFactorScale = 8;
	var psf_logisticPredictionScale = 80.0000;
	var psf_maxKFactor = 40;
	var psf_medianScalingRank = 1600;
	var psf_minKFactor = 10;
	var psf_teamRankWeighting = 6.5000;
	*/

	/*uint32_t m_BaseKFactor;
	uint32_t m_GammaCurveK;
	uint32_t m_GammaCurveRange;
	double m_GammaCurveTheta;
	uint32_t m_KFactorScale;
	double m_LogisticPredictionScale;
	uint32_t m_MaxKFactor;
	uint32_t m_MedianScalingRank;
	uint32_t m_MinKFactor;
	double m_TeamRankWeighting;*/

	double m_BaseKFactor;
	double m_GammaCurveK;
	double m_GammaCurveRange;
	double m_GammaCurveTheta;
	double m_KFactorScale;
	double m_LogisticPredictionScale;
	double m_MaxKFactor;
	double m_MedianScalingRank;
	double m_MinKFactor;
	double m_TeamRankWeighting;

	double m_ConstWinGain;
	double m_ConstLoseGain;

	bool m_NeedRecalculate; // if this is true we need to recalculate properties below before using them because they are not accurate anymore

	vector<PairedPlayerRating> m_team1;
	vector<PairedPlayerRating> m_team2;

	double m_team1avgpsr;
	double m_team2avgpsr;
	double m_team1winPerc;
	double m_team2winPerc;
	vector<double> m_team1kfactors;
	vector<double> m_team2kfactors;
	vector<pair<double, double>> m_team1gainLose;
	vector<pair<double, double>> m_team2gainLose;

  public:
	CPSR();
	~CPSR();

	void SetBaseKFactor(double nBaseKFactor) { m_BaseKFactor = nBaseKFactor; }

	double GetTeamAvgPSR(uint32_t team);
	double GetTeamWinPerc(uint32_t team);
	double GetPlayerKFactor(string player);
	pair<double, double> GetPlayerGainLoss(string player);

	void CalculatePSR(vector<PairedPlayerRating>& team1, vector<PairedPlayerRating>& team2, uint32_t maxWinChanceDiffGainConstant);
	void CalculatePSR_New(vector<PairedPlayerRating>& team1, vector<PairedPlayerRating>& team2);
	// void NeedRecalculate( vector< pair<string, double> > team1, vector< pair<string, double> > team2 );

	vector<pair<double, double>> getTeam1gainLose();
	vector<pair<double, double>> getTeam2gainLose();
};

#endif
