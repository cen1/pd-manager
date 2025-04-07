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

extern CGHost *gGHost;

//
// CGHostDB
//

#include <boost/thread.hpp>
#include <unordered_map>

class CBaseCallable;
class CCallableFromCheck;
class CCallableGameCustomAdd;
class CCallableGameCustomDotAAdd;
class CCallableGameDiv1DotAAdd;
class CCallableGameID;
class CCallableCacheList;
class CCallableContributorList;
class CCallableUnixTimestampGet;
class CDBGame;
class CDBGamePlayerSummary;
class CDBDotAPlayerSummary;
class CDBPlayer;
class CDBPlayer2;
class CDBGamePlayer;
class CDBDotAPlayer;
class CDBDiv1DotAPlayer;

typedef pair<string,string> FromCodes;
typedef pair<uint32_t,list<CDBPlayer2 *> > GameAddReturn;
typedef pair<uint32_t,CDBBanGroup *> BanAddReturn;

class CGHostDB
{
private:
	volatile bool m_HasError;

	string m_Server;
	string m_Database;
	string m_User;
	string m_Password;
	uint32_t m_Port;

	boost::mutex m_JobQueueMutex;
	queue<CBaseCallable *> m_JobQueue;
	queue<CBaseCallable *> m_TempJobQueue;

public:
	CGHostDB( CConfig *CFG );
	~CGHostDB( );

	bool HasError( )			{ return m_HasError; }

	void Update( );
	void WorkerThread( );
	void QueueCallable( CBaseCallable *callable );

	// threaded database functions

	CCallableFromCheck *ThreadedFromCheck( uint32_t slavebotID, string name, uint32_t serverID, uint32_t ip );
	CCallableGameCustomAdd *ThreadedGameCustomAdd( uint32_t mysqlgameid, uint32_t lobbyduration, string lobbylog, string gamelog, string replayname, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, uint32_t creatorserverid, vector<CDBGamePlayer *> gameplayers, bool rmk, uint32_t gametype );
	CCallableGameCustomDotAAdd *ThreadedGameCustomDotAAdd( uint32_t mysqlgameid, uint32_t lobbyduration, string lobbylog, string gamelog, string replayname, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, uint32_t creatorserverid, vector<CDBGamePlayer *> gameplayers, bool rmk, uint32_t gametype, uint32_t winner, uint32_t creepsspawnedtime, uint32_t collectdotastatsovertime, uint32_t min, uint32_t sec, vector<CDBDotAPlayer *> dbdotaplayers, string mode1, string mode2 );
	CCallableGameDiv1DotAAdd *ThreadedGameDiv1DotAAdd( uint32_t mysqlgameid, uint32_t lobbyduration, string lobbylog, string gamelog, string replayname, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, uint32_t creatorserverid, vector<CDBGamePlayer *> gameplayers, bool rmk, uint32_t gametype, uint32_t winner, uint32_t creepsspawnedtime, uint32_t collectdotastatsovertime, uint32_t min, uint32_t sec, vector<CDBDotAPlayer *> dbdotaplayers, string mode1, string mode2, bool ff, bool scored, vector<CDBDiv1DotAPlayer *> dbdiv1dotaplayers );
	CCallableBanAdd *ThreadedBanAdd( string player, uint32_t serverID, uint32_t gameID );
	CCallableDIV1BanAdd *ThreadedDIV1BanAdd( string adminPlayerName, uint32_t adminServerID, string victimPlayerName, uint32_t victimServerID, uint32_t gameID, string reason );
	CCallableGameID *ThreadedGameID( uint32_t slaveID, uint32_t slaveGameID );
	CCallableCacheList *ThreadedCacheList( set<uint32_t> server );
	CCallableContributorList *ThreadedContributorList( uint32_t server );
	CCallableUnixTimestampGet *ThreadedUnixTimestampGet( );
};

//
// global helper functions
//

void DB_Log( string message );
void LOG_Failed_Query( string query, string error );

CDBCache *CacheList( void *conn, set<uint32_t> server );
list<string> ContributorList( void *conn, uint32_t server );
uint32_t UnixTimestampGet( void *conn );
FromCodes FromCheck( void *conn, uint32_t ip );
BanAddReturn BanAdd( void *conn, string playername, uint32_t serverID, uint32_t gameID );
BanAddReturn DIV1BanAdd( void *conn, string adminplayername, uint32_t adminserverID, string victimplayername, uint32_t victimserverID, uint32_t gameID, string reason );
uint32_t GameID( void *conn );

//
// Callables
//

// life cycle of a callable:
//  - the callable is created in one of the database's ThreadedXXX functions
//  - initially the callable is NOT ready (i.e. m_Ready = false)
//  - the ThreadedXXX function normally creates a thread to perform some query and (potentially) store some result in the callable
//  - at the time of this writing all threads are immediately detached, the code does not join any threads (the callable's "readiness" is used for this purpose instead)
//  - when the thread completes it will set m_Ready = true
//  - DO NOT DO *ANYTHING* TO THE CALLABLE UNTIL IT'S READY OR YOU WILL CREATE A CONCURRENCY MESS
//  - THE ONLY SAFE FUNCTION IN THE CALLABLE IS GetReady
//  - when the callable is ready you may access the callable's result which will have been set within the (now terminated) thread

