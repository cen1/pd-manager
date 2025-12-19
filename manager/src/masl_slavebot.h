
#ifndef MASL_SLAVEBOT_H
#define MASL_SLAVEBOT_H

#include <cstdint>
#include <string>

using namespace std;

uint32_t GetTime();
string GetServerAddress(uint32_t serverID);

class CRemoteGamePlayer;
class CRemoteGame;
class CSlaves;
class CSlaveBot;

//
// CQueuedGame
//

class CQueuedGame {
  private:
	string m_CreatorName;
	string m_CreatorServer;
	string m_GameName;
	string m_Map; // map value is not important for DotA (ladder map) games since we load map by GameType variable later
	uint32_t m_AccessLevel;
	uint32_t m_QueuedTime;	   // GetTime when game was initially queued
	uint32_t m_TrueQueuedTime; // if we change m_QueuedTime value in any way to manipulate with this game in queue (to change it's place) m_TrueQueuedTime will contain the original m_QueuedTime time
	uint32_t m_GameState;
	uint32_t m_GameType;
	bool m_Observers;
	uint32_t m_GHostGroup;
	string m_region;

  public:
	CQueuedGame(string nCreatorName, string nCreatorServer, string nGameName, string nMap, uint32_t nAccessLevel, uint32_t nGameState, uint32_t nGameType, bool nObservers, uint32_t nGHostGroup, string nRegion)
	  : m_CreatorName(nCreatorName)
	  , m_CreatorServer(nCreatorServer)
	  , m_GameName(nGameName)
	  , m_Map(nMap)
	  , m_AccessLevel(nAccessLevel)
	  , m_GameState(nGameState)
	  , m_GameType(nGameType)
	  , m_Observers(nObservers)
	  , m_GHostGroup(nGHostGroup)
	  , m_region(nRegion)
	  , m_QueuedTime(GetTime())
	  , m_TrueQueuedTime(GetTime())
	{
	}
	~CQueuedGame() {}

	string GetCreatorName() { return m_CreatorName; }
	string GetCreatorServer() { return m_CreatorServer; }
	string GetGameName() { return m_GameName; }
	string GetMap() { return m_Map; }
	uint32_t GetAccessLevel() { return m_AccessLevel; }
	uint32_t GetQueuedTime() { return m_QueuedTime; }
	uint32_t GetTrueQueuedTime() { return m_TrueQueuedTime; }
	uint32_t GetGameState() { return m_GameState; }
	uint32_t GetGameType() { return m_GameType; }
	bool GetObservers() { return m_Observers; }
	uint32_t GetGHostGroup() { return m_GHostGroup; }
	string GetRegion() { return m_region; }

	void SetGameName(string nGameName) { m_GameName = nGameName; }
	void SetMap(string nMap) { m_Map = nMap; }
	void SetAccessLevel(uint32_t nAccessLevel) { m_AccessLevel = nAccessLevel; }
	void SetQueuedTime(uint32_t nQueuedTime) { m_QueuedTime = nQueuedTime; }
	void SetGameState(uint32_t nGameState) { m_GameState = nGameState; }
	void SetGameType(uint32_t nGameType) { m_GameType = nGameType; }
	void SetObservers(bool nObservers) { m_Observers = nObservers; }
};

//
// CLoadedMap
//

class CLoadedMap {
  private:
	string m_Name;
	string m_Map;
	uint32_t m_LoadedTime;

  public:
	CLoadedMap(string nName, string nMap, uint32_t nLoadedTime);
	~CLoadedMap();

	string GetName() { return m_Name; }
	string GetMap() { return m_Map; }
	uint32_t GetLoadedTime() { return m_LoadedTime; }

	void SetName(string nName) { m_Name = nName; }
	void SetMap(string nMap) { m_Map = nMap; }
	void SetLoadedTime(uint32_t nLoadedTime) { m_LoadedTime = nLoadedTime; }
};

//
// CRemoteGamePlayer
//

class CRemoteGamePlayer {
  private:
	string m_Name;		 // game player name
	uint32_t m_LeftTime; // how many seconds passed from when game started before player left

  public:
	CRemoteGamePlayer(string nName);
	~CRemoteGamePlayer();

	string GetName() { return m_Name; }
	uint32_t GetLeftTime() { return m_LeftTime; }

	// patch, slave bot sends us minutes not seconds, convert it to seconds format
	void SetLeftTime(uint32_t nLeftTime) { m_LeftTime = nLeftTime * 60; }
};

//
// CRemoteGame
//

class CRemoteGame {
  private:
	uint32_t m_BotID; // slave bot id
	uint32_t m_GameID;
	uint32_t m_GameType;
	uint32_t m_CreatorServerID;
	uint32_t m_GameState;
	uint32_t m_StartTime;
	// uint32_t m_NumSlots;
	uint32_t m_NumStartPlayers; // number of players in the game when it started
	// uint32_t m_CurrentPlayers;			// current number of players in the game
	string m_GameName; // game name
	string m_CFGFile;
	string m_OwnerName;
	CRemoteGamePlayer* m_Players[12]; // players in the lobby or game on their respective slots
	bool m_InLobby;

  public:
	CRemoteGame(uint32_t nBotID, uint32_t nGameID, uint32_t nCreatorServerID, uint32_t nGameState, string nOwnerName, uint32_t nNumSlots, string nGameName, string nCFGFile, uint32_t nGameType);
	CRemoteGame(uint32_t nBotID, uint32_t nGameID, uint32_t nCreatorServerID, uint32_t nGameState, string nOwnerName, uint32_t nNumStartPlayers, string nSlotInfo, string nGameName, string nCFGFile, uint32_t nGameType);
	~CRemoteGame();

	uint32_t GetBotID() { return m_BotID; }
	uint32_t GetGameID() { return m_GameID; }
	uint32_t GetGameType() { return m_GameType; }
	uint32_t GetCreatorServerID() { return m_CreatorServerID; }
	uint32_t GetGameState() { return m_GameState; }
	uint32_t GetDurationInSeconds() { return GetTime() - m_StartTime; }
	uint32_t GetDurationInMinutes() { return (GetTime() - m_StartTime) / 60; }
	uint32_t GetNumStartPlayers() { return m_NumStartPlayers; }
	string GetCreatorServer() { return GetServerAddress(m_CreatorServerID); }
	string GetCFGFile() { return m_CFGFile; }
	string GetGameName() { return m_GameName; }
	string GetOwnerName() { return m_OwnerName; }
	uint32_t GetStartTime() { return m_StartTime; }
	bool GetInLobby() { return m_InLobby; }

	uint32_t GetNumPlayers();
	string GetDescription();
	CRemoteGamePlayer* GetGamePlayer(uint32_t slot);
	CRemoteGamePlayer* GetGamePlayer(string lowercasename);
	string GetNames();

	void SetGameState(uint32_t nGameState) { m_GameState = nGameState; }
	// void SetCurrentPlayers( uint32_t nCurrentPlayers )							{ m_CurrentPlayers = nCurrentPlayers; }
	void SetGameName(string nGameName) { m_GameName = nGameName; }
	void SetCFGFile(string nCFGFile) { m_CFGFile = nCFGFile; }
	void SetOwnerName(string nOwnerName) { m_OwnerName = nOwnerName; }
	void SetInLobby(bool nInLobby) { m_InLobby = nInLobby; }

	void SetGamePlayer(uint32_t slot, string name);
	void SetGamePlayer(string name);
	void DelGamePlayer(string name);
	void SetPlayerLeft(string name);

  private:
	std::string hash();
};

#endif
