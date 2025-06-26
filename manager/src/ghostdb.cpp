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
#include "ghostdb.h"
#include "masl_protocol_2.h"

#include <signal.h>

#ifdef WIN32
 #include <winsock.h>
#endif

#if __has_include(<mysql/mysql.h>)
#  include <mysql/mysql.h>
#elif __has_include(<mysql.h>)
#  include <mysql.h>
#else
#  error "MySQL headers not found"
#endif
#include <time.h>
#include <queue>

string DBLogFile;

void DB_Log( string message )
{
	//cout << message << endl;

	// logging

	if( !DBLogFile.empty( ) )
	{
		ofstream Log;
		Log.open( DBLogFile.c_str( ), ios :: app );

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

void LOG_Failed_Query( string query, string error )
{
	// logging

	DB_Log( query );
	DB_Log( error );
}

string MySQLEscapeString( void *conn, const string str )
{
	char *to = new char[str.size( ) * 2 + 1];
	unsigned long size = mysql_real_escape_string( (MYSQL *)conn, to, str.c_str( ), str.size( ) );
	string result( to, size );
	delete [] to;
	return result;
}

vector<string> MySQLFetchRow( MYSQL_RES *res )
{
	vector<string> Result;

	MYSQL_ROW Row = mysql_fetch_row( res );

	if( Row )
	{
		unsigned long *Lengths;
		Lengths = mysql_fetch_lengths( res );

        for( unsigned int i = 0; i < mysql_num_fields( res ); ++i )
		{
			if( Row[i] )
				Result.push_back( string( Row[i], Lengths[i] ) );
			else
				Result.push_back( string( ) );
		}
	}

	return Result;
}

//
// CGHostDB
//

CGHostDB :: CGHostDB( CConfig *CFG )
{
	m_HasError = false;

	DBLogFile = CFG->GetString( "db_mysql_log_file", string( ) );

	m_Server = CFG->GetString( "db_mysql_server", string( ) );
	m_Database = CFG->GetString( "db_mysql_database", string( ) );
	m_User = CFG->GetString( "db_mysql_user", string( ) );
	m_Password = CFG->GetString( "db_mysql_password", string( ) );
	m_Port = CFG->GetInt( "db_mysql_port", 0 );

	try
	{
		boost :: thread Thread( &CGHostDB::WorkerThread, this );
	}
	catch( boost :: thread_resource_error tre )
	{
		DB_Log( "[MYSQL] error spawning worker thread on attempt #1 [" + string( tre.what( ) ) + "], pausing execution and trying again in 50ms" );
		MILLISLEEP( 50 );

		try
		{
			boost :: thread Thread( &CGHostDB::WorkerThread, this );
		}
		catch( boost :: thread_resource_error tre2 )
		{
			DB_Log( "[MYSQL] error spawning worker thread on attempt #2 [" + string( tre2.what( ) ) + "], giving up" );
			m_HasError = true;
		}
	}
}

CGHostDB :: ~CGHostDB( )
{

}

void CGHostDB :: Update( )
{
	if( !m_TempJobQueue.empty( ) )
	{
		if( m_JobQueueMutex.try_lock( ) )
		{
			while( !m_TempJobQueue.empty( ) )
			{
				m_JobQueue.push( m_TempJobQueue.front( ) );
				m_TempJobQueue.pop( );
			}

			m_JobQueueMutex.unlock( );
		}
	}
}

void CGHostDB :: WorkerThread( )
{
	mysql_library_init( 0, NULL, NULL );

	// create the first connection

	DB_Log( "[MYSQL] connecting to database server" );
	MYSQL *Connection = NULL;

	if( !( Connection = mysql_init( NULL ) ) )
	{
		DB_Log( string( "[MYSQL] " ) + mysql_error( Connection ) );
		m_HasError = true;
		//m_Error = "error initializing MySQL connection";
		DB_Log( "[MYSQL] error initializing MySQL connection" );
		return;
	}

	bool Reconnect = true;
	mysql_options( Connection, MYSQL_OPT_RECONNECT, &Reconnect );

	if( !( mysql_real_connect( Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
	{
		DB_Log( string( "[MYSQL] " ) + mysql_error( Connection ) );
		m_HasError = true;
		//m_Error = "error connecting to MySQL server";
		DB_Log( "[MYSQL] error connecting to MySQL server" );
		return;
	}

	DB_Log( "[MYSQL] connected to database server" );

	// Connection should be ready now
	// start doing jobs from the queue

	CBaseCallable *Callable;

	while( 1 )
	{
		Callable = NULL;

		//boost::mutex::lock lock( m_JobQueueMutex );
		m_JobQueueMutex.lock( );

		if( !m_JobQueue.empty( ) )
		{
			Callable = m_JobQueue.front( );
			m_JobQueue.pop( );
		}

		//boost::mutex::unlock lock( m_JobQueueMutex );
		m_JobQueueMutex.unlock( );

		if( Callable )
			Callable->ThreadedJob( Connection );
		else
			MILLISLEEP( 20 );
	}
}

void CGHostDB :: QueueCallable( CBaseCallable *callable )
{
	// try to get access to the queue,
	// we don't want to wait for mutex here so if we don't get access just put
	// the job in a temporary queue and put it in the real queue later when we get access to real queue

	//if( boost::mutex::try_lock lock( m_JobQueueMutex ) )
	if( m_JobQueueMutex.try_lock( ) )
	{
		while( !m_TempJobQueue.empty( ) )
		{
			m_JobQueue.push( m_TempJobQueue.front( ) );
			m_TempJobQueue.pop( );
		}

		m_JobQueue.push( callable );
		//boost::mutex::unlock lock( m_JobQueueMutex );
		m_JobQueueMutex.unlock( );
	}
	else
		m_TempJobQueue.push( callable );
}

CCallableFromCheck *CGHostDB :: ThreadedFromCheck( uint32_t slavebotID, string name, uint32_t serverID, uint32_t ip )
{
	CCallableFromCheck *Callable = new CCallableFromCheck( slavebotID, name, serverID, ip );
	QueueCallable( Callable );
	return Callable;
}

CCallableGameCustomAdd *CGHostDB :: ThreadedGameCustomAdd( uint32_t mysqlgameid, uint32_t lobbyduration, string lobbylog, string gamelog, string replayname, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, uint32_t creatorserverid, vector<CDBGamePlayer *> gameplayers, bool rmk, uint32_t gametype, std::string region )
{
	CCallableGameCustomAdd *Callable = new CCallableGameCustomAdd( mysqlgameid, lobbyduration, lobbylog, gamelog, replayname, map, gamename, ownername, duration, gamestate, creatorname, creatorserverid, gameplayers, rmk, gametype, region );
	QueueCallable( Callable );
	return Callable;
}

CCallableGameCustomDotAAdd *CGHostDB :: ThreadedGameCustomDotAAdd( uint32_t mysqlgameid, uint32_t lobbyduration, string lobbylog, string gamelog, string replayname, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, uint32_t creatorserverid, vector<CDBGamePlayer *> gameplayers, bool rmk, uint32_t gametype, uint32_t winner, uint32_t creepsspawnedtime, uint32_t collectdotastatsovertime, uint32_t min, uint32_t sec, vector<CDBDotAPlayer *> dbdotaplayers, string mode1, string mode2, std::string region )
{
	CCallableGameCustomDotAAdd *Callable = new CCallableGameCustomDotAAdd( mysqlgameid, lobbyduration, lobbylog, gamelog, replayname, map, gamename, ownername, duration, gamestate, creatorname, creatorserverid, gameplayers, rmk, gametype, winner, creepsspawnedtime, collectdotastatsovertime, min, sec, dbdotaplayers, mode1, mode2, region );
	QueueCallable( Callable );
	return Callable;
}

CCallableGameDiv1DotAAdd *CGHostDB :: ThreadedGameDiv1DotAAdd( uint32_t mysqlgameid, uint32_t lobbyduration, string lobbylog, string gamelog, string replayname, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, uint32_t creatorserverid, vector<CDBGamePlayer *> gameplayers, bool rmk, uint32_t gametype, uint32_t winner, uint32_t creepsspawnedtime, uint32_t collectdotastatsovertime, uint32_t min, uint32_t sec, vector<CDBDotAPlayer *> dbdotaplayers, string mode1, string mode2, bool ff, bool scored, vector<CDBDiv1DotAPlayer *> dbdiv1dotaplayers, std::string region )
{
	CCallableGameDiv1DotAAdd *Callable = new CCallableGameDiv1DotAAdd( mysqlgameid, lobbyduration, lobbylog, gamelog, replayname, map, gamename, ownername, duration, gamestate, creatorname, creatorserverid, gameplayers, rmk, gametype, winner, creepsspawnedtime, collectdotastatsovertime, min, sec, dbdotaplayers, mode1, mode2, ff, scored, dbdiv1dotaplayers, region );
	QueueCallable( Callable );
	return Callable;
}

CCallableBanAdd *CGHostDB :: ThreadedBanAdd( string player, uint32_t serverID, uint32_t gameID )
{
	CCallableBanAdd *Callable = new CCallableBanAdd( player, serverID, gameID );
	QueueCallable( Callable );
	return Callable;
}

CCallableDIV1BanAdd *CGHostDB :: ThreadedDIV1BanAdd( string adminPlayerName, uint32_t adminServerID, string victimPlayerName, uint32_t victimServerID, uint32_t gameID, string reason )
{
	CCallableDIV1BanAdd *Callable = new CCallableDIV1BanAdd( adminPlayerName, adminServerID, victimPlayerName, victimServerID, gameID, reason );
	QueueCallable( Callable );
	return Callable;
}

CCallableGameID *CGHostDB :: ThreadedGameID( uint32_t slaveID, uint32_t slaveGameID, std::string region )
{
	CCallableGameID *Callable = new CCallableGameID( slaveID, slaveGameID, region );
	QueueCallable( Callable );
	return Callable;
}

CCallableCacheList *CGHostDB :: ThreadedCacheList( set<uint32_t> server )
{
	CCallableCacheList *Callable = new CCallableCacheList( server );
	QueueCallable( Callable );
	return Callable;
}

CCallableContributorList *CGHostDB :: ThreadedContributorList( uint32_t server )
{
	CCallableContributorList *Callable = new CCallableContributorList( server );
	QueueCallable( Callable );
	return Callable;
}

CCallableUnixTimestampGet *CGHostDB :: ThreadedUnixTimestampGet( )
{
	CCallableUnixTimestampGet *Callable = new CCallableUnixTimestampGet( );
	QueueCallable( Callable );
	return Callable;
}

//
// Callables
//

void CCallableFromCheck :: ThreadedJob( void *conn )
{
	m_StartTicks = GetTicks( );

	m_Result = FromCheck( conn, m_IP );

	m_EndTicks = GetTicks( );
	m_Ready = true;
}

void CCallableGameCustomAdd :: ThreadedJob( void *conn )
{
	m_StartTicks = GetTicks( );

	ThreadedJob2( conn );

	m_EndTicks = GetTicks( );
	m_Ready = true;
}

void CCallableGameCustomAdd :: ThreadedJob2( void *conn )
{
	for( vector<CDBGamePlayer *> :: iterator i = m_DBGamePlayers.begin( ); i != m_DBGamePlayers.end( ); ++i )
	{
		string EscName = MySQLEscapeString( conn, (*i)->GetName( ) );
		transform( EscName.begin( ), EscName.end( ), EscName.begin( ), (int(*)(int))tolower );

		CDBPlayer2 *DBPlayer2 = NULL;

		uint32_t PlayerID = 0;
		uint32_t PlayerBanGroupID = 0;
		uint32_t PlayerStatsDotAID = 0;

		string PlayerSelectQuery = "SELECT new_player.id, new_player.ban_group_id, new_player.div1_stats_dota_id FROM new_player WHERE new_player.name = '" + EscName + "' AND new_player.server_id = " + UTIL_ToString( GetServerID( (*i)->GetSpoofedRealm( ) ) ) + " LIMIT 1";

		if( mysql_real_query( (MYSQL *)conn, PlayerSelectQuery.c_str( ), PlayerSelectQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( PlayerSelectQuery, mysql_error( (MYSQL *)conn ) );
			m_Error = true;
			return;
		}
		else
		{
			MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

			if( Result )
			{
				vector<string> Row = MySQLFetchRow( Result );

				if( Row.size( ) == 3 )
				{
					// this player has new_player record in the DB

					PlayerID = UTIL_ToUInt32( Row[0] );
					PlayerBanGroupID = UTIL_ToUInt32( Row[1] );
					PlayerStatsDotAID = UTIL_ToUInt32( Row[2] );

					m_DBPlayers2.push_back( new CDBPlayer2( (*i)->GetBanned( ), (*i)->GetName( ), (*i)->GetSpoofedRealm( ), (*i)->GetColour( ), PlayerID, PlayerBanGroupID, PlayerStatsDotAID ) );
				}
				else
				{
					// this player doesn't have new_player record

					string InsertPlayerQuery = "INSERT INTO new_player ( server_id, name ) VALUES ( " + UTIL_ToString( GetServerID( (*i)->GetSpoofedRealm( ) ) ) + ", '" + EscName + "' )";

					if( mysql_real_query( (MYSQL *)conn, InsertPlayerQuery.c_str( ), InsertPlayerQuery.size( ) ) != 0 )
					{
						LOG_Failed_Query( InsertPlayerQuery, mysql_error( (MYSQL *)conn ) );
						m_Error = true;
						return;
					}
					else
					{
						PlayerID = mysql_insert_id( (MYSQL *)conn );

						if( PlayerID )
							m_DBPlayers2.push_back( new CDBPlayer2( (*i)->GetBanned( ), (*i)->GetName( ), (*i)->GetSpoofedRealm( ), (*i)->GetColour( ), PlayerID, PlayerBanGroupID, PlayerStatsDotAID ) );
						else
						{
							DB_Log( "mysql_insert_id is zero, " + InsertPlayerQuery );
							m_Error = true;
							return;
						}
					}
				}

				mysql_free_result( Result );
			}
			else
			{
				LOG_Failed_Query( PlayerSelectQuery, mysql_error( (MYSQL *)conn ) );
				m_Error = true;
				return;
			}
		}

		if( (*i)->GetName( ) == m_CreatorName )
			m_DBCreatorPlayerID = PlayerID;
	}

	// save game

	//string EscLobbyLog = MySQLEscapeString( conn, m_LobbyLog );
	string EscGameName = MySQLEscapeString( conn, m_GameName );
	//string EscGameLog = MySQLEscapeString( conn, m_GameLog );
	string EscMap = MySQLEscapeString( conn, m_Map );

	if( m_DBGameID )
	{
		// normal situation, this game already has a record in the DB inserted at the begining of the game

		string UpdateGameQuery = "UPDATE new_game SET lobbyduration = " + UTIL_ToString( m_LobbyDuration ) + ", map = '" + EscMap + "', datetime = UNIX_TIMESTAMP(), gamename = '" + EscGameName + "', duration = " + UTIL_ToString( m_Duration ) + ", gamestate = " + UTIL_ToString( m_GameState ) + ", creator_player_id = " + UTIL_ToString( m_DBCreatorPlayerID ) + ", creator_server_id = " + UTIL_ToString( m_CreatorServerID ) + ", rmk = " + ( m_RMK ? "1" : "0" ) + ", game_type = " + UTIL_ToString( m_GameType ) + ", game_in_progress = 0 WHERE new_game.id = " + UTIL_ToString( m_DBGameID ) + " LIMIT 1";

		if( mysql_real_query( (MYSQL *)conn, UpdateGameQuery.c_str( ), UpdateGameQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( UpdateGameQuery, mysql_error( (MYSQL *)conn ) );
			m_Error = true;
			return;
		}
		else
		{
			uint32_t UpdateGameQueryAffectedRows = mysql_affected_rows( (MYSQL *)conn );

			if( !UpdateGameQueryAffectedRows )
			{
				DB_Log( "mysql_affected_rows is zero, " + UpdateGameQuery );
				m_Error = true;
				return;
			}
		}
	}
	else
	{
		// this game DB ID is zero
		// something went wrong when we tried to insert game into the DB when game started

		string InsertGameQuery = "INSERT INTO new_game ( lobbyduration, map, datetime, gamename, duration, gamestate, creator_player_id, creator_server_id, rmk, game_type, bot_region ) VALUES ( " + UTIL_ToString( m_LobbyDuration ) + ", '" + EscMap + "', UNIX_TIMESTAMP( ), '" + EscGameName + "', " + UTIL_ToString( m_Duration ) + ", " + UTIL_ToString( m_GameState ) + ", " + UTIL_ToString( m_DBCreatorPlayerID ) + ", " + UTIL_ToString( m_CreatorServerID ) + ", " + ( m_RMK ? "1" : "0" ) + ", " + UTIL_ToString( m_GameType ) + ", '" + m_region + "' )";

		if( mysql_real_query( (MYSQL *)conn, InsertGameQuery.c_str( ), InsertGameQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( InsertGameQuery, mysql_error( (MYSQL *)conn ) );
			m_Error = true;
			return;
		}
		else
		{
			m_DBGameID = mysql_insert_id( (MYSQL *)conn );

			if( !m_DBGameID )
			{
				DB_Log( "mysql_insert_id is zero, " + InsertGameQuery );
				m_Error = true;
				return;
			}
		}
	}

	// save game players

	string InsertGamePlayersQueryValues;

	for( vector<CDBGamePlayer *> :: iterator i = m_DBGamePlayers.begin( ); i != m_DBGamePlayers.end( ); ++i )
	{
		for( list<CDBPlayer2 *> :: iterator j = m_DBPlayers2.begin( ); j != m_DBPlayers2.end( ); ++j )
		{
			if( (*i)->GetColour( ) == (*j)->GetColor( ) )
			{
				if( InsertGamePlayersQueryValues.empty( ) )
					InsertGamePlayersQueryValues = "( " + UTIL_ToString( m_DBGameID ) + ", " + UTIL_ToString( (*j)->GetPlayerID( ) ) + ", '" + (*i)->GetIP( ) + "', " + UTIL_ToString( (*i)->GetReserved( ) ) + ", " + UTIL_ToString( (*i)->GetLoadingTime( ) ) + ", " + UTIL_ToString( (*i)->GetLeft( ) ) + ", '" + (*i)->GetLeftReason( ) + "', " + UTIL_ToString( (*i)->GetTeam( ) ) + ", " + UTIL_ToString( (*i)->GetColour( ) ) + ", " + ( (*i)->GetGProxy( ) ? "1" : "0" ) + " )";
				else
					InsertGamePlayersQueryValues += ", ( " + UTIL_ToString( m_DBGameID ) + ", " + UTIL_ToString( (*j)->GetPlayerID( ) ) + ", '" + (*i)->GetIP( ) + "', " + UTIL_ToString( (*i)->GetReserved( ) ) + ", " + UTIL_ToString( (*i)->GetLoadingTime( ) ) + ", " + UTIL_ToString( (*i)->GetLeft( ) ) + ", '" + (*i)->GetLeftReason( ) + "', " + UTIL_ToString( (*i)->GetTeam( ) ) + ", " + UTIL_ToString( (*i)->GetColour( ) ) + ", " + ( (*i)->GetGProxy( ) ? "1" : "0" ) + " )";

				break;
			}
		}
	}

	string InsertGamePlayersQuery = "INSERT INTO new_gameplayer ( game_id, player_id, ip, reserved, loadingtime, `left`, leftreason, team, color, gproxy ) VALUES " + InsertGamePlayersQueryValues;

	if( mysql_real_query( (MYSQL *)conn, InsertGamePlayersQuery.c_str( ), InsertGamePlayersQuery.size( ) ) != 0 )
	{
		LOG_Failed_Query( InsertGamePlayersQuery, mysql_error( (MYSQL *)conn ) );
		m_Error = true;
		return;
	}

	// update players

	for( list<CDBPlayer2 *> :: iterator i = m_DBPlayers2.begin( ); i != m_DBPlayers2.end( ); ++i )
	{
		string UpdatePlayerQuery = "UPDATE new_player SET custom_games_count = custom_games_count + 1 WHERE id = " + UTIL_ToString( (*i)->GetPlayerID( ) ) + " LIMIT 1";

		if( mysql_real_query( (MYSQL *)conn, UpdatePlayerQuery.c_str( ), UpdatePlayerQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( UpdatePlayerQuery, mysql_error( (MYSQL *)conn ) );
			m_Error = true;
			return;
		}
		else
		{
			uint32_t UpdatePlayerQueryAffectedRows = mysql_affected_rows( (MYSQL *)conn );

			if( !UpdatePlayerQueryAffectedRows )
			{
				DB_Log( "mysql_affected_rows is zero, " + UpdatePlayerQuery );
				m_Error = true;
				return;
			}
		}
	}
}

void CCallableGameCustomDotAAdd :: ThreadedJob( void *conn )
{
	m_StartTicks = GetTicks( );

	ThreadedJob2( conn );

	m_EndTicks = GetTicks( );
	m_Ready = true;
}

void CCallableGameCustomDotAAdd :: ThreadedJob2( void *conn )
{
	for( vector<CDBGamePlayer *> :: iterator i = m_DBGamePlayers.begin( ); i != m_DBGamePlayers.end( ); ++i )
	{
		string EscName = MySQLEscapeString( conn, (*i)->GetName( ) );
		transform( EscName.begin( ), EscName.end( ), EscName.begin( ), (int(*)(int))tolower );

		CDBPlayer2 *DBPlayer2 = NULL;

		uint32_t PlayerID = 0;
		uint32_t PlayerBanGroupID = 0;
		uint32_t PlayerStatsDotAID = 0;

		string PlayerSelectQuery = "SELECT new_player.id, new_player.ban_group_id, new_player.div1_stats_dota_id FROM new_player WHERE new_player.name = '" + EscName + "' AND new_player.server_id = " + UTIL_ToString( GetServerID( (*i)->GetSpoofedRealm( ) ) ) + " LIMIT 1";

		if( mysql_real_query( (MYSQL *)conn, PlayerSelectQuery.c_str( ), PlayerSelectQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( PlayerSelectQuery, mysql_error( (MYSQL *)conn ) );
			m_Error = true;
			return;
		}
		else
		{
			MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

			if( Result )
			{
				vector<string> Row = MySQLFetchRow( Result );

				if( Row.size( ) == 3 )
				{
					// this player has new_player record in the DB

					PlayerID = UTIL_ToUInt32( Row[0] );
					PlayerBanGroupID = UTIL_ToUInt32( Row[1] );
					PlayerStatsDotAID = UTIL_ToUInt32( Row[2] );

					m_DBPlayers2.push_back( new CDBPlayer2( (*i)->GetBanned( ), (*i)->GetName( ), (*i)->GetSpoofedRealm( ), (*i)->GetColour( ), PlayerID, PlayerBanGroupID, PlayerStatsDotAID ) );
				}
				else
				{
					// this player doesn't have new_player record

					string InsertPlayerQuery = "INSERT INTO new_player ( server_id, name ) VALUES ( " + UTIL_ToString( GetServerID( (*i)->GetSpoofedRealm( ) ) ) + ", '" + EscName + "' )";

					if( mysql_real_query( (MYSQL *)conn, InsertPlayerQuery.c_str( ), InsertPlayerQuery.size( ) ) != 0 )
					{
						LOG_Failed_Query( InsertPlayerQuery, mysql_error( (MYSQL *)conn ) );
						m_Error = true;
						return;
					}
					else
					{
						PlayerID = mysql_insert_id( (MYSQL *)conn );

						if( PlayerID )
							m_DBPlayers2.push_back( new CDBPlayer2( (*i)->GetBanned( ), (*i)->GetName( ), (*i)->GetSpoofedRealm( ), (*i)->GetColour( ), PlayerID, PlayerBanGroupID, PlayerStatsDotAID ) );
						else
						{
							DB_Log( "mysql_insert_id is zero, " + InsertPlayerQuery );
							m_Error = true;
							return;
						}
					}
				}

				mysql_free_result( Result );
			}
			else
			{
				LOG_Failed_Query( PlayerSelectQuery, mysql_error( (MYSQL *)conn ) );
				m_Error = true;
				return;
			}
		}

		if( (*i)->GetName( ) == m_CreatorName )
			m_DBCreatorPlayerID = PlayerID;
	}

	// save game

	//string EscLobbyLog = MySQLEscapeString( conn, m_LobbyLog );
	string EscGameName = MySQLEscapeString( conn, m_GameName );
	//string EscGameLog = MySQLEscapeString( conn, m_GameLog );
	string EscMap = MySQLEscapeString( conn, m_Map );

	if( m_DBGameID )
	{
		// normal situation, this game already has a record in the DB inserted at the begining of the game

		string UpdateGameQuery = "UPDATE new_game SET lobbyduration = " + UTIL_ToString( m_LobbyDuration ) + ", map = '" + EscMap + "', datetime = UNIX_TIMESTAMP(), gamename = '" + EscGameName + "', duration = " + UTIL_ToString( m_Duration ) + ", gamestate = " + UTIL_ToString( m_GameState ) + ", creator_player_id = " + UTIL_ToString( m_DBCreatorPlayerID ) + ", creator_server_id = " + UTIL_ToString( m_CreatorServerID ) + ", rmk = " + ( m_RMK ? "1" : "0" ) + ", game_type = " + UTIL_ToString( m_GameType ) + ", game_in_progress = 0 WHERE new_game.id = " + UTIL_ToString( m_DBGameID ) + " LIMIT 1";

		if( mysql_real_query( (MYSQL *)conn, UpdateGameQuery.c_str( ), UpdateGameQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( UpdateGameQuery, mysql_error( (MYSQL *)conn ) );
			m_Error = true;
			return;
		}
		else
		{
			uint32_t UpdateGameQueryAffectedRows = mysql_affected_rows( (MYSQL *)conn );

			if( !UpdateGameQueryAffectedRows )
			{
				DB_Log( "mysql_affected_rows is zero, " + UpdateGameQuery );
				m_Error = true;
				return;
			}
		}
	}
	else
	{
		// this game DB ID is zero
		// something went wrong when we tried to insert game into the DB when game started

		string InsertGameQuery = "INSERT INTO new_game ( lobbyduration, map, datetime, gamename, duration, gamestate, creator_player_id, creator_server_id, rmk, game_type, bot_region ) VALUES ( " + UTIL_ToString( m_LobbyDuration ) + ", '" + EscMap + "', UNIX_TIMESTAMP( ), '" + EscGameName + "', " + UTIL_ToString( m_Duration ) + ", " + UTIL_ToString( m_GameState ) + ", " + UTIL_ToString( m_DBCreatorPlayerID ) + ", " + UTIL_ToString( m_CreatorServerID ) + ", " + ( m_RMK ? "1" : "0" ) + ", " + UTIL_ToString( m_GameType ) + ", '"+ m_region + "' )";

		if( mysql_real_query( (MYSQL *)conn, InsertGameQuery.c_str( ), InsertGameQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( InsertGameQuery, mysql_error( (MYSQL *)conn ) );
			m_Error = true;
			return;
		}
		else
		{
			m_DBGameID = mysql_insert_id( (MYSQL *)conn );

			if( !m_DBGameID )
			{
				DB_Log( "mysql_insert_id is zero, " + InsertGameQuery );
				m_Error = true;
				return;
			}
		}
	}

	// save game players

	string InsertGamePlayersQueryValues;

	for( vector<CDBGamePlayer *> :: iterator i = m_DBGamePlayers.begin( ); i != m_DBGamePlayers.end( ); ++i )
	{
		for( list<CDBPlayer2 *> :: iterator j = m_DBPlayers2.begin( ); j != m_DBPlayers2.end( ); ++j )
		{
			// use name instead of color because two game players can share one color, e.g. both observers have color 12
			//if( (*i)->GetColour( ) == (*j)->GetColor( ) )
			if( (*i)->GetName( ) == (*j)->GetName( ) )
			{
				if( InsertGamePlayersQueryValues.empty( ) )
					InsertGamePlayersQueryValues = "( " + UTIL_ToString( m_DBGameID ) + ", " + UTIL_ToString( (*j)->GetPlayerID( ) ) + ", '" + (*i)->GetIP( ) + "', " + UTIL_ToString( (*i)->GetReserved( ) ) + ", " + UTIL_ToString( (*i)->GetLoadingTime( ) ) + ", " + UTIL_ToString( (*i)->GetLeft( ) ) + ", '" + (*i)->GetLeftReason( ) + "', " + UTIL_ToString( (*i)->GetTeam( ) ) + ", " + UTIL_ToString( (*i)->GetColour( ) ) + ", " + ( (*i)->GetGProxy( ) ? "1" : "0" ) + " )";
				else
					InsertGamePlayersQueryValues += ", ( " + UTIL_ToString( m_DBGameID ) + ", " + UTIL_ToString( (*j)->GetPlayerID( ) ) + ", '" + (*i)->GetIP( ) + "', " + UTIL_ToString( (*i)->GetReserved( ) ) + ", " + UTIL_ToString( (*i)->GetLoadingTime( ) ) + ", " + UTIL_ToString( (*i)->GetLeft( ) ) + ", '" + (*i)->GetLeftReason( ) + "', " + UTIL_ToString( (*i)->GetTeam( ) ) + ", " + UTIL_ToString( (*i)->GetColour( ) ) + ", " + ( (*i)->GetGProxy( ) ? "1" : "0" ) + " )";

				break;
			}
		}
	}

	string InsertGamePlayersQuery = "INSERT INTO new_gameplayer ( game_id, player_id, ip, reserved, loadingtime, `left`, leftreason, team, color, gproxy ) VALUES " + InsertGamePlayersQueryValues;

	if( mysql_real_query( (MYSQL *)conn, InsertGamePlayersQuery.c_str( ), InsertGamePlayersQuery.size( ) ) != 0 )
	{
		LOG_Failed_Query( InsertGamePlayersQuery, mysql_error( (MYSQL *)conn ) );
		m_Error = true;
		return;
	}

	// save DotA game

	string EscMode1 = MySQLEscapeString( conn, m_Mode1 );
	string EscMode2 = MySQLEscapeString( conn, m_Mode2 );

	string InsertDotAGameQuery = "INSERT INTO new_customdotagame ( game_id, winner, creeps_spawned_time, stats_over_time, min, sec, mode1, mode2 ) VALUES ( " + UTIL_ToString( m_DBGameID ) + ", " + UTIL_ToString( m_Winner ) + ", " + UTIL_ToString( m_CreepsSpawnedTime ) + ", " + UTIL_ToString( m_CollectDotAStatsOverTime ) + ", " + UTIL_ToString( m_Min ) + ", " + UTIL_ToString( m_Sec ) + ", '" + EscMode1 + "', '" + EscMode2 + "' )";

	if( mysql_real_query( (MYSQL *)conn, InsertDotAGameQuery.c_str( ), InsertDotAGameQuery.size( ) ) != 0 )
	{
		LOG_Failed_Query( InsertDotAGameQuery, mysql_error( (MYSQL *)conn ) );
		m_Error = true;
		return;
	}
	else
	{
		m_DBDotAGameID = mysql_insert_id( (MYSQL *)conn );

		if( !m_DBDotAGameID )
		{
			DB_Log( "mysql_insert_id is zero, " + InsertDotAGameQuery );
			m_Error = true;
			return;
		}
	}

	// save dota players

	string InsertDotAPlayersQueryValues;

	for( vector<CDBDotAPlayer *> :: iterator i = m_DBDotAPlayers.begin( ); i != m_DBDotAPlayers.end( ); ++i )
	{
		if( InsertDotAPlayersQueryValues.empty( ) )
			InsertDotAPlayersQueryValues = "( " + UTIL_ToString( m_DBGameID ) + ", " + UTIL_ToString( (*i)->GetColour( ) ) + ", " + UTIL_ToString( (*i)->GetLevel( ) ) + ", " + UTIL_ToString( (*i)->GetKills( ) ) + ", " + UTIL_ToString( (*i)->GetDeaths( ) ) + ", " + UTIL_ToString( (*i)->GetCreepKills( ) ) + ", " + UTIL_ToString( (*i)->GetCreepDenies( ) ) + ", " + UTIL_ToString( (*i)->GetAssists( ) ) + ", " + UTIL_ToString( (*i)->GetGold( ) ) + ", " + UTIL_ToString( (*i)->GetNeutralKills( ) ) + ", '" + (*i)->GetItem( 0 ) + "', '" + (*i)->GetItem( 1 ) + "', '" + (*i)->GetItem( 2 ) + "', '" + (*i)->GetItem( 3 ) + "', '" + (*i)->GetItem( 4 ) + "', '" + (*i)->GetItem( 5 ) + "', '" + (*i)->GetHero( ) + "', " + UTIL_ToString( (*i)->GetNewColour( ) ) + ", " + UTIL_ToString( (*i)->GetTowerKills( ) ) + ", " + UTIL_ToString( (*i)->GetRaxKills( ) ) + ", " + UTIL_ToString( (*i)->GetCourierKills( ) ) + " )";
		else
			InsertDotAPlayersQueryValues += ", ( " + UTIL_ToString( m_DBGameID ) + ", " + UTIL_ToString( (*i)->GetColour( ) ) + ", " + UTIL_ToString( (*i)->GetLevel( ) ) + ", " + UTIL_ToString( (*i)->GetKills( ) ) + ", " + UTIL_ToString( (*i)->GetDeaths( ) ) + ", " + UTIL_ToString( (*i)->GetCreepKills( ) ) + ", " + UTIL_ToString( (*i)->GetCreepDenies( ) ) + ", " + UTIL_ToString( (*i)->GetAssists( ) ) + ", " + UTIL_ToString( (*i)->GetGold( ) ) + ", " + UTIL_ToString( (*i)->GetNeutralKills( ) ) + ", '" + (*i)->GetItem( 0 ) + "', '" + (*i)->GetItem( 1 ) + "', '" + (*i)->GetItem( 2 ) + "', '" + (*i)->GetItem( 3 ) + "', '" + (*i)->GetItem( 4 ) + "', '" + (*i)->GetItem( 5 ) + "', '" + (*i)->GetHero( ) + "', " + UTIL_ToString( (*i)->GetNewColour( ) ) + ", " + UTIL_ToString( (*i)->GetTowerKills( ) ) + ", " + UTIL_ToString( (*i)->GetRaxKills( ) ) + ", " + UTIL_ToString( (*i)->GetCourierKills( ) ) + " )";
	}

	string InsertDotAPlayersQuery = "INSERT INTO new_customdotaplayer ( game_id, color, level, kills, deaths, creepkills, creepdenies, assists, gold, neutralkills, item1, item2, item3, item4, item5, item6, hero, newcolor, towerkills, raxkills, courierkills ) VALUES " + InsertDotAPlayersQueryValues;

	if( mysql_real_query( (MYSQL *)conn, InsertDotAPlayersQuery.c_str( ), InsertDotAPlayersQuery.size( ) ) != 0 )
	{
		LOG_Failed_Query( InsertDotAPlayersQuery, mysql_error( (MYSQL *)conn ) );
		m_Error = true;
		return;
	}

	// update players

	for( list<CDBPlayer2 *> :: iterator i = m_DBPlayers2.begin( ); i != m_DBPlayers2.end( ); ++i )
	{
		string UpdatePlayerQuery = "UPDATE new_player SET custom_games_count = custom_games_count + 1 WHERE id = " + UTIL_ToString( (*i)->GetPlayerID( ) ) + " LIMIT 1";

		if( mysql_real_query( (MYSQL *)conn, UpdatePlayerQuery.c_str( ), UpdatePlayerQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( UpdatePlayerQuery, mysql_error( (MYSQL *)conn ) );
			m_Error = true;
			return;
		}
		else
		{
			uint32_t UpdatePlayerQueryAffectedRows = mysql_affected_rows( (MYSQL *)conn );

			if( !UpdatePlayerQueryAffectedRows )
			{
				DB_Log( "mysql_affected_rows is zero, " + UpdatePlayerQuery );
				m_Error = true;
				return;
			}
		}
	}
}

void CCallableGameDiv1DotAAdd :: ThreadedJob( void *conn )
{
	m_StartTicks = GetTicks( );

	ThreadedJob2( conn );

	m_EndTicks = GetTicks( );
	m_Ready = true;
}

void CCallableGameDiv1DotAAdd :: ThreadedJob2( void *conn )
{
	for( vector<CDBGamePlayer *> :: iterator i = m_DBGamePlayers.begin( ); i != m_DBGamePlayers.end( ); ++i )
	{
		string EscName = MySQLEscapeString( conn, (*i)->GetName( ) );
		transform( EscName.begin( ), EscName.end( ), EscName.begin( ), (int(*)(int))tolower );

		CDBPlayer2 *DBPlayer2 = NULL;

		uint32_t PlayerID = 0;
		uint32_t PlayerBanGroupID = 0;
		uint32_t PlayerStatsDotAID = 0;

		string PlayerSelectQuery = "SELECT new_player.id, new_player.ban_group_id, new_player.div1_stats_dota_id FROM new_player WHERE new_player.name = '" + EscName + "' AND new_player.server_id = " + UTIL_ToString( GetServerID( (*i)->GetSpoofedRealm( ) ) ) + " LIMIT 1";

		if( mysql_real_query( (MYSQL *)conn, PlayerSelectQuery.c_str( ), PlayerSelectQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( PlayerSelectQuery, mysql_error( (MYSQL *)conn ) );
			m_Error = true;
			return;
		}
		else
		{
			MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

			if( Result )
			{
				vector<string> Row = MySQLFetchRow( Result );

				if( Row.size( ) == 3 )
				{
					// this player has new_player record in the DB

					PlayerID = UTIL_ToUInt32( Row[0] );
					PlayerBanGroupID = UTIL_ToUInt32( Row[1] );
					PlayerStatsDotAID = UTIL_ToUInt32( Row[2] );

					m_DBPlayers2.push_back( new CDBPlayer2( (*i)->GetBanned( ), (*i)->GetName( ), (*i)->GetSpoofedRealm( ), (*i)->GetColour( ), PlayerID, PlayerBanGroupID, PlayerStatsDotAID ) );
				}
				else
				{
					// this player doesn't have new_player record

					string InsertPlayerQuery = "INSERT INTO new_player ( server_id, name ) VALUES ( " + UTIL_ToString( GetServerID( (*i)->GetSpoofedRealm( ) ) ) + ", '" + EscName + "' )";

					if( mysql_real_query( (MYSQL *)conn, InsertPlayerQuery.c_str( ), InsertPlayerQuery.size( ) ) != 0 )
					{
						LOG_Failed_Query( InsertPlayerQuery, mysql_error( (MYSQL *)conn ) );
						m_Error = true;
						return;
					}
					else
					{
						PlayerID = mysql_insert_id( (MYSQL *)conn );

						if( PlayerID )
							m_DBPlayers2.push_back( new CDBPlayer2( (*i)->GetBanned( ), (*i)->GetName( ), (*i)->GetSpoofedRealm( ), (*i)->GetColour( ), PlayerID, PlayerBanGroupID, PlayerStatsDotAID ) );
						else
						{
							DB_Log( "mysql_insert_id is zero, " + InsertPlayerQuery );
							m_Error = true;
							return;
						}
					}
				}

				mysql_free_result( Result );
			}
			else
			{
				LOG_Failed_Query( PlayerSelectQuery, mysql_error( (MYSQL *)conn ) );
				m_Error = true;
				return;
			}
		}

		if( (*i)->GetName( ) == m_CreatorName )
			m_DBCreatorPlayerID = PlayerID;
	}

	// save game

	//string EscLobbyLog = MySQLEscapeString( conn, m_LobbyLog );
	string EscGameName = MySQLEscapeString( conn, m_GameName );
	//string EscGameLog = MySQLEscapeString( conn, m_GameLog );
	string EscMap = MySQLEscapeString( conn, m_Map );

	if( m_DBGameID )
	{
		// normal situation, this game already has a record in the DB inserted at the begining of the game

		string UpdateGameQuery = "UPDATE new_game SET lobbyduration = " + UTIL_ToString( m_LobbyDuration ) + ", map = '" + EscMap + "', datetime = UNIX_TIMESTAMP(), gamename = '" + EscGameName + "', duration = " + UTIL_ToString( m_Duration ) + ", gamestate = " + UTIL_ToString( m_GameState ) + ", creator_player_id = " + UTIL_ToString( m_DBCreatorPlayerID ) + ", creator_server_id = " + UTIL_ToString( m_CreatorServerID ) + ", rmk = " + ( m_RMK ? "1" : "0" ) + ", game_type = " + UTIL_ToString( m_GameType ) + ", game_in_progress = 0 WHERE new_game.id = " + UTIL_ToString( m_DBGameID ) + " LIMIT 1";

		if( mysql_real_query( (MYSQL *)conn, UpdateGameQuery.c_str( ), UpdateGameQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( UpdateGameQuery, mysql_error( (MYSQL *)conn ) );
			m_Error = true;
			return;
		}
		else
		{
			uint32_t UpdateGameQueryAffectedRows = mysql_affected_rows( (MYSQL *)conn );

			if( !UpdateGameQueryAffectedRows )
			{
				DB_Log( "mysql_affected_rows is zero, " + UpdateGameQuery );
				m_Error = true;
				return;
			}
		}
	}
	else
	{
		// this game DB ID is zero
		// something went wrong when we tried to insert game into the DB when game started

		string InsertGameQuery = "INSERT INTO new_game ( lobbyduration, map, datetime, gamename, duration, gamestate, creator_player_id, creator_server_id, rmk, game_type, bot_region ) VALUES ( " + UTIL_ToString( m_LobbyDuration ) + ", '" + EscMap + "', UNIX_TIMESTAMP( ), '" + EscGameName + "', " + UTIL_ToString( m_Duration ) + ", " + UTIL_ToString( m_GameState ) + ", " + UTIL_ToString( m_DBCreatorPlayerID ) + ", " + UTIL_ToString( m_CreatorServerID ) + ", " + ( m_RMK ? "1" : "0" ) + ", " + UTIL_ToString( m_GameType ) + ", '" + m_region + "' )";

		if( mysql_real_query( (MYSQL *)conn, InsertGameQuery.c_str( ), InsertGameQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( InsertGameQuery, mysql_error( (MYSQL *)conn ) );
			m_Error = true;
			return;
		}
		else
		{
			m_DBGameID = mysql_insert_id( (MYSQL *)conn );

			if( !m_DBGameID )
			{
				DB_Log( "mysql_insert_id is zero, " + InsertGameQuery );
				m_Error = true;
				return;
			}
		}
	}

	// save game players

	string InsertGamePlayersQueryValues;

	for( vector<CDBGamePlayer *> :: iterator i = m_DBGamePlayers.begin( ); i != m_DBGamePlayers.end( ); ++i )
	{
		for( list<CDBPlayer2 *> :: iterator j = m_DBPlayers2.begin( ); j != m_DBPlayers2.end( ); ++j )
		{
			// use name instead of color because two game players can share one color, e.g. both observers have color 12
			//if( (*i)->GetColour( ) == (*j)->GetColor( ) )
			if( (*i)->GetName( ) == (*j)->GetName( ) )
			{
				if( InsertGamePlayersQueryValues.empty( ) )
					InsertGamePlayersQueryValues = "( " + UTIL_ToString( m_DBGameID ) + ", " + UTIL_ToString( (*j)->GetPlayerID( ) ) + ", '" + (*i)->GetIP( ) + "', " + UTIL_ToString( (*i)->GetReserved( ) ) + ", " + UTIL_ToString( (*i)->GetLoadingTime( ) ) + ", " + UTIL_ToString( (*i)->GetLeft( ) ) + ", '" + (*i)->GetLeftReason( ) + "', " + UTIL_ToString( (*i)->GetTeam( ) ) + ", " + UTIL_ToString( (*i)->GetColour( ) ) + ", " + ( (*i)->GetGProxy( ) ? "1" : "0" ) + " )";
				else
					InsertGamePlayersQueryValues += ", ( " + UTIL_ToString( m_DBGameID ) + ", " + UTIL_ToString( (*j)->GetPlayerID( ) ) + ", '" + (*i)->GetIP( ) + "', " + UTIL_ToString( (*i)->GetReserved( ) ) + ", " + UTIL_ToString( (*i)->GetLoadingTime( ) ) + ", " + UTIL_ToString( (*i)->GetLeft( ) ) + ", '" + (*i)->GetLeftReason( ) + "', " + UTIL_ToString( (*i)->GetTeam( ) ) + ", " + UTIL_ToString( (*i)->GetColour( ) ) + ", " + ( (*i)->GetGProxy( ) ? "1" : "0" ) + " )";

				break;
			}
		}
	}

	string InsertGamePlayersQuery = "INSERT INTO new_gameplayer ( game_id, player_id, ip, reserved, loadingtime, `left`, leftreason, team, color, gproxy ) VALUES " + InsertGamePlayersQueryValues;

	if( mysql_real_query( (MYSQL *)conn, InsertGamePlayersQuery.c_str( ), InsertGamePlayersQuery.size( ) ) != 0 )
	{
		LOG_Failed_Query( InsertGamePlayersQuery, mysql_error( (MYSQL *)conn ) );
		m_Error = true;
		return;
	}

	// save DotA game

	string EscMode1 = MySQLEscapeString( conn, m_Mode1 );
	string EscMode2 = MySQLEscapeString( conn, m_Mode2 );

	string InsertDotAGameQuery = "INSERT INTO new_div1dotagame ( game_id, winner, creeps_spawned_time, stats_over_time, min, sec, mode1, mode2, ff, scored ) VALUES ( " + UTIL_ToString( m_DBGameID ) + ", " + UTIL_ToString( m_Winner ) + ", " + UTIL_ToString( m_CreepsSpawnedTime ) + ", " + UTIL_ToString( m_CollectDotAStatsOverTime ) + ", " + UTIL_ToString( m_Min ) + ", " + UTIL_ToString( m_Sec ) + ", '" + EscMode1 + "', '" + EscMode2 + "', " + ( m_FF ? "1" : "0" ) + ", " + ( m_Scored ? "1" : "0" ) + " )";

	if( mysql_real_query( (MYSQL *)conn, InsertDotAGameQuery.c_str( ), InsertDotAGameQuery.size( ) ) != 0 )
	{
		LOG_Failed_Query( InsertDotAGameQuery, mysql_error( (MYSQL *)conn ) );
		m_Error = true;
		return;
	}
	else
	{
		m_DBDotAGameID = mysql_insert_id( (MYSQL *)conn );

		if( !m_DBDotAGameID )
		{
			DB_Log( "mysql_insert_id is zero, " + InsertDotAGameQuery );
			m_Error = true;
			return;
		}
	}

	// save dota players

	string InsertDotAPlayersQueryValues;

	for( vector<CDBDotAPlayer *> :: iterator i = m_DBDotAPlayers.begin( ); i != m_DBDotAPlayers.end( ); ++i )
	{
		for( vector<CDBDiv1DotAPlayer *> :: iterator j = m_DBDiv1DotAPlayers.begin( ); j != m_DBDiv1DotAPlayers.end( ); ++j )
		{
			if( (*i)->GetColour( ) == (*j)->GetColor( ) )
			{
				if( InsertDotAPlayersQueryValues.empty( ) )
					InsertDotAPlayersQueryValues = "( " + UTIL_ToString( (*j)->GetRating( ), 0 ) + ", " + UTIL_ToString( (*j)->GetNewRating( ), 0 ) + ", " + UTIL_ToString( m_DBGameID ) + ", " + UTIL_ToString( (*i)->GetColour( ) ) + ", " + UTIL_ToString( (*i)->GetLevel( ) ) + ", " + UTIL_ToString( (*i)->GetKills( ) ) + ", " + UTIL_ToString( (*i)->GetDeaths( ) ) + ", " + UTIL_ToString( (*i)->GetCreepKills( ) ) + ", " + UTIL_ToString( (*i)->GetCreepDenies( ) ) + ", " + UTIL_ToString( (*i)->GetAssists( ) ) + ", " + UTIL_ToString( (*i)->GetGold( ) ) + ", " + UTIL_ToString( (*i)->GetNeutralKills( ) ) + ", '" + (*i)->GetItem( 0 ) + "', '" + (*i)->GetItem( 1 ) + "', '" + (*i)->GetItem( 2 ) + "', '" + (*i)->GetItem( 3 ) + "', '" + (*i)->GetItem( 4 ) + "', '" + (*i)->GetItem( 5 ) + "', '" + (*i)->GetHero( ) + "', " + UTIL_ToString( (*i)->GetNewColour( ) ) + ", " + UTIL_ToString( (*i)->GetTowerKills( ) ) + ", " + UTIL_ToString( (*i)->GetRaxKills( ) ) + ", " + UTIL_ToString( (*i)->GetCourierKills( ) ) + " )";
				else
					InsertDotAPlayersQueryValues += ", ( " + UTIL_ToString( (*j)->GetRating( ), 0 ) + ", " + UTIL_ToString( (*j)->GetNewRating( ), 0 ) + ", " + UTIL_ToString( m_DBGameID ) + ", " + UTIL_ToString( (*i)->GetColour( ) ) + ", " + UTIL_ToString( (*i)->GetLevel( ) ) + ", " + UTIL_ToString( (*i)->GetKills( ) ) + ", " + UTIL_ToString( (*i)->GetDeaths( ) ) + ", " + UTIL_ToString( (*i)->GetCreepKills( ) ) + ", " + UTIL_ToString( (*i)->GetCreepDenies( ) ) + ", " + UTIL_ToString( (*i)->GetAssists( ) ) + ", " + UTIL_ToString( (*i)->GetGold( ) ) + ", " + UTIL_ToString( (*i)->GetNeutralKills( ) ) + ", '" + (*i)->GetItem( 0 ) + "', '" + (*i)->GetItem( 1 ) + "', '" + (*i)->GetItem( 2 ) + "', '" + (*i)->GetItem( 3 ) + "', '" + (*i)->GetItem( 4 ) + "', '" + (*i)->GetItem( 5 ) + "', '" + (*i)->GetHero( ) + "', " + UTIL_ToString( (*i)->GetNewColour( ) ) + ", " + UTIL_ToString( (*i)->GetTowerKills( ) ) + ", " + UTIL_ToString( (*i)->GetRaxKills( ) ) + ", " + UTIL_ToString( (*i)->GetCourierKills( ) ) + " )";

				break;
			}
		}
	}

	string InsertDotAPlayersQuery = "INSERT INTO new_div1dotaplayer ( rating_before, rating_after, game_id, color, level, kills, deaths, creepkills, creepdenies, assists, gold, neutralkills, item1, item2, item3, item4, item5, item6, hero, newcolor, towerkills, raxkills, courierkills ) VALUES " + InsertDotAPlayersQueryValues;
	CONSOLE_Print(InsertDotAPlayersQuery);

	if( mysql_real_query( (MYSQL *)conn, InsertDotAPlayersQuery.c_str( ), InsertDotAPlayersQuery.size( ) ) != 0 )
	{
		LOG_Failed_Query( InsertDotAPlayersQuery, mysql_error( (MYSQL *)conn ) );
		m_Error = true;
		return;
	}

	// save dota stats

	bool IsDotAEmGame = MASL_PROTOCOL::IsDotAEmMode( m_Mode1 );

	for( vector<CDBDotAPlayer *> :: iterator i = m_DBDotAPlayers.begin( ); i != m_DBDotAPlayers.end( ); ++i )
	{
		CDBDiv1DotAPlayer *DBDiv1DotAPlayer = NULL;
		CDBPlayer2 *DBPlayer2 = NULL;

		for( vector<CDBDiv1DotAPlayer *> :: iterator j = m_DBDiv1DotAPlayers.begin( ); j != m_DBDiv1DotAPlayers.end( ); ++j )
		{
			if( (*i)->GetColour( ) == (*j)->GetColor( ) )
			{
				DBDiv1DotAPlayer = *j;
				break;
			}
		}

		for( list<CDBPlayer2 *> :: iterator j = m_DBPlayers2.begin( ); j != m_DBPlayers2.end( ); ++j )
		{
			if( (*i)->GetColour( ) == (*j)->GetColor( ) )
			{
				DBPlayer2 = *j;
				break;
			}
		}

		if( !DBDiv1DotAPlayer || !DBPlayer2 )
			continue;

		CDBDotAPlayerSummary *DotAPlayerSummary = NULL;

		if( (*i)->GetColour( ) > 11 )
		{
			// this player is an observer, we only need to increase his games_observed count

			if( DBPlayer2->GetStatsDotAID( ) )
			{
				// this player (observer) already has new_stats_dota record so update it

				string UpdateStatsDotAQuery = "UPDATE new_div1_stats_dota SET games = games + 1, games_observed = games_observed + 1, last_played = UNIX_TIMESTAMP( ) WHERE id = " + UTIL_ToString( DBPlayer2->GetStatsDotAID( ) ) + " LIMIT 1";

				if( mysql_real_query( (MYSQL *)conn, UpdateStatsDotAQuery.c_str( ), UpdateStatsDotAQuery.size( ) ) != 0 )
				{
					LOG_Failed_Query( UpdateStatsDotAQuery, mysql_error( (MYSQL *)conn ) );
					m_Error = true;
					return;
				}
				else
				{
					uint32_t UpdateStatsDotAQueryAffectedRows = mysql_affected_rows( (MYSQL *)conn );

					if( !UpdateStatsDotAQueryAffectedRows )
					{
						DB_Log( "mysql_affected_rows is zero, " + UpdateStatsDotAQuery );
						m_Error = true;
						return;
					}
				}

				string SelectStatsDotAQuery = "SELECT id, rating, highest_rating, games, wins, loses, kills, deaths, creepkills, creepdenies, assists, neutralkills, games_observed FROM new_div1_stats_dota WHERE id = " + UTIL_ToString( DBPlayer2->GetStatsDotAID( ) ) + " LIMIT 1";

				if( mysql_real_query( (MYSQL *)conn, SelectStatsDotAQuery.c_str( ), SelectStatsDotAQuery.size( ) ) != 0 )
				{
					LOG_Failed_Query( SelectStatsDotAQuery, mysql_error( (MYSQL *)conn ) );
					m_Error = true;
					return;
				}
				else
				{
					MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

					if( Result )
					{
						vector<string> Row = MySQLFetchRow( Result );

						if( Row.size( ) == 13 )
						{
							DotAPlayerSummary = new CDBDotAPlayerSummary(
								UTIL_ToDouble( Row[1] ),					// Rating
								UTIL_ToDouble( Row[2] ),					// Highest Rating
								UTIL_ToUInt32( Row[3] ),					// Games
								UTIL_ToUInt32( Row[4] ),					// Wins
								UTIL_ToUInt32( Row[5] ),					// Loses
								UTIL_ToUInt32( Row[6] ),					// Kills
								UTIL_ToUInt32( Row[7] ),					// Deaths
								UTIL_ToUInt32( Row[8] ),					// Creep Kills
								UTIL_ToUInt32( Row[9] ),					// Creep Denies
								UTIL_ToUInt32( Row[10] ),					// Assists
								UTIL_ToUInt32( Row[11] ),					// Neutral Kills
								UTIL_ToUInt32( Row[12] )					// Games Observed
								);
						}
						else
							LOG_Failed_Query( SelectStatsDotAQuery, mysql_error( (MYSQL *)conn ) );

						mysql_free_result( Result );
					}
					else
						LOG_Failed_Query( SelectStatsDotAQuery, mysql_error( (MYSQL *)conn ) );
				}
			}
			else
			{
				// this player (observer) doesn't have new_stats_dota record so insert new

				string InsertStatsDotAQuery = "INSERT INTO new_div1_stats_dota ( games, games_observed, last_played ) VALUES ( 1, 1, UNIX_TIMESTAMP( ) )";

				if( mysql_real_query( (MYSQL *)conn, InsertStatsDotAQuery.c_str( ), InsertStatsDotAQuery.size( ) ) != 0 )
				{
					LOG_Failed_Query( InsertStatsDotAQuery, mysql_error( (MYSQL *)conn ) );
					m_Error = true;
					return;
				}
				else
				{
					uint32_t StatsDotAID = mysql_insert_id( (MYSQL *)conn );

					if( !StatsDotAID )
					{
						DB_Log( "mysql_insert_id is zero, " + InsertStatsDotAQuery );
						m_Error = true;
						return;
					}
					else
					{
						DBPlayer2->SetStatsDotAID( StatsDotAID );
						DBPlayer2->SetUpdateStatsDotAID( true );

						DotAPlayerSummary = new CDBDotAPlayerSummary(
							DBDiv1DotAPlayer->GetNewRating( ),		// Rating
							DBDiv1DotAPlayer->GetHighestRating( ),	// Highest Rating
							1,										// Games
							0,										// Wins
							0,										// Loses
							0,										// Kills
							0,										// Deaths
							0,										// Creep Kills
							0,										// Creep Denies
							0,										// Assists
							0,										// Neutral Kills
							1										// Games Observed
							);
					}
				}
			}

			DBPlayer2->SetDotAPlayerSummary( DotAPlayerSummary );

			// we updated/inserted games_observed for this player (observer)
			// that's all what's needed for observers so stop here

			break;
		}

		// this is a regular player (not observer)

		if( m_Scored )
		{
			// someone won this game so we have to update/insert a bunch of data

			uint32_t Wins = 0;
			uint32_t Loses = 0;

			if( DBDiv1DotAPlayer->GetTeam( ) == m_Winner - 1 )
				Wins = 1;

			if( DBDiv1DotAPlayer->GetTeam( ) != m_Winner - 1 )
				Loses = 1;

			if( DBPlayer2->GetStatsDotAID( ) )
			{
				// this player already has new_stats_dota record so update it

				string UpdateStatsDotAQuery = "UPDATE new_div1_stats_dota SET rating = " + UTIL_ToString( DBDiv1DotAPlayer->GetNewRating( ), 0 ) + ", highest_rating = " + UTIL_ToString( DBDiv1DotAPlayer->GetNewHighestRating( ), 0 ) + ", last_played = UNIX_TIMESTAMP( ), last_played_scored = UNIX_TIMESTAMP( ), games = games + 1, games_left = games_left + " + ( DBDiv1DotAPlayer->GetBanned( ) ? "1" : "0" ) + ", em_games = em_games + " + ( IsDotAEmGame ? "1" : "0" ) + ", wins = wins + " + UTIL_ToString( Wins ) + ", loses = loses + " + UTIL_ToString( Loses ) + ", kills = kills + " + UTIL_ToString( (*i)->GetKills( ) ) + ", deaths = deaths + " + UTIL_ToString( (*i)->GetDeaths( ) ) + ", assists = assists + " + UTIL_ToString( (*i)->GetAssists( ) ) + ", creepkills = creepkills + " + UTIL_ToString( (*i)->GetCreepKills( ) ) + ", creepdenies = creepdenies + " + UTIL_ToString( (*i)->GetCreepDenies( ) ) + ", neutralkills = neutralkills + " + UTIL_ToString( (*i)->GetNeutralKills( ) ) + ", towerkills = towerkills + " + UTIL_ToString( (*i)->GetTowerKills( ) ) + ", raxkills = raxkills + " + UTIL_ToString( (*i)->GetRaxKills( ) ) + ", courierkills = courierkills + " + UTIL_ToString( (*i)->GetCourierKills( ) ) + " WHERE id = " + UTIL_ToString( DBPlayer2->GetStatsDotAID( ) ) + " LIMIT 1";

				if( mysql_real_query( (MYSQL *)conn, UpdateStatsDotAQuery.c_str( ), UpdateStatsDotAQuery.size( ) ) != 0 )
				{
					LOG_Failed_Query( UpdateStatsDotAQuery, mysql_error( (MYSQL *)conn ) );
					m_Error = true;
					return;
				}
				else
				{
					uint32_t UpdateStatsDotAQueryAffectedRows = mysql_affected_rows( (MYSQL *)conn );

					if( !UpdateStatsDotAQueryAffectedRows )
					{
						DB_Log( "mysql_affected_rows is zero, " + UpdateStatsDotAQuery );
						m_Error = true;
						return;
					}
				}

				string SelectStatsDotAQuery = "SELECT id, rating, highest_rating, games, wins, loses, kills, deaths, creepkills, creepdenies, assists, neutralkills, games_observed FROM new_div1_stats_dota WHERE id = " + UTIL_ToString( DBPlayer2->GetStatsDotAID( ) ) + " LIMIT 1";

				if( mysql_real_query( (MYSQL *)conn, SelectStatsDotAQuery.c_str( ), SelectStatsDotAQuery.size( ) ) != 0 )
				{
					LOG_Failed_Query( SelectStatsDotAQuery, mysql_error( (MYSQL *)conn ) );
					m_Error = true;
					return;
				}
				else
				{
					MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

					if( Result )
					{
						vector<string> Row = MySQLFetchRow( Result );

						if( Row.size( ) == 13 )
						{
							DotAPlayerSummary = new CDBDotAPlayerSummary(
								UTIL_ToDouble( Row[1] ),					// Rating
								UTIL_ToDouble( Row[2] ),					// Highest Rating
								UTIL_ToUInt32( Row[3] ),					// Games
								UTIL_ToUInt32( Row[4] ),					// Wins
								UTIL_ToUInt32( Row[5] ),					// Loses
								UTIL_ToUInt32( Row[6] ),					// Kills
								UTIL_ToUInt32( Row[7] ),					// Deaths
								UTIL_ToUInt32( Row[8] ),					// Creep Kills
								UTIL_ToUInt32( Row[9] ),					// Creep Denies
								UTIL_ToUInt32( Row[10] ),					// Assists
								UTIL_ToUInt32( Row[11] ),					// Neutral Kills
								UTIL_ToUInt32( Row[12] )					// Games Observed
								);
						}
						else
							LOG_Failed_Query( SelectStatsDotAQuery, mysql_error( (MYSQL *)conn ) );

						mysql_free_result( Result );
					}
					else
						LOG_Failed_Query( SelectStatsDotAQuery, mysql_error( (MYSQL *)conn ) );
				}
			}
			else
			{
				// this player doesn't have new_stats_dota record so insert new

				string InsertStatsDotAQuery = "INSERT INTO new_div1_stats_dota ( rating, highest_rating, last_played, last_played_scored, games, games_left, em_games, wins, loses, kills, deaths, assists, creepkills, creepdenies, neutralkills, towerkills, raxkills, courierkills ) VALUES ( " + UTIL_ToString( DBDiv1DotAPlayer->GetNewRating( ), 2 ) + ", " + UTIL_ToString( DBDiv1DotAPlayer->GetNewHighestRating( ), 2 ) + ", UNIX_TIMESTAMP( ), UNIX_TIMESTAMP( ), 1, " + ( DBDiv1DotAPlayer->GetBanned( ) ? "1" : "0" ) + ", " + ( IsDotAEmGame ? "1" : "0" ) + ", " + UTIL_ToString( Wins ) + ", " + UTIL_ToString( Loses ) + ", " + UTIL_ToString( (*i)->GetKills( ) ) + ", " + UTIL_ToString( (*i)->GetDeaths( ) ) + ", " + UTIL_ToString( (*i)->GetAssists( ) ) + ", " + UTIL_ToString( (*i)->GetCreepKills( ) ) + ", " + UTIL_ToString( (*i)->GetCreepDenies( ) ) + ", " + UTIL_ToString( (*i)->GetNeutralKills( ) ) + ", " + UTIL_ToString( (*i)->GetTowerKills( ) ) + ", " + UTIL_ToString( (*i)->GetRaxKills( ) ) + ", " + UTIL_ToString( (*i)->GetCourierKills( ) ) + " )";

				if( mysql_real_query( (MYSQL *)conn, InsertStatsDotAQuery.c_str( ), InsertStatsDotAQuery.size( ) ) != 0 )
				{
					LOG_Failed_Query( InsertStatsDotAQuery, mysql_error( (MYSQL *)conn ) );
					m_Error = true;
					return;
				}
				else
				{
					uint32_t StatsDotAID = mysql_insert_id( (MYSQL *)conn );

					if( !StatsDotAID )
					{
						DB_Log( "mysql_insert_id is zero, " + InsertStatsDotAQuery );
						m_Error = true;
						return;
					}
					else
					{
						DBPlayer2->SetStatsDotAID( StatsDotAID );
						DBPlayer2->SetUpdateStatsDotAID( true );

						DotAPlayerSummary = new CDBDotAPlayerSummary(
							DBDiv1DotAPlayer->GetNewRating( ),
							DBDiv1DotAPlayer->GetHighestRating( ),
							1,
							Wins,
							Loses,
							(*i)->GetKills( ),
							(*i)->GetDeaths( ),
							(*i)->GetCreepKills( ),
							(*i)->GetCreepDenies( ),
							(*i)->GetAssists( ),
							(*i)->GetNeutralKills( ),
							0									// Games Observed
							);
					}
				}
			}

			// we need DotAPlayerSummary object when this function finishes because we have to update bot DPS cache with it
			// so save it in the list of players that the function returns

			DBPlayer2->SetDotAPlayerSummary( DotAPlayerSummary );
		}
		else
		{
			// noone won this game so we have to update/insert games to +1 and last_played to UNIX_TIMESTAMP()

			if( DBPlayer2->GetStatsDotAID( ) )
			{
				// this player already has new_stats_dota record so update it

				string UpdateStatsDotAQuery = "UPDATE new_div1_stats_dota SET last_played = UNIX_TIMESTAMP( ), games = games + 1, games_left = games_left + " + ( DBDiv1DotAPlayer->GetBanned( ) ? string( "1" ) : string( "0" ) ) + ", em_games = em_games + " + ( IsDotAEmGame ? "1" : "0" ) + ", draws = draws + 1 WHERE id = " + UTIL_ToString( DBPlayer2->GetStatsDotAID( ) ) + " LIMIT 1";

				if( mysql_real_query( (MYSQL *)conn, UpdateStatsDotAQuery.c_str( ), UpdateStatsDotAQuery.size( ) ) != 0 )
				{
					LOG_Failed_Query( UpdateStatsDotAQuery, mysql_error( (MYSQL *)conn ) );
					m_Error = true;
					return;
				}
				else
				{
					uint32_t UpdateStatsDotAQueryAffectedRows = mysql_affected_rows( (MYSQL *)conn );

					if( !UpdateStatsDotAQueryAffectedRows )
					{
						DB_Log( "mysql_affected_rows is zero, " + UpdateStatsDotAQuery );
						m_Error = true;
						return;
					}
				}

				string SelectStatsDotAQuery = "SELECT id, rating, highest_rating, games, wins, loses, kills, deaths, creepkills, creepdenies, assists, neutralkills, games_observed FROM new_div1_stats_dota WHERE id = " + UTIL_ToString( DBPlayer2->GetStatsDotAID( ) ) + " LIMIT 1";

				if( mysql_real_query( (MYSQL *)conn, SelectStatsDotAQuery.c_str( ), SelectStatsDotAQuery.size( ) ) != 0 )
				{
					LOG_Failed_Query( SelectStatsDotAQuery, mysql_error( (MYSQL *)conn ) );
					m_Error = true;
					return;
				}
				else
				{
					MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

					if( Result )
					{
						vector<string> Row = MySQLFetchRow( Result );

						if( Row.size( ) == 13 )
						{
							DotAPlayerSummary = new CDBDotAPlayerSummary(
								UTIL_ToDouble( Row[1] ),					// Rating
								UTIL_ToDouble( Row[2] ),					// Highest Rating
								UTIL_ToUInt32( Row[3] ),					// Games
								UTIL_ToUInt32( Row[4] ),					// Wins
								UTIL_ToUInt32( Row[5] ),					// Loses
								UTIL_ToUInt32( Row[6] ),					// Kills
								UTIL_ToUInt32( Row[7] ),					// Deaths
								UTIL_ToUInt32( Row[8] ),					// Creep Kills
								UTIL_ToUInt32( Row[9] ),					// Creep Denies
								UTIL_ToUInt32( Row[10] ),					// Assists
								UTIL_ToUInt32( Row[11] ),					// Neutral Kills
								UTIL_ToUInt32( Row[12] )					// Games Observed
								);
						}
						else
							LOG_Failed_Query( SelectStatsDotAQuery, mysql_error( (MYSQL *)conn ) );

						mysql_free_result( Result );
					}
					else
						LOG_Failed_Query( SelectStatsDotAQuery, mysql_error( (MYSQL *)conn ) );
				}
			}
			else
			{
				// this player doesn't have new_div1_stats_dota record so insert new one

				string InsertStatsDotAQuery = "INSERT INTO new_div1_stats_dota ( rating, highest_rating, last_played, games, games_left, em_games, draws ) VALUES ( " + UTIL_ToString( DBDiv1DotAPlayer->GetNewRating( ), 0 ) + ", " + UTIL_ToString( DBDiv1DotAPlayer->GetNewHighestRating( ), 0 ) + ", UNIX_TIMESTAMP( ), 1, " + ( DBDiv1DotAPlayer->GetBanned( ) ? "1" : "0" ) + ", " + ( IsDotAEmGame ? "1" : "0" ) + ", 1 )";

				if( mysql_real_query( (MYSQL *)conn, InsertStatsDotAQuery.c_str( ), InsertStatsDotAQuery.size( ) ) != 0 )
				{
					LOG_Failed_Query( InsertStatsDotAQuery, mysql_error( (MYSQL *)conn ) );
					m_Error = true;
					return;
				}
				else
				{
					uint32_t StatsDotAID = mysql_insert_id( (MYSQL *)conn );

					if( !StatsDotAID )
					{
						DB_Log( "mysql_insert_id is zero, " + InsertStatsDotAQuery );
						m_Error = true;
						return;
					}
					else
					{
						DBPlayer2->SetStatsDotAID( StatsDotAID );
						DBPlayer2->SetUpdateStatsDotAID( true );

						DotAPlayerSummary = new CDBDotAPlayerSummary(
							DBDiv1DotAPlayer->GetNewRating( ),			// Rating
							DBDiv1DotAPlayer->GetHighestRating( ),		// Highest Rating
							1,											// Games
							0,											// Wins
							0,											// Loses
							0,											// Kills
							0,											// Deaths
							0,											// Creep Kills
							0,											// Creep Denies
							0,											// Assists
							0,											// Neutral Kills
							0											// Games Observed
							);
					}
				}
			}

			// we need DotAPlayerSummary object when this function finishes because we have to update bot DPS cache with it
			// so save it in the list of players that the function returns

			DBPlayer2->SetDotAPlayerSummary( DotAPlayerSummary );
		}
	}

	// update players

	for( list<CDBPlayer2 *> :: iterator i = m_DBPlayers2.begin( ); i != m_DBPlayers2.end( ); ++i )
	{
		string UpdatePlayerQuery;

		if( (*i)->GetUpdateStatsDotAID( ) )
			UpdatePlayerQuery = "UPDATE new_player SET div1_games_count = div1_games_count + 1, div1_stats_dota_id = " + UTIL_ToString( (*i)->GetStatsDotAID( ) ) + " WHERE id = " + UTIL_ToString( (*i)->GetPlayerID( ) ) + " LIMIT 1";
		else
			UpdatePlayerQuery = "UPDATE new_player SET div1_games_count = div1_games_count + 1 WHERE id = " + UTIL_ToString( (*i)->GetPlayerID( ) ) + " LIMIT 1";

		if( mysql_real_query( (MYSQL *)conn, UpdatePlayerQuery.c_str( ), UpdatePlayerQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( UpdatePlayerQuery, mysql_error( (MYSQL *)conn ) );
			m_Error = true;
			return;
		}
		else
		{
			uint32_t UpdatePlayerQueryAffectedRows = mysql_affected_rows( (MYSQL *)conn );

			if( !UpdatePlayerQueryAffectedRows )
			{
				DB_Log( "mysql_affected_rows is zero, " + UpdatePlayerQuery );
				m_Error = true;
				return;
			}
		}
	}
}

void CCallableBanAdd :: ThreadedJob( void *conn )
{
	m_StartTicks = GetTicks( );

	m_Result = BanAdd( conn, m_Player, m_ServerID, m_GameID );

	m_EndTicks = GetTicks( );
	m_Ready = true;
}

void CCallableDIV1BanAdd :: ThreadedJob( void *conn )
{
	m_StartTicks = GetTicks( );

	m_Result = DIV1BanAdd( conn, m_AdminPlayerName, m_AdminServerID, m_VictimPlayerName, m_VictimServerID, m_GameID, m_Reason );

	m_EndTicks = GetTicks( );
	m_Ready = true;
}


void CCallableGameID :: ThreadedJob( void *conn )
{
	m_StartTicks = GetTicks( );

	m_Result = GameID( conn, m_region );

	m_EndTicks = GetTicks( );
	m_Ready = true;
}

void CCallableCacheList :: ThreadedJob( void *conn )
{
	m_StartTicks = GetTicks( );

	m_Result = CacheList( conn, m_Server );

	m_EndTicks = GetTicks( );
	m_Ready = true;
}

void CCallableContributorList :: ThreadedJob( void *conn )
{
	m_StartTicks = GetTicks( );

	m_Result = ContributorList( conn, m_Server );

	m_EndTicks = GetTicks( );
	m_Ready = true;
}

void CCallableUnixTimestampGet :: ThreadedJob( void *conn )
{
	m_StartTicks = GetTicks( );

	m_Result = UnixTimestampGet( conn );

	m_EndTicks = GetTicks( );
	m_Ready = true;
}

//
// global helper functions
//

FromCodes FromCheck( void *conn, uint32_t ip )
{
	// http://ip-to-country.webhosting.info/node/view/54
	// http://ip-to-country.webhosting.info/downloads/ip-to-country.csv.zip
	// LOAD DATA INFILE '/path/to/csv/iptocountry.csv' INTO TABLE `iptocountry` FIELDS TERMINATED BY ',' ENCLOSED BY '"' LINES TERMINATED BY '\r\n';

	FromCodes From = FromCodes( "??", "??" );

	string Query = "SELECT country_code2, country_name FROM new_ip_to_country WHERE ip_from <= " + UTIL_ToString( ip ) + " AND ip_to >= " + UTIL_ToString( ip ) + " LIMIT 1";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		LOG_Failed_Query( Query, mysql_error( (MYSQL *)conn ) );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if (Row.size() == 2) {
				From = FromCodes(Row[0], Row[1]);
			}
			else {
				DB_Log("[MYSQL] error checking from codes [" + UTIL_ToString(ip) + "] - row doesn't have 2 columns, it has "
					+ UTIL_ToString(Row.size()));
			}

			mysql_free_result( Result );
		}
		else
			LOG_Failed_Query( Query, mysql_error( (MYSQL *)conn ) );
	}

	return From;
}

BanAddReturn BanAdd( void *conn, string playername, uint32_t serverID, uint32_t gameID )
{
	string EscName = MySQLEscapeString( conn, playername );
	transform( EscName.begin( ), EscName.end( ), EscName.begin( ), (int(*)(int))tolower );

	uint32_t PlayerID = 0;
	uint32_t PlayerDIV1StatsID = 0;
	uint32_t PlayerBanGroupID = 0;

	string PlayerSelectQuery = "SELECT new_player.id, new_player.ban_group_id, new_player.div1_stats_dota_id FROM new_player WHERE new_player.name = '" + EscName + "' AND new_player.server_id = " + UTIL_ToString( serverID ) + " LIMIT 1";

	if( mysql_real_query( (MYSQL *)conn, PlayerSelectQuery.c_str( ), PlayerSelectQuery.size( ) ) != 0 )
	{
		LOG_Failed_Query( PlayerSelectQuery, mysql_error( (MYSQL *)conn ) );
		return BanAddReturn( 0, (CDBBanGroup *)NULL );
	}
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 3 )
			{
				// this player has new_player record in the DB

				PlayerID = UTIL_ToUInt32( Row[0] );
				PlayerBanGroupID = UTIL_ToUInt32( Row[1] );
				PlayerDIV1StatsID = UTIL_ToUInt32(Row[2]);
			}
			else
			{
				// this player doesn't have new_player record

				string InsertPlayerQuery = "INSERT INTO new_player ( server_id, name ) VALUES ( " + UTIL_ToString( serverID ) + ", '" + EscName + "' )";

				if( mysql_real_query( (MYSQL *)conn, InsertPlayerQuery.c_str( ), InsertPlayerQuery.size( ) ) != 0 )
				{
					LOG_Failed_Query( InsertPlayerQuery, mysql_error( (MYSQL *)conn ) );
					return BanAddReturn( 0, (CDBBanGroup *)NULL );
				}
				else
				{
					PlayerID = mysql_insert_id( (MYSQL *)conn );

					if( !PlayerID )
					{
						DB_Log( "mysql_insert_id is zero, " + InsertPlayerQuery );
						return BanAddReturn( 0, (CDBBanGroup *)NULL );
					}
				}
			}

			mysql_free_result( Result );
		}
		else
		{
			LOG_Failed_Query( PlayerSelectQuery, mysql_error( (MYSQL *)conn ) );
			return BanAddReturn( 0, (CDBBanGroup *)NULL );
		}
	}

	uint32_t BanGroupDatetime = 0;
	uint32_t BanGroupDuration = 0;
	uint32_t BanGroupBanPoints = 0;
	uint32_t BanGroupWarnPoints = 0;

	uint32_t NumberOfGames = 0;

	/*CONSOLE_Print( "pa1" );
	CONSOLE_Print( "BanGroupDatetime = " + UTIL_ToString( BanGroupDatetime ) );
	CONSOLE_Print( "BanGroupDuration = " + UTIL_ToString( BanGroupDuration ) );
	CONSOLE_Print( "BanGroupBanPoints = " + UTIL_ToString( BanGroupBanPoints ) );
	CONSOLE_Print( "BanGroupWarnPoints = " + UTIL_ToString( BanGroupWarnPoints ) );*/

	if( PlayerBanGroupID )
	{
		string BanGroupSelectQuery = "SELECT new_ban_group.datetime, new_ban_group.duration, new_ban_group.ban_points, new_ban_group.warn_points FROM new_ban_group WHERE new_ban_group.id = " + UTIL_ToString( PlayerBanGroupID ) + " LIMIT 1";

		if( mysql_real_query( (MYSQL *)conn, BanGroupSelectQuery.c_str( ), BanGroupSelectQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( BanGroupSelectQuery, mysql_error( (MYSQL *)conn ) );
			return BanAddReturn( 0, (CDBBanGroup *)NULL );
		}
		else
		{
			MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

			if( Result )
			{
				vector<string> Row = MySQLFetchRow( Result );

				if( Row.size( ) == 4 )
				{
					// this player has new_ban_group record in the DB

					BanGroupDatetime = UTIL_ToUInt32( Row[0] );
					BanGroupDuration = UTIL_ToUInt32( Row[1] );
					BanGroupBanPoints = UTIL_ToUInt32( Row[2] );
					BanGroupWarnPoints = UTIL_ToUInt32( Row[3] );

					/*CONSOLE_Print( "pa2" );
					CONSOLE_Print( "BanGroupDatetime = " + UTIL_ToString( BanGroupDatetime ) );
					CONSOLE_Print( "BanGroupDuration = " + UTIL_ToString( BanGroupDuration ) );
					CONSOLE_Print( "BanGroupBanPoints = " + UTIL_ToString( BanGroupBanPoints ) );
					CONSOLE_Print( "BanGroupWarnPoints = " + UTIL_ToString( BanGroupWarnPoints ) );*/
				}
			}
		}
	}

	if (PlayerDIV1StatsID)
	{
		string DIV1StatsSelectQuery = "SELECT new_div1_stats_dota.games FROM new_div1_stats_dota WHERE new_div1_stats_dota.id = " + UTIL_ToString(PlayerDIV1StatsID) + " LIMIT 1";

		if (mysql_real_query((MYSQL*)conn, DIV1StatsSelectQuery.c_str(), DIV1StatsSelectQuery.size()) != 0)
		{
			//If player doesn't have any stats saved yet, skip this part
			LOG_Failed_Query(DIV1StatsSelectQuery, mysql_error((MYSQL*)conn));
		}
		else
		{
			MYSQL_RES* Result = mysql_store_result((MYSQL*)conn);

			if (Result)
			{
				vector<string> Row = MySQLFetchRow(Result);

				if (Row.size() == 1)
				{
					// this player has new_div1_stats_dota record in the DB

					NumberOfGames = UTIL_ToUInt32(Row[0]);
				}
			}
		}
	}

	CDBBanGroup *BanGroup = new CDBBanGroup( BanGroupDatetime, BanGroupDuration, BanGroupBanPoints, BanGroupWarnPoints );

	uint32_t AutoBanDuration = MASL_PROTOCOL::CalcAutoBanDuration( BanGroup->GetBanPoints( ), NumberOfGames );

	if( BanGroup->GetIsBanned( ) )
	{
		// this player is currently banned (don't overwrite datetime of current ban)

		BanGroup->SetDuration( BanGroup->GetDuration( ) + AutoBanDuration );

		/*CONSOLE_Print( "pa3" );
		CONSOLE_Print( "BanGroup->GetDatetime( ) = " + UTIL_ToString( BanGroup->GetDatetime( ) ) );
		CONSOLE_Print( "BanGroup->GetDuration( ) = " + UTIL_ToString( BanGroup->GetDuration( ) ) );
		CONSOLE_Print( "BanGroup->GetBanPoints( ) = " + UTIL_ToString( BanGroup->GetBanPoints( ) ) );
		CONSOLE_Print( "BanGroup->GetWarnPoints( ) = " + UTIL_ToString( BanGroup->GetWarnPoints( ) ) );*/
	}
	else
	{
		// this player is not currently banned

		BanGroup->SetDatetime( GetMySQLTime( ) );
		BanGroup->SetDuration( AutoBanDuration );

		/*CONSOLE_Print( "pa4" );
		CONSOLE_Print( "BanGroup->GetDatetime( ) = " + UTIL_ToString( BanGroup->GetDatetime( ) ) );
		CONSOLE_Print( "BanGroup->GetDuration( ) = " + UTIL_ToString( BanGroup->GetDuration( ) ) );
		CONSOLE_Print( "BanGroup->GetBanPoints( ) = " + UTIL_ToString( BanGroup->GetBanPoints( ) ) );
		CONSOLE_Print( "BanGroup->GetWarnPoints( ) = " + UTIL_ToString( BanGroup->GetWarnPoints( ) ) );*/
	}

	// add new ban points after you call calc_ban_duration() so only old points affect new ban duration

	BanGroup->SetBanPoints( BanGroup->GetBanPoints( ) + MASL_PROTOCOL :: DB_LEAVER_BAN_POINTS );

	/*CONSOLE_Print( "pa5" );
	CONSOLE_Print( "BanGroup->GetDatetime( ) = " + UTIL_ToString( BanGroup->GetDatetime( ) ) );
	CONSOLE_Print( "BanGroup->GetDuration( ) = " + UTIL_ToString( BanGroup->GetDuration( ) ) );
	CONSOLE_Print( "BanGroup->GetBanPoints( ) = " + UTIL_ToString( BanGroup->GetBanPoints( ) ) );
	CONSOLE_Print( "BanGroup->GetWarnPoints( ) = " + UTIL_ToString( BanGroup->GetWarnPoints( ) ) );*/

	// we calculated new ban duration, now update the DB

	if( PlayerBanGroupID )
	{
		// this player has new_ban_group record already so update it

		string UpdateBanGroupQuery = "UPDATE new_ban_group SET new_ban_group.datetime = " + UTIL_ToString( BanGroup->GetDatetime( ) ) + ", new_ban_group.duration = " + UTIL_ToString( BanGroup->GetDuration( ) ) + ", new_ban_group.ban_points = " + UTIL_ToString( BanGroup->GetBanPoints( ) ) + ", new_ban_group.warn_points = " + UTIL_ToString( BanGroup->GetWarnPoints( ) ) + ", new_ban_group.last_ban_datetime = UNIX_TIMESTAMP() WHERE new_ban_group.id = " + UTIL_ToString( PlayerBanGroupID ) + " LIMIT 1";

		if( mysql_real_query( (MYSQL *)conn, UpdateBanGroupQuery.c_str( ), UpdateBanGroupQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( UpdateBanGroupQuery, mysql_error( (MYSQL *)conn ) );
			return BanAddReturn( 0, (CDBBanGroup *)NULL );
		}
		else
		{
			uint32_t UpdateBanGroupQueryAffectedRows = mysql_affected_rows( (MYSQL *)conn );

			if( !UpdateBanGroupQueryAffectedRows )
			{
				DB_Log( "mysql_affected_rows is zero, " + UpdateBanGroupQuery );
				return BanAddReturn( 0, (CDBBanGroup *)NULL );
			}
		}
	}
	else
	{
		// this player doesn't have new_ban_group record so insert one

		string InsertBanGroupQuery = "INSERT INTO new_ban_group ( datetime, duration, ban_points, warn_points, last_ban_datetime ) VALUES ( " + UTIL_ToString( BanGroup->GetDatetime( ) ) + ", " + UTIL_ToString( BanGroup->GetDuration( ) ) + ", " + UTIL_ToString( BanGroup->GetBanPoints( ) ) + ", " + UTIL_ToString( BanGroup->GetWarnPoints( ) ) + ", UNIX_TIMESTAMP() )";

		if( mysql_real_query( (MYSQL *)conn, InsertBanGroupQuery.c_str( ), InsertBanGroupQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( InsertBanGroupQuery, mysql_error( (MYSQL *)conn ) );
			return BanAddReturn( 0, (CDBBanGroup *)NULL );
		}
		else
		{
			PlayerBanGroupID = mysql_insert_id( (MYSQL *)conn );

			if( !PlayerBanGroupID )
			{
				DB_Log( "mysql_insert_id is zero, " + InsertBanGroupQuery );
				return BanAddReturn( 0, (CDBBanGroup *)NULL );
			}
			else
			{
				// new_ban_group record inserted succesfully, update new_player record with ban_group_id

				string UpdatePlayerQuery = "UPDATE new_player SET new_player.ban_group_id = " + UTIL_ToString( PlayerBanGroupID ) + " WHERE new_player.id = " + UTIL_ToString( PlayerID ) + " LIMIT 1";

				if( mysql_real_query( (MYSQL *)conn, UpdatePlayerQuery.c_str( ), UpdatePlayerQuery.size( ) ) != 0 )
				{
					LOG_Failed_Query( UpdatePlayerQuery, mysql_error( (MYSQL *)conn ) );
					return BanAddReturn( 0, (CDBBanGroup *)NULL );
				}
				else
				{
					uint32_t UpdatePlayerQueryAffectedRows = mysql_affected_rows( (MYSQL *)conn );

					if( !UpdatePlayerQueryAffectedRows )
					{
						DB_Log( "mysql_affected_rows is zero, " + UpdatePlayerQuery );
						return BanAddReturn( 0, (CDBBanGroup *)NULL );
					}
				}
			}
		}
	}

	// make a record of this autoban in new_log

	string InsertBanQuery = "INSERT INTO new_log ( ban_group_id, victim_player_id, game_id, datetime, type, points, duration ) VALUES ( " + UTIL_ToString( PlayerBanGroupID ) + ", " + UTIL_ToString( PlayerID ) + ", " + UTIL_ToString( gameID ) + ", UNIX_TIMESTAMP(), " + UTIL_ToString( MASL_PROTOCOL :: DB_GAME_BAN ) + ", " + UTIL_ToString( MASL_PROTOCOL :: DB_LEAVER_BAN_POINTS ) + ", " + UTIL_ToString( AutoBanDuration ) + " )";

	if( mysql_real_query( (MYSQL *)conn, InsertBanQuery.c_str( ), InsertBanQuery.size( ) ) != 0 )
	{
		LOG_Failed_Query( InsertBanQuery, mysql_error( (MYSQL *)conn ) );
		return BanAddReturn( 0, (CDBBanGroup *)NULL );
	}
	else
	{
		if( !mysql_insert_id( (MYSQL *)conn ) )
		{
			DB_Log( "mysql_insert_id is zero, " + InsertBanQuery );
			return BanAddReturn( 0, (CDBBanGroup *)NULL );
		}
	}

	return BanAddReturn( PlayerBanGroupID, BanGroup );
}

BanAddReturn DIV1BanAdd( void *conn, string adminplayername, uint32_t adminserverID, string victimplayername, uint32_t victimserverID, uint32_t gameID, string reason )
{
	string AdminEscName = MySQLEscapeString( conn, adminplayername );
	transform( AdminEscName.begin( ), AdminEscName.end( ), AdminEscName.begin( ), (int(*)(int))tolower );

	string VictimEscName = MySQLEscapeString( conn, victimplayername );
	transform( VictimEscName.begin( ), VictimEscName.end( ), VictimEscName.begin( ), (int(*)(int))tolower );

	uint32_t AdminPlayerID = 0;
	uint32_t VictimPlayerID = 0;
	uint32_t VictimPlayerBanGroupID = 0;

	// select or insert admin player

	string AdminPlayerSelectQuery = "SELECT new_player.id FROM new_player WHERE new_player.name = '" + AdminEscName + "' AND new_player.server_id = " + UTIL_ToString( adminserverID ) + " LIMIT 1";

	if( mysql_real_query( (MYSQL *)conn, AdminPlayerSelectQuery.c_str( ), AdminPlayerSelectQuery.size( ) ) != 0 )
	{
		LOG_Failed_Query( AdminPlayerSelectQuery, mysql_error( (MYSQL *)conn ) );
		return BanAddReturn( 0, (CDBBanGroup *)NULL );
	}
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
			{
				// this player has new_player record in the DB

				AdminPlayerID = UTIL_ToUInt32( Row[0] );
			}
			else
			{
				// this player doesn't have new_player record in the DB, insert one

				string InsertAdminPlayerQuery = "INSERT INTO new_player ( server_id, name ) VALUES ( " + UTIL_ToString( adminserverID ) + ", '" + AdminEscName + "' )";

				if( mysql_real_query( (MYSQL *)conn, InsertAdminPlayerQuery.c_str( ), InsertAdminPlayerQuery.size( ) ) != 0 )
				{
					LOG_Failed_Query( InsertAdminPlayerQuery, mysql_error( (MYSQL *)conn ) );
					return BanAddReturn( 0, (CDBBanGroup *)NULL );
				}
				else
				{
					AdminPlayerID = mysql_insert_id( (MYSQL *)conn );

					if( AdminPlayerID == 0 )
					{
						DB_Log( "mysql_insert_id is zero, " + InsertAdminPlayerQuery );
						return BanAddReturn( 0, (CDBBanGroup *)NULL );
					}
				}
			}

			mysql_free_result( Result );
		}
		else
		{
			LOG_Failed_Query( AdminPlayerSelectQuery, mysql_error( (MYSQL *)conn ) );
			return BanAddReturn( 0, (CDBBanGroup *)NULL );
		}
	}

	// select or insert victim player

	string VictimPlayerSelectQuery = "SELECT new_player.id, new_player.ban_group_id FROM new_player WHERE new_player.name = '" + VictimEscName + "' AND new_player.server_id = " + UTIL_ToString( victimserverID ) + " LIMIT 1";

	if( mysql_real_query( (MYSQL *)conn, VictimPlayerSelectQuery.c_str( ), VictimPlayerSelectQuery.size( ) ) != 0 )
	{
		LOG_Failed_Query( VictimPlayerSelectQuery, mysql_error( (MYSQL *)conn ) );
		return BanAddReturn( 0, (CDBBanGroup *)NULL );
	}
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 2 )
			{
				// this player has new_player record in the DB

				VictimPlayerID = UTIL_ToUInt32( Row[0] );
				VictimPlayerBanGroupID = UTIL_ToUInt32( Row[1] );
			}
			else
			{
				// this player doesn't have new_player record

				string InsertVictimPlayerQuery = "INSERT INTO new_player ( server_id, name ) VALUES ( " + UTIL_ToString( victimserverID ) + ", '" + VictimEscName + "' )";

				if( mysql_real_query( (MYSQL *)conn, InsertVictimPlayerQuery.c_str( ), InsertVictimPlayerQuery.size( ) ) != 0 )
				{
					LOG_Failed_Query( InsertVictimPlayerQuery, mysql_error( (MYSQL *)conn ) );
					return BanAddReturn( 0, (CDBBanGroup *)NULL );
				}
				else
				{
					VictimPlayerID = mysql_insert_id( (MYSQL *)conn );

					if( VictimPlayerID == 0 )
					{
						DB_Log( "mysql_insert_id is zero, " + InsertVictimPlayerQuery );
						return BanAddReturn( 0, (CDBBanGroup *)NULL );
					}
				}
			}

			mysql_free_result( Result );
		}
		else
		{
			LOG_Failed_Query( VictimPlayerSelectQuery, mysql_error( (MYSQL *)conn ) );
			return BanAddReturn( 0, (CDBBanGroup *)NULL );
		}
	}

	uint32_t VictimBanGroupDatetime = 0;
	uint32_t VictimBanGroupDuration = 0;
	uint32_t VictimBanGroupBanPoints = 0;
	uint32_t VictimBanGroupWarnPoints = 0;

	if( VictimPlayerBanGroupID )
	{
		string VictimBanGroupSelectQuery = "SELECT new_ban_group.datetime, new_ban_group.duration, new_ban_group.ban_points, new_ban_group.warn_points FROM new_ban_group WHERE new_ban_group.id = " + UTIL_ToString( VictimPlayerBanGroupID ) + " LIMIT 1";

		if( mysql_real_query( (MYSQL *)conn, VictimBanGroupSelectQuery.c_str( ), VictimBanGroupSelectQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( VictimBanGroupSelectQuery, mysql_error( (MYSQL *)conn ) );
			return BanAddReturn( 0, (CDBBanGroup *)NULL );
		}
		else
		{
			MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

			if( Result )
			{
				vector<string> Row = MySQLFetchRow( Result );

				if( Row.size( ) == 4 )
				{
					// this player has new_ban_group record in the DB

					VictimBanGroupDatetime = UTIL_ToUInt32( Row[0] );
					VictimBanGroupDuration = UTIL_ToUInt32( Row[1] );
					VictimBanGroupBanPoints = UTIL_ToUInt32( Row[2] );
					VictimBanGroupWarnPoints = UTIL_ToUInt32( Row[3] );
				}
			}
		}
	}

	CDBBanGroup *VictimBanGroup = new CDBBanGroup( VictimBanGroupDatetime, VictimBanGroupDuration, VictimBanGroupBanPoints, VictimBanGroupWarnPoints );

	//uint32_t AutoBanDuration = MASL_PROTOCOL :: CalcBanDuration( MASL_PROTOCOL :: DB_LEAVER_BAN_DURATION, BanGroup->GetBanPoints( ) );
	uint32_t AutoBanDuration = 604800;

	if( VictimBanGroup->GetIsBanned( ) )
	{
		// this player is currently banned (don't overwrite datetime of current ban)

		VictimBanGroup->SetDuration( VictimBanGroup->GetDuration( ) + AutoBanDuration );
	}
	else
	{
		// this player is not currently banned

		VictimBanGroup->SetDatetime( GetMySQLTime( ) );
		VictimBanGroup->SetDuration( AutoBanDuration );
	}

	// add new ban points after you call calc_ban_duration() so only old points affect new ban duration

	//BanGroup->SetBanPoints( BanGroup->GetBanPoints( ) + MASL_PROTOCOL :: DB_LEAVER_BAN_POINTS );

	// we calculated new ban duration, now update the DB

	if( VictimPlayerBanGroupID )
	{
		// this player has new_ban_group record already so update it

		string UpdateVictimBanGroupQuery = "UPDATE new_ban_group SET new_ban_group.datetime = " + UTIL_ToString( VictimBanGroup->GetDatetime( ) ) + ", new_ban_group.duration = " + UTIL_ToString( VictimBanGroup->GetDuration( ) ) + ", new_ban_group.ban_points = " + UTIL_ToString( VictimBanGroup->GetBanPoints( ) ) + ", new_ban_group.warn_points = " + UTIL_ToString( VictimBanGroup->GetWarnPoints( ) ) + ", new_ban_group.last_ban_datetime = UNIX_TIMESTAMP() WHERE new_ban_group.id = " + UTIL_ToString( VictimPlayerBanGroupID ) + " LIMIT 1";

		if( mysql_real_query( (MYSQL *)conn, UpdateVictimBanGroupQuery.c_str( ), UpdateVictimBanGroupQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( UpdateVictimBanGroupQuery, mysql_error( (MYSQL *)conn ) );
			return BanAddReturn( 0, (CDBBanGroup *)NULL );
		}
		else
		{
			uint32_t UpdateVictimBanGroupQueryAffectedRows = mysql_affected_rows( (MYSQL *)conn );

			if( UpdateVictimBanGroupQueryAffectedRows == 0 )
			{
				DB_Log( "mysql_affected_rows is zero, " + UpdateVictimBanGroupQueryAffectedRows );
				return BanAddReturn( 0, (CDBBanGroup *)NULL );
			}
		}
	}
	else
	{
		// this player doesn't have new_ban_group record so insert one

		string InsertVictimBanGroupQuery = "INSERT INTO new_ban_group ( datetime, duration, ban_points, warn_points, last_ban_datetime ) VALUES ( " + UTIL_ToString( VictimBanGroup->GetDatetime( ) ) + ", " + UTIL_ToString( VictimBanGroup->GetDuration( ) ) + ", " + UTIL_ToString( VictimBanGroup->GetBanPoints( ) ) + ", " + UTIL_ToString( VictimBanGroup->GetWarnPoints( ) ) + ", UNIX_TIMESTAMP() )";

		if( mysql_real_query( (MYSQL *)conn, InsertVictimBanGroupQuery.c_str( ), InsertVictimBanGroupQuery.size( ) ) != 0 )
		{
			LOG_Failed_Query( InsertVictimBanGroupQuery, mysql_error( (MYSQL *)conn ) );
			return BanAddReturn( 0, (CDBBanGroup *)NULL );
		}
		else
		{
			VictimPlayerBanGroupID = mysql_insert_id( (MYSQL *)conn );

			if( VictimPlayerBanGroupID == 0 )
			{
				DB_Log( "mysql_insert_id is zero, " + InsertVictimBanGroupQuery );
				return BanAddReturn( 0, (CDBBanGroup *)NULL );
			}
			else
			{
				// new_ban_group record inserted succesfully, update new_player record with ban_group_id

				string UpdateVictimPlayerQuery = "UPDATE new_player SET new_player.ban_group_id = " + UTIL_ToString( VictimPlayerBanGroupID ) + " WHERE new_player.id = " + UTIL_ToString( VictimPlayerID ) + " LIMIT 1";

				if( mysql_real_query( (MYSQL *)conn, UpdateVictimPlayerQuery.c_str( ), UpdateVictimPlayerQuery.size( ) ) != 0 )
				{
					LOG_Failed_Query( UpdateVictimPlayerQuery, mysql_error( (MYSQL *)conn ) );
					return BanAddReturn( 0, (CDBBanGroup *)NULL );
				}
				else
				{
					uint32_t UpdateVictimPlayerQueryAffectedRows = mysql_affected_rows( (MYSQL *)conn );

					if( UpdateVictimPlayerQueryAffectedRows == 0 )
					{
						DB_Log( "mysql_affected_rows is zero, " + UpdateVictimPlayerQuery );
						return BanAddReturn( 0, (CDBBanGroup *)NULL );
					}
				}
			}
		}
	}

	// make a record of this ban in new_log

	//string InsertVictimBanQuery = "INSERT INTO new_log ( ban_group_id, victim_player_id, game_id, datetime, type, points, duration ) VALUES ( " + UTIL_ToString( VictimPlayerBanGroupID ) + ", " + UTIL_ToString( VictimPlayerID ) + ", " + UTIL_ToString( gameID ) + ", UNIX_TIMESTAMP( ), " + UTIL_ToString( MASL_PROTOCOL :: DB_GAME_BAN ) + ", " + UTIL_ToString( MASL_PROTOCOL :: DB_LEAVER_BAN_POINTS ) + ", " + UTIL_ToString( AutoBanDuration ) + " )";
	string InsertVictimBanQuery = "INSERT INTO new_log ( ban_group_id, victim_player_id, admin_player_id, game_id, datetime, type, duration ) VALUES ( " + UTIL_ToString( VictimPlayerBanGroupID ) + ", " + UTIL_ToString( VictimPlayerID ) + ", " + UTIL_ToString( AdminPlayerID ) + ", " + UTIL_ToString( gameID ) + ", UNIX_TIMESTAMP(), " + UTIL_ToString( MASL_PROTOCOL :: DB_GAME_BAN ) + ", " + UTIL_ToString( AutoBanDuration ) + " )";

	if( mysql_real_query( (MYSQL *)conn, InsertVictimBanQuery.c_str( ), InsertVictimBanQuery.size( ) ) != 0 )
	{
		LOG_Failed_Query( InsertVictimBanQuery, mysql_error( (MYSQL *)conn ) );
		return BanAddReturn( 0, (CDBBanGroup *)NULL );
	}
	else
	{
		if( !mysql_insert_id( (MYSQL *)conn ) )
		{
			DB_Log( "mysql_insert_id is zero, " + InsertVictimBanQuery );
			return BanAddReturn( 0, (CDBBanGroup *)NULL );
		}
	}

	return BanAddReturn( VictimPlayerBanGroupID, VictimBanGroup );
}

uint32_t GameID( void *conn, std::string region )
{
	uint32_t GameID = 0;

	// save game

	string InsertGameQuery = "INSERT INTO new_game ( game_in_progress, bot_region ) VALUES ( 1, '"+MySQLEscapeString(conn, region)+"' )";

	if( mysql_real_query( (MYSQL *)conn, InsertGameQuery.c_str( ), InsertGameQuery.size( ) ) != 0 )
	{
		LOG_Failed_Query( InsertGameQuery, mysql_error( (MYSQL *)conn ) );
		return 0;
	}
	else
	{
		GameID = mysql_insert_id( (MYSQL *)conn );

		if( !GameID )
		{
			DB_Log( "mysql_insert_id is zero, " + InsertGameQuery );
			return 0;
		}
	}

	return GameID;
}

CDBCache *CacheList( void *conn, set<uint32_t> server )
{
	vector<CDBBanGroup *> BanGroupList;
	//string UserQuery = "SELECT new_player.name, new_country.id, new_country.name, new_user.id, new_user.ban_datetime, new_user.ban_reason, new_user.ban_duration, new_user.ban_type FROM new_user LEFT JOIN new_player ON new_user.ban_admin_id = new_player.id LEFT JOIN new_country ON new_user.country_id = new_country.id";
	string BanGroupQuery = "SELECT id, datetime, duration, ban_points, warn_points FROM new_ban_group ORDER BY id ASC";

	if( mysql_real_query( (MYSQL *)conn, BanGroupQuery.c_str( ), BanGroupQuery.size( ) ) != 0 )
		LOG_Failed_Query( BanGroupQuery, mysql_error( (MYSQL *)conn ) );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			// index of vector element MUST match MySQL ID from the record
			// there is no record with MySQL ID value of zero so make first vector element NULL pointer

			BanGroupList.push_back( NULL );
			uint32_t i = 0;

			while( Row.size( ) == 5 )
			{
				++i;

				// index of vector element MUST match MySQL ID from the record

				if( UTIL_ToUInt32( Row[0] ) != i )
				{
					BanGroupList.push_back( NULL );
					continue;
				}

				BanGroupList.push_back(
					new CDBBanGroup(
						UTIL_ToUInt32( Row[1] ),		// Datetime
						UTIL_ToUInt32( Row[2] ),		// Duration
						UTIL_ToUInt32( Row[3] ),		// Ban Points
						UTIL_ToUInt32( Row[4] )			// Warn Points
					)
				);

				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			LOG_Failed_Query( BanGroupQuery, mysql_error( (MYSQL *)conn ) );
	}

	vector<CDBDotAPlayerSummary *> DPSList;
	string DPSQuery = "SELECT id, rating, highest_rating, games, wins, loses, kills, deaths, creepkills, creepdenies, assists, neutralkills, games_observed FROM new_div1_stats_dota ORDER BY id ASC";

	if( mysql_real_query( (MYSQL *)conn, DPSQuery.c_str( ), DPSQuery.size( ) ) != 0 )
		LOG_Failed_Query( DPSQuery, mysql_error( (MYSQL *)conn ) );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			// index of vector element MUST match user.id from the record
			// there is no record with user.id value of zero so make first element of vector NULL pointer

			DPSList.push_back( NULL );
			uint32_t i = 0;

			while( Row.size( ) == 13 )
			{
				++i;

				// index of vector element MUST match user.id from the record

				if( UTIL_ToUInt32( Row[0] ) != i )
				{
					DPSList.push_back( NULL );
					continue;
				}

				DPSList.push_back( new CDBDotAPlayerSummary(
					UTIL_ToDouble( Row[1] ),							// Rating
					UTIL_ToDouble( Row[2] ),							// Highest Rating
					UTIL_ToUInt32( Row[3] ),							// Games
					UTIL_ToUInt32( Row[4] ),							// Wins
					UTIL_ToUInt32( Row[5] ),							// Loses
					UTIL_ToUInt32( Row[6] ),							// Kills
					UTIL_ToUInt32( Row[7] ),							// Deaths
					UTIL_ToUInt32( Row[8] ),							// Creep Kills
					UTIL_ToUInt32( Row[9] ),							// Creep Denies
					UTIL_ToUInt32( Row[10] ),							// Assists
					UTIL_ToUInt32( Row[11] ),							// Neutral Kills
					UTIL_ToUInt32( Row[12] )							// Games Observed
					) );

				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			LOG_Failed_Query( DPSQuery, mysql_error( (MYSQL *)conn ) );
	}

	list<pair<uint32_t, std::unordered_map<string, CDBPlayer *> > > PlayerLists;

	for( set<uint32_t> :: iterator i = server.begin( ); i != server.end( ); ++i )
	{
		std::unordered_map<string, CDBPlayer *> PlayerList;
		string PlayerQuery = "SELECT name, ban_group_id, div1_stats_dota_id, access_level FROM new_player WHERE server_id=" + UTIL_ToString( *i );

		if( mysql_real_query( (MYSQL *)conn, PlayerQuery.c_str( ), PlayerQuery.size( ) ) != 0 )
			LOG_Failed_Query( PlayerQuery, mysql_error( (MYSQL *)conn ) );
		else
		{
			MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

			if( Result )
			{
				vector<string> Row = MySQLFetchRow( Result );

				while( Row.size( ) == 4 )
				{
					PlayerList.insert( make_pair( Row[0], new CDBPlayer( UTIL_ToUInt32( Row[1] ), UTIL_ToUInt32( Row[2] ), UTIL_ToUInt32( Row[3] ) ) ) );
					Row = MySQLFetchRow( Result );
				}

				mysql_free_result( Result );
			}
			else
				LOG_Failed_Query( PlayerQuery, mysql_error( (MYSQL *)conn ) );
		}

		PlayerLists.push_back( make_pair( *i, PlayerList ) );
	}

	return new CDBCache( PlayerLists, BanGroupList, DPSList );
}

list<string> ContributorList( void *conn, uint32_t server )
{
	list<string> ContributorList;
	string EscServerID = MySQLEscapeString( conn, UTIL_ToString( server ) );

	string ContributorListSelectQuery = "SELECT LOWER(name) FROM new_contributor WHERE server_id = " + EscServerID;

	if( mysql_real_query( (MYSQL *)conn, ContributorListSelectQuery.c_str( ), ContributorListSelectQuery.size( ) ) != 0 )
		LOG_Failed_Query( ContributorListSelectQuery, mysql_error( (MYSQL *)conn ) );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( Row.size( ) == 1 )
			{
				ContributorList.push_back( Row[0] );
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			LOG_Failed_Query( ContributorListSelectQuery, mysql_error( (MYSQL *)conn ) );
	}

	return ContributorList;
}

uint32_t UnixTimestampGet( void *conn )
{
	uint32_t Timestamp = 0;
	string Query = "SELECT UNIX_TIMESTAMP( )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		LOG_Failed_Query( Query, mysql_error( (MYSQL *)conn ) );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
				Timestamp = UTIL_ToUInt32(Row[0]);
			/* else
				*error = "error checking score [" + category + " : " + name + " : " + server + "] - row doesn't have 1 column"; */

			mysql_free_result( Result );
		}
		else
			LOG_Failed_Query( Query, mysql_error( (MYSQL *)conn ) );
	}

	return Timestamp;
}

//
// CDBCache
//

std::unordered_map<string, CDBPlayer *> CDBCache :: GetPlayerList( uint32_t ServerID )
{
	for( list<pair<uint32_t, std::unordered_map<string, CDBPlayer *> > > :: iterator i = m_PlayerList.begin( ); i != m_PlayerList.end( ); ++i )
	{
		if( (*i).first == ServerID )
			return (*i).second;
	}

	return std::unordered_map<string, CDBPlayer *>( );
}

//
// CDBPlayer
//

CDBGamePlayerSummary *CDBPlayer :: GetGamePlayerSummary( )
{
	return NULL;
}

CDBDotAPlayerSummary *CDBPlayer :: GetDotAPlayerSummary( )
{
	if( gGHost->m_DotAPlayerSummary.size( ) > m_StatsDotAID )
		return gGHost->m_DotAPlayerSummary[m_StatsDotAID];

	return NULL;
}

CDBBanGroup *CDBPlayer :: GetBanGroup( )
{
	//CONSOLE_Print("CDBBanGroup *CDBPlayer :: GetBanGroup( )");
	//CONSOLE_Print("m_BanGroupID = " + UTIL_ToString( m_BanGroupID ));

	if( gGHost->m_BanGroup.size( ) > m_BanGroupID )
		return gGHost->m_BanGroup[m_BanGroupID];

	return NULL;
}

bool CDBPlayer :: GetIsBanned( )
{
	//CONSOLE_Print("bool CDBPlayer :: GetIsBanned( )");

	CDBBanGroup *BanGroup = GetBanGroup( );

	if( BanGroup )
	{
		/*CONSOLE_Print("BanGroup->GetDatetime( ) = " + UTIL_ToString( BanGroup->GetDatetime( ) ));
		CONSOLE_Print("BanGroup->GetDuration( ) = " + UTIL_ToString( BanGroup->GetDuration( ) ));
		CONSOLE_Print("BanGroup->GetBanPoints( ) = " + UTIL_ToString( BanGroup->GetBanPoints( ) ));
		CONSOLE_Print("BanGroup->GetWarnPoints( ) = " + UTIL_ToString( BanGroup->GetWarnPoints( ) ));*/

		return BanGroup->GetIsBanned( );
	}

	return false;
}

//
// CDBBanGroup
//

bool CDBBanGroup :: GetIsBanned( )
{
	/*CONSOLE_Print("bool CDBBanGroup :: GetIsBanned( )");
	CONSOLE_Print("m_Datetime = " + UTIL_ToString( m_Datetime ));
	CONSOLE_Print("m_Duration = " + UTIL_ToString( m_Duration ));
	CONSOLE_Print("m_BanPoints = " + UTIL_ToString( m_BanPoints ));
	CONSOLE_Print("m_WarnPoints = " + UTIL_ToString( m_WarnPoints ));*/

	if( !m_Datetime )
		return false;

	if( !m_Duration )
		return true;	// perma ban

	if( m_Datetime + m_Duration < GetMySQLTime( ) )
		return false;	// ban expired
	else
		return true;	// ban not expired
}

//
// CDBDotAPlayer
//

CDBDotAPlayer :: CDBDotAPlayer( uint32_t nColour, uint32_t nLevel, uint32_t nKills, uint32_t nDeaths, uint32_t nCreepKills, uint32_t nCreepDenies, uint32_t nAssists, uint32_t nGold, uint32_t nNeutralKills, string nItem1, string nItem2, string nItem3, string nItem4, string nItem5, string nItem6, string nHero, uint32_t nNewColour, uint32_t nTowerKills, uint32_t nRaxKills, uint32_t nCourierKills )
	: m_Colour( nColour ), m_Level( nLevel ), m_Kills( nKills ), m_Deaths( nDeaths ), m_CreepKills( nCreepKills ), m_CreepDenies( nCreepDenies ), m_Assists( nAssists ), m_Gold( nGold ), m_NeutralKills( nNeutralKills ), m_Hero( nHero ), m_NewColour( nNewColour ), m_TowerKills( nTowerKills ), m_RaxKills( nRaxKills ), m_CourierKills( nCourierKills )
{
	m_Items[0] = nItem1;
	m_Items[1] = nItem2;
	m_Items[2] = nItem3;
	m_Items[3] = nItem4;
	m_Items[4] = nItem5;
	m_Items[5] = nItem6;
}

CDBDotAPlayer :: ~CDBDotAPlayer( )
{

}

string CDBDotAPlayer :: GetItem( unsigned int i )
{
	if( i < 6 )
		return m_Items[i];

	return string( );
}

void CDBDotAPlayer :: SetItem( unsigned int i, string item )
{
	if( i < 6 )
		m_Items[i] = item;
}
