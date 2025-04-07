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

#include "configuration.h"
#include <stdint.h>

// STL

#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>
#include <list>

#include <boost/asio.hpp>
#include "typedefs.h"

#ifdef WIN32
 #include <unordered_map>
#else
 #include <tr1/unordered_map>
#endif

using namespace std;

// time

uint32_t GetTime( );		// seconds
uint32_t GetTicks( );		// milliseconds
uint32_t GetMySQLTime( );

#ifdef WIN32
 #define MILLISLEEP( x ) Sleep( x )
#else
 #define MILLISLEEP( x ) usleep( ( x ) * 1000 )
#endif

// network

#undef FD_SETSIZE
#define FD_SETSIZE 512

// logging & output

void GHOST_Log( string message, bool printToConsole, string logFile );

void CONSOLE_Print( string message, bool printToConsole = true );
void MYSQL_Print( string message, bool printToConsole = false );
void BNET_Print( string message, bool printToConsole = true );

void DEBUG_Print( string message, bool printToConsole = true );
void DEBUG_Print( BYTEARRAY b, bool printToConsole = true );

// other

uint32_t GetServerID( const string &server );
string GetServerAddress( uint32_t serverID );

//
// CGHost
//

class CUDPSocket;
class CBNET;
class CTourBNET;
class CSlaveBot;
class CGHostDB;
class CBaseCallable;
class CLanguage;
class CConfig;
class CDBBan;
class CCallableUnixTimestampGet;
class CRemoteGame;
class CManager;
class CWebManager;
class CUDPServer;
class CDBCache;
class CPlayers;
class CDBPlayer2;
//class CDBUser;
class CDBBanGroup;
class CDBDotAPlayerSummary;
class CCallableCacheList;
class CCallableGameCustomAdd;
class CCallableGameCustomDotAAdd;
class CCallableGameDiv1DotAAdd;
class CCallableBanAdd;
class CCallableDIV1BanAdd;
class CCallableFromCheck;
class CCallableGameID;

class CGHost
{
public:
	boost::asio::io_context &m_IOService;
	boost::asio::io_context::work &m_Work;

	boost::asio::deadline_timer m_UpdateTimer;

	vector<CBNET *> m_BNETs;				// all our battle.net connections (there can be more than one)
	vector<CTourBNET *> m_TourBNETs;
	CGHostDB *m_DB;							// database
	vector<CBaseCallable *> m_Callables;	// vector of orphaned callables waiting to die
	vector<BYTEARRAY> m_LocalAddresses;		// vector of local IP addresses
	CLanguage *m_Language;					// language
	bool m_Exiting;							// set to true to force ghost to shutdown next update (used by SignalCatcher)
	bool m_ExitingNice;						// set to true to force ghost to disconnect from all battle.net connections and wait for all games to finish before shutting down
	bool m_Enabled;							// set to false to prevent new games from being created
	bool m_EnabledLadder;					// set to false to prevent new LADDER games from being created
	string m_Version;						// GHost++ version string
	string m_LanguageFile;					// config value: language file
	string m_Warcraft3Path;					// config value: Warcraft 3 path
	string m_BindAddress;					// config value: the address to host games on
	string m_IPBlackListFile;				// config value: IP blacklist file (ipblacklist.txt)
	string m_MapPath;						// config value: map path
	unsigned char m_LANWar3Version;			// config value: LAN warcraft 3 version
	bool m_TCPNoDelay;						// config value: use Nagle's algorithm or not
	uint32_t m_DotaMaxWinChanceDiffGainConstant = 0; //config value: if win chance diff is more than this, disabled +1/-1 constant gain
	bool m_ContributorOnlyMode;

	CCallableUnixTimestampGet *m_CallableMysqlTime;		// MySql server time
	CCallableCacheList *m_CallableCacheList;
	CUDPServer *m_UDPCommandSocket;
	CManager *m_Manager;
	CWebManager *m_WebManager;
	vector<CDBDotAPlayerSummary *> m_DotAPlayerSummary;
	//vector<CDBUser *> m_User;
	vector<CDBBanGroup *> m_BanGroup;
	list<CCallableGameCustomAdd *> m_GameCustomAdds;
	list<CCallableGameCustomDotAAdd *> m_GameCustomDotAAdds;
	list<CCallableGameDiv1DotAAdd *> m_GameDiv1DotAAdds;
	list<CCallableBanAdd *> m_BanAdds;
	list<CCallableDIV1BanAdd *> m_DIV1BanAdds;
	list<CCallableFromCheck *> m_FromChecks;
	list<CCallableGameID *> m_GameIDs;
	list<CPlayers *> m_Players;
	//map< int, set<int> > m_AccessLevels;
	string m_TempReplayPath;
	string m_ReplayPath;

	string m_LobbyLogPath;
	string m_LobbyLogPathTmp;
	string m_GameLogPath;
	string m_GameLogPathTmp;

	string m_DisabledMessage;							// reason to be displayed why bot is disabled
	string m_GameListFile;
	uint32_t m_LastMySQLTimeRefreshTime;
	uint32_t m_GameListLastRefreshTime;
	uint32_t m_LastCacheRefreshTime;					// last Cache List refresh GetTime
	uint32_t m_GameListRefresh;
	uint32_t m_MySQLTime;
	uint32_t m_MaxGames;
	bool m_RPGMode;
	bool m_UseNewPSRFormula;

	CGHost( CConfig *CFG, boost::asio::io_context &nIOService, boost::asio::io_context::work &nWork );
	~CGHost( );

	// processing functions

	void Update( const boost::system::error_code& error );

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

	// other

	void SaveLog( string fileName, const string &log );
	//uint32_t GetUnixTime( );
	//uint32_t GetMysqlCurrentTime( );
	CSlaveBot *GetAvailableSlave( );
	CSlaveBot *GetAvailableSlaveFor( int gameState, string server );
	string GetSlavesGameList( );
	CRemoteGame *GetSlaveGame( uint32_t gamePosition );
	bool HaveUserCreatedGameInLobby( string user );
	void SendBanToAllSlaves( string server, string victim, string admin, string reason, uint32_t duration );
	void SendUnbanToAllSlaves( string victim, string server );
	CBNET * GetBnetByName(string name);

};

//BYTEARRAY StringToByteArray( string s );

#endif
