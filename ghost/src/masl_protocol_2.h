
#ifndef MASL_PROTOCOL_2_H
#define MASL_PROTOCOL_2_H

#include "util.h"

namespace MASL_PROTOCOL
{
	inline bool IsDotAEmMode( string mode );
	inline void SSReadString( stringstream &SS, string &Str );
	inline void SSWriteString( stringstream &SS, const string &Str );
	inline uint32_t CalcBanDuration( uint32_t duration, uint32_t banpoints );
	inline uint32_t CalcAutoBanDuration( uint32_t banpoints );
	inline string FlagToString( int flag );


	/* FROM SLAVE TO MASTER */

	const int STM_SLAVE_LOGGEDIN							= 1;		// sent to master when slave logges in bnet
	const int STM_SLAVE_DISCONNECTED						= 3;		// sent to master when slave gets disconnected from bnet
	const int STM_SLAVE_CONNECTED							= 5;		// sent to master when slave connects to bnet
	const int STM_SLAVE_AVAILABLE							= 7;		// sent to master every 60 seconds to let master know this slave is available for hosting
	const int STM_SLAVE_ALIVE								= 9;		// sent to master instead SLAVE_AVAILABLE when slave can't host because it's in lobby or in any other state and can't host
	const int STM_SLAVE_MAXGAMES							= 11;		// sent to master when slave logges in to bnet
	const int STM_SLAVE_MAXGAMESREACHED						= 12;		// sent to master when max number of games is reached when last game in lobby starts

	const int STM_HELLO										= 100;

	const int STM_LOBBY_STATUS								= 150;
	const int STM_SLAVE_STATUS								= 160;
	const int STM_GHOST_GROUP								= 170;

	const int STM_GAME_HOSTED								= 200;		// sent to master when game is hosted
	const int STM_GAME_INLOBBY								= 201;		// sent to master if there is a game in lobby (after GET_STATUS recieved)
	const int STM_GAME_UNHOSTED								= 202;		// sent to master when game is unhosted
	const int STM_GAME_STARTED								= 204;		// sent to master when game starts loading
	const int STM_GAME_INPROGRESS							= 205;		// sent to master when game is in progress (after GET_STATUS recieved)
	const int STM_GAME_ISOVER								= 206;		// sent to master when game is over
	const int STM_GAME_NAMECHANGED							= 208;		// sent to master when game name is changed
	const int STM_GAME_OWNERCHANGED							= 210;		// sent to master when game owner is changed
	const int STM_GAME_PLAYERJOINEDLOBBY					= 212;		// sent to master when player joins the lobby
	
	const int STM_GAME_PLAYERLEFTLOBBY						= 214;		// sent to master when player leaves the lobby
	
	const int STM_PLAYERLEAVE_LOBBY_INFORM					= 213;
	//different left reasons.. not really MASL flags but part of the proto
	const int STM_PLAYERLEAVE_LOBBY_NOMAP					= 215;
	const int STM_PLAYERLEAVE_LOBBY_WRONG_GPROXY			= 217;
	const int STM_PLAYERLEAVE_LOBBY_BANNED					= 218;
	const int STM_PLAYERLEAVE_LOBBY_PING					= 219;
	const int STM_PLAYERLEAVE_LOBBY_WRONG_W3VESION_GPROXY	= 220;
	const int STM_PLAYERLEAVE_LOBBY_FROM					= 221;
	const int STM_PLAYERLEAVE_LOBBY_OWNER_KICKED			= 222;
	const int STM_PLAYERLEAVE_LOBBY_NOSC					= 223;
	const int STM_PLAYERLEAVE_LOBBY_MAX_PSR					= 226;
	const int STM_PLAYERLEAVE_LOBBY_MIN_PSR					= 227;
	const int STM_PLAYERLEAVE_LOBBY_MIN_GAMES				= 228;
	const int STM_PLAYERLEAVE_LOBBY_NORMAL					= 224;

