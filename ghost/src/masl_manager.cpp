
#include "ghost.h"
#include "util.h"
#include "config.h"
#include "language.h"
#include "ghostdb.h"
#include "bnetprotocol.h"
#include "bnet.h"
#include "map.h"
#include "game_base.h"
#include "gameplayer.h"
#include "masl_manager.h"
#include "masl_protocol_2.h"

#include <boost/filesystem.hpp>

using namespace boost :: filesystem;

//
// CManager
//

CManager :: CManager( CGHost *nGHost, string nServer, uint16_t nPort ) : io_service_( ), socket_( io_service_ ), m_GHost( nGHost )
{
	/*boost::asio::io_service *io_service = new boost::asio::io_service( );
	io_service_ = *io_service;*/

	m_Server = nServer;
	m_Port = nPort;
	m_SlaveID = 0;
	m_LastConnectionAttemptTime = 0;
	m_AsyncConnectCompleted = true;
	m_Connected = false;
	m_Connecting = false;
	m_ProtocolInitialized = false;

	m_LastStatusSentTime = 0;
}

CManager :: ~CManager( )
{

}

void CManager :: handle_connect( const boost::system::error_code& error )
{
	if( !error )
	{
		CONSOLE_Print( "[MASL] connection to manager(" + socket_.remote_endpoint( ).address( ).to_string( ) + ":" + UTIL_ToString( socket_.remote_endpoint( ).port( ) ) + ") initiated" );
		m_Connected = true;
	}
	else
	{
		CONSOLE_Print( "[MASL] error in \"CManager :: handle_connect\"" );
		CONSOLE_Print( "[MASL] error " + UTIL_ToString( error.value( ) ) + " [" + error.message( ) + "]" );
		m_Connected = false;
		m_ProtocolInitialized = false;
	}

	m_AsyncConnectCompleted = true;
	m_AsyncReadCompleted = true;
	m_AsyncWriteCompleted = true;
}

void CManager :: handle_read( const boost::system::error_code& error, size_t bytes_transferred )
{
	if( !error )
	{
		m_RecvBuffer += string( data_, bytes_transferred );

		/*CONSOLE_Print( "void CSlave :: handle_read" );
		CONSOLE_Print( "data_ = " + string( data_, bytes_transferred ) );
		CONSOLE_Print( "m_RecvBuffer = " + m_RecvBuffer );*/

		// only start another async read when this one is finished

		m_AsyncReadCompleted = true;
	}
	else
	{
		CONSOLE_Print( "[MASL] error in \"CSlave :: handle_read\"" );
		CONSOLE_Print( "[MASL] error " + UTIL_ToString( error.value( ) ) + " [" + error.message( ) + "]" );
		m_Connected = false;
		m_ProtocolInitialized = false;
		m_TempSendBuffer = m_SendBuffer;
	}
}

void CManager :: handle_write( const boost::system::error_code& error, size_t bytes_transferred )
{
	if( !error )
	{
		m_SendBuffer.erase( 0, bytes_transferred );

		//CONSOLE_Print( "[MASL] \"CSlave :: handle_write\"" );

		// only start another async write when this one is finished

		m_AsyncWriteCompleted = true;
	}
	else
	{
		CONSOLE_Print( "[MASL] error in \"CSlave :: handle_write\"" );
		CONSOLE_Print( "[MASL] error " + UTIL_ToString( error.value( ) ) + " [" + error.message( ) + "]" );
		m_Connected = false;
		m_ProtocolInitialized = false;
		m_TempSendBuffer = m_SendBuffer;
	}
}

void CManager :: Update( )
{
	// try to reconnect every 10 seconds

	if( !m_Connected && GetTime( ) - m_LastConnectionAttemptTime >= 10 )
	{
		CONSOLE_Print( "[MASL] trying to connect to manager on " + m_Server + ":" + UTIL_ToString(m_Port) );

		// can't use async_connect on an open socket

		socket_.close( );

		boost::asio::ip::tcp::endpoint endpoint( boost::asio::ip::address::from_string( m_Server ), m_Port );
		socket_.async_connect( endpoint, boost::bind( &CManager::handle_connect, this, boost::asio::placeholders::error ) );

		m_LastConnectionAttemptTime = GetTime( );

	}

	if( m_Connected && m_AsyncReadCompleted )
	{
		m_AsyncReadCompleted = false;

		socket_.async_read_some( boost::asio::buffer( data_, max_length ),
			boost::bind( &CManager::handle_read, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) );
	}

	if( m_Connected && m_AsyncWriteCompleted && !m_SendBuffer.empty( ) )
	{
		m_AsyncWriteCompleted = false;

		socket_.async_write_some( boost::asio::buffer( m_SendBuffer ),
			boost::bind( &CManager::handle_write, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) );
	}

	io_service_.reset( );
	io_service_.poll( );

	if( m_Connected && GetTime( ) - m_LastStatusSentTime >= 10 )
	{
		/*if( m_GHost->m_CurrentGame )
			SendLobbyStatus( true, m_GHost->m_CurrentGame->GetGameID( ), m_GHost->m_CurrentGame->GetOwnerName( ) );
		else
			SendLobbyStatus( false, 0, string( ) );*/

		SendSlaveStatus( );
		//send STM_SLAVE_NAME


		m_LastStatusSentTime = GetTime( );
	}

	ExtractPackets( );

	ProcessPackets( );
}

void CManager :: ExtractPackets( )
{
	// packet minimum size is 6 bytes (4 byte packet size + 2 byte packet ID)

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

			CONSOLE_Print( "[MASL] received packet " + MASL_PROTOCOL :: FlagToString( PacketID ) + " [" + UTIL_ByteArrayToHexString( Header ) + "][" + Data + "]" );
			CONSOLE_Print( "[MASL] Extracted header [" + UTIL_ByteArrayToHexString( Header ) + "] size=" + UTIL_ToString( DataSize ) + " ID=" + UTIL_ToString( PacketID ) );

			// remove extracted packet from the receive buffer

			m_RecvBuffer.erase( 0, 6 + DataSize );
		}
		else
			break;
	}
}

