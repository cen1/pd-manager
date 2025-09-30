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
#include "config.h"
#include "language.h"
#include "socket.h"
#include "ghostdb.h"
#include "bnet.h"
#include "bnet_tour.h"
#include "masl_slavebot.h"
#include "masl_manager.h"
#include "masl_protocol_2.h"

#include <signal.h>
#include <stdlib.h>

#ifdef WIN32
 #include <ws2tcpip.h>		// for WSAIoctl
#endif

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

time_t gStartTime;
string gSystemLogFile;
string gMYSQLLogFile;
string gDebugLogFile;
string gBNETLogFile;
CGHost *gGHost = NULL;

#if PD_THREADED_LOGGING == 1
 #include <boost/thread/mutex.hpp>
 boost::mutex gThreadedLoggingMutex;
#endif

uint32_t GetTime( )
{
	return (uint32_t)( ( GetTicks( ) / 1000 ) - gStartTime );
}

uint32_t GetMySQLTime( )
{
	if( gGHost->m_MySQLTime > 0 )
		return gGHost->m_MySQLTime + ( GetTime( ) - gGHost->m_LastMySQLTimeRefreshTime );
	
	return 0;
}

uint32_t GetTicks( )
{
#ifdef WIN32
	return GetTickCount( );
#elif __APPLE__
	uint64_t current = mach_absolute_time( );
	static mach_timebase_info_data_t info = { 0, 0 };
	// get timebase info
	if( info.denom == 0 )
		mach_timebase_info( &info );
	uint64_t elapsednano = current * ( info.numer / info.denom );
	// convert ns to ms
	return elapsednano / 1e6;
#else
	uint32_t ticks;
	struct timespec t;
	clock_gettime( CLOCK_MONOTONIC, &t );
	ticks = t.tv_sec * 1000;
	ticks += t.tv_nsec / 1000000;
	return ticks;
#endif
}

void SignalCatcher2( int s )
{
	CONSOLE_Print( "[!!!] caught signal " + UTIL_ToString( s ) + ", exiting NOW" );

	if( gGHost )
	{
		if( gGHost->m_Exiting )
			exit( 1 );
		else
			gGHost->m_Exiting = true;
	}
	else
		exit( 1 );
}

void SignalCatcher( int s )
{
	// signal( SIGABRT, SignalCatcher2 );
	signal( SIGINT, SignalCatcher2 );

	CONSOLE_Print( "[!!!] caught signal " + UTIL_ToString( s ) + ", exiting nicely" );

	if( gGHost )
		gGHost->m_ExitingNice = true;
	else
		exit( 1 );
}

//
// main
//

int main( int argc, char **argv )
{
	// patch, changed ghost.cfg to master.cfg
	string CFGFile = "pd-manager.cfg";

	if( argc > 1 && argv[1] )
		CFGFile = argv[1];

	// read config file

	CConfig CFG;
	CFG.Read( CFGFile );

	gSystemLogFile = CFG.GetString( "bot_log_system", string( ) );
	gMYSQLLogFile = CFG.GetString( "db_mysql_log_file", string( ) );
	gDebugLogFile = CFG.GetString( "bot_log_debug", string( ) );
	gBNETLogFile = CFG.GetString( "bot_log_bnet", string( ) );

	// print something for logging purposes

	CONSOLE_Print( "[GHOST] starting up" );

	// catch SIGABRT and SIGINT

	// signal( SIGABRT, SignalCatcher );
	signal( SIGINT, SignalCatcher );

#ifndef WIN32
	// disable SIGPIPE since some systems like OS X don't define MSG_NOSIGNAL

	signal( SIGPIPE, SIG_IGN );
#endif

	// initialize the start time

	gStartTime = 0;
	gStartTime = GetTime( );

#ifdef WIN32
	// increase process priority

	CONSOLE_Print( "[GHOST] setting process priority to \"above normal\"" );
	SetPriorityClass( GetCurrentProcess( ), ABOVE_NORMAL_PRIORITY_CLASS );

	// initialize winsock

	CONSOLE_Print( "[GHOST] starting winsock" );
	WSADATA wsadata;

	if( WSAStartup( MAKEWORD( 2, 2 ), &wsadata ) != 0 )
	{
		CONSOLE_Print( "[GHOST] error starting winsock" );
		return 1;
	}
#endif

	boost::asio::io_context IOService;
	boost::asio::io_context::work Work( IOService );

	// initialize ghost

	gGHost = new CGHost( &CFG, IOService, Work );

	/*while( 1 )
	{
		// block for 50ms on all sockets - if you intend to perform any timed actions more frequently you should change this
		// that said it's likely we'll loop more often than this due to there being data waiting on one of the sockets but there aren't any guarantees

		if( gGHost->Update( 50000, io_context ) )
			break;
	}*/

	IOService.run( );

	// shutdown ghost

	CONSOLE_Print( "[GHOST] shutting down" );
	delete gGHost;
	gGHost = NULL;

#ifdef WIN32
	// shutdown winsock

	CONSOLE_Print( "[GHOST] shutting down winsock" );
	WSACleanup( );
#endif

	return 0;
}

