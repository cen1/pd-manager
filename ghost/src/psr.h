
#ifndef PSR_H
#define PSR_H

#include <vector>

typedef pair<string,double> PairedPlayerRating;

//
// CPSR
//

class CPSR
{
private:
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

	bool m_NeedRecalculate;				// if this is true we need to recalculate properties below before using them because they are not accurate anymore

	vector<PairedPlayerRating> m_team1;
	vector<PairedPlayerRating> m_team2;

	double m_team1avgpsr;
	double m_team2avgpsr;
	double m_team1winPerc;
	double m_team2winPerc;
	vector<double> m_team1kfactors;
	vector<double> m_team2kfactors;
	vector< pair<double, double> > m_team1gainLose;
	vector< pair<double, double> > m_team2gainLose;

public:
	CPSR( );
	~CPSR( );

	void SetBaseKFactor( double nBaseKFactor )					{ m_BaseKFactor = nBaseKFactor; }

	double GetTeamAvgPSR( uint32_t team );
	double GetTeamWinPerc( uint32_t team );
	double GetPlayerKFactor( string player );
	pair<double, double> GetPlayerGainLoss( string player );

	void CalculatePSR( vector<PairedPlayerRating> &team1, vector<PairedPlayerRating> &team2, uint32_t maxWinChanceDiffGainContant);
	void CalculatePSR_New(vector<PairedPlayerRating>& team1, vector<PairedPlayerRating>& team2);

	double GetStartRating( )		{ return 1500; }
};

#endif
