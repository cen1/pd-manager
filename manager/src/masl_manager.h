
#ifndef MASL_MANAGER_H
#define MASL_MANAGER_H

#include <boost/bind.hpp>
#include <boost/asio.hpp>

class CQueuedGame;
class CDBGamePlayers;

//
// CMASLPacket
//

class CMASLPacket
{
private:
	int m_ID;
	string m_Data;

public:
	CMASLPacket( int nID, string nData ) : m_ID( nID ), m_Data( nData ) { }
	~CMASLPacket( ) { }

	int GetID()			{ return m_ID; }
	string GetData()	{ return m_Data; }
};

//
// CPlayers
//

class CPlayers
{
public:
	CGHost *m_GHost;

	std::unordered_map<string, CDBPlayer *> m_Players;
	string m_Server;
	uint32_t m_ServerID;

public:
	CPlayers( CGHost *nGHost, string nServer, uint32_t nServerID );
	~CPlayers( );

	CDBPlayer *GetPlayer( string name );
	string GetServer( )			{ return m_Server; }
	uint32_t GetServerID( )		{ return m_ServerID; }

	void AddPlayer( string name, CDBPlayer *player );
};

//
// CSlave
//

class CSlave
{
public:
	CGHost *m_GHost;
	CManager *m_Manager;

	queue<CMASLPacket *> m_Packets;
	set<uint32_t> m_BNETs;
	uint32_t m_SlaveBotID;
	map<uint32_t, string> m_SlaveNames; //BNET ID + username, there can be multiple usernames due to multiple realms
	uint32_t m_MaxGames;
	bool m_ProtocolInitialized;
	bool m_GameInLobby;
	bool m_DeleteMe;
	bool m_AsyncReadCompleted;
	bool m_AsyncWriteCompleted;
	string m_SendBuffer;
	string m_RecvBuffer;
	string m_LobbyCreator;
	uint32_t m_GHostGroup;
	bool m_BeeingExchanged;

	boost::asio::ip::tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length];

public:
	CSlave( CGHost *nGHost, CManager *nManager, uint32_t nSlaveID, boost::asio::io_context& io_context );
	~CSlave( );

	boost::asio::ip::tcp::socket& socket( ) { return socket_; }

	void start( );
	void handle_read( const boost::system::error_code& error, size_t bytes_transferred );
	void handle_write( const boost::system::error_code& error, size_t bytes_transferred );

	bool Update( );
	void ExtractPackets( );
	void ProcessPackets( );

	string GetLobbyCreator( )			{ return m_LobbyCreator; }
	uint32_t GetID( )					{ return m_SlaveBotID; }
	uint32_t GetMaxGames( )				{ return m_MaxGames; }
	uint32_t GetNumGames( );
	bool GetProtocolInitialized( )		{ return m_ProtocolInitialized; }
	bool GetGameInLobby( )				{ return m_GameInLobby; }
	bool CanHost( string server, uint32_t ghostGroup );
	bool LoggedIn( uint32_t serverID );
	uint32_t GetGHostGroup( )				{ return m_GHostGroup; }

	void SetGameInLobby( bool nGameInLobby )			{ m_GameInLobby = nGameInLobby; }
	void SetLobbyCreator( string nLobbyCreator )		{ m_LobbyCreator = nLobbyCreator; }

	void SendCreateGame( string creatorServer, uint32_t gameType, uint32_t gameState, string creatorName, string gameName, string mapName, bool observers );
	/*void SendCreateDotAGame( string creatorServer, uint32_t gameState, string creatorName, string gameName );
	void SendCreateCustomGame( string creatorServer, uint32_t gameState, string creatorName, string gameName, string mapName );*/

	void SendContributorOnlyMode( bool contributor_only_mode );

	void ReloadConfig();

	void Send( uint16_t flag );
	void Send( uint16_t flag, string msg );
};

//
// CManager
//

class CManager
{
public:
	CGHost *m_GHost;

