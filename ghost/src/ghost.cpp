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

#include "ghost.h"
#include "util.h"
#include "crc32.h"
#include "sha1.h"
#include "config.h"
#include "language.h"
#include "socket.h"
#include "ghostdb.h"
#include "bnet.h"
#include "map.h"
#include "masl_manager.h"
#include "masl_protocol_2.h"
#include "packed.h"
#include "savegame.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "gpsprotocol.h"
#include "game_base.h"
#include "game_custom.h"
#include "game_customdota.h"
#include "game_div1dota.h"
#include "regions.h"

#include <signal.h>
#include <stdlib.h>

#ifdef WIN32
 #include <ws2tcpip.h>		// for WSAIoctl
 #include "Windows.h"
#pragma comment(lib, "Winmm.lib")
#endif

#define __STORMLIB_SELF__
#include <StormLib.h>

#ifdef WIN32
 #include <windows.h>
 #include <winsock.h>
#endif

#include <time.h>

#ifndef WIN32
 #include <sys/time.h>
#endif

#ifdef __APPLE__
 #include <mach/mach_time.h>
#endif

//
// CGHost
//
CGHost::CGHost(bool mock) {
	//mock constructor
}

CGHost :: CGHost( CConfig *CFG, string nCFGFile )
{
	m_CFGFile = nCFGFile;

	m_UDPSocket = new CUDPSocket( );
	m_UDPSocket->SetBroadcastTarget( CFG->GetString( "udp_broadcasttarget", string( ) ) );
	m_UDPSocket->SetDontRoute( CFG->GetInt( "udp_dontroute", 0 ) == 0 ? false : true );
	m_ReconnectSocket = NULL;
	m_GPSProtocol = new CGPSProtocol( );
	m_CRC = new CCRC32( );
	m_CRC->Initialize( );
	m_SHA = new CSHA1( );
	m_CurrentGame = NULL;
	//string DBType = CFG->GetString( "db_type", "sqlite3" );
	//CONSOLE_Print( "[GHOST] opening primary database" );

	/*if( DBType == "mysql" )
	{
#ifdef GHOST_MYSQL
		m_DB = new CGHostDBMySQL( CFG );
#else
		CONSOLE_Print( "[GHOST] warning - this binary was not compiled with MySQL database support, using SQLite database instead" );
		m_DB = new CGHostDBSQLite( CFG );
#endif
	}
	else
		m_DB = new CGHostDBSQLite( CFG );

	CONSOLE_Print( "[GHOST] opening secondary (local) database" );
	m_DBLocal = new CGHostDBSQLite( CFG );*/

	// get a list of local IP addresses
	// this list is used elsewhere to determine if a player connecting to the bot is local or not

	CONSOLE_Print( "[GHOST] attempting to find local IP addresses" );

#ifdef WIN32
	// use a more reliable Windows specific method since the portable method doesn't always work properly on Windows
	// code stolen from: http://tangentsoft.net/wskfaq/examples/getifaces.html

	SOCKET sd = WSASocket( AF_INET, SOCK_DGRAM, 0, 0, 0, 0 );

	if( sd == SOCKET_ERROR )
		CONSOLE_Print( "[GHOST] error finding local IP addresses - failed to create socket (error code " + UTIL_ToString( WSAGetLastError( ) ) + ")" );
	else
	{
		INTERFACE_INFO InterfaceList[20];
		unsigned long nBytesReturned;

		if( WSAIoctl( sd, SIO_GET_INTERFACE_LIST, 0, 0, &InterfaceList, sizeof(InterfaceList), &nBytesReturned, 0, 0 ) == SOCKET_ERROR )
			CONSOLE_Print( "[GHOST] error finding local IP addresses - WSAIoctl failed (error code " + UTIL_ToString( WSAGetLastError( ) ) + ")" );
		else
		{
			int nNumInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);

                        for( int i = 0; i < nNumInterfaces; ++i )
			{
				sockaddr_in *pAddress;
				pAddress = (sockaddr_in *)&(InterfaceList[i].iiAddress);
				CONSOLE_Print( "[GHOST] local IP address #" + UTIL_ToString( i + 1 ) + " is [" + string( inet_ntoa( pAddress->sin_addr ) ) + "]" );
				m_LocalAddresses.push_back( UTIL_CreateByteArray( (uint32_t)pAddress->sin_addr.s_addr, false ) );
			}
		}

		closesocket( sd );
	}
#else
	// use a portable method

	char HostName[255];

	if( gethostname( HostName, 255 ) == SOCKET_ERROR )
		CONSOLE_Print( "[GHOST] error finding local IP addresses - failed to get local hostname" );
	else
	{
		CONSOLE_Print( "[GHOST] local hostname is [" + string( HostName ) + "]" );
		struct hostent *HostEnt = gethostbyname( HostName );

		if( !HostEnt )
			CONSOLE_Print( "[GHOST] error finding local IP addresses - gethostbyname failed" );
		else
		{
                        for( int i = 0; HostEnt->h_addr_list[i] != NULL; ++i )
			{
				struct in_addr Address;
				memcpy( &Address, HostEnt->h_addr_list[i], sizeof(struct in_addr) );
				CONSOLE_Print( "[GHOST] local IP address #" + UTIL_ToString( i + 1 ) + " is [" + string( inet_ntoa( Address ) ) + "]" );
				m_LocalAddresses.push_back( UTIL_CreateByteArray( (uint32_t)Address.s_addr, false ) );
			}
		}
	}