// example usage:
//  - normally you will call a ThreadedXXX function, store the callable in a vector, and periodically check if the callable is ready
//  - when the callable is ready you will consume the result then you will pass the callable back to the database via the RecoverCallable function
//  - the RecoverCallable function allows the database to recover some of the callable's resources to be reused later (e.g. MySQL connections)
//  - note that this will NOT free the callable's memory, you must do that yourself after calling the RecoverCallable function
//  - be careful not to leak any callables, it's NOT safe to delete a callable even if you decide that you don't want the result anymore
//  - you should deliver any to-be-orphaned callables to the main vector in CGHost so they can be properly deleted when ready even if you don't care about the result anymore
//  - e.g. if a player does a stats check immediately before a game is deleted you can't just delete the callable on game deletion unless it's ready

class CBaseCallable
{
protected:
	volatile bool m_Ready;
	uint32_t m_StartTicks;
	uint32_t m_EndTicks;

public:
	CBaseCallable( ) : m_Ready( false ), m_StartTicks( 0 ), m_EndTicks( 0 ) { }
	virtual ~CBaseCallable( ) { }

	//virtual void operator( )( void *conn ) { }

	virtual void ThreadedJob( void *conn ) { }

	virtual bool GetReady( )				{ return m_Ready; }
	virtual void SetReady( bool nReady )	{ m_Ready = nReady; }
	virtual uint32_t GetElapsed( )			{ return m_Ready ? m_EndTicks - m_StartTicks : 0; }
};

class CCallableFromCheck : virtual public CBaseCallable
{
protected:
	string m_Name;
	uint32_t m_SlaveBotID;
	uint32_t m_ServerID;
	uint32_t m_IP;
	FromCodes m_Result;

public:
	CCallableFromCheck( uint32_t nSlaveBotID, string nName, uint32_t nServerID, uint32_t nIP ) : CBaseCallable( ), m_SlaveBotID( nSlaveBotID ), m_Name( nName ), m_ServerID( nServerID ), m_IP( nIP ), m_Result( FromCodes( "??", "??" ) ) { }
	virtual ~CCallableFromCheck( ) { }

	virtual void ThreadedJob( void *conn );

	string GetName( )							{ return m_Name; }
	uint32_t GetSlaveBotID( )					{ return m_SlaveBotID; }
	uint32_t GetServerID( )						{ return m_ServerID; }
	uint32_t GetIP( )							{ return m_IP; }
	virtual FromCodes GetResult( )				{ return m_Result; }
	virtual void SetResult( FromCodes nResult )	{ m_Result = nResult; }
};

class CCallableBanCheck : virtual public CBaseCallable
{
protected:
	string m_Server;
	string m_User;
	CDBBan *m_Result;

public:
	CCallableBanCheck( string nServer, string nUser ) : CBaseCallable( ), m_Server( nServer ), m_User( nUser ), m_Result( NULL ) { }
	virtual ~CCallableBanCheck( ) { }

	virtual void ThreadedJob( void *conn );

	virtual string GetServer( )					{ return m_Server; }
	virtual string GetUser( )					{ return m_User; }
	virtual CDBBan *GetResult( )				{ return m_Result; }
	virtual void SetResult( CDBBan *nResult )	{ m_Result = nResult; }
};

class CCallableGameCustomAdd : virtual public CBaseCallable
{
public:
	bool m_Error;

	vector<CDBGamePlayer *> m_DBGamePlayers;
	list<CDBPlayer2 *> m_DBPlayers2;
	string m_LobbyLog;
	string m_GameLog;
	string m_ReplayName;
	string m_Map;
	string m_GameName;
	string m_OwnerName;
	string m_CreatorName;
	uint32_t m_CreatorServerID;
	uint32_t m_DBGameID;
	uint32_t m_DBCreatorPlayerID;
	uint32_t m_LobbyDuration;
	uint32_t m_Duration;
	uint32_t m_GameState;
	uint32_t m_GameType;
	bool m_RMK;

public:
	CCallableGameCustomAdd( uint32_t nDBGameID, uint32_t nLobbyDuration, string nLobbyLog, string nGameLog, string nReplayName, string nMap, string nGameName, string nOwnerName, uint32_t nDuration, uint32_t nGameState, string nCreatorName, uint32_t nCreatorServerID, vector<CDBGamePlayer *> nDBGamePlayers, bool nRMK, uint32_t nGameType ) 
		: CBaseCallable( ), m_Error( false ), m_DBGameID( nDBGameID ), m_DBCreatorPlayerID( 0 ), m_LobbyDuration( nLobbyDuration ), m_LobbyLog( nLobbyLog ), m_GameLog( nGameLog ), m_ReplayName( nReplayName ), m_Map( nMap ), m_GameName( nGameName ), m_OwnerName( nOwnerName ), m_Duration( nDuration ), m_GameState( nGameState ), m_CreatorName( nCreatorName ), m_CreatorServerID( nCreatorServerID ), m_DBGamePlayers( nDBGamePlayers ), m_RMK( nRMK ), m_GameType( nGameType ) { }
	virtual ~CCallableGameCustomAdd( ) { }

	virtual void ThreadedJob( void *conn );
	void ThreadedJob2( void *conn );
};

class CCallableGameCustomDotAAdd : virtual public CBaseCallable
{
public:
	bool m_Error;

	vector<CDBGamePlayer *> m_DBGamePlayers;
	list<CDBPlayer2 *> m_DBPlayers2;
	string m_LobbyLog;
	string m_GameLog;
	string m_ReplayName;
	string m_Map;
	string m_GameName;
	string m_OwnerName;
	string m_CreatorName;
	uint32_t m_CreatorServerID;
	uint32_t m_DBGameID;
	uint32_t m_DBCreatorPlayerID;
	uint32_t m_LobbyDuration;
	uint32_t m_Duration;
	uint32_t m_GameState;
	uint32_t m_GameType;
	bool m_RMK;