void CManager :: ProcessPackets( )
{
	while( !m_Packets.empty( ) )
	{
		CMASLPacket *Packet = m_Packets.front( );
		m_Packets.pop( );

		stringstream SS;
		SS << Packet->GetData( );

		switch( Packet->GetID( ) )
		{
		case MASL_PROTOCOL :: MTS_HI:
			{
				SS >> m_SlaveID;

				// send all realms on which this bot is able to host on at the moment

				string ServersInfo;

				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
				{
					if( (*i)->GetLoggedIn( ) )
					{
						if( ServersInfo.empty( ) )
							ServersInfo = UTIL_ToString( (*i)->GetServerID( ) );
						else
							ServersInfo += " " + UTIL_ToString( (*i)->GetServerID( ) );
					}
				}

				if( !ServersInfo.empty( ) )
					Send( MASL_PROTOCOL :: STM_SERVERINFO, ServersInfo, true );

				// send game in lobby

				if( m_GHost->m_CurrentGame )
				{
					stringstream GameData;

					GameData << m_GHost->m_CurrentGame->GetGameType( );
					GameData << " " << GetServerID( m_GHost->m_CurrentGame->GetCreatorServer( ) );
					GameData << " " << m_GHost->m_CurrentGame->GetGameID( );
					GameData << " " << m_GHost->m_CurrentGame->GetGameState( );
					GameData << " " << m_GHost->m_CurrentGame->GetSlotsOccupied( );
					GameData << " " << m_GHost->m_CurrentGame->m_Slots.size( );

					MASL_PROTOCOL :: SSWriteString( GameData, m_GHost->m_CurrentGame->GetCreatorName( ) );
					MASL_PROTOCOL :: SSWriteString( GameData, m_GHost->m_CurrentGame->GetOwnerName( ) );

					GameData << " " << m_GHost->m_CurrentGame->GetGameName( ).size( );
					GameData << " " << m_GHost->m_CurrentGame->GetGameName( );

					GameData << " " << m_GHost->m_CurrentGame->m_Map->GetMapLocalPath( ).size( );
					GameData << " " << m_GHost->m_CurrentGame->m_Map->GetMapLocalPath( );

					for( int i = 0; i < m_GHost->m_CurrentGame->m_Slots.size( ); ++i )
					{
						unsigned char SlotStatus = m_GHost->m_CurrentGame->m_Slots[i].GetSlotStatus( );

						if( SlotStatus == 2 )
						{
							CGamePlayer *Player;
							Player = m_GHost->m_CurrentGame->GetPlayerFromPID( m_GHost->m_CurrentGame->m_Slots[i].GetPID( ) );

							if( Player )
							{
								GameData << " " << SlotStatus;
								GameData << " " << GetServerID( Player->GetJoinedRealm( ) );
								MASL_PROTOCOL :: SSWriteString( GameData, Player->GetName( ) );
							}
							else
								GameData << " " << (unsigned char)1;
						}
						else
							GameData << " " << SlotStatus;
					}

					Send( MASL_PROTOCOL :: STM_GAME_INLOBBY, GameData.str( ), true );
				}

				// send all running games

				for( vector<CBaseGame *> :: iterator i = m_GHost->m_Games.begin( ); i != m_GHost->m_Games.end( ); ++i )
				{
					stringstream GameData;

					GameData << (*i)->GetGameType( );
					GameData << " " << GetServerID( (*i)->GetCreatorServer( ) );
					GameData << " " << (*i)->GetGameID( );
					GameData << " " << (*i)->GetGameState( );
					GameData << " " << (*i)->GetSlotsOccupied( );
					GameData << " " << (*i)->m_Slots.size( );

					MASL_PROTOCOL :: SSWriteString( GameData, (*i)->GetCreatorName( ) );
					MASL_PROTOCOL :: SSWriteString( GameData, (*i)->GetOwnerName( ) );

					GameData << " " << (*i)->GetGameName( ).size( );
					GameData << " " << (*i)->GetGameName( );

					string MapLocalPath = (*i)->m_MapPath.substr( 14 );
					GameData << " " << MapLocalPath.size( );
					GameData << " " << MapLocalPath;

					for( int j = 0; j < (*i)->m_Slots.size( ); ++j )
					{
						unsigned char SlotStatus = (*i)->m_Slots[j].GetSlotStatus( );

						if( SlotStatus == 2 )
						{
							CGamePlayer *Player;
							Player = (*i)->GetPlayerFromPID( (*i)->m_Slots[j].GetPID( ) );

							if( Player )
							{
								GameData << " " << SlotStatus;
								GameData << " " << GetServerID( Player->GetJoinedRealm( ) );
								MASL_PROTOCOL :: SSWriteString( GameData, Player->GetName( ) );
							}
							else
								GameData << " " << (unsigned char)1;
						}
						else
							GameData << " " << SlotStatus;
					}

					Send( MASL_PROTOCOL :: STM_GAME_INPROGRESS, GameData.str( ), true );
				}

				/*for( vector<CBaseGame *> :: iterator i = m_GHost->m_Games.begin( ); i != m_GHost->m_Games.end( ); ++i )
				{
					stringstream GameData;
					GameData << (*i)->GetGameID( );
					GameData << " " << (uint32_t)(*i)->GetGameState( );

					if( (*i)->GetGameLoaded( ) )
					{
						GameData << " " << 3;
						GameData << " " << (*i)->m_GameTicks / 1000;
					}
					else if( (*i)->GetGameLoading( ) )
						GameData << " " << 2;
					else
					{
						GameData << " " << 1;
						GameData << " " << GetTime( ) - (*i)->m_CreationTime;
					}

					MASL_PROTOCOL :: SSWriteString( GameData, (*i)->GetCreatorName( ) );
					MASL_PROTOCOL :: SSWriteString( GameData, (*i)->GetCreatorServer( ) );
					MASL_PROTOCOL :: SSWriteString( GameData, (*i)->GetOwnerName( ) );
					GameData << " " << (*i)->GetGameName( ).size( );
					GameData << " " << (*i)->GetGameName( );
					GameData << " " << (*i)->GetSlotsOccupied( );
					GameData << " " << (*i)->m_Map->GetMapLocalPath( ).size( );
					GameData << " " << (*i)->m_Map->GetMapLocalPath( );
					GameData << " " << (*i)->m_Slots.size( );

					for( int j = 0; j < (*i)->m_Slots.size( ); ++j )
					{
						GameData << " " << (uint32_t)(*i)->m_Slots[j].GetSlotStatus( );

						CGamePlayer *Player;
						Player = (*i)->GetPlayerFromPID( (*i)->m_Slots[j].GetPID( ) );

						if( Player )
						{
							MASL_PROTOCOL :: SSWriteString( GameData, Player->GetName( ) );
							MASL_PROTOCOL :: SSWriteString( GameData, Player->GetSpoofedRealm( ) );
						}
					}

					if( (*i)->m_DotALadderGame )
					{
						GameData << " " << 1;
						GameData << " " << (*i)->m_MinimumRatingRequirment;
						GameData << " " << (*i)->m_MaximumRatingRequirment;
					}
					else
						GameData << " " << 0;

					Send( MASL_PROTOCOL :: STM_GAME_INPROGRESS, GameData.str( ), true );
				}*/

				Send( MASL_PROTOCOL :: STM_GHOST_GROUP, UTIL_ToString( m_GHost->m_GHostGroup ) );

				Send( MASL_PROTOCOL :: STM_THATSALL, true );
			}
			break;

		case MASL_PROTOCOL :: MTS_OK:
			{
				CONSOLE_Print( "[MASL] protocol initialized" );
				m_ProtocolInitialized = true;
				m_SendBuffer = m_TempSendBuffer;
			}
			break;

		case MASL_PROTOCOL :: MTS_CONTRIBUTOR_ONLY_MODE:
			{
				bool ContributorOnlyMode;
				SS >> ContributorOnlyMode;

				if( ContributorOnlyMode )
					CONSOLE_Print( "[MASL] contributor only mode ON" );
				else
					CONSOLE_Print( "[MASL] contributor only mode OFF" );

				m_GHost->m_ContributorOnlyMode = ContributorOnlyMode;
			}
			break;

		case MASL_PROTOCOL :: MTS_REMOTE_CMD:
			{
				uint32_t ServerID;
				string User;
				string Command;

				SS >> ServerID;
				SS >> User;
				getline( SS, Command );

				string :: size_type Start = Command.find_first_not_of( " " );

				if( Start != string :: npos )
					Command = Command.substr( Start );

				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
				{
					if( (*i)->GetServerID( ) == ServerID )
					{
						CIncomingChatEvent *chatCommand = new CIncomingChatEvent( CBNETProtocol::EID_WHISPER, 0, User, Command );
						(*i)->ProcessChatEvent( chatCommand );
						delete chatCommand;
						break;
					}
				}
			}
			break;

		case MASL_PROTOCOL :: MTS_USER_GETINFO_RESPONSE:
			{
				if( m_GHost->m_CurrentGame )
				{
					string Server;
					string Name;

					SS >> Server;
					MASL_PROTOCOL :: SSReadString( SS, Name );

					CONSOLE_Print( "Server = " + Server );
					CONSOLE_Print( "Name = " + Name );

					for( list<CCallablePlayerCheck *> :: iterator i = m_GHost->m_CurrentGame->m_PlayerChecks.begin( ); i != m_GHost->m_CurrentGame->m_PlayerChecks.end( ); ++i )
					{
						if( (*i)->GetServer( ) == Server && (*i)->GetName( ) == Name )
						{
							string From;
							uint32_t LongFromSize;
							string LongFrom;
							uint32_t AccessLevel;
							uint32_t Banned;
							uint32_t GamePlayerSummarySent;
							uint32_t Div1DotAPlayerSummarySent;
							CDBGPS *GamePlayerSummary = NULL;
							CDBDiv1DPS *Div1DotAPlayerSummary = NULL;

							MASL_PROTOCOL :: SSReadString( SS, From );
							SS >> LongFromSize;
							LongFrom = UTIL_SSRead( SS, 1, LongFromSize );
							SS >> AccessLevel;
							SS >> Banned;
							SS >> GamePlayerSummarySent;
							SS >> Div1DotAPlayerSummarySent;

							CONSOLE_Print( "From = " + From );
							CONSOLE_Print( "LongFromSize = " + UTIL_ToString( LongFromSize ) );
							CONSOLE_Print( "LongFrom = " + LongFrom );
							CONSOLE_Print( "AccessLevel = " + UTIL_ToString( AccessLevel ) );
							CONSOLE_Print( "Banned = " + UTIL_ToString( Banned ) );
							CONSOLE_Print( "GamePlayerSummarySent = " + UTIL_ToString( GamePlayerSummarySent ) );
							CONSOLE_Print( "Div1DotAPlayerSummarySent = " + UTIL_ToString( Div1DotAPlayerSummarySent ) );

							if( Div1DotAPlayerSummarySent )
							{
								uint32_t TotalGames;
								uint32_t TotalWins;
								uint32_t TotalLosses;
								uint32_t TotalKills;
								uint32_t TotalDeaths;
								uint32_t TotalCreepKills;
								uint32_t TotalCreepDenies;
								uint32_t TotalAssists;
								uint32_t TotalNeutralKills;
								double Rating;
								double HighestRating;

								SS >> TotalGames;
								SS >> TotalWins;
								SS >> TotalLosses;
								SS >> TotalKills;
								SS >> TotalDeaths;
								SS >> TotalCreepKills;
								SS >> TotalCreepDenies;
								SS >> TotalAssists;
								SS >> TotalNeutralKills;
								SS >> Rating;
								SS >> HighestRating;

								CONSOLE_Print( "TotalGames = " + UTIL_ToString( TotalGames ) );
								CONSOLE_Print( "TotalWins = " + UTIL_ToString( TotalWins ) );
								CONSOLE_Print( "TotalLosses = " + UTIL_ToString( TotalLosses ) );
								CONSOLE_Print( "TotalKills = " + UTIL_ToString( TotalKills ) );
								CONSOLE_Print( "TotalDeaths = " + UTIL_ToString( TotalDeaths ) );
								CONSOLE_Print( "TotalCreepKills = " + UTIL_ToString( TotalCreepKills ) );
								CONSOLE_Print( "TotalCreepDenies = " + UTIL_ToString( TotalCreepDenies ) );
								CONSOLE_Print( "TotalAssists = " + UTIL_ToString( TotalAssists ) );
								CONSOLE_Print( "TotalNeutralKills = " + UTIL_ToString( TotalNeutralKills ) );
								CONSOLE_Print( "Rating = " + UTIL_ToString( Rating, 0 ) );
								CONSOLE_Print( "HighestRating = " + UTIL_ToString( HighestRating, 0 ) );

								Div1DotAPlayerSummary = new CDBDiv1DPS( Rating, HighestRating, TotalGames, TotalWins, TotalLosses, TotalKills, TotalDeaths, TotalCreepKills, TotalCreepDenies, TotalAssists, TotalNeutralKills );
							}

							(*i)->SetResult( new CDBPlayer( GamePlayerSummary, Div1DotAPlayerSummary, Server, Name, From, LongFrom, AccessLevel, Banned ) );
							(*i)->SetReady( true );
							break;
						}
					}
				}
			}
			break;

		case MASL_PROTOCOL :: MTS_PLAYER_FROMCODES:
			{
				if( m_GHost->m_CurrentGame )
				{
					string Name;
					uint32_t ServerID;
					string From;
					uint32_t LongFromSize;
					string LongFrom;

					MASL_PROTOCOL :: SSReadString( SS, Name );
					SS >> ServerID;
					MASL_PROTOCOL :: SSReadString( SS, From );
					SS >> LongFromSize;
					LongFrom = UTIL_SSRead( SS, 1, LongFromSize );

					CONSOLE_Print( "Server = " + UTIL_ToString( ServerID ) );
					CONSOLE_Print( "Name = " + Name );
					CONSOLE_Print( "From = " + From );
					CONSOLE_Print( "LongFrom = " + LongFrom );

					m_GHost->m_CurrentGame->SetFromCodes( Name, ServerID, From, LongFrom );
				}
			}
			break;

		case MASL_PROTOCOL :: MTS_GAME_GETDBID_RESPONSE:
			{
				uint32_t GameID = 0;
				uint32_t MySQLGameID = 0;

				SS >> GameID;
				SS >> MySQLGameID;

				CONSOLE_Print( "GameID = " + UTIL_ToString( GameID ) );
				CONSOLE_Print( "MySQLGameID = " + UTIL_ToString( MySQLGameID ) );

				for( vector<CBaseGame *> :: iterator i = m_GHost->m_Games.begin( ); i != m_GHost->m_Games.end( ); ++i )
				{
					if( (*i)->GetGameID( ) == GameID )
					{
						(*i)->SetMySQLGameID( MySQLGameID );
						break;
					}
				}
			}
			break;

		case MASL_PROTOCOL :: MTS_CONFIG_RELOAD:
			{
				m_GHost->ReloadConfigs();
				CONSOLE_Print("[MASL] RSC command received, reloading config");
			}
			break;

		case MASL_PROTOCOL :: MTS_CREATE_GAME:
			{
				string CreatorServer;
				uint32_t GameType;
				uint32_t GameState;
				uint32_t Observers;
				string CreatorName;
				uint32_t GameNameSize;
				string GameName;
				uint32_t MapNameSize;
				string MapName;
				uint32_t heldPlayersSize = 0;
				vector<string> heldPlayers;

				MASL_PROTOCOL :: SSReadString( SS, CreatorServer );
				SS >> GameType;
				SS >> GameState;
				SS >> Observers;
				MASL_PROTOCOL :: SSReadString( SS, CreatorName );

				CONSOLE_Print( "CreatorServer = " + CreatorServer );
				CONSOLE_Print( "GameState = " + UTIL_ToString( GameState ) );
				CONSOLE_Print( "CreatorName = " + CreatorName );

				SS >> GameNameSize;
				GameName = UTIL_SSRead( SS, 1, GameNameSize );

				CONSOLE_Print( "GameNameSize = " + UTIL_ToString( GameNameSize ) );
				CONSOLE_Print( "GameName = " + GameName );

				SS >> MapNameSize;
				MapName = UTIL_SSRead( SS, 1, MapNameSize );

				CONSOLE_Print( "MapNameSize = " + UTIL_ToString( MapNameSize ) );
				CONSOLE_Print( "MapName = " + MapName );

				SS >> heldPlayersSize;
				CONSOLE_Print("heldPlayersSize = " + UTIL_ToString(heldPlayersSize));

				for (int i = 0; i < heldPlayersSize; i++) {
					string heldPlayer;
					SS >> heldPlayer;
					heldPlayers.push_back(heldPlayer);
					CONSOLE_Print(UTIL_ToString(i)+": heldPlayer = " + heldPlayer);
				}

				switch( GameType )
				{
				case MASL_PROTOCOL :: DB_DIV1_DOTA_GAME:
					{
						if (Observers) {
							m_GHost->CreateGame(m_GHost->m_DotALadderMapObs, GameState, false, GameName, CreatorName,
								CreatorName, CreatorServer, MASL_PROTOCOL::DB_DIV1_DOTA_GAME, Observers, heldPlayers);
						}
						else {
							if (MapName == "l") {
								m_GHost->CreateGame(m_GHost->m_DotALadderMapLegacy, GameState, false, GameName, CreatorName,
									CreatorName, CreatorServer, MASL_PROTOCOL::DB_DIV1_DOTA_GAME, Observers, heldPlayers);
							}
							else {
								m_GHost->CreateGame(m_GHost->m_Map, GameState, false, GameName, CreatorName, CreatorName,
									CreatorServer, MASL_PROTOCOL::DB_DIV1_DOTA_GAME, Observers, heldPlayers);
							}
						}
					}
					break;

				case MASL_PROTOCOL :: DB_CUSTOM_DOTA_GAME:
					{
						if( Observers )
							m_GHost->CreateGame( m_GHost->m_DotALadderMapObs, GameState, false, GameName, CreatorName,
								CreatorName, CreatorServer, MASL_PROTOCOL :: DB_CUSTOM_DOTA_GAME, Observers, heldPlayers);
						else
							m_GHost->CreateGame( m_GHost->m_Map, GameState, false, GameName, CreatorName,
								CreatorName, CreatorServer, MASL_PROTOCOL :: DB_CUSTOM_DOTA_GAME, Observers, heldPlayers);
					}
					break;

				case MASL_PROTOCOL :: DB_CUSTOM_GAME:
					{
						CBNET *Server = NULL;

						for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
						{
							if( (*i)->GetServer( ) == CreatorServer )
							{
								Server = *i;
								break;
							}
						}

						string FoundMaps;

						try
						{
							path MapPath( m_GHost->m_MapPath );
							string Pattern = MapName;
							transform( Pattern.begin( ), Pattern.end( ), Pattern.begin( ), (int(*)(int))tolower );

							if( !exists( MapPath ) )
							{
								CONSOLE_Print( "[MASL] error listing maps - map path doesn't exist" );
								Server->SendChatCommand( m_GHost->m_Language->ErrorListingMaps( ), CreatorName );
							}
							else
							{
								directory_iterator EndIterator;
								path LastMatch;
								uint32_t Matches = 0;

								for( directory_iterator i( MapPath ); i != EndIterator; i++ )
								{
									string FileName = i->path( ).filename( ).string( );
									string Stem = i->path( ).stem( ).string( );
									transform( FileName.begin( ), FileName.end( ), FileName.begin( ), (int(*)(int))tolower );
									transform( Stem.begin( ), Stem.end( ), Stem.begin( ), (int(*)(int))tolower );

									if( !is_directory( i->status( ) ) && FileName.find( Pattern ) != string :: npos )
									{
										LastMatch = i->path( );
										++Matches;

										if( FoundMaps.empty( ) )
											FoundMaps = i->path( ).filename( ).string( );
										else
											FoundMaps += ", " + i->path( ).filename( ).string( );

										// if the pattern matches the filename exactly, with or without extension, stop any further matching

										if( FileName == Pattern || Stem == Pattern )
										{
											Matches = 1;
											break;
										}
									}
								}

								if( Matches == 0 )
									Server->SendChatCommand( m_GHost->m_Language->NoMapsFound( ), CreatorName );
								else if( Matches == 1 )
								{
									string File = LastMatch.filename( ).string( );
									Server->SendChatCommand( m_GHost->m_Language->LoadingConfigFile( File ), CreatorName );

									// hackhack: create a config file in memory with the required information to load the map

									CConfig MapCFG;
									MapCFG.Set( "map_path", "Maps\\Download\\" + File );
									MapCFG.Set( "map_localpath", File );
									//m_GHost->m_CustomGameMap->Load( &MapCFG, File );
									m_GHost->LoadCustomGameMap( &MapCFG, File );

									//CONSOLE_Print( "[MASL : " + GameName + "] creating custom game with owner [" + OwnerName + "] and game state " + UTIL_ToString( GameState ) );
									//CONSOLE_Print( "[MASL : map] " + m_GHost->m_RpgMap->GetCFGFile( ) );

									if( GameState == MASL_PROTOCOL :: GAME_PUB )
										CONSOLE_Print( "[MASL] creating public custom game by [" + CreatorName + "] with game name [" + GameName + "], using map [" + File + "]" );
									else
										CONSOLE_Print( "[MASL] creating private custom game by [" + CreatorName + "] with game name [" + GameName + "], using map [" + File + "]" );

									m_GHost->CreateGame( m_GHost->m_CustomGameMap, GameState, false, GameName, CreatorName,
										CreatorName, CreatorServer, MASL_PROTOCOL :: DB_CUSTOM_GAME, false, heldPlayers);
								}
								else
									Server->SendChatCommand( m_GHost->m_Language->FoundMaps( FoundMaps ), CreatorName );
							}
						}
						catch( const exception &ex )
						{
							// CONSOLE_Print( "[MASL] error listing maps - caught exception [" + ex.what( ) + "]" );
							CONSOLE_Print( "[MASL] error listing maps - caught exception [ex.what( )]" );
							Server->SendChatCommand( m_GHost->m_Language->ErrorListingMaps( ), CreatorName );
						}
					}
					break;

				default:
					break;
				}
			}
			break;

		/*case MASL_PROTOCOL :: MTS_CREATE_DOTAGAME:
			{
				string CreatorServer;
				uint32_t GameState;
				string CreatorName;
				uint32_t GameNameSize;
				string GameName;

				MASL_PROTOCOL :: SSReadString( SS, CreatorServer );
				SS >> GameState;
				MASL_PROTOCOL :: SSReadString( SS, CreatorName );

				CONSOLE_Print( "CreatorServer = " + CreatorServer );
				CONSOLE_Print( "GameState = " + UTIL_ToString( GameState ) );
				CONSOLE_Print( "CreatorName = " + CreatorName );

				SS >> GameNameSize;
				GameName = UTIL_SSRead( SS, 1, GameNameSize );

				CONSOLE_Print( "GameNameSize = " + UTIL_ToString( GameNameSize ) );
				CONSOLE_Print( "GameName = " + GameName );

				if( GameState == MASL_PROTOCOL :: GAME_PUB )
					CONSOLE_Print( "[MASL] creating public DotA ladder game by [" + CreatorName + "] with game name [" + GameName + "]" );
				else
					CONSOLE_Print( "[MASL] creating private DotA ladder game by [" + CreatorName + "] with game name [" + GameName + "]" );

				m_GHost->CreateGame( m_GHost->m_Map, GameState, false, GameName, CreatorName, CreatorName, CreatorServer, true );
			}
			break;*/

		/*case MASL_PROTOCOL :: MTS_CREATE_RPGGAME:
			{
				string CreatorServer;
				uint32_t GameState;
				string CreatorName;
				uint32_t GameNameSize;
				string GameName;
				uint32_t MapNameSize;
				string MapName;

				MASL_PROTOCOL :: SSReadString( SS, CreatorServer );
				SS >> GameState;
				MASL_PROTOCOL :: SSReadString( SS, CreatorName );

				CONSOLE_Print( "CreatorServer = " + CreatorServer );
				CONSOLE_Print( "GameState = " + UTIL_ToString( GameState ) );
				CONSOLE_Print( "CreatorName = " + CreatorName );

				SS >> GameNameSize;
				GameName = UTIL_SSRead( SS, 1, GameNameSize );

				CONSOLE_Print( "GameNameSize = " + UTIL_ToString( GameNameSize ) );
				CONSOLE_Print( "GameName = " + GameName );

				SS >> MapNameSize;
				MapName = UTIL_SSRead( SS, 1, MapNameSize );

				CONSOLE_Print( "MapNameSize = " + UTIL_ToString( MapNameSize ) );
				CONSOLE_Print( "MapName = " + MapName );

				CBNET *Server = NULL;

				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
				{
					if( (*i)->GetServer( ) == CreatorServer )
					{
						Server = *i;
						break;
					}
				}

				string FoundMaps;

				try
				{
					path MapPath( m_GHost->m_MapPath );
					string Pattern = MapName;
					transform( Pattern.begin( ), Pattern.end( ), Pattern.begin( ), (int(*)(int))tolower );

					if( !exists( MapPath ) )
					{
						CONSOLE_Print( "[MASL] error listing maps - map path doesn't exist" );
						Server->SendChatCommand( m_GHost->m_Language->ErrorListingMaps( ), CreatorName );
					}
					else
					{
						directory_iterator EndIterator;
						path LastMatch;
						uint32_t Matches = 0;

						for( directory_iterator i( MapPath ); i != EndIterator; i++ )
						{
							string FileName = i->filename( );
							string Stem = i->path( ).stem( );
							transform( FileName.begin( ), FileName.end( ), FileName.begin( ), (int(*)(int))tolower );
							transform( Stem.begin( ), Stem.end( ), Stem.begin( ), (int(*)(int))tolower );

							if( !is_directory( i->status( ) ) && FileName.find( Pattern ) != string :: npos )
							{
								LastMatch = i->path( );
								Matches++;

								if( FoundMaps.empty( ) )
									FoundMaps = i->filename( );
								else
									FoundMaps += ", " + i->filename( );

								// if the pattern matches the filename exactly, with or without extension, stop any further matching

								if( FileName == Pattern || Stem == Pattern )
								{
									Matches = 1;
									break;
								}
							}
						}

						if( Matches == 0 )
							Server->SendChatCommand( m_GHost->m_Language->NoMapsFound( ), CreatorName );
						else if( Matches == 1 )
						{
							string File = LastMatch.filename( );
							Server->SendChatCommand( m_GHost->m_Language->LoadingConfigFile( File ), CreatorName );

							// hackhack: create a config file in memory with the required information to load the map

							CConfig MapCFG;
							MapCFG.Set( "map_path", "Maps\\Download\\" + File );
							MapCFG.Set( "map_localpath", File );
							m_GHost->m_CustomGameMap->Load( &MapCFG, File );

							//CONSOLE_Print( "[MASL : " + GameName + "] creating custom game with owner [" + OwnerName + "] and game state " + UTIL_ToString( GameState ) );
							//CONSOLE_Print( "[MASL : map] " + m_GHost->m_RpgMap->GetCFGFile( ) );

							if( GameState == MASL_PROTOCOL :: GAME_PUB )
								CONSOLE_Print( "[MASL] creating public custom game by [" + CreatorName + "] with game name [" + GameName + "], using map [" + File + "]" );
							else
								CONSOLE_Print( "[MASL] creating private custom game by [" + CreatorName + "] with game name [" + GameName + "], using map [" + File + "]" );

							m_GHost->CreateGame( m_GHost->m_CustomGameMap, GameState, false, GameName, CreatorName, CreatorName, CreatorServer, true );
						}
						else
							Server->SendChatCommand( m_GHost->m_Language->FoundMaps( FoundMaps ), CreatorName );
					}
				}
				catch( const exception &ex )
				{
					// CONSOLE_Print( "[MASL] error listing maps - caught exception [" + ex.what( ) + "]" );
					CONSOLE_Print( "[MASL] error listing maps - caught exception [ex.what( )]" );
					Server->SendChatCommand( m_GHost->m_Language->ErrorListingMaps( ), CreatorName );
				}
			}
			break;*/
		}

		delete Packet;
	}
}

