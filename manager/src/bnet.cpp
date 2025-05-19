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
#include "ghostdb.h"
#include "bncsutilinterface.h"
#include "bnetprotocol.h"
#include "bnet.h"
#include "masl_manager.h"
#include "masl_protocol_2.h"
#include "masl_slavebot.h"
#include "regions.h"
#include <unordered_map>

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

using namespace boost::filesystem;

//
// CBNET
//

CBNET::CBNET(CGHost* nGHost, string nServer, string nServerAlias, string nBNLSServer, uint16_t nBNLSPort, uint32_t nBNLSWardenCookie, string nCDKeyROC, string nCDKeyTFT, string nCountryAbbrev, string nCountry, string nUserName, string nUserPassword, string nFirstChannel, string nRootAdmin, char nCommandTrigger, bool nPublicCommands, unsigned char nWar3Version, BYTEARRAY nEXEVersion, BYTEARRAY nEXEVersionHash, string nPasswordHashType, uint32_t nMaxMessageLength, uint32_t nServerID)
{
	// todotodo: append path seperator to Warcraft3Path if needed

	m_GHost = nGHost;
	m_Socket = new CTCPClient();
	m_Protocol = new CBNETProtocol();
	m_BNCSUtil = new CBNCSUtilInterface(nUserName, nUserPassword);
	m_Exiting = false;
	m_Server = nServer;
	string LowerServer = m_Server;
	transform(LowerServer.begin(), LowerServer.end(), LowerServer.begin(), (int(*)(int))tolower);

	if (!nServerAlias.empty())
		m_ServerAlias = nServerAlias;
	else if (LowerServer == "useast.battle.net")
		m_ServerAlias = "USEast";
	else if (LowerServer == "uswest.battle.net")
		m_ServerAlias = "USWest";
	else if (LowerServer == "asia.battle.net")
		m_ServerAlias = "Asia";
	else if (LowerServer == "europe.battle.net")
		m_ServerAlias = "Europe";
	else
		m_ServerAlias = m_Server;

	m_CDKeyROC = nCDKeyROC;
	m_CDKeyTFT = nCDKeyTFT;

	// remove dashes from CD keys and convert to uppercase

	m_CDKeyROC.erase(remove(m_CDKeyROC.begin(), m_CDKeyROC.end(), '-'), m_CDKeyROC.end());
	m_CDKeyTFT.erase(remove(m_CDKeyTFT.begin(), m_CDKeyTFT.end(), '-'), m_CDKeyTFT.end());
	transform(m_CDKeyROC.begin(), m_CDKeyROC.end(), m_CDKeyROC.begin(), (int(*)(int))toupper);
	transform(m_CDKeyTFT.begin(), m_CDKeyTFT.end(), m_CDKeyTFT.begin(), (int(*)(int))toupper);

	if (m_CDKeyROC.size() != 26)
	{
		CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] warning - your ROC CD key is not 26 characters long and is probably invalid");
		BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] warning - your ROC CD key is not 26 characters long and is probably invalid");
	}

	if (m_CDKeyTFT.size() != 26)
	{
		CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] warning - your TFT CD key is not 26 characters long and is probably invalid");
		BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] warning - your TFT CD key is not 26 characters long and is probably invalid");
	}

	m_CountryAbbrev = nCountryAbbrev;
	m_Country = nCountry;
	m_UserName = nUserName;
	m_UserPassword = nUserPassword;
	m_FirstChannel = nFirstChannel;
	m_RootAdmin = nRootAdmin;
	transform(m_RootAdmin.begin(), m_RootAdmin.end(), m_RootAdmin.begin(), (int(*)(int))tolower);
	m_CommandTrigger = nCommandTrigger;
	m_War3Version = nWar3Version;
	m_EXEVersion = nEXEVersion;
	m_EXEVersionHash = nEXEVersionHash;
	m_PasswordHashType = nPasswordHashType;
	m_MaxMessageLength = nMaxMessageLength;
	m_NextConnectTime = GetTime();
	m_LastNullTime = 0;
	m_LastOutPacketTicks = 0;
	m_LastOutPacketSize = 0;
	m_WaitingToConnect = true;
	m_LoggedIn = false;
	m_InChat = false;

	m_ServerID = nServerID;

	m_CallableContributorList = m_GHost->m_DB->ThreadedContributorList(m_ServerID);
	m_LastContributorRefreshTime = GetTime();
}

CBNET :: ~CBNET()
{
	delete m_Socket;
	delete m_Protocol;

	while (!m_Packets.empty())
	{
		delete m_Packets.front();
		m_Packets.pop();
	}

	delete m_BNCSUtil;
}

BYTEARRAY CBNET::GetUniqueName()
{
	return m_Protocol->GetUniqueName();
}

unsigned int CBNET::SetFD(void* fd, void* send_fd, int* nfds)
{
	unsigned int NumFDs = 0;

	if (!m_Socket->HasError() && m_Socket->GetConnected())
	{
		m_Socket->SetFD((fd_set*)fd, (fd_set*)send_fd, nfds);
		NumFDs++;
	}

	return NumFDs;
}

bool CBNET::Update(void* fd, void* send_fd)
{
	if (m_CallableContributorList && m_CallableContributorList->GetReady())
	{
		//CONSOLE_Print( "[BNET: " + m_ServerAlias + " " + m_UserName + "] updated contributor list, total size of " + UTIL_ToString( m_Contributors.size( ) ) + " rows" );
		m_Contributors = m_CallableContributorList->GetResult();

		delete m_CallableContributorList;
		m_CallableContributorList = NULL;
		m_LastContributorRefreshTime = GetTime();
	}

	if (!m_CallableContributorList && GetTime() - m_LastContributorRefreshTime >= 60)
		m_CallableContributorList = m_GHost->m_DB->ThreadedContributorList(m_ServerID);

	// update loaded maps, delete the loaded map when it's older then 2 minutes

	if (m_GHost->m_RPGMode)
	{
		for (vector<CLoadedMap*> ::iterator i = m_LoadedMaps.begin(); i != m_LoadedMaps.end(); )
		{
			if (GetTime() - (*i)->GetLoadedTime() > 120)
			{
				delete* i;
				i = m_LoadedMaps.erase(i);
			}
			else
				i++;
		}
	}

	// we return at the end of each if statement so we don't have to deal with errors related to the order of the if statements
	// that means it might take a few ms longer to complete a task involving multiple steps (in this case, reconnecting) due to blocking or sleeping
	// but it's not a big deal at all, maybe 100ms in the worst possible case (based on a 50ms blocking time)

	if (m_Socket->HasError())
	{
		// the socket has an error

		CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] disconnected from battle.net due to socket error");
		CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] waiting 90 seconds to reconnect");

		BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] disconnected from battle.net due to socket error");
		BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] waiting 90 seconds to reconnect");

		m_GHost->EventBNETDisconnected(this);
		m_BNCSUtil->Reset(m_UserName, m_UserPassword);
		m_Socket->Reset();
		m_NextConnectTime = GetTime() + 90;
		m_LoggedIn = false;
		m_InChat = false;
		m_WaitingToConnect = true;
		return m_Exiting;
	}

	if (!m_Socket->GetConnecting() && !m_Socket->GetConnected() && !m_WaitingToConnect)
	{
		// the socket was disconnected

		CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] disconnected from battle.net due to socket not connected");
		CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] waiting 90 seconds to reconnect");

		BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] disconnected from battle.net due to socket not connected");
		BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] waiting 90 seconds to reconnect");

		m_GHost->EventBNETDisconnected(this);
		m_BNCSUtil->Reset(m_UserName, m_UserPassword);
		m_Socket->Reset();
		m_NextConnectTime = GetTime() + 90;
		m_LoggedIn = false;
		m_InChat = false;
		m_WaitingToConnect = true;
		return m_Exiting;
	}

	if (m_Socket->GetConnected())
	{
		// the socket is connected and everything appears to be working properly

		m_Socket->DoRecv((fd_set*)fd);
		ExtractPackets();
		ProcessPackets();

		// check if at least one packet is waiting to be sent and if we've waited long enough to prevent flooding
		// this formula has changed many times but currently we wait 1 second if the last packet was "small", 3.5 seconds if it was "medium", and 4 seconds if it was "big"

		uint32_t WaitTicks = 0;

		if (m_LastOutPacketSize < 10)
			WaitTicks = 1000;
		else if (m_LastOutPacketSize < 100)
			WaitTicks = 3500;
		else
			WaitTicks = 4000;

		if (!m_OutPackets.empty() && GetTicks() >= m_LastOutPacketTicks + WaitTicks)
		{
			if (m_OutPackets.size() > 7)
			{
				CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] packet queue warning - there are " + UTIL_ToString(m_OutPackets.size()) + " packets waiting to be sent");
				BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] packet queue warning - there are " + UTIL_ToString(m_OutPackets.size()) + " packets waiting to be sent");
			}

			m_Socket->PutBytes(m_OutPackets.front());
			m_LastOutPacketSize = m_OutPackets.front().size();
			m_OutPackets.pop();
			m_LastOutPacketTicks = GetTicks();
		}

		// send a null packet every 60 seconds to detect disconnects

		if (GetTime() >= m_LastNullTime + 60 && GetTicks() >= m_LastOutPacketTicks + 60000)
		{
			m_Socket->PutBytes(m_Protocol->SEND_SID_NULL());
			m_LastNullTime = GetTime();
		}

		// try to send a queued whisper
		for (vector< pair<string, uint32_t> >::iterator it = m_DelayedWhispers.begin(); it != m_DelayedWhispers.end();)
		{
			if (GetTicks() > it->second)
			{
				SendChatCommand(it->first);
				it = m_DelayedWhispers.erase(it);
			}
			else {
				++it;
			}
		}

		m_Socket->DoSend((fd_set*)send_fd);
		return m_Exiting;
	}

	if (m_Socket->GetConnecting())
	{
		// we are currently attempting to connect to battle.net

		if (m_Socket->CheckConnect())
		{
			// the connection attempt completed

			CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] connected");
			BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] connected");

			m_GHost->EventBNETConnected(this);
			m_Socket->PutBytes(m_Protocol->SEND_PROTOCOL_INITIALIZE_SELECTOR());
			m_Socket->PutBytes(m_Protocol->SEND_SID_AUTH_INFO(m_War3Version, m_CountryAbbrev, m_Country));
			m_Socket->DoSend((fd_set*)send_fd);
			m_LastNullTime = GetTime();
			m_LastOutPacketTicks = GetTicks();

			while (!m_OutPackets.empty())
				m_OutPackets.pop();

			return m_Exiting;
		}
		else if (GetTime() >= m_NextConnectTime + 15)
		{
			// the connection attempt timed out (15 seconds)

			CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] connect timed out");
			CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] waiting 90 seconds to reconnect");

			BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] connect timed out");
			BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] waiting 90 seconds to reconnect");

			m_GHost->EventBNETConnectTimedOut(this);
			m_Socket->Reset();
			m_NextConnectTime = GetTime() + 90;
			m_WaitingToConnect = true;
			return m_Exiting;
		}
	}

	if (!m_Socket->GetConnecting() && !m_Socket->GetConnected() && GetTime() >= m_NextConnectTime)
	{
		// attempt to connect to battle.net

		CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] connecting to server [" + m_Server + "] on port 6112");
		BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] connecting to server [" + m_Server + "] on port 6112");
		m_GHost->EventBNETConnecting(this);

		if (!m_GHost->m_BindAddress.empty())
		{
			CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] attempting to bind to address [" + m_GHost->m_BindAddress + "]");
			BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] attempting to bind to address [" + m_GHost->m_BindAddress + "]");
		}

		if (m_ServerIP.empty())
		{
			m_Socket->Connect(m_GHost->m_BindAddress, m_Server, 6112);

			if (!m_Socket->HasError())
			{
				// don't cache IPs for pvpgn servers

				if (m_PasswordHashType == "pvpgn")
				{
					CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] resolved (but not cached) pvpgn server IP address");
					BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] resolved (but not cached) pvpgn server IP address");
				}
				else
				{
					m_ServerIP = m_Socket->GetIPString();
					CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] resolved and cached server IP address " + m_ServerIP);
					BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] resolved and cached server IP address " + m_ServerIP);
				}
			}
		}
		else
		{
			// use cached server IP address since resolving takes time and is blocking

			CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] using cached server IP address " + m_ServerIP);
			BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] using cached server IP address " + m_ServerIP);
			m_Socket->Connect(m_GHost->m_BindAddress, m_ServerIP, 6112);
		}

		m_WaitingToConnect = false;
		return m_Exiting;
	}

	return m_Exiting;
}