#endif

	m_Language = NULL;
	m_Exiting = false;
	m_ExitingNice = false;
	m_Enabled = true;
	m_Version = "7.0.0";
	m_HostCounter = 1;
	m_AllGamesFinished = false;
	m_AllGamesFinishedTime = 0;
	m_TFT = CFG->GetInt( "bot_tft", 1 ) == 0 ? false : true;

	if( m_TFT )
		CONSOLE_Print( "[GHOST] acting as Warcraft III: The Frozen Throne" );
	else
		CONSOLE_Print( "[GHOST] acting as Warcraft III: Reign of Chaos" );

	m_HostPort = CFG->GetInt( "bot_hostport", 6112 );
	m_Reconnect = CFG->GetInt( "bot_reconnect", 1 ) == 0 ? false : true;
	m_ReconnectPort = CFG->GetInt( "bot_reconnectport", 6114 );
	m_DefaultMap = CFG->GetString( "bot_defaultmap", "map" );
	m_DefaultMapLegacy = CFG->GetString("bot_defaultmap_legacy", "map_legacy");
	m_DefaultMapObs = CFG->GetString("bot_defaultmap_obs", "map_obs");
	m_LANWar3Version = CFG->GetInt( "lan_war3version", 24 );
	m_ReplayWar3Version = CFG->GetInt( "replay_war3version", 24 );
	m_ReplayBuildNumber = CFG->GetInt( "replay_buildnumber", 6059 );
	SetConfigs( CFG );

	// load the battle.net connections
	// we're just loading the config data and creating the CBNET classes here, the connections are established later (in the Update function)

	for( uint32_t i = 1; i < 10; ++i )
	{
		string Prefix;

		if( i == 1 )
			Prefix = "bnet_";
		else
			Prefix = "bnet" + UTIL_ToString( i ) + "_";

		string Server = CFG->GetString( Prefix + "server", string( ) );
		string ServerAlias = CFG->GetString( Prefix + "serveralias", string( ) );
		string CDKeyROC = CFG->GetString( Prefix + "cdkeyroc", string( ) );
		string CDKeyTFT = CFG->GetString( Prefix + "cdkeytft", string( ) );
		string CountryAbbrev = CFG->GetString( Prefix + "countryabbrev", "USA" );
		string Country = CFG->GetString( Prefix + "country", "United States" );
		string Locale = CFG->GetString( Prefix + "locale", "system" );
		uint32_t LocaleID;

		if( Locale == "system" )
		{
#ifdef WIN32
			LocaleID = GetUserDefaultLangID( );
#else
			LocaleID = 1033;
#endif
		}
		else
			LocaleID = UTIL_ToUInt32( Locale );

		string UserName = CFG->GetString( Prefix + "username", string( ) );
		string UserPassword = CFG->GetString( Prefix + "password", string( ) );
		string FirstChannel = CFG->GetString( Prefix + "firstchannel", "The Void" );
		string RootAdmin = CFG->GetString( Prefix + "rootadmin", string( ) );
		string BNETCommandTrigger = CFG->GetString( Prefix + "commandtrigger", "!" );

		if( BNETCommandTrigger.empty( ) )
			BNETCommandTrigger = "!";

		bool HoldFriends = CFG->GetInt( Prefix + "holdfriends", 1 ) == 0 ? false : true;
		bool HoldClan = CFG->GetInt( Prefix + "holdclan", 1 ) == 0 ? false : true;
		bool PublicCommands = CFG->GetInt( Prefix + "publiccommands", 1 ) == 0 ? false : true;
		string BNLSServer = CFG->GetString( Prefix + "bnlsserver", string( ) );
		int BNLSPort = CFG->GetInt( Prefix + "bnlsport", 9367 );
		int BNLSWardenCookie = CFG->GetInt( Prefix + "bnlswardencookie", 0 );
		unsigned char War3Version = CFG->GetInt( Prefix + "custom_war3version", 24 );
		BYTEARRAY EXEVersion = UTIL_ExtractNumbers( CFG->GetString( Prefix + "custom_exeversion", string( ) ), 4 );
		BYTEARRAY EXEVersionHash = UTIL_ExtractNumbers( CFG->GetString( Prefix + "custom_exeversionhash", string( ) ), 4 );
		string PasswordHashType = CFG->GetString( Prefix + "custom_passwordhashtype", string( ) );
		string PVPGNRealmName = CFG->GetString( Prefix + "custom_pvpgnrealmname", "PvPGN Realm" );
		uint32_t MaxMessageLength = CFG->GetInt( Prefix + "custom_maxmessagelength", 200 );
		uint32_t XPAMGProxyVersion = CFG->GetInt( Prefix + "custom_xpamgproxyversion", 0 );
		
		if( Server.empty( ) )
			break;

		uint32_t ServerID = CFG->GetInt( Prefix + "serverid", 0 );

		if( ServerID == 0 )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "serverid, skipping this battle.net connection" );
			continue;
		}

		if( CDKeyROC.empty( ) )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "cdkeyroc, skipping this battle.net connection" );
			continue;
		}

		if( m_TFT && CDKeyTFT.empty( ) )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "cdkeytft, skipping this battle.net connection" );
			continue;
		}

		if( UserName.empty( ) )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "username, skipping this battle.net connection" );
			continue;
		}

		if( UserPassword.empty( ) )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "password, skipping this battle.net connection" );
			continue;
		}

		//CONSOLE_Print( "[GHOST] found battle.net connection #" + UTIL_ToString( i ) + " for server [" + Server + "]" );
		CONSOLE_Print( "[GHOST] found battle.net connection #" + UTIL_ToString( i ) + " for server [" + Server + "] with ID " + UTIL_ToString( ServerID ) );

		if( Locale == "system" )
		{
#ifdef WIN32
			CONSOLE_Print( "[GHOST] using system locale of " + UTIL_ToString( LocaleID ) );
#else
			CONSOLE_Print( "[GHOST] unable to get system locale, using default locale of 1033" );
#endif
		}

		m_BNETs.push_back( new CBNET( this, Server, ServerAlias, BNLSServer, (uint16_t)BNLSPort, (uint32_t)BNLSWardenCookie, CDKeyROC, CDKeyTFT, CountryAbbrev, Country, LocaleID, UserName, UserPassword, FirstChannel, RootAdmin, BNETCommandTrigger[0], HoldFriends, HoldClan, PublicCommands, War3Version, EXEVersion, EXEVersionHash, PasswordHashType, PVPGNRealmName, MaxMessageLength, i, ServerID, XPAMGProxyVersion ) );
	}

	if( m_BNETs.empty( ) )
		CONSOLE_Print( "[GHOST] warning - no battle.net connections found in config file" );

	// extract common.j and blizzard.j from War3Patch.mpq if we can
	// these two files are necessary for calculating "map_crc" when loading maps so we make sure to do it before loading the default map
	// see CMap :: Load for more information

	ExtractScripts( );

	// load the default maps (note: make sure to run ExtractScripts first)

	if( m_DefaultMap.size( ) < 4 || m_DefaultMap.substr( m_DefaultMap.size( ) - 4 ) != ".cfg" )
	{
		m_DefaultMap += ".cfg";
		CONSOLE_Print( "[GHOST] adding \".cfg\" to default map -> new default is [" + m_DefaultMap + "]" );
	}
	if (m_DefaultMapLegacy.size() < 4 || m_DefaultMapLegacy.substr(m_DefaultMapLegacy.size() - 4) != ".cfg")
	{
		m_DefaultMapLegacy += ".cfg";
		CONSOLE_Print("[GHOST] adding \".cfg\" to default map legacy -> new default is [" + m_DefaultMapLegacy + "]");
	}
	if (m_DefaultMapObs.size() < 4 || m_DefaultMapObs.substr(m_DefaultMapObs.size() - 4) != ".cfg")
	{
		m_DefaultMapObs += ".cfg";
		CONSOLE_Print("[GHOST] adding \".cfg\" to default map obs -> new default is [" + m_DefaultMapObs + "]");
	}

	// DotA ladder game map
	CConfig MapCFG;
	MapCFG.Read( m_MapCFGPath + m_DefaultMap );
	m_Map = new CMap( this, &MapCFG, m_MapCFGPath + m_DefaultMap );

	// DotA ladder game map legacy
	CConfig DotALadderMapLegacyCFG;
	DotALadderMapLegacyCFG.Read(m_MapCFGPath + m_DefaultMapLegacy);
	m_DotALadderMapLegacy = new CMap(this, &DotALadderMapLegacyCFG, m_MapCFGPath + m_DefaultMapLegacy);

	// DotA ladder game map with observer slots
	CConfig DotALadderMapObsCFG;
	DotALadderMapObsCFG.Read( m_MapCFGPath + m_DefaultMapObs );
	m_DotALadderMapObs = new CMap( this, &DotALadderMapObsCFG, m_MapCFGPath + m_DefaultMapObs );

	// custom game map

	/*CConfig CustomGameMapCFG;
	CustomGameMapCFG.Read( m_MapCFGPath + m_DefaultMap );
	m_CustomGameMap = new CMap( this, &CustomGameMapCFG, m_MapCFGPath + m_DefaultMap );*/

	m_CustomGameMap = NULL;

	/*if( !m_AdminGameMap.empty( ) )
	{
		if( m_AdminGameMap.size( ) < 4 || m_AdminGameMap.substr( m_AdminGameMap.size( ) - 4 ) != ".cfg" )
		{
			m_AdminGameMap += ".cfg";
			CONSOLE_Print( "[GHOST] adding \".cfg\" to default admin game map -> new default is [" + m_AdminGameMap + "]" );
		}

		CONSOLE_Print( "[GHOST] trying to load default admin game map" );
		CConfig AdminMapCFG;
		AdminMapCFG.Read( m_MapCFGPath + m_AdminGameMap );
		m_AdminMap = new CMap( this, &AdminMapCFG, m_MapCFGPath + m_AdminGameMap );

		if( !m_AdminMap->GetValid( ) )
		{
			CONSOLE_Print( "[GHOST] default admin game map isn't valid, using hardcoded admin game map instead" );
			delete m_AdminMap;
			m_AdminMap = new CMap( this );
		}
	}
	else
	{
		CONSOLE_Print( "[GHOST] using hardcoded admin game map" );
		m_AdminMap = new CMap( this );
	}*/

	//m_AutoHostMap = new CMap( *m_Map );
	m_SaveGame = new CSaveGame( );

	// load the iptocountry data

	//LoadIPToCountryData( );

	// create the admin game

	/*if( m_AdminGameCreate )
	{
		CONSOLE_Print( "[GHOST] creating admin game" );
		m_AdminGame = new CAdminGame( this, m_AdminMap, NULL, m_AdminGamePort, 0, "GHost++ Admin Game", m_AdminGamePassword );

		if( m_AdminGamePort == m_HostPort )
			CONSOLE_Print( "[GHOST] warning - admingame_port and bot_hostport are set to the same value, you won't be able to host any games" );
	}
	else
		m_AdminGame = NULL;*/

	/*if( m_BNETs.empty( ) && !m_AdminGame )
		CONSOLE_Print( "[GHOST] warning - no battle.net connections found and no admin game created" );*/

	if( m_BNETs.empty( ) )
		CONSOLE_Print( "[GHOST] warning - no battle.net connections found" );
	
	CONSOLE_Print( "[GHOST] pdBOT Version " + m_Version + " (eurobattle.net)" );

	string MASLServer = CFG->GetString( "masl_server", string( ) );
	uint32_t MASLPort = CFG->GetInt( "masl_port", 0 );

	if( MASLServer.empty( ) )
	{
		CONSOLE_Print( "[GHOST] error - missing masl_server" );
	}

	if( MASLPort == 0 )
	{
		CONSOLE_Print( "[GHOST] error - missing masl_port" );
	}

	m_Manager = new CManager( this, MASLServer, MASLPort );
	m_LastGameID = 0;

	m_DotAAllowedModes.insert( "ap" );
	m_DotAAllowedModes.insert( "ar" );
	m_DotAAllowedModes.insert( "rd" );
	m_DotAAllowedModes.insert( "sd" );
	m_DotAAllowedModes.insert( "cd" );
	m_DotAAllowedModes.insert( "cm" );
	/*m_DotAAllowedModes.insert( "xl" );
	m_DotAAllowedModes.insert( "lm" );
	m_DotAAllowedModes.insert( "tr" );
	m_DotAAllowedModes.insert( "mr" );
	m_DotAAllowedModes.insert( "vr" );
	m_DotAAllowedModes.insert( "rv" );
	m_DotAAllowedModes.insert( "oi" );
	m_DotAAllowedModes.insert( "aa" );
	m_DotAAllowedModes.insert( "ai" );
	m_DotAAllowedModes.insert( "as" );
	m_DotAAllowedModes.insert( "ns" );
	m_DotAAllowedModes.insert( "nr" );
	m_DotAAllowedModes.insert( "mi" );
	m_DotAAllowedModes.insert( "mo" );
	m_DotAAllowedModes.insert( "ro" );
	m_DotAAllowedModes.insert( "zm" );
	m_DotAAllowedModes.insert( "sp" );*/
	//m_DotAAllowedModes.insert( "so" );
	m_DotAAllowedModes.insert( "em" );

	m_DidYouKnowEnabled = CFG->GetInt("bot_did_you_know_enabled", 1) == 0 ? false : true;
	m_allowLanJoinAsRealm = CFG->GetString("bot_allow_lan_join_as_realm", "");
	m_AutoStartHeldPlayersMaxTimeInMin = CFG->GetInt("bot_auto_start_held_players_max_time_in_min", 3);
	m_region = CFG->GetString("bot_region", Regions::DEFAULT_REGION);
	if (Regions::map.find(m_region) == Regions::map.end()) {
	    CONSOLE_Print("[GHOST] ERROR - Invalid key for bot_region. Defaulting to eu.");
	    m_region=Regions::DEFAULT_REGION;
	}
	CONSOLE_Print("[GHOST] Region set to "+m_region);

	m_BlockMS = 5;

	uint32_t DotaMaxWinChanceDiff = CFG->GetInt("dota_max_win_chance_diff", 0);
	uint32_t DotaMaxWinChanceDiffGainConstant = CFG->GetInt("dota_max_win_chance_diff_gain_constant", 0);
	uint32_t DotaMaxWLDiff = CFG->GetInt("dota_max_wl_diff", 0);
	if (DotaMaxWinChanceDiff > 100 || DotaMaxWinChanceDiff < 0) {
		DotaMaxWinChanceDiff = 0;
		CONSOLE_Print("[GHOST] warn - dota_max_win_chance_diff incorrect range, disabling");
	}
	if (DotaMaxWinChanceDiffGainConstant > 100 || DotaMaxWinChanceDiffGainConstant < 0) {
		DotaMaxWinChanceDiffGainConstant = 0;
		CONSOLE_Print("[GHOST] warn - dota_max_win_chance_diff_gain_constant incorrect range, disabling");
	}
	if (DotaMaxWLDiff > 100 || DotaMaxWLDiff < 0) {
		DotaMaxWLDiff = 0;
		CONSOLE_Print("[GHOST] warn - dota_max_wl_diff incorrect range, disabling");
	}
	m_DotaMaxWinChanceDiff = DotaMaxWinChanceDiff;
	m_DotaMaxWinChanceDiffGainConstant = DotaMaxWinChanceDiffGainConstant;
	m_DotaMaxWLDiff = DotaMaxWLDiff;

	m_ContributorOnlyMode = false;
}