	const int STM_GAME_PLAYERLEFTGAME						= 216;		// sent to master when player leaves/dc while game is in progress
	const int STM_GAME_SAVEDATA								= 220;
	const int STM_GAME_GETDBID								= 225;
	
	const int STM_USER_WASBANNED							= 250;		// sent to master when user gets banned in game
	const int STM_USER_GETINFO								= 255;

	const int STM_ERROR_GAMEINLOBBY							= 307;		// sent to master when slave can't create game because another game is in lobby
	const int STM_ERROR_MAXGAMESREACHED						= 308;		// sent to master when slave can't create game because maximum number of hosted games is reached
	const int STM_ERROR_GAMENAMEEXISTS						= 309;		// sent to master when slave can't create game because game name already exists on any realm

	const int STM_THATSALL									= 405;
	const int STM_SERVERINFO								= 420;

	const int STM_DIV1_PLAYERBANNEDBYPLAYER					= 500;		// sent to master when leaver gets banned by another player, ie !banlast


	/* FROM MASTER TO SLAVE(s) */

	const int MTS_HI										= 2400;
	const int MTS_OK										= 2410;

	const int MTS_CONTRIBUTOR_ONLY_MODE						= 2435;

	const int MTS_CREATE_DOTAGAME							= 2500;		// sent to slave to create dota game
	const int MTS_CREATE_RPGGAME							= 2504;		// sent to slave to create custom rpg game
	const int MTS_CREATE_LEAGUEDOTAGAME						= 2506;		// sent to slave to create league dota game

	const int MTS_CREATE_GAME								= 2510;		// sent to slave to create a game

	const int MTS_USER_WASBANNED							= 2600;		// sent to slave when user gets banned on master
	const int MTS_USER_WASUNBANNED							= 2601;		// sent to slave when user gets unbanned on master
	const int MTS_USER_NEWACCESSLEVEL						= 2605;		// sent to slave when user gets new access level

	const int MTS_ADMIN_ADDED								= 2700;		// sent to slave when admin is added on master
	const int MTS_ADMIN_DELETED								= 2701;		// sent to slave when admin is removed on master

	const int MTS_REMOTE_CMD								= 2800;		// sent to all slaves as root admin chat command from realm where !cmd was issued
	const int MTS_ANNOUNCEMENT								= 2803;		// sent to all slaves, announcement from an admin from !announce command

	const int MTS_USER_GETINFO_RESPONSE						= 2900;
	const int MTS_PLAYER_FROMCODES							= 2910;
	const int MTS_GAME_GETDBID_RESPONSE						= 2950;

	const int MTS_CONFIG_RELOAD								= 2960;		


	/* OTHER */

	const int GAME_PUB										= 16;		// public game code
	const int GAME_PRIV										= 17;		// private game code

	const int UPDATE_MYSQL_UNIX_TIMESTAMP_SECONDS			= 600;

	const int WEB_MANAGER_PORT								= 6005;
	const int WEB_UPDATE_BAN_GROUP							= 5;
	const int WEB_UPDATE_PLAYER								= 10;
	const int WEB_UPDATE_DIV1_STATS_DOTA					= 15;


	/* DATABASE */

	// don't change these, checking is done on some places in a way:
	/*	if(game_type < 100)
	 *		// ladder game
	 *	else
	 *		// custom game
	*/

	const int DB_NORMAL_BAN									= 1;		// 
	const int DB_GAME_IN_PROGRESS_BAN						= 3;		// 
	const int DB_GAME_BAN									= 5;		// 

	const int DB_LEAVER_BAN_DURATION						= 300;		// base ban duration for game leavers in seconds
	const int DB_LEAVER_BAN_POINTS							= 2;		// ban points for game leave autoban

	const int DB_DIV1_DOTA_GAME								= 1;
	const int DB_DIV1_STARTING_PSR							= 1500;
	const int NORMAL_MAP_OBSERVER_VALUE						= 1;
	//const int DB_DIV1_DOTA_EM_GAME						= 2;
	const int DB_DIV2_DOTA_GAME								= 11;
	const int DB_DIV3_DOTA_GAME								= 21;