void CBNET::ExtractPackets()
{
	// extract as many packets as possible from the socket's receive buffer and put them in the m_Packets queue

	string* RecvBuffer = m_Socket->GetBytes();
	BYTEARRAY Bytes = UTIL_CreateByteArray((unsigned char*)RecvBuffer->c_str(), RecvBuffer->size());

	// a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

	while (Bytes.size() >= 4)
	{
		// byte 0 is always 255

		if (Bytes[0] == BNET_HEADER_CONSTANT)
		{
			// bytes 2 and 3 contain the length of the packet

			uint16_t Length = UTIL_ByteArrayToUInt16(Bytes, false, 2);

			if (Length >= 4)
			{
				if (Bytes.size() >= Length)
				{
					m_Packets.push(new CCommandPacket(BNET_HEADER_CONSTANT, Bytes[1], BYTEARRAY(Bytes.begin(), Bytes.begin() + Length)));
					*RecvBuffer = RecvBuffer->substr(Length);
					Bytes = BYTEARRAY(Bytes.begin() + Length, Bytes.end());
				}
				else
					return;
			}
			else
			{
				CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] error - received invalid packet from battle.net (bad length), disconnecting");
				BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] error - received invalid packet from battle.net (bad length), disconnecting");
				m_Socket->Disconnect();
				return;
			}
		}
		else
		{
			CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] error - received invalid packet from battle.net (bad header constant), disconnecting");
			BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] error - received invalid packet from battle.net (bad header constant), disconnecting");
			m_Socket->Disconnect();
			return;
		}
	}
}

void CBNET::ProcessPackets()
{
	CIncomingGameHost* GameHost = NULL;
	CIncomingChatEvent* ChatEvent = NULL;
	BYTEARRAY WardenData;
	//vector<CIncomingFriendList *> Friends;
	//vector<CIncomingClanList *> Clans;

	// process all the received packets in the m_Packets queue
	// this normally means sending some kind of response

	while (!m_Packets.empty())
	{
		CCommandPacket* Packet = m_Packets.front();
		m_Packets.pop();

		/*CONSOLE_Print( "GetPacketType = " + UTIL_ToString( (int)Packet->GetPacketType( ) ) );
		CONSOLE_Print( "GetID = " + UTIL_ToString( Packet->GetID( ) ) );
		CONSOLE_Print( "GetData.size = " + UTIL_ToString( Packet->GetData( ).size( ) ) );
		BYTEARRAY DDData = Packet->GetData( );
		DEBUG_Print( Packet->GetData( ) );
		CONSOLE_Print( "GetData = " + string( DDData.begin( ), DDData.end( ) ) );
		CONSOLE_Print( "-----------" );*/

		if (Packet->GetPacketType() == BNET_HEADER_CONSTANT)
		{
			switch (Packet->GetID())
			{
			case CBNETProtocol::SID_NULL:
				// warning: we do not respond to NULL packets with a NULL packet of our own
				// this is because PVPGN servers are programmed to respond to NULL packets so it will create a vicious cycle of useless traffic
				// official battle.net servers do not respond to NULL packets

				m_Protocol->RECEIVE_SID_NULL(Packet->GetData());
				break;

			case CBNETProtocol::SID_GETADVLISTEX:
				GameHost = m_Protocol->RECEIVE_SID_GETADVLISTEX(Packet->GetData());

				if (GameHost)
				{
					CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] joining game [" + GameHost->GetGameName() + "]");
					BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] joining game [" + GameHost->GetGameName() + "]");
				}

				delete GameHost;
				GameHost = NULL;
				break;

			case CBNETProtocol::SID_ENTERCHAT:
				if (m_Protocol->RECEIVE_SID_ENTERCHAT(Packet->GetData()))
				{
					CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] joining channel [" + m_FirstChannel + "]");
					BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] joining channel [" + m_FirstChannel + "]");
					m_InChat = true;
					m_Socket->PutBytes(m_Protocol->SEND_SID_JOINCHANNEL(m_FirstChannel));
				}

				break;

			case CBNETProtocol::SID_CHATEVENT:
				ChatEvent = m_Protocol->RECEIVE_SID_CHATEVENT(Packet->GetData());

				if (ChatEvent)
					ProcessChatEvent(ChatEvent);

				delete ChatEvent;
				ChatEvent = NULL;
				break;

			case CBNETProtocol::SID_CHECKAD:
				m_Protocol->RECEIVE_SID_CHECKAD(Packet->GetData());
				break;

			case CBNETProtocol::SID_STARTADVEX3:
				if (m_Protocol->RECEIVE_SID_STARTADVEX3(Packet->GetData()))
				{
					m_InChat = false;
					m_GHost->EventBNETGameRefreshed(this);
				}
				else
				{
					CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] startadvex3 failed");
					BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] startadvex3 failed");
					m_GHost->EventBNETGameRefreshFailed(this);
				}

				break;

			case CBNETProtocol::SID_PING:
				m_Socket->PutBytes(m_Protocol->SEND_SID_PING(m_Protocol->RECEIVE_SID_PING(Packet->GetData())));
				break;

			case CBNETProtocol::SID_AUTH_INFO:
				if (m_Protocol->RECEIVE_SID_AUTH_INFO(Packet->GetData()))
				{
					if (m_BNCSUtil->HELP_SID_AUTH_CHECK(m_GHost->m_Warcraft3Path, m_CDKeyROC, m_CDKeyTFT, m_Protocol->GetValueStringFormulaString(), m_Protocol->GetIX86VerFileNameString(), m_Protocol->GetClientToken(), m_Protocol->GetServerToken()))
					{
						// override the exe information generated by bncsutil if specified in the config file
						// apparently this is useful for pvpgn users

						if (m_EXEVersion.size() == 4)
						{
							CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] using custom exe version bnet_custom_exeversion = " + UTIL_ToString(m_EXEVersion[0]) + " " + UTIL_ToString(m_EXEVersion[1]) + " " + UTIL_ToString(m_EXEVersion[2]) + " " + UTIL_ToString(m_EXEVersion[3]));
							BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] using custom exe version bnet_custom_exeversion = " + UTIL_ToString(m_EXEVersion[0]) + " " + UTIL_ToString(m_EXEVersion[1]) + " " + UTIL_ToString(m_EXEVersion[2]) + " " + UTIL_ToString(m_EXEVersion[3]));
							m_BNCSUtil->SetEXEVersion(m_EXEVersion);
						}

						if (m_EXEVersionHash.size() == 4)
						{
							CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] using custom exe version hash bnet_custom_exeversionhash = " + UTIL_ToString(m_EXEVersionHash[0]) + " " + UTIL_ToString(m_EXEVersionHash[1]) + " " + UTIL_ToString(m_EXEVersionHash[2]) + " " + UTIL_ToString(m_EXEVersionHash[3]));
							BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] using custom exe version hash bnet_custom_exeversionhash = " + UTIL_ToString(m_EXEVersionHash[0]) + " " + UTIL_ToString(m_EXEVersionHash[1]) + " " + UTIL_ToString(m_EXEVersionHash[2]) + " " + UTIL_ToString(m_EXEVersionHash[3]));
							m_BNCSUtil->SetEXEVersionHash(m_EXEVersionHash);
						}

						m_Socket->PutBytes(m_Protocol->SEND_SID_AUTH_CHECK(m_Protocol->GetClientToken(), m_BNCSUtil->GetEXEVersion(), m_BNCSUtil->GetEXEVersionHash(), m_BNCSUtil->GetKeyInfoROC(), m_BNCSUtil->GetKeyInfoTFT(), m_BNCSUtil->GetEXEInfo(), "GHost"));

						// the Warden seed is the first 4 bytes of the ROC key hash
					}
					else
					{
						CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon failed - bncsutil key hash failed (check your Warcraft 3 path and cd keys), disconnecting");
						BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon failed - bncsutil key hash failed (check your Warcraft 3 path and cd keys), disconnecting");
						m_Socket->Disconnect();
						delete Packet;
						return;
					}
				}

				break;

			case CBNETProtocol::SID_AUTH_CHECK:
				if (m_Protocol->RECEIVE_SID_AUTH_CHECK(Packet->GetData()))
				{
					// cd keys accepted

					CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] cd keys accepted");
					BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] cd keys accepted");
					m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGON();
					m_Socket->PutBytes(m_Protocol->SEND_SID_AUTH_ACCOUNTLOGON(m_BNCSUtil->GetClientKey(), m_UserName));
				}
				else
				{
					// cd keys not accepted

					switch (UTIL_ByteArrayToUInt32(m_Protocol->GetKeyState(), false))
					{
					case CBNETProtocol::KR_ROC_KEY_IN_USE:
						CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon failed - ROC CD key in use by user [" + m_Protocol->GetKeyStateDescription() + "], disconnecting");
						BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon failed - ROC CD key in use by user [" + m_Protocol->GetKeyStateDescription() + "], disconnecting");
						break;
					case CBNETProtocol::KR_TFT_KEY_IN_USE:
						CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon failed - TFT CD key in use by user [" + m_Protocol->GetKeyStateDescription() + "], disconnecting");
						BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon failed - TFT CD key in use by user [" + m_Protocol->GetKeyStateDescription() + "], disconnecting");
						break;
					case CBNETProtocol::KR_OLD_GAME_VERSION:
						CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon failed - game version is too old, disconnecting");
						BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon failed - game version is too old, disconnecting");
						break;
					case CBNETProtocol::KR_INVALID_VERSION:
						CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon failed - game version is invalid, disconnecting");
						BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon failed - game version is invalid, disconnecting");
						break;
					default:
						CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon failed - cd keys not accepted, disconnecting");
						BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon failed - cd keys not accepted, disconnecting");
						break;
					}

					m_Socket->Disconnect();
					delete Packet;
					return;
				}

				break;

			case CBNETProtocol::SID_AUTH_ACCOUNTLOGON:
				if (m_Protocol->RECEIVE_SID_AUTH_ACCOUNTLOGON(Packet->GetData()))
				{
					CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] username [" + m_UserName + "] accepted");
					BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] username [" + m_UserName + "] accepted");

					if (m_PasswordHashType == "pvpgn")
					{
						// pvpgn logon

						CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] using pvpgn logon type (for pvpgn servers only)");
						BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] using pvpgn logon type (for pvpgn servers only)");
						m_BNCSUtil->HELP_PvPGNPasswordHash(m_UserPassword);
						m_Socket->PutBytes(m_Protocol->SEND_SID_AUTH_ACCOUNTLOGONPROOF(m_BNCSUtil->GetPvPGNPasswordHash()));
					}
					else
					{
						// battle.net logon

						CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] using battle.net logon type (for official battle.net servers only)");
						BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] using battle.net logon type (for official battle.net servers only)");
						m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGONPROOF(m_Protocol->GetSalt(), m_Protocol->GetServerPublicKey());
						m_Socket->PutBytes(m_Protocol->SEND_SID_AUTH_ACCOUNTLOGONPROOF(m_BNCSUtil->GetM1()));
					}
				}
				else
				{
					CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon failed - invalid username, disconnecting");
					BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon failed - invalid username, disconnecting");
					m_Socket->Disconnect();
					delete Packet;
					return;
				}

				break;

			case CBNETProtocol::SID_AUTH_ACCOUNTLOGONPROOF:
				if (m_Protocol->RECEIVE_SID_AUTH_ACCOUNTLOGONPROOF(Packet->GetData()))
				{
					// logon successful

					CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon successful");
					BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon successful");
					m_LoggedIn = true;
					m_GHost->EventBNETLoggedIn(this);
					m_Socket->PutBytes(m_Protocol->SEND_SID_ENTERCHAT());
					//m_Socket->PutBytes( m_Protocol->SEND_SID_FRIENDSLIST( ) );
					//m_Socket->PutBytes( m_Protocol->SEND_SID_CLANMEMBERLIST( ) );
				}
				else
				{
					CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon failed - invalid password, disconnecting");
					BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] logon failed - invalid password, disconnecting");

					// try to figure out if the user might be using the wrong logon type since too many people are confused by this

					string Server = m_Server;
					transform(Server.begin(), Server.end(), Server.begin(), (int(*)(int))tolower);

					if (m_PasswordHashType == "pvpgn" && (Server == "useast.battle.net" || Server == "uswest.battle.net" || Server == "asia.battle.net" || Server == "europe.battle.net"))
					{
						CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] it looks like you're trying to connect to a battle.net server using a pvpgn logon type, check your config file's \"battle.net custom data\" section");
						BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] it looks like you're trying to connect to a battle.net server using a pvpgn logon type, check your config file's \"battle.net custom data\" section");
					}
					else if (m_PasswordHashType != "pvpgn" && (Server != "useast.battle.net" && Server != "uswest.battle.net" && Server != "asia.battle.net" && Server != "europe.battle.net"))
					{
						CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] it looks like you're trying to connect to a pvpgn server using a battle.net logon type, check your config file's \"battle.net custom data\" section");
						BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] it looks like you're trying to connect to a pvpgn server using a battle.net logon type, check your config file's \"battle.net custom data\" section");
					}

					m_Socket->Disconnect();
					delete Packet;
					return;
				}

				break;

			case CBNETProtocol::SID_WARDEN:
				WardenData = m_Protocol->RECEIVE_SID_WARDEN(Packet->GetData());

				/*if( m_BNLSClient )
					m_BNLSClient->QueueWardenRaw( WardenData );
				else
					BNET_Print( "[BNET: " + m_ServerAlias + " " + m_UserName + "] warning - received warden packet but no BNLS server is available, you will be kicked from battle.net soon" );*/

				CONSOLE_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] warning - received warden packet but no BNLS server is available, you will be kicked from battle.net soon");
				BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] warning - received warden packet but no BNLS server is available, you will be kicked from battle.net soon");

				break;

				/*case CBNETProtocol :: SID_FRIENDSLIST:
					Friends = m_Protocol->RECEIVE_SID_FRIENDSLIST( Packet->GetData( ) );

					for( vector<CIncomingFriendList *> :: iterator i = m_Friends.begin( ); i != m_Friends.end( ); i++ )
						delete *i;

					m_Friends = Friends;*/

					/* DEBUG_Print( "received " + UTIL_ToString( Friends.size( ) ) + " friends" );
					for( vector<CIncomingFriendList *> :: iterator i = m_Friends.begin( ); i != m_Friends.end( ); i++ )
						DEBUG_Print( "friend: " + (*i)->GetAccount( ) ); */

						//break;

					/*case CBNETProtocol :: SID_CLANMEMBERLIST:
						vector<CIncomingClanList *> Clans = m_Protocol->RECEIVE_SID_CLANMEMBERLIST( Packet->GetData( ) );

						for( vector<CIncomingClanList *> :: iterator i = m_Clans.begin( ); i != m_Clans.end( ); i++ )
							delete *i;

						m_Clans = Clans;*/

						/* DEBUG_Print( "received " + UTIL_ToString( Clans.size( ) ) + " clan members" );
						for( vector<CIncomingClanList *> :: iterator i = m_Clans.begin( ); i != m_Clans.end( ); i++ )
							DEBUG_Print( "clan member: " + (*i)->GetName( ) ); */

							//break;
			}
		}

		delete Packet;
	}
}

