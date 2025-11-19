
#include "ghost.h"
#include "util.h"
#include "config.h"
#include "bnet.h"
#include "ghostdb.h"
#include "language.h"
#include "socket.h"
#include "masl_manager.h"
#include "masl_protocol_2.h"
#include "masl_slavebot.h"
#include "psr.h"

//
// sorting classes
//

class CQueuedGameSortAscByAccessLevelAndQueuedTime
{
public:
	bool operator( ) ( CQueuedGame *Game1, CQueuedGame *Game2 ) const
	{
		if( Game1->GetAccessLevel( ) == Game2->GetAccessLevel( ) )
			return Game1->GetQueuedTime( ) > Game2->GetQueuedTime( );
		else
			return Game1->GetAccessLevel( ) < Game2->GetAccessLevel( );
	}
};

//
// CPlayers
//

CPlayers :: CPlayers( CGHost *nGHost, string nServer, uint32_t nServerID ) : m_GHost( nGHost ), m_Server( nServer ), m_ServerID( nServerID )
{
	//m_CallablePlayerList = m_GHost->m_DB->ThreadedPlayerList( m_Server );
}

CPlayers :: ~CPlayers( )
{
	for( std::unordered_map<string, CDBPlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); ++i )
		delete (*i).second;
}

CDBPlayer *CPlayers :: GetPlayer( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	std::unordered_map<string, CDBPlayer *>::iterator i = m_Players.find( name );

	if( i != m_Players.end( ) )
		return (*i).second;

	return NULL;
}

void CPlayers :: AddPlayer( string name, CDBPlayer *player )
{
	//CONSOLE_Print( "pp 145.1" );
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	//CONSOLE_Print( "pp 15.2" );
	m_Players.insert( make_pair( name, player ) );
	//CONSOLE_Print( "pp 15.3" );
}

//
// CSlave
//

CSlave :: CSlave( CGHost *nGHost, CManager *nManager, uint32_t nSlaveID, boost::asio::io_context& io_context ) : socket_( io_context )
{
	m_GHost = nGHost;
	m_Manager = nManager;

	m_SlaveBotID = nSlaveID;
	m_MaxGames = 0;
	m_ProtocolInitialized = false;
	m_GameInLobby = false;
	m_DeleteMe = false;
	m_AsyncReadCompleted = true;
	m_AsyncWriteCompleted = true;
	m_BeeingExchanged = false;

	m_GHostGroup = MASL_PROTOCOL :: GHOST_GROUP_ALL;

	//CONSOLE_Print( "[MASL: slave " + UTIL_ToString( m_SlaveBotID ) + "] new slave object created with ID " + UTIL_ToString( m_SlaveBotID ) );
}

CSlave :: ~CSlave( )
{
	CONSOLE_Print( "[MASL: slave " + UTIL_ToString( m_SlaveBotID ) + "] deleting slave object with ID " + UTIL_ToString( m_SlaveBotID ) );

	while( !m_Packets.empty( ) )
	{
		delete m_Packets.front( );
		m_Packets.pop( );
	}
}

void CSlave::start()
{
	CONSOLE_Print("[MASL: slave " + UTIL_ToString(m_SlaveBotID) + "] slave(" + socket_.remote_endpoint().address().to_string() + ":" + UTIL_ToString(socket_.remote_endpoint().port()) + ") initiated connection, assigned ID is " + UTIL_ToString(m_SlaveBotID) + "");

	Send(MASL_PROTOCOL::MTS_HI, UTIL_ToString(m_SlaveBotID));
}

void CSlave::handle_read(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (!error)
	{
		/*boost::asio::async_write( socket_, boost::asio::buffer( data_, bytes_transferred ),
			boost::bind( &CSlave::handle_write, this, boost::asio::placeholders::error ) );*/

		m_RecvBuffer += string(data_, bytes_transferred);

		//DEBUG_Print( "[MASL: slave " + UTIL_ToString( m_SlaveBotID ) + "] \"CSlave :: handle_read\"" );

		/*CONSOLE_Print( "void CSlave :: handle_read" );
		CONSOLE_Print( "data_ = " + string( data_, bytes_transferred ) );
		CONSOLE_Print( "m_RecvBuffer = " + m_RecvBuffer );*/

		//string Packet = string( data_, bytes_transferred );

		/*if( Packet == "close" )
		{
			socket_->shutdown( boost::asio::ip::tcp::socket::shutdown_both );
			socket_->close( );
		}
		else if( Packet == "send" )
		{
			char Data[5] = "1234";

			boost::asio::async_write( *socket_, boost::asio::buffer( Data, 5 ),
				boost::bind( &CSlave::handle_write, this, boost::asio::placeholders::error ) );
		}*/

		// only start another async read when this one is finished
	}
	else
	{
		//CONSOLE_Print( "[MASL: slave " + UTIL_ToString( m_SlaveBotID ) + "] error in \"CSlave :: handle_read\"" );
		CONSOLE_Print("[MASL: slave " + UTIL_ToString(m_SlaveBotID) + "] error in \"CSlave :: handle_read\", error " + UTIL_ToString(error.value()) + " [" + error.message() + "]");

		m_DeleteMe = true;
	}

	m_AsyncReadCompleted = true;
}

void CSlave::handle_write(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (!error)
	{
		/*socket_.async_read_some( boost::asio::buffer( data_, max_length ),
			boost::bind( &CSlave::handle_read, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) );*/

		m_SendBuffer.erase(0, bytes_transferred);

		DEBUG_Print("[MASL: slave " + UTIL_ToString(m_SlaveBotID) + "] \"CSlave :: handle_write\"");

		// only start another async write when this one is finished
	}
	else
	{
		//CONSOLE_Print( "[MASL: slave " + UTIL_ToString( m_SlaveBotID ) + "] error in \"CSlave :: handle_write\"" );
		CONSOLE_Print("[MASL: slave " + UTIL_ToString(m_SlaveBotID) + "] error in \"CSlave :: handle_write\", error " + UTIL_ToString(error.value()) + " [" + error.message() + "]");

		m_DeleteMe = true;
	}

	m_AsyncWriteCompleted = true;
}