	list<CRemoteGame *> m_RemoteGames;				// all our slave bot games in progress
	list<CRemoteGame *> m_RemoteGamesInLobby;		// all our slave bot games in lobby
	vector<CQueuedGame *> m_QueuedGames;
	string m_BindAddress;
	uint32_t m_Port;
	string m_GameListFile;
	uint32_t m_GameListRefreshInterval;
	uint32_t m_GameListLastRefreshTime;
	uint32_t m_LastSlaveID;

public:
	boost::asio::ip::tcp::acceptor *m_Acceptor;

	list<CSlave *> m_Slaves;
	list<CSlave *> m_ConnectingSlaves;

private:
	bool m_AcceptingNewConnections;
	bool slaveUsernameMatches(CSlave* slave, uint32_t number);

public:
	CManager( CGHost *nGHost, string nManagerBindAddress, uint16_t nPort, string nGameListFile, uint32_t nGameListRefreshInterval );
	~CManager( ) { }

	void start_accepting( );
	void handle_accept( CSlave* new_slave, const boost::system::error_code& error );

	void Update( );

	CQueuedGame *GetQueuedGame( uint32_t gameNumber );
	uint32_t QueueGameCount( );
	uint32_t PositionInQueue( CBNET *creatorServer, string creatorName );
	bool HasQueuedGame( CBNET *creatorServer, string creatorName );
	uint32_t HasQueuedGameFromNamePartial( CBNET *creatorServer, string creatorName, CQueuedGame **game );
	void UnqueueGame( CBNET *creatorServer, string creatorName );
	void DeleteQueue( );
	void QueueGame( CBNET *creatorServer, uint32_t gameType, uint32_t gameState, string creatorName, uint32_t accessLevel, string gameName, string map, bool observers, bool queue, uint32_t ghostGroup );
	void EchoPushPlayer( CBNET *server, string pusher, string player, int32_t howMuch );
	/*
	void CreateDotAGame( CBNET *creatorServer, uint32_t gameState, string ownerName, string gameName );
	void CreateRpgGame( CBNET *creatorServer, uint32_t gameState, string ownerName, string gameName, string map );
	void CreateLeagueDotAGame( CBNET *creatorServer, uint32_t gameState, string ownerName, string gameName, string slots, bool ischallenge );
	*/

	uint32_t GetNumLobbies( uint32_t serverID );
	uint32_t GetMaxLobbies( uint32_t serverID );
	uint32_t GetMaxGames( uint32_t serverID );

	CRemoteGame *GetLobby( uint32_t lobbyNumber );
	CRemoteGame *GetGame( uint32_t gameNumber );

	string GetGamesDescription( uint32_t serverID );
	string GetLobbyDescription( string server, string lobbyNumber );
	string GetGameDescription( string server, string gameNumber );
	string GetUsersDescription( CBNET *server );

	string GetLobbyPlayerNames( string lobbyNumber );
	string GetGamePlayerNames( string gameNumber );
	void SendPlayerWhereDescription( CBNET *server, string user, string player );
	string GetWherePlayer( string player );
	CSlave *GetAvailableSlave( string server, uint32_t ghostGroup, bool useTotalGamesLimit );
	bool SoftDisableSlave(uint32_t slaveUsernameNumber);
	void SoftDisableAllSlaves();
	bool SoftEnableSlave(uint32_t slaveUsernameNumber);
	void SoftEnableAllSlaves();
	vector<string> PrintSlaveInfo( );
	vector<string> PrintSlaveInfoLesser( );
	void ReloadSlaveConfig();

	void SendAll( uint16_t flag );
	void SendAll( uint16_t flag, string msg );
	void SendTo( uint32_t slaveID, uint16_t flag );
	void SendTo( uint32_t slaveID, uint16_t flag, string msg );
};

//
// CWebManager
//

class CWebManager
{
public:
	CGHost *m_GHost;

private:
	CUDPServer *m_UDPServer;
	string m_BindAddress;
	uint16_t m_Port;

public:
	CWebManager( CGHost *nGHost, string nBindAddress, uint16_t m_Port );
	~CWebManager( );

	void SetFD( void *fd, void *send_fd, int *nfds );
	void Update( void *fd );
};

#endif