	vector<CDBDotAPlayer *> m_DBDotAPlayers;
	string m_Mode1;
	string m_Mode2;
	uint32_t m_DBDotAGameID;
	uint32_t m_Winner;
	uint32_t m_CreepsSpawnedTime;
	uint32_t m_CollectDotAStatsOverTime;
	uint32_t m_Min;
	uint32_t m_Sec;

public:
	CCallableGameCustomDotAAdd( uint32_t nDBGameID, uint32_t nLobbyDuration, string nLobbyLog, string nGameLog, string nReplayName, string nMap, string nGameName, string nOwnerName, uint32_t nDuration, uint32_t nGameState, string nCreatorName, uint32_t nCreatorServerID, vector<CDBGamePlayer *> nDBGamePlayers, bool nRMK, uint32_t nGameType, uint32_t nWinner, uint32_t nCreepsSpawnedTime, uint32_t nCollectDotAStatsOverTime, uint32_t nMin, uint32_t nSec, vector<CDBDotAPlayer *> nDBDotAPlayers, string nMode1, string nMode2 )
		: CBaseCallable( ), m_Error( false ), m_DBGameID( nDBGameID ), m_DBCreatorPlayerID( 0 ), m_LobbyDuration( nLobbyDuration ), m_LobbyLog( nLobbyLog ), m_GameLog( nGameLog ), m_ReplayName( nReplayName ), m_Map( nMap ), m_GameName( nGameName ), m_OwnerName( nOwnerName ), m_Duration( nDuration ), m_GameState( nGameState ), m_CreatorName( nCreatorName ), m_CreatorServerID( nCreatorServerID ), m_DBGamePlayers( nDBGamePlayers ), m_RMK( nRMK ), m_GameType( nGameType ), m_DBDotAGameID( 0 ), m_Winner( nWinner ), m_CreepsSpawnedTime( nCreepsSpawnedTime ), m_CollectDotAStatsOverTime( nCollectDotAStatsOverTime ), m_Min( nMin ), m_Sec( nSec ), m_DBDotAPlayers( nDBDotAPlayers ), m_Mode1( nMode1 ), m_Mode2( nMode2 ) { }
	virtual ~CCallableGameCustomDotAAdd( ) { }

	virtual void ThreadedJob( void *conn );
	void ThreadedJob2( void *conn );
};

class CCallableGameDiv1DotAAdd : virtual public CBaseCallable
{
public:
	bool m_Error;

	vector<CDBGamePlayer *> m_DBGamePlayers;
	list<CDBPlayer2 *> m_DBPlayers2;
	string m_LobbyLog;
	string m_GameLog;
	string m_ReplayName;
	string m_Map;
	string m_GameName;
	string m_OwnerName;
	string m_CreatorName;
	uint32_t m_CreatorServerID;
	uint32_t m_DBGameID;
	uint32_t m_DBCreatorPlayerID;
	uint32_t m_LobbyDuration;
	uint32_t m_Duration;
	uint32_t m_GameState;
	uint32_t m_GameType;
	bool m_RMK;

	vector<CDBDotAPlayer *> m_DBDotAPlayers;
	string m_Mode1;
	string m_Mode2;
	uint32_t m_DBDotAGameID;
	uint32_t m_Winner;
	uint32_t m_CreepsSpawnedTime;
	uint32_t m_CollectDotAStatsOverTime;
	uint32_t m_Min;
	uint32_t m_Sec;
	bool m_FF;
	bool m_Scored;

	vector<CDBDiv1DotAPlayer *> m_DBDiv1DotAPlayers;

public:
	CCallableGameDiv1DotAAdd( uint32_t nDBGameID, uint32_t nLobbyDuration, string nLobbyLog, string nGameLog, string nReplayName, string nMap, string nGameName, string nOwnerName, uint32_t nDuration, uint32_t nGameState, string nCreatorName, uint32_t nCreatorServerID, vector<CDBGamePlayer *> nDBGamePlayers, bool nRMK, uint32_t nGameType, uint32_t nWinner, uint32_t nCreepsSpawnedTime, uint32_t nCollectDotAStatsOverTime, uint32_t nMin, uint32_t nSec, vector<CDBDotAPlayer *> nDBDotAPlayers, string nMode1, string nMode2, bool nFF, bool nScored, vector<CDBDiv1DotAPlayer *> nDBDiv1DotAPlayers ) 
		: CBaseCallable( ), m_Error( false ), m_DBGameID( nDBGameID ), m_DBCreatorPlayerID( 0 ), m_LobbyDuration( nLobbyDuration ), m_LobbyLog( nLobbyLog ), m_GameLog( nGameLog ), m_ReplayName( nReplayName ), m_Map( nMap ), m_GameName( nGameName ), m_OwnerName( nOwnerName ), m_Duration( nDuration ), m_GameState( nGameState ), m_CreatorName( nCreatorName ), m_CreatorServerID( nCreatorServerID ), m_DBGamePlayers( nDBGamePlayers ), m_RMK( nRMK ), m_GameType( nGameType ), m_DBDotAGameID( 0 ), m_Winner( nWinner ), m_CreepsSpawnedTime( nCreepsSpawnedTime ), m_CollectDotAStatsOverTime( nCollectDotAStatsOverTime ), m_Min( nMin ), m_Sec( nSec ), m_DBDotAPlayers( nDBDotAPlayers ), m_Mode1( nMode1 ), m_Mode2( nMode2 ), m_FF( nFF ), m_Scored( nScored ), m_DBDiv1DotAPlayers( nDBDiv1DotAPlayers ) { }
	virtual ~CCallableGameDiv1DotAAdd( ) { }