void CBNET::ProcessChatEvent(CIncomingChatEvent* chatEvent)
{
	CBNETProtocol::IncomingChatEvent Event = chatEvent->GetChatEvent();
	bool Whisper = (Event == CBNETProtocol::EID_WHISPER);
	string User = chatEvent->GetUser();
	string Message = chatEvent->GetMessage();

	if (Event == CBNETProtocol::EID_WHISPER || Event == CBNETProtocol::EID_TALK)
	{
		CDBPlayer* Player = NULL;

		for (list<CPlayers*> ::iterator i = m_GHost->m_Players.begin(); i != m_GHost->m_Players.end(); ++i)
		{
			if ((*i)->GetServerID() == m_ServerID)
			{
				Player = (*i)->GetPlayer(User);
				break;
			}
		}

		// delete Player object later if it's temporary object

		bool DeletePlayer = false;

		if (!Player)
		{
			Player = new CDBPlayer(0, 0, 1);
			DeletePlayer = true;
		}

		if (IsRootAdmin(User))
			Player->SetAccessLevel(9001);

		/*if( Event == CBNETProtocol :: EID_WHISPER )
			m_GHost->EventBNETWhisper( this, User, Message );
		else
			m_GHost->EventBNETChat( this, User, Message );*/

			// log chat activity

		if (!Message.empty() && (Message[0] == '.' || Message[0] == '!'))
		{
			if (Event == CBNETProtocol::EID_WHISPER)
				BNET_Print("[WHISPER: " + m_ServerAlias + " " + m_UserName + "] " + (Player->GetIsAdmin() ? "admin" : "user") + " [" + User + " (" + UTIL_ToString(Player->GetAccessLevel()) + ")] sent command [" + Message + "]");
			else
				BNET_Print("[LOCAL: " + m_ServerAlias + " " + m_UserName + "] " + (Player->GetIsAdmin() ? "admin" : "user") + " [" + User + " (" + UTIL_ToString(Player->GetAccessLevel()) + ")] sent command [" + Message + "]");
		}
		else if (Event == CBNETProtocol::EID_WHISPER)
			BNET_Print("[WHISPER: " + m_ServerAlias + " " + m_UserName + "] " + (Player->GetIsAdmin() ? "admin" : "user") + " [" + User + " (" + UTIL_ToString(Player->GetAccessLevel()) + ")] sent message [" + Message + "]");
		else
			BNET_Print("[LOCAL: " + m_ServerAlias + " " + m_UserName + "] " + (Player->GetIsAdmin() ? "admin" : "user") + " [" + User + " (" + UTIL_ToString(Player->GetAccessLevel()) + ")] sent message [" + Message + "]");

		// handle bot commands (commands with trigger "." are also available)

		if (!Message.empty() && Message[0] == '.')
		{
			// extract the command trigger, the command, and the payload
			// e.g. "!say hello world" -> command: "say", payload: "hello world"

			string Command;
			string Payload;
			string::size_type PayloadStart = Message.find(" ");

			if (PayloadStart != string::npos)
			{
				Command = Message.substr(1, PayloadStart - 1);
				Payload = Message.substr(PayloadStart + 1);
			}
			else
				Command = Message.substr(1);

			transform(Command.begin(), Command.end(), Command.begin(), (int(*)(int))tolower);

			//
			// .DOTA<region>
			//
			if (Command == "dota" && !Payload.empty())
			{
				if (Whisper)
				{
					if (m_GHost->m_Enabled)
					{
						if (!Player->GetIsBanned() || Player->GetIsAdmin())
						{
							// create public custom DotA game

							if (Payload.size() > 31)
								//SendChatCommand( m_GHost->m_Language->UnableToCreateGameNameTooLong( Payload ), User );
								SendChatCommand("Unable to create game, the game name is too long (the maximum is 31 characters).", User);
							else
							{
							    	const std::string region = Regions::regionFromPostfix(Command);
								if (IsInChannel(User))
									m_GHost->m_Manager->QueueGame(this, MASL_PROTOCOL::DB_CUSTOM_DOTA_GAME, 16, User, Player->GetAccessLevel(), Payload, string(), false, true, MASL_PROTOCOL::GHOST_GROUP_CUSTOM_DOTA, region);
								else
									m_GHost->m_Manager->QueueGame(this, MASL_PROTOCOL::DB_CUSTOM_DOTA_GAME, 16, User, Player->GetAccessLevel(), Payload, string(), false, false, MASL_PROTOCOL::GHOST_GROUP_CUSTOM_DOTA, region);
							}
						}
						else
							SendChatCommand(m_GHost->m_Language->UserYouCantCreateGameBecauseYouAreBanned(User), User);
					}
					else
						SendChatCommand(m_GHost->m_Language->UnableToCreateGameDisabled(m_GHost->m_DisabledMessage), User);
				}
				else
					SendChatCommand("You have to whisper the bot (/w " + m_UserName + " .dota)", User);
			}

			//
			// .DOTAOBS<region>
			//
			if (Command == "dotaobs" && !Payload.empty())
			{
				if (Whisper)
				{
					if (m_GHost->m_Enabled)
					{
						if (!Player->GetIsBanned() || Player->GetIsAdmin())
						{
							// create public custom DotA game with observer slots

							if (Payload.size() > 31)
								//SendChatCommand( m_GHost->m_Language->UnableToCreateGameNameTooLong( Payload ), User );
								SendChatCommand("Unable to create game, the game name is too long (the maximum is 31 characters).", User);
							else
							{
							    	const std::string region = Regions::regionFromPostfix(Command);
								if (IsInChannel(User))
									m_GHost->m_Manager->QueueGame(this, MASL_PROTOCOL::DB_CUSTOM_DOTA_GAME, 16, User, Player->GetAccessLevel(), Payload, string(), true, true, MASL_PROTOCOL::GHOST_GROUP_CUSTOM_DOTA, region);
								else
									m_GHost->m_Manager->QueueGame(this, MASL_PROTOCOL::DB_CUSTOM_DOTA_GAME, 16, User, Player->GetAccessLevel(), Payload, string(), true, false, MASL_PROTOCOL::GHOST_GROUP_CUSTOM_DOTA, region);
							}
						}
						else
							SendChatCommand(m_GHost->m_Language->UserYouCantCreateGameBecauseYouAreBanned(User), User);
					}
					else
						SendChatCommand(m_GHost->m_Language->UnableToCreateGameDisabled(m_GHost->m_DisabledMessage), User);
				}
				else
					SendChatCommand("You have to whisper the bot (/w " + m_UserName + " .dotaobs)", User);
			}

			//
			// .TOUR<region>
			//
			if (Command == "tour" && !Payload.empty())
			{
			    	if (Whisper)
				{
					if (m_GHost->m_Enabled)
					{
						if (!Player->GetIsBanned() || Player->GetIsAdmin())
						{
							// create public custom DotA game

							if (Payload.size() > 31)
								//SendChatCommand( m_GHost->m_Language->UnableToCreateGameNameTooLong( Payload ), User );
								SendChatCommand("Unable to create game, the game name is too long (the maximum is 31 characters).", User);
							else
							{
							    	const std::string region = Regions::regionFromPostfix(Command);
								if (IsInChannel(User))
									m_GHost->m_Manager->QueueGame(this, MASL_PROTOCOL::DB_CUSTOM_DOTA_GAME, 16, User, Player->GetAccessLevel(), Payload, string(), false, true, MASL_PROTOCOL::GHOST_GROUP_TOUR, region);
								else
									m_GHost->m_Manager->QueueGame(this, MASL_PROTOCOL::DB_CUSTOM_DOTA_GAME, 16, User, Player->GetAccessLevel(), Payload, string(), false, false, MASL_PROTOCOL::GHOST_GROUP_TOUR, region);
							}
						}
						else
							SendChatCommand(m_GHost->m_Language->UserYouCantCreateGameBecauseYouAreBanned(User), User);
					}
					else
						SendChatCommand(m_GHost->m_Language->UnableToCreateGameDisabled(m_GHost->m_DisabledMessage), User);
				}
				else
					//SendChatCommand( "Hosting commands must be whispered to the bot from now on (/w playdota.eu !gopub zzz), we aim to make channel more chat friendly without !gopub spam", User );
					SendChatCommand("You have to whisper the bot (/w " + m_UserName + " .dota)", User);
			}

			//
			// .TOUROBS<region>
			//
			if (Command == "tourobs" && !Payload.empty())
			{
				if (Whisper)
				{
					if (m_GHost->m_Enabled)
					{
						if (!Player->GetIsBanned() || Player->GetIsAdmin())
						{
							// create public custom DotA game with observer slots

							if (Payload.size() > 31)
								//SendChatCommand( m_GHost->m_Language->UnableToCreateGameNameTooLong( Payload ), User );
								SendChatCommand("Unable to create game, the game name is too long (the maximum is 31 characters).", User);
							else
							{
							    	const std::string region = Regions::regionFromPostfix(Command);
								if (IsInChannel(User))
									m_GHost->m_Manager->QueueGame(this, MASL_PROTOCOL::DB_CUSTOM_DOTA_GAME, 16, User, Player->GetAccessLevel(), Payload, string(), true, true, MASL_PROTOCOL::GHOST_GROUP_TOUR, region);
								else
									m_GHost->m_Manager->QueueGame(this, MASL_PROTOCOL::DB_CUSTOM_DOTA_GAME, 16, User, Player->GetAccessLevel(), Payload, string(), true, false, MASL_PROTOCOL::GHOST_GROUP_TOUR, region);
							}
						}
						else
							SendChatCommand(m_GHost->m_Language->UserYouCantCreateGameBecauseYouAreBanned(User), User);
					}
					else
						SendChatCommand(m_GHost->m_Language->UnableToCreateGameDisabled(m_GHost->m_DisabledMessage), User);
				}
				else
					SendChatCommand("You have to whisper the bot (/w " + m_UserName + " .dotaobs)", User);
			}

			//
			// .PRIV<region>
			//
			else if (Command.starts_with("priv") && !Payload.empty())
			{
				if (Whisper)
				{
					if (m_GHost->m_Enabled)
					{
						if (!Player->GetIsBanned() || Player->GetIsAdmin())
						{
							// create private custom game
							string LoadedMap = string();
							string Name = User;
							transform(Name.begin(), Name.end(), Name.begin(), (int(*)(int))tolower);

							for (vector<CLoadedMap*> ::iterator i = m_LoadedMaps.begin(); i != m_LoadedMaps.end(); i++)
							{
								if ((*i)->GetName() == Name)
								{
									LoadedMap = (*i)->GetMap();
									break;
								}
							}

							if (LoadedMap.empty())
								SendChatCommand("Use .map <pattern> command to load a map, visit https://eurobattle.net/maps/upload to upload a new map.\"", User);
							else
							{
								if (Payload.size() > 31)
									//SendChatCommand( m_GHost->m_Language->UnableToCreateGameNameTooLong( Payload ), User );
									SendChatCommand("Unable to create game, the game name is too long (the maximum is 31 characters).", User);
								else
								{
								    	const std::string region = Regions::regionFromPostfix(Command);
									if (IsInChannel(User))
										m_GHost->m_Manager->QueueGame(this, MASL_PROTOCOL::DB_CUSTOM_GAME, 17, User, Player->GetAccessLevel(), Payload, LoadedMap, false, true, MASL_PROTOCOL::GHOST_GROUP_CUSTOM, region);
									else
										m_GHost->m_Manager->QueueGame(this, MASL_PROTOCOL::DB_CUSTOM_GAME, 17, User, Player->GetAccessLevel(), Payload, LoadedMap, false, false, MASL_PROTOCOL::GHOST_GROUP_CUSTOM, region);
								}
							}
						}
						else {
						    	SendChatCommand(m_GHost->m_Language->UserYouCantCreateGameBecauseYouAreBanned(User), User);
						}
					}
					else {
					    	SendChatCommand(m_GHost->m_Language->UnableToCreateGameDisabled(m_GHost->m_DisabledMessage), User);
					}
				}
				else {
					SendChatCommand("You have to whisper the bot (/w " + m_UserName + " .gopriv)", User);
				}
			}

			//
			// .PUB<region>
			//
			else if (Command.starts_with("pub") && !Payload.empty())
			{
				if (m_GHost->m_Enabled)
				{
					if (!Player->GetIsBanned() || Player->GetIsAdmin())
					{
						// create public custom game

						string LoadedMap = string();
						string Name = User;
						transform(Name.begin(), Name.end(), Name.begin(), (int(*)(int))tolower);

						for (vector<CLoadedMap*> ::iterator i = m_LoadedMaps.begin(); i != m_LoadedMaps.end(); i++)
						{
							if ((*i)->GetName() == Name)
							{
								LoadedMap = (*i)->GetMap();
								break;
							}
						}

						if (LoadedMap.empty())
							SendChatCommand("Use .map <pattern> command to load a map, visit https://eurobattle.net/maps/upload to upload a new map.", User);
						else
						{
							if (Payload.size() > 31)
								SendChatCommand("Unable to create game, the game name is too long (the maximum is 31 characters).", User);
							else
							{
							    	const std::string region = Regions::regionFromPostfix(Command);
								if (IsInChannel(User))
									m_GHost->m_Manager->QueueGame(this, MASL_PROTOCOL::DB_CUSTOM_GAME, 16, User, Player->GetAccessLevel(), Payload, LoadedMap, false, true, MASL_PROTOCOL::GHOST_GROUP_CUSTOM, region);
								else
									m_GHost->m_Manager->QueueGame(this, MASL_PROTOCOL::DB_CUSTOM_GAME, 16, User, Player->GetAccessLevel(), Payload, LoadedMap, false, false, MASL_PROTOCOL::GHOST_GROUP_CUSTOM, region);
							}
						}
					}
					else {
						SendChatCommand(m_GHost->m_Language->UserYouCantCreateGameBecauseYouAreBanned(User), User);
					}
				}
				else {
				    	SendChatCommand(m_GHost->m_Language->UnableToCreateGameDisabled(m_GHost->m_DisabledMessage), User);
				}
			}

			//
			// .MAP
			//
			else if (Command == "map")
			{
				if (Whisper)
				{
					if (m_GHost->m_Enabled)
					{
						if (!Player->GetIsBanned() || Player->GetIsAdmin())
						{
							if (Payload.empty())
							{
								string LoadedMap = string();
								string Name = User;
								transform(Name.begin(), Name.end(), Name.begin(), (int(*)(int))tolower);

								for (vector<CLoadedMap*> ::iterator i = m_LoadedMaps.begin(); i != m_LoadedMaps.end(); i++)
								{
									if ((*i)->GetName() == Name)
									{
										LoadedMap = (*i)->GetMap();
										break;
									}
								}

								if (LoadedMap.empty())
									// SendChatCommand( "You haven't loaded any map, use " + string( 1, m_CommandTrigger ) + "map <pattern> command, visit www.playdota.eu to add a new map via our map upload web page.", User );
									SendChatCommand("Use .map <pattern> command to load a map, visit https://eurobattle.net/maps/upload to upload a new map.", User);
								else
									SendChatCommand(m_GHost->m_Language->CurrentlyLoadedMapCFGIs(LoadedMap), User);
							}
							else
							{
								string FoundMaps;

								try
								{
									BNET_Print("[MAP] Start");
									path MapPath(m_GHost->m_MapPath);
									string Pattern = Payload;
									transform(Pattern.begin(), Pattern.end(), Pattern.begin(), (int(*)(int))tolower);
									BNET_Print("[MAP] Check path: " + m_GHost->m_MapPath);

									if (!exists(MapPath))
									{
										BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] error listing maps - map path doesn't exist");
										SendChatCommand(m_GHost->m_Language->ErrorListingMaps(), User);
									}
									else
									{
										directory_iterator EndIterator;
										path LastMatch;
										uint32_t Matches = 0;

										for (directory_iterator i(MapPath); i != EndIterator; ++i)
										{
											BNET_Print("[MAP] Iteration");
											string FileName = i->path().filename().string();
											string Stem = i->path().stem().string();
											transform(FileName.begin(), FileName.end(), FileName.begin(), (int(*)(int))tolower);
											transform(Stem.begin(), Stem.end(), Stem.begin(), (int(*)(int))tolower);
											BNET_Print("[MAP] Check : " + FileName);
											if (!is_directory(i->status()) && FileName.find(Pattern) != string::npos)
											{
												LastMatch = i->path();
												++Matches;
												BNET_Print("[MAP] Match++");

												if (FoundMaps.empty())
													FoundMaps = i->path().filename().string();
												else
													FoundMaps += ", " + i->path().filename().string();

												// if the pattern matches the filename exactly, with or without extension, stop any further matching

												if (FileName == Pattern || Stem == Pattern)
												{
													Matches = 1;
													break;
												}
											}
										}

										BNET_Print("[MAP] Check matches");

										if (Matches == 0)
											SendChatCommand("No maps found, visit https://eurobattle.net/maps/upload to upload a new map.", User);
										else if (Matches == 1)
										{
											string File = LastMatch.filename().string();
											SendChatCommand(m_GHost->m_Language->LoadingConfigFile(File), User);

											string Name = User;
											transform(Name.begin(), Name.end(), Name.begin(), (int(*)(int))tolower);

											for (vector<CLoadedMap*> ::iterator i = m_LoadedMaps.begin(); i != m_LoadedMaps.end(); i++)
											{
												if ((*i)->GetName() == Name)
												{
													delete* i;
													m_LoadedMaps.erase(i);
													break;
												}
											}

											m_LoadedMaps.push_back(new CLoadedMap(Name, File, GetTime()));

											// patch, we don't load the map here, we only search for the map name
											/*
											// hackhack: create a config file in memory with the required information to load the map

											CConfig MapCFG;
											MapCFG.Set( "map_path", "Maps\\Download\\" + File );
											MapCFG.Set( "map_localpath", File );
											m_GHost->m_Map->Load( &MapCFG, File );
											*/
										}
										else
											SendChatCommand(m_GHost->m_Language->FoundMaps(FoundMaps), User);

										BNET_Print("[MAP] End");
									}
								}
								catch (const exception& ex)
								{
									BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] error listing maps - caught exception [" + ex.what() + "]");
									SendChatCommand(m_GHost->m_Language->ErrorListingMaps(), User);
								}
							}
						}
						else
							SendChatCommand(m_GHost->m_Language->UserYouCantCreateGameBecauseYouAreBanned(User), User);
					}
					else
						SendChatCommand(m_GHost->m_Language->UnableToCreateGameDisabled(m_GHost->m_DisabledMessage), User);
				}
				else
					//SendChatCommand( "Hosting commands must be whispered to the bot from now on (/w playdota.eu !gopub zzz), we aim to make channel more chat friendly without !gopub spam", User );
					SendChatCommand("You have to whisper the bot (/w " + m_UserName + " .map)", User);
			}
		}
		else if (!Message.empty() && Message[0] == m_CommandTrigger)
		{
			// extract the command trigger, the command, and the payload
			// e.g. "!say hello world" -> command: "say", payload: "hello world"

			string Command;
			string Payload;
			string::size_type PayloadStart = Message.find(" ");

			if (PayloadStart != string::npos)
			{
				Command = Message.substr(1, PayloadStart - 1);
				Payload = Message.substr(PayloadStart + 1);
			}
			else
				Command = Message.substr(1);

			transform(Command.begin(), Command.end(), Command.begin(), (int(*)(int))tolower);

			//BNET_Print( "[BNET: " + m_ServerAlias + " " + m_UserName + "] admin [" + User + "] sent command [" + Message + "]" );
			//BNET_Print( "[BNET: " + m_ServerAlias + " " + m_UserName + "] user [" + User + " (" + UTIL_ToString( Player->GetAccessLevel( ) ) + ")] sent command [" + Message + "]" );

			bool IsAdmin = false;

			if (Player)
				IsAdmin = Player->GetIsAdmin();

			if (IsRootAdmin(User) || IsAdmin)
			{
				//
				// !ANNOUNCE
				// !ANN
				//

				if ((Command == "announce" || Command == "ann") && !Payload.empty())
				{
					// only allow tour staff to use !announce
					// 4050 	Tour Admin
					// 4001 	Tour Moderator

					if (IsRootAdmin(User) || Player->GetAccessLevel() == 4050
						|| Player->GetAccessLevel() == 4001 || Player->GetAccessLevel() > 8000)
						SendChatCommand("/ann " + Payload);
					else
						SendChatCommand("Only tour staff or lvl>8000 can use this command.");
				}

				//
				// !ADDADMIN
				//

				//
				// !ADDBAN
				// !BAN
				//

				//
				// !ADDBANFOR
				// !BANFOR
				//

				//
				// !ADDWARN
				// !WARN
				//

				//
				// !BAN
				// !CBAN
				//

				else if ((Command == "ban" || Command == "cban") && !Payload.empty())
				{
					string Victim;
					string Reason;

					stringstream SS;
					SS << Payload;
					SS >> Victim;

					if (!SS.eof())
					{
						getline(SS, Reason);
						SendChatCommand("/ban " + Victim + " " + User + ": " + Reason + ".");
					}
					else
						SendChatCommand("/ban " + Victim + " You were banned by " + User + ".");
				}

				//
				// !CHANNEL (change channel)
				//

				else if (Command == "channel" && !Payload.empty())
				{
					if (IsRootAdmin(User))
						SendChatCommand("/join " + Payload);
					else
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
				}

				//
				// !CHECKADMIN
				//

				//
				// !CHECKCONTRIBUTOR
				//

				//
				// !CHECKWARN
				// !CW
				//

				//
				// !CMD
				//

				else if (Command == "cmd" && !Payload.empty())
				{
					if (IsRootAdmin(User))
						m_GHost->m_Manager->SendAll(MASL_PROTOCOL::MTS_REMOTE_CMD, UTIL_ToString(m_ServerID) + " " + User + " " + Payload);
					else
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
				}

				//
				// !COUNTADMINS
				//

				//
				// !COUNTBANS
				//

				//
				// !COUNTCONTRIBUTORS
				//

				//
				// !COUNTWARN
				// !COUNTWARNS
				//

				//
				// !CUNBAN
				//

				else if ((Command == "unban" || Command == "cunban") && !Payload.empty())
				{
					SendChatCommand("/unban " + Payload);
					SendChatCommand(Payload + " should now be unbanned from the channel.", User);
				}

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
				// !DELWARN
				//

				//
				// !DISABLE
				//

				else if (Command == "disable")
				{
					if (IsRootAdmin(User))
					{
						SendChatCommand(m_GHost->m_Language->BotDisabled(), User);
						m_GHost->m_Enabled = false;

						if (!Payload.empty())
							m_GHost->m_DisabledMessage = Payload;

						// delete all games from the queue

						m_GHost->m_Manager->DeleteQueue();
					}
					else {
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
					}
				}

				else if (Command == "disableladder")
				{
					if (IsRootAdmin(User))
					{
						SendChatCommand(m_GHost->m_Language->BotLadderDisabled(), User);
						m_GHost->m_EnabledLadder = false;
					}
					else {
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
					}
				}

				// !COM

				else if (Command == "com")
				{
					if (IsRootAdmin(User))
					{
						if (m_GHost->m_ContributorOnlyMode)
						{
							m_GHost->m_ContributorOnlyMode = false;
							SendChatCommand("Contributor only mode is now OFF", User);
						}
						else
						{
							m_GHost->m_ContributorOnlyMode = true;
							SendChatCommand("Contributor only mode is now ON", User);
						}

						for (list<CSlave*> ::iterator i = m_GHost->m_Manager->m_Slaves.begin(); i != m_GHost->m_Manager->m_Slaves.end(); ++i)
							(*i)->SendContributorOnlyMode(m_GHost->m_ContributorOnlyMode);
					}
					else
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
				}

				// !COMSTATUS

				else if (Command == "comstatus")
				{
					if (IsRootAdmin(User))
					{
						if (m_GHost->m_ContributorOnlyMode)
							SendChatCommand("Contributor only mode is ON", User);
						else
							SendChatCommand("Contributor only mode is OFF", User);
					}
					else
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
				}

				//
				// !DEVOICE
				//

				else if (Command == "devoice" && !Payload.empty())
				{
					if (IsRootAdmin(User) || Player->GetAccessLevel() > 8000)
						SendChatCommand("/devoice " + Payload);
					else
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
				}

				//
				// !DEVOICEME
				//

				else if (Command == "devoiceme")
					SendChatCommand("/devoice " + User);

				//
				// !ENABLE
				//

				else if (Command == "enable")
				{
					if (IsRootAdmin(User))
					{
						SendChatCommand(m_GHost->m_Language->BotEnabled(), User);
						m_GHost->m_Enabled = true;
						m_GHost->m_DisabledMessage.clear();
					}
					else
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
				}

				else if (Command == "enableladder")
				{
					if (IsRootAdmin(User))
					{
						SendChatCommand(m_GHost->m_Language->BotLadderEnabled(), User);
						m_GHost->m_EnabledLadder = true;
					}
					else {
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
					}
				}

				//
				// !EXIT
				// !QUIT
				//

				else if (Command == "exit" || Command == "quit")
				{
					if (IsRootAdmin(User))
					{
						if (Payload == "nice")
							m_GHost->m_ExitingNice = true;
						else if (Payload == "force")
							m_Exiting = true;
						else
							m_Exiting = true;
					}
					else
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
				}

				//
				// !KICK
				// !CKICK
				//

				else if ((Command == "kick" || Command == "ckick") && !Payload.empty())
				{
					string Victim;
					string Reason;

					stringstream SS;
					SS << Payload;
					SS >> Victim;

					if (!SS.eof())
					{
						getline(SS, Reason);
						SendChatCommand("/kick " + Victim + " " + User + ": " + Reason + ".");
					}
					else
						SendChatCommand("/kick " + Victim + " You were kicked by " + User + ".");
				}

				//
				// !MAXGAMES
				//

				else if (Command == "maxgames")
				{
					if (IsRootAdmin(User))
					{
						if (Payload.empty())
							SendChatCommand("Max games is set to " + UTIL_ToString(m_GHost->m_MaxGames), User);
						else
						{
							m_GHost->m_MaxGames = UTIL_ToUInt32(Payload);
							SendChatCommand("Setting max games to " + Payload, User);
						}
					}
					else
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
				}

				//
				// !MODERATE
				//

				else if (Command == "moderate")
					SendChatCommand("/moderate");

				//
				// !PUSH
				//

				//
				// !SAY
				//

				else if (Command == "say" && !Payload.empty())
				{
					if (IsRootAdmin(User))
						QueueChatCommand(Payload);
					else
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
				}

				//
				// !UNHOST
				//

				//
				// !TOPIC
				//

				else if (Command == "topic" && !Payload.empty())
					SendChatCommand("/topic " + m_CurrentChannel + " \"" + Payload + "\"");

				//
				// !VOICE (temporary voice, expires when user reconnects)
				//

				else if (Command == "voice" && !Payload.empty())
				{
					if (IsRootAdmin(User))
						SendChatCommand("/voice " + Payload);
					else
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
				}

				//
				// !VOP (permanent voice)
				//

				else if (Command == "vop" && !Payload.empty())
				{
					if (IsRootAdmin(User) || Player->GetAccessLevel() > 8000)
						SendChatCommand("/vop " + Payload);
					else
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
				}

				//
				// !VOICEME
				//

				else if (Command == "voiceme")
					SendChatCommand("/voice " + User);

				//
				// !VOPME
				//

				else if (Command == "vopme")
					SendChatCommand("/vop " + User);

				//
				// !WARDENSTATUS
				//

				//
				// !SLAVES
				//

				else if (Command == "slaves")
				{
					if (IsRootAdmin(User))
					{
						vector<string> slaveinfo = m_GHost->m_Manager->PrintSlaveInfo();
						for (vector<string>::iterator i = slaveinfo.begin(); i != slaveinfo.end(); ++i)
						{
							SendChatCommand(*i, User);
						}
					}
					else
					{
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
					}
				}

				//
				// !SLAVES
				//

				else if (Command == "slavesl")
				{
					if (IsRootAdmin(User))
					{
						vector<string> slaveinfo = m_GHost->m_Manager->PrintSlaveInfoLesser();
						for (vector<string>::iterator i = slaveinfo.begin(); i != slaveinfo.end(); ++i)
						{
							SendChatCommand(*i, User);
						}
					}
					else
					{
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
					}
				}

				//
				// !RSC
				//

				else if (Command == "rsc")
				{
					if (IsRootAdmin(User))
					{
						m_GHost->m_Manager->ReloadSlaveConfig();
						SendChatCommand("Config reload sent to all slaves", User);
					}
					else
					{
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
					}
				}

				//
				// !DS
				//

				else if (Command == "ds")
				{
					if (IsRootAdmin(User))
					{
						if (Payload.empty())
						{
							m_GHost->m_Manager->SoftDisableAllSlaves();
							SendChatCommand("Softly disabling all slaves", User);
						}
						else
						{
							stringstream SS;
							uint32_t slaveID = 0;
							SS << Payload;
							SS >> slaveID;

							if (SS.fail())
							{
								SendChatCommand("Failed to disable slave with unknown ID", User);
							}
							else
							{
								bool result = m_GHost->m_Manager->SoftDisableSlave(slaveID);
								if (result)
								{
									SendChatCommand("Successfully disabled slave with ID " + UTIL_ToString(slaveID), User);
								}
								else
								{
									SendChatCommand("Failed to disable slave with ID " + UTIL_ToString(slaveID), User);
								}
							}
						}
					}
					else
					{
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
					}
				}

				//
				// !ES
				//

				else if (Command == "es")
				{
					if (IsRootAdmin(User))
					{
						if (Payload.empty())
						{
							m_GHost->m_Manager->SoftEnableAllSlaves();
							SendChatCommand("Softly enabling all slaves", User);
						}
						else
						{
							stringstream SS;
							uint32_t slaveID = 0;
							SS << Payload;
							SS >> slaveID;

							if (SS.fail())
							{
								SendChatCommand("Failed to enable slave with unknown ID", User);
							}
							else
							{
								bool result = m_GHost->m_Manager->SoftEnableSlave(slaveID);
								if (result)
								{
									SendChatCommand("Successfully enabled slave with ID " + UTIL_ToString(slaveID), User);
								}
								else
								{
									SendChatCommand("Failed to enable slave with ID " + UTIL_ToString(slaveID), User);
								}
							}
						}
					}
					else
					{
						SendChatCommand(m_GHost->m_Language->YouDontHaveAccessToThatCommand(), User);
					}
				}
			}

			//
			// !CHECKALL
			//

			if (Command == "_checkall")
			{
				if (Payload.empty())
					Payload = User;

				CONSOLE_Print("m_BanGroup.size( ) = " + UTIL_ToString(m_GHost->m_BanGroup.size()));
				CONSOLE_Print("m_BanGroup.capacity( ) = " + UTIL_ToString(m_GHost->m_BanGroup.capacity()));
				CONSOLE_Print("m_DotAPlayerSummary.size( ) = " + UTIL_ToString(m_GHost->m_DotAPlayerSummary.size()));
				CONSOLE_Print("m_DotAPlayerSummary.capacity( ) = " + UTIL_ToString(m_GHost->m_DotAPlayerSummary.capacity()));
				CONSOLE_Print("m_Players.size( ) = " + UTIL_ToString(m_GHost->m_Players.size()));
			}

			//
			// !CHECKBAN
			//

			else if (Command == "checkban")
			{
				string Name = Payload.empty() ? User : Payload;
				string NameLower = Name;
				transform(NameLower.begin(), NameLower.end(), NameLower.begin(), (int(*)(int))tolower);
				CDBPlayer* CheckbanPlayer = NULL;

				if (Payload.empty())
					CheckbanPlayer = Player;
				else
				{
					for (list<CPlayers*> ::iterator i = m_GHost->m_Players.begin(); i != m_GHost->m_Players.end(); ++i)
					{
						if ((*i)->GetServerID() == m_ServerID)
						{
							CheckbanPlayer = (*i)->GetPlayer(NameLower);
							break;
						}
					}
				}

				if (CheckbanPlayer)
				{
					CDBBanGroup* BanGroup = CheckbanPlayer->GetBanGroup();

					if (BanGroup && BanGroup->GetIsBanned())
					{
						/*string RemainingTime = Player->GetBan( )->GetReadableTimeRemaining( GetMySQLTime( ) );

						if( RemainingTime.empty( ) )
							SendChatCommand( m_GHost->m_Language->UserWasBannedOnByBecause( m_Server, Payload, Player->GetBan( )->GetReadableDate( ), Player->GetBan( )->GetAdmin( ), Player->GetBan( )->GetReason( ) ), User );
						else
							SendChatCommand( m_GHost->m_Language->UserWasBannedOnByBecauseExpiringIn( Payload, Player->GetBan( )->GetReadableDate( ), Player->GetBan( )->GetAdmin( ), Player->GetBan( )->GetReason( ), RemainingTime ), User );*/

						string CheckbanStr;

						// get human readable date

						time_t DateTime = (time_t)BanGroup->GetDatetime();
						tm* Date = localtime(&DateTime);
						CheckbanStr += "[" + Name + "] was banned on " + UTIL_ToString(Date->tm_mday) + "-" + UTIL_ToString(Date->tm_mon + 1) + "-" + UTIL_ToString(Date->tm_year + 1900) + ".";

						// print when is the ban expiring if it's not perma ban

						if (BanGroup->GetDuration())
						{
							CheckbanStr += " Ban expiring in";

							if (GetMySQLTime() >= (BanGroup->GetDatetime() + BanGroup->GetDuration()))
								CheckbanStr += " less then a minute";

							uint32_t RemainingTime;
							RemainingTime = (BanGroup->GetDatetime() + BanGroup->GetDuration()) - GetMySQLTime();

							uint32_t Months = RemainingTime / 2629743;
							RemainingTime = RemainingTime % 2629743;

							uint32_t Days = RemainingTime / 86400;
							RemainingTime = RemainingTime % 86400;

							uint32_t Hours = RemainingTime / 3600;
							RemainingTime = RemainingTime % 3600;

							uint32_t Minutes = RemainingTime / 60;
							RemainingTime = RemainingTime % 60;

							uint32_t Seconds = RemainingTime;

							if (Months)
							{
								CheckbanStr += " " + UTIL_ToString(Months) + " months";
								if (Days)
									CheckbanStr += " " + UTIL_ToString(Days) + " days";
							}
							else
							{
								if (Days)
								{
									CheckbanStr += " " + UTIL_ToString(Days) + " days";
									if (Hours)
										CheckbanStr += " " + UTIL_ToString(Hours) + " hours";
								}
								else
								{
									if (Hours)
									{
										CheckbanStr += " " + UTIL_ToString(Hours) + " hours";
										if (Minutes)
											CheckbanStr += " " + UTIL_ToString(Minutes) + " minutes";
									}
									else
									{
										if (Minutes)
											CheckbanStr += " " + UTIL_ToString(Minutes) + " minutes";
										else
											CheckbanStr += " less then a minute";
									}
								}
							}

							CheckbanStr += ".";
						}

						CheckbanStr += " More info online";

						SendChatCommand(CheckbanStr, User);
						SendChatCommand("http://lagabuse.com/stats/player/" + UTIL_ToString(m_ServerID) + "/" + NameLower, User);
					}
					else
						SendChatCommand(m_GHost->m_Language->UserIsNotBanned(m_Server, Name), User);
				}
				else
					SendChatCommand(m_GHost->m_Language->UserIsNotBanned(m_Server, Name), User);
			}

			//
			// !GAME
			//

			else if (Command == "game" && !Payload.empty())
				SendChatCommand(m_GHost->m_Manager->GetGameDescription(m_Server, Payload), User);

			//
			// !GAMESINFO
			//

			else if (Command == "gamesinfo")
			{
				if (m_GHost->m_Manager->m_RemoteGames.size() > 0)
				{
					unsigned int j = 0;
					for (list<CRemoteGame *> :: iterator i = m_GHost->m_Manager->m_RemoteGames.begin( ); i != m_GHost->m_Manager->m_RemoteGames.end( ); ++i )
					{
						SendChatCommand(UTIL_ToString(++j) + ". " + (*i)->GetDescription( ), User);
					}
				}
				else
				{
					SendChatCommand("There are no running games at the moment.", User);
				}
			}

			//
			// !GAMES
			//

			else if (Command == "games")
				SendChatCommand(m_GHost->m_Manager->GetGamesDescription(m_ServerID), User);

			//
			// !GNAMES
			//

			else if (Command == "gnames")
			{
				if (!Payload.empty())
				{
					SendChatCommand(m_GHost->m_Manager->GetGamePlayerNames(Payload), User);
				}
				else
				{
					if (m_GHost->m_Manager->m_RemoteGames.size() > 0)
					{
						for (list<CRemoteGame *> :: iterator i = m_GHost->m_Manager->m_RemoteGames.begin(); i != m_GHost->m_Manager->m_RemoteGames.end(); ++i)
						{
							SendChatCommand((*i)->GetNames(), User);
						}
					}
					else
					{
						SendChatCommand("There are no running games at the moment.", User);
					}
				}
			}

			//
			// !PRIVL<region>
			// !PRIV<region>
			//
			else if ((Command.starts_with("priv") || Command.starts_with("privl")) && !Payload.empty())
			{
				const bool legacyMap = Command.starts_with("privl");

				if (Whisper)
				{
					if (m_GHost->m_Enabled && m_GHost->m_EnabledLadder)
					{
						if (!Player->GetIsBanned() || Player->GetIsAdmin())
						{
							/*uint32_t AccessLevel = 100;

							if( IsRootAdmin( User ) )
								AccessLevel = 9001;
							else if( IsAdmin( User ) )
								AccessLevel = 8000;
							else if( IsContributor( User ) )
								AccessLevel = 1000;*/

								// create DotA game
							if (Payload.size() > 31)
								//SendChatCommand( m_GHost->m_Language->UnableToCreateGameNameTooLong( Payload ), User );
								SendChatCommand("Unable to create game, the game name is too long (the maximum is 31 characters).", User);
							else
							{
								if (legacyMap && !UTIL_IsTDay()) {
									SendChatCommand("DotA v6 is being phased out. It can only be played on Sundays.", User);
								}
								else {
									const bool isInChannel = IsInChannel(User);
									const std::string region = Regions::regionFromPostfix(Command);
									const std::string mapName = legacyMap ? "l" : string();
									m_GHost->m_Manager->QueueGame(this, MASL_PROTOCOL::DB_DIV1_DOTA_GAME, 17, User, Player->GetAccessLevel(), Payload, mapName, false, isInChannel, MASL_PROTOCOL::GHOST_GROUP_DIV1DOTA, region);
								}
							}
						}
						else
							SendChatCommand(m_GHost->m_Language->UserYouCantCreateGameBecauseYouAreBanned(User), User);
					}
					else
						SendChatCommand(m_GHost->m_Language->UnableToCreateGameDisabled(m_GHost->m_DisabledMessage), User);
				}
				else
					SendChatCommand("You have to whisper the bot (/w " + m_UserName + " !gopriv)", User);
			}

			//
			// !PRIVOBS<region>
			//
			else if (Command.starts_with("privobs") && !Payload.empty())
			{
				if (Whisper)
				{
					if (m_GHost->m_Enabled && m_GHost->m_EnabledLadder)
					{
						if (!Player->GetIsBanned() || Player->GetIsAdmin())
						{
							/*uint32_t AccessLevel = 100;

							if( IsRootAdmin( User ) )
								AccessLevel = 9001;
							else if( IsAdmin( User ) )
								AccessLevel = 8000;
							else if( IsContributor( User ) )
								AccessLevel = 1000;*/

							// create DotA game
							if (Payload.size() > 31)
								SendChatCommand("Unable to create game, the game name is too long (the maximum is 31 characters).", User);
							else
							{
							    	const std::string region = Regions::regionFromPostfix(Command);
								if (IsInChannel(User))
									m_GHost->m_Manager->QueueGame(this, MASL_PROTOCOL::DB_DIV1_DOTA_GAME, 17, User, Player->GetAccessLevel(), Payload, string(), true, true, MASL_PROTOCOL::GHOST_GROUP_DIV1DOTA, region);
								else
									m_GHost->m_Manager->QueueGame(this, MASL_PROTOCOL::DB_DIV1_DOTA_GAME, 17, User, Player->GetAccessLevel(), Payload, string(), true, false, MASL_PROTOCOL::GHOST_GROUP_DIV1DOTA, region);
							}
						}
						else
							SendChatCommand(m_GHost->m_Language->UserYouCantCreateGameBecauseYouAreBanned(User), User);
					}
					else
						SendChatCommand(m_GHost->m_Language->UnableToCreateGameDisabled(m_GHost->m_DisabledMessage), User);
				}
				else
					SendChatCommand("You have to whisper the bot (/w " + m_UserName + " !goprivobs)", User);
			}

			//
			// !PUBL<region>
			// !PUB<region>
			//
			else if ((Command.starts_with("pub") || Command.starts_with("publ")) && !Payload.empty())
			{
				const bool legacyMap = Command.starts_with("publ");

				if (m_GHost->m_Enabled && m_GHost->m_EnabledLadder)
				{
					if (!Player->GetIsBanned() || Player->GetIsAdmin())
					{
						/*uint32_t AccessLevel = 100;

						if( IsRootAdmin( User ) )
							AccessLevel = 9001;
						else if( IsAdmin( User ) )
							AccessLevel = 8000;
						else if( IsContributor( User ) )
							AccessLevel = 1000;*/

							// create DotA game
						if (Payload.size() > 31) {
							SendChatCommand("Unable to create game, the game name is too long (the maximum is 31 characters).", User);
						}
						else {
							if (legacyMap && !UTIL_IsTDay()) {
								SendChatCommand("DotA v6 is being phased out. It can only be played on Sundays.", User);
							}
							else {
							    	const std::string region = Regions::regionFromPostfix(Command);
								const bool isInChannel = IsInChannel(User);
								const std::string mapName = legacyMap ? "l" : string();
								m_GHost->m_Manager->QueueGame(this, MASL_PROTOCOL::DB_DIV1_DOTA_GAME, 16, User, Player->GetAccessLevel(), Payload, mapName, false, isInChannel, MASL_PROTOCOL::GHOST_GROUP_DIV1DOTA, region);
							}
						}
					}
					else {
					    	SendChatCommand(m_GHost->m_Language->UserYouCantCreateGameBecauseYouAreBanned(User), User);
					}
				}
				else {
					SendChatCommand(m_GHost->m_Language->UnableToCreateGameDisabled(m_GHost->m_DisabledMessage), User);
				}
			}

			//
			// !INFO
			//

			if (Command == "info")
			{
				string CheckPlayerName = User;
				CDBPlayer* CheckPlayer = Player;

				if (!Payload.empty())
				{
					CheckPlayerName = Payload;
					CheckPlayer = NULL;

					for (list<CPlayers*> ::iterator i = m_GHost->m_Players.begin(); i != m_GHost->m_Players.end(); ++i)
					{
						if ((*i)->GetServerID() == m_ServerID)
						{
							CheckPlayer = (*i)->GetPlayer(Payload);
							break;
						}
					}
				}

				if (CheckPlayer)
					SendChatCommand("[" + CheckPlayerName + "] Access Level: " + UTIL_ToString(CheckPlayer->GetAccessLevel()) + ", Admin: " + (CheckPlayer->GetIsAdmin() ? "Yes" : "No") + ", Banned: " + (CheckPlayer->GetIsBanned() ? "Yes" : "No") + ", From: " + CheckPlayer->GetFrom() + "(" + CheckPlayer->GetLongFrom() + ").", User);
				else
					SendChatCommand("Player [" + CheckPlayerName + "] not found in the database.", User);
			}

			//
			// !LISTWARN
			// !LW
			//

			/*else if( Command == "listwarn" || Command == "lw" )
			{
				if( Payload.empty( ) )
					Payload = User;

				m_PairedListWarningChecks.push_back( PairedListWarningCheck( User, m_GHost->m_DB->ThreadedListWarningCheck( m_Server, Payload ) ) );
			}*/

			//
			// !LOAD
			//

			/*
			if( Command == "load" && !Payload.empty( ) )
				SendChatCommand( m_GHost->m_Manager->GetLobbyDescription( m_Server, Payload ), User );
			*/

			//
			// !LOBBY
			//

			else if (Command == "lobby" && !Payload.empty())
				SendChatCommand(m_GHost->m_Manager->GetLobbyDescription(m_Server, Payload), User);

			//
			// !NAMES
			//

			else if (Command == "names" && !Payload.empty())
				SendChatCommand(m_GHost->m_Manager->GetLobbyPlayerNames(Payload), User);

			//
			// !LOBBIES
			//

			else if (Command == "lobbies")
			{
				if (m_GHost->m_Manager->m_RemoteGamesInLobby.size() > 0)
				{
					for (int i = 1; i <= m_GHost->m_Manager->m_RemoteGamesInLobby.size(); i++)
					{
						SendChatCommand(m_GHost->m_Manager->GetLobbyPlayerNames(UTIL_ToString(i)), User);
					}
				}
				else {
					SendChatCommand("There are no open lobbies at the moment.", User);
				}
			}

			//
			// !QUEUE
			//

			else if (Command == "queue" || Command == "q")
			{
				if (Whisper)
				{
					uint32_t GameCount = m_GHost->m_Manager->QueueGameCount();

					if (GameCount)
					{
						if (Payload.empty())
						{
							SendChatCommand("There are " + UTIL_ToString(GameCount) + " users in the queue:", User);

							// list all players in the queue

							uint32_t i = 1;
							CQueuedGame* Game = m_GHost->m_Manager->GetQueuedGame(i);
							string Queue;

							while (Game != NULL)
							{
								Queue += UTIL_ToString(i) + ". " + Game->GetCreatorName() + " ";

								if (Queue.size() > 150)
								{
									SendChatCommand(Queue, User);
									Queue.clear();
								}

								i++;
								Game = m_GHost->m_Manager->GetQueuedGame(i);
							}

							if (!Queue.empty())
								SendChatCommand(Queue, User);
						}
						else
						{
							CQueuedGame* QueuedGame = m_GHost->m_Manager->GetQueuedGame(UTIL_ToUInt32(Payload));

							if (QueuedGame)
								SendChatCommand("Game in queue number " + Payload + " is [" + QueuedGame->GetGameName() + " : " + QueuedGame->GetCreatorName() + " : " + UTIL_ToString((GetTime() - QueuedGame->GetQueuedTime()) / 60) + "m" + (QueuedGame->GetMap().empty() ? "" : " : " + QueuedGame->GetMap()) + "].", User);
							else
								SendChatCommand("There is no game number " + Payload + " in the queue.", User);
						}
					}
					else
						SendChatCommand("There are no games in the queue.", User);
				}
				else
					SendChatCommand("You have to whisper the bot (/w " + m_UserName + " !queue)", User);
			}

			//
			// !STATSDOTA
			// !SD
			//

			else if (Command == "statsdota" || Command == "sd")
			{
				CDBDotAPlayerSummary* DotAPlayerSummary = NULL;
				string TargetUser;

				if (Payload.empty())
				{
					if (Player)
						DotAPlayerSummary = Player->GetDotAPlayerSummary();

					TargetUser = User;
				}
				else
				{
					CDBPlayer* TargetPlayer = NULL;

					for (list<CPlayers*> ::iterator i = m_GHost->m_Players.begin(); i != m_GHost->m_Players.end(); ++i)
					{
						if ((*i)->GetServerID() == m_ServerID)
						{
							CDBPlayer* TargetPlayer = (*i)->GetPlayer(Payload);

							if (TargetPlayer)
								DotAPlayerSummary = TargetPlayer->GetDotAPlayerSummary();

							break;
						}
					}

					TargetUser = Payload;
				}

				if (DotAPlayerSummary)
				{
					SendChatCommand("[" + TargetUser + "] PSR: " + UTIL_ToString(DotAPlayerSummary->GetRating(), 0) + " games: " + UTIL_ToString(DotAPlayerSummary->GetTotalGames()) + " W/L: " + UTIL_ToString(DotAPlayerSummary->GetTotalWins()) + "/" + UTIL_ToString(DotAPlayerSummary->GetTotalLosses()), User);
					SendChatCommand("H. K/D/A " + UTIL_ToString(DotAPlayerSummary->GetAvgKills(), 2) + "/" + UTIL_ToString(DotAPlayerSummary->GetAvgDeaths(), 2) + "/" + UTIL_ToString(DotAPlayerSummary->GetAvgAssists(), 2) + " C. K/D/N " + UTIL_ToString(DotAPlayerSummary->GetAvgCreepKills(), 2) + "/" + UTIL_ToString(DotAPlayerSummary->GetAvgCreepDenies(), 2) + "/" + UTIL_ToString(DotAPlayerSummary->GetAvgNeutralKills(), 2), User);
				}
				else
					SendChatCommand(m_GHost->m_Language->HasntPlayedDotAGamesWithThisBot(TargetUser), User);
			}

			//
			// !HIGHSCORE
			// !HS
			//

			else if (Command == "highscore" || Command == "hs")
			{
				CDBDotAPlayerSummary* DotAPlayerSummary = NULL;
				string TargetUser;

				if (Payload.empty())
				{
					if (Player)
						DotAPlayerSummary = Player->GetDotAPlayerSummary();

					TargetUser = User;
				}
				else
				{
					CDBPlayer* TargetPlayer = NULL;

					for (list<CPlayers*> ::iterator i = m_GHost->m_Players.begin(); i != m_GHost->m_Players.end(); ++i)
					{
						if ((*i)->GetServerID() == m_ServerID)
						{
							CDBPlayer* TargetPlayer = (*i)->GetPlayer(Payload);

							if (TargetPlayer)
								DotAPlayerSummary = TargetPlayer->GetDotAPlayerSummary();

							break;
						}
					}

					TargetUser = Payload;
				}

				if (DotAPlayerSummary)
				{
					SendChatCommand("[" + TargetUser + "] highest PSR: " + UTIL_ToString(DotAPlayerSummary->GetHighestRating(), 0), User);
				}
				else
					SendChatCommand(m_GHost->m_Language->HasntPlayedDotAGamesWithThisBot(TargetUser), User);
			}

			//
			// !UNQUEUE
			//

			else if (Command == "unqueue")
			{
				string Target = User;

				if (!Payload.empty() && (IsAdmin || IsRootAdmin(User)))
					Target = Payload;

				if (m_GHost->m_Manager->HasQueuedGame(this, Target))
				{
					SendChatCommand("Deleted game from queue for user [" + Target + "].", User);
					m_GHost->m_Manager->UnqueueGame(this, Target);
				}
			}

			//
			// !USERS
			//

			else if (Command == "users")
				SendChatCommand(m_GHost->m_Manager->GetUsersDescription(this), User);

			//
			// !VERSION
			//

			else if (Command == "version")
				SendChatCommand(m_GHost->m_Language->Version(m_GHost->m_Version), User);

			else if (Command == "regions") {
			    SendChatCommand(Regions::toString(), User);
			}

			//
			// !WHERE
			//

			else if (Command == "where" && !Payload.empty())
				m_GHost->m_Manager->SendPlayerWhereDescription(this, User, Payload);
		}

		// delete Player if it's a fake temporary object

		if (DeletePlayer)
			delete Player;
	}
	else if (Event == CBNETProtocol::EID_CHANNEL)
	{
		// keep track of current channel so we can rejoin it after hosting a game

		BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] joined channel [" + Message + "]");
		m_CurrentChannel = Message;

		// clean the old player list from the previous channel when the bot joins a new channel or
		// when the bot rejoins the server and same channel (after DC, server restart, ...)

		m_PlayersInChannel.clear();
	}
	else if (Event == CBNETProtocol::EID_INFO)
	{
		BNET_Print("[INFO: " + m_ServerAlias + " " + m_UserName + "] " + Message);

		// extract the first word which we hope is the username
		// this is not necessarily true though since info messages also include channel MOTD's and such

		string UserName;
		string::size_type Split = Message.find(" ");

		if (Split != string::npos)
			UserName = Message.substr(0, Split);
		else
			UserName = Message.substr(0);
	}
	else if (Event == CBNETProtocol::EID_ERROR)
		BNET_Print("[ERROR: " + m_ServerAlias + " " + m_UserName + "] " + Message);
	else if (Event == CBNETProtocol::EID_EMOTE)
	{
		BNET_Print("[EMOTE: " + m_ServerAlias + " " + m_UserName + "] [" + User + "] " + Message);
		m_GHost->EventBNETEmote(this, User, Message);
	}
	else if (Event == CBNETProtocol::EID_SHOWUSER)
	{
		// we joined a new channel and this player was already in it

		string Name = User;
		transform(Name.begin(), Name.end(), Name.begin(), (int(*)(int))tolower);

		// add player to the channel players list

		m_PlayersInChannel.push_back(Name);
	}
	else if (Event == CBNETProtocol::EID_JOIN)
	{
		// player joined our channel

		string Name = User;
		transform(Name.begin(), Name.end(), Name.begin(), (int(*)(int))tolower);

		// add player to the channel players list

		m_PlayersInChannel.push_back(Name);
	}
	else if (Event == CBNETProtocol::EID_LEAVE)
	{
		// player left our channel

		string Name = User;
		transform(Name.begin(), Name.end(), Name.begin(), (int(*)(int))tolower);

		// unqueue the game when player leaves the channel

		if (m_GHost->m_Manager->HasQueuedGame(this, Name))
		{
			SendChatCommand("Your game was unqueued since you left the channel.", Name);
			m_GHost->m_Manager->UnqueueGame(this, Name);
		}

		// remove player from the channel players list

		for (list<string> ::iterator i = m_PlayersInChannel.begin(); i != m_PlayersInChannel.end(); )
		{
			if ((*i) == Name)
				i = m_PlayersInChannel.erase(i);
			else
				++i;
		}
	}
}

