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

#ifndef GHOST_H
#define GHOST_H

#include "includes.h"
#include "didyouknow.h"
#include <chrono>

//
// CGHost
//

class CUDPSocket;
class CTCPServer;
class CTCPSocket;
class CGPSProtocol;
class CCRC32;
class CSHA1;
class CBNET;
class CBaseGame;
//class CAdminGame;
//class CGHostDB;
class CBaseCallable;
class CLanguage;
class CMap;
class CSaveGame;
class CConfig;
class CManager;

class CGHost
{
public:
	CUDPSocket *m_UDPSocket;				// a UDP socket for sending broadcasts and other junk (used with !sendlan)
	CTCPServer *m_ReconnectSocket;			// listening socket for GProxy++ reliable reconnects
	vector<CTCPSocket *> m_ReconnectSockets;// vector of sockets attempting to reconnect (connected but not identified yet)
	CGPSProtocol *m_GPSProtocol;
	CCRC32 *m_CRC;							// for calculating CRC's
	CSHA1 *m_SHA;							// for calculating SHA1's
	vector<CBNET *> m_BNETs;				// all our battle.net connections (there can be more than one)
	CBaseGame *m_CurrentGame;				// this game is still in the lobby state
	vector<CBaseGame *> m_Games;			// these games are in progress
	vector<CBaseCallable *> m_Callables;	// vector of orphaned callables waiting to die
	vector<BYTEARRAY> m_LocalAddresses;		// vector of local IP addresses
	CLanguage *m_Language;					// language
	CMap *m_Map;							// DotA ladder map
	CMap *m_DotALadderMapLegacy;			// DotA ladder map legacy
	CMap *m_DotALadderMapObs;				// DotA ladder map with observers
	CMap *m_CustomGameMap;					// the currently loaded map for custom games
	CSaveGame *m_SaveGame;					// the save game to use
	vector<PIDPlayer> m_EnforcePlayers;		// vector of pids to force players to use in the next game (used with saved games)
	bool m_Exiting;							// set to true to force ghost to shutdown next update (used by SignalCatcher)
	bool m_ExitingNice;						// set to true to force ghost to disconnect from all battle.net connections and wait for all games to finish before shutting down
	bool m_Enabled;							// set to false to prevent new games from being created
	string m_Version;						// GHost++ version string
	uint32_t m_HostCounter;					// the current host counter (a unique number to identify a game, incremented each time a game is created)
	bool m_AllGamesFinished;				// if all games finished (used when exiting nicely)
	uint32_t m_AllGamesFinishedTime;		// GetTime when all games finished (used when exiting nicely)
	string m_LanguageFile;					// config value: language file
	string m_Warcraft3Path;					// config value: Warcraft 3 path
	bool m_TFT;								// config value: TFT enabled or not
	string m_BindAddress;					// config value: the address to host games on
	uint16_t m_HostPort;					// config value: the port to host games on
	bool m_Reconnect;						// config value: GProxy++ reliable reconnects enabled or not
	uint16_t m_ReconnectPort;				// config value: the port to listen for GProxy++ reliable reconnects on
	uint32_t m_ReconnectWaitTime;			// config value: the maximum number of minutes to wait for a GProxy++ reliable reconnect
	uint32_t m_MaxGames;					// config value: maximum number of games in progress
	char m_CommandTrigger;					// config value: the command trigger inside games
	string m_MapCFGPath;					// config value: map cfg path
	string m_SaveGamePath;					// config value: savegame path
	string m_MapPath;						// config value: map path
	bool m_SaveReplays;						// config value: save replays
	string m_ReplayPath;					// config value: replay path
	string m_VirtualHostName;				// config value: virtual host name
	bool m_HideIPAddresses;					// config value: hide IP addresses from players
	bool m_CheckMultipleIPUsage;			// config value: check for multiple IP address usage
	uint32_t m_SpoofChecks;					// config value: do automatic spoof checks or not
	bool m_RequireSpoofChecks;				// config value: require spoof checks or not
	bool m_RequireMuteChecks;				// config value: require mute checks or not
	bool m_ReserveAdmins;					// config value: consider admins to be reserved players or not
	bool m_RefreshMessages;					// config value: display refresh messages or not (by default)
	bool m_AutoLock;						// config value: auto lock games when the owner is present
	bool m_AutoSave;						// config value: auto save before someone disconnects
	uint32_t m_AllowDownloads;				// config value: allow map downloads or not
	bool m_PingDuringDownloads;				// config value: ping during map downloads or not
	uint32_t m_MaxDownloaders;				// config value: maximum number of map downloaders at the same time
	uint32_t m_MaxDownloadSpeed;			// config value: maximum total map download speed in KB/sec
	bool m_LCPings;							// config value: use LC style pings (divide actual pings by two)
	uint32_t m_AutoKickPing;				// config value: auto kick players with ping higher than this
	uint32_t m_BanMethod;					// config value: ban method (ban by name/ip/both)
	string m_IPBlackListFile;				// config value: IP blacklist file (ipblacklist.txt)
	uint32_t m_LobbyTimeLimit;				// config value: auto close the game lobby after this many minutes without any reserved players
	uint32_t m_Latency;						// config value: the latency (by default)
	uint32_t m_MinLatency;					// config value: Minimum latency which can be set by game owner
	uint32_t m_MaxLatency;					// config value: Maximum latency which can be set by game owner
	uint32_t m_SyncLimit;					// config value: the maximum number of packets a player can fall out of sync before starting the lag screen (by default)
	uint32_t m_MinSyncLimit;				// config value: Minimum synclimit value which can be set by game owner
	uint32_t m_MaxSyncLimit;				// config value: Maximum synclimit value which can be set by game owner
	bool m_VoteKickAllowed;					// config value: if votekicks are allowed or not
	uint32_t m_VoteKickPercentage;			// config value: percentage of players required to vote yes for a votekick to pass
	string m_DefaultMap;					// config value: default map (map.cfg)
	string m_DefaultMapLegacy;				// config value: default map (map_legacy.cfg)
	string m_DefaultMapObs;					// config value: default map (map_obs.cfg)
	string m_MOTDFile;						// config value: motd.txt
	string m_GameLoadedFile;				// config value: gameloaded.txt
	string m_GameOverFile;					// config value: gameover.txt
	bool m_LocalAdminMessages;				// config value: send local admin messages or not
	unsigned char m_LANWar3Version;			// config value: LAN warcraft 3 version
	uint32_t m_ReplayWar3Version;			// config value: replay warcraft 3 version (for saving replays)
	uint32_t m_ReplayBuildNumber;			// config value: replay build number (for saving replays)
	bool m_TCPNoDelay;						// config value: use Nagle's algorithm or not
	uint32_t m_MatchMakingMethod;			// config value: the matchmaking method
	bool m_restrictW3Version;				// config value: restrict ladder DotA games to particular w3 version
	W3Version * m_restrictedW3Version;		// config value: if restrictW3Version is true, which w3 version should be required, ex: 1.26.0.6401
	uint32_t m_msGameTimeBeforeAutoban;		// config value: duration in ms after game start before autoban is turned ON
	bool m_UseNewPSRFormula;				// config value: use old (myubernick) or new (luke) formula to calculate PSR for winners and losers after dota game
	string m_allowLanJoinAsRealm;			// config value: allows joining of LAN players and giving them this value as realm name. For testing with fake bot players only.
	uint32_t m_AutoStartHeldPlayersMaxTimeInMin; //config value: max time in minutes to wait for held players to join the game
	string m_region;			     //config value: bot_region, specify in which GEO region bot is located (see common/src/regions.h)