	virtual void ThreadedJob( void *conn );
	void ThreadedJob2( void *conn );
};

class CCallableBanAdd : virtual public CBaseCallable
{
protected:
	string m_Player;
	uint32_t m_ServerID;
	uint32_t m_GameID;
	BanAddReturn m_Result;

public:
	CCallableBanAdd( string nPlayer, uint32_t nServerID, uint32_t nGameID ) : CBaseCallable( ), m_Player( nPlayer ), m_ServerID( nServerID ), m_GameID( nGameID ) { }
	virtual ~CCallableBanAdd( ) { }

	virtual void ThreadedJob( void *conn );

	virtual BanAddReturn GetResult( )					{ return m_Result; }
	virtual void SetResult( BanAddReturn nResult )		{ m_Result = nResult; }

	string GetName( )									{ return m_Player; }
	uint32_t GetServerID( )								{ return m_ServerID; }
	uint32_t GetGameID( )								{ return m_GameID; }
};

class CCallableDIV1BanAdd : virtual public CBaseCallable
{
protected:
	string m_AdminPlayerName;
	uint32_t m_AdminServerID;
	string m_VictimPlayerName;
	uint32_t m_VictimServerID;
	uint32_t m_GameID;
	string m_Reason;
	BanAddReturn m_Result;

public:
	CCallableDIV1BanAdd( string nAdminPlayerName, uint32_t nAdminServerID, string nVictimPlayerName, uint32_t nVictimServerID, uint32_t nGameID, string nReason ) 
		: CBaseCallable( ), m_AdminPlayerName( nAdminPlayerName ), m_AdminServerID( nAdminServerID ), m_VictimPlayerName( nVictimPlayerName ), m_VictimServerID( nVictimServerID ), m_GameID( nGameID ), m_Reason( nReason ) { }
	virtual ~CCallableDIV1BanAdd( ) { }

	virtual void ThreadedJob( void *conn );

	virtual BanAddReturn GetResult( )					{ return m_Result; }
	virtual void SetResult( BanAddReturn nResult )		{ m_Result = nResult; }

	string GetVictimPlayerName( )						{ return m_VictimPlayerName; }
	uint32_t GetVictimServerID( )						{ return m_VictimServerID; }
	uint32_t GetGameID( )								{ return m_GameID; }
};

class CCallableGameID : virtual public CBaseCallable
{
protected:
	uint32_t m_SlaveID;
	uint32_t m_SlaveGameID;
	uint32_t m_Result;

public:
	CCallableGameID( uint32_t nSlaveID, uint32_t nSlaveGameID ) : CBaseCallable( ), m_SlaveID( nSlaveID ), m_SlaveGameID( nSlaveGameID ), m_Result( 0 ) { }
	virtual ~CCallableGameID( ) { }

	virtual void ThreadedJob( void *conn );

	virtual uint32_t GetResult( )					{ return m_Result; }
	virtual void SetResult( uint32_t nResult )		{ m_Result = nResult; }

	uint32_t GetSlaveID( )							{ return m_SlaveID; }
	uint32_t GetSlaveGameID( )						{ return m_SlaveGameID; }
};

class CCallableCacheList : virtual public CBaseCallable
{
protected:
	set<uint32_t> m_Server;
	CDBCache *m_Result;

public:
	CCallableCacheList( set<uint32_t> nServer ) : CBaseCallable( ), m_Server( nServer ) { }
	virtual ~CCallableCacheList( ) { }

	virtual void ThreadedJob( void *conn );

	virtual CDBCache *GetResult( )				{ return m_Result; }
	virtual void SetResult( CDBCache *nResult )	{ m_Result = nResult; }
};

class CCallableContributorList : virtual public CBaseCallable
{
protected:
	uint32_t m_Server;
	list<string> m_Result;

public:
	CCallableContributorList( uint32_t nServer ) : CBaseCallable( ), m_Server( nServer ) { }
	virtual ~CCallableContributorList( ) { }

	virtual void ThreadedJob( void *conn );

	virtual list<string> GetResult( )				{ return m_Result; }
	virtual void SetResult( list<string> nResult )	{ m_Result = nResult; }
};

class CCallableDotAPlayerSummaryCheck : virtual public CBaseCallable
{
protected:
	string m_Name;
	string m_Server;
	CDBDotAPlayerSummary *m_Result;

public:
	CCallableDotAPlayerSummaryCheck( string nName, string nServer ) : CBaseCallable( ), m_Name( nName ), m_Server( nServer ), m_Result( NULL ) { }
	virtual ~CCallableDotAPlayerSummaryCheck( ) { }

	virtual void ThreadedJob( void *conn );

	virtual string GetName( )								{ return m_Name; }
	virtual string GetServer( )								{ return m_Server; }
	virtual CDBDotAPlayerSummary *GetResult( )				{ return m_Result; }
	virtual void SetResult( CDBDotAPlayerSummary *nResult )	{ m_Result = nResult; }
};

class CCallableUnixTimestampGet : virtual public CBaseCallable
{
protected:
	uint32_t m_Result;

public:
	CCallableUnixTimestampGet( ) : CBaseCallable( ), m_Result( 0 ) { }
	virtual ~CCallableUnixTimestampGet( ) { }

	virtual void ThreadedJob( void *conn );

	virtual uint32_t GetResult( )				{ return m_Result; }
	virtual void SetResult( uint32_t nResult )	{ m_Result = nResult; }
};

