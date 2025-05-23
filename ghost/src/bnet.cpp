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
#include "commandpacket.h"
#include "bncsutilinterface.h"
#include "bnetprotocol.h"
#include "bnet.h"
#include "map.h"
#include "packed.h"
#include "savegame.h"
#include "replay.h"
#include "gameprotocol.h"
#include "game_base.h"
#include "gameplayer.h"
#include "masl_protocol_2.h"
#include "game_div1dota.h"
#include "regions.h"

#include <boost/filesystem.hpp>

using namespace boost :: filesystem;

//
// CBNET
//

CBNET :: CBNET( CGHost *nGHost, string nServer, string nServerAlias, string nBNLSServer, uint16_t nBNLSPort, uint32_t nBNLSWardenCookie, string nCDKeyROC, string nCDKeyTFT, string nCountryAbbrev, string nCountry, uint32_t nLocaleID, string nUserName, string nUserPassword, string nFirstChannel, string nRootAdmin, char nCommandTrigger, bool nHoldFriends, bool nHoldClan, bool nPublicCommands, unsigned char nWar3Version, BYTEARRAY nEXEVersion, BYTEARRAY nEXEVersionHash, string nPasswordHashType, string nPVPGNRealmName, uint32_t nMaxMessageLength, uint32_t nHostCounterID, uint32_t nServerID, uint32_t nXPAMGProxyVersion )
{
	// todotodo: append path seperator to Warcraft3Path if needed

	m_GHost = nGHost;
	m_Socket = new CTCPClient( );
	m_Protocol = new CBNETProtocol( );
	m_BNCSUtil = new CBNCSUtilInterface( nUserName, nUserPassword );
	m_Exiting = false;
	m_Server = nServer;
	string LowerServer = m_Server;
	transform( LowerServer.begin( ), LowerServer.end( ), LowerServer.begin( ), (int(*)(int))tolower );

	if( !nServerAlias.empty( ) )
		m_ServerAlias = nServerAlias;
	else if( LowerServer == "useast.battle.net" )
		m_ServerAlias = "USEast";
	else if( LowerServer == "uswest.battle.net" )
		m_ServerAlias = "USWest";
	else if( LowerServer == "asia.battle.net" )
		m_ServerAlias = "Asia";
	else if( LowerServer == "europe.battle.net" )
		m_ServerAlias = "Europe";
	else
		m_ServerAlias = m_Server;

	if( nPasswordHashType == "pvpgn" && !nBNLSServer.empty( ) )
	{
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] pvpgn connection found with a configured BNLS server, ignoring BNLS server" );
		nBNLSServer.clear( );
		nBNLSPort = 0;
		nBNLSWardenCookie = 0;
	}

	m_CDKeyROC = nCDKeyROC;
	m_CDKeyTFT = nCDKeyTFT;

	// remove dashes and spaces from CD keys and convert to uppercase

	m_CDKeyROC.erase( remove( m_CDKeyROC.begin( ), m_CDKeyROC.end( ), '-' ), m_CDKeyROC.end( ) );
	m_CDKeyTFT.erase( remove( m_CDKeyTFT.begin( ), m_CDKeyTFT.end( ), '-' ), m_CDKeyTFT.end( ) );
	m_CDKeyROC.erase( remove( m_CDKeyROC.begin( ), m_CDKeyROC.end( ), ' ' ), m_CDKeyROC.end( ) );
	m_CDKeyTFT.erase( remove( m_CDKeyTFT.begin( ), m_CDKeyTFT.end( ), ' ' ), m_CDKeyTFT.end( ) );
	transform( m_CDKeyROC.begin( ), m_CDKeyROC.end( ), m_CDKeyROC.begin( ), (int(*)(int))toupper );
	transform( m_CDKeyTFT.begin( ), m_CDKeyTFT.end( ), m_CDKeyTFT.begin( ), (int(*)(int))toupper );

	if( m_CDKeyROC.size( ) != 26 )
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] warning - your ROC CD key is not 26 characters long and is probably invalid" );

	if( m_GHost->m_TFT && m_CDKeyTFT.size( ) != 26 )
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] warning - your TFT CD key is not 26 characters long and is probably invalid" );

	m_CountryAbbrev = nCountryAbbrev;
	m_Country = nCountry;
	m_LocaleID = nLocaleID;
	m_UserName = nUserName;
	m_UserPassword = nUserPassword;
	m_FirstChannel = nFirstChannel;
	m_RootAdmin = nRootAdmin;
	transform( m_RootAdmin.begin( ), m_RootAdmin.end( ), m_RootAdmin.begin( ), (int(*)(int))tolower );
	m_CommandTrigger = nCommandTrigger;
	m_War3Version = nWar3Version;
	m_EXEVersion = nEXEVersion;
	m_EXEVersionHash = nEXEVersionHash;
	m_PasswordHashType = nPasswordHashType;
	m_PVPGNRealmName = nPVPGNRealmName;
	m_MaxMessageLength = nMaxMessageLength;
	m_HostCounterID = nHostCounterID;
	m_LastDisconnectedTime = 0;
	m_LastConnectionAttemptTime = 0;
	m_LastNullTime = 0;
	m_LastOutPacketTicks = 0;
	m_LastOutPacketSize = 0;
	m_FrequencyDelayTimes = 0;
	m_FirstConnect = true;
	m_WaitingToConnect = true;
	m_LoggedIn = false;
	m_InChat = false;
	m_XPAMGProxyVersion = nXPAMGProxyVersion;
	m_FingerChatBuffer = {};

	m_ServerID = nServerID;
}

CBNET :: ~CBNET( )
{
	delete m_Socket;
	delete m_Protocol;

	while( !m_Packets.empty( ) )
	{
		delete m_Packets.front( );
		m_Packets.pop( );
	}

	delete m_BNCSUtil;
}

BYTEARRAY CBNET :: GetUniqueName( )
{
	return m_Protocol->GetUniqueName( );
}

unsigned int CBNET :: SetFD( void *fd, void *send_fd, int *nfds )
{
	unsigned int NumFDs = 0;

	if( !m_Socket->HasError( ) && m_Socket->GetConnected( ) )
	{
		m_Socket->SetFD( (fd_set *)fd, (fd_set *)send_fd, nfds );
		++NumFDs;
	}

	return NumFDs;
}