void CBNET::SendJoinChannel(string channel)
{
	if (m_LoggedIn && m_InChat)
		m_Socket->PutBytes(m_Protocol->SEND_SID_JOINCHANNEL(channel));
}

void CBNET::QueueEnterChat()
{
	if (m_LoggedIn)
		m_OutPackets.push(m_Protocol->SEND_SID_ENTERCHAT());
}

void CBNET::QueueChatCommand(string chatCommand)
{
	if (chatCommand.empty())
		return;

	if (m_LoggedIn)
	{
		if (m_PasswordHashType == "pvpgn" && chatCommand.size() > m_MaxMessageLength)
			chatCommand = chatCommand.substr(0, m_MaxMessageLength);

		if (chatCommand.size() > 255)
			chatCommand = chatCommand.substr(0, 255);

		if (m_OutPackets.size() > 10)
			BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] attempted to queue chat command [" + chatCommand + "] but there are too many (" + UTIL_ToString(m_OutPackets.size()) + ") packets queued, discarding");
		else
		{
			BNET_Print("[QUEUED: " + m_ServerAlias + " " + m_UserName + "] " + chatCommand);
			m_OutPackets.push(m_Protocol->SEND_SID_CHATCOMMAND(chatCommand));
		}
	}
}

void CBNET::QueueChatCommand(string chatCommand, string user, bool whisper)
{
	if (chatCommand.empty())
		return;

	// if whisper is true send the chat command as a whisper to user, otherwise just queue the chat command
	if (whisper)
		QueueChatCommand("/w " + user + " " + chatCommand);
	else
		QueueChatCommand(chatCommand);
}