	const int DB_CUSTOM_GAME								= 101;
	const int DB_CUSTOM_DOTA_GAME							= 111;

	const int GHOST_GROUP_ALL								= 299;
	const int GHOST_GROUP_DIV1DOTA							= 301;
	const int GHOST_GROUP_DIV2DOTA							= 305;
	const int GHOST_GROUP_DIV3DOTA							= 310;
	const int GHOST_GROUP_CG								= 401;
	const int GHOST_GROUP_TOUR								= 501;

	/*const int BAN_AUTOBANGAMEINPROGRESS					= 20;		// private game code
	const int BAN_AUTOBAN									= 21;		// private game code
	const int BAN_ADMINBAN									= 22;		// private game code*/
}

inline bool MASL_PROTOCOL :: IsDotAEmMode( string mode )
{
	transform( mode.begin( ), mode.end( ), mode.begin( ), (int(*)(int))tolower );
	uint32_t NumModes = mode.size( ) / 2;

	for( unsigned int i = 0; i < NumModes; ++i )
	{
		string Mode = mode.substr( i * 2, 2 );

		if( Mode == "em" )
			return true;
	}

	return false;
}

inline void MASL_PROTOCOL :: SSReadString( stringstream &SS, string &Str )
{
	SS >> Str;

	if( Str == "/" )
		Str.clear( );
}

inline void MASL_PROTOCOL :: SSWriteString( stringstream &SS, const string &Str )
{
	if( Str.empty( ) )
		SS << " " << "/";
	else
		SS << " " << Str;
}

inline uint32_t MASL_PROTOCOL :: CalcBanDuration( uint32_t duration, uint32_t banpoints )
{
	if( banpoints )
		return duration * banpoints;
	
	return duration;
}

inline uint32_t MASL_PROTOCOL :: CalcAutoBanDuration( uint32_t banpoints )
{
	uint32_t BanNumber = banpoints / MASL_PROTOCOL::DB_LEAVER_BAN_POINTS;

	switch( BanNumber )
	{
	case 0:							// first time
		return 86400 * 2;			// one day ban
	case 1:
		return 86400 * 4;
	case 2:
		return 86400 * 6;
	case 3:
		return 86400 * 6;
	case 4:
		return 86400 * 7;
	case 5:
		return 86400 * 7;
	case 6:
		return 86400 * 7;
	case 7:
		return 86400 * 7;
	case 8:
		return 86400 * 14;
	case 9:
		return 86400 * 14;
	default:						// tenth time and above
		return 86400 * 30;
	}

	/*switch( BanNumber )
	{
	case 0:							// first time
		return 20;			// one day ban
	case 1:
		return 40;
	case 2:
		return 60;
	case 3:
		return 60;
	case 4:
		return 70;
	case 5:
		return 70;
	case 6:
		return 70;
	case 7:
		return 70;
	case 8:
		return 140;
	case 9:
		return 140;
	default:						// tenth time and above
		return 300;
	}*/
}