bool CBNET :: Update( void *fd, void *send_fd )
{
	// we return at the end of each if statement so we don't have to deal with errors related to the order of the if statements
	// that means it might take a few ms longer to complete a task involving multiple steps (in this case, reconnecting) due to blocking or sleeping
	// but it's not a big deal at all, maybe 100ms in the worst possible case (based on a 50ms blocking time)

	if( m_Socket->HasError( ) )
	{
		// the socket has an error

		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] disconnected from battle.net due to socket error" );

		if( m_Socket->GetError( ) == ECONNRESET && GetTime( ) - m_LastConnectionAttemptTime <= 15 )
			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] warning - you are probably temporarily IP banned from battle.net" );

		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] waiting 90 seconds to reconnect" );
		m_GHost->EventBNETDisconnected( this );
		m_BNCSUtil->Reset( m_UserName, m_UserPassword );
		m_Socket->Reset( );
		m_LastDisconnectedTime = GetTime( );
		m_LoggedIn = false;
		m_InChat = false;
		m_WaitingToConnect = true;
		return m_Exiting;
	}

	if( !m_Socket->GetConnecting( ) && !m_Socket->GetConnected( ) && !m_WaitingToConnect )
	{
		// the socket was disconnected

		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] disconnected from battle.net" );
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] waiting 90 seconds to reconnect" );
		m_GHost->EventBNETDisconnected( this );
		m_BNCSUtil->Reset( m_UserName, m_UserPassword );
		m_Socket->Reset( );
		m_LastDisconnectedTime = GetTime( );
		m_LoggedIn = false;
		m_InChat = false;
		m_WaitingToConnect = true;
		return m_Exiting;
	}

	if( m_Socket->GetConnected( ) )
	{
		// the socket is connected and everything appears to be working properly

		m_Socket->DoRecv( (fd_set *)fd );
		ExtractPackets( );
		ProcessPackets( );

		// check if at least one packet is waiting to be sent and if we've waited long enough to prevent flooding
		// this formula has changed many times but currently we wait 1 second if the last packet was "small", 3.5 seconds if it was "medium", and 4 seconds if it was "big"

		uint32_t WaitTicks = 0;

		if( m_LastOutPacketSize < 10 )
			WaitTicks = 1300;
		else if( m_LastOutPacketSize < 30 )
			WaitTicks = 3400;
		else if( m_LastOutPacketSize < 50 )
			WaitTicks = 3600;
		else if( m_LastOutPacketSize < 100 )
			WaitTicks = 3900;
		else
			WaitTicks = 5500;

		// add on frequency delay
		WaitTicks += m_FrequencyDelayTimes * 60;

		if( !m_OutPackets.empty( ) && GetTicks( ) - m_LastOutPacketTicks >= WaitTicks )
		{
			if( m_OutPackets.size( ) > 7 )
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] packet queue warning - there are " + UTIL_ToString( m_OutPackets.size( ) ) + " packets waiting to be sent" );

			m_Socket->PutBytes( m_OutPackets.front( ) );
			m_LastOutPacketSize = m_OutPackets.front( ).size( );
			m_OutPackets.pop( );
			// reset frequency delay (or increment it)
			if( m_FrequencyDelayTimes >= 100 || GetTicks( ) > m_LastOutPacketTicks + WaitTicks + 500 )
				m_FrequencyDelayTimes = 0;
			else
				m_FrequencyDelayTimes++;
			m_LastOutPacketTicks = GetTicks( );
		}

		// send a null packet every 60 seconds to detect disconnects

		if( GetTime( ) - m_LastNullTime >= 60 && GetTicks( ) - m_LastOutPacketTicks >= 60000 )
		{
			m_Socket->PutBytes( m_Protocol->SEND_SID_NULL( ) );
			m_LastNullTime = GetTime( );
		}

		m_Socket->DoSend( (fd_set *)send_fd );
		return m_Exiting;
	}

	if( m_Socket->GetConnecting( ) )
	{
		// we are currently attempting to connect to battle.net

		if( m_Socket->CheckConnect( ) )
		{
			// the connection attempt completed

			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] connected" );
			m_GHost->EventBNETConnected( this );
			m_Socket->PutBytes( m_Protocol->SEND_PROTOCOL_INITIALIZE_SELECTOR( ) );
			m_Socket->PutBytes( m_Protocol->SEND_SID_AUTH_INFO( m_War3Version, m_GHost->m_TFT, m_LocaleID, m_CountryAbbrev, m_Country ) );
			m_Socket->DoSend( (fd_set *)send_fd );
			m_LastNullTime = GetTime( );
			m_LastOutPacketTicks = GetTicks( );

			while( !m_OutPackets.empty( ) )
				m_OutPackets.pop( );

			return m_Exiting;
		}
		else if( GetTime( ) - m_LastConnectionAttemptTime >= 15 )
		{
			// the connection attempt timed out (15 seconds)

			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] connect timed out" );
			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] waiting 90 seconds to reconnect" );
			m_GHost->EventBNETConnectTimedOut( this );
			m_Socket->Reset( );
			m_LastDisconnectedTime = GetTime( );
			m_WaitingToConnect = true;
			return m_Exiting;
		}
	}

	if( !m_Socket->GetConnecting( ) && !m_Socket->GetConnected( ) && ( m_FirstConnect || GetTime( ) - m_LastDisconnectedTime >= 90 ) )
	{
		// attempt to connect to battle.net

		m_FirstConnect = false;
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] connecting to server [" + m_Server + "] on port 6112" );
		m_GHost->EventBNETConnecting( this );

		if( !m_GHost->m_BindAddress.empty( ) )
			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] attempting to bind to address [" + m_GHost->m_BindAddress + "]" );

		if( m_ServerIP.empty( ) )
		{
			m_Socket->Connect( m_GHost->m_BindAddress, m_Server, 6112 );

			if( !m_Socket->HasError( ) )
			{
				m_ServerIP = m_Socket->GetIPString( );
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] resolved and cached server IP address " + m_ServerIP );
			}
		}
		else
		{
			// use cached server IP address since resolving takes time and is blocking

			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] using cached server IP address " + m_ServerIP );
			m_Socket->Connect( m_GHost->m_BindAddress, m_ServerIP, 6112 );
		}

		m_WaitingToConnect = false;
		m_LastConnectionAttemptTime = GetTime( );
		return m_Exiting;
	}

	return m_Exiting;
}

void CBNET :: ExtractPackets( )
{
	// extract as many packets as possible from the socket's receive buffer and put them in the m_Packets queue

	string *RecvBuffer = m_Socket->GetBytes( );
	BYTEARRAY Bytes = UTIL_CreateByteArray( (unsigned char *)RecvBuffer->c_str( ), RecvBuffer->size( ) );

	// a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

	while( Bytes.size( ) >= 4 )
	{
		// byte 0 is always 255

		if( Bytes[0] == BNET_HEADER_CONSTANT )
		{
			// bytes 2 and 3 contain the length of the packet

			uint16_t Length = UTIL_ByteArrayToUInt16( Bytes, false, 2 );

			if( Length >= 4 )
			{
				if( Bytes.size( ) >= Length )
				{
					m_Packets.push( new CCommandPacket( BNET_HEADER_CONSTANT, Bytes[1], BYTEARRAY( Bytes.begin( ), Bytes.begin( ) + Length ) ) );
					*RecvBuffer = RecvBuffer->substr( Length );
					Bytes = BYTEARRAY( Bytes.begin( ) + Length, Bytes.end( ) );
				}
				else
					return;
			}
			else
			{
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error - received invalid packet from battle.net (bad length), disconnecting" );
				m_Socket->Disconnect( );
				return;
			}
		}
		else
		{
			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error - received invalid packet from battle.net (bad header constant), disconnecting" );
			m_Socket->Disconnect( );
			return;
		}
	}
}

