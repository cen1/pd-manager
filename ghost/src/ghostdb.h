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

#ifndef GHOSTDB_H
#define GHOSTDB_H

#include <cstdint>
#include <map>
#include <string>

using namespace std;

//
// CGHostDB
//

class CBaseCallable;
class CDBPlayer;
class CDBGamePlayer;
class CDBGPS;
class CDBDiv1DPS;
class CBaseGame;
class CGamePlayer;

//
// Callables
//

class CBaseCallable {
  protected:
	string m_Error;
	bool m_Ready;
	uint32_t m_StartTicks;
	uint32_t m_EndTicks;

  public:
	CBaseCallable()
	  : m_Error()
	  , m_Ready(false)
	  , m_StartTicks(0)
	  , m_EndTicks(0)
	{
	}
	virtual ~CBaseCallable() {}

	// virtual void operator( )( ) { }

	// virtual void Init( );
	// virtual void Close( );

	virtual string GetError() { return m_Error; }
	virtual bool GetReady() { return m_Ready; }
	virtual void SetReady(bool nReady) { m_Ready = nReady; }
	virtual uint32_t GetElapsed() { return m_Ready ? m_EndTicks - m_StartTicks : 0; }
};

class CCallablePlayerCheck : virtual public CBaseCallable {
  protected:
	string m_Name;
	string m_Server;
	CDBPlayer* m_Result;

  public:
	CCallablePlayerCheck(string nName, string nServer)
	  : CBaseCallable()
	  , m_Name(nName)
	  , m_Server(nServer)
	  , m_Result(NULL)
	{
	}
	virtual ~CCallablePlayerCheck() {}

	virtual string GetName() { return m_Name; }
	virtual string GetServer() { return m_Server; }
	virtual CDBPlayer* GetResult() { return m_Result; }
	virtual void SetResult(CDBPlayer* nResult) { m_Result = nResult; }
};

//
// CDBPlayer
//

class CDBPlayer {
  private:
	CDBGPS* m_GPS;
	CDBDiv1DPS* m_Div1DPS;
	string m_Server;
	string m_Name;
	string m_From;
	string m_LongFrom;
	uint32_t m_AccessLevel;
	bool m_Banned;

  public:
	CDBPlayer(CDBGPS* nGPS, CDBDiv1DPS* nDiv1DPS, string nServer, string nName, string nFrom, string nLongFrom, uint32_t nAccessLevel, bool nBanned)
	  : m_GPS(nGPS)
	  , m_Div1DPS(nDiv1DPS)
	  , m_Server(nServer)
	  , m_Name(nName)
	  , m_From(nFrom)
	  , m_LongFrom(nLongFrom)
	  , m_AccessLevel(nAccessLevel)
	  , m_Banned(nBanned)
	{
	}
	~CDBPlayer();

	CDBGPS* GetGPS() { return m_GPS; }
	CDBDiv1DPS* GetDiv1DPS() { return m_Div1DPS; }
	string GetServer() { return m_Server; }
	string GetName() { return m_Name; }
	string GetFrom() { return m_From; }
	string GetLongFrom() { return m_LongFrom; }
	uint32_t GetAccessLevel() { return m_AccessLevel; }
	bool GetIsAdmin() { return m_AccessLevel > 1000; }
	bool GetIsBanned() { return m_Banned; }

	void SetFrom(string nFrom) { m_From = nFrom; }
	void SetLongFrom(string nLongFrom) { m_LongFrom = nLongFrom; }
};

//
// CDBGamePlayer
//

class CDBGamePlayer {
  private:
	string m_Name;
	string m_IP;
	uint32_t m_Spoofed;
	string m_SpoofedRealm;
	uint32_t m_Reserved;
	uint32_t m_LoadingTime;
	uint32_t m_Left;
	string m_LeftReason;
	uint32_t m_Team;
	uint32_t m_Colour;
	bool m_Banned;
	bool m_GProxy;