CGHost :: ~CGHost( )
{
	delete m_UDPSocket;
	delete m_ReconnectSocket;

	for( vector<CTCPSocket *> :: iterator i = m_ReconnectSockets.begin( ); i != m_ReconnectSockets.end( ); ++i )
		delete *i;

	delete m_GPSProtocol;
	delete m_CRC;
	delete m_SHA;

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		delete *i;

	delete m_CurrentGame;
	//delete m_AdminGame;

	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); ++i )
		delete *i;

	//delete m_DB;
	//delete m_DBLocal;

	// warning: we don't delete any entries of m_Callables here because we can't be guaranteed that the associated threads have terminated
	// this is fine if the program is currently exiting because the OS will clean up after us
	// but if you try to recreate the CGHost object within a single session you will probably leak resources!

	if( !m_Callables.empty( ) )
		CONSOLE_Print( "[GHOST] warning - " + UTIL_ToString( m_Callables.size( ) ) + " orphaned callables were leaked (this is not an error)" );

	delete m_Language;
	delete m_Map;
	//delete m_AdminMap;
	//delete m_AutoHostMap;
	delete m_SaveGame;

	delete m_Manager;

	if (m_restrictedW3Version != nullptr) {
		delete m_restrictedW3Version;
	}
}

bool CGHost :: Update( long usecBlock )
{
	uint32_t startTime = GetTicks();
	MILLISLEEP( m_BlockMS );

	// todotodo: do we really want to shutdown if there's a database error? is there any way to recover from this?

	/*if( m_DB->HasError( ) )
	{
		CONSOLE_Print( "[GHOST] database error - " + m_DB->GetError( ) );
		return true;
	}

	if( m_DBLocal->HasError( ) )
	{
		CONSOLE_Print( "[GHOST] local database error - " + m_DBLocal->GetError( ) );
		return true;
	}*/

	// try to exit nicely if requested to do so

	if( m_ExitingNice )
	{
		if( !m_BNETs.empty( ) )
		{
			CONSOLE_Print( "[GHOST] deleting all battle.net connections in preparation for exiting nicely" );

			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
				delete *i;

			m_BNETs.clear( );
		}

		if( m_CurrentGame )
		{
			CONSOLE_Print( "[GHOST] deleting current game in preparation for exiting nicely" );
			delete m_CurrentGame;
			m_CurrentGame = NULL;
		}

		/*if( m_AdminGame )
		{
			CONSOLE_Print( "[GHOST] deleting admin game in preparation for exiting nicely" );
			delete m_AdminGame;
			m_AdminGame = NULL;
		}*/

		if( m_Games.empty( ) )
		{
			if( !m_AllGamesFinished )
			{
				CONSOLE_Print( "[GHOST] all games finished, waiting 60 seconds for threads to finish" );
				CONSOLE_Print( "[GHOST] there are " + UTIL_ToString( m_Callables.size( ) ) + " threads in progress" );
				m_AllGamesFinished = true;
				m_AllGamesFinishedTime = GetTime( );
			}
			else
			{
				if( m_Callables.empty( ) )
				{
					CONSOLE_Print( "[GHOST] all threads finished, exiting nicely" );
					m_Exiting = true;
				}
				else if( GetTime( ) - m_AllGamesFinishedTime >= 60 )
				{
					CONSOLE_Print( "[GHOST] waited 60 seconds for threads to finish, exiting anyway" );
					CONSOLE_Print( "[GHOST] there are " + UTIL_ToString( m_Callables.size( ) ) + " threads still in progress which will be terminated" );
					m_Exiting = true;
				}
			}
		}
	}

	// update callables

	/*for( vector<CBaseCallable *> :: iterator i = m_Callables.begin( ); i != m_Callables.end( ); )
	{
		if( (*i)->GetReady( ) )
		{
			m_DB->RecoverCallable( *i );
			delete *i;
			i = m_Callables.erase( i );
		}
		else
			++i;
	}*/

	// create the GProxy++ reconnect listener

	if( m_Reconnect )
	{
		if( !m_ReconnectSocket )
		{
			m_ReconnectSocket = new CTCPServer( );

			if( m_ReconnectSocket->Listen( m_BindAddress, m_ReconnectPort ) )
				CONSOLE_Print( "[GHOST] listening for GProxy++ reconnects on port " + UTIL_ToString( m_ReconnectPort ) );
			else
			{
				CONSOLE_Print( "[GHOST] error listening for GProxy++ reconnects on port " + UTIL_ToString( m_ReconnectPort ) );
				delete m_ReconnectSocket;
				m_ReconnectSocket = NULL;
				m_Reconnect = false;
			}
		}
		else if( m_ReconnectSocket->HasError( ) )
		{
			CONSOLE_Print( "[GHOST] GProxy++ reconnect listener error (" + m_ReconnectSocket->GetErrorString( ) + ")" );
			delete m_ReconnectSocket;
			m_ReconnectSocket = NULL;
			m_Reconnect = false;
		}
	}

	unsigned int NumFDs = 0;

	// take every socket we own and throw it in one giant select statement so we can block on all sockets

	int nfds = 0;
	fd_set fd;
	fd_set send_fd;
	FD_ZERO( &fd );
	FD_ZERO( &send_fd );

	// 1. all battle.net sockets

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		NumFDs += (*i)->SetFD( &fd, &send_fd, &nfds );

	// 2. the current game's server and player sockets

	if( m_CurrentGame )
		NumFDs += m_CurrentGame->SetFD( &fd, &send_fd, &nfds );

	// 3. the admin game's server and player sockets

	/*if( m_AdminGame )
		NumFDs += m_AdminGame->SetFD( &fd, &send_fd, &nfds );*/

	// 4. all running games' player sockets

	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); ++i )
		NumFDs += (*i)->SetFD( &fd, &send_fd, &nfds );

	// 5. the GProxy++ reconnect socket(s)

	if( m_Reconnect && m_ReconnectSocket )
	{
		m_ReconnectSocket->SetFD( &fd, &send_fd, &nfds );
		++NumFDs;
	}

	for( vector<CTCPSocket *> :: iterator i = m_ReconnectSockets.begin( ); i != m_ReconnectSockets.end( ); ++i )
	{
		(*i)->SetFD( &fd, &send_fd, &nfds );
		++NumFDs;
	}

	// before we call select we need to determine how long to block for
	// previously we just blocked for a maximum of the passed usecBlock microseconds
	// however, in an effort to make game updates happen closer to the desired latency setting we now use a dynamic block interval
	// note: we still use the passed usecBlock as a hard maximum

	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); ++i )
	{
		if( (*i)->GetNextTimedActionTicks( ) * 1000 < usecBlock )
			usecBlock = (*i)->GetNextTimedActionTicks( ) * 1000;
	}

	// always block for at least 1ms just in case something goes wrong
	// this prevents the bot from sucking up all the available CPU if a game keeps asking for immediate updates
	// it's a bit ridiculous to include this check since, in theory, the bot is programmed well enough to never make this mistake
	// however, considering who programmed it, it's worthwhile to do it anyway

	if( usecBlock < 1000 )
		usecBlock = 1000;

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = usecBlock;

	struct timeval send_tv;
	send_tv.tv_sec = 0;
	send_tv.tv_usec = 0;