void CManager :: SendLoggedInToBNET( uint32_t serverID )
{
	Send( MASL_PROTOCOL :: STM_SLAVE_LOGGEDIN, UTIL_ToString( serverID ) );
}

void CManager :: SendDisconnectedFromBNET( uint32_t serverID )
{
	Send( MASL_PROTOCOL :: STM_SLAVE_DISCONNECTED, UTIL_ToString( serverID ) );
}

void CManager :: SendMaxGamesReached( )
{
	Send( MASL_PROTOCOL :: STM_SLAVE_MAXGAMESREACHED );
}

void CManager :: SendGameHosted( uint32_t gameType, uint32_t serverID, uint32_t gameID, uint32_t gameState, uint32_t mapSlots, string ownerName, string gameName, string cfgFile )
{
	Send( MASL_PROTOCOL :: STM_GAME_HOSTED, UTIL_ToString( gameType ) + " " + UTIL_ToString( serverID ) + " " + UTIL_ToString( gameID ) + " " + UTIL_ToString( gameState ) + " " + ownerName + " " + UTIL_ToString( mapSlots ) + " </" + cfgFile + "/> " + gameName );
}

void CManager :: SendGameUnhosted( uint32_t gameID )
{
	Send( MASL_PROTOCOL :: STM_GAME_UNHOSTED, UTIL_ToString( gameID ) );
}