  public:
	CDBGamePlayer(string nName, string nIP, uint32_t nSpoofed, string nSpoofedRealm, uint32_t nReserved, uint32_t nLoadingTime, uint32_t nLeft, string nLeftReason, uint32_t nTeam, uint32_t nColour, bool nGProxy)
	  : m_Name(nName)
	  , m_IP(nIP)
	  , m_Spoofed(nSpoofed)
	  , m_SpoofedRealm(nSpoofedRealm)
	  , m_Reserved(nReserved)
	  , m_LoadingTime(nLoadingTime)
	  , m_Left(nLeft)
	  , m_LeftReason(nLeftReason)
	  , m_Team(nTeam)
	  , m_Colour(nColour)
	  , m_GProxy(nGProxy)
	  , m_Banned(false)
	{
	}
	~CDBGamePlayer() {}

	string GetName() { return m_Name; }
	string GetIP() { return m_IP; }
	uint32_t GetSpoofed() { return m_Spoofed; }
	string GetSpoofedRealm() { return m_SpoofedRealm; }
	uint32_t GetReserved() { return m_Reserved; }
	uint32_t GetLoadingTime() { return m_LoadingTime; }
	uint32_t GetLeft() { return m_Left; }
	string GetLeftReason() { return m_LeftReason; }
	uint32_t GetTeam() { return m_Team; }
	uint32_t GetColour() { return m_Colour; }
	bool GetBanned() { return m_Banned; }
	bool GetGProxy() { return m_GProxy; }

	void SetLoadingTime(uint32_t nLoadingTime) { m_LoadingTime = nLoadingTime; }
	void SetLeft(uint32_t nLeft) { m_Left = nLeft; }
	void SetLeftReason(string nLeftReason) { m_LeftReason = nLeftReason; }
	void SetBanned(bool nBanned) { m_Banned = nBanned; }
};

//
// CDBGPS (game player summary)
//

class CDBGPS {
  private:
	uint32_t m_TotalGames; // total number of games played

  public:
	CDBGPS(uint32_t nTotalGames)
	  : m_TotalGames(nTotalGames)
	{
	}
	~CDBGPS() {}

	uint32_t GetTotalGames() { return m_TotalGames; }
};

//
// CDotAPlayer
//

class CDotAPlayer {
  private:
	CBaseGame* m_Game;
	unsigned char m_PID;
	unsigned char m_LobbyColor; // initialized to 255, will be set to CGamePlayer color when game begins to load
	unsigned char m_NewColor;	// initialized to 255, will be set to CGamePlayer color when game begins to load, will be updated when the DotA map sends slot info
	uint32_t m_Level;
	uint32_t m_Kills;
	uint32_t m_Deaths;
	uint32_t m_CreepKills;
	uint32_t m_CreepDenies;
	uint32_t m_Assists;
	uint32_t m_Gold;
	uint32_t m_NeutralKills;
	uint32_t m_TowerKills;
	uint32_t m_RaxKills;
	uint32_t m_CourierKills;
	uint32_t m_ServerID;
	string m_Name;
	string m_Hero;
	string m_Items[6];

	// This data must be voted on to get the correct final count due to possibility of packet tampering
	map<unsigned char, unsigned char> m_NewColorV;
	map<unsigned char, uint32_t> m_LevelV;
	map<unsigned char, uint32_t> m_KillsV;
	map<unsigned char, uint32_t> m_DeathsV;
	map<unsigned char, uint32_t> m_CreepKillsV;
	map<unsigned char, uint32_t> m_CreepDeniesV;
	map<unsigned char, uint32_t> m_AssistsV;
	map<unsigned char, uint32_t> m_NeutralKillsV;
	map<unsigned char, uint32_t> m_TowerKillsV;
	map<unsigned char, uint32_t> m_RaxKillsV;
	map<unsigned char, uint32_t> m_CourierKillsV;

	// all picked up and dropped items separated with new line char \n

	// format: "type time item\n"
	// type:					1 - picked up item, 2 - dropped item
	// time:					seconds since game started
	// item:					4 characters item string

	// example: "1 125 HSLV"
	// type:					picked up item
	// time:					2 minutes 5 seconds
	// item:					HSLV (Healing Salve)

	string m_ItemsLog;

	bool m_HasLeftTheGame;
	bool m_ItemsSent;

  public:
	CDotAPlayer(CBaseGame* nGame, unsigned char nPID, string nName, uint32_t nServerID);
	virtual ~CDotAPlayer();