inline string MASL_PROTOCOL :: FlagToString( int flag )
{
	switch( flag )
	{
	case STM_SLAVE_LOGGEDIN:
		return "STM_SLAVE_LOGGEDIN";
		break;

	case STM_SLAVE_DISCONNECTED:
		return "STM_SLAVE_DISCONNECTED";
		break;

	case STM_SLAVE_CONNECTED:
		return "STM_SLAVE_CONNECTED";
		break;

	case STM_SLAVE_AVAILABLE:
		return "STM_SLAVE_AVAILABLE";
		break;

	case STM_SLAVE_ALIVE:
		return "STM_SLAVE_ALIVE";
		break;

	case STM_SLAVE_MAXGAMES:
		return "STM_SLAVE_MAXGAMES";
		break;

	case STM_SLAVE_MAXGAMESREACHED:
		return "STM_SLAVE_MAXGAMESREACHED";
		break;

	case STM_HELLO:
		return "STM_HELLO";
		break;

	case STM_GAME_HOSTED:
		return "STM_GAME_HOSTED";
		break;

	case STM_GAME_INLOBBY:
		return "STM_GAME_INLOBBY";
		break;

	case STM_GAME_UNHOSTED:
		return "STM_GAME_UNHOSTED";
		break;

	case STM_GAME_STARTED:
		return "STM_GAME_STARTED";
		break;

	case STM_GAME_INPROGRESS:
		return "STM_GAME_INPROGRESS";
		break;

	case STM_GAME_ISOVER:
		return "STM_GAME_ISOVER";
		break;

	case STM_GAME_NAMECHANGED: 
		return "STM_GAME_NAMECHANGED";
		break;

	case STM_GAME_OWNERCHANGED:
		return "STM_GAME_OWNERCHANGED";
		break;

	case STM_GAME_PLAYERJOINEDLOBBY:
		return "STM_GAME_PLAYERJOINEDLOBBY";
		break;

	case STM_GAME_PLAYERLEFTLOBBY:
		return "STM_GAME_PLAYERLEFTLOBBY";
		break;

	case STM_PLAYERLEAVE_LOBBY_INFORM:
		return "STM_PLAYERLEAVE_LOBBY_INFORM";
		break;

	case STM_GAME_PLAYERLEFTGAME:
		return "STM_GAME_PLAYERLEFTGAME";
		break;

	case STM_GAME_SAVEDATA:
		return "STM_GAME_SAVEDATA";
		break;

	case STM_ERROR_GAMEINLOBBY:
		return "STM_ERROR_GAMEINLOBBY";
		break;

	case STM_ERROR_MAXGAMESREACHED:
		return "STM_ERROR_MAXGAMESREACHED";
		break;

	case STM_ERROR_GAMENAMEEXISTS:
		return "STM_ERROR_GAMENAMEEXISTS";
		break;

	case STM_USER_WASBANNED:
		return "STM_USER_WASBANNED";
		break;

	case STM_USER_GETINFO:
		return "STM_USER_GETINFO";
		break;

	case STM_THATSALL:
		return "STM_THATSALL";
		break;

	case STM_SERVERINFO:
		return "STM_SERVERINFO";
		break;

	case MTS_HI:
		return "MTS_HI";
		break;

	case MTS_OK:
		return "MTS_OK";
		break;

	case MTS_CREATE_DOTAGAME:
		return "MTS_CREATE_DOTAGAME";
		break;

	case MTS_CREATE_RPGGAME:
		return "MTS_CREATE_RPGGAME";
		break;

	case MTS_CREATE_LEAGUEDOTAGAME:
		return "MTS_CREATE_LEAGUEDOTAGAME";
		break;

	case MTS_CREATE_GAME:
		return "MTS_CREATE_GAME";
		break;

	case MTS_USER_WASBANNED:
		return "MTS_USER_WASBANNED";
		break;

	case MTS_USER_WASUNBANNED:
		return "MTS_USER_WASUNBANNED";
		break;

	case MTS_USER_NEWACCESSLEVEL:
		return "MTS_USER_NEWACCESSLEVEL";
		break;

	case MTS_ADMIN_ADDED:
		return "MTS_ADMIN_ADDED";
		break;

	case MTS_ADMIN_DELETED:
		return "MTS_ADMIN_DELETED";
		break;

	case MTS_REMOTE_CMD:
		return "MTS_REMOTE_CMD";
		break;

	case MTS_ANNOUNCEMENT:
		return "MTS_ANNOUNCEMENT";
		break;

	case MTS_USER_GETINFO_RESPONSE:
		return "MTS_USER_GETINFO_RESPONSE";
		break;
	case MTS_CONFIG_RELOAD:
		return "MTS_CONFIG_RELOAD";
		break;

	case GAME_PUB:
		return "GAME_PUB";
		break;

	case GAME_PRIV:
		return "GAME_PRIV";
		break;
	
	default:
		return UTIL_ToString( flag );
		break;
	}
}

#endif