void CManager :: SendGameStarted( uint32_t gameType, uint32_t creatorServerID, uint32_t gameID, uint32_t gameState, uint32_t startPlayers, string ownerName, string gameName, CBaseGame *game )
{
	string SlotInfo = string( );

	for( uint32_t i = 0; i < 12; i++ )
	{
		CGamePlayer *Player;
		Player = game->GetPlayerFromSID( i );

		if( Player )
			SlotInfo += " " + UTIL_ToString( i + 1 ) + " " + Player->GetName( );
	}

	if( !SlotInfo.empty( ) )
		SlotInfo.erase( 0, 1 );

	Send( MASL_PROTOCOL :: STM_GAME_STARTED, UTIL_ToString( gameType ) + " " + UTIL_ToString( creatorServerID ) + " " + UTIL_ToString( gameID ) + " " + UTIL_ToString( gameState ) + " " + ownerName + " " + UTIL_ToString( startPlayers ) + " </" + SlotInfo + "/> " + gameName );
}

void CManager :: SendGameIsOver( uint32_t gameID )
{
	Send( MASL_PROTOCOL :: STM_GAME_ISOVER, UTIL_ToString( gameID ) );
}

void CManager :: SendGameNameChanged( uint32_t gameID, uint32_t gameState, string gameName )
{
	Send( MASL_PROTOCOL :: STM_GAME_NAMECHANGED, UTIL_ToString( gameID ) + " " + UTIL_ToString( gameState ) + " " + gameName );
}