	unsigned char GetPID() { return m_PID; }
	unsigned char GetLobbyTeam();
	unsigned char GetLobbyColor();
	unsigned char GetCurrentTeam();
	unsigned char GetCurrentColor();
	uint32_t GetLevel() { return m_Level; }
	uint32_t GetKills() { return m_Kills; }
	uint32_t GetDeaths() { return m_Deaths; }
	uint32_t GetCreepKills() { return m_CreepKills; }
	uint32_t GetCreepDenies() { return m_CreepDenies; }
	uint32_t GetAssists() { return m_Assists; }
	uint32_t GetGold() { return m_Gold; }
	uint32_t GetNeutralKills() { return m_NeutralKills; }
	uint32_t GetTowerKills() { return m_TowerKills; }
	uint32_t GetRaxKills() { return m_RaxKills; }
	uint32_t GetCourierKills() { return m_CourierKills; }
	uint32_t GetServerID() { return m_ServerID; }
	string GetName() { return m_Name; }
	string GetHero() { return m_Hero; }
	string GetItem(unsigned int i);
	bool GetHasLeftTheGame() { return m_HasLeftTheGame; }
	bool GetItemsSent() { return m_ItemsSent; }

	void SetLobbyColor(unsigned char nColor) { m_LobbyColor = nColor; }
	void SetNewColor(unsigned char nColor) { m_NewColor = nColor; }
	void SetLevel(uint32_t nLevel) { m_Level = nLevel; }
	void SetKills(uint32_t nKills) { m_Kills = nKills; }
	void SetDeaths(uint32_t nDeaths) { m_Deaths = nDeaths; }
	void SetCreepKills(uint32_t nCreepKills) { m_CreepKills = nCreepKills; }
	void SetCreepDenies(uint32_t nCreepDenies) { m_CreepDenies = nCreepDenies; }
	void SetAssists(uint32_t nAssists) { m_Assists = nAssists; }
	void SetGold(uint32_t nGold) { m_Gold = nGold; }
	void SetNeutralKills(uint32_t nNeutralKills) { m_NeutralKills = nNeutralKills; }
	void SetTowerKills(uint32_t nTowerKills) { m_TowerKills = nTowerKills; }
	void SetRaxKills(uint32_t nRaxKills) { m_RaxKills = nRaxKills; }
	void SetCourierKills(uint32_t nCourierKills) { m_CourierKills = nCourierKills; }
	void SetHero(string nHero) { m_Hero = nHero; }
	void SetItem(unsigned int i, string item);
	void PickUpItem(string item);
	void DropItem(string item);
	void SetHasLeftTheGame(bool nHasLeftTheGame) { m_HasLeftTheGame = nHasLeftTheGame; }
	void SetItemsSent(bool nItemsSent) { m_ItemsSent = nItemsSent; }

	// Voted stats
	void setNewColorV(CGamePlayer* reporter, unsigned char nColor);
	void setLevelV(CGamePlayer* reporter, uint32_t reportedValue);
	void setCreepKillsV(CGamePlayer* reporter, uint32_t reportedValue);
	void setCreepDeniesV(CGamePlayer* reporter, uint32_t reportedValue);
	void setNeutralKillsV(CGamePlayer* reporter, uint32_t reportedValue);
	void setKillsV(CGamePlayer* reporter, uint32_t reportedValue);
	void setDeathsV(CGamePlayer* reporter, uint32_t reportedValue);
	void setAssistsV(CGamePlayer* reporter, uint32_t reportedValue);

	void addKillsV(CGamePlayer* reporter);
	void addDeathsV(CGamePlayer* reporter);
	void addAssistsV(CGamePlayer* reporter);
	void addTowerKillsV(CGamePlayer* reporter);
	void addRaxKillsV(CGamePlayer* reporter);
	void addCourierKillsV(CGamePlayer* reporter);
};

//
// CDBDiv1DPS (dota player summary)
//

class CDBDiv1DPS {
  private:
	double m_Rating;
	double m_HighestRating;
	uint32_t m_TotalGames;		  // total number of dota games played
	uint32_t m_TotalWins;		  // total number of dota games won
	uint32_t m_TotalLosses;		  // total number of dota games lost
	uint32_t m_TotalKills;		  // total number of hero kills
	uint32_t m_TotalDeaths;		  // total number of deaths
	uint32_t m_TotalCreepKills;	  // total number of creep kills
	uint32_t m_TotalCreepDenies;  // total number of creep denies
	uint32_t m_TotalAssists;	  // total number of assists
	uint32_t m_TotalNeutralKills; // total number of neutral kills
								  // uint32_t m_TotalTowerKills;	// total number of tower kills
								  // uint32_t m_TotalRaxKills;		// total number of rax kills
								  // uint32_t m_TotalCourierKills;	// total number of courier kills