#ifdef WIN32
	select( 1, &fd, NULL, NULL, &tv );
	select( 1, NULL, &send_fd, NULL, &send_tv );
#else
	select( nfds + 1, &fd, NULL, NULL, &tv );
	select( nfds + 1, NULL, &send_fd, NULL, &send_tv );
#endif

	if( NumFDs == 0 )
	{
		// we don't have any sockets (i.e. we aren't connected to battle.net maybe due to a lost connection and there aren't any games running)
		// select will return immediately and we'll chew up the CPU if we let it loop so just sleep for 50ms to kill some time

		MILLISLEEP( 50 );
	}

	bool AdminExit = false;
	bool BNETExit = false;

	// update current game

	if( m_CurrentGame )
	{
		if( m_CurrentGame->Update( &fd, &send_fd ) )
		{
			m_Manager->SendGameUnhosted( m_CurrentGame->GetGameID( ) );

			CONSOLE_Print( "[GHOST] deleting current game [" + m_CurrentGame->GetGameName( ) + "]" );
			delete m_CurrentGame;
			m_CurrentGame = NULL;

			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
			{
				(*i)->QueueGameUncreate( );
				(*i)->QueueEnterChat( );
			}
		}
		else if( m_CurrentGame )
			m_CurrentGame->UpdatePost( &send_fd );
	}

	// update admin game

	/*if( m_AdminGame )
	{
		if( m_AdminGame->Update( &fd, &send_fd ) )
		{
			CONSOLE_Print( "[GHOST] deleting admin game" );
			delete m_AdminGame;
			m_AdminGame = NULL;
			AdminExit = true;
		}
		else if( m_AdminGame )
			m_AdminGame->UpdatePost( &send_fd );
	}*/

	// update running games

	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); )
	{
		if( (*i)->Update( &fd, &send_fd ) )
		{
			CONSOLE_Print( "[GHOST] deleting game [" + (*i)->GetGameName( ) + "]" );
			EventGameDeleted( *i );
			delete *i;
			i = m_Games.erase( i );
		}
		else
		{
			(*i)->UpdatePost( &send_fd );
			++i;
		}
	}

	// update battle.net connections

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
	{
		if( (*i)->Update( &fd, &send_fd ) )
			BNETExit = true;
	}

	// update GProxy++ reliable reconnect sockets
	uint32_t gpstartTime = GetTicks();
	if( m_Reconnect && m_ReconnectSocket )
	{
		CTCPSocket *NewSocket = m_ReconnectSocket->Accept( &fd );

		if( NewSocket )
			m_ReconnectSockets.push_back( NewSocket );
	}

	for( vector<CTCPSocket *> :: iterator i = m_ReconnectSockets.begin( ); i != m_ReconnectSockets.end( ); )
	{
		if( (*i)->HasError( ) || !(*i)->GetConnected( ) || GetTime( ) - (*i)->GetLastRecv( ) >= 10 )
		{
			delete *i;
			i = m_ReconnectSockets.erase( i );
			continue;
		}

		(*i)->DoRecv( &fd );
		string *RecvBuffer = (*i)->GetBytes( );
		BYTEARRAY Bytes = UTIL_CreateByteArray( (unsigned char *)RecvBuffer->c_str( ), RecvBuffer->size( ) );

		// a packet is at least 4 bytes

		if( Bytes.size( ) >= 4 )
		{
			if( Bytes[0] == GPS_HEADER_CONSTANT )
			{
				// bytes 2 and 3 contain the length of the packet

				uint16_t Length = UTIL_ByteArrayToUInt16( Bytes, false, 2 );

				if( Length >= 4 )
				{
					if( Bytes.size( ) >= Length )
					{
						if( Bytes[1] == CGPSProtocol :: GPS_RECONNECT && Length == 13 )
						{
							unsigned char PID = Bytes[4];
							uint32_t ReconnectKey = UTIL_ByteArrayToUInt32( Bytes, false, 5 );
							uint32_t LastPacket = UTIL_ByteArrayToUInt32( Bytes, false, 9 );

							// look for a matching player in a running game

							CGamePlayer *Match = NULL;

							for( vector<CBaseGame *> :: iterator j = m_Games.begin( ); j != m_Games.end( ); ++j )
							{
								if( (*j)->GetGameLoaded( ) )
								{
									CGamePlayer *Player = (*j)->GetPlayerFromPID( PID );

									if( Player && Player->GetGProxy( ) && Player->GetGProxyReconnectKey( ) == ReconnectKey )
									{
										Match = Player;
										break;
									}
								}
							}

							if( Match )
							{
								// reconnect successful!

								*RecvBuffer = RecvBuffer->substr( Length );
								Match->EventGProxyReconnect( *i, LastPacket );
								i = m_ReconnectSockets.erase( i );
								continue;
							}
							else
							{
								(*i)->PutBytes( m_GPSProtocol->SEND_GPSS_REJECT( REJECTGPS_NOTFOUND ) );
								(*i)->DoSend( &send_fd );
								delete *i;
								i = m_ReconnectSockets.erase( i );
								continue;
							}
						}
						else
						{
							(*i)->PutBytes( m_GPSProtocol->SEND_GPSS_REJECT( REJECTGPS_INVALID ) );
							(*i)->DoSend( &send_fd );
							delete *i;
							i = m_ReconnectSockets.erase( i );
							continue;
						}
					}
				}
				else
				{
					(*i)->PutBytes( m_GPSProtocol->SEND_GPSS_REJECT( REJECTGPS_INVALID ) );
					(*i)->DoSend( &send_fd );
					delete *i;
					i = m_ReconnectSockets.erase( i );
					continue;
				}
			}
			else
			{
				(*i)->PutBytes( m_GPSProtocol->SEND_GPSS_REJECT( REJECTGPS_INVALID ) );
				(*i)->DoSend( &send_fd );
				delete *i;
				i = m_ReconnectSockets.erase( i );
				continue;
			}
		}

		(*i)->DoSend( &send_fd );
		++i;
	}
	uint32_t gpendTime = GetTicks();
	// autohost

	/*if( !m_AutoHostGameName.empty( ) && m_AutoHostMaximumGames != 0 && m_AutoHostAutoStartPlayers != 0 && GetTime( ) - m_LastAutoHostTime >= 30 )
	{
		// copy all the checks from CGHost :: CreateGame here because we don't want to spam the chat when there's an error
		// instead we fail silently and try again soon

		if( !m_ExitingNice && m_Enabled && !m_CurrentGame && m_Games.size( ) < m_MaxGames && m_Games.size( ) < m_AutoHostMaximumGames )
		{
			if( m_AutoHostMap->GetValid( ) )
			{
				string GameName = m_AutoHostGameName + " #" + UTIL_ToString( m_HostCounter );

				if( GameName.size( ) <= 31 )
				{
					CreateGame( m_AutoHostMap, GAME_PUBLIC, false, GameName, m_AutoHostOwner, m_AutoHostOwner, m_AutoHostServer, false );

					if( m_CurrentGame )
					{
						m_CurrentGame->SetAutoStartPlayers( m_AutoHostAutoStartPlayers );

						if( m_AutoHostMatchMaking )
						{
							if( !m_Map->GetMapMatchMakingCategory( ).empty( ) )
							{
								if( !( m_Map->GetMapOptions( ) & MAPOPT_FIXEDPLAYERSETTINGS ) )
									CONSOLE_Print( "[GHOST] autohostmm - map_matchmakingcategory [" + m_Map->GetMapMatchMakingCategory( ) + "] found but matchmaking can only be used with fixed player settings, matchmaking disabled" );
								else
								{
									CONSOLE_Print( "[GHOST] autohostmm - map_matchmakingcategory [" + m_Map->GetMapMatchMakingCategory( ) + "] found, matchmaking enabled" );

									m_CurrentGame->SetMatchMaking( true );
									m_CurrentGame->SetMinimumScore( m_AutoHostMinimumScore );
									m_CurrentGame->SetMaximumScore( m_AutoHostMaximumScore );
								}
							}
							else
								CONSOLE_Print( "[GHOST] autohostmm - map_matchmakingcategory not found, matchmaking disabled" );
						}
					}
				}
				else
				{
					CONSOLE_Print( "[GHOST] stopped auto hosting, next game name [" + GameName + "] is too long (the maximum is 31 characters)" );
					m_AutoHostGameName.clear( );
					m_AutoHostOwner.clear( );
					m_AutoHostServer.clear( );
					m_AutoHostMaximumGames = 0;
					m_AutoHostAutoStartPlayers = 0;
					m_AutoHostMatchMaking = false;
					m_AutoHostMinimumScore = 0.0;
					m_AutoHostMaximumScore = 0.0;
				}
			}
			else
			{
				CONSOLE_Print( "[GHOST] stopped auto hosting, map config file [" + m_AutoHostMap->GetCFGFile( ) + "] is invalid" );
				m_AutoHostGameName.clear( );
				m_AutoHostOwner.clear( );
				m_AutoHostServer.clear( );
				m_AutoHostMaximumGames = 0;
				m_AutoHostAutoStartPlayers = 0;
				m_AutoHostMatchMaking = false;
				m_AutoHostMinimumScore = 0.0;
				m_AutoHostMaximumScore = 0.0;
			}
		}

		m_LastAutoHostTime = GetTime( );
	}*/

	m_Manager->Update( );
	uint32_t endTime = GetTicks();
	uint32_t totaltime = endTime-startTime;
	uint32_t gptime = gpendTime - gpstartTime;
	
	/*if (totaltime > 100) {
		CONSOLE_Print("[TIMING DEBUG] Over 20ms, totaltime is "+UTIL_ToString(totaltime)+" and gp time is "+UTIL_ToString(gptime));
	}*/

	return m_Exiting || AdminExit || BNETExit;
}