	uint32_t m_DotaMaxWinChanceDiffGainConstant = 0; //config value: if win chance diff is more than this, disabled +1/-1 constant gain
	uint32_t m_DotaMaxWinChanceDiff = 0;	// config value: max win chance diff allowed for game to be allowed to start. 0-100. 0 means disabled
	uint32_t m_DotaMaxWLDiff = 0;			// config value: max WL diff allowed for game to be allowed to start. 0-100. 0 means disabled

	string m_GGHostName;	// hack to change bot name in game list when hosting games

	CManager *m_Manager;
	set<string> m_DotAAllowedModes;
	uint32_t m_LastGameID;
	uint32_t m_BlockMS;						// block the main loop for m_BlockMS milliseconds

	uint32_t m_GHostGroup;

	bool m_ContributorOnlyMode;
	bool m_ReplaceAutobanWithPSRPenalty;

	string m_CFGFile;

	CGHost( CConfig *CFG, string nCFGFile );
	CGHost(bool mock);
	~CGHost( );

	// processing functions

	bool Update( long usecBlock );

	// events

	void EventBNETConnecting( CBNET *bnet );
	void EventBNETConnected( CBNET *bnet );
	void EventBNETDisconnected( CBNET *bnet );
	void EventBNETLoggedIn( CBNET *bnet );
	void EventBNETGameRefreshed( CBNET *bnet );
	void EventBNETGameRefreshFailed( CBNET *bnet );
	void EventBNETConnectTimedOut( CBNET *bnet );
	void EventBNETWhisper( CBNET *bnet, string user, string message );
	void EventBNETChat( CBNET *bnet, string user, string message );
	void EventBNETEmote( CBNET *bnet, string user, string message );
	void EventGameDeleted( CBaseGame *game );

	// did you know?
	DidYouKnow m_DidYouKnowGenerator;
	bool m_DidYouKnowEnabled;

	// other functions

	void ReloadConfigs( );
	void SetConfigs( CConfig *CFG );
	void ExtractScripts( );
	//void LoadIPToCountryData( );
	void LoadCustomGameMap( CConfig *CFG, string nCFGFile );
	void CreateGame( CMap *map, unsigned char gameState, bool saveGame, string gameName, string ownerName, 
		string creatorName, string creatorServer, uint32_t gameType, bool privobs, vector<string> heldPlayers);

private:
	void LoadMapCfgs();
};

#endif