void CManager :: SendGameOwnerChanged( uint32_t gameID, string ownerName )
{
	Send( MASL_PROTOCOL :: STM_GAME_OWNERCHANGED, UTIL_ToString( gameID ) + " " + ownerName );
}

void CManager :: SendGamePlayerJoinedLobby( uint32_t gameID, string name )
{
	Send( MASL_PROTOCOL :: STM_GAME_PLAYERJOINEDLOBBY, UTIL_ToString( gameID ) + " " + name );
}

void CManager :: SendGamePlayerLeftLobby( uint32_t gameID, string name )
{
	Send( MASL_PROTOCOL :: STM_GAME_PLAYERLEFTLOBBY, UTIL_ToString( gameID ) + " " + name);
}

void CManager :: SendGamePlayerLeftLobbyInform( string server, string name, uint16_t reason )
{
	if (!server.empty())
		Send( MASL_PROTOCOL :: STM_PLAYERLEAVE_LOBBY_INFORM, server + " " + name + " " + UTIL_ToString(reason));
}

void CManager :: SendGamePlayerLeftGame( uint32_t gameID, string name )
{
	Send( MASL_PROTOCOL :: STM_GAME_PLAYERLEFTGAME, UTIL_ToString( gameID ) + " " + name );
}

void CManager :: SendGameGetID( uint32_t gameID )
{
	Send( MASL_PROTOCOL :: STM_GAME_GETDBID, UTIL_ToString( gameID ) );
}