void GHOST_Log( string message, bool printToConsole, string logFile )
{
#if PD_THREADED_LOGGING == 1
	// we need to protect this function if it's going to be accessed from multiple threads
	boost::mutex::scoped_lock lock( gThreadedLoggingMutex );
#endif

	if( printToConsole )
		cout << message << endl;

	// logging

	if( !logFile.empty( ) )
	{
		ofstream Log;
		Log.open( logFile.c_str( ), ios :: app );

		if( !Log.fail( ) )
		{
			time_t Now = time( NULL );
			string Time = asctime( localtime( &Now ) );

			// erase the newline
			
			Time.erase( Time.size( ) - 1 );
			Log << "[" << Time << "] " << message << endl;
			Log.close( );
		}
	}
}

void CONSOLE_Print( string message, bool printToConsole )
{
	GHOST_Log( message, printToConsole, gSystemLogFile );
}

void MYSQL_Print( string message, bool printToConsole )
{
	GHOST_Log( message, printToConsole, gMYSQLLogFile );
}

void BNET_Print( string message, bool printToConsole )
{
	GHOST_Log( message, printToConsole, gBNETLogFile );
}

void DEBUG_Print( string message, bool printToConsole )
{
#if PD_DEBUG == 1
	GHOST_Log( message, printToConsole, gDebugLogFile );
#endif
}

void DEBUG_Print( BYTEARRAY b, bool printToConsole )
{
#if PD_DEBUG == 1
	stringstream Message;
	Message	<< "{ ";

	for( unsigned int i = 0; i < b.size( ); ++i )
		Message << hex << (int)b[i] << " ";

	Message << "}" << endl;

	GHOST_Log( Message.str( ), printToConsole, gDebugLogFile );
#endif
}

uint32_t GetServerID( const string &server )
{
	if( server == "server.eurobattle.net" )
		return 1;
	else if( server == "fawkz.com" )
		return 2;
	else if( server == "europe.battle.net" )
		return 3;
	else if( server == "fordota.com" )
		return 4;
	else if( server == "mymgn.com" )
		return 5;
	else
		return 0;
}

string GetServerAddress( uint32_t serverID )
{
	switch( serverID )
	{
	case 1:
		return "server.eurobattle.net";
	case 2:
		return "fawkz.com";
	case 3:
		return "europe.battle.net";
	case 4:
		return "fordota.com";
	case 5:
		return "mymgn.com";
	default:
		return string( );
	}
}

//
// CGHost
//

CGHost :: CGHost( CConfig *CFG, boost::asio::io_context &nIOService, boost::asio::io_context::work &nWork ) : m_IOService( nIOService ), m_Work( nWork ), m_UpdateTimer( nIOService )
{
	CONSOLE_Print( "[GHOST] opening MySQL database" );
	//m_DB = new CGHostDBMySQL( CFG );
	m_DB = new CGHostDB( CFG );

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

			for( int i = 0; i < nNumInterfaces; i++ )
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
			for( int i = 0; HostEnt->h_addr_list[i] != NULL; i++ )
			{
				struct in_addr Address;
				memcpy( &Address, HostEnt->h_addr_list[i], sizeof(struct in_addr) );
				CONSOLE_Print( "[GHOST] local IP address #" + UTIL_ToString( i + 1 ) + " is [" + string( inet_ntoa( Address ) ) + "]" );
				m_LocalAddresses.push_back( UTIL_CreateByteArray( (uint32_t)Address.s_addr, false ) );
			}
		}
	}