void CGHost :: EventBNETConnecting( CBNET *bnet )
{
	/*if( m_AdminGame )
		m_AdminGame->SendAllChat( m_Language->ConnectingToBNET( bnet->GetServer( ) ) );*/

	/*if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->ConnectingToBNET( bnet->GetServer( ) ) );*/
}

void CGHost :: EventBNETConnected( CBNET *bnet )
{
	/*if( m_AdminGame )
		m_AdminGame->SendAllChat( m_Language->ConnectedToBNET( bnet->GetServer( ) ) );*/

	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->ConnectedToBNET( bnet->GetServer( ) ) );

	//m_Manager->SendConnectedToBNET( bnet->GetServerID( ) );
}

void CGHost :: EventBNETDisconnected( CBNET *bnet )
{
	/*if( m_AdminGame )
		m_AdminGame->SendAllChat( m_Language->DisconnectedFromBNET( bnet->GetServer( ) ) );*/

	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->DisconnectedFromBNET( bnet->GetServer( ) ) );

	m_Manager->SendDisconnectedFromBNET( bnet->GetServerID( ) );
}

void CGHost :: EventBNETLoggedIn( CBNET *bnet )
{
	/*if( m_AdminGame )
		m_AdminGame->SendAllChat( m_Language->LoggedInToBNET( bnet->GetServer( ) ) );*/

	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->LoggedInToBNET( bnet->GetServer( ) ) );

	m_Manager->SendLoggedInToBNET( bnet->GetServerID( ) );

}

void CGHost :: EventBNETGameRefreshed( CBNET *bnet )
{
	/*if( m_AdminGame )
		m_AdminGame->SendAllChat( m_Language->BNETGameHostingSucceeded( bnet->GetServer( ) ) );*/

	if( m_CurrentGame )
		m_CurrentGame->EventGameRefreshed( bnet->GetServer( ) );
}

void CGHost :: EventBNETGameRefreshFailed( CBNET *bnet )
{
	if( m_CurrentGame )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		{
			(*i)->QueueChatCommand( m_Language->UnableToCreateGameTryAnotherName( bnet->GetServer( ), m_CurrentGame->GetGameName( ) ) );

			if( (*i)->GetServer( ) == m_CurrentGame->GetCreatorServer( ) )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameTryAnotherName( bnet->GetServer( ), m_CurrentGame->GetGameName( ) ), m_CurrentGame->GetCreatorName( ), true );
		}

		/*if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->BNETGameHostingFailed( bnet->GetServer( ), m_CurrentGame->GetGameName( ) ) );*/

		m_CurrentGame->SendAllChat( m_Language->UnableToCreateGameTryAnotherName( bnet->GetServer( ), m_CurrentGame->GetGameName( ) ) );

		// we take the easy route and simply close the lobby if a refresh fails
		// it's possible at least one refresh succeeded and therefore the game is still joinable on at least one battle.net (plus on the local network) but we don't keep track of that
		// we only close the game if it has no players since we support game rehosting (via !priv and !pub in the lobby)

		if( m_CurrentGame->GetNumHumanPlayers( ) == 0 )
		{
			m_CurrentGame->SetExiting( true );

			m_Manager->SendErrorGameNameExists( m_CurrentGame->GetGameName( ) );
		}

		m_CurrentGame->SetRefreshError( true );
	}
}

void CGHost :: EventBNETConnectTimedOut( CBNET *bnet )
{
	/*if( m_AdminGame )
		m_AdminGame->SendAllChat( m_Language->ConnectingToBNETTimedOut( bnet->GetServer( ) ) );*/

	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->ConnectingToBNETTimedOut( bnet->GetServer( ) ) );
}

void CGHost :: EventBNETWhisper( CBNET *bnet, string user, string message )
{
	/*if( m_AdminGame )
	{
		m_AdminGame->SendAdminChat( "[W: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		if( m_CurrentGame )
			m_CurrentGame->SendLocalAdminChat( "[W: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); ++i )
			(*i)->SendLocalAdminChat( "[W: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );
	}*/
}

void CGHost :: EventBNETChat( CBNET *bnet, string user, string message )
{
	/*if( m_AdminGame )
	{
		m_AdminGame->SendAdminChat( "[L: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		if( m_CurrentGame )
			m_CurrentGame->SendLocalAdminChat( "[L: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); ++i )
			(*i)->SendLocalAdminChat( "[L: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );
	}*/
}

void CGHost :: EventBNETEmote( CBNET *bnet, string user, string message )
{
	/*if( m_AdminGame )
	{
		m_AdminGame->SendAdminChat( "[E: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		if( m_CurrentGame )
			m_CurrentGame->SendLocalAdminChat( "[E: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); ++i )
			(*i)->SendLocalAdminChat( "[E: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );
	}*/
}