void CBNET :: ProcessPackets( )
{
	CIncomingGameHost *GameHost = NULL;
	CIncomingChatEvent *ChatEvent = NULL;
	BYTEARRAY WardenData;
	//vector<CIncomingFriendList *> Friends;
	//vector<CIncomingClanList *> Clans;

	// process all the received packets in the m_Packets queue
	// this normally means sending some kind of response

	while( !m_Packets.empty( ) )
	{
		CCommandPacket *Packet = m_Packets.front( );
		m_Packets.pop( );

		if( Packet->GetPacketType( ) == BNET_HEADER_CONSTANT )
		{
			switch( Packet->GetID( ) )
			{
			case CBNETProtocol :: SID_NULL:
				// warning: we do not respond to NULL packets with a NULL packet of our own
				// this is because PVPGN servers are programmed to respond to NULL packets so it will create a vicious cycle of useless traffic
				// official battle.net servers do not respond to NULL packets

				m_Protocol->RECEIVE_SID_NULL( Packet->GetData( ) );
				break;

			case CBNETProtocol :: SID_GETADVLISTEX:
				GameHost = m_Protocol->RECEIVE_SID_GETADVLISTEX( Packet->GetData( ) );

				if( GameHost )
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] joining game [" + GameHost->GetGameName( ) + "]" );

				delete GameHost;
				GameHost = NULL;
				break;

			case CBNETProtocol :: SID_ENTERCHAT:
				if( m_Protocol->RECEIVE_SID_ENTERCHAT( Packet->GetData( ) ) )
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] joining channel [" + m_FirstChannel + "]" );
					m_InChat = true;
					m_Socket->PutBytes( m_Protocol->SEND_SID_JOINCHANNEL( m_FirstChannel ) );
				}

				break;

			case CBNETProtocol :: SID_CHATEVENT:
				ChatEvent = m_Protocol->RECEIVE_SID_CHATEVENT( Packet->GetData( ) );

				if( ChatEvent )
					ProcessChatEvent( ChatEvent );

				delete ChatEvent;
				ChatEvent = NULL;
				break;

			case CBNETProtocol :: SID_CHECKAD:
				m_Protocol->RECEIVE_SID_CHECKAD( Packet->GetData( ) );
				break;

			case CBNETProtocol :: SID_STARTADVEX3:
				if( m_Protocol->RECEIVE_SID_STARTADVEX3( Packet->GetData( ) ) )
				{
					m_InChat = false;
					m_GHost->EventBNETGameRefreshed( this );
				}
				else
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] startadvex3 failed" );
					m_GHost->EventBNETGameRefreshFailed( this );
				}

				break;

			case CBNETProtocol :: SID_PING:
				m_Socket->PutBytes( m_Protocol->SEND_SID_PING( m_Protocol->RECEIVE_SID_PING( Packet->GetData( ) ) ) );
				break;

			case CBNETProtocol :: SID_AUTH_INFO:
				if( m_Protocol->RECEIVE_SID_AUTH_INFO( Packet->GetData( ) ) )
				{
					if( m_BNCSUtil->HELP_SID_AUTH_CHECK( m_GHost->m_TFT, m_GHost->m_Warcraft3Path, m_CDKeyROC, m_CDKeyTFT, m_Protocol->GetValueStringFormulaString( ), m_Protocol->GetIX86VerFileNameString( ), m_Protocol->GetClientToken( ), m_Protocol->GetServerToken( ) ) )
					{
						// override the exe information generated by bncsutil if specified in the config file
						// apparently this is useful for pvpgn users

						if( m_EXEVersion.size( ) == 4 )
						{
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] using custom exe version bnet_custom_exeversion = " + UTIL_ToString( m_EXEVersion[0] ) + " " + UTIL_ToString( m_EXEVersion[1] ) + " " + UTIL_ToString( m_EXEVersion[2] ) + " " + UTIL_ToString( m_EXEVersion[3] ) );
							m_BNCSUtil->SetEXEVersion( m_EXEVersion );
						}

						if( m_EXEVersionHash.size( ) == 4 )
						{
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] using custom exe version hash bnet_custom_exeversionhash = " + UTIL_ToString( m_EXEVersionHash[0] ) + " " + UTIL_ToString( m_EXEVersionHash[1] ) + " " + UTIL_ToString( m_EXEVersionHash[2] ) + " " + UTIL_ToString( m_EXEVersionHash[3] ) );
							m_BNCSUtil->SetEXEVersionHash( m_EXEVersionHash );
						}

						if( m_GHost->m_TFT )
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] attempting to auth as Warcraft III: The Frozen Throne" );
						else
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] attempting to auth as Warcraft III: Reign of Chaos" );							

						m_Socket->PutBytes( m_Protocol->SEND_SID_AUTH_CHECK( m_GHost->m_TFT, m_Protocol->GetClientToken( ), m_BNCSUtil->GetEXEVersion( ), m_BNCSUtil->GetEXEVersionHash( ), m_BNCSUtil->GetKeyInfoROC( ), m_BNCSUtil->GetKeyInfoTFT( ), m_BNCSUtil->GetEXEInfo( ), "GHost" ) );

						// the Warden seed is the first 4 bytes of the ROC key hash
					}
					else
					{
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - bncsutil key hash failed (check your Warcraft 3 path and cd keys), disconnecting" );
						m_Socket->Disconnect( );
						delete Packet;
						return;
					}
				}

				break;

			case CBNETProtocol :: SID_AUTH_CHECK:
				if( m_Protocol->RECEIVE_SID_AUTH_CHECK( Packet->GetData( ) ) )
				{
					// cd keys accepted

					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] cd keys accepted" );
					m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGON( );
					m_Socket->PutBytes( m_Protocol->SEND_SID_AUTH_ACCOUNTLOGON( m_BNCSUtil->GetClientKey( ), m_UserName ) );
				}
				else
				{
					// cd keys not accepted

					switch( UTIL_ByteArrayToUInt32( m_Protocol->GetKeyState( ), false ) )
					{
					case CBNETProtocol :: KR_ROC_KEY_IN_USE:
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - ROC CD key in use by user [" + m_Protocol->GetKeyStateDescription( ) + "], disconnecting" );
						break;
					case CBNETProtocol :: KR_TFT_KEY_IN_USE:
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - TFT CD key in use by user [" + m_Protocol->GetKeyStateDescription( ) + "], disconnecting" );
						break;
					case CBNETProtocol :: KR_OLD_GAME_VERSION:
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - game version is too old, disconnecting" );
						break;
					case CBNETProtocol :: KR_INVALID_VERSION:
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - game version is invalid, disconnecting" );
						break;
					default:
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - cd keys not accepted, disconnecting" );
						break;
					}

					m_Socket->Disconnect( );
					delete Packet;
					return;
				}

				break;

			case CBNETProtocol :: SID_AUTH_ACCOUNTLOGON:
				if( m_Protocol->RECEIVE_SID_AUTH_ACCOUNTLOGON( Packet->GetData( ) ) )
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] username [" + m_UserName + "] accepted" );

					if( m_PasswordHashType == "pvpgn" )
					{
						// pvpgn logon

						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] using pvpgn logon type (for pvpgn servers only)" );
						m_BNCSUtil->HELP_PvPGNPasswordHash( m_UserPassword );
						m_Socket->PutBytes( m_Protocol->SEND_SID_AUTH_ACCOUNTLOGONPROOF( m_BNCSUtil->GetPvPGNPasswordHash( ) ) );
					}
					else
					{
						// battle.net logon

						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] using battle.net logon type (for official battle.net servers only)" );
						m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGONPROOF( m_Protocol->GetSalt( ), m_Protocol->GetServerPublicKey( ) );
						m_Socket->PutBytes( m_Protocol->SEND_SID_AUTH_ACCOUNTLOGONPROOF( m_BNCSUtil->GetM1( ) ) );
					}
				}
				else
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - invalid username, disconnecting" );
					m_Socket->Disconnect( );
					delete Packet;
					return;
				}

				break;

			case CBNETProtocol :: SID_AUTH_ACCOUNTLOGONPROOF:
				if( m_Protocol->RECEIVE_SID_AUTH_ACCOUNTLOGONPROOF( Packet->GetData( ) ) )
				{
					// logon successful

					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon successful" );
					m_LoggedIn = true;
					m_GHost->EventBNETLoggedIn( this );
					m_Socket->PutBytes( m_Protocol->SEND_SID_NETGAMEPORT( m_GHost->m_HostPort ) );
					m_Socket->PutBytes( m_Protocol->SEND_SID_ENTERCHAT( ) );
					m_Socket->PutBytes( m_Protocol->SEND_SID_FRIENDSLIST( ) );
					m_Socket->PutBytes( m_Protocol->SEND_SID_CLANMEMBERLIST( ) );
				}
				else
				{
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] logon failed - invalid password, disconnecting" );

					// try to figure out if the user might be using the wrong logon type since too many people are confused by this

					string Server = m_Server;
					transform( Server.begin( ), Server.end( ), Server.begin( ), (int(*)(int))tolower );

					if( m_PasswordHashType == "pvpgn" && ( Server == "useast.battle.net" || Server == "uswest.battle.net" || Server == "asia.battle.net" || Server == "europe.battle.net" ) )
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] it looks like you're trying to connect to a battle.net server using a pvpgn logon type, check your config file's \"battle.net custom data\" section" );
					else if( m_PasswordHashType != "pvpgn" && ( Server != "useast.battle.net" && Server != "uswest.battle.net" && Server != "asia.battle.net" && Server != "europe.battle.net" ) )
						CONSOLE_Print( "[BNET: " + m_ServerAlias + "] it looks like you're trying to connect to a pvpgn server using a battle.net logon type, check your config file's \"battle.net custom data\" section" );

					m_Socket->Disconnect( );
					delete Packet;
					return;
				}

				break;

			case CBNETProtocol :: SID_WARDEN:
				WardenData = m_Protocol->RECEIVE_SID_WARDEN( Packet->GetData( ) );

				/*if( m_BNLSClient )
					m_BNLSClient->QueueWardenRaw( WardenData );
				else
					CONSOLE_Print( "[BNET: " + m_ServerAlias + "] warning - received warden packet but no BNLS server is available, you will be kicked from battle.net soon" );*/

				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] warning - received warden packet but no BNLS server is available, you will be kicked from battle.net soon" );

				break;

			/*case CBNETProtocol :: SID_FRIENDSLIST:
				Friends = m_Protocol->RECEIVE_SID_FRIENDSLIST( Packet->GetData( ) );

                                for( vector<CIncomingFriendList *> :: iterator i = m_Friends.begin( ); i != m_Friends.end( ); ++i )
					delete *i;

				m_Friends = Friends;
				break;*/

			/*case CBNETProtocol :: SID_CLANMEMBERLIST:
				vector<CIncomingClanList *> Clans = m_Protocol->RECEIVE_SID_CLANMEMBERLIST( Packet->GetData( ) );

                                for( vector<CIncomingClanList *> :: iterator i = m_Clans.begin( ); i != m_Clans.end( ); ++i )
					delete *i;

				m_Clans = Clans;
				break;*/
			}
		}

		delete Packet;
	}
}