#endif

	m_Exiting = false;
	m_ExitingNice = false;

	m_Enabled = CFG->GetInt("bot_enabled", 1) ? true : false;
	m_EnabledLadder = CFG->GetInt("bot_enabledladder", 1) ? true : false;

	CONSOLE_Print("[GHOST] bot_enabled set to " + UTIL_ToString(m_Enabled));
	CONSOLE_Print("[GHOST] bot_enabledladder set to " + UTIL_ToString(m_EnabledLadder));

	m_LanguageFile = CFG->GetString( "bot_language", "language.cfg" );
	m_Language = new CLanguage( m_LanguageFile );
	m_Warcraft3Path = CFG->GetString( "bot_war3path", "C:\\Program Files\\Warcraft III\\" );
	m_BindAddress = CFG->GetString( "bot_bindaddress", string( ) );
	m_IPBlackListFile = CFG->GetString( "bot_ipblacklistfile", "ipblacklist.txt" );
	m_MapPath = UTIL_AddPathSeperator( CFG->GetString( "bot_mappath", string( ) ) );
	// patch
	m_RPGMode = CFG->GetInt( "rpg_mode", 0 ) ? true : false;
	m_DisabledMessage = string( );
	m_MaxGames = 500;
	m_UseNewPSRFormula = CFG->GetInt("dota_usenewpsrformula", 0) == 0 ? false : true;
	double m_DotaAutobanPSRMultiplier;


	// this bot is based on original ghost++ version 14.4
	// m_Version = "14.4";

	m_TempReplayPath = UTIL_AddPathSeperator( CFG->GetString( "bot_tempreplaypath", string( ) ) );
	m_ReplayPath = UTIL_AddPathSeperator( CFG->GetString( "bot_replaypath", string( ) ) );

	m_LobbyLogPath = UTIL_AddPathSeperator( CFG->GetString( "bot_lobbylogpath", "lobbylogs/real/" ) );
	m_LobbyLogPathTmp = UTIL_AddPathSeperator( CFG->GetString( "bot_lobbylogpathtmp", "lobbylogs/tmp/" ) );
	m_GameLogPath = UTIL_AddPathSeperator( CFG->GetString( "bot_gamelogpath", "gamelogs/real/" ) );
	m_GameLogPathTmp = UTIL_AddPathSeperator( CFG->GetString( "bot_gamelogpathtmp", "gamelogs/tmp/" ) );

	//PSR limits
	uint32_t DotaMaxWinChanceDiffGainConstant = CFG->GetInt("dota_max_win_chance_diff_gain_constant", 0);
	if (DotaMaxWinChanceDiffGainConstant > 100 || DotaMaxWinChanceDiffGainConstant < 0) {
		DotaMaxWinChanceDiffGainConstant = 0;
		CONSOLE_Print("[GHOST] warn - dota_max_win_chance_diff_gain_constant incorrect range, disabling");
	}
	m_DotaMaxWinChanceDiffGainConstant = DotaMaxWinChanceDiffGainConstant;
	m_DotaAutobanPSRMultiplier = CFG->GetDouble("dota_autoban_psr_multiplier", 0.0);

	m_Version = "7.0";
	m_ContributorOnlyMode = CFG->GetInt( "bot_contributor_only_mode", 0 );
	m_MySQLTime = 0;
	m_CallableMysqlTime = m_DB->ThreadedUnixTimestampGet( );
	m_LastMySQLTimeRefreshTime = GetTime( );

	// inititalize manager

	uint16_t ManagerPort = CFG->GetInt( "masl_port", 15002 );
	string ManagerBindAddress = CFG->GetString( "masl_bind_address", "127.0.0.1" );
	string ManagerGameListFile = CFG->GetString( "game_list", string( ) );
	uint32_t ManagerGameListRefreshInterval = CFG->GetInt( "game_list_refresh", 10 );

	m_Manager = new CManager( this, ManagerBindAddress, ManagerPort, ManagerGameListFile, ManagerGameListRefreshInterval );

	// initialize web manager

	uint16_t WebManagerPort = CFG->GetInt( "web_manager_port", 6005 );
	string WebManagerBindAddress = CFG->GetString( "web_manager_bind_address", "127.0.0.1" );

	m_WebManager = new CWebManager( this, WebManagerBindAddress, WebManagerPort );

	// load the battle.net connections
	// we're just loading the config data and creating the CBNET classes here, the connections are established later (in the Update function)

	set<uint32_t> Servers;

	for( uint32_t i = 1; i < 10; i++ )
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
		string UserName = CFG->GetString( Prefix + "username", string( ) );
		string UserPassword = CFG->GetString( Prefix + "password", string( ) );
		string FirstChannel = CFG->GetString( Prefix + "firstchannel", "The Void" );
		string RootAdmin = CFG->GetString( Prefix + "rootadmin", string( ) );
		string BNETCommandTrigger = CFG->GetString( Prefix + "commandtrigger", "!" );

		if( BNETCommandTrigger.empty( ) )
			BNETCommandTrigger = "!";

		bool PublicCommands = CFG->GetInt( Prefix + "publiccommands", 1 ) == 0 ? false : true;
		string BNLSServer = CFG->GetString( Prefix + "bnlsserver", string( ) );
		int BNLSPort = CFG->GetInt( Prefix + "bnlsport", 9367 );
		int BNLSWardenCookie = CFG->GetInt( Prefix + "bnlswardencookie", 0 );
		unsigned char War3Version = CFG->GetInt( Prefix + "custom_war3version", 24 );
		BYTEARRAY EXEVersion = UTIL_ExtractNumbers( CFG->GetString( Prefix + "custom_exeversion", string( ) ), 4 );
		BYTEARRAY EXEVersionHash = UTIL_ExtractNumbers( CFG->GetString( Prefix + "custom_exeversionhash", string( ) ), 4 );
		string PasswordHashType = CFG->GetString( Prefix + "custom_passwordhashtype", string( ) );
		uint32_t MaxMessageLength = CFG->GetInt( Prefix + "custom_maxmessagelength", 200 );

		if( Server.empty( ) )
			break;

		uint32_t ServerID = CFG->GetInt( Prefix + "serverid", 0 );

		if( ServerID == 0 )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "serverid, skipping this battle.net connection" );
			continue;
		}

		m_Players.push_back( new CPlayers( this, Server, ServerID ) );
		Servers.insert( ServerID );

		uint32_t Connect = CFG->GetInt( Prefix + "connect", 0 );

		if( Connect == 0 )
		{
			CONSOLE_Print( "[GHOST] " + Prefix + "connect set to false, not connecting to this battle.net connection" );
			continue;
		}

		if( CDKeyROC.empty( ) )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "cdkeyroc, skipping this battle.net connection" );
			continue;
		}

		if( CDKeyTFT.empty( ) )
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
		m_BNETs.push_back( new CBNET( this, Server, ServerAlias, BNLSServer, (uint16_t)BNLSPort, (uint32_t)BNLSWardenCookie, CDKeyROC, CDKeyTFT, CountryAbbrev, Country, UserName, UserPassword, FirstChannel, RootAdmin, BNETCommandTrigger[0], PublicCommands, War3Version, EXEVersion, EXEVersionHash, PasswordHashType, MaxMessageLength, ServerID ) );
	}

	if( m_BNETs.empty( ) )
		CONSOLE_Print( "[GHOST] warning - no dota div1 battle.net connections found in config file" );

	// load the DotA div2 battle.net connections
	// we're just loading the config data and creating the CDotADiv2BNET classes here, the connections are established later (in the Update function)

	/*for( uint32_t i = 1; i < 10; i++ )
	{
		string Prefix;

		if( i == 1 )
			Prefix = "dota_div2_bnet_";
		else
			Prefix = "dota_div2_bnet" + UTIL_ToString( i ) + "_";

		string Server = CFG->GetString( Prefix + "server", string( ) );
		string ServerAlias = CFG->GetString( Prefix + "serveralias", string( ) );
		string CDKeyROC = CFG->GetString( Prefix + "cdkeyroc", string( ) );
		string CDKeyTFT = CFG->GetString( Prefix + "cdkeytft", string( ) );
		string CountryAbbrev = CFG->GetString( Prefix + "countryabbrev", "USA" );
		string Country = CFG->GetString( Prefix + "country", "United States" );
		string UserName = CFG->GetString( Prefix + "username", string( ) );
		string UserPassword = CFG->GetString( Prefix + "password", string( ) );
		string FirstChannel = CFG->GetString( Prefix + "firstchannel", "The Void" );
		string RootAdmin = CFG->GetString( Prefix + "rootadmin", string( ) );
		string BNETCommandTrigger = CFG->GetString( Prefix + "commandtrigger", "!" );

		if( BNETCommandTrigger.empty( ) )
			BNETCommandTrigger = "!";

		bool PublicCommands = CFG->GetInt( Prefix + "publiccommands", 1 ) == 0 ? false : true;
		string BNLSServer = CFG->GetString( Prefix + "bnlsserver", string( ) );
		int BNLSPort = CFG->GetInt( Prefix + "bnlsport", 9367 );
		int BNLSWardenCookie = CFG->GetInt( Prefix + "bnlswardencookie", 0 );
		unsigned char War3Version = CFG->GetInt( Prefix + "custom_war3version", 24 );
		BYTEARRAY EXEVersion = UTIL_ExtractNumbers( CFG->GetString( Prefix + "custom_exeversion", string( ) ), 4 );
		BYTEARRAY EXEVersionHash = UTIL_ExtractNumbers( CFG->GetString( Prefix + "custom_exeversionhash", string( ) ), 4 );
		string PasswordHashType = CFG->GetString( Prefix + "custom_passwordhashtype", string( ) );
		uint32_t MaxMessageLength = CFG->GetInt( Prefix + "custom_maxmessagelength", 200 );

		if( Server.empty( ) )
			break;

		uint32_t ServerID = CFG->GetInt( Prefix + "serverid", 0 );

		if( ServerID == 0 )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "serverid, skipping this battle.net connection" );
			continue;
		}

		m_Players.push_back( new CPlayers( this, Server, ServerID ) );
		Servers.insert( ServerID );

		uint32_t Connect = CFG->GetInt( Prefix + "connect", 0 );

		if( Connect == 0 )
		{
			CONSOLE_Print( "[GHOST] " + Prefix + "connect set to false, not connecting to this battle.net connection" );
			continue;
		}

		if( CDKeyROC.empty( ) )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "cdkeyroc, skipping this battle.net connection" );
			continue;
		}

		if( CDKeyTFT.empty( ) )
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

		CONSOLE_Print( "[GHOST] found DotA div2 battle.net connection #" + UTIL_ToString( i ) + " for server [" + Server + "] with ID " + UTIL_ToString( ServerID ) );
		m_DotADiv2BNETs.push_back( new CDotADiv2BNET( this, Server, ServerAlias, BNLSServer, (uint16_t)BNLSPort, (uint32_t)BNLSWardenCookie, CDKeyROC, CDKeyTFT, CountryAbbrev, Country, UserName, UserPassword, FirstChannel, RootAdmin, BNETCommandTrigger[0], PublicCommands, War3Version, EXEVersion, EXEVersionHash, PasswordHashType, MaxMessageLength, ServerID ) );
	}

	if( m_DotADiv2BNETs.empty( ) )
		CONSOLE_Print( "[GHOST] warning - no DotA div2 battle.net connections found in config file" );*/

	// load the TOUR battle.net connections
	// we're just loading the config data and creating the CTourBNET classes here, the connections are established later (in the Update function)

	for( uint32_t i = 1; i < 10; i++ )
	{
		string Prefix;

		if( i == 1 )
			Prefix = "tour_bnet_";
		else
			Prefix = "tour_bnet" + UTIL_ToString( i ) + "_";

		string Server = CFG->GetString( Prefix + "server", string( ) );
		string ServerAlias = CFG->GetString( Prefix + "serveralias", string( ) );
		string CDKeyROC = CFG->GetString( Prefix + "cdkeyroc", string( ) );
		string CDKeyTFT = CFG->GetString( Prefix + "cdkeytft", string( ) );
		string CountryAbbrev = CFG->GetString( Prefix + "countryabbrev", "USA" );
		string Country = CFG->GetString( Prefix + "country", "United States" );
		string UserName = CFG->GetString( Prefix + "username", string( ) );
		string UserPassword = CFG->GetString( Prefix + "password", string( ) );
		string FirstChannel = CFG->GetString( Prefix + "firstchannel", "The Void" );
		string RootAdmin = CFG->GetString( Prefix + "rootadmin", string( ) );
		string BNETCommandTrigger = CFG->GetString( Prefix + "commandtrigger", "!" );

		if( BNETCommandTrigger.empty( ) )
			BNETCommandTrigger = "!";

		bool PublicCommands = CFG->GetInt( Prefix + "publiccommands", 1 ) == 0 ? false : true;
		string BNLSServer = CFG->GetString( Prefix + "bnlsserver", string( ) );
		int BNLSPort = CFG->GetInt( Prefix + "bnlsport", 9367 );
		int BNLSWardenCookie = CFG->GetInt( Prefix + "bnlswardencookie", 0 );
		unsigned char War3Version = CFG->GetInt( Prefix + "custom_war3version", 24 );
		BYTEARRAY EXEVersion = UTIL_ExtractNumbers( CFG->GetString( Prefix + "custom_exeversion", string( ) ), 4 );
		BYTEARRAY EXEVersionHash = UTIL_ExtractNumbers( CFG->GetString( Prefix + "custom_exeversionhash", string( ) ), 4 );
		string PasswordHashType = CFG->GetString( Prefix + "custom_passwordhashtype", string( ) );
		uint32_t MaxMessageLength = CFG->GetInt( Prefix + "custom_maxmessagelength", 200 );

		if( Server.empty( ) )
			break;

		uint32_t ServerID = CFG->GetInt( Prefix + "serverid", 0 );

		if( ServerID == 0 )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "serverid, skipping this TOUR battle.net connection" );
			continue;
		}

		m_Players.push_back( new CPlayers( this, Server, ServerID ) );
		Servers.insert( ServerID );

		uint32_t Connect = CFG->GetInt( Prefix + "connect", 0 );

		if( Connect == 0 )
		{
			CONSOLE_Print( "[GHOST] " + Prefix + "connect set to false, not connecting to this TOUR battle.net connection" );
			continue;
		}

		if( CDKeyROC.empty( ) )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "cdkeyroc, skipping this TOUR battle.net connection" );
			continue;
		}

		if( CDKeyTFT.empty( ) )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "cdkeytft, skipping this TOUR battle.net connection" );
			continue;
		}

		if( UserName.empty( ) )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "username, skipping this TOUR battle.net connection" );
			continue;
		}

		if( UserPassword.empty( ) )
		{
			CONSOLE_Print( "[GHOST] missing " + Prefix + "password, skipping this TOUR battle.net connection" );
			continue;
		}

		//CONSOLE_Print( "[GHOST] found battle.net connection #" + UTIL_ToString( i ) + " for server [" + Server + "]" );
		CONSOLE_Print( "[GHOST] found TOUR battle.net connection #" + UTIL_ToString( i ) + " for server [" + Server + "] with ID " + UTIL_ToString( ServerID ) );
		m_TourBNETs.push_back( new CTourBNET( this, Server, ServerAlias, BNLSServer, (uint16_t)BNLSPort, (uint32_t)BNLSWardenCookie, CDKeyROC, CDKeyTFT, CountryAbbrev, Country, UserName, UserPassword, FirstChannel, RootAdmin, BNETCommandTrigger[0], PublicCommands, War3Version, EXEVersion, EXEVersionHash, PasswordHashType, MaxMessageLength, ServerID ) );
	}

	if( m_TourBNETs.empty( ) )
		CONSOLE_Print( "[GHOST] warning - no TOUR battle.net connections found in config file" );

	CONSOLE_Print( "[GHOST] pdManager Version " + m_Version );

	m_CallableCacheList = m_DB->ThreadedCacheList( Servers );

	m_UpdateTimer.expires_from_now( boost::posix_time::milliseconds( 50 ) );
	m_UpdateTimer.async_wait( boost::bind( &CGHost::Update, this, boost::asio::placeholders::error ) );
}