void CBNET::SendChatCommand(string chatCommand)
{
	// skip the queue and send instant chat command
	if (chatCommand.empty())
		return;

	if (m_PasswordHashType != "pvpgn")
	{
		BNET_Print("[BNET: " + m_ServerAlias + " " + m_UserName + "] attempted to send chat command [" + chatCommand + "] but server is not PVPGN, discarding");
		return;
	}

	if (m_LoggedIn)
	{
		if (chatCommand.size() > 255)
			chatCommand = chatCommand.substr(0, 255);

		BNET_Print("[SENT: " + m_ServerAlias + " " + m_UserName + "] " + chatCommand);

		m_Socket->PutBytes(m_Protocol->SEND_SID_CHATCOMMAND(chatCommand));
	}
}

void CBNET::SendChatCommand(string chatCommand, string user)
{
	// skip the queue and send instant chat command
	if (chatCommand.empty())
		return;

	unsigned int payloadLength = user.length() + chatCommand.length() + 4;

	if (payloadLength <= CBNET::BNET_MAX_WHISPER_SIZE) {
		SendChatCommand("/w " + user + " " + chatCommand);
	}
	else {
		unsigned int chunkSize = CBNET::BNET_MAX_WHISPER_SIZE - user.length() - 4;

		for (unsigned int i=0; i<chatCommand.length(); i+=chunkSize) {
			string chunk = chatCommand.substr(i, chunkSize);
			SendChatCommand("/w " + user + " " + chunk);
		}
	}
}