void CManager :: SendLobbyStatus( bool gameInLobby, uint32_t gameID, string creatorName )
{
	if( gameInLobby )
		Send( MASL_PROTOCOL :: STM_LOBBY_STATUS, "1 " + UTIL_ToString( gameID ) + " " + creatorName );
	else
		Send( MASL_PROTOCOL :: STM_LOBBY_STATUS, "0" );
}

void CManager :: SendSlaveStatus( )
{
	stringstream SS;

	if( m_GHost->m_CurrentGame )
	{
		SS << true;
		SS << " " << m_GHost->m_CurrentGame->GetGameID( );
		MASL_PROTOCOL::SSWriteString( SS, m_GHost->m_CurrentGame->GetOwnerName( ) );
	}
	else
		SS << false;

	SS << " " << m_GHost->m_BNETs.size( );

	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
	{
		SS << " " << (*i)->GetServerID( );

		if( (*i)->GetLoggedIn( ) )
		{
			SS << " " << true;
			SS << " " << (*i)->GetBnetUserName( );
		}
		else
		{
			SS << " " << false;
			SS << " " << (*i)->GetBnetUserName( );
		}
	}

	Send( MASL_PROTOCOL :: STM_SLAVE_STATUS, SS.str( ) );
}

void CManager :: SendGameSave( uint32_t mysqlGameID, uint32_t lobbyDuration, string lobbyLog, string gameLog, string replayName, string mapPath, string gameName, string ownerName, uint32_t gameDuration, uint32_t gameState, string creatorName, string creatorServer, vector<CDBGamePlayer *> gamePlayers, bool rmk, uint32_t gameType, string gameInfo )
{
	stringstream GameData;

	// don't need gameID here, it's sent in GAME_ISOVER packet

	GameData << mysqlGameID;
	GameData << " " << lobbyDuration;
	GameData << " " << lobbyLog.size( );
	GameData << " " << lobbyLog;
	GameData << " " << gameLog.size( );
	GameData << " " << gameLog;
	GameData << " " << replayName.size( );
	GameData << " " << replayName;
	GameData << " " << mapPath.size( );
	GameData << " " << mapPath;
	GameData << " " << gameName.size( );
	GameData << " " << gameName;
	GameData << " " << ownerName;
	GameData << " " << gameDuration;
	GameData << " " << gameState;
	GameData << " " << creatorName;
	GameData << " " << creatorServer;

	GameData << " " << gamePlayers.size( );

	for( vector<CDBGamePlayer *> :: iterator i = gamePlayers.begin( ); i != gamePlayers.end( ); ++i )
	{
		GameData << " " << (*i)->GetName( );
		GameData << " " << (*i)->GetSpoofedRealm( );
		GameData << " " << (*i)->GetColour( );
		GameData << " " << (*i)->GetIP( );
		GameData << " " << (*i)->GetLeft( );
		GameData << " " << (*i)->GetLeftReason( ).size( );
		GameData << " " << (*i)->GetLeftReason( );
		GameData << " " << (*i)->GetLoadingTime( );
		GameData << " " << (*i)->GetTeam( );
		GameData << " " << (*i)->GetReserved( );
		GameData << " " << (*i)->GetBanned( );
		GameData << " " << (*i)->GetGProxy( );
	}

	GameData << " " << rmk ? 1 : 0;

	GameData << " " << gameType;
	GameData << " " << gameInfo;

	Send( MASL_PROTOCOL :: STM_GAME_SAVEDATA, GameData.str( ) );
}