void CGHost :: EventGameDeleted( CBaseGame *game )
{
	/*for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
	{
		(*i)->QueueChatCommand( m_Language->GameIsOver( game->GetDescription( ) ) );

		if( (*i)->GetServer( ) == game->GetCreatorServer( ) )
			(*i)->QueueChatCommand( m_Language->GameIsOver( game->GetDescription( ) ), game->GetCreatorName( ), true );
	}*/

	m_Manager->SendGameIsOver( game->GetGameID( ) );
}

void CGHost :: ReloadConfigs( )
{
	CConfig CFG;
	CFG.Read( "default.cfg" );
	CFG.Read( m_CFGFile );
	SetConfigs( &CFG );

	//TODO: reload map.cfg
}

void CGHost :: SetConfigs( CConfig *CFG )
{
	// this doesn't set EVERY config value since that would potentially require reconfiguring the battle.net connections
	// it just set the easily reloadable values

	m_LanguageFile = CFG->GetString( "bot_language", "language.cfg" );
	delete m_Language;
	m_Language = new CLanguage( m_LanguageFile );
	m_Warcraft3Path = UTIL_AddPathSeperator( CFG->GetString( "bot_war3path", "C:\\Program Files\\Warcraft III\\" ) );
	m_BindAddress = CFG->GetString( "bot_bindaddress", string( ) );
	m_ReconnectWaitTime = CFG->GetInt( "bot_reconnectwaittime", 3 );
	m_MaxGames = CFG->GetInt( "bot_maxgames", 5 );
	string BotCommandTrigger = CFG->GetString( "bot_commandtrigger", "!" );

	if( BotCommandTrigger.empty( ) )
		BotCommandTrigger = "!";

	m_CommandTrigger = BotCommandTrigger[0];
	m_MapCFGPath = UTIL_AddPathSeperator( CFG->GetString( "bot_mapcfgpath", string( ) ) );
	m_SaveGamePath = UTIL_AddPathSeperator( CFG->GetString( "bot_savegamepath", string( ) ) );
	m_MapPath = UTIL_AddPathSeperator( CFG->GetString( "bot_mappath", string( ) ) );
	m_SaveReplays = CFG->GetInt( "bot_savereplays", 0 ) == 0 ? false : true;
	m_ReplayPath = UTIL_AddPathSeperator( CFG->GetString( "bot_replaypath", string( ) ) );
	m_VirtualHostName = CFG->GetString( "bot_virtualhostname", "|cFF4080C0GHost" );
	m_HideIPAddresses = CFG->GetInt( "bot_hideipaddresses", 0 ) == 0 ? false : true;
	m_CheckMultipleIPUsage = CFG->GetInt( "bot_checkmultipleipusage", 1 ) == 0 ? false : true;

	if( m_VirtualHostName.size( ) > 15 )
	{
		m_VirtualHostName = "|cFF4080C0GHost";
		CONSOLE_Print( "[GHOST] warning - bot_virtualhostname is longer than 15 characters, using default virtual host name" );
	}

	m_SpoofChecks = CFG->GetInt( "bot_spoofchecks", 2 );
	m_RequireSpoofChecks = CFG->GetInt( "bot_requirespoofchecks", 0 ) == 0 ? false : true;
	m_RequireMuteChecks = CFG->GetInt("bot_requiremutechecks", 0) == 0 ? false : true;
	m_ReserveAdmins = CFG->GetInt( "bot_reserveadmins", 1 ) == 0 ? false : true;
	m_RefreshMessages = CFG->GetInt( "bot_refreshmessages", 0 ) == 0 ? false : true;
	m_AutoLock = CFG->GetInt( "bot_autolock", 0 ) == 0 ? false : true;
	m_AutoSave = CFG->GetInt( "bot_autosave", 0 ) == 0 ? false : true;
	m_AllowDownloads = CFG->GetInt( "bot_allowdownloads", 0 );
	m_PingDuringDownloads = CFG->GetInt( "bot_pingduringdownloads", 0 ) == 0 ? false : true;
	m_MaxDownloaders = CFG->GetInt( "bot_maxdownloaders", 3 );
	m_MaxDownloadSpeed = CFG->GetInt( "bot_maxdownloadspeed", 100 );
	m_LCPings = CFG->GetInt( "bot_lcpings", 1 ) == 0 ? false : true;
	m_AutoKickPing = CFG->GetInt( "bot_autokickping", 400 );
	m_BanMethod = CFG->GetInt( "bot_banmethod", 1 );
	m_IPBlackListFile = CFG->GetString( "bot_ipblacklistfile", "ipblacklist.txt" );
	m_LobbyTimeLimit = CFG->GetInt( "bot_lobbytimelimit", 10 );
	m_Latency = CFG->GetInt( "bot_latency", 100 );
	m_MinLatency = CFG->GetInt("bot_latency_min", 80);
	m_MaxLatency = CFG->GetInt("bot_latency_max", 120);
	m_SyncLimit = CFG->GetInt( "bot_synclimit", 150 );
	m_MinSyncLimit = CFG->GetInt("bot_synclimit_min", 50);
	m_MaxSyncLimit = CFG->GetInt("bot_synclimit_max", 300);
	m_VoteKickAllowed = CFG->GetInt( "bot_votekickallowed", 1 ) == 0 ? false : true;
	m_VoteKickPercentage = CFG->GetInt( "bot_votekickpercentage", 100 );

	if( m_VoteKickPercentage > 100 )
	{
		m_VoteKickPercentage = 100;
		CONSOLE_Print( "[GHOST] warning - bot_votekickpercentage is greater than 100, using 100 instead" );
	}

	m_MOTDFile = CFG->GetString( "bot_motdfile", "motd.txt" );
	m_GameLoadedFile = CFG->GetString( "bot_gameloadedfile", "gameloaded.txt" );
	m_GameOverFile = CFG->GetString( "bot_gameoverfile", "gameover.txt" );
	m_LocalAdminMessages = CFG->GetInt( "bot_localadminmessages", 1 ) == 0 ? false : true;
	m_TCPNoDelay = CFG->GetInt( "tcp_nodelay", 0 ) == 0 ? false : true;
	m_MatchMakingMethod = CFG->GetInt( "bot_matchmakingmethod", 1 );

	m_GHostGroup = CFG->GetInt( "bot_ghost_group", MASL_PROTOCOL :: GHOST_GROUP_ALL );

	//reload gproxy for bnet
	uint32_t XPAMGProxyVersion = CFG->GetInt("bnet_custom_xpamgproxyversion", 0);
	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
	{
		(*i)->SetXPAMGProxyVersion(XPAMGProxyVersion);
	}

	//Restricted mode - players need a specific w3 version to join, using extended GPS protocol
	m_restrictW3Version = CFG->GetInt("bot_restrict_w3_version", 0) == 0 ? false : true;
	string w3vs = CFG->GetString("bot_restricted_w3_version", "1.26.0.6401");
	W3Version * w3v = UTIL_toW3Version(w3vs);
	if (w3v == nullptr) { //parse error
		w3v = new W3Version(1, 26, 0, 6401);
	}
	m_restrictedW3Version = w3v;
	if (m_restrictW3Version) {
		CONSOLE_Print("[GHOST] Restricted mode is enabled, accepting w3 version "+w3v->toString());
	}
	else {
		CONSOLE_Print("[GHOST] Restricted mode is disabled");
	}

	m_msGameTimeBeforeAutoban = CFG->GetInt("bot_ms_game_time_before_autoban", 360);

	//Use new (1) or old (0) formula to adjust PSR after game
	m_UseNewPSRFormula = CFG->GetInt("dota_usenewpsrformula", 0) == 0 ? false : true;
}

