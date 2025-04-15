
#ifndef MASL_MANAGER_H
#define MASL_MANAGER_H

#include <boost/bind.hpp>
#include <boost/asio.hpp>

class CDBGamePlayer;
class CDBBan;

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
// CManager
//

class CManager
{
public:
	CGHost *m_GHost;

	queue<CMASLPacket *> m_Packets;
	string m_Server;
	string m_SendBuffer;
	string m_TempSendBuffer;
	string m_RecvBuffer;
	uint32_t m_SlaveID;
	uint32_t m_LastConnectionAttemptTime;
	uint16_t m_Port;
	bool m_AsyncConnectCompleted;
	bool m_AsyncReadCompleted;
	bool m_AsyncWriteCompleted;
	bool m_Connected;
	bool m_Connecting;
	bool m_ProtocolInitialized;

	uint32_t m_LastStatusSentTime;

	boost::asio::io_service io_service_;
	boost::asio::ip::tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length];

public:
	CManager( CGHost *nGHost, string nServer, uint16_t nPort );
	~CManager( );

	void handle_connect( const boost::system::error_code& error );
	void handle_read( const boost::system::error_code& error, size_t bytes_transferred );
	void handle_write( const boost::system::error_code& error, size_t bytes_transferred );

	void Update( );
	void ExtractPackets( );
	void ProcessPackets( );

	void SendLoggedInToBNET( uint32_t serverID );
	void SendDisconnectedFromBNET( uint32_t serverID );
	void SendMaxGamesReached( );
	void SendGameHosted( uint32_t gameType, uint32_t serverID, uint32_t gameID, uint32_t gameState, uint32_t mapSlots, string ownerName, string gameName, string cfgFile );
	void SendGameUnhosted( uint32_t gameID );
	void SendGameStarted( uint32_t gameType, uint32_t creatorServerID, uint32_t gameID, uint32_t gameState, uint32_t startPlayers, string ownerName, string gameName, CBaseGame *game );
	void SendGameIsOver( uint32_t gameID );
	void SendGameNameChanged( uint32_t gameID, uint32_t gameState, string gameName );
	void SendGameOwnerChanged( uint32_t gameID, string ownerName );
	void SendGamePlayerJoinedLobby( uint32_t gameID, string name );
	void SendGamePlayerLeftLobby( uint32_t gameID, string name );
	void SendGamePlayerLeftLobbyInform( string server, string name, uint16_t reason);
	void SendGamePlayerLeftGame( uint32_t gameID, string name );
	void SendGameGetID( uint32_t gameID );

	void SendLobbyStatus( bool gameInLobby, uint32_t gameID, string creatorName );
	void SendSlaveStatus( );

	void SendGameSave( uint32_t mysqlGameID, uint32_t lobbyDuration, string lobbyLog, string gameLog, string replayName, string mapPath, string gameName, string ownerName, uint32_t gameDuration, uint32_t gameState, string creatorName, string creatorServer, vector<CDBGamePlayer *> gamePlayers, bool rmk, uint32_t gameType, string gameInfo );

	void SendErrorGameInLobby( );
	void SendErrorMaxGamesReached( );
	void SendErrorGameNameExists( string gameName );
	void SendUserWasBanned( string server, string victim, uint32_t mysqlGameID, string reason );
	//void SendGetPlayerInfo( string server, string IP, string name );
	void SendGetPlayerInfo( string server, uint32_t IP, string name );

	void SendDIV1PlayerWasBanned( uint32_t adminServerID, string adminName, uint32_t victimServerID, string victimName, uint32_t mysqlGameID, string reason );

	void Send( uint16_t flag, bool send = false );
	void Send( uint16_t flag, string msg, bool send = false );
};

#endif