//
// CDBCache
//

class CDBCache
{
private:
	list<pair<uint32_t, std::unordered_map<string, CDBPlayer *> > > m_PlayerList;
	//vector<CDBUser *> m_UserList;
	vector<CDBBanGroup *> m_BanGroupList;
	vector<CDBDotAPlayerSummary *> m_DPSList;

public:
	CDBCache(
		list<pair<uint32_t, std::unordered_map<string, CDBPlayer *> > > nPlayerList, 
		vector<CDBBanGroup *> nBanGroupList, 
		vector<CDBDotAPlayerSummary *> nDPSList
	) : m_PlayerList( nPlayerList ), m_BanGroupList( nBanGroupList ), m_DPSList( nDPSList ) { }
	~CDBCache( ) { }

	std::unordered_map<string, CDBPlayer *> GetPlayerList( uint32_t ServerID );
	//vector<CDBUser *> GetUserList( )					{ return m_UserList; }
	vector<CDBBanGroup *> GetBanGroupList( )			{ return m_BanGroupList; }
	vector<CDBDotAPlayerSummary *> GetDPSList( )		{ return m_DPSList; }
};

//
// CDBPlayer
//

class CDBPlayer
{
private:
	uint32_t m_BanGroupID;
	uint32_t m_StatsDotAID;
	uint32_t m_AccessLevel;
	string m_From;
	string m_LongFrom;

public:
	CDBPlayer( uint32_t nBanGroupID, uint32_t nStatsDotAID, uint32_t nAccessLevel ) 
		: m_BanGroupID( nBanGroupID ), m_StatsDotAID( nStatsDotAID ), m_AccessLevel( nAccessLevel ), m_From( "??" ), m_LongFrom( "??" ) { }
	CDBPlayer( uint32_t nBanGroupID, uint32_t nStatsDotAID, uint32_t nAccessLevel, string nFrom, string nLongFrom ) 
		: m_BanGroupID( nBanGroupID ), m_StatsDotAID( nStatsDotAID ), m_AccessLevel( nAccessLevel ), m_From( nFrom ), m_LongFrom( nLongFrom ) { }
	~CDBPlayer( ) { }

	uint32_t GetBanGroupID( )						{ return m_BanGroupID; }
	uint32_t GetStatsDotAID( )						{ return m_StatsDotAID; }
	uint32_t GetAccessLevel( )						{ return m_AccessLevel; }
	string GetFrom( )								{ return m_From; }
	string GetLongFrom( )							{ return m_LongFrom; }

	CDBGamePlayerSummary *GetGamePlayerSummary( );
	CDBDotAPlayerSummary *GetDotAPlayerSummary( );
	CDBBanGroup *GetBanGroup( );
	bool GetIsBanned( );
	bool GetIsAdmin( )								{ return m_AccessLevel > 1000 ? true : false; }

	void SetBanGroupID( uint32_t nBanGroupID )		{ m_BanGroupID = nBanGroupID; }
	void SetStatsDotAID( uint32_t nStatsDotAID )	{ m_StatsDotAID = nStatsDotAID; }
	void SetAccessLevel( uint32_t nAccessLevel )	{ m_AccessLevel = nAccessLevel; }
	void SetFrom( string nFrom )					{ m_From = nFrom; }
	void SetLongFrom( string nLongFrom )			{ m_LongFrom = nLongFrom; }
};

//
// CDBPlayer2
//

class CDBPlayer2
{
private:
	string m_Name;
	string m_Server;
	uint32_t m_Color;
	uint32_t m_PlayerID;
	uint32_t m_BanGroupID;
	uint32_t m_StatsDotAID;
	bool m_Banned;
	bool m_UpdateBanGroupID;
	bool m_UpdateStatsDotAID;
	CDBBanGroup *m_BanGroup;
	CDBDotAPlayerSummary *m_DotAPlayerSummary;
	//double m_Rating;
	//double m_RatingDiff;
	//double m_HighestRating;

public:
	CDBPlayer2( bool nBanned, string nName, string nServer, uint32_t nColor, uint32_t nPlayerID, uint32_t nBanGroupID, uint32_t nStatsDotAID ) 
		: m_Banned( nBanned ), m_Name( nName ), m_Server( nServer ), m_Color( nColor ), m_PlayerID( nPlayerID ), m_BanGroupID( nBanGroupID ), m_StatsDotAID( nStatsDotAID ) { }
	CDBPlayer2( ) { }
	~CDBPlayer2( ) { }

	string GetName( )														{ return m_Name; }
	string GetServer( )														{ return m_Server; }
	uint32_t GetColor( )													{ return m_Color; }
	uint32_t GetPlayerID( )													{ return m_PlayerID; }
	uint32_t GetBanGroupID( )												{ return m_BanGroupID; }
	uint32_t GetStatsDotAID( )												{ return m_StatsDotAID; }
	bool GetBanned( )														{ return m_Banned; }
	bool GetUpdateBanGroupID( )												{ return m_UpdateBanGroupID; }
	bool GetUpdateStatsDotAID( )											{ return m_UpdateStatsDotAID; }
	CDBBanGroup *GetBanGroup( )												{ return m_BanGroup; }
	CDBDotAPlayerSummary *GetDotAPlayerSummary( )							{ return m_DotAPlayerSummary; }
	//double GetRating( )													{ return m_Rating; }
	//double GetRatingDiff( )												{ return m_RatingDiff; }
	//double GetHighestRating( )											{ return m_HighestRating; }