void CGHost :: ExtractScripts( )
{
	string PatchMPQFileName = m_Warcraft3Path + "War3Patch.mpq";
	HANDLE PatchMPQ;

	if( SFileOpenArchive( PatchMPQFileName.c_str( ), 0, MPQ_OPEN_FORCE_MPQ_V1, &PatchMPQ ) )
	{
		CONSOLE_Print( "[GHOST] loading MPQ file [" + PatchMPQFileName + "]" );
		HANDLE SubFile;

		// common.j

		if( SFileOpenFileEx( PatchMPQ, "Scripts\\common.j", 0, &SubFile ) )
		{
			uint32_t FileLength = SFileGetFileSize( SubFile, NULL );

			if( FileLength > 0 && FileLength != 0xFFFFFFFF )
			{
				char *SubFileData = new char[FileLength];
				DWORD BytesRead = 0;

				if( SFileReadFile( SubFile, SubFileData, FileLength, &BytesRead, 0 ) )
				{
					CONSOLE_Print( "[GHOST] extracting Scripts\\common.j from MPQ file to [" + m_MapCFGPath + "common.j]" );
					UTIL_FileWrite( m_MapCFGPath + "common.j", (unsigned char *)SubFileData, BytesRead );
				}
				else
					CONSOLE_Print( "[GHOST] warning - unable to extract Scripts\\common.j from MPQ file" );

				delete [] SubFileData;
			}

			SFileCloseFile( SubFile );
		}
		else
			CONSOLE_Print( "[GHOST] couldn't find Scripts\\common.j in MPQ file" );

		// blizzard.j

		if( SFileOpenFileEx( PatchMPQ, "Scripts\\blizzard.j", 0, &SubFile ) )
		{
			uint32_t FileLength = SFileGetFileSize( SubFile, NULL );

			if( FileLength > 0 && FileLength != 0xFFFFFFFF )
			{
				char *SubFileData = new char[FileLength];
				DWORD BytesRead = 0;

				if( SFileReadFile( SubFile, SubFileData, FileLength, &BytesRead, 0) )
				{
					CONSOLE_Print( "[GHOST] extracting Scripts\\blizzard.j from MPQ file to [" + m_MapCFGPath + "blizzard.j]" );
					UTIL_FileWrite( m_MapCFGPath + "blizzard.j", (unsigned char *)SubFileData, BytesRead );
				}
				else
					CONSOLE_Print( "[GHOST] warning - unable to extract Scripts\\blizzard.j from MPQ file" );

				delete [] SubFileData;
			}

			SFileCloseFile( SubFile );
		}
		else
			CONSOLE_Print( "[GHOST] couldn't find Scripts\\blizzard.j in MPQ file" );

		SFileCloseArchive( PatchMPQ );
	}
	else
		CONSOLE_Print( "[GHOST] warning - unable to load MPQ file [" + PatchMPQFileName + "] - error code " + UTIL_ToString( GetLastError( ) ) );
}

/*void CGHost :: LoadIPToCountryData( )
{
	ifstream in;
	in.open( "ip-to-country.csv" );

	if( in.fail( ) )
		CONSOLE_Print( "[GHOST] warning - unable to read file [ip-to-country.csv], iptocountry data not loaded" );
	else
	{
		CONSOLE_Print( "[GHOST] started loading [ip-to-country.csv]" );

		// the begin and commit statements are optimizations
		// we're about to insert ~4 MB of data into the database so if we allow the database to treat each insert as a transaction it will take a LONG time
		// todotodo: handle begin/commit failures a bit more gracefully

		if( !m_DBLocal->Begin( ) )
			CONSOLE_Print( "[GHOST] warning - failed to begin local database transaction, iptocountry data not loaded" );
		else
		{
			unsigned char Percent = 0;
			string Line;
			string IP1;
			string IP2;
			string Country;
			CSVParser parser;

			// get length of file for the progress meter

			in.seekg( 0, ios :: end );
			uint32_t FileLength = in.tellg( );
			in.seekg( 0, ios :: beg );

			while( !in.eof( ) )
			{
				getline( in, Line );

				if( Line.empty( ) )
					continue;

				parser << Line;
				parser >> IP1;
				parser >> IP2;
				parser >> Country;
				m_DBLocal->FromAdd( UTIL_ToUInt32( IP1 ), UTIL_ToUInt32( IP2 ), Country );

				// it's probably going to take awhile to load the iptocountry data (~10 seconds on my 3.2 GHz P4 when using SQLite3)
				// so let's print a progress meter just to keep the user from getting worried

				unsigned char NewPercent = (unsigned char)( (float)in.tellg( ) / FileLength * 100 );

				if( NewPercent != Percent && NewPercent % 10 == 0 )
				{
					Percent = NewPercent;
					CONSOLE_Print( "[GHOST] iptocountry data: " + UTIL_ToString( Percent ) + "% loaded" );
				}
			}

			if( !m_DBLocal->Commit( ) )
				CONSOLE_Print( "[GHOST] warning - failed to commit local database transaction, iptocountry data not loaded" );
			else
				CONSOLE_Print( "[GHOST] finished loading [ip-to-country.csv]" );
		}

		in.close( );
	}
}*/

void CGHost :: LoadCustomGameMap( CConfig *CFG, string nCFGFile )
{
	if( m_CustomGameMap )
		delete m_CustomGameMap;

	m_CustomGameMap = new CMap( this, CFG, nCFGFile );
}