CGHost :: ~CGHost( )
{
	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		delete *i;

	delete m_Manager;
	delete m_WebManager;

	delete m_DB;

	// warning: we don't delete any entries of m_Callables here because we can't be guaranteed that the associated threads have terminated
	// this is fine if the program is currently exiting because the OS will clean up after us
	// but if you try to recreate the CGHost object within a single session you will probably leak resources!

	if( !m_Callables.empty( ) )
		CONSOLE_Print( "[GHOST] warning - " + UTIL_ToString( m_Callables.size( ) ) + " orphaned callables were leaked (this is not an error)" );

	delete m_Language;
}

void CGHost :: Update( const boost::system::error_code& error )
{
	if( !error )
	{

	}
	else
	{

	}

	// todotodo: do we really want to shutdown if there's a database error? is there any way to recover from this?

	if( m_DB->HasError( ) )
	{
		//CONSOLE_Print( "[GHOST] database error - " + m_DB->GetError( ) );
		CONSOLE_Print( "[GHOST] database error" );
		//return true;
		m_Exiting = true;
	}

	// refresh the MySQL time every UPDATE_MYSQL_UNIX_TIMESTAMP_SECONDS seconds

	if( !m_CallableMysqlTime && GetTime( ) - m_LastMySQLTimeRefreshTime >= MASL_PROTOCOL :: UPDATE_MYSQL_UNIX_TIMESTAMP_SECONDS )
		m_CallableMysqlTime = m_DB->ThreadedUnixTimestampGet( );

	if( m_CallableMysqlTime && m_CallableMysqlTime->GetReady( ) )
	{
		// if the query failed and result is for some reason zero it's better to keep the current cached time

		if( m_CallableMysqlTime->GetResult( ) > 0 )
			m_MySQLTime = m_CallableMysqlTime->GetResult( );

		delete m_CallableMysqlTime;
		m_CallableMysqlTime = NULL;
		m_LastMySQLTimeRefreshTime = GetTime( );
	}

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
	}

	// update callables

	for( vector<CBaseCallable *> :: iterator i = m_Callables.begin( ); i != m_Callables.end( ); )
	{
		if( (*i)->GetReady( ) )
		{
			//m_DB->RecoverCallable( *i );
			delete *i;
			i = m_Callables.erase( i );
		}
		else
			++i;
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

	// 2. all TOUR battle.net sockets

	for( vector<CTourBNET *> :: iterator i = m_TourBNETs.begin( ); i != m_TourBNETs.end( ); ++i )
		NumFDs += (*i)->SetFD( &fd, &send_fd, &nfds );

	m_WebManager->SetFD( &fd, &send_fd, &nfds );

	// SetFD of the UDPServer does not return the number of sockets belonging to it as it's obviously one

	NumFDs++;

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

#ifdef WIN32
	select( 1, &fd, &send_fd, NULL, &tv );
#else
	select( nfds + 1, &fd, &send_fd, NULL, &tv );
#endif

	/*
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

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
	}*/

	bool AdminExit = false;
	bool BNETExit = false;

	// update battle.net connections

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
	{
		if( (*i)->Update( &fd, &send_fd ) )
			BNETExit = true;
	}

	// update TOUR battle.net connections

	for( vector<CTourBNET *> :: iterator i = m_TourBNETs.begin( ); i != m_TourBNETs.end( ); ++i )
	{
		if( (*i)->Update( &fd, &send_fd ) )
			BNETExit = true;
	}

	// update all slaves

	m_Manager->Update( );

	// update web manager

	m_WebManager->Update( &fd );

	// update database

	m_DB->Update( );

	if( m_CallableCacheList && m_CallableCacheList->GetReady( ) )
	{
		CONSOLE_Print( "[MASL] DPS, ban group and player cache lists updated" );

		m_BanGroup = m_CallableCacheList->GetResult( )->GetBanGroupList( );
		m_DotAPlayerSummary = m_CallableCacheList->GetResult( )->GetDPSList( );

		for( list<CPlayers *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); ++i )
			(*i)->m_Players = m_CallableCacheList->GetResult( )->GetPlayerList( (*i)->GetServerID( ) );

		//m_DB->RecoverCallable( m_CallableCacheList );
		delete m_CallableCacheList;
		m_CallableCacheList = NULL;

		m_Manager->start_accepting( );
	}

	for( list<CCallableGameID *> :: iterator i = m_GameIDs.begin( ); i != m_GameIDs.end( ); )
	{
		if( (*i)->GetReady( ) )
		{
			m_Manager->SendTo( (*i)->GetSlaveID( ), MASL_PROTOCOL::MTS_GAME_GETDBID_RESPONSE, UTIL_ToString( (*i)->GetSlaveGameID( ) ) + " " + UTIL_ToString( (*i)->GetResult( ) ) );

			delete *i;
			i = m_GameIDs.erase( i );
		}
		else
			++i;
	}

	// Iterates over ban queue, updates player cache for each successful DB ban
	for( list<CCallableBanAdd *> :: iterator i = m_BanAdds.begin( ); i != m_BanAdds.end( ); )
	{
		if( (*i)->GetReady( ) )
		{
			uint32_t BanGroupID = (*i)->GetResult( ).first;
			CDBBanGroup *BanGroup = (*i)->GetResult( ).second;

			if( BanGroupID )
			{
				// update bot ban_group cache

				/*CONSOLE_Print( "pa3" );
				CONSOLE_Print( "BanGroup->GetDatetime( ) = " + UTIL_ToString( BanGroup->GetDatetime( ) ) );
				CONSOLE_Print( "BanGroup->GetDuration( ) = " + UTIL_ToString( BanGroup->GetDuration( ) ) );
				CONSOLE_Print( "BanGroup->GetBanPoints( ) = " + UTIL_ToString( BanGroup->GetBanPoints( ) ) );
				CONSOLE_Print( "BanGroup->GetWarnPoints( ) = " + UTIL_ToString( BanGroup->GetWarnPoints( ) ) );*/

				if( m_BanGroup.size( ) > BanGroupID )
				{
					if( m_BanGroup[BanGroupID] != NULL )
						delete m_BanGroup[BanGroupID];
				}
				else
					m_BanGroup.resize( BanGroupID + 1, NULL );

				m_BanGroup[BanGroupID] = BanGroup;

				// update bot players cache

				for( list<CPlayers *> :: iterator j = m_Players.begin( ); j != m_Players.end( ); ++j )
				{
					if( (*i)->GetServerID( ) == (*j)->GetServerID( ) )
					{
						CDBPlayer *Player = (*j)->GetPlayer( (*i)->GetName( ) );

						if( Player )
							Player->SetBanGroupID( BanGroupID );
						else
							(*j)->AddPlayer( (*i)->GetName( ), new CDBPlayer( BanGroupID, 0, 1 ) );

						break;
					}
				}
			}

			delete *i;
			i = m_BanAdds.erase( i );
		}
		else
			++i;
	}

	for( list<CCallableFromCheck *> :: iterator i = m_FromChecks.begin( ); i != m_FromChecks.end( ); )
	{
		if( (*i)->GetReady( ) )
		{
			string From = (*i)->GetResult( ).first;
			string LongFrom = (*i)->GetResult( ).second;

			// update bot players cache

			for( list<CPlayers *> :: iterator j = m_Players.begin( ); j != m_Players.end( ); ++j )
			{
				if( (*i)->GetServerID( ) == (*j)->GetServerID( ) )
				{
					CDBPlayer *Player = (*j)->GetPlayer( (*i)->GetName( ) );

					if( Player )
					{
						Player->SetFrom( From );
						Player->SetLongFrom( LongFrom );
					}
					else
						(*j)->AddPlayer( (*i)->GetName( ), new CDBPlayer( 0, 0, 1, From, LongFrom ) );

					break;
				}
			}

			m_Manager->SendTo( (*i)->GetSlaveBotID( ), MASL_PROTOCOL::MTS_PLAYER_FROMCODES, (*i)->GetName( ) + " " + UTIL_ToString( (*i)->GetServerID( ) ) + " " + From + " " + UTIL_ToString( LongFrom.size( ) ) + " " + LongFrom );

			delete *i;
			i = m_FromChecks.erase( i );
		}
		else
			++i;
	}

	for( list<CCallableGameCustomAdd *> :: iterator i = m_GameCustomAdds.begin( ); i != m_GameCustomAdds.end( ); )
	{
		if( (*i)->GetReady( ) )
		{
			if( (*i)->m_DBGameID )
			{
				// slave bot saved the replay in temp folder, move the replay and rename it to "DBGameID"

				if( !(*i)->m_ReplayName.empty( ) )
				{
					string TempName = m_TempReplayPath + (*i)->m_ReplayName;
					string NewName = m_ReplayPath + UTIL_ToString( (*i)->m_DBGameID );

					if( rename( TempName.c_str( ), NewName.c_str( ) ) == 0 )
						CONSOLE_Print("[MASL] succesfully moved replay from [" + TempName + "] to [" + NewName + "]");
					else
						CONSOLE_Print("[MASL] error code " + UTIL_ToString( errno ) + " - failed to move replay from [" + TempName + "] to [" + NewName + "]");
				}
				else
					CONSOLE_Print("[MASL] error - failed to move replay, replay name is empty");

				// save game logs

				SaveLog( m_LobbyLogPath + UTIL_ToString( (*i)->m_DBGameID ), (*i)->m_LobbyLog );
				SaveLog( m_GameLogPath + UTIL_ToString( (*i)->m_DBGameID ), (*i)->m_GameLog );

				// TODO: update player's global stats
			}
			else
				CONSOLE_Print("[MASL] error - failed to update cache, game ID is zero (custom game)");

			delete *i;
			i = m_GameCustomAdds.erase( i );
		}
		else
			++i;
	}

	for( list<CCallableGameCustomDotAAdd *> :: iterator i = m_GameCustomDotAAdds.begin( ); i != m_GameCustomDotAAdds.end( ); )
	{
		if( (*i)->GetReady( ) )
		{
			if( (*i)->m_DBGameID )
			{
				// slave bot saved the replay in temp folder, move the replay and rename it to "DBGameID"

				if( !(*i)->m_ReplayName.empty( ) )
				{
					string TempName = m_TempReplayPath + (*i)->m_ReplayName;
					string NewName = m_ReplayPath + UTIL_ToString( (*i)->m_DBGameID );

					if( rename( TempName.c_str( ), NewName.c_str( ) ) == 0 )
						CONSOLE_Print("[MASL] succesfully moved replay from [" + TempName + "] to [" + NewName + "]");
					else
						CONSOLE_Print("[MASL] error code " + UTIL_ToString( errno ) + " - failed to move replay from [" + TempName + "] to [" + NewName + "]");
				}
				else
					CONSOLE_Print("[MASL] error - failed to move replay, replay name is empty");

				// save game logs

				SaveLog( m_LobbyLogPath + UTIL_ToString( (*i)->m_DBGameID ), (*i)->m_LobbyLog );
				SaveLog( m_GameLogPath + UTIL_ToString( (*i)->m_DBGameID ), (*i)->m_GameLog );

				// TODO: update player's global stats
			}
			else
				CONSOLE_Print("[MASL] error - failed to update cache, game ID is zero (custom DotA game)");

			delete *i;
			i = m_GameCustomDotAAdds.erase( i );
		}
		else
			++i;
	}

	for( list<CCallableGameDiv1DotAAdd *> :: iterator i = m_GameDiv1DotAAdds.begin( ); i != m_GameDiv1DotAAdds.end( ); )
	{
		if( (*i)->GetReady( ) )
		{
			if( (*i)->m_DBGameID && !(*i)->m_Error )
			{
				// slave bot saved the replay in temp folder, move the replay and rename it to "DBGameID"

				if( !(*i)->m_ReplayName.empty( ) )
				{
					string TempName = m_TempReplayPath + (*i)->m_ReplayName;
					string NewName = m_ReplayPath + UTIL_ToString( (*i)->m_DBGameID );

					if( rename( TempName.c_str( ), NewName.c_str( ) ) == 0 )
						CONSOLE_Print("[MASL] succesfully moved replay from [" + TempName + "] to [" + NewName + "]");
					else
						CONSOLE_Print("[MASL] error code " + UTIL_ToString( errno ) + " - failed to move replay from [" + TempName + "] to [" + NewName + "]");
				}
				else
					CONSOLE_Print("[MASL] error - failed to move replay, replay name is empty");

				// save game logs

				SaveLog( m_LobbyLogPath + UTIL_ToString( (*i)->m_DBGameID ), (*i)->m_LobbyLog );
				SaveLog( m_GameLogPath + UTIL_ToString( (*i)->m_DBGameID ), (*i)->m_GameLog );

				// update game players cache

				for( list<CDBPlayer2 *> :: iterator j = (*i)->m_DBPlayers2.begin( ); j != (*i)->m_DBPlayers2.end( ); ++j )
				{
					if( (*j)->GetStatsDotAID( ) == 0 )
					{
						// this should never happend, player's StatsDotAID should never be zero after playing a DIV DotA game

						continue;
					}

					for( list<CPlayers *> :: iterator z = m_Players.begin( ); z != m_Players.end( ); ++z )
					{
						if( (*j)->GetServer( ) == (*z)->GetServer( ) )
						{
							CDBPlayer *Player = (*z)->GetPlayer( (*j)->GetName( ) );

							// update/create game player in the cache

							if( Player )
							{
								CONSOLE_Print( "[MASL] player [" + (*j)->GetName( ) + " : " + (*j)->GetServer( ) + "] exists in players cache, updating" );
								Player->SetStatsDotAID( (*j)->GetStatsDotAID( ) );
							}
							else
							{
								CONSOLE_Print( "[MASL] player [" + (*j)->GetName( ) + " : " + (*j)->GetServer( ) + "] doesn't exist in players cache, adding player to cache" );
								(*z)->AddPlayer( (*j)->GetName( ), new CDBPlayer( 0, (*j)->GetStatsDotAID( ), 1 ) );
							}

							break;
						}
					}

					// update DIV1 DotA stats

					if( m_DotAPlayerSummary.size( ) > (*j)->GetStatsDotAID( ) )
					{
						if( m_DotAPlayerSummary[(*j)->GetStatsDotAID( )] != NULL )
							delete m_DotAPlayerSummary[(*j)->GetStatsDotAID( )];
					}
					else
						m_DotAPlayerSummary.resize( (*j)->GetStatsDotAID( ) + 1, NULL );

					m_DotAPlayerSummary[(*j)->GetStatsDotAID( )] = (*j)->GetDotAPlayerSummary( );

					// TODO: update global stats
				}
			}
			else
				CONSOLE_Print("[MASL] error - failed to update cache, game ID is zero (DIV1 DotA game)");

			delete *i;
			i = m_GameDiv1DotAAdds.erase( i );
		}
		else
			++i;
	}

	//io_context.poll( );

	m_UpdateTimer.expires_from_now( boost::posix_time::milliseconds( 50 ) );
	m_UpdateTimer.async_wait( boost::bind( &CGHost::Update, this, boost::asio::placeholders::error ) );

	//return m_Exiting || AdminExit || BNETExit;

	if( m_Exiting || AdminExit || BNETExit )
		m_IOService.stop( );
}