	void SetName( string nName )											{ m_Name = nName; }
	void SetServer( string nServer)											{ m_Server = nServer; }
	void SetColor( uint32_t nColor )										{ m_Color = nColor; }
	void SetPlayerID( uint32_t nPlayerID )									{ m_PlayerID = nPlayerID; }
	void SetBanGroupID( uint32_t nBanGroupID )								{ m_BanGroupID = nBanGroupID; }
	void SetStatsDotAID( uint32_t nStatsDotAID )							{ m_StatsDotAID = nStatsDotAID; }
	void SetBanned( bool nBanned )											{ m_Banned = nBanned; }
	void SetUpdateBanGroupID( bool nUpdateBanGroupID )						{ m_UpdateBanGroupID = nUpdateBanGroupID; }
	void SetUpdateStatsDotAID( bool nUpdateStatsDotAID )					{ m_UpdateStatsDotAID = nUpdateStatsDotAID; }
	void SetBanGroup( CDBBanGroup *nBanGroup )								{ m_BanGroup = nBanGroup; }
	void SetDotAPlayerSummary( CDBDotAPlayerSummary *nDotAPlayerSummary )	{ m_DotAPlayerSummary = nDotAPlayerSummary; }
	//void SetRating( double nRating )										{ m_Rating = nRating; }
	//void SetRatingDiff( double nRatingDiff )								{ m_RatingDiff = nRatingDiff; }
	//void SetHighestRating( double nHighestRating )						{ m_HighestRating = nHighestRating; }
};

//
// CDBBanGroup
//

class CDBBanGroup
{
private:
	uint32_t m_Datetime;
	uint32_t m_Duration;
	uint32_t m_BanPoints;
	uint32_t m_WarnPoints;

public:
	CDBBanGroup( uint32_t nDatetime, uint32_t nDuration, uint32_t nBanPoints, uint32_t nWarnPoints ) : m_Datetime( nDatetime ), m_Duration( nDuration ), m_BanPoints( nBanPoints ), m_WarnPoints( nWarnPoints ) { }
	~CDBBanGroup( ) { }

	bool GetIsBanned( );

	uint32_t GetDatetime( )							{ return m_Datetime; }
	uint32_t GetDuration( )							{ return m_Duration; }
	uint32_t GetBanPoints( )						{ return m_BanPoints; }
	uint32_t GetWarnPoints( )						{ return m_WarnPoints; }

	void SetDatetime( uint32_t nDatetime )			{ m_Datetime = nDatetime; }
	void SetDuration( uint32_t nDuration )			{ m_Duration = nDuration; }
	void SetBanPoints( uint32_t nBanPoints )		{ m_BanPoints = nBanPoints; }
	void SetWarnPoints( uint32_t nWarnPoints )		{ m_WarnPoints = nWarnPoints; }
};

//
// CDBAutoBan
//

class CDBAutoBan
{
private:
	uint32_t m_PlayerID;
	uint32_t m_Datetime;
	uint32_t m_Duration;
	uint32_t m_BanPoints; // ban points because of this ban

public:
	CDBAutoBan( uint32_t nPlayerID, uint32_t nDatetime, uint32_t nDuration, uint32_t nBanPoints ) : m_PlayerID( nPlayerID ), m_Datetime( nDatetime ), m_Duration( nDuration ), m_BanPoints( nBanPoints ) { }
	~CDBAutoBan() { }

	uint32_t GetPlayerID( )							{ return m_PlayerID; }
	uint32_t GetDatetime( )							{ return m_Datetime; }
	uint32_t GetDuration( )							{ return m_Duration; }
	uint32_t GetBanPoints( )						{ return m_BanPoints; }

	void SetPlayerID( uint32_t nPlayerID )			{ m_PlayerID = nPlayerID; }
	void SetDatetime( uint32_t nDatetime )			{ m_Datetime = nDatetime; }
	void SetDuration( uint32_t nDuration )			{ m_Duration = nDuration; }
	void SetBanPoints( uint32_t nBanPoints )		{ m_BanPoints = nBanPoints; }
};

//
// CDBGamePlayer
//

class CDBGamePlayer
{
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
	CDBGamePlayer( bool nBanned, string nName, string nIP, uint32_t nSpoofed, string nSpoofedRealm, uint32_t nReserved, uint32_t nLoadingTime, uint32_t nLeft, string nLeftReason, uint32_t nTeam, uint32_t nColour, bool nGProxy )
		: m_Banned( nBanned ), m_Name( nName ), m_IP( nIP ), m_Spoofed( nSpoofed ), m_SpoofedRealm( nSpoofedRealm ), m_Reserved( nReserved ), m_LoadingTime( nLoadingTime ), m_Left( nLeft ), m_LeftReason( nLeftReason ), m_Team( nTeam ), m_Colour( nColour ), m_GProxy( nGProxy ) { }
	~CDBGamePlayer( ) { }

	string GetName( )			{ return m_Name; }
	string GetIP( )				{ return m_IP; }
	uint32_t GetSpoofed( )		{ return m_Spoofed; }
	string GetSpoofedRealm( )	{ return m_SpoofedRealm; }
	uint32_t GetReserved( )		{ return m_Reserved; }
	uint32_t GetLoadingTime( )	{ return m_LoadingTime; }
	uint32_t GetLeft( )			{ return m_Left; }
	string GetLeftReason( )		{ return m_LeftReason; }
	uint32_t GetTeam( )			{ return m_Team; }
	uint32_t GetColour( )		{ return m_Colour; }
	bool GetBanned( )			{ return m_Banned; }
	bool GetGProxy( )			{ return m_GProxy; }

