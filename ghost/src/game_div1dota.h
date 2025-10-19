/*

   Copyright [2008] [Trevor Hogan]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

*/

#ifndef GAME_DIV1DOTA_H
#define GAME_DIV1DOTA_H

#include "game_base.h"
#include "ghostdb.h"

//
// CBalanceSlot
//

class CBalanceSlot {
  public:
	CBalanceSlot(bool nLocked, unsigned char nPID, unsigned char nTeam, double nRating)
	  : m_Locked(nLocked)
	  , m_PID(nPID)
	  , m_Team(nTeam)
	  , m_Rating(nRating)
	{
	}
	~CBalanceSlot() {}

	bool m_Locked;
	unsigned char m_PID;
	unsigned char m_Team;
	double m_Rating;
};

//
// CDIV1DotAPlayer
//

class CDIV1DotAPlayer : public CDotAPlayer {
  private:
	double m_Rating;
	bool m_FFVote;
	bool m_Banned;
	bool m_RecvNegativePSR;
	bool m_Locked;

  public:
	CDIV1DotAPlayer(CBaseGame* nGame, unsigned char nPID, string nName, uint32_t nServerID, double nRating, bool nLocked)
	  : CDotAPlayer(nGame, nPID, nName, nServerID)
	  , m_Rating(nRating)
	  , m_FFVote(false)
	  , m_Banned(false)
	  , m_RecvNegativePSR(false)
	  , m_Locked(nLocked)
	{
	}
	~CDIV1DotAPlayer() {}

	double GetRating() { return m_Rating; }
	bool GetFFVote() { return m_FFVote; }
	bool GetBanned() { return m_Banned; }
	bool GetRecvNegativePSR() { return m_RecvNegativePSR; }
	bool GetLocked() { return m_Locked; }

	void SetFFVote(bool nFFVote) { m_FFVote = nFFVote; }
	void SetBanned(bool nBanned) { m_Banned = nBanned; }
	void SetRecvNegativePSR(bool nRecvNegativePSR) { m_RecvNegativePSR = nRecvNegativePSR; }
	void SetLocked(bool nLocked) { m_Locked = nLocked; }
};

//
// CDiv1DotAGame
//

class CDBDotAPlayer;
class CPSR;

class CDiv1DotAGame : public CBaseGame {
  protected:
	list<CDIV1DotAPlayer*> m_DotAPlayers;
	string m_Mode1;
	string m_Mode2;
	uint32_t m_Winner;
	uint32_t m_Min;
	uint32_t m_Sec;
	uint32_t m_CreepsSpawnedTime;
	uint32_t m_TreeLowHPTime;
	uint32_t m_ThroneLowHPTime;
	bool m_CreepsSpawned;
	bool m_TreeLowHP;
	bool m_ThroneLowHP;
	uint32_t m_MinPSR;
	uint32_t m_MaxPSR;
	uint32_t m_MinGames;
	uint32_t m_MaxWinChanceDiffGainConstant = 0;
	uint32_t m_MaxWinChanceDiff = 0;
	uint32_t m_MaxWLDiff = 0;

	// CollectDotAStats variables describe the time when game result was determined (when !rmk or !ff passed, tree/throne destroyed, game became a draw because of the leaver)

	bool m_CollectDotAStats;			 // if this is false, DotA stats will be parsed but no more stats will be saved in DotA player objects
	uint32_t m_CollectDotAStatsOverTime; // GetTime when we stoped collecting DotA stats

	CPSR* m_PSR;
	uint32_t m_SentinelVoteFFStartedTime;
	uint32_t m_ScourgeVoteFFStartedTime;
	bool m_SentinelVoteFFStarted;
	bool m_ScourgeVoteFFStarted;
	bool m_FF;
	bool m_LadderGameOver;
	bool m_AutoBanOn;
	bool m_PlayerLeftDuringLoading;
	unsigned char m_Observers;
	string m_CachedBalanceString;
	string m_LastVoteKickedPlayerName;

	// when this goes to zero, it means one side (lvl 3 tower and both baracks) of the sentinel/scourge base is destroyed

	uint32_t m_SentinelTopRax;
	uint32_t m_SentinelMidRax;
	uint32_t m_SentinelBotRax;

	uint32_t m_ScourgeTopRax;
	uint32_t m_ScourgeMidRax;
	uint32_t m_ScourgeBotRax;

	map<unsigned char, uint32_t> m_SentinelTopRaxV;
	map<unsigned char, uint32_t> m_SentinelMidRaxV;
	map<unsigned char, uint32_t> m_SentinelBotRaxV;

	map<unsigned char, uint32_t> m_ScourgeTopRaxV;
	map<unsigned char, uint32_t> m_ScourgeMidRaxV;
	map<unsigned char, uint32_t> m_ScourgeBotRaxV;

  public:
	CDiv1DotAGame(CGHost* nGHost, CMap* nMap, CSaveGame* nSaveGame, uint16_t nHostPort, unsigned char nGameState, string nGameName, string nOwnerName, string nCreatorName, string nCreatorServer);
	virtual ~CDiv1DotAGame();

	list<CDIV1DotAPlayer*>* GetDotaPlayers() { return &m_DotAPlayers; }
	virtual bool EventPlayerAction(CGamePlayer* player, CIncomingAction* action);
	virtual void EventPlayerBotCommand2(CGamePlayer* player, string command, string payload, bool rootadmin, bool admin, bool owner);
	virtual void EventPlayerEnteredLobby(CGamePlayer* player);
	virtual void EventPlayerDeleted(CGamePlayer* player);
	virtual void EventGameStarted();
	virtual void EventGameLoaded();

	virtual void SwapSlots(unsigned char SID1, unsigned char SID2);
	virtual void ChangeOwner(string name);
	virtual bool IsGameDataSaved();
	virtual void SaveGameData();

	CDIV1DotAPlayer* GetDIV1DotAPlayerFromPID(unsigned char PID);
	CDIV1DotAPlayer* GetDIV1DotAPlayerFromSID(unsigned char SID);
	CDIV1DotAPlayer* GetDIV1DotAPlayerFromLobbyColor(unsigned char color);
	void BalanceSlots();
	void BalanceSlots2(const set<unsigned char>& locked_slots);
	unsigned int BalanceSlots2Recursive(list<CBalanceSlot>& slots_to_balance);

	// min, max, games
	void CheckMinPSR();
	void CheckMaxPSR();
	void CheckMinGames(string User);
	std::tuple<double, double, double> getWLDiff();

	void subtractSentinelTopRaxV(CGamePlayer* reporter);
	void subtractSentinelMidRaxV(CGamePlayer* reporter);
	void subtractSentinelBotRaxV(CGamePlayer* reporter);

	void subtractScourgeTopRaxV(CGamePlayer* reporter);
	void subtractScourgeMidRaxV(CGamePlayer* reporter);
	void subtractScourgeBotRaxV(CGamePlayer* reporter);

	void recalcPSR();
};

#endif