void CBNET :: ProcessChatEvent( CIncomingChatEvent *chatEvent )
{
	CBNETProtocol :: IncomingChatEvent Event = chatEvent->GetChatEvent( );
	bool Whisper = ( Event == CBNETProtocol :: EID_WHISPER );
	string User = chatEvent->GetUser( );
	string Message = chatEvent->GetMessage( );

	if( Event == CBNETProtocol :: EID_WHISPER /*|| Event == CBNETProtocol :: EID_TALK */)
	{
		/*if( Event == CBNETProtocol :: EID_WHISPER )
		{*/
			CONSOLE_Print( "[WHISPER: " + m_ServerAlias + "] [" + User + "] " + Message );
			m_GHost->EventBNETWhisper( this, User, Message );
		/*}
		else
		{
			CONSOLE_Print( "[LOCAL: " + m_ServerAlias + "] [" + User + "] " + Message );
			m_GHost->EventBNETChat( this, User, Message );
		}*/

		// handle spoof checking for current game
		// this case covers whispers - we assume that anyone who sends a whisper to the bot with message "spoofcheck" should be considered spoof checked
		// note that this means you can whisper "spoofcheck" even in a public game to manually spoofcheck if the /whois fails

		//if( Event == CBNETProtocol :: EID_WHISPER && m_GHost->m_CurrentGame )
		if( m_GHost->m_CurrentGame )
		{
			if( Message == "s" || Message == "sc" || Message == "spoof" || Message == "check" || Message == "spoofcheck" )
			{
				m_GHost->m_CurrentGame->AddToSpoofed( m_Server, User, true );
				
				//we check !min, !max and !games
				if (m_GHost->m_CurrentGame->GetGameType() == MASL_PROTOCOL :: DB_DIV1_DOTA_GAME)
				{
					((CDiv1DotAGame*)(m_GHost->m_CurrentGame))->CheckMinPSR();
					((CDiv1DotAGame*)(m_GHost->m_CurrentGame))->CheckMaxPSR();
					((CDiv1DotAGame*)(m_GHost->m_CurrentGame))->CheckMinGames(User);
				}
			}
			else if( Message.find( m_GHost->m_CurrentGame->GetGameName( ) ) != string :: npos )
			{
				// look for messages like "entered a Warcraft III The Frozen Throne game called XYZ"
				// we don't look for the English part of the text anymore because we want this to work with multiple languages
				// it's a pretty safe bet that anyone whispering the bot with a message containing the game name is a valid spoofcheck

				if( m_PasswordHashType == "pvpgn" && User == m_PVPGNRealmName )
				{
					// the equivalent pvpgn message is: [PvPGN Realm] Your friend abc has entered a Warcraft III Frozen Throne game named "xyz".

					vector<string> Tokens = UTIL_Tokenize( Message, ' ' );

					if( Tokens.size( ) >= 3 )
						m_GHost->m_CurrentGame->AddToSpoofed( m_Server, Tokens[2], false );
				}
				else
					m_GHost->m_CurrentGame->AddToSpoofed( m_Server, User, false );
			}
		}

		// handle bot commands

		/*if( Message == "?trigger" && ( IsAdmin( User ) || IsRootAdmin( User ) || ( m_PublicCommands && m_OutPackets.size( ) <= 3 ) ) )
			QueueChatCommand( m_GHost->m_Language->CommandTrigger( string( 1, m_CommandTrigger ) ), User, Whisper );
		else if( !Message.empty( ) && Message[0] == m_CommandTrigger )*/
		if( !Message.empty( ) && Message[0] == m_CommandTrigger )
		{
			// extract the command trigger, the command, and the payload
			// e.g. "!say hello world" -> command: "say", payload: "hello world"

			string Command;
			string Payload;
			string :: size_type PayloadStart = Message.find( " " );

			if( PayloadStart != string :: npos )
			{
				Command = Message.substr( 1, PayloadStart - 1 );
				Payload = Message.substr( PayloadStart + 1 );
			}
			else
				Command = Message.substr( 1 );

			transform( Command.begin( ), Command.end( ), Command.begin( ), (int(*)(int))tolower );

			//if( IsAdmin( User ) || IsRootAdmin( User ) )
			if( IsRootAdmin( User ) )
			{
				//CONSOLE_Print( "[BNET: " + m_ServerAlias + "] admin [" + User + "] sent command [" + Message + "]" );
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] root admin [" + User + "] sent command [" + Message + "]" );

				/*****************
				* ADMIN COMMANDS *
				******************/

				//
				// !ADDADMIN
				//

				//
				// !ADDBAN
				// !BAN
				//

				//
				// !ANNOUNCE
				//

				//
				// !AUTOHOST
				//

				//
				// !AUTOHOSTMM
				//

				//
				// !AUTOSTART
				//

				//
				// !BLOCK
				//

				if( Command == "block" )
				{
					if( Payload.empty( ) )
						SendChatCommand( "Currently blocking for " + UTIL_ToString( m_GHost->m_BlockMS ) + "ms.", User );
					else
					{
						m_GHost->m_BlockMS = UTIL_ToUInt32( Payload );
						SendChatCommand( "Blocking for " + UTIL_ToString( m_GHost->m_BlockMS ) + "ms.", User );
					}
				}

				//
				// !CHANNEL (change channel)
				//

				else if( Command == "channel" && !Payload.empty( ) )
					SendChatCommand( "/join " + Payload );

				//
				// !CHECKADMIN
				//

				//
				// !CHECKBAN
				//

				//
				// !CLOSE (close slot)
				//

				//
				// !CLOSEALL
				//

				//
				// !COUNTADMINS
				//

				//
				// !COUNTBANS
				//

				//
				// !DBSTATUS
				//

				//
				// !DELADMIN
				//

				//
				// !DELBAN
				// !UNBAN
				//

				//
				// !DISABLE
				//

				else if( Command == "disable" )
				{
					QueueChatCommand( m_GHost->m_Language->BotDisabled( ), User, Whisper );
					m_GHost->m_Enabled = false;
				}

				//
				// !DOWNLOADS
				//

				/*else if( Command == "downloads" && !Payload.empty( ) )
				{
					uint32_t Downloads = UTIL_ToUInt32( Payload );

					if( Downloads == 0 )
					{
						QueueChatCommand( m_GHost->m_Language->MapDownloadsDisabled( ), User, Whisper );
						m_GHost->m_AllowDownloads = 0;
					}
					else if( Downloads == 1 )
					{
						QueueChatCommand( m_GHost->m_Language->MapDownloadsEnabled( ), User, Whisper );
						m_GHost->m_AllowDownloads = 1;
					}
					else if( Downloads == 2 )
					{
						QueueChatCommand( m_GHost->m_Language->MapDownloadsConditional( ), User, Whisper );
						m_GHost->m_AllowDownloads = 2;
					}
				}*/

				//
				// !ENABLE
				//

				else if( Command == "enable" )
				{
					QueueChatCommand( m_GHost->m_Language->BotEnabled( ), User, Whisper );
					m_GHost->m_Enabled = true;
				}

				//
				// !END
				//

				//
				// !ENFORCESG
				//

				//
				// !EXIT
				// !QUIT
				//

				else if( Command == "exit" || Command == "quit" )
				{
					if( Payload == "nice" )
						m_GHost->m_ExitingNice = true;
					else if( Payload == "force" )
						m_Exiting = true;
					else
					{
						if( m_GHost->m_CurrentGame || !m_GHost->m_Games.empty( ) )
							QueueChatCommand( m_GHost->m_Language->AtLeastOneGameActiveUseForceToShutdown( ), User, Whisper );
						else
							m_Exiting = true;
					}
				}

				//
				// !GETCLAN
				//

				//
				// !GETFRIENDS
				//

				//
				// !GETGAME
				//

				else if( Command == "getgame" && !Payload.empty( ) )
				{
					uint32_t GameNumber = UTIL_ToUInt32( Payload ) - 1;

					if( GameNumber < m_GHost->m_Games.size( ) )
						QueueChatCommand( m_GHost->m_Language->GameNumberIs( Payload, m_GHost->m_Games[GameNumber]->GetDescription( ) ), User, Whisper );
					else
						QueueChatCommand( m_GHost->m_Language->GameNumberDoesntExist( Payload ), User, Whisper );
				}

				//
				// !GETGAMES
				//

				else if( Command == "getgames" )
				{
					if( m_GHost->m_CurrentGame )
						QueueChatCommand( m_GHost->m_Language->GameIsInTheLobby( m_GHost->m_CurrentGame->GetDescription( ), UTIL_ToString( m_GHost->m_Games.size( ) ), UTIL_ToString( m_GHost->m_MaxGames ) ), User, Whisper );
					else
						QueueChatCommand( m_GHost->m_Language->ThereIsNoGameInTheLobby( UTIL_ToString( m_GHost->m_Games.size( ) ), UTIL_ToString( m_GHost->m_MaxGames ) ), User, Whisper );
				}

				//
				// !HOLD (hold a slot for someone)
				//

				//
				// !HOSTSG
				//

				//
				// !LOAD (load config file)
				//

				else if( Command == "load" )
				{
					if( Payload.empty( ) )
						QueueChatCommand( m_GHost->m_Language->CurrentlyLoadedMapCFGIs( m_GHost->m_Map->GetCFGFile( ) ), User, Whisper );
					else
					{
						string FoundMapConfigs;

						try
						{
							path MapCFGPath( m_GHost->m_MapCFGPath );
							string Pattern = Payload;
							transform( Pattern.begin( ), Pattern.end( ), Pattern.begin( ), (int(*)(int))tolower );

							if( !exists( MapCFGPath ) )
							{
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing map configs - map config path doesn't exist" );
								QueueChatCommand( m_GHost->m_Language->ErrorListingMapConfigs( ), User, Whisper );
							}
							else
							{
								directory_iterator EndIterator;
								path LastMatch;
								uint32_t Matches = 0;

								for( directory_iterator i( MapCFGPath ); i != EndIterator; ++i )
								{
									string FileName = i->path( ).filename( ).string( );
									string Stem = i->path( ).stem( ).string( );
									transform( FileName.begin( ), FileName.end( ), FileName.begin( ), (int(*)(int))tolower );
									transform( Stem.begin( ), Stem.end( ), Stem.begin( ), (int(*)(int))tolower );

									if( !is_directory( i->status( ) ) && i->path( ).extension( ) == ".cfg" && FileName.find( Pattern ) != string :: npos )
									{
										LastMatch = i->path( );
										++Matches;

										if( FoundMapConfigs.empty( ) )
											FoundMapConfigs = i->path( ).filename( ).string( );
										else
											FoundMapConfigs += ", " + i->path( ).filename( ).string( );

										// if the pattern matches the filename exactly, with or without extension, stop any further matching

										if( FileName == Pattern || Stem == Pattern )
										{
											Matches = 1;
											break;
										}
									}
								}

								if( Matches == 0 )
									QueueChatCommand( m_GHost->m_Language->NoMapConfigsFound( ), User, Whisper );
								else if( Matches == 1 )
								{
									string File = LastMatch.filename( ).string( );
									QueueChatCommand( m_GHost->m_Language->LoadingConfigFile( m_GHost->m_MapCFGPath + File ), User, Whisper );
									CConfig MapCFG;
									MapCFG.Read( LastMatch.string( ) );
									m_GHost->m_Map->Load( &MapCFG, m_GHost->m_MapCFGPath + File );
								}
								else
									QueueChatCommand( m_GHost->m_Language->FoundMapConfigs( FoundMapConfigs ), User, Whisper );
							}
						}
						catch( const exception &ex )
						{
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing map configs - caught exception [" + ex.what( ) + "]" );
							QueueChatCommand( m_GHost->m_Language->ErrorListingMapConfigs( ), User, Whisper );
						}
					}
				}

				//
				// !LOADSG
				//

				//
				// !MAP (load map file)
				//

				else if( Command == "map" )
				{
					if( Payload.empty( ) )
						QueueChatCommand( m_GHost->m_Language->CurrentlyLoadedMapCFGIs( m_GHost->m_Map->GetCFGFile( ) ), User, Whisper );
					else
					{
						string FoundMaps;

						try
						{
							path MapPath( m_GHost->m_MapPath );
							string Pattern = Payload;
							transform( Pattern.begin( ), Pattern.end( ), Pattern.begin( ), (int(*)(int))tolower );

							if( !exists( MapPath ) )
							{
								CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing maps - map path doesn't exist" );
								QueueChatCommand( m_GHost->m_Language->ErrorListingMaps( ), User, Whisper );
							}
							else
							{
								directory_iterator EndIterator;
								path LastMatch;
								uint32_t Matches = 0;

								for( directory_iterator i( MapPath ); i != EndIterator; ++i )
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
									QueueChatCommand( m_GHost->m_Language->NoMapsFound( ), User, Whisper );
								else if( Matches == 1 )
								{
									string File = LastMatch.filename( ).string( );
									QueueChatCommand( m_GHost->m_Language->LoadingConfigFile( File ), User, Whisper );

									// hackhack: create a config file in memory with the required information to load the map

									CConfig MapCFG;
									MapCFG.Set( "map_path", "Maps\\Download\\" + File );
									MapCFG.Set( "map_localpath", File );
									m_GHost->m_Map->Load( &MapCFG, File );

									CConfig DotALadderMapObsCFG;
									DotALadderMapObsCFG.Set( "map_path", "Maps\\Download\\" + File );
									DotALadderMapObsCFG.Set( "map_localpath", File );
									DotALadderMapObsCFG.Set( "map_observers", "4" );
									m_GHost->m_DotALadderMapObs->Load( &DotALadderMapObsCFG, File );
								}
								else
									QueueChatCommand( m_GHost->m_Language->FoundMaps( FoundMaps ), User, Whisper );
							}
						}
						catch( const exception &ex )
						{
							CONSOLE_Print( "[BNET: " + m_ServerAlias + "] error listing maps - caught exception [" + ex.what( ) + "]" );
							QueueChatCommand( m_GHost->m_Language->ErrorListingMaps( ), User, Whisper );
						}
					}
				}

				//
				// !OPEN (open slot)
				//

				//
				// !OPENALL
				//

				//
				// !PRIV (host private game)
				//

				else if( ( Command == "gopriv" || Command == "priv" ) && !Payload.empty( ) )
					m_GHost->CreateGame( m_GHost->m_Map, GAME_PRIVATE, false, Payload, User, User, m_Server, 
					MASL_PROTOCOL::DB_DIV1_DOTA_GAME, false, vector<string>());

				//
				// !PRIVOBS (host private game with observers)
				//

				else if( ( Command == "goprivobs" || Command == "privobs" ) && !Payload.empty( ) )
					m_GHost->CreateGame( m_GHost->m_DotALadderMapObs, GAME_PRIVATE, false, Payload, User, 
					User, m_Server, MASL_PROTOCOL :: DB_DIV1_DOTA_GAME, true, vector<string>());

				//
				// !PRIVBY (host private game by other player)
				//

				//
				// !PUB (host public game)
				//

				else if( ( Command == "gopub" || Command == "pub" ) && !Payload.empty( ) )
					m_GHost->CreateGame( m_GHost->m_Map, GAME_PUBLIC, false, Payload, User, User, m_Server, 
						MASL_PROTOCOL::DB_DIV1_DOTA_GAME, false, vector<string>());

				//
				// !RELOAD
				//

				else if( Command == "reload" )
				{
					if( IsRootAdmin( User ) )
					{
						QueueChatCommand( m_GHost->m_Language->ReloadingConfigurationFiles( ), User, Whisper );
						m_GHost->ReloadConfigs( );
					}
					else
						QueueChatCommand( m_GHost->m_Language->YouDontHaveAccessToThatCommand( ), User, Whisper );
				}

				//
				// !GGHOSTNAME
				//

				else if( ( Command == "gghostname" || Command == "gghostname" ) && !Payload.empty( ) )
					m_GHost->m_GGHostName = Payload;

				//
				// !PUBBY (host public game by other player)
				//

				//
				// !RELOAD
				//

				//
				// !SAY
				//

				else if( Command == "say" && !Payload.empty( ) )
					QueueChatCommand( Payload );

				//
				// !SAYGAME
				//

				//
				// !SAYGAMES
				//

				//
				// !SP
				//

				//
				// !START
				//

				//
				// !SWAP (swap slots)
				//

				//
				// !UNHOST
				//

				else if( Command == "unhost" )
				{
					if( m_GHost->m_CurrentGame )
					{
						if( m_GHost->m_CurrentGame->GetCountDownStarted( ) )
							QueueChatCommand( m_GHost->m_Language->UnableToUnhostGameCountdownStarted( m_GHost->m_CurrentGame->GetDescription( ) ), User, Whisper );

						// if the game owner is still in the game only allow the root admin to unhost the game

						else if( m_GHost->m_CurrentGame->GetPlayerFromName( m_GHost->m_CurrentGame->GetOwnerName( ), false ) && !IsRootAdmin( User ) )
							QueueChatCommand( m_GHost->m_Language->CantUnhostGameOwnerIsPresent( m_GHost->m_CurrentGame->GetOwnerName( ) ), User, Whisper );
						else
						{
							QueueChatCommand( m_GHost->m_Language->UnhostingGame( m_GHost->m_CurrentGame->GetDescription( ) ), User, Whisper );
							m_GHost->m_CurrentGame->SetExiting( true );
						}
					}
					else
						QueueChatCommand( m_GHost->m_Language->UnableToUnhostGameNoGameInLobby( ), User, Whisper );
				}

				//
				// !REGION
				//

				else if( Command == "region" )
				{
				    QueueChatCommand(Regions::toString(m_GHost->m_region), User, Whisper );
				}

				//
				// !VERSION
				//

				else if( Command == "version" )
				{
					QueueChatCommand( m_GHost->m_Language->VersionAdmin( m_GHost->m_Version ), User, Whisper );
				}

				//
				// !WARDENSTATUS
				//
			}
			else
				CONSOLE_Print( "[BNET: " + m_ServerAlias + "] non-admin [" + User + "] sent command [" + Message + "]" );

			/*********************
			* NON ADMIN COMMANDS *
			*********************/

			// don't respond to non admins if there are more than 3 messages already in the queue
			// this prevents malicious users from filling up the bot's chat queue and crippling the bot
			// in some cases the queue may be full of legitimate messages but we don't really care if the bot ignores one of these commands once in awhile
			// e.g. when several users join a game at the same time and cause multiple /whois messages to be queued at once

			/*if( IsAdmin( User ) || IsRootAdmin( User ) || ( m_PublicCommands && m_OutPackets.size( ) <= 3 ) )
			{
				//
				// !STATS
				//

				//
				// !STATSDOTA
				//

				//
				// !VERSION
				//

				// moved to root admin only commands
			}*/
		}
	}
	else if( Event == CBNETProtocol :: EID_CHANNEL )
	{
		// keep track of current channel so we can rejoin it after hosting a game

		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] joined channel [" + Message + "]" );
		m_CurrentChannel = Message;
	}
	else if( Event == CBNETProtocol :: EID_INFO )
	{
		CONSOLE_Print( "[INFO: " + m_ServerAlias + "] " + Message );

		if (m_GHost->m_RequireMuteChecks) {
			
			// this could be the response from finger command
			// Each line is it's own message so we need to store it in a temporary buffer
			// to figure out which user is muted
			/*
			Login: me             #123456 Sex:
			Created: Fri Jan 30 20:13 2009
			Location:                         Age:
			Last login Mon Dec 13 13:51 2021 from 127.0.0.1
			Email: me@example.com , Operator: -1 , Admin: -1 , Locked: 0 , Muted: 0 , Ladder: -1
			Last login Owner: me
			*/
			if (Message.rfind("Login:", 0) == 0) {
				m_FingerChatBuffer.push_back(Message);

				if (m_FingerChatBuffer.size() > 10) {
					m_FingerChatBuffer.pop_front();
				}
			}
			else if (Message.rfind("Email:", 0) == 0 && !m_FingerChatBuffer.empty()) {

				string loginMessage = m_FingerChatBuffer.back();
				string::size_type firstSpacePos = loginMessage.find(" ");

				if (firstSpacePos != string::npos) {
					string::size_type secondSpacePos = loginMessage.find(" ", firstSpacePos + 1);

					if (secondSpacePos != string::npos) {
						string UserName = loginMessage.substr(firstSpacePos + 1, secondSpacePos - firstSpacePos - 1);
						size_t mutedPos = Message.find("Muted:");

						if (mutedPos != string::npos) {
							char mutedC = Message.at(mutedPos + 7);
							CONSOLE_Print("[INFO: " + m_ServerAlias + "] Muted value for username " + UserName + " is " + mutedC);

							if (mutedC == '1') {
								CGamePlayer *victim = m_GHost->m_CurrentGame->GetPlayerFromName(UserName, true);
								if (m_GHost->m_CurrentGame && victim)
								{
									victim->SetMuted(true);
									m_GHost->m_CurrentGame->SendAllChat(m_GHost->m_Language->MutedPlayerBanlist(victim->GetName()));
								}
							}
						}
					}
				}
			}
		}
		else {
			// extract the first word which we hope is the username
			// this is not necessarily true though since info messages also include channel MOTD's and such

			string UserName;
			string::size_type Split = Message.find(" ");

			if (Split != string::npos)
				UserName = Message.substr(0, Split);
			else
				UserName = Message.substr(0);

			// handle spoof checking for current game
			// this case covers whois results which are used when hosting a public game (we send out a "/whois [player]" for each player)
			// at all times you can still /w the bot with "spoofcheck" to manually spoof check

			if (m_GHost->m_CurrentGame && m_GHost->m_CurrentGame->GetPlayerFromName(UserName, true))
			{
				if (Message.find("is away") != string::npos)
					m_GHost->m_CurrentGame->SendAllChat(m_GHost->m_Language->SpoofPossibleIsAway(UserName));
				else if (Message.find("is unavailable") != string::npos)
					m_GHost->m_CurrentGame->SendAllChat(m_GHost->m_Language->SpoofPossibleIsUnavailable(UserName));
				else if (Message.find("is refusing messages") != string::npos)
					m_GHost->m_CurrentGame->SendAllChat(m_GHost->m_Language->SpoofPossibleIsRefusingMessages(UserName));
				else if (Message.find("is using Warcraft III The Frozen Throne in the channel") != string::npos)
					m_GHost->m_CurrentGame->SendAllChat(m_GHost->m_Language->SpoofDetectedIsNotInGame(UserName));
				else if (Message.find("is using Warcraft III The Frozen Throne in channel") != string::npos)
					m_GHost->m_CurrentGame->SendAllChat(m_GHost->m_Language->SpoofDetectedIsNotInGame(UserName));
				else if (Message.find("is using Warcraft III The Frozen Throne in a private channel") != string::npos)
					m_GHost->m_CurrentGame->SendAllChat(m_GHost->m_Language->SpoofDetectedIsInPrivateChannel(UserName));

				if (Message.find("is using Warcraft III The Frozen Throne in game") != string::npos || Message.find("is using Warcraft III Frozen Throne and is currently in  game") != string::npos)
				{
					// check both the current game name and the last game name against the /whois response
					// this is because when the game is rehosted, players who joined recently will be in the previous game according to battle.net
					// note: if the game is rehosted more than once it is possible (but unlikely) for a false positive because only two game names are checked

					if (Message.find(m_GHost->m_CurrentGame->GetGameName()) != string::npos || Message.find(m_GHost->m_CurrentGame->GetLastGameName()) != string::npos)
						m_GHost->m_CurrentGame->AddToSpoofed(m_Server, UserName, false);
					else
						m_GHost->m_CurrentGame->SendAllChat(m_GHost->m_Language->SpoofDetectedIsInAnotherGame(UserName));
				}
			}
		}
	}
	else if( Event == CBNETProtocol :: EID_ERROR )
		CONSOLE_Print( "[ERROR: " + m_ServerAlias + "] " + Message );
	else if( Event == CBNETProtocol :: EID_EMOTE )
	{
		CONSOLE_Print( "[EMOTE: " + m_ServerAlias + "] [" + User + "] " + Message );
		m_GHost->EventBNETEmote( this, User, Message );
	}
}