void CGHost :: EventBNETConnecting( CBNET *bnet )
{

}

void CGHost :: EventBNETConnected( CBNET *bnet )
{

}

void CGHost :: EventBNETDisconnected( CBNET *bnet )
{

}

void CGHost :: EventBNETLoggedIn( CBNET *bnet )
{

}

void CGHost :: EventBNETGameRefreshed( CBNET *bnet )
{

}

void CGHost :: EventBNETGameRefreshFailed( CBNET *bnet )
{

}

void CGHost :: EventBNETConnectTimedOut( CBNET *bnet )
{

}

void CGHost :: EventBNETWhisper( CBNET *bnet, string user, string message )
{

}

void CGHost :: EventBNETChat( CBNET *bnet, string user, string message )
{

}

void CGHost :: EventBNETEmote( CBNET *bnet, string user, string message )
{

}

void CGHost :: SaveLog( string fileName, const string &log )
{
	CONSOLE_Print( "[GHOST] saving log file to [" + fileName + "]" );

	if( !fileName.empty( ) )
	{
		ofstream Log;
		Log.open( fileName.c_str( ), ios :: app );

		if( !Log.fail( ) )
		{
			Log << log;
			Log.close( );
		}
	}
}

CBNET * CGHost :: GetBnetByName(string name)
{
	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); ++i )
	{
		if( (*i)->GetServer( ) == name )
			return *i;
	}
	return NULL;
}
/*uint32_t CGHost :: GetUnixTime( )
{
	return m_MysqlTime;
}*/

/*uint32_t CGHost :: GetMysqlCurrentTime( )
{
	if( m_MysqlTime )
		return m_MysqlTime + GetTime( );
	else
		return 0;
}*/

/*BYTEARRAY StringToByteArray( string s )
{
	BYTEARRAY b;
	char *c = new char[s.length()+2];
	strncpy(c,s.c_str(), s.length());
	c[s.length()]=0;
	b=UTIL_CreateByteArray(c,s.length());
	return b;
}*/