	void SetLoadingTime( uint32_t nLoadingTime )	{ m_LoadingTime = nLoadingTime; }
	void SetLeft( uint32_t nLeft )					{ m_Left = nLeft; }
	void SetLeftReason( string nLeftReason )		{ m_LeftReason = nLeftReason; }
	void SetBanned( bool nBanned )					{ m_Banned = nBanned; }
};

//
// CDBDotAPlayer
//

class CDBDotAPlayer
{
private:
	uint32_t m_Colour;
	uint32_t m_Level;
	uint32_t m_Kills;
	uint32_t m_Deaths;
	uint32_t m_CreepKills;
	uint32_t m_CreepDenies;
	uint32_t m_Assists;
	uint32_t m_Gold;
	uint32_t m_NeutralKills;
	string m_Items[6];
	string m_Hero;
	uint32_t m_NewColour;
	uint32_t m_TowerKills;
	uint32_t m_RaxKills;
	uint32_t m_CourierKills;

public:
	CDBDotAPlayer( uint32_t nColour, uint32_t nLevel, uint32_t nKills, uint32_t nDeaths, uint32_t nCreepKills, uint32_t nCreepDenies, uint32_t nAssists, uint32_t nGold, uint32_t nNeutralKills, string nItem1, string nItem2, string nItem3, string nItem4, string nItem5, string nItem6, string nHero, uint32_t nNewColour, uint32_t nTowerKills, uint32_t nRaxKills, uint32_t nCourierKills );
	~CDBDotAPlayer( );

	//uint32_t GetID( )			{ return m_ID; }
	//uint32_t GetGameID( )		{ return m_GameID; }
	uint32_t GetColour( )		{ return m_Colour; }
	uint32_t GetLevel( )		{ return m_Level; }
	uint32_t GetKills( )		{ return m_Kills; }
	uint32_t GetDeaths( )		{ return m_Deaths; }
	uint32_t GetCreepKills( )	{ return m_CreepKills; }
	uint32_t GetCreepDenies( )	{ return m_CreepDenies; }
	uint32_t GetAssists( )		{ return m_Assists; }
	uint32_t GetGold( )			{ return m_Gold; }
	uint32_t GetNeutralKills( )	{ return m_NeutralKills; }
	string GetItem( unsigned int i );
	string GetHero( )			{ return m_Hero; }
	uint32_t GetNewColour( )	{ return m_NewColour; }
	uint32_t GetTowerKills( )	{ return m_TowerKills; }
	uint32_t GetRaxKills( )		{ return m_RaxKills; }
	uint32_t GetCourierKills( )	{ return m_CourierKills; }

	void SetColour( uint32_t nColour )				{ m_Colour = nColour; }
	void SetLevel( uint32_t nLevel )				{ m_Level = nLevel; }
	void SetKills( uint32_t nKills )				{ m_Kills = nKills; }
	void SetDeaths( uint32_t nDeaths )				{ m_Deaths = nDeaths; }
	void SetCreepKills( uint32_t nCreepKills )		{ m_CreepKills = nCreepKills; }
	void SetCreepDenies( uint32_t nCreepDenies )	{ m_CreepDenies = nCreepDenies; }
	void SetAssists( uint32_t nAssists )			{ m_Assists = nAssists; }
	void SetGold( uint32_t nGold )					{ m_Gold = nGold; }
	void SetNeutralKills( uint32_t nNeutralKills )	{ m_NeutralKills = nNeutralKills; }
	void SetItem( unsigned int i, string item );
	void SetHero( string nHero )					{ m_Hero = nHero; }
	void SetNewColour( uint32_t nNewColour )		{ m_NewColour = nNewColour; }
	void SetTowerKills( uint32_t nTowerKills )		{ m_TowerKills = nTowerKills; }
	void SetRaxKills( uint32_t nRaxKills )			{ m_RaxKills = nRaxKills; }
	void SetCourierKills( uint32_t nCourierKills )	{ m_CourierKills = nCourierKills; }
};

//
// CDBDiv1DotAPlayer
//

class CDBDiv1DotAPlayer
{
private:
	string m_Name;
	uint32_t m_ServerID;
	uint32_t m_Color;
	uint32_t m_Team;
	double m_Rating;
	double m_HighestRating;
	double m_NewRating;
	double m_NewHighestRating;
	bool m_Banned;
	bool m_RecvNegativePSR;

public:
	CDBDiv1DotAPlayer( string nName, uint32_t nServerID, uint32_t nColor, uint32_t nTeam, double nRating, double nHighestRating, double nNewRating, double nNewHighestRating, bool nBanned, bool nRecvNegativePSR )
		: m_Name( nName ), m_ServerID( nServerID ), m_Color( nColor ), m_Team( nTeam ), m_Rating( nRating ), m_HighestRating( nHighestRating ), m_NewRating( nNewRating ), m_NewHighestRating( nNewHighestRating ), m_Banned( nBanned ), m_RecvNegativePSR( nRecvNegativePSR ) { }
	~CDBDiv1DotAPlayer( ) { }

	string GetName( )										{ return m_Name; }
	uint32_t GetServerID( )									{ return m_ServerID; }
	uint32_t GetColor( )									{ return m_Color; }
	uint32_t GetTeam( )										{ return m_Team; }
	double GetRating( )										{ return m_Rating; }
	double GetHighestRating( )								{ return m_HighestRating; }
	double GetNewRating( )									{ return m_NewRating; }
	double GetNewHighestRating( )							{ return m_NewHighestRating; }
	bool GetBanned( )										{ return m_Banned; }
	bool GetRecvNegativePSR( )								{ return m_RecvNegativePSR; }