void CBNET :: SendJoinChannel( string channel )
{
	if( m_LoggedIn && m_InChat )
		m_Socket->PutBytes( m_Protocol->SEND_SID_JOINCHANNEL( channel ) );
}

/*void CBNET :: SendGetFriendsList( )
{
	if( m_LoggedIn )
		m_Socket->PutBytes( m_Protocol->SEND_SID_FRIENDSLIST( ) );
}*/

/*void CBNET :: SendGetClanList( )
{
	if( m_LoggedIn )
		m_Socket->PutBytes( m_Protocol->SEND_SID_CLANMEMBERLIST( ) );
}*/

void CBNET :: QueueEnterChat( )
{
	if( m_LoggedIn )
		m_OutPackets.push( m_Protocol->SEND_SID_ENTERCHAT( ) );
}

void CBNET :: QueueChatCommand( string chatCommand )
{
	if( chatCommand.empty( ) )
		return;

	if( m_LoggedIn )
	{
		if( m_PasswordHashType == "pvpgn" && chatCommand.size( ) > m_MaxMessageLength )
			chatCommand = chatCommand.substr( 0, m_MaxMessageLength );

		if( chatCommand.size( ) > 255 )
			chatCommand = chatCommand.substr( 0, 255 );

		if( m_OutPackets.size( ) > 10 )
			CONSOLE_Print( "[BNET: " + m_ServerAlias + "] attempted to queue chat command [" + chatCommand + "] but there are too many (" + UTIL_ToString( m_OutPackets.size( ) ) + ") packets queued, discarding" );
		else
		{
			CONSOLE_Print( "[QUEUED: " + m_ServerAlias + "] " + chatCommand );
			m_OutPackets.push( m_Protocol->SEND_SID_CHATCOMMAND( chatCommand ) );
		}
	}
}