void CManager :: SendErrorGameInLobby( )
{
	Send( MASL_PROTOCOL :: STM_ERROR_GAMEINLOBBY );
}

void CManager :: SendErrorMaxGamesReached( )
{
	Send( MASL_PROTOCOL :: STM_ERROR_MAXGAMESREACHED );
}

void CManager :: SendErrorGameNameExists( string gameName )
{
	Send( MASL_PROTOCOL :: STM_ERROR_GAMENAMEEXISTS, gameName );
}

void CManager :: SendUserWasBanned( string server, string victim, uint32_t mysqlGameID, string reason )
{
	Send( MASL_PROTOCOL :: STM_USER_WASBANNED, server + " " + victim + " " + UTIL_ToString( mysqlGameID ) + " " + reason );
}

void CManager :: SendDIV1PlayerWasBanned( uint32_t adminServerID, string adminName, uint32_t victimServerID, string victimName, uint32_t mysqlGameID, string reason )
{
	Send( MASL_PROTOCOL::STM_DIV1_PLAYERBANNEDBYPLAYER, UTIL_ToString( adminServerID ) + " " + adminName + " " + UTIL_ToString( victimServerID ) + " " + victimName + " " + UTIL_ToString( mysqlGameID ) + " " + reason );
}

void CManager :: SendGetPlayerInfo( string server, uint32_t IP, string name )
{
	Send( MASL_PROTOCOL :: STM_USER_GETINFO, name + " " + server + " " + UTIL_ToString( IP ) );
}