  public:
	CDBDiv1DPS(double nRating, double nHighestRating, uint32_t nTotalGames, uint32_t nTotalWins, uint32_t nTotalLosses, uint32_t nTotalKills, uint32_t nTotalDeaths, uint32_t nTotalCreepKills, uint32_t nTotalCreepDenies, uint32_t nTotalAssists, uint32_t nTotalNeutralKills)
	  : m_Rating(nRating)
	  , m_HighestRating(nHighestRating)
	  , m_TotalGames(nTotalGames)
	  , m_TotalWins(nTotalWins)
	  , m_TotalLosses(nTotalLosses)
	  , m_TotalKills(nTotalKills)
	  , m_TotalDeaths(nTotalDeaths)
	  , m_TotalCreepKills(nTotalCreepKills)
	  , m_TotalCreepDenies(nTotalCreepDenies)
	  , m_TotalAssists(nTotalAssists)
	  , m_TotalNeutralKills(nTotalNeutralKills)
	{
	}
	~CDBDiv1DPS() {}

	uint32_t GetTotalGames() { return m_TotalGames; }
	uint32_t GetTotalWins() { return m_TotalWins; }
	uint32_t GetTotalLosses() { return m_TotalLosses; }
	uint32_t GetTotalKills() { return m_TotalKills; }
	uint32_t GetTotalDeaths() { return m_TotalDeaths; }
	uint32_t GetTotalCreepKills() { return m_TotalCreepKills; }
	uint32_t GetTotalCreepDenies() { return m_TotalCreepDenies; }
	uint32_t GetTotalAssists() { return m_TotalAssists; }
	uint32_t GetTotalNeutralKills() { return m_TotalNeutralKills; }
	// uint32_t GetTotalTowerKills( )	{ return m_TotalTowerKills; }
	// uint32_t GetTotalRaxKills( )		{ return m_TotalRaxKills; }
	// uint32_t GetTotalCourierKills( )	{ return m_TotalCourierKills; }
	float GetAvgKills() { return (m_TotalWins + m_TotalLosses) > 0 ? (float)m_TotalKills / (m_TotalWins + m_TotalLosses) : 0; }
	float GetAvgDeaths() { return (m_TotalWins + m_TotalLosses) > 0 ? (float)m_TotalDeaths / (m_TotalWins + m_TotalLosses) : 0; }
	float GetAvgCreepKills() { return (m_TotalWins + m_TotalLosses) > 0 ? (float)m_TotalCreepKills / (m_TotalWins + m_TotalLosses) : 0; }
	float GetAvgCreepDenies() { return (m_TotalWins + m_TotalLosses) > 0 ? (float)m_TotalCreepDenies / (m_TotalWins + m_TotalLosses) : 0; }
	float GetAvgAssists() { return (m_TotalWins + m_TotalLosses) > 0 ? (float)m_TotalAssists / (m_TotalWins + m_TotalLosses) : 0; }
	float GetAvgNeutralKills() { return (m_TotalWins + m_TotalLosses) > 0 ? (float)m_TotalNeutralKills / (m_TotalWins + m_TotalLosses) : 0; }
	// float GetAvgTowerKills( )			{ return ( m_TotalWins + m_TotalLosses ) > 0 ? (float)m_TotalTowerKills / ( m_TotalWins + m_TotalLosses ) : 0; }
	// float GetAvgRaxKills( )			{ return ( m_TotalWins + m_TotalLosses ) > 0 ? (float)m_TotalRaxKills / ( m_TotalWins + m_TotalLosses ) : 0; }
	// float GetAvgCourierKills( )		{ return ( m_TotalWins + m_TotalLosses ) > 0 ? (float)m_TotalCourierKills / ( m_TotalWins + m_TotalLosses ) : 0; }
	double GetRating() { return m_Rating; }
	double GetHighestRating() { return m_HighestRating; }
};

#endif