void CBNET :: QueueChatCommand( string chatCommand, string user, bool whisper )
{
	if( chatCommand.empty( ) )
		return;

	// if whisper is true send the chat command as a whisper to user, otherwise just queue the chat command

	if( whisper )
		QueueChatCommand( "/w " + user + " " + chatCommand );
	else
		QueueChatCommand( chatCommand );
}

void CBNET :: SendChatCommand( string chatCommand )
{
	if( m_PasswordHashType != "pvpgn" )
		return;

	if( chatCommand.empty( ) )
		return;

	if( m_LoggedIn )
	{
		if( m_PasswordHashType == "pvpgn" && chatCommand.size( ) > m_MaxMessageLength )
			chatCommand = chatCommand.substr( 0, m_MaxMessageLength );

		if( chatCommand.size( ) > 255 )
			chatCommand = chatCommand.substr( 0, 255 );

		CONSOLE_Print( "[SENT: " + m_ServerAlias + "] " + chatCommand );
		m_Socket->PutBytes( m_Protocol->SEND_SID_CHATCOMMAND( chatCommand ) );
	}
}

void CBNET :: SendChatCommand( string chatCommand, string user )
{
	if( chatCommand.empty( ) )
		return;

	// if whisper is true send the chat command as a whisper to user, otherwise just queue the chat command

	if( !user.empty( ) )
		SendChatCommand( "/w " + user + " " + chatCommand );
	else
		SendChatCommand( chatCommand );
}