void CBNET::SendDelayedWhisper(string chatCommand, string user, uint32_t msec)
{

	if (chatCommand.empty())
		return;

	pair<string, uint32_t> pair("/w " + user + " " + chatCommand, GetTicks() + msec);
	m_DelayedWhispers.push_back(pair);
}

bool CBNET::IsRootAdmin(string name)
{
	// m_RootAdmin was already transformed to lower case in the constructor
	transform(name.begin(), name.end(), name.begin(), (int(*)(int))tolower);

	// updated to permit multiple root admins seperated by a space, e.g. "Varlock Kilranin Instinct121"
	// note: this function gets called frequently so it would be better to parse the root admins just once and store them in a list somewhere
	// however, it's hardly worth optimizing at this point since the code's already written

	stringstream SS;
	string s;
	SS << m_RootAdmin;

	while (!SS.eof())
	{
		SS >> s;

		if (name == s)
			return true;
	}

	return false;
}

bool CBNET::IsContributor(string name)
{
	transform(name.begin(), name.end(), name.begin(), (int(*)(int))tolower);

	for (list<string> ::iterator i = m_Contributors.begin(); i != m_Contributors.end(); ++i)
	{
		if (*i == name)
			return true;
	}

	return false;
}

/*CDBBan *CBNET :: IsBannedName( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	// todotodo: optimize this - maybe use a map?

	for( vector<CDBBan *> :: iterator i = m_Bans.begin( ); i != m_Bans.end( ); i++ )
	{
		if( (*i)->GetName( ) == name )
		{
			if( (*i)->GetDuration( ) )
			{
				string BannedDateStr = (*i)->GetDatetime( );
				tm BannedDate;

				for( uint16_t i=0; i<BannedDateStr.length( ); i++ )
				{
					if( BannedDateStr.at( i ) == '-' || BannedDateStr.at( i ) == ':' )
						BannedDateStr.replace( i, 1, " " );
				}

				stringstream SS;
				SS << BannedDateStr;

				SS >> BannedDate.tm_year;
				SS >> BannedDate.tm_mon;
				SS >> BannedDate.tm_mday;
				SS >> BannedDate.tm_hour;
				SS >> BannedDate.tm_min;
				SS >> BannedDate.tm_sec;

				BannedDate.tm_mon -= 1;  // month range is from 0 to 11
				BannedDate.tm_year -= 1900;  // years since 1900

				uint32_t BannedDateInSec = mktime( &BannedDate );

				if( m_GHost->GetMysqlCurrentTime( ) > ( BannedDateInSec + (*i)->GetDuration( ) ) )
				{
					// ban has expired, unban the user silently(don't write it in chat)
					m_PairedBanSilentRemoves.push_back( PairedBanRemove( string( ), m_GHost->m_DB->ThreadedBanRemove( m_Server, name ) ) );
					//m_GHost->m_Manager->SendUnban( m_Server, name );
					m_GHost->m_Manager->Send( MASL_PROTOCOL :: MTS_USER_WASUNBANNED, m_Server + " " + name );

					return NULL;
				}
			}
			return *i;
		}
	}

	return NULL;
}*/