	//void SetColor( uint32_t nColor )						{ m_Color = nColor; }
	void SetRating( double nRating )						{ m_Rating = nRating; }
	void SetHighestRating( double nHighestRating )			{ m_HighestRating = nHighestRating; }
	void SetNewRating( double nNewRating )					{ m_NewRating = nNewRating; }
	void SetNewHighestRating( double nNewHighestRating )	{ m_NewHighestRating = nNewHighestRating; }
	void SetBanned( bool nBanned )							{ m_Banned = nBanned; }
};

//
// CDBDotAPlayerSummary
//

class CDBDotAPlayerSummary
{
private:
	uint32_t m_TotalGames;			// total number of dota games played
	uint32_t m_GamesObserved;
	uint32_t m_TotalWins;			// total number of dota games won
	uint32_t m_TotalLosses;			// total number of dota games lost
	uint32_t m_TotalKills;			// total number of hero kills
	uint32_t m_TotalDeaths;			// total number of deaths
	uint32_t m_TotalCreepKills;		// total number of creep kills
	uint32_t m_TotalCreepDenies;	// total number of creep denies
	uint32_t m_TotalAssists;		// total number of assists
	uint32_t m_TotalNeutralKills;	// total number of neutral kills
	//uint32_t m_TotalTowerKills;	// total number of tower kills
	//uint32_t m_TotalRaxKills;		// total number of rax kills
	//uint32_t m_TotalCourierKills;	// total number of courier kills
	double m_Rating;
	double m_HighestRating;

public:
	CDBDotAPlayerSummary( double nRating, double nHighestRating, uint32_t nTotalGames, uint32_t nTotalWins, uint32_t nTotalLosses, uint32_t nTotalKills, uint32_t nTotalDeaths, uint32_t nTotalCreepKills, uint32_t nTotalCreepDenies, uint32_t nTotalAssists, uint32_t nTotalNeutralKills, uint32_t nGamesObserved )
		: m_Rating( nRating ), m_HighestRating( nHighestRating ), m_TotalGames( nTotalGames ), m_TotalWins( nTotalWins ), m_TotalLosses( nTotalLosses ), m_TotalKills( nTotalKills ), m_TotalDeaths( nTotalDeaths ), m_TotalCreepKills( nTotalCreepKills ), m_TotalCreepDenies( nTotalCreepDenies ), m_TotalAssists( nTotalAssists ), m_TotalNeutralKills( nTotalNeutralKills ), m_GamesObserved( nGamesObserved ) { }
	~CDBDotAPlayerSummary( ) { }

	uint32_t GetTotalGames( )			{ return m_TotalGames; }
	uint32_t GetGamesObserved( )		{ return m_GamesObserved; }
	uint32_t GetTotalWins( )			{ return m_TotalWins; }
	uint32_t GetTotalLosses( )			{ return m_TotalLosses; }
	uint32_t GetTotalKills( )			{ return m_TotalKills; }
	uint32_t GetTotalDeaths( )			{ return m_TotalDeaths; }
	uint32_t GetTotalCreepKills( )		{ return m_TotalCreepKills; }
	uint32_t GetTotalCreepDenies( )		{ return m_TotalCreepDenies; }
	uint32_t GetTotalAssists( )			{ return m_TotalAssists; }
	uint32_t GetTotalNeutralKills( )	{ return m_TotalNeutralKills; }
	//uint32_t GetTotalTowerKills( )	{ return m_TotalTowerKills; }
	//uint32_t GetTotalRaxKills( )		{ return m_TotalRaxKills; }
	//uint32_t GetTotalCourierKills( )	{ return m_TotalCourierKills; }
	float GetAvgKills( )				{ return ( m_TotalWins + m_TotalLosses ) > 0 ? (float)m_TotalKills / ( m_TotalWins + m_TotalLosses ) : 0; }
	float GetAvgDeaths( )				{ return ( m_TotalWins + m_TotalLosses ) > 0 ? (float)m_TotalDeaths / ( m_TotalWins + m_TotalLosses ) : 0; }
	float GetAvgCreepKills( )			{ return ( m_TotalWins + m_TotalLosses ) > 0 ? (float)m_TotalCreepKills / ( m_TotalWins + m_TotalLosses ) : 0; }
	float GetAvgCreepDenies( )			{ return ( m_TotalWins + m_TotalLosses ) > 0 ? (float)m_TotalCreepDenies / ( m_TotalWins + m_TotalLosses ) : 0; }
	float GetAvgAssists( )				{ return ( m_TotalWins + m_TotalLosses ) > 0 ? (float)m_TotalAssists / ( m_TotalWins + m_TotalLosses ) : 0; }
	float GetAvgNeutralKills( )			{ return ( m_TotalWins + m_TotalLosses ) > 0 ? (float)m_TotalNeutralKills / ( m_TotalWins + m_TotalLosses ) : 0; }
	//float GetAvgTowerKills( )			{ return m_TotalGames > 0 ? (float)m_TotalTowerKills / m_TotalGames : 0; }
	//float GetAvgRaxKills( )			{ return m_TotalGames > 0 ? (float)m_TotalRaxKills / m_TotalGames : 0; }
	//float GetAvgCourierKills( )		{ return m_TotalGames > 0 ? (float)m_TotalCourierKills / m_TotalGames : 0; }
	double GetRating( )					{ return m_Rating; }
	double GetHighestRating( )			{ return m_HighestRating; }
};

#endif