void CGHost :: CreateGame( CMap *map, unsigned char gameState, bool saveGame, string gameName, string ownerName, 
	string creatorName, string creatorServer, uint32_t gameType, bool privobs, vector<string> heldPlayers )
{
	// no support for saved games ATM

	if( saveGame )
		return;

	if( !m_Enabled )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		{
			/*if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameDisabled( gameName ), creatorName, whisper );*/

			if( (*i)->GetServer( ) == creatorServer )
			{
				if( (*i)->GetPasswordHashType( ) == "pvpgn" )
					(*i)->SendChatCommand( m_Language->UnableToCreateGameDisabled( gameName ), creatorName );
				else
					(*i)->QueueChatCommand( m_Language->UnableToCreateGameDisabled( gameName ), creatorName, true );
			}
		}

		/*if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->UnableToCreateGameDisabled( gameName ) );*/

		return;
	}

	if( gameName.size( ) > 31 )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		{
			/*if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameNameTooLong( gameName ), creatorName, whisper );*/

			if( (*i)->GetServer( ) == creatorServer )
			{
				if( (*i)->GetPasswordHashType( ) == "pvpgn" )
					(*i)->SendChatCommand( m_Language->UnableToCreateGameNameTooLong( gameName ), creatorName );
				else
					(*i)->QueueChatCommand( m_Language->UnableToCreateGameNameTooLong( gameName ), creatorName, true );
			}
		}

		/*if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->UnableToCreateGameNameTooLong( gameName ) );*/

		return;
	}

	if( !map->GetValid( ) )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		{
			/*if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameInvalidMap( gameName ), creatorName, whisper );*/

			if( (*i)->GetServer( ) == creatorServer )
			{
				if( (*i)->GetPasswordHashType( ) == "pvpgn" )
					(*i)->SendChatCommand( m_Language->UnableToCreateGameInvalidMap( gameName ), creatorName );
				else
					(*i)->QueueChatCommand( m_Language->UnableToCreateGameInvalidMap( gameName ), creatorName, true );
			}
		}

		/*if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->UnableToCreateGameInvalidMap( gameName ) );*/

		return;
	}

	if( saveGame )
	{
		if( !m_SaveGame->GetValid( ) )
		{
			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
			{
				/*if( (*i)->GetServer( ) == creatorServer )
					(*i)->QueueChatCommand( m_Language->UnableToCreateGameInvalidSaveGame( gameName ), creatorName, whisper );*/

				if( (*i)->GetServer( ) == creatorServer )
				{
					if( (*i)->GetPasswordHashType( ) == "pvpgn" )
						(*i)->SendChatCommand( m_Language->UnableToCreateGameInvalidSaveGame( gameName ), creatorName );
					else
						(*i)->QueueChatCommand( m_Language->UnableToCreateGameInvalidSaveGame( gameName ), creatorName, true );
				}
			}

			/*if( m_AdminGame )
				m_AdminGame->SendAllChat( m_Language->UnableToCreateGameInvalidSaveGame( gameName ) );*/

			return;
		}

		string MapPath1 = m_SaveGame->GetMapPath( );
		string MapPath2 = map->GetMapPath( );
		transform( MapPath1.begin( ), MapPath1.end( ), MapPath1.begin( ), (int(*)(int))tolower );
		transform( MapPath2.begin( ), MapPath2.end( ), MapPath2.begin( ), (int(*)(int))tolower );

		if( MapPath1 != MapPath2 )
		{
			CONSOLE_Print( "[GHOST] path mismatch, saved game path is [" + MapPath1 + "] but map path is [" + MapPath2 + "]" );

			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
			{
				/*if( (*i)->GetServer( ) == creatorServer )
					(*i)->QueueChatCommand( m_Language->UnableToCreateGameSaveGameMapMismatch( gameName ), creatorName, whisper );*/

				if( (*i)->GetServer( ) == creatorServer )
				{
					if( (*i)->GetPasswordHashType( ) == "pvpgn" )
						(*i)->SendChatCommand( m_Language->UnableToCreateGameSaveGameMapMismatch( gameName ), creatorName );
					else
						(*i)->QueueChatCommand( m_Language->UnableToCreateGameSaveGameMapMismatch( gameName ), creatorName, true );
				}
			}

			/*if( m_AdminGame )
				m_AdminGame->SendAllChat( m_Language->UnableToCreateGameSaveGameMapMismatch( gameName ) );*/

			return;
		}

		if( m_EnforcePlayers.empty( ) )
		{
			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
			{
				/*if( (*i)->GetServer( ) == creatorServer )
					(*i)->QueueChatCommand( m_Language->UnableToCreateGameMustEnforceFirst( gameName ), creatorName, whisper );*/

				if( (*i)->GetServer( ) == creatorServer )
				{
					if( (*i)->GetPasswordHashType( ) == "pvpgn" )
						(*i)->SendChatCommand( m_Language->UnableToCreateGameMustEnforceFirst( gameName ), creatorName );
					else
						(*i)->QueueChatCommand( m_Language->UnableToCreateGameMustEnforceFirst( gameName ), creatorName, true );
				}
			}

			/*if( m_AdminGame )
				m_AdminGame->SendAllChat( m_Language->UnableToCreateGameMustEnforceFirst( gameName ) );*/

			return;
		}
	}

	if( m_CurrentGame )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		{
			/*if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameAnotherGameInLobby( gameName, m_CurrentGame->GetDescription( ) ), creatorName, whisper );*/

			if( (*i)->GetServer( ) == creatorServer )
			{
				if( (*i)->GetPasswordHashType( ) == "pvpgn" )
					(*i)->SendChatCommand( m_Language->UnableToCreateGameAnotherGameInLobby( gameName, m_CurrentGame->GetDescription( ) ), creatorName );
				else
					(*i)->QueueChatCommand( m_Language->UnableToCreateGameAnotherGameInLobby( gameName, m_CurrentGame->GetDescription( ) ), creatorName, true );
			}
		}

		/*if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->UnableToCreateGameAnotherGameInLobby( gameName, m_CurrentGame->GetDescription( ) ) );*/

		m_Manager->SendErrorGameInLobby( );

		return;
	}

	if( m_Games.size( ) >= m_MaxGames )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		{
			/*if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameMaxGamesReached( gameName, UTIL_ToString( m_MaxGames ) ), creatorName, whisper );*/

			if( (*i)->GetServer( ) == creatorServer )
			{
				if( (*i)->GetPasswordHashType( ) == "pvpgn" )
					(*i)->SendChatCommand( m_Language->UnableToCreateGameMaxGamesReached( gameName, UTIL_ToString( m_MaxGames ) ), creatorName );
				else
					(*i)->QueueChatCommand( m_Language->UnableToCreateGameMaxGamesReached( gameName, UTIL_ToString( m_MaxGames ) ), creatorName, true );
			}
		}

		/*if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->UnableToCreateGameMaxGamesReached( gameName, UTIL_ToString( m_MaxGames ) ) );*/

		m_Manager->SendErrorMaxGamesReached( );

		return;
	}

	CONSOLE_Print( "[GHOST] creating game [" + gameName + "]" );

	switch( gameType )
	{
	case MASL_PROTOCOL :: DB_CUSTOM_GAME:
		{
			m_CurrentGame = new CCustomGame( this, map, NULL, m_HostPort, gameState, gameName, ownerName, creatorName, creatorServer );
		}
		break;

	case MASL_PROTOCOL :: DB_CUSTOM_DOTA_GAME:
		{
			m_CurrentGame = new CCustomDotAGame( this, map, NULL, m_HostPort, gameState, gameName, ownerName, creatorName, creatorServer );
		}
		break;

	case MASL_PROTOCOL :: DB_DIV1_DOTA_GAME:
		{
			m_CurrentGame = new CDiv1DotAGame( this, map, NULL, m_HostPort, gameState, gameName, ownerName, creatorName, creatorServer );
		}
		break;

	default:
		return;
	}

	m_Manager->SendGameHosted( gameType, GetServerID( creatorServer ), m_CurrentGame->GetGameID( ), gameState, map->GetSlots( ).size( ), ownerName, gameName, map->GetCFGFile( ) );

	// todotodo: check if listening failed and report the error to the user

	if( m_SaveGame )
	{
		m_CurrentGame->SetEnforcePlayers( m_EnforcePlayers );
		m_EnforcePlayers.clear( );
	}

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
	{
		/*if( whisper && (*i)->GetServer( ) == creatorServer )
		{
			// note that we send this whisper only on the creator server

			if( gameState == GAME_PRIVATE )
				(*i)->QueueChatCommand( m_Language->CreatingPrivateGame( gameName, ownerName ), creatorName, whisper );
			else if( gameState == GAME_PUBLIC )
				(*i)->QueueChatCommand( m_Language->CreatingPublicGame( gameName, ownerName ), creatorName, whisper );
		}
		else
		{
			// note that we send this chat message on all other bnet servers

			if( gameState == GAME_PRIVATE )
				(*i)->QueueChatCommand( m_Language->CreatingPrivateGame( gameName, ownerName ) );
			else if( gameState == GAME_PUBLIC )
				(*i)->QueueChatCommand( m_Language->CreatingPublicGame( gameName, ownerName ) );
		}*/

		if( (*i)->GetServer( ) == creatorServer )
		{
			// note that we send this whisper only on the creator server

			if( gameState == GAME_PRIVATE )
			{
				if( (*i)->GetPasswordHashType( ) == "pvpgn" )
					(*i)->SendChatCommand( m_Language->CreatingPrivateGame( gameName, ownerName ), creatorName );
				else
					(*i)->QueueChatCommand( m_Language->CreatingPrivateGame( gameName, ownerName ), creatorName, true );
			}
			else if( gameState == GAME_PUBLIC )
			{
				if( (*i)->GetPasswordHashType( ) == "pvpgn" )
					(*i)->SendChatCommand( m_Language->CreatingPublicGame( gameName, ownerName ), creatorName );
				else
					(*i)->QueueChatCommand( m_Language->CreatingPublicGame( gameName, ownerName ), creatorName, true );
			}
		}

		if( saveGame )
			(*i)->QueueGameCreate( gameState, gameName, string( ), map, m_SaveGame, m_CurrentGame->GetHostCounter( ) );
		else
			(*i)->QueueGameCreate( gameState, gameName, string( ), map, NULL, m_CurrentGame->GetHostCounter( ) );
	}

	/*if( m_AdminGame )
	{
		if( gameState == GAME_PRIVATE )
			m_AdminGame->SendAllChat( m_Language->CreatingPrivateGame( gameName, ownerName ) );
		else if( gameState == GAME_PUBLIC )
			m_AdminGame->SendAllChat( m_Language->CreatingPublicGame( gameName, ownerName ) );
	}*/

	/*if( map->GetMapType( ) == "dota" )
		m_Manager->SendGameHosted( creatorServer, m_CurrentGame->GetGameID( ), gameState, map->GetSlots( ).size( ), ownerName, gameName, string( ) );
	else
		m_Manager->SendGameHosted( creatorServer, m_CurrentGame->GetGameID( ), gameState, map->GetSlots( ).size( ), ownerName, gameName, map->GetCFGFile( ) );*/

	// if we're creating a private game we don't need to send any game refresh messages so we can rejoin the chat immediately
	// unfortunately this doesn't work on PVPGN servers because they consider an enterchat message to be a gameuncreate message when in a game
	// so don't rejoin the chat if we're using PVPGN

	if( gameState == GAME_PRIVATE )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
		{
			if( (*i)->GetPasswordHashType( ) != "pvpgn" )
				(*i)->QueueEnterChat( );
		}
	}

	// hold friends and/or clan members

	/*for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
	{
		if( (*i)->GetHoldFriends( ) )
			(*i)->HoldFriends( m_CurrentGame );

		if( (*i)->GetHoldClan( ) )
			(*i)->HoldClan( m_CurrentGame );
	}*/

	//Add held players to reserved
	for (auto i = heldPlayers.begin(); i != heldPlayers.end(); ++i) {
		m_CurrentGame->AddToReserved(*i);
		CONSOLE_Print("Added " + (*i) + " to reserved.");
	}

	//Autostart when num of held players is present
	if (heldPlayers.size() > 0) {
		m_CurrentGame->SetAutoStartPlayers(heldPlayers.size());
		CONSOLE_Print("Game will autostart when " + UTIL_ToString(heldPlayers.size()) + " players are present.");
	}

	if( m_CustomGameMap && gameType == MASL_PROTOCOL :: DB_CUSTOM_GAME )
	{
		delete m_CustomGameMap;
		m_CustomGameMap = NULL;
	}
}