/*CDBBan *CBNET :: IsBannedIP( string ip )
{
	// todotodo: optimize this - maybe use a map?

	for( vector<CDBBan *> :: iterator i = m_Bans.begin( ); i != m_Bans.end( ); i++ )
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
	m_Bans.push_back( new CDBBan( m_Server, name, ip, "N/A", gamename, admin, reason, 0 ) );
}*/

/*void CBNET :: AddBan( string name, string ip, string gamename, string admin, string reason, uint32_t duration )
{
	time_t MysqlTimeT = m_GHost->GetMysqlCurrentTime( );
	tm *MysqlTime;
	MysqlTime = localtime( &MysqlTimeT );

	MysqlTime->tm_year += 1900;
	MysqlTime->tm_mon += 1;

	string Datetime = UTIL_ToString( MysqlTime->tm_year ) + "-" + UTIL_ToString( MysqlTime->tm_mon ) + "-" + UTIL_ToString( MysqlTime->tm_mday ) + " " + UTIL_ToString( MysqlTime->tm_hour ) + ":" + UTIL_ToString( MysqlTime->tm_min ) + ":" + UTIL_ToString( MysqlTime->tm_sec );

	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	m_Bans.push_back( new CDBBan( m_Server, name, ip, Datetime, gamename, admin, reason, duration ) );
}*/

/*void CBNET :: RemoveAdmin( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( vector<string> :: iterator i = m_Admins.begin( ); i != m_Admins.end( ); )
	{
		if( *i == name )
			i = m_Admins.erase( i );
		else
			i++;
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
			i++;
	}
}*/

bool CBNET::IsInChannel(string name)
{
	transform(name.begin(), name.end(), name.begin(), (int(*)(int))tolower);

	for (list<string> ::iterator i = m_PlayersInChannel.begin(); i != m_PlayersInChannel.end(); ++i)
	{
		if ((*i) == name)
			return true;
	}

	return false;
}