bool CSlave::Update()
{
	if (m_AsyncReadCompleted)
	{
		socket_.async_read_some(boost::asio::buffer(data_, max_length),
			boost::bind(&CSlave::handle_read, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

		m_AsyncReadCompleted = false;
	}

	if (m_AsyncWriteCompleted && !m_SendBuffer.empty())
	{
		socket_.async_write_some(boost::asio::buffer(m_SendBuffer),
			boost::bind(&CSlave::handle_write, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

		m_AsyncWriteCompleted = false;
	}

	ExtractPackets();

	ProcessPackets();

	return m_DeleteMe;
}

void CSlave :: ExtractPackets( )
{
	// packet minimum size is 6 bytes (4 byte for packet size + 2 byte packet ID)

	while( m_RecvBuffer.size( ) >= 6 )
	{
		// first 4 bytes contain the length of the packet

		BYTEARRAY Header = UTIL_CreateByteArray( (unsigned char *)m_RecvBuffer.c_str( ), 6 );

		uint32_t DataSize = UTIL_ByteArrayToUInt32( Header, true );
		uint32_t PacketID = UTIL_ByteArrayToUInt16( Header, true, 4 );

		if( m_RecvBuffer.size( ) >= 6 + DataSize )
		{
			string Data = m_RecvBuffer.substr( 6, DataSize );
			m_Packets.push( new CMASLPacket( PacketID, Data ) );

			DEBUG_Print( "[MASL: slave " + UTIL_ToString( m_SlaveBotID ) + "] received packet " + MASL_PROTOCOL :: FlagToString( PacketID ) + " [" + UTIL_ByteArrayToHexString( Header ) + "][" + Data + "], header size [" + UTIL_ToString( DataSize ) + "], header ID [" + UTIL_ToString( PacketID ) + "]" );
			//CONSOLE_Print( "[MASL: slave " + UTIL_ToString( m_SlaveBotID ) + "] Extracted header [" + UTIL_ByteArrayToHexString( Header ) + "] size=" + UTIL_ToString( DataSize ) + " ID=" + UTIL_ToString( PacketID ) );

			// remove extracted packet from the receive buffer

			m_RecvBuffer.erase( 0, 6 + DataSize );
		}
		else
			break;
	}
}

void CSlave :: ProcessPackets( )
{
	while( !m_Packets.empty( ) )
	{
		CMASLPacket *Packet = m_Packets.front( );
		m_Packets.pop( );

		stringstream SS;
		SS << Packet->GetData( );

		switch( Packet->GetID( ) )
		{
		case MASL_PROTOCOL :: STM_THATSALL:
			{
				m_ProtocolInitialized = true;
				Send( MASL_PROTOCOL :: MTS_OK );

				SendContributorOnlyMode( m_GHost->m_ContributorOnlyMode );
			}
			break;

		case MASL_PROTOCOL :: STM_SERVERINFO:
			{
				while( !SS.eof( ) )
				{
					uint32_t ServerID;
					SS >> ServerID;
					m_BNETs.insert( ServerID );
				}
			}
			break;

		case MASL_PROTOCOL :: STM_GHOST_GROUP:
			{
				if ( !SS.eof( ) ) {
				    SS >> m_GHostGroup;
				    SS >> m_region;
				}
				DEBUG_Print( "[MASL: slave " + UTIL_ToString( m_SlaveBotID ) + "] received packet STM_GHOST_GROUP "+SS.str());
			}
			break;

		case MASL_PROTOCOL :: STM_GAME_SAVEDATA:
			{
				uint32_t MySQLGameID;
				uint32_t LobbyDuration;
				uint32_t LobbyLogSize;
				string LobbyLog;
				uint32_t GameLogSize;
				string GameLog;
				uint32_t ReplayNameSize;
				string ReplayName;
				uint32_t MapLocalPathSize;
				string MapLocalPath;
				uint32_t GameNameSize;
				string GameName;
				string OwnerName;
				uint32_t GameDuration;
				uint32_t GameState;
				string CreatorName;
				string CreatorServer;
				uint32_t GamePlayersSize;
				vector<CDBGamePlayer *> GamePlayers;
				uint32_t RMK;
				uint32_t GameType;

				// read the values

				SS >> MySQLGameID;
				SS >> LobbyDuration;

				SS >> LobbyLogSize;
				LobbyLog = UTIL_SSRead( SS, 1, LobbyLogSize );

				SS >> GameLogSize;
				GameLog = UTIL_SSRead( SS, 1, GameLogSize );

				SS >> ReplayNameSize;
				ReplayName = UTIL_SSRead( SS, 1, ReplayNameSize );

				SS >> MapLocalPathSize;
				MapLocalPath = UTIL_SSRead( SS, 1, MapLocalPathSize );

				SS >> GameNameSize;
				GameName = UTIL_SSRead( SS, 1, GameNameSize );

				SS >> OwnerName;
				transform( OwnerName.begin( ), OwnerName.end( ), OwnerName.begin( ), (int(*)(int))tolower );

				SS >> GameDuration;
				SS >> GameState;

				SS >> CreatorName;
				transform( CreatorName.begin( ), CreatorName.end( ), CreatorName.begin( ), (int(*)(int))tolower );

				SS >> CreatorServer;
				SS >> GamePlayersSize;

				DEBUG_Print( "MapLocalPathSize = " + UTIL_ToString( MapLocalPathSize ) );
				DEBUG_Print( "MapLocalPath = " + MapLocalPath );
				DEBUG_Print( "GameNameSize = " + UTIL_ToString( GameNameSize ) );
				DEBUG_Print( "GameName = " + GameName );
				DEBUG_Print( "OwnerName = " + OwnerName );
				DEBUG_Print( "GameDuration = " + UTIL_ToString( GameDuration ) );
				DEBUG_Print( "GameState = " + UTIL_ToString( GameState ) );
				DEBUG_Print( "CreatorName = " + CreatorName );
				DEBUG_Print( "CreatorServer = " + CreatorServer );
				DEBUG_Print( "GamePlayersSize = " + UTIL_ToString( GamePlayersSize ) );

				for( int i = 0; i < GamePlayersSize; ++i )
				{
					string Name;
					string Server;
					uint32_t Color;
					string IP;
					uint32_t Left;
					uint32_t LeftReasonSize;
					string LeftReason;
					uint32_t LoadingTime;
					uint32_t Team;
					uint32_t Reserved;
					bool Banned;
					bool GProxy;

					SS >> Name;
					transform( Name.begin( ), Name.end( ), Name.begin( ), (int(*)(int))tolower );

					SS >> Server;
					SS >> Color;
					SS >> IP;
					SS >> Left;

					SS >> LeftReasonSize;
					LeftReason = UTIL_SSRead( SS, 1, LeftReasonSize );

					SS >> LoadingTime;
					SS >> Team;
					SS >> Reserved;
					SS >> Banned;
					SS >> GProxy;
					
					DEBUG_Print( "Name = " + Name );
					DEBUG_Print( "Server = " + Server );
					DEBUG_Print( "Color = " + UTIL_ToString( Color ) );
					DEBUG_Print( "IP = " + IP );
					DEBUG_Print( "Left = " + UTIL_ToString( Left ) );
					DEBUG_Print( "LeftReasonSize = " + UTIL_ToString( LeftReasonSize ) );
					DEBUG_Print( "LeftReason = " + LeftReason );
					DEBUG_Print( "LoadingTime = " + UTIL_ToString( LoadingTime ) );
					DEBUG_Print( "Team = " + UTIL_ToString( Team ) );
					DEBUG_Print( "Reserved = " + UTIL_ToString( Reserved ) );
					DEBUG_Print( "Banned = " + string( Banned ? "true" : "false" ) );
					DEBUG_Print( "GProxy = " + string( GProxy ? "true" : "false" ) );

					GamePlayers.push_back( new CDBGamePlayer( Banned, Name, IP, 1, Server, Reserved, LoadingTime, Left, LeftReason, Team, Color, GProxy ) );
				}

				SS >> RMK;
				SS >> GameType;

				switch( GameType )
				{
				case MASL_PROTOCOL :: DB_CUSTOM_GAME:
					{
						m_GHost->m_GameCustomAdds.push_back( m_GHost->m_DB->ThreadedGameCustomAdd( MySQLGameID, LobbyDuration, LobbyLog, GameLog, ReplayName, MapLocalPath, GameName, OwnerName, GameDuration, GameState, CreatorName, GetServerID( CreatorServer ), GamePlayers, RMK, GameType, m_region ) );
					}
					break;

				case MASL_PROTOCOL :: DB_CUSTOM_DOTA_GAME:
					{
						uint32_t Winner;
						uint32_t CreepsSpawnedTime;
						uint32_t CollectDotAStatsOverTime;
						uint32_t Min;
						uint32_t Sec;
						string Mode1;
						string Mode2;
						uint32_t DBDotAPlayersSize;
						vector<CDBDotAPlayer *> DBDotAPlayers;

						SS >> Winner;
						SS >> CreepsSpawnedTime;
						SS >> CollectDotAStatsOverTime;
						SS >> Min;
						SS >> Sec;
						MASL_PROTOCOL :: SSReadString( SS, Mode1 );
						MASL_PROTOCOL :: SSReadString( SS, Mode2 );
						transform( Mode1.begin( ), Mode1.end( ), Mode1.begin( ), (int(*)(int))tolower );
						transform( Mode2.begin( ), Mode2.end( ), Mode2.begin( ), (int(*)(int))tolower );
						//SS >> Mode1;
						//SS >> Mode2;
						SS >> DBDotAPlayersSize;

						DEBUG_Print( "Winner = " + UTIL_ToString( Winner ) );
						DEBUG_Print( "Min = " + UTIL_ToString( Min ) );
						DEBUG_Print( "Sec = " + UTIL_ToString( Sec ) );
						DEBUG_Print( "Mode1 = " + Mode1 );
						DEBUG_Print( "Mode2 = " + Mode2 );
						DEBUG_Print( "DBDotAPlayersSize = " + UTIL_ToString( DBDotAPlayersSize ) );

						for( int i = 0; i < DBDotAPlayersSize; ++i )
						{
							uint32_t Color;
							uint32_t Level;
							uint32_t Kills;
							uint32_t Deaths;
							uint32_t CreepKills;
							uint32_t CreepDenies;
							uint32_t Assists;
							uint32_t Gold;
							uint32_t NeutralKills;
							string Item1;
							string Item2;
							string Item3;
							string Item4;
							string Item5;
							string Item6;
							string Hero;
							uint32_t NewColor;
							uint32_t TowerKills;
							uint32_t RaxKills;
							uint32_t CourierKills;

							SS >> Color;
							SS >> Level;
							SS >> Kills;
							SS >> Deaths;
							SS >> CreepKills;
							SS >> CreepDenies;
							SS >> Assists;
							SS >> Gold;
							SS >> NeutralKills;
							MASL_PROTOCOL :: SSReadString( SS, Item1 );
							MASL_PROTOCOL :: SSReadString( SS, Item2 );
							MASL_PROTOCOL :: SSReadString( SS, Item3 );
							MASL_PROTOCOL :: SSReadString( SS, Item4 );
							MASL_PROTOCOL :: SSReadString( SS, Item5 );
							MASL_PROTOCOL :: SSReadString( SS, Item6 );
							MASL_PROTOCOL :: SSReadString( SS, Hero );
							SS >> NewColor;
							SS >> TowerKills;
							SS >> RaxKills;
							SS >> CourierKills;

							DEBUG_Print( "Color = " + UTIL_ToString( Color ) );
							DEBUG_Print( "Level = " + UTIL_ToString( Level ) );
							DEBUG_Print( "Kills = " + UTIL_ToString( Kills ) );
							DEBUG_Print( "Deaths = " + UTIL_ToString( Deaths ) );
							DEBUG_Print( "CreepKills = " + UTIL_ToString( CreepKills ) );
							DEBUG_Print( "CreepDenies = " + UTIL_ToString( CreepDenies ) );
							DEBUG_Print( "Assists = " + UTIL_ToString( Assists ) );
							DEBUG_Print( "Gold = " + UTIL_ToString( Gold ) );
							DEBUG_Print( "NeutralKills = " + UTIL_ToString( NeutralKills ) );
							DEBUG_Print( "Item1 = " + Item1 );
							DEBUG_Print( "Item2 = " + Item2 );
							DEBUG_Print( "Item3 = " + Item3 );
							DEBUG_Print( "Item4 = " + Item4 );
							DEBUG_Print( "Item5 = " + Item5 );
							DEBUG_Print( "Item6 = " + Item6 );
							DEBUG_Print( "Hero = " + Hero );
							DEBUG_Print( "NewColor = " + UTIL_ToString( NewColor ) );
							DEBUG_Print( "TowerKills = " + UTIL_ToString( TowerKills ) );
							DEBUG_Print( "RaxKills = " + UTIL_ToString( RaxKills ) );
							DEBUG_Print( "CourierKills = " + UTIL_ToString( CourierKills ) );

							DBDotAPlayers.push_back( new CDBDotAPlayer( Color, Level, Kills, Deaths, CreepKills, CreepDenies, Assists, Gold, NeutralKills, Item1, Item2, Item3, Item4, Item5, Item6, Hero, NewColor, TowerKills, RaxKills, CourierKills ) );
						}

						m_GHost->m_GameCustomDotAAdds.push_back( m_GHost->m_DB->ThreadedGameCustomDotAAdd( MySQLGameID, LobbyDuration, LobbyLog, GameLog, ReplayName, MapLocalPath, GameName, OwnerName, GameDuration, GameState, CreatorName, GetServerID( CreatorServer ), GamePlayers, RMK, GameType, Winner, CreepsSpawnedTime, CollectDotAStatsOverTime, Min, Sec, DBDotAPlayers, Mode1, Mode2, m_region ) );
					}
					break;

				case MASL_PROTOCOL :: DB_DIV1_DOTA_GAME:
					{
						uint32_t Winner;
						uint32_t CreepsSpawnedTime;
						uint32_t CollectDotAStatsOverTime;
						uint32_t Min;
						uint32_t Sec;
						string Mode1;
						string Mode2;
						uint32_t DBDotAPlayersSize;
						vector<CDBDotAPlayer *> DBDotAPlayers;

						SS >> Winner;
						SS >> CreepsSpawnedTime;
						SS >> CollectDotAStatsOverTime;
						SS >> Min;
						SS >> Sec;
						MASL_PROTOCOL :: SSReadString( SS, Mode1 );
						MASL_PROTOCOL :: SSReadString( SS, Mode2 );
						transform( Mode1.begin( ), Mode1.end( ), Mode1.begin( ), (int(*)(int))tolower );
						transform( Mode2.begin( ), Mode2.end( ), Mode2.begin( ), (int(*)(int))tolower );
						//SS >> Mode1;
						//SS >> Mode2;
						SS >> DBDotAPlayersSize;

						DEBUG_Print( "Winner = " + UTIL_ToString( Winner ) );
						DEBUG_Print( "Min = " + UTIL_ToString( Min ) );
						DEBUG_Print( "Sec = " + UTIL_ToString( Sec ) );
						DEBUG_Print( "Mode1 = " + Mode1 );
						DEBUG_Print( "Mode2 = " + Mode2 );
						DEBUG_Print( "DBDotAPlayersSize = " + UTIL_ToString( DBDotAPlayersSize ) );

						for( int i = 0; i < DBDotAPlayersSize; ++i )
						{
							uint32_t Color;
							uint32_t Level;
							uint32_t Kills;
							uint32_t Deaths;
							uint32_t CreepKills;
							uint32_t CreepDenies;
							uint32_t Assists;
							uint32_t Gold;
							uint32_t NeutralKills;
							string Item1;
							string Item2;
							string Item3;
							string Item4;
							string Item5;
							string Item6;
							string Hero;
							uint32_t NewColor;
							uint32_t TowerKills;
							uint32_t RaxKills;
							uint32_t CourierKills;

							SS >> Color;
							SS >> Level;
							SS >> Kills;
							SS >> Deaths;
							SS >> CreepKills;
							SS >> CreepDenies;
							SS >> Assists;
							SS >> Gold;
							SS >> NeutralKills;
							MASL_PROTOCOL :: SSReadString( SS, Item1 );
							MASL_PROTOCOL :: SSReadString( SS, Item2 );
							MASL_PROTOCOL :: SSReadString( SS, Item3 );
							MASL_PROTOCOL :: SSReadString( SS, Item4 );
							MASL_PROTOCOL :: SSReadString( SS, Item5 );
							MASL_PROTOCOL :: SSReadString( SS, Item6 );
							MASL_PROTOCOL :: SSReadString( SS, Hero );
							SS >> NewColor;
							SS >> TowerKills;
							SS >> RaxKills;
							SS >> CourierKills;

							DEBUG_Print( "Color = " + UTIL_ToString( Color ) );
							DEBUG_Print( "Level = " + UTIL_ToString( Level ) );
							DEBUG_Print( "Kills = " + UTIL_ToString( Kills ) );
							DEBUG_Print( "Deaths = " + UTIL_ToString( Deaths ) );
							DEBUG_Print( "CreepKills = " + UTIL_ToString( CreepKills ) );
							DEBUG_Print( "CreepDenies = " + UTIL_ToString( CreepDenies ) );
							DEBUG_Print( "Assists = " + UTIL_ToString( Assists ) );
							DEBUG_Print( "Gold = " + UTIL_ToString( Gold ) );
							DEBUG_Print( "NeutralKills = " + UTIL_ToString( NeutralKills ) );
							DEBUG_Print( "Item1 = " + Item1 );
							DEBUG_Print( "Item2 = " + Item2 );
							DEBUG_Print( "Item3 = " + Item3 );
							DEBUG_Print( "Item4 = " + Item4 );
							DEBUG_Print( "Item5 = " + Item5 );
							DEBUG_Print( "Item6 = " + Item6 );
							DEBUG_Print( "Hero = " + Hero );
							DEBUG_Print( "NewColor = " + UTIL_ToString( NewColor ) );
							DEBUG_Print( "TowerKills = " + UTIL_ToString( TowerKills ) );
							DEBUG_Print( "RaxKills = " + UTIL_ToString( RaxKills ) );
							DEBUG_Print( "CourierKills = " + UTIL_ToString( CourierKills ) );

							DBDotAPlayers.push_back( new CDBDotAPlayer( Color, Level, Kills, Deaths, CreepKills, CreepDenies, Assists, Gold, NeutralKills, Item1, Item2, Item3, Item4, Item5, Item6, Hero, NewColor, TowerKills, RaxKills, CourierKills ) );
						}

						bool Scored;
						bool FF;
						uint32_t DBDiv1DotAPlayersSize;
						vector<CDBDiv1DotAPlayer *> DBDiv1DotAPlayers;

						SS >> Scored;
						SS >> FF;
						SS >> DBDiv1DotAPlayersSize;

						for( int i = 0; i < DBDiv1DotAPlayersSize; ++i )
						{
							string Name;
							uint32_t ServerID;
							uint32_t Color;
							uint32_t Team;
							bool Banned;
							bool RecvNegativePSR;

							SS >> Name;
							transform( Name.begin( ), Name.end( ), Name.begin( ), (int(*)(int))tolower );

							SS >> ServerID;
							SS >> Color;
							SS >> Team;
							SS >> Banned;
							SS >> RecvNegativePSR;

							DEBUG_Print( "Color = " + UTIL_ToString( Color ) );
							DEBUG_Print( "Team = " + UTIL_ToString( Team ) );

							double Rating = MASL_PROTOCOL :: DB_DIV1_STARTING_PSR;
							double HighestRating = MASL_PROTOCOL :: DB_DIV1_STARTING_PSR;

							CDBPlayer *Player = NULL;

							for( list<CPlayers *> :: iterator j = m_GHost->m_Players.begin( ); j != m_GHost->m_Players.end( ); ++j )
							{
								if( ServerID == (*j)->GetServerID( ) )
								{
									Player = (*j)->GetPlayer( Name );

									if( Player )
									{
										CDBDotAPlayerSummary *DotAPlayerSummary = Player->GetDotAPlayerSummary( );
										if( DotAPlayerSummary )
										{
											Rating = DotAPlayerSummary->GetRating( );
											HighestRating = DotAPlayerSummary->GetHighestRating( );
										}
									}

									break;
								}
							}

							DEBUG_Print( "Rating = " + UTIL_ToString( Rating, 2 ) );
							DEBUG_Print( "HighestRating = " + UTIL_ToString( HighestRating, 2 ) );

							// new Rating and new HighestRating will get overwritten later if someone won the game

							DBDiv1DotAPlayers.push_back( new CDBDiv1DotAPlayer( Name, ServerID, Color, Team, Rating, HighestRating, Rating, HighestRating, Banned, RecvNegativePSR ) );
						}

						// calculate players PSR

						vector<PairedPlayerRating> Team1;
						vector<PairedPlayerRating> Team2;

						for( vector<CDBDiv1DotAPlayer *> :: iterator i = DBDiv1DotAPlayers.begin( ); i != DBDiv1DotAPlayers.end( ); ++i )
						{
							// skip this player if he's an observer

							if( (*i)->GetColor( ) > 11 )
								continue;

							// GetTeam( ), sentinel => 0, scourge => 1

							if( (*i)->GetTeam( ) == 0 )
								Team1.push_back( make_pair( (*i)->GetName( ), (*i)->GetRating( ) ) );
							else if( (*i)->GetTeam( ) == 1 )
								Team2.push_back( make_pair( (*i)->GetName( ), (*i)->GetRating( ) ) );
						}

						DEBUG_Print( "Team1.size( ) = " + UTIL_ToString( Team1.size( ) ) );
						DEBUG_Print( "Team2.size( ) = " + UTIL_ToString( Team2.size( ) ) );
						
						CPSR *PSR = new CPSR( );

						// lower the K factor for -em games, players will receive less rating from -em games

						uint32_t NumModes = Mode1.size( ) / 2;

						for( int i = 0; i < NumModes; ++i )
						{
							string Mode = Mode1.substr( i * 2, 2 );

							if( Mode == "em" )
								PSR->SetBaseKFactor( 8 );
						}

						if (m_GHost->m_UseNewPSRFormula) {
							PSR->CalculatePSR_New(Team1, Team2);
						}
						else {
							PSR->CalculatePSR(Team1, Team2, m_GHost->m_DotaMaxWinChanceDiffGainConstant);
						}

						for( vector<CDBDiv1DotAPlayer *> :: iterator i = DBDiv1DotAPlayers.begin( ); i != DBDiv1DotAPlayers.end( ); ++i )
						{
							// skip this player if he's an observer

							if( (*i)->GetColor( ) > 11 )
								continue;

							// GetTeam( ), sentinel => 0, scourge => 1, observers => 12?

							if( Scored && !(*i)->GetBanned( ) && !(*i)->GetRecvNegativePSR( ) )
							{
								// update rating only if someone won the game

								if( (*i)->GetTeam( ) == Winner - 1 )
									(*i)->SetNewRating( (*i)->GetRating( ) + PSR->GetPlayerGainLoss( (*i)->GetName( ) ).first );
								else
									(*i)->SetNewRating( (*i)->GetRating( ) + ( -1 * PSR->GetPlayerGainLoss( (*i)->GetName( ) ).second ) );
								
								if( (*i)->GetNewRating( ) > (*i)->GetHighestRating( ) )
									(*i)->SetNewHighestRating( (*i)->GetNewRating( ) );
							}

							if( (*i)->GetBanned( ) || (*i)->GetRecvNegativePSR( ) )
							{
								// give negative rating to all leavers
								double lossAmount = PSR->GetPlayerGainLoss( (*i)->GetName( ) ).second;

								// If multiplier is set, we penalize with multiplied PSR instead of autoban
								if ( m_GHost->m_DotaAutobanPSRMultiplier > 0 ) {
								    lossAmount = lossAmount * m_GHost->m_DotaAutobanPSRMultiplier;
								    (*i)->SetBanned(false);
								}

								double newRating = (*i)->GetRating( ) - lossAmount;
								(*i)->SetNewRating( newRating );
								// Also update the base rating so the penalty persists in cache
								(*i)->SetRating( newRating );
							}

							CONSOLE_Print( "(*i)->GetName( ) = " + (*i)->GetName( ) );
							CONSOLE_Print( "(*i)->GetRating( ) = " + UTIL_ToString( (*i)->GetRating( ), 0 ) );
							CONSOLE_Print( "(*i)->GetHighestRating( ) = " + UTIL_ToString( (*i)->GetHighestRating( ), 0 ) );
							CONSOLE_Print( "(*i)->GetNewRating( ) = " + UTIL_ToString( (*i)->GetNewRating( ), 0 ) );
							CONSOLE_Print( "(*i)->GetNewHighestRating( ) = " + UTIL_ToString( (*i)->GetNewHighestRating( ), 0 ) );
						}

						delete PSR;

						DEBUG_Print( "pp 0" );

						m_GHost->m_GameDiv1DotAAdds.push_back( m_GHost->m_DB->ThreadedGameDiv1DotAAdd( MySQLGameID, LobbyDuration, LobbyLog, GameLog, ReplayName, MapLocalPath, GameName, OwnerName, GameDuration, GameState, CreatorName, GetServerID( CreatorServer ), GamePlayers, RMK, GameType, Winner, CreepsSpawnedTime, CollectDotAStatsOverTime, Min, Sec, DBDotAPlayers, Mode1, Mode2, FF, Scored, DBDiv1DotAPlayers, m_region ) );

						DEBUG_Print( "pp 2" );
					}
					break;
				}
			}
			break;

		case MASL_PROTOCOL :: STM_USER_WASBANNED:
			{
				string Server;
				string Name;
				uint32_t MySQLGameID = 0;
				string Reason;
				
				SS >> Server;
				SS >> Name;
				SS >> MySQLGameID;
				SS >> Reason;

				m_GHost->m_BanAdds.push_back( m_GHost->m_DB->ThreadedBanAdd( Name, GetServerID( Server ), MySQLGameID ) );
			}
			break;

		case MASL_PROTOCOL :: STM_DIV1_PLAYERBANNEDBYPLAYER:
			{
				/*uint32_t AdminServerID;
				string AdminName;
				uint32_t VictimServerID;
				string VictimName;
				uint32_t MySQLGameID;
				string Reason;
				
				SS >> AdminServerID;
				SS >> AdminName;
				SS >> VictimServerID;
				SS >> VictimName;
				SS >> MySQLGameID;
				getline( SS, Reason );

				m_GHost->m_DIV1BanAdds.push_back( m_GHost->m_DB->ThreadedDIV1BanAdd( AdminName, AdminServerID, VictimName, VictimServerID, MySQLGameID, Reason ) );*/
			}
			break;

		case MASL_PROTOCOL :: STM_GAME_GETDBID:
			{
				uint32_t SlaveGameID = 0;
				SS >> SlaveGameID;

				m_GHost->m_GameIDs.push_back( m_GHost->m_DB->ThreadedGameID( m_SlaveBotID, SlaveGameID, m_region ) );
			}
			break;

		case MASL_PROTOCOL :: STM_LOBBY_STATUS:
			{
				uint32_t GameInLobby;
				uint32_t GameID;
				string Owner;

				SS >> GameInLobby;

				if( GameInLobby )
				{
					SS >> GameID;
					SS >> Owner;

					m_GameInLobby = true;
					m_LobbyCreator = Owner;
				}
				else
				{
					m_GameInLobby = false;
					m_LobbyCreator.clear( );
				}
			}
			break;

		case MASL_PROTOCOL :: STM_SLAVE_STATUS:
			{
				string DebugString;

				bool GameInLobby;
				uint32_t GameID;
				string OwnerName;

				SS >> GameInLobby;

				if( GameInLobby )
				{
					SS >> GameID;
					MASL_PROTOCOL::SSReadString( SS, OwnerName );

					m_GameInLobby = true;
					m_LobbyCreator = OwnerName;
				}
				else
				{
					m_GameInLobby = false;
					m_LobbyCreator.clear( );
				}

				DebugString += "m_SlaveBotID = " + UTIL_ToString( m_SlaveBotID );
				DebugString += ", GameInLobby = " + string( GameInLobby ? "true" : "false" );
				DebugString += ", GameID = " + UTIL_ToString( GameID );
				DebugString += ", OwnerName = " + OwnerName;

				uint32_t ServersSize;

				SS >> ServersSize;

				DebugString += ", ServersSize = " + UTIL_ToString( ServersSize );

				for( unsigned int i = 0; i < ServersSize; ++i )
				{
					uint32_t ServerID;
					bool LoggedIn;
					string Username;

					SS >> ServerID;
					SS >> LoggedIn;
					SS >> Username;

					if( LoggedIn )
						m_BNETs.insert( ServerID );
					else
						m_BNETs.erase( ServerID );

					m_SlaveNames.insert(pair<uint32_t, string>(ServerID, Username));
					DebugString += ", ServerID = " + UTIL_ToString( ServerID );
					DebugString += ", LoggedIn = " + string( LoggedIn ? "true" : "false" );
					DebugString += ", Username = " + Username;
				}

				DEBUG_Print( DebugString );
			}
			break;

		case MASL_PROTOCOL :: STM_SLAVE_LOGGEDIN:
			{
				uint32_t ServerID;
				SS >> ServerID;
				m_BNETs.insert( ServerID );
			}
			break;

		case MASL_PROTOCOL :: STM_SLAVE_DISCONNECTED:
			{
				uint32_t ServerID;
				SS >> ServerID;
				m_BNETs.erase( ServerID );
			}
			break;

		case MASL_PROTOCOL :: STM_GAME_HOSTED:
			{
				m_GameInLobby = true;

				uint32_t GameType;
				uint32_t CreatorServerID;
				uint32_t GameID;
				uint32_t GameState;
				uint32_t MapSlots;
				string OwnerName;
				string GameName;
				string CFGFile;

				SS >> GameType;
				SS >> CreatorServerID;
				SS >> GameID;
				SS >> GameState;
				MASL_PROTOCOL :: SSReadString( SS, OwnerName );
				transform( OwnerName.begin( ), OwnerName.end( ), OwnerName.begin( ), (int(*)(int))tolower );
				m_LobbyCreator = OwnerName;
				SS >> MapSlots;
				getline( SS, GameName );

				string :: size_type Start1 = GameName.find_first_not_of( " " );

				if( Start1 != string :: npos )
					GameName = GameName.substr( Start1 );

				uint32_t pos;
				if ( GameName.find( "</" ) == 0 && ( pos = GameName.find( "/>" ) ) != string::npos )
				{
					CFGFile = GameName.substr( 2, pos - 2 );
					GameName.erase( 0, pos + 2 );
				}

				string :: size_type Start2 = GameName.find_first_not_of( " " );

				if( Start2 != string :: npos )
					GameName = GameName.substr( Start2 );

				m_Manager->m_RemoteGamesInLobby.push_back( new CRemoteGame( m_SlaveBotID, GameID, CreatorServerID, GameState, OwnerName, MapSlots, GameName, CFGFile, GameType ) );

				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
				{
					if( (*i)->GetServerID( ) == CreatorServerID )
					{
						// note that we send this only on the creator server

						if( GameState == MASL_PROTOCOL :: GAME_PRIV )
							(*i)->SendChatCommand( m_GHost->m_Language->CreatingPrivateGame( GameName, OwnerName ), OwnerName );
						else if( GameState == MASL_PROTOCOL :: GAME_PUB )
							(*i)->SendChatCommand( m_GHost->m_Language->CreatingPublicGame( GameName, OwnerName ), OwnerName );

						break;
					}
				}
			}
			break;

		case MASL_PROTOCOL :: STM_GAME_INLOBBY:
			{
				uint32_t GameType;
				uint32_t CreatorServerID;
				uint32_t GameID;
				unsigned char GameState;
				uint32_t OccupiedSlots;
				uint32_t TotalSlots;
				string CreatorName;
				string OwnerName;
				uint32_t GameNameSize;
				string GameName;
				uint32_t MapNameSize;
				string MapName;

				SS >> GameType;
				SS >> CreatorServerID;
				SS >> GameID;
				SS >> GameState;
				SS >> OccupiedSlots;
				SS >> TotalSlots;
				
				MASL_PROTOCOL :: SSReadString( SS, CreatorName );
				transform( CreatorName.begin( ), CreatorName.end( ), CreatorName.begin( ), (int(*)(int))tolower );

				MASL_PROTOCOL :: SSReadString( SS, OwnerName );
				transform( OwnerName.begin( ), OwnerName.end( ), OwnerName.begin( ), (int(*)(int))tolower );
				
				SS >> GameNameSize;
				GameName = UTIL_SSRead( SS, 1, GameNameSize );

				SS >> MapNameSize;
				MapName = UTIL_SSRead( SS, 1, MapNameSize );

				CRemoteGame *Game = new CRemoteGame( m_SlaveBotID, GameID, CreatorServerID, GameState, OwnerName, TotalSlots, GameName, MapName, GameType );

				for( unsigned int i = 0; i < TotalSlots; ++i )
				{
					unsigned char SlotStatus;	// slot status (0 = open, 1 = closed, 2 = occupied)
					uint32_t ServerID;
					string Player;

					SS >> SlotStatus;

					DEBUG_Print("SlotStatus = " + UTIL_ToString( SlotStatus ) );

					if( SlotStatus == 2 )
					{
						SS >> ServerID;
						MASL_PROTOCOL :: SSReadString( SS, Player );
						transform( Player.begin( ), Player.end( ), Player.begin( ), (int(*)(int))tolower );

						DEBUG_Print("Player = " + Player );
						DEBUG_Print("ServerID = " + UTIL_ToString( ServerID ) );

						Game->SetGamePlayer( i + 1, Player );
					}
				}

				m_Manager->m_RemoteGamesInLobby.push_back( Game );

				m_GameInLobby = true;
				m_LobbyCreator = OwnerName;
			}
			break;

		case MASL_PROTOCOL :: STM_GAME_INPROGRESS:
			{
				uint32_t GameType;
				uint32_t CreatorServerID;
				uint32_t GameID;
				unsigned char GameState;
				uint32_t OccupiedSlots;
				uint32_t TotalSlots;
				string CreatorName;
				string OwnerName;
				uint32_t GameNameSize;
				string GameName;
				uint32_t MapNameSize;
				string MapName;

				SS >> GameType;
				SS >> CreatorServerID;
				SS >> GameID;
				SS >> GameState;
				SS >> OccupiedSlots;
				SS >> TotalSlots;
				
				MASL_PROTOCOL :: SSReadString( SS, CreatorName );
				transform( CreatorName.begin( ), CreatorName.end( ), CreatorName.begin( ), (int(*)(int))tolower );

				MASL_PROTOCOL :: SSReadString( SS, OwnerName );
				transform( OwnerName.begin( ), OwnerName.end( ), OwnerName.begin( ), (int(*)(int))tolower );
				
				SS >> GameNameSize;
				GameName = UTIL_SSRead( SS, 1, GameNameSize );

				SS >> MapNameSize;
				MapName = UTIL_SSRead( SS, 1, MapNameSize );

				CRemoteGame *Game = new CRemoteGame( m_SlaveBotID, GameID, CreatorServerID, GameState, OwnerName, TotalSlots, GameName, MapName, GameType );

				for( unsigned int i = 0; i < TotalSlots; ++i )
				{
					unsigned char SlotStatus;	// slot status (0 = open, 1 = closed, 2 = occupied)
					uint32_t ServerID;
					string Player;

					SS >> SlotStatus;

					DEBUG_Print("SlotStatus = " + UTIL_ToString( SlotStatus ) );

					if( SlotStatus == 2 )
					{
						SS >> ServerID;
						MASL_PROTOCOL :: SSReadString( SS, Player );
						transform( Player.begin( ), Player.end( ), Player.begin( ), (int(*)(int))tolower );

						DEBUG_Print("Player = " + Player );
						DEBUG_Print("ServerID = " + UTIL_ToString( ServerID ) );

						Game->SetGamePlayer( i + 1, Player );
					}
				}

				m_Manager->m_RemoteGames.push_back( Game );

				/*CRemoteGame *RemoteGame;

				uint32_t GameID;
				uint32_t GameState;
				uint32_t GameStatus;			// 1 = in lobby, 2 = loading, 3 = loaded
				uint32_t GameDuration;			// if in lobby or loading duration(seconds) will be sent after status
				string CreatorName;
				string CreatorServer;
				string OwnerName;
				uint32_t GameNameSize;
				string GameName;
				uint32_t SlotsOccupied;
				uint32_t MapLocalPathSize;
				string MapLocalPath;			// map name
				uint32_t SlotsSize;				// number of map slots

				SS >> GameID;
				SS >> GameState;
				SS >> GameStatus;
				SS >> GameDuration;
				MASL_PROTOCOL :: SSReadString( SS, CreatorName );
				transform( CreatorName.begin( ), CreatorName.end( ), CreatorName.begin( ), (int(*)(int))tolower );
				MASL_PROTOCOL :: SSReadString( SS, CreatorServer );
				MASL_PROTOCOL :: SSReadString( SS, OwnerName );
				transform( OwnerName.begin( ), OwnerName.end( ), OwnerName.begin( ), (int(*)(int))tolower );

				CONSOLE_Print("GameID = " + UTIL_ToString( GameID ) );
				CONSOLE_Print("GameState = " + UTIL_ToString( GameState ) );
				CONSOLE_Print("GameStatus = " + UTIL_ToString( GameStatus ) );
				CONSOLE_Print("GameDuration = " + UTIL_ToString( GameDuration ) );
				CONSOLE_Print("CreatorName = " + CreatorName );
				CONSOLE_Print("CreatorServer = " + CreatorServer );
				CONSOLE_Print("OwnerName = " + OwnerName );

				SS >> GameNameSize;
				GameName = UTIL_SSRead( SS, 1, GameNameSize );

				CONSOLE_Print("GameNameSize = " + UTIL_ToString( GameNameSize ) );
				CONSOLE_Print("GameName = " + GameName );
				
				SS >> SlotsOccupied;

				CONSOLE_Print("SlotsOccupied = " + UTIL_ToString( SlotsOccupied ) );

				SS >> MapLocalPathSize;
				MapLocalPath = UTIL_SSRead( SS, 1, MapLocalPathSize );

				CONSOLE_Print("MapLocalPathSize = " + UTIL_ToString( MapLocalPathSize ) );
				CONSOLE_Print("MapLocalPath = " + MapLocalPath );

				SS >> SlotsSize;

				CONSOLE_Print("SlotsSize = " + UTIL_ToString( SlotsSize ) );

				RemoteGame = new CRemoteGame( m_SlaveBotID, GameID, CreatorServer, GameState, OwnerName, SlotsSize, GameName, MapLocalPath );

				for( int i = 0; i < SlotsSize; ++i )
				{
					uint32_t SlotStatus;	// slot status (0 = open, 1 = closed, 2 = occupied)
					string PlayerName;
					string SpoofedRealm;

					SS >> SlotStatus;

					CONSOLE_Print("SlotStatus = " + UTIL_ToString( SlotStatus ) );

					if( SlotStatus == 2 )
					{
						MASL_PROTOCOL :: SSReadString( SS, PlayerName );
						transform( PlayerName.begin( ), PlayerName.end( ), PlayerName.begin( ), (int(*)(int))tolower );
						MASL_PROTOCOL :: SSReadString( SS, SpoofedRealm );

						CONSOLE_Print("PlayerName = " + PlayerName );
						CONSOLE_Print("SpoofedRealm = " + SpoofedRealm );

						RemoteGame->SetGamePlayer( i + 1, PlayerName );
					}
				}

				m_Manager->m_RemoteGames.push_back( RemoteGame );*/
			}
			break;

		case MASL_PROTOCOL :: STM_GAME_UNHOSTED:
			{
				m_GameInLobby = false;
				m_LobbyCreator.clear( );

				uint32_t GameID;
				SS >> GameID;

				for( list<CRemoteGame *> :: iterator i = m_Manager->m_RemoteGamesInLobby.begin( ); i != m_Manager->m_RemoteGamesInLobby.end( ); ++i )
				{
					if( (*i)->GetBotID( ) == m_SlaveBotID && (*i)->GetGameID( ) == GameID )
					{
						delete *i;
						//i = m_GHost->m_RemoteGamesInLobby.erase( i );
						m_Manager->m_RemoteGamesInLobby.erase( i );
						break;
					}
				}
			}
			break;

		case MASL_PROTOCOL :: STM_GAME_STARTED:
			{
				m_GameInLobby = false;
				m_LobbyCreator.clear( );

				uint32_t GameType;
				uint32_t CreatorServerID;
				uint32_t GameID;
				uint32_t GameState;
				string OwnerName;
				uint32_t NumPlayers;
				string SlotInfo;
				string GameName;

				SS >> GameType;
				SS >> CreatorServerID;
				SS >> GameID;
				SS >> GameState;

				SS >> OwnerName;
				transform( OwnerName.begin( ), OwnerName.end( ), OwnerName.begin( ), (int(*)(int))tolower );

				SS >> NumPlayers;

				getline( SS, GameName );
				// GameName = "</" + SlotInfo + "/> " + GameName ...

				string :: size_type Start1 = GameName.find_first_not_of( " " );

				if( Start1 != string :: npos )
					GameName = GameName.substr( Start1 );

				uint32_t pos;
				if ( GameName.find( "</" ) == 0 && ( pos = GameName.find( "/>" ) ) != string::npos )
				{
					SlotInfo = GameName.substr( 2, pos - 2 );
					GameName.erase( 0, pos + 2 );
				}

				string :: size_type Start2 = GameName.find_first_not_of( " " );

				if( Start2 != string :: npos )
					GameName = GameName.substr( Start2 );

				string CFGFile = string( );

				for( list<CRemoteGame *> :: iterator i = m_Manager->m_RemoteGamesInLobby.begin( ); i != m_Manager->m_RemoteGamesInLobby.end( ); ++i )
				{
					if( (*i)->GetBotID( ) == m_SlaveBotID && (*i)->GetGameID( ) == GameID )
					{
						CFGFile = (*i)->GetCFGFile( );

						delete *i;
						//i = m_GHost->m_RemoteGamesInLobby.erase( i );
						m_Manager->m_RemoteGamesInLobby.erase( i );
						break;
					}
				}

				m_Manager->m_RemoteGames.push_back( new CRemoteGame( m_SlaveBotID, GameID, CreatorServerID, GameState, OwnerName, NumPlayers, SlotInfo, GameName, CFGFile, GameType ) );
			}
			break;

		case MASL_PROTOCOL :: STM_GAME_ISOVER:
			{
				uint32_t GameID;
				SS >> GameID;

				// remove the game from remote games

				for( list<CRemoteGame *> :: iterator i = m_Manager->m_RemoteGames.begin( ); i != m_Manager->m_RemoteGames.end( ); ++i )
				{
					if( (*i)->GetBotID( ) == m_SlaveBotID && (*i)->GetGameID( ) == GameID )
					{
						delete *i;
						//i = m_GHost->m_RemoteGames.erase( i );
						m_Manager->m_RemoteGames.erase( i );
						break;
					}
				}
			}
			break;

		case MASL_PROTOCOL :: STM_GAME_NAMECHANGED:
			{
				uint32_t GameID;
				uint32_t GameState;
				string GameName;

				SS >> GameID;
				SS >> GameState;
				getline( SS, GameName );

				string :: size_type Start = GameName.find_first_not_of( " " );

				if( Start != string :: npos )
					GameName = GameName.substr( Start );

				for( list<CRemoteGame *> :: iterator i = m_Manager->m_RemoteGamesInLobby.begin( ); i != m_Manager->m_RemoteGamesInLobby.end( ); ++i )
				{
					if( (*i)->GetBotID( ) == m_SlaveBotID && (*i)->GetGameID( ) == GameID )
					{
						(*i)->SetGameName( GameName );
						(*i)->SetGameState( GameState );
					}
				}
			}
			break;

		case MASL_PROTOCOL :: STM_GAME_OWNERCHANGED:
			{
				uint32_t GameID;
				string GameOwner;

				SS >> GameID;
				SS >> GameOwner;

				for( list<CRemoteGame *> :: iterator i = m_Manager->m_RemoteGamesInLobby.begin( ); i != m_Manager->m_RemoteGamesInLobby.end( ); ++i )
				{
					if( (*i)->GetBotID( ) == m_SlaveBotID && (*i)->GetGameID( ) == GameID )
					{
						(*i)->SetOwnerName( GameOwner );
					}
				}
			}
			break;

		case MASL_PROTOCOL :: STM_GAME_PLAYERJOINEDLOBBY:
			{
				uint32_t GameID;
				string PlayerName;

				SS >> GameID;
				SS >> PlayerName;

				for( list<CRemoteGame *> :: iterator i = m_Manager->m_RemoteGamesInLobby.begin( ); i != m_Manager->m_RemoteGamesInLobby.end( ); ++i )
				{
					if( (*i)->GetBotID( ) == m_SlaveBotID && (*i)->GetGameID( ) == GameID )
					{
						(*i)->SetGamePlayer( PlayerName );
						break;
					}
				}
			}
			break;

		case MASL_PROTOCOL :: STM_GAME_PLAYERLEFTLOBBY:
			{
				uint32_t GameID;
				string PlayerName;

				SS >> GameID;
				SS >> PlayerName;

				for( list<CRemoteGame *> :: iterator i = m_Manager->m_RemoteGamesInLobby.begin( ); i != m_Manager->m_RemoteGamesInLobby.end( ); ++i )
				{
					if( (*i)->GetBotID( ) == m_SlaveBotID && (*i)->GetGameID( ) == GameID )
					{
						(*i)->DelGamePlayer( PlayerName );
						break;
					}
				}
			}
			break;

		case MASL_PROTOCOL :: STM_PLAYERLEAVE_LOBBY_INFORM:
			{
				string bnet;
				string name;
				uint16_t reason;
				SS >> bnet;
				SS >> name;
				SS >> reason;

				if (reason == MASL_PROTOCOL :: STM_PLAYERLEAVE_LOBBY_NOMAP)
				{
					m_GHost->GetBnetByName(bnet)->SendDelayedWhisper("Kicked for not having the map", name, 4000);
				}
				else if (reason == MASL_PROTOCOL :: STM_PLAYERLEAVE_LOBBY_WRONG_GPROXY)
				{
					m_GHost->GetBnetByName(bnet)->SendDelayedWhisper("Kicked for not having the correct gproxy version or no gproxy", name, 4000);
				}
				else if (reason == MASL_PROTOCOL :: STM_PLAYERLEAVE_LOBBY_BANNED)
				{
					m_GHost->GetBnetByName(bnet)->SendDelayedWhisper("Kicked for being banned on the bot", name, 4000);
				}
				else if (reason == MASL_PROTOCOL :: STM_PLAYERLEAVE_LOBBY_PING)
				{
					m_GHost->GetBnetByName(bnet)->SendDelayedWhisper("Kicked for too high ping", name, 4000);
				}
				else if (reason == MASL_PROTOCOL :: STM_PLAYERLEAVE_LOBBY_FROM)
				{
					m_GHost->GetBnetByName(bnet)->SendDelayedWhisper("Kicked for wrong country origin", name, 4000);
				}
				else if (reason == MASL_PROTOCOL :: STM_PLAYERLEAVE_LOBBY_OWNER_KICKED)
				{
					m_GHost->GetBnetByName(bnet)->SendDelayedWhisper("Kicked by the game host", name, 4000);
				}
				else if (reason == MASL_PROTOCOL :: STM_PLAYERLEAVE_LOBBY_NOSC)
				{
					m_GHost->GetBnetByName(bnet)->SendDelayedWhisper("Kicked for not spoofchecking in time", name, 4000);
				}
				else if (reason == MASL_PROTOCOL :: STM_PLAYERLEAVE_LOBBY_MAX_PSR)
				{
					m_GHost->GetBnetByName(bnet)->SendDelayedWhisper("Kicked for having too high PSR", name, 4000);
				}
				else if (reason == MASL_PROTOCOL :: STM_PLAYERLEAVE_LOBBY_MIN_PSR)
				{
					m_GHost->GetBnetByName(bnet)->SendDelayedWhisper("Kicked for having too low PSR", name, 4000);
				}
				else if (reason == MASL_PROTOCOL :: STM_PLAYERLEAVE_LOBBY_MIN_GAMES)
				{
					m_GHost->GetBnetByName(bnet)->SendDelayedWhisper("Kicked for having too low number of games", name, 4000);
				}
				else if (reason == MASL_PROTOCOL :: STM_PLAYERLEAVE_LOBBY_NORMAL)
				{
					//do nothing, normal leave or some other unprecedented reason
				}
			}
			break;
		
		case MASL_PROTOCOL :: STM_GAME_PLAYERLEFTGAME:
			{
				uint32_t GameID;
				string Name;

				SS >> GameID;
				SS >> Name;

				for( list<CRemoteGame *> :: iterator i = m_Manager->m_RemoteGames.begin( ); i != m_Manager->m_RemoteGames.end( ); ++i )
				{
					if( (*i)->GetBotID( ) == m_SlaveBotID && (*i)->GetGameID( ) == GameID )
					{
						(*i)->SetPlayerLeft( Name );
						break;
					}
				}
			}
			break;

		case MASL_PROTOCOL :: STM_ERROR_GAMEINLOBBY:
			{
				// what kind of comment is this
				// the slave we thougt is free is not free, set it's status to alive and try to host with another slave

				//m_GameInLobby = false;
				//m_LobbyCreator.clear( );
			}
			break;

		case MASL_PROTOCOL :: STM_ERROR_MAXGAMESREACHED:
			{
				m_GameInLobby = false;
				m_LobbyCreator.clear( );
			}
			break;

		case MASL_PROTOCOL :: STM_ERROR_GAMENAMEEXISTS:
			{
				m_GameInLobby = false;
				m_LobbyCreator.clear( );

				string GameName;
				getline( SS, GameName );

				string :: size_type Start = GameName.find_first_not_of( " " );

				if( Start != string :: npos )
					GameName = GameName.substr( Start );

				//m_GHost->m_BNETs[0]->QueueChatCommand( m_GHost->m_Language->UnableToCreateGameTryAnotherName( m_GHost->m_BNETs[0]->GetServer( ), GameName ) );
			}
			break;

		/*case MASL_PROTOCOL :: STM_USER_WASBANNED:
			{
				uint32_t Duration;
				string Victim;
				string Admin;
				string Reason;

				SS >> Duration;
				SS >> Victim;
				SS >> Admin;
				getline( SS, Reason );

				string :: size_type Start = Reason.find_first_not_of( " " );

				if( Start != string :: npos )
					Reason = Reason.substr( Start );

				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
				{
					if( (*i)->GetServer( ) == server )
					{
						(*i)->AddBan( Victim, string( ), string( ), Admin, Reason, Duration );
						break;
					}
				}
			}
			break;*/

		case MASL_PROTOCOL :: STM_USER_GETINFO:
			{
				string Name;
				string Server;
				uint32_t IP;

				SS >> Name;
				SS >> Server;
				SS >> IP;

				DEBUG_Print( "Name = " + Name );
				DEBUG_Print( "Server = " + Server );
				DEBUG_Print( "IP = " + UTIL_ToString( IP ) );

				CDBPlayer *Player = NULL;

				for( list<CPlayers *> :: iterator i = m_GHost->m_Players.begin( ); i != m_GHost->m_Players.end( ); ++i )
				{
					if( (*i)->GetServer( ) == Server )
					{
						Player = (*i)->GetPlayer( Name );
						break;
					}
				}

				stringstream UserInfo;

				if( Player )
				{
					if( Player->GetFrom( ) == "??" )
					{
						// player exists in cache but his from is unknown, query MySQL for new from

						m_GHost->m_FromChecks.push_back( m_GHost->m_DB->ThreadedFromCheck( m_SlaveBotID, Name, GetServerID( Server ), IP ) );
					}

					UserInfo << Server;
					UserInfo << " " << Name;
					UserInfo << " " << Player->GetFrom( );
					UserInfo << " " << Player->GetLongFrom( ).size( );
					UserInfo << " " << Player->GetLongFrom( );
					UserInfo << " " << Player->GetAccessLevel( );
					UserInfo << " " << ( Player->GetIsBanned( ) ? 1 : 0 );

					if( Player->GetGamePlayerSummary( ) )
						UserInfo << " " << 1;
					else
						UserInfo << " " << 0;

					CDBDotAPlayerSummary *DotAPlayerSummary = Player->GetDotAPlayerSummary( );
					if( DotAPlayerSummary )
					{
						UserInfo << " " << 1;
						UserInfo << " " << DotAPlayerSummary->GetTotalGames( );
						UserInfo << " " << DotAPlayerSummary->GetTotalWins( );
						UserInfo << " " << DotAPlayerSummary->GetTotalLosses( );
						UserInfo << " " << DotAPlayerSummary->GetTotalKills( );
						UserInfo << " " << DotAPlayerSummary->GetTotalDeaths( );
						UserInfo << " " << DotAPlayerSummary->GetTotalCreepKills( );
						UserInfo << " " << DotAPlayerSummary->GetTotalCreepDenies( );
						UserInfo << " " << DotAPlayerSummary->GetTotalAssists( );
						UserInfo << " " << DotAPlayerSummary->GetTotalNeutralKills( );
						UserInfo << " " << DotAPlayerSummary->GetRating( );
						UserInfo << " " << DotAPlayerSummary->GetHighestRating( );
					}
					else
						UserInfo << " " << 0;
				}
				else
				{
					// this is a new player, he has no cache info, query MySQL for from

					m_GHost->m_FromChecks.push_back( m_GHost->m_DB->ThreadedFromCheck( m_SlaveBotID, Name, GetServerID( Server ), IP ) );

					UserInfo << Server;				// player server
					MASL_PROTOCOL :: SSWriteString( UserInfo, Name );
					UserInfo << " " << "??";		// short from
					UserInfo << " " << 2;			// long from size
					UserInfo << " " << "??";		// long from
					UserInfo << " " << 1;			// access level
					UserInfo << " " << 0;			// is banned
					UserInfo << " " << 0;			// is game player summary sent
					UserInfo << " " << 0;			// is dota player summary sent
				}

				Send( MASL_PROTOCOL :: MTS_USER_GETINFO_RESPONSE, UserInfo.str( ) );
			}
			break;
		}

		delete Packet;
	}
}

bool CSlave :: CanHost(string server, uint32_t ghostGroup, string region)
{
	/*CONSOLE_Print( "bool CSlave :: CanHost( " + server + " )" );
	CONSOLE_Print( "m_BNETs.size( ) = " + UTIL_ToString( m_BNETs.size( ) ) );*/

	if( m_GameInLobby )
		return false;

	if( !LoggedIn( GetServerID( server ) ) )
		return false;

	if( m_GHostGroup != ghostGroup && m_GHostGroup != MASL_PROTOCOL :: GHOST_GROUP_ALL )
		return false;

	if (m_region != region) {
	    return false;
	}

	if (m_BeeingExchanged)
	{
		return false;
	}

	return true;
}

bool CSlave :: LoggedIn( uint32_t serverID )
{
	return m_BNETs.find( serverID ) != m_BNETs.end( );
}

uint32_t CSlave :: GetNumGames( )
{
	uint32_t NumGames = 0;

	for( list<CRemoteGame *> :: iterator i = m_Manager->m_RemoteGames.begin( ); i != m_Manager->m_RemoteGames.end( ); ++i )
	{
		if( (*i)->GetBotID( ) == m_SlaveBotID )
			++NumGames;
	}

	return NumGames;
}

void CSlave :: SendCreateGame( string creatorServer, uint32_t gameType, uint32_t gameState, string creatorName, string gameName, string mapName, bool observers )
{
	Send( MASL_PROTOCOL :: MTS_CREATE_GAME, creatorServer + " " + UTIL_ToString( gameType ) + " " + UTIL_ToString( gameState ) + " " + ( observers ? "1" : "0" ) + " " + creatorName + " " + UTIL_ToString( gameName.size( ) ) + " " + gameName + " " + UTIL_ToString( mapName.size( ) ) + " " + mapName );
}

/*void CSlave :: SendCreateDotAGame( string creatorServer, uint32_t gameState, string creatorName, string gameName )
{
	Send( MASL_PROTOCOL :: MTS_CREATE_DOTAGAME, creatorServer + " " + UTIL_ToString( gameState ) + " " + creatorName + " " + UTIL_ToString( gameName.size( ) ) + " " + gameName );
}

void CSlave :: SendCreateCustomGame( string creatorServer, uint32_t gameState, string creatorName, string gameName, string mapName )
{
	Send( MASL_PROTOCOL :: MTS_CREATE_RPGGAME, creatorServer + " " + UTIL_ToString( gameState ) + " " + creatorName + " " + UTIL_ToString( gameName.size( ) ) + " " + gameName + " " + UTIL_ToString( mapName.size( ) ) + " " + mapName );
}*/

void CSlave :: SendContributorOnlyMode( bool contributor_only_mode )
{
	stringstream SS;
	SS << contributor_only_mode;

	Send( MASL_PROTOCOL :: MTS_CONTRIBUTOR_ONLY_MODE, SS.str( ) );
}

void CSlave :: ReloadConfig()
{
	Send( MASL_PROTOCOL :: MTS_CONFIG_RELOAD);
}

void CSlave :: Send( uint16_t flag )
{
	BYTEARRAY Packet = UTIL_CreateByteArray( (uint32_t)0, true );
	BYTEARRAY Flag = UTIL_CreateByteArray( flag, true );
	UTIL_AppendByteArrayFast( Packet, Flag );

	DEBUG_Print( "[MASL: slave " + UTIL_ToString( m_SlaveBotID ) + "] queued to buffer packet " + MASL_PROTOCOL :: FlagToString( flag ) );

	m_SendBuffer += string( Packet.begin( ), Packet.end( ) );
}

void CSlave :: Send( uint16_t flag, string msg )
{
	BYTEARRAY Packet = UTIL_CreateByteArray( (uint32_t)msg.size( ), true );
	BYTEARRAY Flag = UTIL_CreateByteArray( flag, true );
	UTIL_AppendByteArrayFast( Packet, Flag );

	DEBUG_Print( "[MASL: slave " + UTIL_ToString( m_SlaveBotID ) + "] queued to buffer packet " + MASL_PROTOCOL :: FlagToString( flag ) + " [" + UTIL_ByteArrayToHexString( Packet ) + "][" + msg + "]" );

	m_SendBuffer += string( Packet.begin( ), Packet.end( ) );
	m_SendBuffer += msg;
}

//
// CManager
//

CManager :: CManager( CGHost *nGHost, string nBindAddress, uint16_t nPort, string nGameListFile, uint32_t nGameListRefreshInterval ) : m_GHost( nGHost ), m_BindAddress( nBindAddress ), m_Port( nPort ), m_GameListFile( nGameListFile ), m_GameListRefreshInterval( nGameListRefreshInterval )
{
	CONSOLE_Print( "[MASL] starting manager" );

	m_Acceptor = NULL;
	m_GameListLastRefreshTime = 0;
	m_LastSlaveID = 0;
	m_AcceptingNewConnections = false;

	/*CSlave* new_slave = new CSlave( m_GHost, this, ++m_LastSlaveID, io_service_ );

	acceptor_.async_accept( new_slave->socket( ), 
	boost::bind( &CManager::handle_accept, this, new_slave, boost::asio::placeholders::error ) );*/

	//start_accepting( );
}

void CManager :: start_accepting( )
{
	if( m_AcceptingNewConnections )
		return;
	
	boost::asio::ip::tcp::endpoint Endpoint;

	if( m_BindAddress.empty( ) )
		Endpoint = boost::asio::ip::tcp::endpoint( boost::asio::ip::tcp::v4( ), m_Port );
	else
		Endpoint = boost::asio::ip::tcp::endpoint( boost::asio::ip::address::from_string( m_BindAddress ), m_Port );

	CONSOLE_Print( "[MASL] listening on TCP port " + Endpoint.address( ).to_string( ) + ":" + UTIL_ToString( Endpoint.port( ) ) );

	//acceptor_( io_service_, boost::asio::ip::tcp::endpoint( boost::asio::ip::tcp::v4( ), nPort ) );
	m_Acceptor = new boost::asio::ip::tcp::acceptor( m_GHost->m_IOService, Endpoint );
	CSlave* new_slave = new CSlave( m_GHost, this, ++m_LastSlaveID, m_GHost->m_IOService );

	/*acceptor_.async_accept( new_slave->socket( ), 
		boost::bind( &CManager::handle_accept, this, new_slave, boost::asio::placeholders::error ) );*/

	m_Acceptor->async_accept( new_slave->socket( ), 
		boost::bind( &CManager::handle_accept, this, new_slave, boost::asio::placeholders::error ) );

	m_AcceptingNewConnections = true;
}

void CManager :: handle_accept( CSlave* new_slave, const boost::system::error_code& error )
{
	DEBUG_Print( "CManager :: handle_accept()" );

	if( !error )
	{
		new_slave->start( );
		m_ConnectingSlaves.push_back( new_slave );

		new_slave = new CSlave( m_GHost, this, ++m_LastSlaveID, m_GHost->m_IOService );

		m_Acceptor->async_accept( new_slave->socket( ), 
			boost::bind( &CManager::handle_accept, this, new_slave, boost::asio::placeholders::error ) );
	}
	else
	{
		//CONSOLE_Print( "[MASL] error in \"CManager :: handle_accept\"" );
		CONSOLE_Print( "[MASL] error in \"CManager :: handle_accept\", error " + UTIL_ToString( error.value( ) ) + " [" + error.message( ) + "]" );
		delete new_slave;
	}
}

void CManager :: Update( )
{
	for( list<CSlave *> :: iterator i = m_Slaves.begin( ); i != m_Slaves.end( ); )
	{
		if( (*i)->Update( ) )
		{
			delete *i;
			i = m_Slaves.erase( i );
		}
		else
			++i;
	}

	for( list<CSlave *> :: iterator i = m_ConnectingSlaves.begin( ); i != m_ConnectingSlaves.end( ); )
	{
		if( (*i)->Update( ) )
		{
			delete *i;
			i = m_ConnectingSlaves.erase( i );
		}
		else
		{
			if( (*i)->GetProtocolInitialized( ) )
			{
				m_Slaves.push_back( *i );
				i = m_ConnectingSlaves.erase( i );
			}
			else
				++i;
		}
	}

	// create games from the queue if there is a slave bot available

	if( m_QueuedGames.size( ) )
	{
		//CONSOLE_Print( "m_QueuedGames.size( ) = " + UTIL_ToString( m_QueuedGames.size( ) ) );

		CQueuedGame *QueuedGame = m_QueuedGames.back( );
		CSlave *Slave = NULL;

		if( QueuedGame->GetAccessLevel( ) > 100 )
			Slave = GetAvailableSlave( QueuedGame->GetCreatorServer( ), QueuedGame->GetGHostGroup( ), false, QueuedGame->GetRegion() );
		else
			Slave = GetAvailableSlave( QueuedGame->GetCreatorServer( ), QueuedGame->GetGHostGroup( ), true, QueuedGame->GetRegion() );

		if( Slave )
		{
			/*CONSOLE_Print( "Slave->GetGameInLobby( ) = " + ( Slave->GetGameInLobby( ) ? string( "true" ) : string( "false" ) ) );
			CONSOLE_Print( "Slave->GetID( ) = " + UTIL_ToString( Slave->GetID( ) ) );
			CONSOLE_Print( "Slave->GetLobbyCreator( ) = " + Slave->GetLobbyCreator( ) );
			CONSOLE_Print( "QueuedGame->GetCreatorServer( ) = " + QueuedGame->GetCreatorServer( ) );
			CONSOLE_Print( "QueuedGame->GetGameType( ) = " + UTIL_ToString( QueuedGame->GetGameType( ) ) );
			CONSOLE_Print( "QueuedGame->GetCreatorName( ) = " + QueuedGame->GetCreatorName( ) );
			CONSOLE_Print( "QueuedGame->GetGameName( ) = " + QueuedGame->GetGameName( ) );
			CONSOLE_Print( "QueuedGame->GetMap( ) = " + QueuedGame->GetMap( ) );*/

			//Slave->SetCanHost( false );
			Slave->SetGameInLobby( true );
			Slave->SetLobbyCreator( QueuedGame->GetCreatorName( ) );

			/*if( QueuedGame->GetMap( ).empty( ) )
				Slave->SendCreateDotAGame( QueuedGame->GetCreatorServer( ), QueuedGame->GetGameState( ), QueuedGame->GetCreatorName( ), QueuedGame->GetGameName( ) );
			else
				Slave->SendCreateCustomGame( QueuedGame->GetCreatorServer( ), QueuedGame->GetGameState( ), QueuedGame->GetCreatorName( ), QueuedGame->GetGameName( ), QueuedGame->GetMap( ) );*/

			Slave->SendCreateGame( QueuedGame->GetCreatorServer( ), QueuedGame->GetGameType( ), QueuedGame->GetGameState( ), QueuedGame->GetCreatorName( ), QueuedGame->GetGameName( ), QueuedGame->GetMap( ), QueuedGame->GetObservers( ) );

			m_QueuedGames.pop_back( );
		}
	}

	// create a game list file

	if( !m_GameListFile.empty( ) && ( GetTime( ) - m_GameListLastRefreshTime ) > m_GameListRefreshInterval )
	{
		string Players;
		string Games;

		for( list<CRemoteGame *> :: iterator i = m_RemoteGames.begin( ); i != m_RemoteGames.end( ); ++i )
		{
			CRemoteGamePlayer *Player = NULL;

			for( uint32_t j = 1; j < 12; ++j )
			{
				Player = (*i)->GetGamePlayer( j );

				if( Player && Player->GetLeftTime( ) == 0 )
					Players += ",\"" + Player->GetName( ) + "\":{\"bot_id\":" + UTIL_ToString( (*i)->GetBotID( ) ) + ",\"game_id\":" + UTIL_ToString( (*i)->GetGameID( ) ) + "}";
			}

			string GameName = (*i)->GetGameName( );
			UTIL_Replace( GameName, "\"", "\\\"" );

			Games += ",\"" + UTIL_ToString( (*i)->GetBotID( ) ) + "_" + UTIL_ToString( (*i)->GetGameID( ) ) + "\":{\"in_lobby\":0,\"game_name\":\"" + GameName + "\"}";
		}

		for( list<CRemoteGame *> :: iterator i = m_RemoteGamesInLobby.begin( ); i != m_RemoteGamesInLobby.end( ); ++i )
		{
			CRemoteGamePlayer *Player = NULL;

			for( uint32_t j = 1; j < 12; ++j )
			{
				Player = (*i)->GetGamePlayer( j );

				if( Player )
					Players += ",\"" + Player->GetName( ) + "\":{\"bot_id\":" + UTIL_ToString( (*i)->GetBotID( ) ) + ",\"game_id\":" + UTIL_ToString( (*i)->GetGameID( ) ) + "}";
			}

			string GameName = (*i)->GetGameName( );
			UTIL_Replace( GameName, "\"", "\\\"" );

			Games += ",\"" + UTIL_ToString( (*i)->GetBotID( ) ) + "_" + UTIL_ToString( (*i)->GetGameID( ) ) + "\":{\"in_lobby\":1,\"game_name\":\"" + (*i)->GetGameName( ) + "\"}";
		}

		// remove the first comma

		if( !Players.empty( ) )
			Players.erase( 0, 1 );

		Players = "GL_players={" + Players + "};";

		if( !Games.empty( ) )
			Games.erase( 0, 1 );

		Games = "GL_games={" + Games + "}";

		ofstream GameList;
		GameList.open( m_GameListFile.c_str( ), ios::trunc );

		if( !GameList.fail( ) )
		{
			GameList << Players;
			GameList << Games;
			GameList.close( );
		}

		m_GameListLastRefreshTime = GetTime( );
	}
}

CQueuedGame *CManager :: GetQueuedGame( uint32_t gameNumber )
{
	gameNumber = m_QueuedGames.size( ) - gameNumber;

	if( gameNumber < m_QueuedGames.size( ) )
		return m_QueuedGames[gameNumber];

	return NULL;
}

uint32_t CManager :: QueueGameCount( )
{
	return m_QueuedGames.size( );
}

uint32_t CManager :: PositionInQueue( CBNET *creatorServer, string creatorName )
{
	transform( creatorName.begin( ), creatorName.end( ), creatorName.begin( ), (int(*)(int))tolower );

	uint32_t j = 0;

	for( vector<CQueuedGame *> :: iterator i = m_QueuedGames.begin( ); i != m_QueuedGames.end( ); i++ )
	{
		if( (*i)->GetCreatorName( ) == creatorName && (*i)->GetCreatorServer( ) == creatorServer->GetServer( ) )
			return m_QueuedGames.size( ) - j;

		j++;
	}

	return 0;
}

bool CManager :: HasQueuedGame( CBNET *creatorServer, string creatorName )
{
	transform( creatorName.begin( ), creatorName.end( ), creatorName.begin( ), (int(*)(int))tolower );

	for( vector<CQueuedGame *> :: iterator i = m_QueuedGames.begin( ); i != m_QueuedGames.end( ); i++ )
	{
		if( (*i)->GetCreatorName( ) == creatorName && (*i)->GetCreatorServer( ) == creatorServer->GetServer( ) )
			return true;
	}

	return false;
}

uint32_t CManager :: HasQueuedGameFromNamePartial( CBNET *creatorServer, string creatorName, CQueuedGame **game )
{
	transform( creatorName.begin( ), creatorName.end( ), creatorName.begin( ), (int(*)(int))tolower );
	uint32_t Matches = 0;
	*game = NULL;

	// try to match each player with the passed string (e.g. "Varlock" would be matched with "lock")

	for( vector<CQueuedGame *> :: iterator i = m_QueuedGames.begin( ); i != m_QueuedGames.end( ); i++ )
	{
		string TestName = (*i)->GetCreatorName( );
		transform( TestName.begin( ), TestName.end( ), TestName.begin( ), (int(*)(int))tolower );

		if( TestName.find( creatorName ) != string :: npos )
		{
			Matches++;
			*game = *i;

			// if the name matches exactly stop any further matching

			if( TestName == creatorName )
			{
				Matches = 1;
				break;
			}
		}
	}

	return Matches;
}

void CManager :: UnqueueGame( CBNET *creatorServer, string creatorName )
{
	transform( creatorName.begin( ), creatorName.end( ), creatorName.begin( ), (int(*)(int))tolower );

	for( vector<CQueuedGame *> :: iterator i = m_QueuedGames.begin( ); i != m_QueuedGames.end( ); )
	{
		if( (*i)->GetCreatorName( ) == creatorName && (*i)->GetCreatorServer( ) == creatorServer->GetServer( ) )
			i = m_QueuedGames.erase( i );
		else
			i++;
	}
}

void CManager :: DeleteQueue( )
{
	m_QueuedGames.clear( );
}

void CManager :: QueueGame( CBNET *creatorServer, uint32_t gameType, uint32_t gameState, string creatorName, uint32_t accessLevel, string gameName, string map, bool observers, bool queue, uint32_t ghostGroup, string region )
{
	transform( creatorName.begin( ), creatorName.end( ), creatorName.begin( ), (int(*)(int))tolower );

	// check if user is contributor and update his access level

	if( creatorServer->IsContributor( creatorName ) && accessLevel < 11 )
		accessLevel = 11;

	// check if the bot is in contributor only mode, only contributors and admins can host

	if( m_GHost->m_ContributorOnlyMode && !creatorServer->IsContributor( creatorName ) && !( accessLevel > 100 ) )
	{
		creatorServer->SendChatCommand( "Unable to create game, only contributors can host at the moment.", creatorName );
		creatorServer->SendChatCommand( "Visit dota.eurobattle.net for more info.", creatorName );
		return;
	}

	// check if user already has game hosted

	for( list<CSlave *> :: iterator i = m_Slaves.begin( ); i != m_Slaves.end( ); ++i )
	{
		if( (*i)->GetLobbyCreator( ) == creatorName )
		{
			creatorServer->SendChatCommand( m_GHost->m_Language->UnhostGameInLobbyBeforeHostingAgain( ), creatorName );
			return;
		}
	}

	// check if user already has game in queue, if true update that game with new game info

	for( vector<CQueuedGame *> :: iterator i = m_QueuedGames.begin( ); i != m_QueuedGames.end( ); ++i )
	{
		if( (*i)->GetCreatorName( ) == creatorName && (*i)->GetCreatorServer( ) == creatorServer->GetServer( ) )
		{
			(*i)->SetGameName( gameName );
			(*i)->SetMap( map );
			(*i)->SetAccessLevel( accessLevel );
			(*i)->SetGameState( gameState );
			
			// user might have new access level so sort games again

			sort( m_QueuedGames.begin( ), m_QueuedGames.end( ), CQueuedGameSortAscByAccessLevelAndQueuedTime( ) );

			creatorServer->SendChatCommand( "Your game in queue was updated, you are on position #" + UTIL_ToString( PositionInQueue( creatorServer, creatorName ) ) + ".", creatorName );

			return;
		}
	}

	// if queue is false for some reason we will only create if there's a slave bot available and not queue the game

	if( !queue )
	{
		CSlave *Slave = NULL;

		if( accessLevel > 100 )
			Slave = GetAvailableSlave( creatorServer->GetServer( ), ghostGroup, false, region );
		else
			Slave = GetAvailableSlave( creatorServer->GetServer( ), ghostGroup, true, region );

		if( Slave )
		{
			//Slave->SetCanHost( false );
			Slave->SetGameInLobby( true );
			Slave->SetLobbyCreator( creatorName );

			/*if( map.empty( ) )
				Slave->Send( MASL_PROTOCOL :: MTS_CREATE_DOTAGAME, creatorServer->GetServer( ) + " " + UTIL_ToString( gameState ) + " " + creatorName + " " + gameName );
			else
				Slave->Send( MASL_PROTOCOL :: MTS_CREATE_RPGGAME, creatorServer->GetServer( ) + " " + UTIL_ToString( gameState ) + " " + creatorName + " </" + map + "/> " + gameName );*/

			/*if( map.empty( ) )
				Slave->SendCreateDotAGame( creatorServer->GetServer( ), gameState, creatorName, gameName );
			else
				Slave->SendCreateCustomGame( creatorServer->GetServer( ), gameState, creatorName, gameName, map );*/

			Slave->SendCreateGame( creatorServer->GetServer( ), gameType, gameState, creatorName, gameName, map, observers );
		}
		else
			creatorServer->SendChatCommand( m_GHost->m_Language->NoBotsAvailableAtTheMoment( ), creatorName );

		return;
	}

	if( m_QueuedGames.size( ) > 0 )
	{
		m_QueuedGames.push_back( new CQueuedGame( creatorName, creatorServer->GetServer( ), gameName, map, accessLevel, gameState, gameType, observers, ghostGroup, region ) );
		sort( m_QueuedGames.begin( ), m_QueuedGames.end( ), CQueuedGameSortAscByAccessLevelAndQueuedTime( ) );

		uint32_t di = m_QueuedGames.size( );

		for( vector<CQueuedGame *> :: iterator i = m_QueuedGames.begin( ); i != m_QueuedGames.end( ); ++i )
		{
			//CONSOLE_Print( "[QUEUE DEBUG] " + UTIL_ToString( di ) + ". " + (*i)->GetCreatorName( ) + " " + UTIL_ToString( (*i)->GetAccessLevel( ) ) + " " + UTIL_ToString( (*i)->GetQueuedTime( ) ) );
			
			di--;
		}

		uint32_t j = 0;

		for( vector<CQueuedGame *> :: iterator i = m_QueuedGames.begin( ); i != m_QueuedGames.end( ); ++i )
		{
			if( (*i)->GetCreatorName( ) == creatorName && (*i)->GetCreatorServer( ) == creatorServer->GetServer( ) )
			{
				creatorServer->SendChatCommand( "Your game has been queued on position #" + UTIL_ToString( m_QueuedGames.size( ) - j ), creatorName );
				creatorServer->SendChatCommand( "Whisper bot with !q to check the game queue", creatorName );
				break;
			}

			j++;
		}
	}
	else
	{
		CSlave *Slave = NULL;

		if( accessLevel > 100 )
			Slave = GetAvailableSlave( creatorServer->GetServer( ), ghostGroup, false, region );
		else
			Slave = GetAvailableSlave( creatorServer->GetServer( ), ghostGroup, true, region );

		if( Slave )
		{
			//Slave->SetCanHost( false );
			Slave->SetGameInLobby( true );
			Slave->SetLobbyCreator( creatorName );

			/*if( map.empty( ) )
				Slave->Send( MASL_PROTOCOL :: MTS_CREATE_DOTAGAME, creatorServer->GetServer( ) + " " + UTIL_ToString( gameState ) + " " + creatorName + " " + gameName );
			else
				Slave->Send( MASL_PROTOCOL :: MTS_CREATE_RPGGAME, creatorServer->GetServer( ) + " " + UTIL_ToString( gameState ) + " " + creatorName + " </" + map + "/> " + gameName );*/

			/*if( map.empty( ) )
				Slave->SendCreateDotAGame( creatorServer->GetServer( ), gameState, creatorName, gameName );
			else
				Slave->SendCreateCustomGame( creatorServer->GetServer( ), gameState, creatorName, gameName, map );*/

			Slave->SendCreateGame( creatorServer->GetServer( ), gameType, gameState, creatorName, gameName, map, observers );
		}
		else
		{
			m_QueuedGames.push_back( new CQueuedGame( creatorName, creatorServer->GetServer( ), gameName, map, accessLevel, gameState, gameType, observers, ghostGroup, region ) );
			sort( m_QueuedGames.begin( ), m_QueuedGames.end( ), CQueuedGameSortAscByAccessLevelAndQueuedTime( ) );

			uint32_t j = 0;

			for( vector<CQueuedGame *> :: iterator i = m_QueuedGames.begin( ); i != m_QueuedGames.end( ); ++i )
			{
				if( (*i)->GetCreatorName( ) == creatorName && (*i)->GetCreatorServer( ) == creatorServer->GetServer( ) )
				{
					creatorServer->SendChatCommand( "Your game has been queued on position #" + UTIL_ToString( m_QueuedGames.size( ) - j ), creatorName );
					creatorServer->SendChatCommand( "Whisper bot with !q to check the game queue", creatorName );
					break;
				}

				j++;
			}
		}
	}
}

void CManager :: EchoPushPlayer( CBNET *creatorServer, string pusher, string creatorName, int32_t howMuch )
{
	/*
	string Pusher = pusher;
	string CreatorName = creatorName;
	transform( Pusher.begin( ), Pusher.end( ), Pusher.begin( ), (int(*)(int))tolower );
	transform( CreatorName.begin( ), CreatorName.end( ), CreatorName.begin( ), (int(*)(int))tolower );

	if( m_QueuedGames.size( ) > 0 )
	{
		CQueuedGame *LastMatch = NULL;
		uint32_t Matches = HasQueuedGameFromNamePartial( creatorServer, CreatorName, &LastMatch );

		if( Matches == 0 )
			creatorServer->SendChatCommand( "No matches found in queue for [" + creatorName + "]." );
		else if( Matches == 1 )
		{
			string Player = LastMatch->GetCreatorName( );
			uint32_t NewTime = LastMatch->GetQueuedTime( );

			if( howMuch > 0 )
			{
				uint32_t Position = 0;

				for( vector<CQueuedGame *> :: iterator i = m_QueuedGames.begin( ); i != m_QueuedGames.end( ); i++ )
				{
					if( Position )
					{
						NewTime = (*i)->GetQueuedTime( ) - 1;
						Position++;
					}

					if( Position > howMuch )
						break;

					if( (*i)->GetCreatorName( ) == Player )
						Position = 1;
				}
			}

			if( howMuch < 0 )
			{
				uint32_t Position = 0;

				for( vector<CQueuedGame *> :: iterator i = m_QueuedGames.begin( ); i != m_QueuedGames.end( ); i++ )
				{
					if( Position )
					{
						NewTime = (*i)->GetQueuedTime( ) - 1;
						Position++;
					}

					if( Position > howMuch )
						break;

					if( (*i)->GetCreatorName( ) == Player )
						Position = 1;
				}
			}

			if( NewTime != LastMatch->GetQueuedTime( ) )
			{
				LastMatch->SetQueuedTime( NewTime );
				sort( m_QueuedGames.begin( ), m_QueuedGames.end( ), CQueuedGameSortAscByAccessLevelAndQueuedTime( ) );

				creatorServer->SendChatCommand(  );
			}
			else
			{
				
			}
		}
		else
			creatorServer->SendChatCommand( "Found more then one match in queue for [" + creatorName + "]." );
	}
	else
		creatorServer->SendChatCommand( "There are no games in queue." );
	*/
}

CRemoteGame *CManager :: GetGame( uint32_t gameNumber )
{
	if( gameNumber <= m_RemoteGames.size( ) )
	{
		uint32_t j = 0;

		for( list<CRemoteGame *> :: iterator i = m_RemoteGames.begin( ); i != m_RemoteGames.end( ); ++i )
		{
			++j;

			if( j == gameNumber )
				return *i;
		}
	}

	return NULL;
}

CRemoteGame *CManager :: GetLobby( uint32_t lobbyNumber )
{
	if( lobbyNumber <= m_RemoteGamesInLobby.size( ) )
	{
		uint32_t j = 0;

		for( list<CRemoteGame *> :: iterator i = m_RemoteGames.begin( ); i != m_RemoteGames.end( ); ++i )
		{
			++j;

			if( j == lobbyNumber )
				return *i;
		}
	}

	return NULL;
}

/*
string CManager :: GetGamesDescription( )
{
	uint32_t MaxLobbies = GetMaxLobbies( );
	uint32_t Lobbies = GetNumLobbies( );

	uint32_t MaxGames = GetMaxGames( );
	uint32_t Privs = 0;
	uint32_t Pubs = 0;

	for( vector<CRemoteGame *> :: iterator i = m_RemoteGames.begin( ); i != m_RemoteGames.end( ); i++ )
	{
		if( (*i)->GetGameState( ) == 16 )
			Pubs++;
		else
			Privs++;
	}

	if( Lobbies + Privs + Pubs )
		return m_GHost->m_Language->ThereAreGamesInProgress( UTIL_ToString( Privs + Pubs ), UTIL_ToString( MaxGames ), UTIL_ToString( Pubs ), UTIL_ToString( Privs ), UTIL_ToString( Lobbies ), UTIL_ToString( MaxLobbies ) );
	else
		return m_GHost->m_Language->ThereIsNoGamesInProgress( );
}
*/

string CManager :: GetGamesDescription( uint32_t serverID )
{
	uint32_t MaxLobbies = GetMaxLobbies( serverID );
	uint32_t Lobbies = GetNumLobbies( serverID );

	//uint32_t MaxGames = GetMaxGames( );
	// we can't host max games of ( bots * each bot max games ), there's lag
	uint32_t MaxGames = m_GHost->m_MaxGames;

	uint32_t LadderGames = 0;
	uint32_t CustomGames = 0;

	for( list<CRemoteGame *> :: iterator i = m_RemoteGames.begin( ); i != m_RemoteGames.end( ); ++i )
	{
		if( (*i)->GetGameType( ) == MASL_PROTOCOL::DB_CUSTOM_GAME || (*i)->GetGameType( ) == MASL_PROTOCOL::DB_CUSTOM_DOTA_GAME )
			++CustomGames;
		else
			++LadderGames;
	}

	/*if( Lobbies + LadderGames + CustomGames )
		return m_GHost->m_Language->ThereAreGamesInProgress( UTIL_ToString( LadderGames + CustomGames + Lobbies ), UTIL_ToString( MaxGames ), UTIL_ToString( LadderGames ), UTIL_ToString( CustomGames ), UTIL_ToString( Lobbies ), UTIL_ToString( MaxLobbies ) );
	else
		return m_GHost->m_Language->ThereIsNoGamesInProgress( );*/

	return m_GHost->m_Language->ThereAreGamesInProgress( UTIL_ToString( LadderGames + CustomGames + Lobbies ), UTIL_ToString( MaxGames ), UTIL_ToString( LadderGames ), UTIL_ToString( CustomGames ), UTIL_ToString( Lobbies ), UTIL_ToString( MaxLobbies ) );
}

uint32_t CManager :: GetMaxLobbies( uint32_t serverID )
{
	uint32_t MaxLobbies = 0;

	for( list<CSlave *> :: iterator i = m_Slaves.begin( ); i != m_Slaves.end( ); ++i )
	{
		if( (*i)->LoggedIn( serverID ) )
			++MaxLobbies;
	}

	return MaxLobbies;
}

uint32_t CManager :: GetNumLobbies( uint32_t serverID )
{
	uint32_t NumLobbies = 0;

	for( list<CSlave *> :: iterator i = m_Slaves.begin( ); i != m_Slaves.end( ); ++i )
	{
		if( (*i)->LoggedIn( serverID ) && (*i)->GetGameInLobby( ) )
			++NumLobbies;
	}

	return NumLobbies;
}

uint32_t CManager :: GetMaxGames( uint32_t serverID )
{
	uint32_t MaxGames = 0;

	for( list<CSlave *> :: iterator i = m_Slaves.begin( ); i != m_Slaves.end( ); ++i )
	{
		if( (*i)->LoggedIn( serverID ) )
			MaxGames += (*i)->GetMaxGames( );
	}

	return MaxGames;
}

string CManager :: GetLobbyDescription( string server, string lobbyNumber )
{
	uint32_t LobbyNumber = UTIL_ToUInt32( lobbyNumber );

	if( LobbyNumber <= m_RemoteGamesInLobby.size( ) )
	{
		uint32_t j = 0;

		for( list<CRemoteGame *> :: iterator i = m_RemoteGamesInLobby.begin( ); i != m_RemoteGamesInLobby.end( ); ++i )
		{
			if( LobbyNumber == ++j )
			{
				if( server == (*i)->GetCreatorServer( ) )
					return m_GHost->m_Language->RemoteGameInLobbyNumberIs( lobbyNumber, (*i)->GetDescription( ) );
				else
					return m_GHost->m_Language->RemoteGameInLobbyNumberIs( lobbyNumber, (*i)->GetDescription( ) + " : " + (*i)->GetCreatorServer( ) );
			}
		}
	}

	return m_GHost->m_Language->RemoteGameInLobbyNumberDoesntExist( lobbyNumber );
}


string CManager :: GetGameDescription( string server, string gameNumber )
{
	uint32_t GameNumber = UTIL_ToUInt32( gameNumber );

	if( GameNumber <= m_RemoteGames.size( ) )
	{
		uint32_t j = 0;

		for( list<CRemoteGame *> :: iterator i = m_RemoteGames.begin( ); i != m_RemoteGames.end( ); ++i )
		{
			if( GameNumber == ++j )
			{
				if( server == (*i)->GetCreatorServer( ) )
					return m_GHost->m_Language->RemoteGameNumberIs( gameNumber, (*i)->GetDescription( ) );
				else
					return m_GHost->m_Language->RemoteGameNumberIs( gameNumber, (*i)->GetDescription( ) + " : " + (*i)->GetCreatorServer( ) );
			}
		}
	}

	return m_GHost->m_Language->RemoteGameNumberDoesntExist( gameNumber );
}

string CManager :: GetUsersDescription( CBNET *server )
{
	uint32_t Games = 0;
	uint32_t Players = 0;

	for( list<CRemoteGame *> :: iterator i = m_RemoteGames.begin( ); i != m_RemoteGames.end( ); ++i )
	{
		Players += (*i)->GetNumPlayers( );
		++Games;
	}

	for( list<CRemoteGame *> :: iterator i = m_RemoteGamesInLobby.begin( ); i != m_RemoteGamesInLobby.end( ); ++i )
	{
		Players += (*i)->GetNumPlayers( );
		++Games;
	}
	
	return "There are " + UTIL_ToString( Players ) + " users in " + UTIL_ToString( Games ) + " games and " + UTIL_ToString( server->GetNumUsersInChannel( ) ) + " users in channel [" + server->GetCurrentChannel( ) + "].";
}

string CManager :: GetLobbyPlayerNames( string lobbyNumber )
{
	uint32_t LobbyNumber = UTIL_ToUInt32( lobbyNumber );

	if( LobbyNumber <= m_RemoteGamesInLobby.size( ) )
	{
		uint32_t j = 0;

		for( list<CRemoteGame *> :: iterator i = m_RemoteGamesInLobby.begin( ); i != m_RemoteGamesInLobby.end( ); ++i )
		{
			if( LobbyNumber == ++j )
				return (*i)->GetNames( );
		}
	}

	return m_GHost->m_Language->RemoteGameInLobbyNumberDoesntExist( lobbyNumber );
}

string CManager :: GetGamePlayerNames( string gameNumber )
{
	uint32_t GameNumber = UTIL_ToUInt32( gameNumber );

	if( GameNumber <= m_RemoteGames.size( ) )
	{
		uint32_t j = 0;

		for( list<CRemoteGame *> :: iterator i = m_RemoteGames.begin( ); i != m_RemoteGames.end( ); ++i )
		{
			if( GameNumber == ++j )
				return (*i)->GetNames( );
		}
	}

	return m_GHost->m_Language->RemoteGameNumberDoesntExist( gameNumber );
}

void CManager :: SendPlayerWhereDescription( CBNET *server, string user, string player )
{
	string Name = player;
	transform( Name.begin( ), Name.end( ), Name.begin( ), (int(*)(int))tolower );

	CRemoteGamePlayer *Player = NULL;
	uint32_t Counter = 1;
	bool PlayerLocated = false;

	for( list<CRemoteGame *> :: iterator i = m_RemoteGamesInLobby.begin( ); i != m_RemoteGamesInLobby.end( ); ++i )
	{
		Player = (*i)->GetGamePlayer( Name );

		if( Player != NULL )
		{
			PlayerLocated = true;

			server->SendChatCommand( "[" + player + "] is in lobby number " + UTIL_ToString( Counter ) + " [" + (*i)->GetGameName( ) + " : " + (*i)->GetOwnerName( ) + " : " + UTIL_ToString( (*i)->GetNumPlayers( ) ) + "/" + UTIL_ToString( (*i)->GetNumStartPlayers( ) ) + " : " + UTIL_ToString( ( GetTime( ) - (*i)->GetStartTime( ) ) / 60 ) + "m].", user );
			Player = NULL;
		}

		++Counter;
	}

	Counter = 1;

	for( list<CRemoteGame *> :: iterator i = m_RemoteGames.begin( ); i != m_RemoteGames.end( ); ++i )
	{
		Player = (*i)->GetGamePlayer( Name );

		if( Player != NULL )
		{
			PlayerLocated = true;

			if( Player->GetLeftTime( ) == 0 )
				server->SendChatCommand( "[" + player + "] is in game number " + UTIL_ToString( Counter ) + " [" + (*i)->GetGameName( ) + " : " + (*i)->GetOwnerName( ) + " : " + UTIL_ToString( (*i)->GetNumPlayers( ) ) + "/" + UTIL_ToString( (*i)->GetNumStartPlayers( ) ) + " : " + UTIL_ToString( ( GetTime( ) - (*i)->GetStartTime( ) ) / 60 ) + "m].", user );
			else
				server->SendChatCommand( "[" + player + "] left from game number " + UTIL_ToString( Counter ) + " [" + (*i)->GetGameName( ) + " : " + (*i)->GetOwnerName( ) + " : " + UTIL_ToString( (*i)->GetNumPlayers( ) ) + "/" + UTIL_ToString( (*i)->GetNumStartPlayers( ) ) + " : " + UTIL_ToString( ( GetTime( ) - (*i)->GetStartTime( ) ) / 60 ) + "m] at " + UTIL_ToString( Player->GetLeftTime( ) / 60 ) + "m.", user );

			Player = NULL;
		}

		++Counter;
	}

	if( server->IsInChannel( Name ) )
	{
		PlayerLocated = true;
		server->SendChatCommand( "[" + player + "] is in channel [" + server->GetCurrentChannel( ) + "].", user );
	}

	if( !PlayerLocated )
		server->SendChatCommand( "I don't know where [" + player + "] is.", user );
}

string CManager :: GetWherePlayer( string player )
{
	string Name = player;
	transform( Name.begin( ), Name.end( ), Name.begin( ), (int(*)(int))tolower );

	CRemoteGamePlayer *Player;
	uint32_t Counter = 1;

	for( list<CRemoteGame *> :: iterator i = m_RemoteGamesInLobby.begin( ); i != m_RemoteGamesInLobby.end( ); ++i )
	{
		Player = (*i)->GetGamePlayer( Name );
		if( Player != NULL && Player->GetLeftTime( ) == 0 )
			return "Player [" + player + "] is in lobby number " + UTIL_ToString( Counter ) + " [" + (*i)->GetGameName( ) + " : " + (*i)->GetOwnerName( ) + " : " + UTIL_ToString( (*i)->GetNumPlayers( ) ) + "/" + UTIL_ToString( (*i)->GetNumStartPlayers( ) ) + " : " + UTIL_ToString( ( GetTime( ) - (*i)->GetStartTime( ) ) / 60 ) + "m]";

		++Counter;
	}

	Counter = 1;

	for( list<CRemoteGame *> :: iterator i = m_RemoteGames.begin( ); i != m_RemoteGames.end( ); ++i )
	{
		Player = (*i)->GetGamePlayer( Name );
		if( Player != NULL && Player->GetLeftTime( ) == 0 )
			return "Player [" + player + "] is in game number " + UTIL_ToString( Counter ) + " [" + (*i)->GetGameName( ) + " : " + (*i)->GetOwnerName( ) + " : " + UTIL_ToString( (*i)->GetNumPlayers( ) ) + "/" + UTIL_ToString( (*i)->GetNumStartPlayers( ) ) + " : " + UTIL_ToString( ( GetTime( ) - (*i)->GetStartTime( ) ) / 60 ) + "m]";

		++Counter;
	}

	return "I don't know where [" + player + "] is";
}

CSlave *CManager :: GetAvailableSlave( string server, uint32_t ghostGroup, bool useTotalGamesLimit, string region )
{
	// we maybe don't want to use this restriction when admin or contributor wants to create a game

	if( useTotalGamesLimit )
	{
		uint32_t CurrentGamesNum = m_RemoteGames.size( ) + m_RemoteGamesInLobby.size( );

		if( CurrentGamesNum >= m_GHost->m_MaxGames )
			return NULL;
	}

	CSlave *Slave = NULL;

	for( list<CSlave *> :: iterator i = m_Slaves.begin( ); i != m_Slaves.end( ); ++i )
	{
		/*if( (*i)->CanHost( server ) )
			CONSOLE_Print( "(*i)->CanHost( " + server + " ) = yes" );
		else
			CONSOLE_Print( "(*i)->CanHost( " + server + " ) = no" );

		if( (*i)->GetGameInLobby( ) )
			CONSOLE_Print( "(*i)->GetGameInLobby( ) = yes" );
		else
			CONSOLE_Print( "(*i)->GetGameInLobby( ) = no" );

		CONSOLE_Print( "(*i)->GetLobbyCreator( ) = " + (*i)->GetLobbyCreator( ) );
		CONSOLE_Print( "(*i)->GetMaxGames( ) = " + UTIL_ToString( (*i)->GetMaxGames( ) ) );
		CONSOLE_Print( "(*i)->GetNumGames( ) = " + UTIL_ToString( (*i)->GetNumGames( ) ) );
		CONSOLE_Print( "(*i)->GetID( ) = " + (*i)->GetID( ) );*/

		if((*i)->CanHost( server, ghostGroup, region ))
		{
			if( (*i)->GetNumGames( ) == 0 )
			{
				Slave = *i;
				break;
			}

			if( !Slave )
			{
				Slave = *i;
				continue;
			}

			if( (*i)->GetNumGames( ) < Slave->GetNumGames( ) )
				Slave = *i;
		}
	}

	return Slave;
}

bool CManager::SoftDisableSlave(uint32_t slaveUsernameNumber)
{
	for (list<CSlave*> ::iterator i = m_Slaves.begin(); i != m_Slaves.end(); ++i)
	{
		if (slaveUsernameMatches(*i, slaveUsernameNumber))
		{
			CONSOLE_Print("[SLAVEMANAGER] Softly disabling slave " + UTIL_ToString(slaveUsernameNumber));
			(*i)->m_BeeingExchanged = true;
			return true;
		}
	}

	CONSOLE_Print("[SLAVEMANAGER] slave " + UTIL_ToString(slaveUsernameNumber)+" not found");
	return false;
}

void CManager::SoftDisableAllSlaves()
{
	for (list<CSlave*> ::iterator i = m_Slaves.begin(); i != m_Slaves.end(); ++i)
	{
		CONSOLE_Print("[SLAVEMANAGER] Softly disabling slave with LiveID " + UTIL_ToString((*i)->GetID()));
		(*i)->m_BeeingExchanged = true;
	}
}

bool CManager::SoftEnableSlave(uint32_t slaveUsernameNumber)
{
	for (list<CSlave*> ::iterator i = m_Slaves.begin(); i != m_Slaves.end(); ++i)
	{
		if (slaveUsernameMatches(*i, slaveUsernameNumber))
		{
			CONSOLE_Print("[SLAVEMANAGER] Softly enabling slave lagabuse.com." + UTIL_ToString(slaveUsernameNumber));
			(*i)->m_BeeingExchanged = false;
			return true;
		}
	}

	CONSOLE_Print("[SLAVEMANAGER] slave lagabuse.com." + UTIL_ToString(slaveUsernameNumber) + " not found");
	return false;
}

void CManager::SoftEnableAllSlaves()
{
	for (list<CSlave*> ::iterator i = m_Slaves.begin(); i != m_Slaves.end(); ++i)
	{
		CONSOLE_Print("[SLAVEMANAGER] Softly enabling slave with LiveID " + UTIL_ToString((*i)->GetID()));
		(*i)->m_BeeingExchanged = false;
	}
}

bool CManager::slaveUsernameMatches(CSlave* slave, uint32_t number) {
	for (map<uint32_t, string> ::iterator j = slave->m_SlaveNames.begin(); j != slave->m_SlaveNames.end(); ++j)
	{
		string botName = j->second;
		size_t lastIndex = botName.find_last_not_of("0123456789");
		if (lastIndex == string::npos) {
			continue;
		}
		string result = botName.substr(lastIndex + 1);
		
		uint32_t parsedNumberFromName = UTIL_ToUInt32(result);

		if (number == parsedNumberFromName)
		{
			return true;
		}
	}
	return false;
}

vector<string> CManager :: PrintSlaveInfo( )
{
	vector<string> InfoString;

	for ( list<CSlave *> :: iterator i = m_Slaves.begin( ); i != m_Slaves.end( ); ++i )
	{
		InfoString.push_back("Slave ID: " + UTIL_ToString( (*i)->GetID( )));
		InfoString.push_back("Region: " + (*i)->GetRegion());
		InfoString.push_back("Lobby: " + string( (*i)->GetGameInLobby( ) ? "true" : "false" ));
		InfoString.push_back("Lobby creator: " + (*i)->GetLobbyCreator( ));
		InfoString.push_back("Max games: " + UTIL_ToString( (*i)->GetMaxGames( ) ));
		InfoString.push_back("Number of games: " + UTIL_ToString( (*i)->GetNumGames( ) ));
		InfoString.push_back("Protocol init:" + string( (*i)->GetProtocolInitialized( ) ? "true" : "false" ));
		InfoString.push_back("Hosting enabled: " + string( (*i)->m_BeeingExchanged ? "false" : "true"));

		InfoString.push_back("BNET USERNAMES (BotID - BnetID - Username)");
		for (map<uint32_t, string> :: iterator j = (*i)->m_SlaveNames.begin(); j != (*i)->m_SlaveNames.end(); ++j)
		{
			InfoString.push_back(UTIL_ToString((*i)->GetID())+" "+UTIL_ToString(j->first)+" "+j->second+" ");
		}
		InfoString.push_back("========== ==========");
	}

	return InfoString;
}

vector<string> CManager :: PrintSlaveInfoLesser( )
{
	vector<string> InfoString;
	InfoString.push_back("BNET USERNAMES (BotID - BnetID - Username - region)");

	for ( list<CSlave *> :: iterator i = m_Slaves.begin( ); i != m_Slaves.end( ); ++i )
	{
		for (map<uint32_t, string> :: iterator j = (*i)->m_SlaveNames.begin(); j != (*i)->m_SlaveNames.end(); ++j)
		{
			InfoString.push_back(UTIL_ToString((*i)->GetID())+" "+UTIL_ToString(j->first)+" "+j->second+" "+(*i)->GetRegion());
		}
	}
	return InfoString;
}

void CManager :: ReloadSlaveConfig()
{
	for ( list<CSlave *> :: iterator i = m_Slaves.begin( ); i != m_Slaves.end( ); ++i )
	{
		(*i)->ReloadConfig();
	}
}

void CManager :: SendAll( uint16_t flag )
{
	for( list<CSlave *> :: iterator i = m_Slaves.begin( ); i != m_Slaves.end( ); ++i )
		(*i)->Send( flag );
}

void CManager :: SendAll( uint16_t flag, string msg )
{
	for( list<CSlave *> :: iterator i = m_Slaves.begin( ); i != m_Slaves.end( ); ++i )
		(*i)->Send( flag, msg );
}

void CManager :: SendTo( uint32_t slaveID, uint16_t flag )
{
	for( list<CSlave *> :: iterator i = m_Slaves.begin( ); i != m_Slaves.end( ); ++i )
	{
		if( (*i)->GetID( ) == slaveID )
		{
			(*i)->Send( flag );
			break;
		}
	}
}

void CManager :: SendTo( uint32_t slaveID, uint16_t flag, string msg )
{
	for( list<CSlave *> :: iterator i = m_Slaves.begin( ); i != m_Slaves.end( ); ++i )
	{
		if( (*i)->GetID( ) == slaveID )
		{
			(*i)->Send( flag, msg );
			break;
		}
	}
}

CWebManager :: CWebManager( CGHost *nGHost, string nBindAddress, uint16_t nPort ) : m_GHost( nGHost ), m_BindAddress( nBindAddress ), m_Port( nPort )
{
	CONSOLE_Print( "[WEBMANAGER] starting web manager" );
	CONSOLE_Print( "[WEBMANAGER] listening on UDP port " + m_BindAddress + ":" + UTIL_ToString( m_Port ) );

	m_UDPServer = new CUDPServer( );
	m_UDPServer->Bind( m_BindAddress, m_Port );
}

CWebManager :: ~CWebManager( )
{

}

void CWebManager :: SetFD( void *fd, void *send_fd, int *nfds )
{
	m_UDPServer->SetFD( (fd_set *)fd, (fd_set *)send_fd, nfds );
}

void CWebManager :: Update( void *fd )
{
	string Packet;
	sockaddr_in recvAddr;

	m_UDPServer->RecvFrom( (fd_set *)fd, &recvAddr, &Packet );

	if( !Packet.empty( ) )
	{
		CONSOLE_Print( "[MASL] recived web UDP packet [" + Packet + "]" );

		stringstream SS;
		SS << Packet;

		uint32_t PacketID = 0;
		SS >> PacketID;

		switch( PacketID )
		{
		case MASL_PROTOCOL :: WEB_UPDATE_BAN_GROUP:
			{
				string Name;
				uint32_t ServerID;
				uint32_t BanGroupID;
				uint32_t BanGroupDatetime;
				uint32_t BanGroupDuration;
				uint32_t BanGroupBanPoints;
				uint32_t BanGroupWarnPoints;

				SS >> Name;
				SS >> ServerID;
				SS >> BanGroupID;
				SS >> BanGroupDatetime;
				SS >> BanGroupDuration;
				SS >> BanGroupBanPoints;
				SS >> BanGroupWarnPoints;

				// update players cache

				for( list<CPlayers *> :: iterator i = m_GHost->m_Players.begin( ); i != m_GHost->m_Players.end( ); ++i )
				{
					if( (*i)->GetServerID( ) == ServerID )
					{
						CDBPlayer *Player = (*i)->GetPlayer( Name );

						if( Player )
							Player->SetBanGroupID( BanGroupID );
						else
							(*i)->AddPlayer( Name, new CDBPlayer( BanGroupID, 0, 1 ) );

						break;
					}
				}

				// update ban_group cache

				if( m_GHost->m_BanGroup.size( ) > BanGroupID )
				{
					if( m_GHost->m_BanGroup[BanGroupID] != NULL )
						delete m_GHost->m_BanGroup[BanGroupID];
				}
				else
					m_GHost->m_BanGroup.resize( BanGroupID + 1, NULL );

				m_GHost->m_BanGroup[BanGroupID] = new CDBBanGroup( BanGroupDatetime, BanGroupDuration, BanGroupBanPoints, BanGroupWarnPoints );
			}
			break;

		case MASL_PROTOCOL :: WEB_UPDATE_PLAYER:
			{
				string Name;
				uint32_t ServerID;
				uint32_t BotMembergroupID;

				SS >> Name;
				SS >> ServerID;
				SS >> BotMembergroupID;

				// update players cache

				for( list<CPlayers *> :: iterator i = m_GHost->m_Players.begin( ); i != m_GHost->m_Players.end( ); ++i )
				{
					if( (*i)->GetServerID( ) == ServerID )
					{
						CDBPlayer *Player = (*i)->GetPlayer( Name );

						if( Player )
							Player->SetAccessLevel( BotMembergroupID );
						else
							(*i)->AddPlayer( Name, new CDBPlayer( 0, 0, BotMembergroupID ) );

						break;
					}
				}
			}
			break;

		case MASL_PROTOCOL :: WEB_UPDATE_DIV1_STATS_DOTA:
			{
				uint32_t DIV1StatsDotAID;
				uint32_t Rating;
				uint32_t HighestRating;
				uint32_t Games;
				uint32_t GamesLeft;
				uint32_t EmGames;
				uint32_t GamesObserved;
				uint32_t Wins;
				uint32_t Loses;
				uint32_t Draws;
				uint32_t Kills;
				uint32_t Deaths;
				uint32_t Assists;
				uint32_t CreepKills;
				uint32_t CreepDenies;
				uint32_t NeutralKills;
				uint32_t TowerKills;
				uint32_t RaxKills;
				uint32_t CourierKills;

				SS >> DIV1StatsDotAID;
				SS >> Rating;
				SS >> HighestRating;
				SS >> Games;
				SS >> GamesLeft;
				SS >> EmGames;
				SS >> GamesObserved;
				SS >> Wins;
				SS >> Loses;
				SS >> Draws;
				SS >> Kills;
				SS >> Deaths;
				SS >> Assists;
				SS >> CreepKills;
				SS >> CreepDenies;
				SS >> NeutralKills;
				SS >> TowerKills;
				SS >> RaxKills;
				SS >> CourierKills;

				// update DIV1 stats cache

				if( m_GHost->m_DotAPlayerSummary.size( ) > DIV1StatsDotAID )
				{
					if( m_GHost->m_DotAPlayerSummary[DIV1StatsDotAID] != NULL )
						delete m_GHost->m_DotAPlayerSummary[DIV1StatsDotAID];
				}
				else
					m_GHost->m_DotAPlayerSummary.resize( DIV1StatsDotAID + 1, NULL );

				m_GHost->m_DotAPlayerSummary[DIV1StatsDotAID] = new CDBDotAPlayerSummary( Rating, HighestRating, Games, Wins, Loses, Kills, Deaths, CreepKills, CreepDenies, Assists, NeutralKills, GamesObserved );
			}
			break;
		}
	}
}