void CBNET :: QueueGameCreate( unsigned char state, string gameName, string hostName, CMap *map, CSaveGame *savegame, uint32_t hostCounter )
{
	if( m_LoggedIn && map )
	{
		if( !m_CurrentChannel.empty( ) )
			m_FirstChannel = m_CurrentChannel;

		m_InChat = false;

		// a game creation message is just a game refresh message with upTime = 0

		//QueueGameRefresh( state, gameName, hostName, map, savegame, 0, hostCounter );

		string GGHostName = m_GHost->m_GGHostName;

		if( GGHostName.empty( ) )
			GGHostName = hostName;

		QueueGameRefresh( state, gameName, GGHostName, map, savegame, 0, hostCounter );
	}
}

void CBNET :: QueueGameRefresh( unsigned char state, string gameName, string hostName, CMap *map, CSaveGame *saveGame, uint32_t upTime, uint32_t hostCounter )
{
	if( hostName.empty( ) )
	{
		BYTEARRAY UniqueName = m_Protocol->GetUniqueName( );
		hostName = string( UniqueName.begin( ), UniqueName.end( ) );
	}

	if( m_LoggedIn && map )
	{
		// construct a fixed host counter which will be used to identify players from this realm
		// the fixed host counter's 4 most significant bits will contain a 4 bit ID (0-15)
		// the rest of the fixed host counter will contain the 28 least significant bits of the actual host counter
		// since we're destroying 4 bits of information here the actual host counter should not be greater than 2^28 which is a reasonable assumption
		// when a player joins a game we can obtain the ID from the received host counter
		// note: LAN broadcasts use an ID of 0, battle.net refreshes use an ID of 1-10, the rest are unused

		uint32_t FixedHostCounter = ( hostCounter & 0x0FFFFFFF ) | ( m_HostCounterID << 28 );

		if( saveGame )
		{
			uint32_t MapGameType = MAPGAMETYPE_SAVEDGAME;

			// the state should always be private when creating a saved game

			if( state == GAME_PRIVATE )
				MapGameType |= MAPGAMETYPE_PRIVATEGAME;

			// use an invalid map width/height to indicate reconnectable games

			BYTEARRAY MapWidth;
			MapWidth.push_back( 192 );
			MapWidth.push_back( 7 );
			BYTEARRAY MapHeight;
			MapHeight.push_back( 192 );
			MapHeight.push_back( 7 );

			if( m_GHost->m_Reconnect )
				m_OutPackets.push( m_Protocol->SEND_SID_STARTADVEX3( state, UTIL_CreateByteArray( MapGameType, false ), map->GetMapGameFlags( ), MapWidth, MapHeight, gameName, hostName, upTime, "Save\\Multiplayer\\" + saveGame->GetFileNameNoPath( ), saveGame->GetMagicNumber( ), map->GetMapSHA1( ), FixedHostCounter ) );
			else
				m_OutPackets.push( m_Protocol->SEND_SID_STARTADVEX3( state, UTIL_CreateByteArray( MapGameType, false ), map->GetMapGameFlags( ), UTIL_CreateByteArray( (uint16_t)0, false ), UTIL_CreateByteArray( (uint16_t)0, false ), gameName, hostName, upTime, "Save\\Multiplayer\\" + saveGame->GetFileNameNoPath( ), saveGame->GetMagicNumber( ), map->GetMapSHA1( ), FixedHostCounter ) );
		}
		else
		{
			uint32_t MapGameType = map->GetMapGameType( );
			MapGameType |= MAPGAMETYPE_UNKNOWN0;

			if( state == GAME_PRIVATE )
				MapGameType |= MAPGAMETYPE_PRIVATEGAME;

			// use an invalid map width/height to indicate reconnectable games

			BYTEARRAY MapWidth;
			MapWidth.push_back( 192 );
			MapWidth.push_back( 7 );
			BYTEARRAY MapHeight;
			MapHeight.push_back( 192 );
			MapHeight.push_back( 7 );

			if( m_GHost->m_Reconnect )
				m_OutPackets.push( m_Protocol->SEND_SID_STARTADVEX3( state, UTIL_CreateByteArray( MapGameType, false ), map->GetMapGameFlags( ), MapWidth, MapHeight, gameName, hostName, upTime, map->GetMapPath( ), map->GetMapCRC( ), map->GetMapSHA1( ), FixedHostCounter ) );
			else
				m_OutPackets.push( m_Protocol->SEND_SID_STARTADVEX3( state, UTIL_CreateByteArray( MapGameType, false ), map->GetMapGameFlags( ), map->GetMapWidth( ), map->GetMapHeight( ), gameName, hostName, upTime, map->GetMapPath( ), map->GetMapCRC( ), map->GetMapSHA1( ), FixedHostCounter ) );
		}
	}
}