void CManager :: Send( uint16_t flag, bool send )
{
	BYTEARRAY Packet = UTIL_CreateByteArray( (uint32_t)0, true );
	BYTEARRAY Flag = UTIL_CreateByteArray( flag, true );
	UTIL_AppendByteArrayFast( Packet, Flag );

	//CONSOLE_Print( "[MASL] queued to buffer packet " + MASL_PROTOCOL :: FlagToString( flag ) );

	// we use send = true if the packet is part of protocol initialization process

	if( m_ProtocolInitialized || send )
		m_SendBuffer += string( Packet.begin( ), Packet.end( ) );
	else if( flag == MASL_PROTOCOL :: STM_GAME_SAVEDATA )
		m_TempSendBuffer += string( Packet.begin( ), Packet.end( ) );
}

void CManager :: Send( uint16_t flag, string msg, bool send )
{
	BYTEARRAY Packet = UTIL_CreateByteArray( (uint32_t)msg.size( ), true );
	BYTEARRAY Flag = UTIL_CreateByteArray( flag, true );
	UTIL_AppendByteArrayFast( Packet, Flag );

	//CONSOLE_Print( "[MASL] queued to buffer packet " + MASL_PROTOCOL :: FlagToString( flag ) + " [" + UTIL_ByteArrayToHexString( Packet ) + "][" + msg + "]" );

	// we use send = true if the packet is part of protocol initialization process

	if( m_ProtocolInitialized || send )
	{
		m_SendBuffer += string( Packet.begin( ), Packet.end( ) );
		m_SendBuffer += msg;
	}
	else if( flag == MASL_PROTOCOL :: STM_GAME_SAVEDATA )
	{
		m_TempSendBuffer += string( Packet.begin( ), Packet.end( ) );
		m_TempSendBuffer += msg;
	}
}