void CBNET :: QueueGameUncreate( )
{
	if( m_LoggedIn )
		m_OutPackets.push( m_Protocol->SEND_SID_STOPADV( ) );
}

void CBNET :: UnqueuePackets( unsigned char type )
{
	queue<BYTEARRAY> Packets;
	uint32_t Unqueued = 0;

	while( !m_OutPackets.empty( ) )
	{
		// todotodo: it's very inefficient to have to copy all these packets while searching the queue

		BYTEARRAY Packet = m_OutPackets.front( );
		m_OutPackets.pop( );

		if( Packet.size( ) >= 2 && Packet[1] == type )
                        ++Unqueued;
		else
			Packets.push( Packet );
	}

	m_OutPackets = Packets;

	if( Unqueued > 0 )
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] unqueued " + UTIL_ToString( Unqueued ) + " packets of type " + UTIL_ToString( type ) );
}

void CBNET :: UnqueueChatCommand( string chatCommand )
{
	// hackhack: this is ugly code
	// generate the packet that would be sent for this chat command
	// then search the queue for that exact packet

	BYTEARRAY PacketToUnqueue = m_Protocol->SEND_SID_CHATCOMMAND( chatCommand );
	queue<BYTEARRAY> Packets;
	uint32_t Unqueued = 0;

	while( !m_OutPackets.empty( ) )
	{
		// todotodo: it's very inefficient to have to copy all these packets while searching the queue

		BYTEARRAY Packet = m_OutPackets.front( );
		m_OutPackets.pop( );

		if( Packet == PacketToUnqueue )
			++Unqueued;
		else
			Packets.push( Packet );
	}

	m_OutPackets = Packets;

	if( Unqueued > 0 )
		CONSOLE_Print( "[BNET: " + m_ServerAlias + "] unqueued " + UTIL_ToString( Unqueued ) + " chat command packets" );
}

void CBNET :: UnqueueGameRefreshes( )
{
	UnqueuePackets( CBNETProtocol :: SID_STARTADVEX3 );
}

/*bool CBNET :: IsAdmin( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

        for( vector<string> :: iterator i = m_Admins.begin( ); i != m_Admins.end( ); ++i )
	{
		if( *i == name )
			return true;
	}

	return false;
}*/

bool CBNET :: IsRootAdmin( string name )
{
	// m_RootAdmin was already transformed to lower case in the constructor

	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	// updated to permit multiple root admins seperated by a space, e.g. "Varlock Kilranin Instinct121"
	// note: this function gets called frequently so it would be better to parse the root admins just once and store them in a list somewhere
	// however, it's hardly worth optimizing at this point since the code's already written

	stringstream SS;
	string s;
	SS << m_RootAdmin;

	while( !SS.eof( ) )
	{
		SS >> s;

		if( name == s )
			return true;
	}

	return false;
}

/*CDBBan *CBNET :: IsBannedName( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	// todotodo: optimize this - maybe use a map?

        for( vector<CDBBan *> :: iterator i = m_Bans.begin( ); i != m_Bans.end( ); ++i )
	{
		if( (*i)->GetName( ) == name )
			return *i;
	}

	return NULL;
}*/

/*CDBBan *CBNET :: IsBannedIP( string ip )
{
	// todotodo: optimize this - maybe use a map?

        for( vector<CDBBan *> :: iterator i = m_Bans.begin( ); i != m_Bans.end( ); ++i )
	{
		if( (*i)->GetIP( ) == ip )
			return *i;
	}

	return NULL;
}*/

/*void CBNET :: AddAdmin( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	m_Admins.push_back( name );
}*/

/*void CBNET :: AddBan( string name, string ip, string gamename, string admin, string reason )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	m_Bans.push_back( new CDBBan( m_Server, name, ip, "N/A", gamename, admin, reason ) );
}*/

/*void CBNET :: RemoveAdmin( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( vector<string> :: iterator i = m_Admins.begin( ); i != m_Admins.end( ); )
	{
		if( *i == name )
			i = m_Admins.erase( i );
		else
                        ++i;
	}
}*/

/*void CBNET :: RemoveBan( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( vector<CDBBan *> :: iterator i = m_Bans.begin( ); i != m_Bans.end( ); )
	{
		if( (*i)->GetName( ) == name )
			i = m_Bans.erase( i );
		else
                        ++i;
	}
}*/

/*void CBNET :: HoldFriends( CBaseGame *game )
{
	if( game )
	{
                for( vector<CIncomingFriendList *> :: iterator i = m_Friends.begin( ); i != m_Friends.end( ); ++i )
			game->AddToReserved( (*i)->GetAccount( ) );
	}
}*/

/*void CBNET :: HoldClan( CBaseGame *game )
{
	if( game )
	{
                for( vector<CIncomingClanList *> :: iterator i = m_Clans.begin( ); i != m_Clans.end( ); ++i )
			game->AddToReserved( (*i)->GetName( ) );
	}
}*/
