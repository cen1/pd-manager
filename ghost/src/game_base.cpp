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

#include "game_base.h"
#include "bnet.h"
#include "config.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "ghost.h"
#include "ghostdb.h"
#include "language.h"
#include "map.h"
#include "masl_manager.h"
#include "masl_protocol_2.h"
#include "packed.h"
#include "replay.h"
#include "savegame.h"
#include "socket.h"
#include "util.h"

#include <cmath>
#include <string.h>
#include <time.h>

#include "next_combination.h"

//
// sorting classes
//

class CGamePlayerSortAscByPing {
  public:
	bool operator()(CGamePlayer* Player1, CGamePlayer* Player2) const
	{
		return Player1->GetPing(false) < Player2->GetPing(false);
	}
};

class CGamePlayerSortDescByPing {
  public:
	bool operator()(CGamePlayer* Player1, CGamePlayer* Player2) const
	{
		return Player1->GetPing(false) > Player2->GetPing(false);
	}
};

//
// CGameLog
//

CGameLog ::CGameLog(CBaseGame* nGame)
  : m_Game(nGame)
{
	BYTEARRAY Header = UTIL_CreateByteArray((uint32_t)1, true);
	m_GameLog += string(Header.begin(), Header.end());
}

void CGameLog ::AddMessage(unsigned char extraFlags, string from, string message)
{
	if (!message.empty()) {
		uint32_t Seconds;

		if (m_Game->m_GameLoading)
			Seconds = GetTime() - m_Game->m_GameStartedLoadingTime;
		else if (m_Game->m_GameLoaded)
			Seconds = m_Game->m_GameTicks / 1000;
		else
			Seconds = GetTime() - m_Game->m_FirstCreationTime;

		// CONSOLE_Print( "Seconds = " + UTIL_ToString( Seconds ) );

		BYTEARRAY Message = BYTEARRAY(1, extraFlags);
		UTIL_AppendByteArray(Message, (uint16_t)Seconds, true);
		Message.push_back((unsigned char)from.size());
		Message.push_back((unsigned char)message.size());
		UTIL_AppendByteArray(Message, UTIL_CreateByteArray((unsigned char*)from.c_str(), from.size()));
		UTIL_AppendByteArray(Message, UTIL_CreateByteArray((unsigned char*)message.c_str(), message.size()));

		m_GameLog += string(Message.begin(), Message.end());

		// CONSOLE_Print( "Message = " + UTIL_ByteArrayToHexString( Message ) );

		// m_GameLog += "1 " + UTIL_ToString( extraFlags ) + " " + from + " " + UTIL_ToString( Seconds / 60 ) + ":" + UTIL_ToString( Seconds % 60 ) + " " + UTIL_ToString( message.size( ) ) + " " + message;
	}
}

void CGameLog ::AddMessage(string from, string message)
{
	if (!message.empty()) {
		uint32_t Seconds;

		if (m_Game->m_GameLoading)
			Seconds = GetTime() - m_Game->m_GameStartedLoadingTime;
		else if (m_Game->m_GameLoaded)
			Seconds = m_Game->m_GameTicks / 1000;
		else
			Seconds = GetTime() - m_Game->m_FirstCreationTime;

		// CONSOLE_Print( "Seconds = " + UTIL_ToString( Seconds ) );

		BYTEARRAY Message = BYTEARRAY(1, (unsigned char)0);
		UTIL_AppendByteArray(Message, (uint16_t)Seconds, true);
		Message.push_back((unsigned char)from.size());
		Message.push_back((unsigned char)message.size());
		UTIL_AppendByteArray(Message, UTIL_CreateByteArray((unsigned char*)from.c_str(), from.size()));
		UTIL_AppendByteArray(Message, UTIL_CreateByteArray((unsigned char*)message.c_str(), message.size()));

		m_GameLog += string(Message.begin(), Message.end());

		// CONSOLE_Print( "Message = " + UTIL_ByteArrayToHexString( Message ) );

		// m_GameLog += "2 " + from + " " + UTIL_ToString( Seconds / 60 ) + ":" + UTIL_ToString( Seconds % 60 ) + " " + UTIL_ToString( message.size( ) ) + " " + message;
	}
}

void CGameLog ::AddMessage(string message)
{
	if (!message.empty()) {
		uint32_t Seconds;

		if (m_Game->m_GameLoading)
			Seconds = GetTime() - m_Game->m_GameStartedLoadingTime;
		else if (m_Game->m_GameLoaded)
			Seconds = m_Game->m_GameTicks / 1000;
		else
			Seconds = GetTime() - m_Game->m_FirstCreationTime;

		// CONSOLE_Print( "Seconds = " + UTIL_ToString( Seconds ) );

		BYTEARRAY Message = BYTEARRAY(1, (unsigned char)0);
		UTIL_AppendByteArray(Message, (uint16_t)Seconds, true);
		Message.push_back((unsigned char)0);
		Message.push_back((unsigned char)message.size());
		// UTIL_AppendByteArray( Message, UTIL_CreateByteArray( from.c_str( ), from.size( ) ) );
		UTIL_AppendByteArray(Message, UTIL_CreateByteArray((unsigned char*)message.c_str(), message.size()));

		m_GameLog += string(Message.begin(), Message.end());

		// CONSOLE_Print( "Message = " + UTIL_ByteArrayToHexString( Message ) );

		// m_GameLog += "3 " + UTIL_ToString( Seconds / 60 ) + ":" + UTIL_ToString( Seconds % 60 ) + " " + UTIL_ToString( message.size( ) ) + " " + message;
	}
}

void CGameLog::AddMessage(CGamePlayer* reporter, string message)
{
	if (reporter->GetSelectedForGameLog()) {
		AddMessage("[" + reporter->GetName() + "]" + message);
	}
}

//
// CBaseGame
//

CBaseGame::CBaseGame(CGHost* nGHost, CMap* nMap, CSaveGame* nSaveGame, uint16_t nHostPort, unsigned char nGameState, string nGameName, string nOwnerName, string nCreatorName, string nCreatorServer, uint32_t nGameType)
{
	m_GHost = nGHost;
	m_Socket = new CTCPServer();
	m_Protocol = new CGameProtocol(m_GHost);

	m_Map = new CMap(*nMap);
	m_MapPath = m_Map->GetMapPath();
	m_SaveGame = nSaveGame;

	if (m_GHost->m_SaveReplays && !m_SaveGame)
		m_Replay = new CReplay();
	else
		m_Replay = NULL;

	m_Exiting = false;
	// m_Saving = false;
	m_HostPort = nHostPort;
	m_GameState = nGameState;
	m_VirtualHostPID = 255;
	m_FakePlayerPID = 255;

	// wait time of 1 minute  = 0 empty actions required
	// wait time of 2 minutes = 1 empty action required
	// etc...

	if (m_GHost->m_ReconnectWaitTime == 0)
		m_GProxyEmptyActions = 0;
	else {
		m_GProxyEmptyActions = m_GHost->m_ReconnectWaitTime - 1;

		// clamp to 9 empty actions (10 minutes)

		if (m_GProxyEmptyActions > 9)
			m_GProxyEmptyActions = 9;
	}

	m_GameName = nGameName;
	m_LastGameName = nGameName;
	m_VirtualHostName = m_GHost->m_VirtualHostName;
	m_OwnerName = nOwnerName;
	m_CreatorName = nCreatorName;
	m_CreatorServer = nCreatorServer;
	m_GameType = nGameType;
	m_HCLCommandString = m_Map->GetMapDefaultHCL();
	m_RandomSeed = GetTicks();
	m_HostCounter = m_GHost->m_HostCounter++;
	m_EntryKey = rand();
	m_Latency = m_GHost->m_Latency;
	m_SyncLimit = m_GHost->m_SyncLimit;
	m_SyncCounter = 0;
	m_GameTicks = 0;
	m_CreationTime = GetTime();
	m_TrueCreationTime = GetTime();
	m_LastPingTime = GetTime();
	m_LastRefreshTime = GetTime();
	m_LastDownloadTicks = GetTime();
	m_DownloadCounter = 0;
	m_LastDownloadCounterResetTicks = GetTicks();
	// m_LastAnnounceTime = 0;
	// m_AnnounceInterval = 0;
	m_LastAutoStartTime = GetTime();
	m_AutoStartPlayers = 0;
	m_LastCountDownTicks = 0;
	m_CountDownCounter = 0;
	m_StartedLoadingTicks = 0;
	m_StartPlayers = 0;
	m_LastLagScreenResetTime = 0;
	m_LastActionSentTicks = 0;
	m_LastActionLateBy = 0;
	m_StartedLaggingTime = 0;
	m_LastLagScreenTime = 0;
	m_LastReservedSeen = GetTime();
	m_StartedKickVoteTime = 0;
	m_GameOverTime = 0;
	m_LastPlayerLeaveTicks = 0;
	m_MinimumScore = 0.0;
	m_MaximumScore = 0.0;
	m_SlotInfoChanged = false;
	m_Locked = false;
	m_RefreshMessages = m_GHost->m_RefreshMessages;
	m_RefreshError = false;
	m_RefreshRehosted = false;
	m_MuteAll = false;
	m_MuteLobby = false;
	m_CountDownStarted = false;
	m_GameLoading = false;
	m_GameLoaded = false;
	m_LoadInGame = m_Map->GetMapLoadInGame();
	m_Lagging = false;
	m_AutoSave = m_GHost->m_AutoSave;
	m_MatchMaking = false;
	m_LocalAdminMessages = m_GHost->m_LocalAdminMessages;

	// did you know
	m_LastDidYouKnowTime = GetTime();
	m_DidYouKnowIntervalInSeconds = 120;

	if (m_SaveGame) {
		m_EnforceSlots = m_SaveGame->GetSlots();
		m_Slots = m_SaveGame->GetSlots();

		// the savegame slots contain player entries
		// we really just want the open/closed/computer entries
		// so open all the player slots

		for (vector<CGameSlot>::iterator i = m_Slots.begin(); i != m_Slots.end(); ++i) {
			if ((*i).GetSlotStatus() == SLOTSTATUS_OCCUPIED && (*i).GetComputer() == 0) {
				(*i).SetPID(0);
				(*i).SetDownloadStatus(255);
				(*i).SetSlotStatus(SLOTSTATUS_OPEN);
			}
		}
	}
	else
		m_Slots = m_Map->GetSlots();

	if (!m_GHost->m_IPBlackListFile.empty()) {
		ifstream in;
		in.open(m_GHost->m_IPBlackListFile.c_str());

		if (in.fail())
			CONSOLE_Print("[GAME: " + m_GameName + "] error loading IP blacklist file [" + m_GHost->m_IPBlackListFile + "]");
		else {
			CONSOLE_Print("[GAME: " + m_GameName + "] loading IP blacklist file [" + m_GHost->m_IPBlackListFile + "]");
			string Line;

			while (!in.eof()) {
				getline(in, Line);

				// ignore blank lines and comments

				if (Line.empty() || Line[0] == '#')
					continue;

				// remove newlines and partial newlines to help fix issues with Windows formatted files on Linux systems

				Line.erase(remove(Line.begin(), Line.end(), ' '), Line.end());
				Line.erase(remove(Line.begin(), Line.end(), '\r'), Line.end());
				Line.erase(remove(Line.begin(), Line.end(), '\n'), Line.end());

				// ignore lines that don't look like IP addresses

				if (Line.find_first_not_of("1234567890.") != string ::npos)
					continue;

				m_IPBlackList.insert(Line);
			}

			in.close();

			CONSOLE_Print("[GAME: " + m_GameName + "] loaded " + UTIL_ToString(m_IPBlackList.size()) + " lines from IP blacklist file");
		}
	}

	// start listening for connections

	if (!m_GHost->m_BindAddress.empty())
		CONSOLE_Print("[GAME: " + m_GameName + "] attempting to bind to address [" + m_GHost->m_BindAddress + "]");
	else
		CONSOLE_Print("[GAME: " + m_GameName + "] attempting to bind to all available addresses");

	if (m_Socket->Listen(m_GHost->m_BindAddress, m_HostPort))
		CONSOLE_Print("[GAME: " + m_GameName + "] listening on port " + UTIL_ToString(m_HostPort));
	else {
		CONSOLE_Print("[GAME: " + m_GameName + "] error listening on port " + UTIL_ToString(m_HostPort));
		m_Exiting = true;
	}

	m_GameID = ++m_GHost->m_LastGameID;
	// m_RehostCounter = 0;
	m_GameNameRehostCounter = 0;
	m_VoteRemakeStartedTime = 0;
	m_GameLoadedPlayers = 0;
	m_GameLoadedTime = 0;
	m_LastOwnerActiveTime = GetTime();
	m_FirstCreationTime = GetTime();
	m_VoteRemakeStarted = false;
	m_LobbyLog = new CGameLog(this);
	m_GameLog = new CGameLog(this);
	m_RMK = false;
	m_MySQLGameID = 0;
	m_Saved = false;
}

CBaseGame::~CBaseGame()
{
	// save replay
	// todotodo: put this in a thread

	if (m_Replay && (m_GameLoading || m_GameLoaded)) {
		/*time_t Now = time( NULL );
		char Time[17];
		memset( Time, 0, sizeof( char ) * 17 );
		strftime( Time, sizeof( char ) * 17, "%Y-%m-%d %H-%M", localtime( &Now ) );
		string MinString = UTIL_ToString( ( m_GameTicks / 1000 ) / 60 );
		string SecString = UTIL_ToString( ( m_GameTicks / 1000 ) % 60 );

		if( MinString.size( ) == 1 )
			MinString.insert( 0, "0" );

		if( SecString.size( ) == 1 )
			SecString.insert( 0, "0" );*/

		m_Replay->BuildReplay(m_GameName, m_StatString, m_GHost->m_ReplayWar3Version, m_GHost->m_ReplayBuildNumber);
		// m_Replay->Save( m_GHost->m_TFT, m_GHost->m_ReplayPath + UTIL_FileSafeName( "GHost++ " + string( Time ) + " " + m_GameName + " (" + MinString + "m" + SecString + "s).w3g" ) );
		m_Replay->Save(m_GHost->m_TFT, m_GHost->m_ReplayPath + m_ReplayName);
	}

	delete m_Socket;
	delete m_Protocol;
	delete m_Map;
	delete m_Replay;

	for (vector<CPotentialPlayer*>::iterator i = m_Potentials.begin(); i != m_Potentials.end(); ++i)
		delete *i;

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i)
		delete *i;

	/*for( vector<CCallableScoreCheck *> :: iterator i = m_ScoreChecks.begin( ); i != m_ScoreChecks.end( ); ++i )
		m_GHost->m_Callables.push_back( *i );*/

	while (!m_Actions.empty()) {
		delete m_Actions.front();
		m_Actions.pop();
	}

	for (list<CCallablePlayerCheck*>::iterator i = m_PlayerChecks.begin(); i != m_PlayerChecks.end(); ++i)
		delete *i;

	delete m_LobbyLog;
	delete m_GameLog;
}

uint32_t CBaseGame::GetNextTimedActionTicks()
{
	// return the number of ticks (ms) until the next "timed action", which for our purposes is the next game update
	// the main GHost++ loop will make sure the next loop update happens at or before this value
	// note: there's no reason this function couldn't take into account the game's other timers too but they're far less critical
	// warning: this function must take into account when actions are not being sent (e.g. during loading or lagging)

	if (!m_GameLoaded || m_Lagging)
		return 50;

	uint32_t TicksSinceLastUpdate = GetTicks() - m_LastActionSentTicks;

	if (TicksSinceLastUpdate > m_Latency - m_LastActionLateBy)
		return 0;
	else
		return m_Latency - m_LastActionLateBy - TicksSinceLastUpdate;
}

uint32_t CBaseGame::GetSlotsOccupied()
{
	uint32_t NumSlotsOccupied = 0;

	for (vector<CGameSlot>::iterator i = m_Slots.begin(); i != m_Slots.end(); ++i) {
		if ((*i).GetSlotStatus() == SLOTSTATUS_OCCUPIED)
			++NumSlotsOccupied;
	}

	return NumSlotsOccupied;
}

uint32_t CBaseGame::GetSlotsOpen()
{
	uint32_t NumSlotsOpen = 0;

	for (vector<CGameSlot>::iterator i = m_Slots.begin(); i != m_Slots.end(); ++i) {
		if ((*i).GetSlotStatus() == SLOTSTATUS_OPEN)
			++NumSlotsOpen;
	}

	return NumSlotsOpen;
}

uint32_t CBaseGame::GetNumPlayers()
{
	uint32_t NumPlayers = GetNumHumanPlayers();

	if (m_FakePlayerPID != 255)
		++NumPlayers;

	return NumPlayers;
}

uint32_t CBaseGame::GetNumHumanPlayers()
{
	uint32_t NumHumanPlayers = 0;

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if (!(*i)->GetLeftMessageSent())
			++NumHumanPlayers;
	}

	return NumHumanPlayers;
}

string CBaseGame::GetDescription()
{
	string Description = m_GameName + " : " + m_OwnerName + " : " + UTIL_ToString(GetNumHumanPlayers()) + "/" + UTIL_ToString(m_GameLoading || m_GameLoaded ? m_StartPlayers : m_Slots.size());

	if (m_GameLoading || m_GameLoaded)
		Description += " : " + UTIL_ToString((m_GameTicks / 1000) / 60) + "m";
	else
		Description += " : " + UTIL_ToString((GetTime() - m_CreationTime) / 60) + "m";

	return Description;
}

/*void CBaseGame:: SetAnnounce( uint32_t interval, string message )
{
	m_AnnounceInterval = interval;
	m_AnnounceMessage = message;
	m_LastAnnounceTime = GetTime( );
}*/

unsigned int CBaseGame::SetFD(void* fd, void* send_fd, int* nfds)
{
	unsigned int NumFDs = 0;

	if (m_Socket) {
		m_Socket->SetFD((fd_set*)fd, (fd_set*)send_fd, nfds);
		++NumFDs;
	}

	for (vector<CPotentialPlayer*>::iterator i = m_Potentials.begin(); i != m_Potentials.end(); ++i) {
		if ((*i)->GetSocket()) {
			(*i)->GetSocket()->SetFD((fd_set*)fd, (fd_set*)send_fd, nfds);
			++NumFDs;
		}
	}

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if ((*i)->GetSocket()) {
			(*i)->GetSocket()->SetFD((fd_set*)fd, (fd_set*)send_fd, nfds);
			++NumFDs;
		}
	}

	return NumFDs;
}

bool CBaseGame::Update(void* fd, void* send_fd)
{
	if (!m_GameLoading && !m_GameLoaded) {
		// unhost the game if the owner is inactive for more then 10 minutes
		if (GetTime() - m_LastOwnerActiveTime > 600)
			m_Exiting = true;

		// don't let lobby last more than 1 hour
		if (GetTime() - m_TrueCreationTime > 3600)
			m_Exiting = true;

		// spam did you know facts
		if (m_GHost->m_DidYouKnowEnabled && GetTime() - m_LastDidYouKnowTime >= m_DidYouKnowIntervalInSeconds) {
			string fact = "[Did you know?] " + m_GHost->m_DidYouKnowGenerator.getRandomPhrase();
			SendAllChat(fact);
			m_LastDidYouKnowTime = GetTime();
		}
	}

	// update callables

	/*for( vector<CCallableScoreCheck *> :: iterator i = m_ScoreChecks.begin( ); i != m_ScoreChecks.end( ); )
	{
		if( (*i)->GetReady( ) )
		{
			double Score = (*i)->GetResult( );

			for( vector<CPotentialPlayer *> :: iterator j = m_Potentials.begin( ); j != m_Potentials.end( ); ++j )
			{
				if( (*j)->GetJoinPlayer( ) && (*j)->GetJoinPlayer( )->GetName( ) == (*i)->GetName( ) )
					EventPlayerJoinedWithScore( *j, (*j)->GetJoinPlayer( ), Score );
			}

			m_GHost->m_DB->RecoverCallable( *i );
			delete *i;
			i = m_ScoreChecks.erase( i );
		}
		else
			++i;
	}*/

	for (list<CCallablePlayerCheck*>::iterator i = m_PlayerChecks.begin(); i != m_PlayerChecks.end();) {
		if ((*i)->GetReady()) {
			for (vector<CPotentialPlayer*>::iterator j = m_Potentials.begin(); j != m_Potentials.end(); ++j) {
				if ((*j)->GetJoinPlayer() && (*j)->GetJoinPlayer()->GetName() == (*i)->GetName())
					EventPlayerJoinedWithInfo(*j, (*j)->GetJoinPlayer(), (*i)->GetResult());
			}

			delete *i;
			i = m_PlayerChecks.erase(i);
		}
		else
			++i;
	}

	// update players

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end();) {
		if ((*i)->Update(fd)) {
			EventPlayerDeleted(*i);
			delete *i;
			i = m_Players.erase(i);
		}
		else
			++i;
	}

	for (vector<CPotentialPlayer*>::iterator i = m_Potentials.begin(); i != m_Potentials.end();) {
		if ((*i)->Update(fd)) {
			// flush the socket (e.g. in case a rejection message is queued)

			if ((*i)->GetSocket())
				(*i)->GetSocket()->DoSend((fd_set*)send_fd);

			delete *i;
			i = m_Potentials.erase(i);
		}
		else
			++i;
	}

	// create the virtual host player

	if (!m_GameLoading && !m_GameLoaded && GetNumPlayers() < 12)
		CreateVirtualHost();

	// unlock the game

	/*if( m_Locked && !GetPlayerFromName( m_OwnerName, false ) )
	{
		SendAllChat( m_GHost->m_Language->GameUnlocked( ) );
		m_Locked = false;
	}*/

	// ping every 5 seconds
	// changed this to ping during game loading as well to hopefully fix some problems with people disconnecting during loading
	// changed this to ping during the game as well

	if (GetTime() - m_LastPingTime >= 5) {
		// note: we must send pings to players who are downloading the map because Warcraft III disconnects from the lobby if it doesn't receive a ping every ~90 seconds
		// so if the player takes longer than 90 seconds to download the map they would be disconnected unless we keep sending pings
		// todotodo: ignore pings received from players who have recently finished downloading the map

		SendAll(m_Protocol->SEND_W3GS_PING_FROM_HOST());

		// we also broadcast the game to the local network every 5 seconds so we hijack this timer for our nefarious purposes
		// however we only want to broadcast if the countdown hasn't started
		// see the !sendlan code later in this file for some more information about how this works
		// todotodo: should we send a game cancel message somewhere? we'll need to implement a host counter for it to work

		if (!m_CountDownStarted) {
			// construct a fixed host counter which will be used to identify players from this "realm" (i.e. LAN)
			// the fixed host counter's 4 most significant bits will contain a 4 bit ID (0-15)
			// the rest of the fixed host counter will contain the 28 least significant bits of the actual host counter
			// since we're destroying 4 bits of information here the actual host counter should not be greater than 2^28 which is a reasonable assumption
			// when a player joins a game we can obtain the ID from the received host counter
			// note: LAN broadcasts use an ID of 0, battle.net refreshes use an ID of 1-10, the rest are unused

			uint32_t FixedHostCounter = m_HostCounter & 0x0FFFFFFF;

			// we send 12 for SlotsTotal because this determines how many PID's Warcraft 3 allocates
			// we need to make sure Warcraft 3 allocates at least SlotsTotal + 1 but at most 12 PID's
			// this is because we need an extra PID for the virtual host player (but we always delete the virtual host player when the 12th person joins)
			// however, we can't send 13 for SlotsTotal because this causes Warcraft 3 to crash when sharing control of units
			// nor can we send SlotsTotal because then Warcraft 3 crashes when playing maps with less than 12 PID's (because of the virtual host player taking an extra PID)
			// we also send 12 for SlotsOpen because Warcraft 3 assumes there's always at least one player in the game (the host)
			// so if we try to send accurate numbers it'll always be off by one and results in Warcraft 3 assuming the game is full when it still needs one more player
			// the easiest solution is to simply send 12 for both so the game will always show up as (1/12) players

			if (m_SaveGame) {
				// note: the PrivateGame flag is not set when broadcasting to LAN (as you might expect)

				uint32_t MapGameType = MAPGAMETYPE_SAVEDGAME;
				BYTEARRAY MapWidth;
				MapWidth.push_back(0);
				MapWidth.push_back(0);
				BYTEARRAY MapHeight;
				MapHeight.push_back(0);
				MapHeight.push_back(0);
				m_GHost->m_UDPSocket->Broadcast(6112, m_Protocol->SEND_W3GS_GAMEINFO(m_GHost->m_TFT, m_GHost->m_LANWar3Version, UTIL_CreateByteArray(MapGameType, false), m_Map->GetMapGameFlags(), MapWidth, MapHeight, m_GameName, "Varlock", GetTime() - m_CreationTime, "Save\\Multiplayer\\" + m_SaveGame->GetFileNameNoPath(), m_SaveGame->GetMagicNumber(), 12, 12, m_HostPort, FixedHostCounter, m_EntryKey));
			}
			else {
				// note: the PrivateGame flag is not set when broadcasting to LAN (as you might expect)
				// note: we do not use m_Map->GetMapGameType because none of the filters are set when broadcasting to LAN (also as you might expect)

				uint32_t MapGameType = MAPGAMETYPE_UNKNOWN0;
				m_GHost->m_UDPSocket->Broadcast(6112, m_Protocol->SEND_W3GS_GAMEINFO(m_GHost->m_TFT, m_GHost->m_LANWar3Version, UTIL_CreateByteArray(MapGameType, false), m_Map->GetMapGameFlags(), m_Map->GetMapWidth(), m_Map->GetMapHeight(), m_GameName, "Varlock", GetTime() - m_CreationTime, m_Map->GetMapPath(), m_Map->GetMapCRC(), 12, 12, m_HostPort, FixedHostCounter, m_EntryKey));
			}
		}

		m_LastPingTime = GetTime();
	}

	// auto rehost if there was a refresh error in autohosted games

	/*if( m_RefreshError && !m_CountDownStarted && m_GameState == GAME_PUBLIC && !m_GHost->m_AutoHostGameName.empty( ) && m_GHost->m_AutoHostMaximumGames != 0 && m_GHost->m_AutoHostAutoStartPlayers != 0 && m_AutoStartPlayers != 0 )
	{
		// there's a slim chance that this isn't actually an autohosted game since there is no explicit autohost flag
		// however, if autohosting is enabled and this game is public and this game is set to autostart, it's probably autohosted
		// so rehost it using the current autohost game name

		string GameName = m_GHost->m_AutoHostGameName + " #" + UTIL_ToString( m_GHost->m_HostCounter );
		CONSOLE_Print( "[GAME: " + m_GameName + "] automatically trying to rehost as public game [" + GameName + "] due to refresh failure" );
		m_LastGameName = m_GameName;
		m_GameName = GameName;
		m_HostCounter = m_GHost->m_HostCounter++;
		m_RefreshError = false;

		for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
		{
			(*i)->QueueGameUncreate( );
			(*i)->QueueEnterChat( );

			// the game creation message will be sent on the next refresh
		}

		m_CreationTime = GetTime( );
		m_LastRefreshTime = GetTime( );
	}*/

	// refresh every 3 seconds

	if (!m_RefreshError && !m_CountDownStarted && m_GameState == GAME_PUBLIC && GetSlotsOpen() > 0 && GetTime() - m_LastRefreshTime >= 3) {
		// send a game refresh packet to each battle.net connection

		bool Refreshed = false;

		for (vector<CBNET*>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i) {
			// don't queue a game refresh message if the queue contains more than 1 packet because they're very low priority

			if ((*i)->GetOutPacketsQueued() <= 1) {
				(*i)->QueueGameRefresh(m_GameState, m_GameName, string(), m_Map, m_SaveGame, GetTime() - m_CreationTime, m_HostCounter);
				Refreshed = true;
			}
		}

		// only print the "game refreshed" message if we actually refreshed on at least one battle.net server

		if (m_RefreshMessages && Refreshed)
			SendAllChat(m_GHost->m_Language->GameRefreshed());

		m_LastRefreshTime = GetTime();
	}

	// send more map data

	if (!m_GameLoading && !m_GameLoaded && GetTicks() - m_LastDownloadCounterResetTicks >= 1000) {
		// hackhack: another timer hijack is in progress here
		// since the download counter is reset once per second it's a great place to update the slot info if necessary

		if (m_SlotInfoChanged)
			SendAllSlotInfo();

		m_DownloadCounter = 0;
		m_LastDownloadCounterResetTicks = GetTicks();
	}

	if (!m_GameLoading && !m_GameLoaded && GetTicks() - m_LastDownloadTicks >= 100) {
		uint32_t Downloaders = 0;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			if ((*i)->GetDownloadStarted() && !(*i)->GetDownloadFinished()) {
				Downloaders++;

				if (m_GHost->m_MaxDownloaders > 0 && Downloaders > m_GHost->m_MaxDownloaders)
					break;

				// send up to 100 pieces of the map at once so that the download goes faster
				// if we wait for each MAPPART packet to be acknowledged by the client it'll take a long time to download
				// this is because we would have to wait the round trip time (the ping time) between sending every 1442 bytes of map data
				// doing it this way allows us to send at least 140 KB in each round trip interval which is much more reasonable
				// the theoretical throughput is [140 KB * 1000 / ping] in KB/sec so someone with 100 ping (round trip ping, not LC ping) could download at 1400 KB/sec
				// note: this creates a queue of map data which clogs up the connection when the client is on a slower connection (e.g. dialup)
				// in this case any changes to the lobby are delayed by the amount of time it takes to send the queued data (i.e. 140 KB, which could be 30 seconds or more)
				// for example, players joining and leaving, slot changes, chat messages would all appear to happen much later for the low bandwidth player
				// note: the throughput is also limited by the number of times this code is executed each second
				// e.g. if we send the maximum amount (140 KB) 10 times per second the theoretical throughput is 1400 KB/sec
				// therefore the maximum throughput is 1400 KB/sec regardless of ping and this value slowly diminishes as the player's ping increases
				// in addition to this, the throughput is limited by the configuration value bot_maxdownloadspeed
				// in summary: the actual throughput is MIN( 140 * 1000 / ping, 1400, bot_maxdownloadspeed ) in KB/sec assuming only one player is downloading the map

				uint32_t MapSize = UTIL_ByteArrayToUInt32(m_Map->GetMapSize(), false);

				while ((*i)->GetLastMapPartSent() < (*i)->GetLastMapPartAcked() + 1442 * 100 && (*i)->GetLastMapPartSent() < MapSize) {
					if ((*i)->GetLastMapPartSent() == 0) {
						// overwrite the "started download ticks" since this is the first time we've sent any map data to the player
						// prior to this we've only determined if the player needs to download the map but it's possible we could have delayed sending any data due to download limits

						(*i)->SetStartedDownloadingTicks(GetTicks());
					}

					// limit the download speed if we're sending too much data
					// the download counter is the # of map bytes downloaded in the last second (it's reset once per second)

					if (m_GHost->m_MaxDownloadSpeed > 0 && m_DownloadCounter > m_GHost->m_MaxDownloadSpeed * 1024)
						break;

					Send(*i, m_Protocol->SEND_W3GS_MAPPART(GetHostPID(), (*i)->GetPID(), (*i)->GetLastMapPartSent(), m_Map->GetMapData()));
					(*i)->SetLastMapPartSent((*i)->GetLastMapPartSent() + 1442);
					m_DownloadCounter += 1442;
				}
			}
		}

		m_LastDownloadTicks = GetTicks();
	}

	// announce every m_AnnounceInterval seconds

	/*if( !m_AnnounceMessage.empty( ) && !m_CountDownStarted && GetTime( ) - m_LastAnnounceTime >= m_AnnounceInterval )
	{
		SendAllChat( m_AnnounceMessage );
		m_LastAnnounceTime = GetTime( );
	}*/

	// kick players who don't spoof check within 20 seconds when spoof checks are required and the game is autohosted

	/*if( !m_CountDownStarted && m_GHost->m_RequireSpoofChecks && m_GameState == GAME_PUBLIC && !m_GHost->m_AutoHostGameName.empty( ) && m_GHost->m_AutoHostMaximumGames != 0 && m_GHost->m_AutoHostAutoStartPlayers != 0 && m_AutoStartPlayers != 0 )
	{
				for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); ++i )
		{
			if( !(*i)->GetSpoofed( ) && GetTime( ) - (*i)->GetJoinTime( ) >= 20 )
			{
				(*i)->SetDeleteMe( true );
				(*i)->SetLeftReason( m_GHost->m_Language->WasKickedForNotSpoofChecking( ) );
				(*i)->SetLeftCode( PLAYERLEAVE_LOBBY );
				OpenSlot( GetSIDFromPID( (*i)->GetPID( ) ), false );
			}
		}
	}*/

	// try to auto start every 10 seconds
	if (!m_CountDownStarted && m_AutoStartPlayers != 0 && GetTime() - m_LastAutoStartTime >= 10) {
		StartCountDownAuto(m_GHost->m_RequireSpoofChecks);
		m_LastAutoStartTime = GetTime();
	}

	// countdown every 500 ms

	if (m_CountDownStarted && GetTicks() - m_LastCountDownTicks >= 500) {
		if (m_CountDownCounter > 0) {
			// we use a countdown counter rather than a "finish countdown time" here because it might alternately round up or down the count
			// this sometimes resulted in a countdown of e.g. "6 5 3 2 1" during my testing which looks pretty dumb
			// doing it this way ensures it's always "5 4 3 2 1" but each interval might not be *exactly* the same length

			SendAllChat(UTIL_ToString(m_CountDownCounter) + ". . .");
			m_CountDownCounter--;
		}
		else if (!m_GameLoading && !m_GameLoaded)
			EventGameStarted();

		m_LastCountDownTicks = GetTicks();
	}

	// check if the lobby is "abandoned" and needs to be closed since it will never start

	// if( !m_GameLoading && !m_GameLoaded && m_AutoStartPlayers == 0 && m_GHost->m_LobbyTimeLimit > 0 )
	if (!m_GameLoading && !m_GameLoaded) {
		// check if there's a player with reserved status in the game

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			if ((*i)->GetReserved())
				m_LastReservedSeen = GetTime();
		}

		// check if we've hit the time limit

		// if( GetTime( ) - m_LastReservedSeen >= m_GHost->m_LobbyTimeLimit * 60 )
		if (GetTime() - m_LastReservedSeen >= 60 * 5) {
			CONSOLE_Print("[GAME: " + m_GameName + "] is over (lobby time limit hit)");
			return true;
		}
	}

	// check if the game is loaded

	if (m_GameLoading) {
		bool FinishedLoading = true;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			FinishedLoading = (*i)->GetFinishedLoading();

			if (!FinishedLoading)
				break;
		}

		if (FinishedLoading) {
			m_LastActionSentTicks = GetTicks();
			m_GameLoading = false;
			m_GameLoaded = true;
			EventGameLoaded();
		}
		else {
			// reset the "lag" screen (the load-in-game screen) every 30 seconds

			if (m_LoadInGame && GetTime() - m_LastLagScreenResetTime >= 30) {
				bool UsingGProxy = false;

				for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
					if ((*i)->GetGProxy())
						UsingGProxy = true;
				}

				for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
					if ((*i)->GetFinishedLoading()) {
						// stop the lag screen

						for (vector<CGamePlayer*>::iterator j = m_Players.begin(); j != m_Players.end(); ++j) {
							if (!(*j)->GetFinishedLoading())
								Send(*i, m_Protocol->SEND_W3GS_STOP_LAG(*j, true));
						}

						// send an empty update
						// this resets the lag screen timer but creates a rather annoying problem
						// in order to prevent a desync we must make sure every player receives the exact same "desyncable game data" (updates and player leaves) in the exact same order
						// unfortunately we cannot send updates to players who are still loading the map, so we buffer the updates to those players (see the else clause a few lines down for the code)
						// in addition to this we must ensure any player leave messages are sent in the exact same position relative to these updates so those must be buffered too

						if (UsingGProxy && !(*i)->GetGProxy()) {
							// we must send empty actions to non-GProxy++ players
							// GProxy++ will insert these itself so we don't need to send them to GProxy++ players
							// empty actions are used to extend the time a player can use when reconnecting

							for (unsigned char j = 0; j < m_GProxyEmptyActions; ++j)
								Send(*i, m_Protocol->SEND_W3GS_INCOMING_ACTION(queue<CIncomingAction*>(), 0));
						}

						Send(*i, m_Protocol->SEND_W3GS_INCOMING_ACTION(queue<CIncomingAction*>(), 0));

						// start the lag screen

						Send(*i, m_Protocol->SEND_W3GS_START_LAG(m_Players, true));
					}
					else {
						// buffer the empty update since the player is still loading the map

						if (UsingGProxy && !(*i)->GetGProxy()) {
							// we must send empty actions to non-GProxy++ players
							// GProxy++ will insert these itself so we don't need to send them to GProxy++ players
							// empty actions are used to extend the time a player can use when reconnecting

							for (unsigned char j = 0; j < m_GProxyEmptyActions; ++j)
								(*i)->AddLoadInGameData(m_Protocol->SEND_W3GS_INCOMING_ACTION(queue<CIncomingAction*>(), 0));
						}

						(*i)->AddLoadInGameData(m_Protocol->SEND_W3GS_INCOMING_ACTION(queue<CIncomingAction*>(), 0));
					}
				}

				// add actions to replay

				if (m_Replay) {
					if (UsingGProxy) {
						for (unsigned char i = 0; i < m_GProxyEmptyActions; ++i)
							m_Replay->AddTimeSlot(0, queue<CIncomingAction*>());
					}

					m_Replay->AddTimeSlot(0, queue<CIncomingAction*>());
				}

				// Warcraft III doesn't seem to respond to empty actions

				/* if( UsingGProxy )
					m_SyncCounter += m_GProxyEmptyActions;

				m_SyncCounter++; */
				m_LastLagScreenResetTime = GetTime();
			}
		}
	}

	// keep track of the largest sync counter (the number of keepalive packets received by each player)
	// if anyone falls behind by more than m_SyncLimit keepalives we start the lag screen

	if (m_GameLoaded) {
		// check if anyone has started lagging
		// we consider a player to have started lagging if they're more than m_SyncLimit keepalives behind

		if (!m_Lagging) {
			string LaggingString;

			for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
				if (m_SyncCounter - (*i)->GetSyncCounter() > m_SyncLimit) {
					(*i)->SetLagging(true);
					(*i)->SetStartedLaggingTicks(GetTicks());
					m_Lagging = true;
					m_StartedLaggingTime = GetTime();

					if (LaggingString.empty())
						LaggingString = (*i)->GetName();
					else
						LaggingString += ", " + (*i)->GetName();
				}
			}

			if (m_Lagging) {
				// start the lag screen

				CONSOLE_Print("[GAME: " + m_GameName + "] started lagging on [" + LaggingString + "]");
				SendAll(m_Protocol->SEND_W3GS_START_LAG(m_Players));

				// reset everyone's drop vote

				for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i)
					(*i)->SetDropVote(false);

				m_LastLagScreenResetTime = GetTime();
			}
		}

		if (m_Lagging) {
			bool UsingGProxy = false;

			for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
				if ((*i)->GetGProxy())
					UsingGProxy = true;
			}

			uint32_t WaitTime = 60;

			if (UsingGProxy)
				WaitTime = (m_GProxyEmptyActions + 1) * 60;

			if (GetTime() - m_StartedLaggingTime >= WaitTime)
				StopLaggers(m_GHost->m_Language->WasAutomaticallyDroppedAfterSeconds(UTIL_ToString(WaitTime)));

			// we cannot allow the lag screen to stay up for more than ~65 seconds because Warcraft III disconnects if it doesn't receive an action packet at least this often
			// one (easy) solution is to simply drop all the laggers if they lag for more than 60 seconds
			// another solution is to reset the lag screen the same way we reset it when using load-in-game
			// this is required in order to give GProxy++ clients more time to reconnect

			if (GetTime() - m_LastLagScreenResetTime >= 60) {
				for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
					// stop the lag screen

					for (vector<CGamePlayer*>::iterator j = m_Players.begin(); j != m_Players.end(); ++j) {
						if ((*j)->GetLagging())
							Send(*i, m_Protocol->SEND_W3GS_STOP_LAG(*j));
					}

					// send an empty update
					// this resets the lag screen timer

					if (UsingGProxy && !(*i)->GetGProxy()) {
						// we must send additional empty actions to non-GProxy++ players
						// GProxy++ will insert these itself so we don't need to send them to GProxy++ players
						// empty actions are used to extend the time a player can use when reconnecting

						for (unsigned char j = 0; j < m_GProxyEmptyActions; ++j)
							Send(*i, m_Protocol->SEND_W3GS_INCOMING_ACTION(queue<CIncomingAction*>(), 0));
					}

					Send(*i, m_Protocol->SEND_W3GS_INCOMING_ACTION(queue<CIncomingAction*>(), 0));

					// start the lag screen

					Send(*i, m_Protocol->SEND_W3GS_START_LAG(m_Players));
				}

				// add actions to replay

				if (m_Replay) {
					if (UsingGProxy) {
						for (unsigned char i = 0; i < m_GProxyEmptyActions; ++i)
							m_Replay->AddTimeSlot(0, queue<CIncomingAction*>());
					}

					m_Replay->AddTimeSlot(0, queue<CIncomingAction*>());
				}

				// Warcraft III doesn't seem to respond to empty actions

				/* if( UsingGProxy )
					m_SyncCounter += m_GProxyEmptyActions;

				m_SyncCounter++; */
				m_LastLagScreenResetTime = GetTime();
			}

			// check if anyone has stopped lagging normally
			// we consider a player to have stopped lagging if they're less than half m_SyncLimit keepalives behind

			for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
				if ((*i)->GetLagging() && m_SyncCounter - (*i)->GetSyncCounter() < m_SyncLimit / 2) {
					// stop the lag screen for this player

					CONSOLE_Print("[GAME: " + m_GameName + "] stopped lagging on [" + (*i)->GetName() + "]");
					SendAll(m_Protocol->SEND_W3GS_STOP_LAG(*i));
					(*i)->SetLagging(false);
					(*i)->SetStartedLaggingTicks(0);
				}
			}

			// check if everyone has stopped lagging

			bool Lagging = false;

			for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
				if ((*i)->GetLagging())
					Lagging = true;
			}

			m_Lagging = Lagging;

			// reset m_LastActionSentTicks because we want the game to stop running while the lag screen is up

			m_LastActionSentTicks = GetTicks();

			// keep track of the last lag screen time so we can avoid timing out players

			m_LastLagScreenTime = GetTime();
		}
	}

	// send actions every m_Latency milliseconds
	// actions are at the heart of every Warcraft 3 game but luckily we don't need to know their contents to relay them
	// we queue player actions in EventPlayerAction then just resend them in batches to all players here

	if (m_GameLoaded && !m_Lagging && GetTicks() - m_LastActionSentTicks >= m_Latency - m_LastActionLateBy)
		SendAllActions();

	// expire the votekick

	if (!m_KickVotePlayer.empty() && GetTime() - m_StartedKickVoteTime >= 60) {
		CONSOLE_Print("[GAME: " + m_GameName + "] votekick against player [" + m_KickVotePlayer + "] expired");
		SendAllChat(m_GHost->m_Language->VoteKickExpired(m_KickVotePlayer));
		m_KickVotePlayer.clear();
		m_StartedKickVoteTime = 0;
	}

	// expire the votermk after 5 minutes

	if (m_VoteRemakeStarted && GetTime() >= m_VoteRemakeStartedTime + 300) {
		CONSOLE_Print("[GAME: " + m_GameName + "] vote remake expired");

		m_VoteRemakeStarted = false;
		m_VoteRemakeStartedTime = 0;
	}

	// start the gameover timer if there's only one player left

	if (m_Players.size() == 1 && m_FakePlayerPID == 255 && m_GameOverTime == 0 && (m_GameLoading || m_GameLoaded)) {
		CONSOLE_Print("[GAME: " + m_GameName + "] gameover timer started (one player left)");
		m_GameOverTime = GetTime();
	}

	// finish the gameover timer. Kick all still active players.

	if (m_GameOverTime != 0 && GetTime() - m_GameOverTime >= 60) {
		bool AlreadyStopped = true;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			if (!(*i)->GetDeleteMe()) {
				AlreadyStopped = false;
				break;
			}
		}

		if (!AlreadyStopped) {
			CONSOLE_Print("[GAME: " + m_GameName + "] is over (gameover timer finished)");
			StopPlayers("was disconnected (gameover timer finished)");
		}
	}

	// end the game if there aren't any players left

	if (m_Players.empty() && (m_GameLoading || m_GameLoaded)) {
		if (!IsGameDataSaved()) {
			SaveGameData();
			return true;
		}
	}

	// accept new connections

	if (m_Socket) {
		CTCPSocket* NewSocket = m_Socket->Accept((fd_set*)fd);

		if (NewSocket) {
			// check the IP blacklist

			if (m_IPBlackList.find(NewSocket->GetIPString()) == m_IPBlackList.end()) {
				if (m_GHost->m_TCPNoDelay)
					NewSocket->SetNoDelay(true);

				m_Potentials.push_back(new CPotentialPlayer(m_Protocol, this, NewSocket));
			}
			else {
				CONSOLE_Print("[GAME: " + m_GameName + "] rejected connection from [" + NewSocket->GetIPString() + "] due to blacklist");
				delete NewSocket;
			}
		}

		if (m_Socket->HasError())
			return true;
	}

	return m_Exiting;
}

void CBaseGame::UpdatePost(void* send_fd)
{
	// we need to manually call DoSend on each player now because CGamePlayer :: Update doesn't do it
	// this is in case player 2 generates a packet for player 1 during the update but it doesn't get sent because player 1 already finished updating
	// in reality since we're queueing actions it might not make a big difference but oh well

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if ((*i)->GetSocket())
			(*i)->GetSocket()->DoSend((fd_set*)send_fd);
	}

	for (vector<CPotentialPlayer*>::iterator i = m_Potentials.begin(); i != m_Potentials.end(); ++i) {
		if ((*i)->GetSocket())
			(*i)->GetSocket()->DoSend((fd_set*)send_fd);
	}
}

void CBaseGame::Send(CGamePlayer* player, BYTEARRAY data)
{
	if (player)
		player->Send(data);
}

void CBaseGame::Send(unsigned char PID, BYTEARRAY data)
{
	Send(GetPlayerFromPID(PID), data);
}

void CBaseGame::Send(BYTEARRAY PIDs, BYTEARRAY data)
{
	for (unsigned int i = 0; i < PIDs.size(); ++i)
		Send(PIDs[i], data);
}

void CBaseGame::SendAll(BYTEARRAY data)
{
	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i)
		(*i)->Send(data);
}

void CBaseGame::SendChat(unsigned char fromPID, CGamePlayer* player, string message)
{
	// send a private message to one player - it'll be marked [Private] in Warcraft 3

	if (player) {
		if (!m_GameLoading && !m_GameLoaded) {
			if (message.size() > 254)
				message = message.substr(0, 254);

			Send(player, m_Protocol->SEND_W3GS_CHAT_FROM_HOST(fromPID, UTIL_CreateByteArray(player->GetPID()), 16, BYTEARRAY(), message));
		}
		else {
			unsigned char ExtraFlags[] = { 3, 0, 0, 0 };

			// based on my limited testing it seems that the extra flags' first byte contains 3 plus the recipient's colour to denote a private message

			unsigned char SID = GetSIDFromPID(player->GetPID());

			if (SID < m_Slots.size())
				ExtraFlags[0] = 3 + m_Slots[SID].GetColour();

			if (message.size() > 127)
				message = message.substr(0, 127);

			Send(player, m_Protocol->SEND_W3GS_CHAT_FROM_HOST(fromPID, UTIL_CreateByteArray(player->GetPID()), 32, UTIL_CreateByteArray(ExtraFlags, 4), message));
		}
	}
}

void CBaseGame::SendChat(unsigned char fromPID, unsigned char toPID, string message)
{
	SendChat(fromPID, GetPlayerFromPID(toPID), message);
}

void CBaseGame::SendChat(CGamePlayer* player, string message)
{
	SendChat(GetHostPID(), player, message);
}

void CBaseGame::SendChat(unsigned char toPID, string message)
{
	SendChat(GetHostPID(), toPID, message);
}

void CBaseGame::SendAllChat(unsigned char fromPID, string message)
{
	// send a public message to all players - it'll be marked [All] in Warcraft 3

	if (GetNumHumanPlayers() > 0) {
		CONSOLE_Print("[GAME: " + m_GameName + "] [Local]: " + message);

		CGamePlayer* Player = GetPlayerFromPID(fromPID);

		if (!m_GameLoading && !m_GameLoaded) {
			if (message.size() > 254)
				message = message.substr(0, 254);

			SendAll(m_Protocol->SEND_W3GS_CHAT_FROM_HOST(fromPID, GetPIDs(), 16, BYTEARRAY(), message));

			if (Player)
				m_LobbyLog->AddMessage(Player->GetName(), message);
			else
				m_LobbyLog->AddMessage(message);
		}
		else {
			if (message.size() > 127)
				message = message.substr(0, 127);

			SendAll(m_Protocol->SEND_W3GS_CHAT_FROM_HOST(fromPID, GetPIDs(), 32, UTIL_CreateByteArray((uint32_t)0, false), message));

			if (m_Replay)
				m_Replay->AddChatMessage(fromPID, 32, 0, message);

			if (Player)
				m_GameLog->AddMessage(0, Player->GetName(), message);
			else
				m_GameLog->AddMessage(message);
		}
	}
}

void CBaseGame::SendAllChat(string message)
{
	// SendAllChat( GetHostPID( ), message );

	unsigned char fromPID = GetHostPID();

	// send a public message to all players - it'll be marked [All] in Warcraft 3

	if (GetNumHumanPlayers() > 0) {
		CONSOLE_Print("[GAME: " + m_GameName + "] [Local]: " + message);

		if (!m_GameLoading && !m_GameLoaded) {
			if (message.size() > 254)
				message = message.substr(0, 254);

			SendAll(m_Protocol->SEND_W3GS_CHAT_FROM_HOST(fromPID, GetPIDs(), 16, BYTEARRAY(), message));

			m_LobbyLog->AddMessage(message);
		}
		else {
			if (message.size() > 127)
				message = message.substr(0, 127);

			SendAll(m_Protocol->SEND_W3GS_CHAT_FROM_HOST(fromPID, GetPIDs(), 32, UTIL_CreateByteArray((uint32_t)0, false), message));

			if (m_Replay)
				m_Replay->AddChatMessage(fromPID, 32, 0, message);

			m_GameLog->AddMessage(message);
		}
	}
}

void CBaseGame::SendAllAutobanON()
{
	if (m_GHost->m_ReplaceAutobanWithPSRPenalty)
		SendAllChat("PSR penalty is ON, if you leave you will lose multiples of PSR.");
	else
		SendAllChat("Autoban is ON, if you leave you will get autobanned.");
}

void CBaseGame::SendAllAutobanOFF()
{
	if (m_GHost->m_ReplaceAutobanWithPSRPenalty)
		SendAllChat("PSR penalty is OFF, you can now leave or continue playing for fun.");
	else
		SendAllChat("Autoban is OFF, you can now leave or continue playing for fun.");
}

void CBaseGame::SendLocalAdminChat(string message)
{
	if (!m_LocalAdminMessages)
		return;

	// send a message to LAN/local players who are admins
	// at the time of this writing it is only possible for the game owner to meet this criteria because being an admin requires spoof checking
	// this is mainly used for relaying battle.net whispers, chat messages, and emotes to these players

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if ((*i)->GetSpoofed() && IsOwner((*i)->GetName()) && (UTIL_IsLanIP((*i)->GetExternalIP()) || UTIL_IsLocalIP((*i)->GetExternalIP(), m_GHost->m_LocalAddresses))) {
			if (m_VirtualHostPID != 255)
				SendChat(m_VirtualHostPID, *i, message);
			else {
				// make the chat message originate from the recipient since it's not going to be logged to the replay

				SendChat((*i)->GetPID(), *i, message);
			}
		}
	}
}

void CBaseGame::SendAllSlotInfo()
{
	if (!m_GameLoading && !m_GameLoaded) {
		SendAll(m_Protocol->SEND_W3GS_SLOTINFO(m_Slots, m_RandomSeed, m_Map->GetMapLayoutStyle(), m_Map->GetMapNumPlayers()));
		m_SlotInfoChanged = false;
	}
}

void CBaseGame::SendVirtualHostPlayerInfo(CGamePlayer* player)
{
	if (m_VirtualHostPID == 255)
		return;

	BYTEARRAY IP;
	IP.push_back(0);
	IP.push_back(0);
	IP.push_back(0);
	IP.push_back(0);
	Send(player, m_Protocol->SEND_W3GS_PLAYERINFO(m_VirtualHostPID, m_VirtualHostName, IP, IP));
}

void CBaseGame::SendFakePlayerInfo(CGamePlayer* player)
{
	if (m_FakePlayerPID == 255)
		return;

	BYTEARRAY IP;
	IP.push_back(0);
	IP.push_back(0);
	IP.push_back(0);
	IP.push_back(0);
	Send(player, m_Protocol->SEND_W3GS_PLAYERINFO(m_FakePlayerPID, "FakePlayer", IP, IP));
}

void CBaseGame::SendAllActions()
{
	bool UsingGProxy = false;

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if ((*i)->GetGProxy())
			UsingGProxy = true;
	}

	m_GameTicks += m_Latency;

	if (UsingGProxy) {
		// we must send empty actions to non-GProxy++ players
		// GProxy++ will insert these itself so we don't need to send them to GProxy++ players
		// empty actions are used to extend the time a player can use when reconnecting

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			if (!(*i)->GetGProxy()) {
				for (unsigned char j = 0; j < m_GProxyEmptyActions; ++j)
					Send(*i, m_Protocol->SEND_W3GS_INCOMING_ACTION(queue<CIncomingAction*>(), 0));
			}
		}

		if (m_Replay) {
			for (unsigned char i = 0; i < m_GProxyEmptyActions; ++i)
				m_Replay->AddTimeSlot(0, queue<CIncomingAction*>());
		}
	}

	// Warcraft III doesn't seem to respond to empty actions

	/* if( UsingGProxy )
		m_SyncCounter += m_GProxyEmptyActions; */

	++m_SyncCounter;

	// we aren't allowed to send more than 1460 bytes in a single packet but it's possible we might have more than that many bytes waiting in the queue

	if (!m_Actions.empty()) {
		// we use a "sub actions queue" which we keep adding actions to until we reach the size limit
		// start by adding one action to the sub actions queue

		queue<CIncomingAction*> SubActions;
		CIncomingAction* Action = m_Actions.front();
		m_Actions.pop();
		SubActions.push(Action);
		uint32_t SubActionsLength = Action->GetLength();

		while (!m_Actions.empty()) {
			Action = m_Actions.front();
			m_Actions.pop();

			// check if adding the next action to the sub actions queue would put us over the limit (1452 because the INCOMING_ACTION and INCOMING_ACTION2 packets use an extra 8 bytes)

			if (SubActionsLength + Action->GetLength() > 1452) {
				// we'd be over the limit if we added the next action to the sub actions queue
				// so send everything already in the queue and then clear it out
				// the W3GS_INCOMING_ACTION2 packet handles the overflow but it must be sent *before* the corresponding W3GS_INCOMING_ACTION packet

				SendAll(m_Protocol->SEND_W3GS_INCOMING_ACTION2(SubActions));

				if (m_Replay)
					m_Replay->AddTimeSlot2(SubActions);

				while (!SubActions.empty()) {
					delete SubActions.front();
					SubActions.pop();
				}

				SubActionsLength = 0;
			}

			SubActions.push(Action);
			SubActionsLength += Action->GetLength();
		}

		SendAll(m_Protocol->SEND_W3GS_INCOMING_ACTION(SubActions, m_Latency));

		if (m_Replay)
			m_Replay->AddTimeSlot(m_Latency, SubActions);

		while (!SubActions.empty()) {
			delete SubActions.front();
			SubActions.pop();
		}
	}
	else {
		SendAll(m_Protocol->SEND_W3GS_INCOMING_ACTION(m_Actions, m_Latency));

		if (m_Replay)
			m_Replay->AddTimeSlot(m_Latency, m_Actions);
	}

	uint32_t ActualSendInterval = GetTicks() - m_LastActionSentTicks;
	uint32_t ExpectedSendInterval = m_Latency - m_LastActionLateBy;
	m_LastActionLateBy = ActualSendInterval - ExpectedSendInterval;

	if (m_LastActionLateBy > m_Latency) {
		// something is going terribly wrong - GHost++ is probably starved of resources
		// print a message because even though this will take more resources it should provide some information to the administrator for future reference
		// other solutions - dynamically modify the latency, request higher priority, terminate other games, ???

		CONSOLE_Print("[GAME: " + m_GameName + "] warning - the latency is " + UTIL_ToString(m_Latency) + "ms but the last update was late by " + UTIL_ToString(m_LastActionLateBy) + "ms");
		m_LastActionLateBy = m_Latency;
	}

	m_LastActionSentTicks = GetTicks();
}

void CBaseGame::SendWelcomeMessage(CGamePlayer* player)
{
	// read from motd.txt if available (thanks to zeeg for this addition)

	ifstream in;
	in.open(m_GHost->m_MOTDFile.c_str());

	if (in.fail()) {
		// default welcome message

		if (m_HCLCommandString.empty())
			SendChat(player, " ");

		SendChat(player, " ");
		SendChat(player, " ");
		SendChat(player, " ");
		SendChat(player, "GHost++                                         http://www.codelain.com/");
		SendChat(player, "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
		SendChat(player, "     Game Name:                 " + m_GameName);

		if (!m_HCLCommandString.empty())
			SendChat(player, "     HCL Command String:  " + m_HCLCommandString);
	}
	else {
		// custom welcome message
		// don't print more than 8 lines

		uint32_t Count = 0;
		string Line;

		while (!in.eof() && Count < 8) {
			getline(in, Line);

			if (Line.empty()) {
				if (!in.eof())
					SendChat(player, " ");
			}
			else {
				const size_t pos = Line.find("{{region}}");
				if (pos != std::string::npos) {
					Line.replace(pos, sizeof("{{region}}") - 1, m_GHost->m_region);
				}

				SendChat(player, Line);
			}

			++Count;
		}

		in.close();
	}

	for (vector<CBNET*>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i) {
		if (player->GetJoinedRealm() == (*i)->GetServer()) {
			SendChat(player, "Spoof check by whispering bot with \"sc\" [ /w " + (*i)->GetBnetUserName() + " sc ]");
			break;
		}
	}
}

void CBaseGame::SendEndMessage()
{
	// read from gameover.txt if available

	ifstream in;
	in.open(m_GHost->m_GameOverFile.c_str());

	if (!in.fail()) {
		// don't print more than 8 lines

		uint32_t Count = 0;
		string Line;

		while (!in.eof() && Count < 8) {
			getline(in, Line);

			if (Line.empty()) {
				if (!in.eof())
					SendAllChat(" ");
			}
			else
				SendAllChat(Line);

			++Count;
		}

		in.close();
	}
}

void CBaseGame::EventPlayerDeleted(CGamePlayer* player)
{
	if (m_GameLoading || m_GameLoaded) {
		m_GHost->m_Manager->SendGamePlayerLeftGame(m_GameID, player->GetName());

		// We log ingame leave messages later in this function
	}
	else {
		m_GHost->m_Manager->SendGamePlayerLeftLobby(m_GameID, player->GetName());
		m_LobbyLog->AddMessage(player->GetName() + " has left the game.");
	}

	CONSOLE_Print("[GAME: " + m_GameName + "] deleting player [" + player->GetName() + "]: " + player->GetLeftReason());

	// remove any queued spoofcheck messages for this player

	if (player->GetWhoisSent() && !player->GetJoinedRealm().empty() && player->GetSpoofedRealm().empty()) {
		for (vector<CBNET*>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i) {
			if ((*i)->GetServer() == player->GetJoinedRealm()) {
				// hackhack: there must be a better way to do this

				if ((*i)->GetPasswordHashType() == "pvpgn")
					(*i)->UnqueueChatCommand("/whereis " + player->GetName());
				else
					(*i)->UnqueueChatCommand("/whois " + player->GetName());

				(*i)->UnqueueChatCommand("/w " + player->GetName() + " " + m_GHost->m_Language->SpoofCheckByReplying());
			}
		}
	}

	m_LastPlayerLeaveTicks = GetTicks();

	// in some cases we're forced to send the left message early so don't send it again

	if (player->GetLeftMessageSent())
		return;

	// DIV1 DotA games have additional info to write with this message

	if (m_GameLoaded && m_GameType != MASL_PROTOCOL::DB_DIV1_DOTA_GAME) {
		SendAllChat(player->GetName() + " " + player->GetLeftReason() + ".");
	}

	if (player->GetLagging())
		SendAll(m_Protocol->SEND_W3GS_STOP_LAG(player));

	// autosave

	if (m_GameLoaded && player->GetLeftCode() == PLAYERLEAVE_DISCONNECT && m_AutoSave) {
		string SaveGameName = UTIL_FileSafeName("playdota.eu AutoSave " + m_GameName + " (" + player->GetName() + ").w3z");
		CONSOLE_Print("[GAME: " + m_GameName + "] auto saving [" + SaveGameName + "] before player drop, shortened send interval = " + UTIL_ToString(GetTicks() - m_LastActionSentTicks));
		BYTEARRAY CRC;
		BYTEARRAY Action;
		Action.push_back(6);
		UTIL_AppendByteArray(Action, SaveGameName);
		m_Actions.push(new CIncomingAction(player->GetPID(), CRC, Action));

		// todotodo: with the new latency system there needs to be a way to send a 0-time action

		SendAllActions();
	}

	if (m_GameLoading && m_LoadInGame) {
		// we must buffer player leave messages when using "load in game" to prevent desyncs
		// this ensures the player leave messages are correctly interleaved with the empty updates sent to each player

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			if ((*i)->GetFinishedLoading()) {
				if (!player->GetFinishedLoading())
					Send(*i, m_Protocol->SEND_W3GS_STOP_LAG(player));

				Send(*i, m_Protocol->SEND_W3GS_PLAYERLEAVE_OTHERS(player->GetPID(), player->GetLeftCode()));
			}
			else
				(*i)->AddLoadInGameData(m_Protocol->SEND_W3GS_PLAYERLEAVE_OTHERS(player->GetPID(), player->GetLeftCode()));
		}
	}
	else {
		// tell everyone about the player leaving

		SendAll(m_Protocol->SEND_W3GS_PLAYERLEAVE_OTHERS(player->GetPID(), player->GetLeftCode()));
	}

	// set the replay's host PID and name to the last player to leave the game
	// this will get overwritten as each player leaves the game so it will eventually be set to the last player

	if (m_Replay && (m_GameLoading || m_GameLoaded)) {
		m_Replay->SetHostPID(player->GetPID());
		m_Replay->SetHostName(player->GetName());

		// add leave message to replay

		if (m_GameLoading && !m_LoadInGame)
			m_Replay->AddLeaveGameDuringLoading(1, player->GetPID(), player->GetLeftCode());
		else
			m_Replay->AddLeaveGame(1, player->GetPID(), player->GetLeftCode());
	}

	// abort the countdown if there was one in progress

	if (m_CountDownStarted && !m_GameLoading && !m_GameLoaded) {
		SendAllChat(m_GHost->m_Language->CountDownAborted());
		m_CountDownStarted = false;
	}

	// abort the votekick

	/*if( !m_KickVotePlayer.empty( ) )
		SendAllChat( m_GHost->m_Language->VoteKickCancelled( m_KickVotePlayer ) );*/

	m_KickVotePlayer.clear();
	m_StartedKickVoteTime = 0;

	// abort vote remake

	/*if( m_VoteRemakeStarted )
		SendAllChat( "[" + player->GetName( ) + "] left, vote rmk canceled." );*/

	m_VoteRemakeStarted = false;
	m_VoteRemakeStartedTime = 0;

	// record everything we need to know about the player for storing in the database later
	// since we haven't stored the game yet (it's not over yet!) we can't link the gameplayer to the game
	// see the destructor for where these CDBGamePlayers are stored in the database
	// we could have inserted an incomplete record on creation and updated it later but this makes for a cleaner interface

	if (m_GameLoading || m_GameLoaded) {
		// todotodo: since we store players that crash during loading it's possible that the stats classes could have no information on them
		// that could result in a DBGamePlayer without a corresponding DBDotAPlayer - just be aware of the possibility

		unsigned char SID = GetSIDFromPID(player->GetPID());
		unsigned char Team = 255;
		unsigned char Colour = 255;

		if (SID < m_Slots.size()) {
			Team = m_Slots[SID].GetTeam();
			Colour = m_Slots[SID].GetColour();
		}

		CDBGamePlayer* DBGamePlayer = new CDBGamePlayer(player->GetName(), player->GetExternalIPString(), player->GetSpoofed() ? 1 : 0, player->GetSpoofedRealm(), player->GetReserved() ? 1 : 0, player->GetFinishedLoading() ? player->GetFinishedLoadingTicks() - m_StartedLoadingTicks : 0, m_GameTicks / 1000, player->GetLeftReason(), Team, Colour, player->GetGProxy());
		m_DBGamePlayers.push_back(DBGamePlayer);
	}
}

void CBaseGame::EventPlayerDisconnectTimedOut(CGamePlayer* player)
{
	if (player->GetGProxy() && m_GameLoaded) {
		if (!player->GetGProxyDisconnectNoticeSent()) {
			SendAllChat(player->GetName() + " " + m_GHost->m_Language->HasLostConnectionTimedOutGProxy() + ".");
			player->SetGProxyDisconnectNoticeSent(true);
		}

		if (GetTime() - player->GetLastGProxyWaitNoticeSentTime() >= 20) {
			uint32_t TimeRemaining = (m_GProxyEmptyActions + 1) * 60 - (GetTime() - m_StartedLaggingTime);

			if (TimeRemaining > ((uint32_t)m_GProxyEmptyActions + 1) * 60)
				TimeRemaining = (m_GProxyEmptyActions + 1) * 60;

			SendAllChat(player->GetPID(), m_GHost->m_Language->WaitForReconnectSecondsRemain(UTIL_ToString(TimeRemaining)));
			player->SetLastGProxyWaitNoticeSentTime(GetTime());
		}

		return;
	}

	// not only do we not do any timeouts if the game is lagging, we allow for an additional grace period of 10 seconds
	// this is because Warcraft 3 stops sending packets during the lag screen
	// so when the lag screen finishes we would immediately disconnect everyone if we didn't give them some extra time

	if (GetTime() - m_LastLagScreenTime >= 10) {
		player->SetDeleteMe(true);
		player->SetLeftReason(m_GHost->m_Language->HasLostConnectionTimedOut());
		player->SetLeftCode(PLAYERLEAVE_DISCONNECT);

		if (!m_GameLoading && !m_GameLoaded)
			OpenSlot(GetSIDFromPID(player->GetPID()), false);
	}
}

void CBaseGame::EventPlayerDisconnectPlayerError(CGamePlayer* player)
{
	// at the time of this comment there's only one player error and that's when we receive a bad packet from the player
	// since TCP has checks and balances for data corruption the chances of this are pretty slim

	player->SetDeleteMe(true);
	player->SetLeftReason(m_GHost->m_Language->HasLostConnectionPlayerError(player->GetErrorString()));
	player->SetLeftCode(PLAYERLEAVE_DISCONNECT);

	if (!m_GameLoading && !m_GameLoaded)
		OpenSlot(GetSIDFromPID(player->GetPID()), false);
}

void CBaseGame::EventPlayerDisconnectSocketError(CGamePlayer* player)
{
	if (player->GetGProxy() && m_GameLoaded) {
		if (!player->GetGProxyDisconnectNoticeSent()) {
			SendAllChat(player->GetName() + " " + m_GHost->m_Language->HasLostConnectionSocketErrorGProxy(player->GetSocket()->GetErrorString()) + ".");
			player->SetGProxyDisconnectNoticeSent(true);
		}

		if (GetTime() - player->GetLastGProxyWaitNoticeSentTime() >= 20) {
			uint32_t TimeRemaining = (m_GProxyEmptyActions + 1) * 60 - (GetTime() - m_StartedLaggingTime);

			if (TimeRemaining > ((uint32_t)m_GProxyEmptyActions + 1) * 60)
				TimeRemaining = (m_GProxyEmptyActions + 1) * 60;

			SendAllChat(player->GetPID(), m_GHost->m_Language->WaitForReconnectSecondsRemain(UTIL_ToString(TimeRemaining)));
			player->SetLastGProxyWaitNoticeSentTime(GetTime());
		}

		return;
	}

	player->SetDeleteMe(true);
	player->SetLeftReason(m_GHost->m_Language->HasLostConnectionSocketError(player->GetSocket()->GetErrorString()));
	player->SetLeftCode(PLAYERLEAVE_DISCONNECT);

	if (!m_GameLoading && !m_GameLoaded)
		OpenSlot(GetSIDFromPID(player->GetPID()), false);
}

void CBaseGame::EventPlayerDisconnectConnectionClosed(CGamePlayer* player)
{
	if (player->GetGProxy() && m_GameLoaded) {
		if (!player->GetGProxyDisconnectNoticeSent()) {
			SendAllChat(player->GetName() + " " + m_GHost->m_Language->HasLostConnectionClosedByRemoteHostGProxy() + ".");
			player->SetGProxyDisconnectNoticeSent(true);
		}

		if (GetTime() - player->GetLastGProxyWaitNoticeSentTime() >= 20) {
			uint32_t TimeRemaining = (m_GProxyEmptyActions + 1) * 60 - (GetTime() - m_StartedLaggingTime);

			if (TimeRemaining > ((uint32_t)m_GProxyEmptyActions + 1) * 60)
				TimeRemaining = (m_GProxyEmptyActions + 1) * 60;

			SendAllChat(player->GetPID(), m_GHost->m_Language->WaitForReconnectSecondsRemain(UTIL_ToString(TimeRemaining)));
			player->SetLastGProxyWaitNoticeSentTime(GetTime());
		}

		return;
	}

	player->SetDeleteMe(true);
	player->SetLeftReason(m_GHost->m_Language->HasLostConnectionClosedByRemoteHost());
	player->SetLeftCode(PLAYERLEAVE_DISCONNECT);

	if (!m_GameLoading && !m_GameLoaded)
		OpenSlot(GetSIDFromPID(player->GetPID()), false);
}

void CBaseGame::EventPlayerJoined(CPotentialPlayer* potential, CIncomingJoinPlayer* joinPlayer)
{
	// check if the new player's name is empty or too long

	if (joinPlayer->GetName().empty() || joinPlayer->GetName().size() > 15) {
		CONSOLE_Print("[GAME: " + m_GameName + "] player [" + joinPlayer->GetName() + "|" + potential->GetExternalIPString() + "] is trying to join the game with an invalid name of length " + UTIL_ToString(joinPlayer->GetName().size()));
		potential->Send(m_Protocol->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
		potential->SetDeleteMe(true);
		return;
	}

	// check if the new player's name is the same as the virtual host name

	if (joinPlayer->GetName() == m_VirtualHostName) {
		CONSOLE_Print("[GAME: " + m_GameName + "] player [" + joinPlayer->GetName() + "|" + potential->GetExternalIPString() + "] is trying to join the game with the virtual host name");
		potential->Send(m_Protocol->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
		potential->SetDeleteMe(true);
		return;
	}

	// check if the new player's name is already taken

	if (GetPlayerFromName(joinPlayer->GetName(), false)) {
		CONSOLE_Print("[GAME: " + m_GameName + "] player [" + joinPlayer->GetName() + "|" + potential->GetExternalIPString() + "] is trying to join the game but that name is already taken");
		// SendAllChat( m_GHost->m_Language->TryingToJoinTheGameButTaken( joinPlayer->GetName( ) ) );
		potential->Send(m_Protocol->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
		potential->SetDeleteMe(true);
		return;
	}

	// identify their joined realm
	// this is only possible because when we send a game refresh via LAN or battle.net we encode an ID value in the 4 most significant bits of the host counter
	// the client sends the host counter when it joins so we can extract the ID value here
	// note: this is not a replacement for spoof checking since it doesn't verify the player's name and it can be spoofed anyway

	uint32_t HostCounterID = joinPlayer->GetHostCounter() >> 28;
	string JoinedRealm;

	// we use an ID value of 0 to denote joining via LAN

	if (HostCounterID == 0) {
		// the player is pretending to join via LAN, which they might or might not be (i.e. it could be spoofed)
		// however, we've been broadcasting a random entry key to the LAN
		// if the player is really on the LAN they'll know the entry key, otherwise they won't
		// or they're very lucky since it's a 32 bit number

		if (joinPlayer->GetEntryKey() != m_EntryKey) {
			CONSOLE_Print(">" + m_GHost->m_allowLanJoinAsRealm + "<");
			if (m_GHost->m_allowLanJoinAsRealm != "") {
				JoinedRealm = m_GHost->m_allowLanJoinAsRealm;
				potential->m_isFakeLANPlayer = true;
				CONSOLE_Print("[GAME: " + m_GameName + "] !!!TESTING!!! player [" + joinPlayer->GetName() + "|" + potential->GetExternalIPString() + "] used an incorrect LAN key but we allow it and assign the default configured server as realm.");
			}
			else {
				CONSOLE_Print("[GAME: " + m_GameName + "] player [" + joinPlayer->GetName() + "|" + potential->GetExternalIPString() + "] is trying to join the game over LAN but used an incorrect entry key");
				potential->Send(m_Protocol->SEND_W3GS_REJECTJOIN(REJECTJOIN_WRONGPASSWORD));
				potential->SetDeleteMe(true);
				return;
			}
		}
	}
	else {
		for (vector<CBNET*>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i) {
			if ((*i)->GetHostCounterID() == HostCounterID)
				JoinedRealm = (*i)->GetServer();
		}
	}

	if (JoinedRealm.empty()) {
		CONSOLE_Print("[GAME: " + m_GameName + "] player [" + joinPlayer->GetName() + "|" + potential->GetExternalIPString() + "] is trying to join the game from an unknown realm with HostCounterID=" + UTIL_ToString(HostCounterID));
		potential->Send(m_Protocol->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
		potential->SetDeleteMe(true);
		return;
	}

	// REAL CODE
	// request player data from the manager
	m_GHost->m_Manager->SendGetPlayerInfo(JoinedRealm, UTIL_ByteArrayToUInt32(potential->GetExternalIP(), true), joinPlayer->GetName());
	m_PlayerChecks.push_back(new CCallablePlayerCheck(joinPlayer->GetName(), JoinedRealm));

	return;
}

void CBaseGame::EventPlayerJoinedWithInfo(CPotentialPlayer* potential, CIncomingJoinPlayer* joinPlayer, CDBPlayer* playerInfo)
{
	// check if the new player's name is the same as the virtual host name

	if (joinPlayer->GetName() == m_VirtualHostName) {
		CONSOLE_Print("[GAME: " + m_GameName + "] player [" + joinPlayer->GetName() + "|" + potential->GetExternalIPString() + "] is trying to join the game with the virtual host name");
		potential->Send(m_Protocol->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
		potential->SetDeleteMe(true);
		return;
	}

	// check if the new player's name is already taken

	if (GetPlayerFromName(joinPlayer->GetName(), false)) {
		CONSOLE_Print("[GAME: " + m_GameName + "] player [" + joinPlayer->GetName() + "|" + potential->GetExternalIPString() + "] is trying to join the game but that name is already taken");
		// SendAllChat( m_GHost->m_Language->TryingToJoinTheGameButTaken( joinPlayer->GetName( ) ) );
		potential->Send(m_Protocol->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
		potential->SetDeleteMe(true);
		return;
	}

	// check if the new player's name is banned but only if bot_banmethod is not 0
	// this is because if bot_banmethod is 0 and we announce the ban here it's possible for the player to be rejected later because the game is full
	// this would allow the player to spam the chat by attempting to join the game multiple times in a row

	/*if( m_GHost->m_BanMethod != 0 )
	{
		for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
		{
			if( (*i)->GetServer( ) == JoinedRealm )
			{
				CDBBan *Ban = (*i)->IsBannedName( joinPlayer->GetName( ) );

				if( Ban )
				{
					if( m_GHost->m_BanMethod == 1 || m_GHost->m_BanMethod == 3 )
					{
						CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is trying to join the game but is banned by name" );

						if( m_IgnoredNames.find( joinPlayer->GetName( ) ) == m_IgnoredNames.end( ) )
						{
							SendAllChat( m_GHost->m_Language->TryingToJoinTheGameButBannedByName( joinPlayer->GetName( ) ) );
							SendAllChat( m_GHost->m_Language->UserWasBannedOnByBecause( Ban->GetServer( ), Ban->GetName( ), Ban->GetDate( ), Ban->GetAdmin( ), Ban->GetReason( ) ) );
							m_IgnoredNames.insert( joinPlayer->GetName( ) );
						}

						// let banned players "join" the game with an arbitrary PID then immediately close the connection
						// this causes them to be kicked back to the chat channel on battle.net

						vector<CGameSlot> Slots = m_Map->GetSlots( );
						potential->Send( m_Protocol->SEND_W3GS_SLOTINFOJOIN( 1, potential->GetSocket( )->GetPort( ), potential->GetExternalIP( ), Slots, 0, m_Map->GetMapLayoutStyle( ), m_Map->GetMapNumPlayers( ) ) );
						potential->SetDeleteMe( true );
						return;
					}

					break;
				}
			}

			CDBBan *Ban = (*i)->IsBannedIP( potential->GetExternalIPString( ) );

			if( Ban )
			{
				if( m_GHost->m_BanMethod == 2 || m_GHost->m_BanMethod == 3 )
				{
					CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is trying to join the game but is banned by IP address" );

					if( m_IgnoredNames.find( joinPlayer->GetName( ) ) == m_IgnoredNames.end( ) )
					{
						SendAllChat( m_GHost->m_Language->TryingToJoinTheGameButBannedByIP( joinPlayer->GetName( ), potential->GetExternalIPString( ), Ban->GetName( ) ) );
						SendAllChat( m_GHost->m_Language->UserWasBannedOnByBecause( Ban->GetServer( ), Ban->GetName( ), Ban->GetDate( ), Ban->GetAdmin( ), Ban->GetReason( ) ) );
						m_IgnoredNames.insert( joinPlayer->GetName( ) );
					}

					// let banned players "join" the game with an arbitrary PID then immediately close the connection
					// this causes them to be kicked back to the chat channel on battle.net

					vector<CGameSlot> Slots = m_Map->GetSlots( );
					potential->Send( m_Protocol->SEND_W3GS_SLOTINFOJOIN( 1, potential->GetSocket( )->GetPort( ), potential->GetExternalIP( ), Slots, 0, m_Map->GetMapLayoutStyle( ), m_Map->GetMapNumPlayers( ) ) );
					potential->SetDeleteMe( true );
					return;
				}

				break;
			}
		}
	}*/

	bool AnyAdminCheck = playerInfo->GetIsAdmin();

	if (playerInfo->GetIsBanned() && !AnyAdminCheck) {
		CONSOLE_Print("[GAME: " + m_GameName + "] player [" + joinPlayer->GetName() + "|" + potential->GetExternalIPString() + "] is trying to join the game but is banned by name");

		if (m_IgnoredNames.find(joinPlayer->GetName()) == m_IgnoredNames.end()) {
			// SendAllChat( m_GHost->m_Language->TryingToJoinTheGameButBannedByName( joinPlayer->GetName( ) ) );
			// SendAllChat( m_GHost->m_Language->UserWasBannedOnByBecause( Ban->GetServer( ), Ban->GetName( ), Ban->GetDate( ), Ban->GetAdmin( ), Ban->GetReason( ) ) );
			m_IgnoredNames.insert(joinPlayer->GetName());
		}

		// let banned players "join" the game with an arbitrary PID then immediately close the connection
		// this causes them to be kicked back to the chat channel on battle.net

		vector<CGameSlot> Slots = m_Map->GetSlots();
		potential->Send(m_Protocol->SEND_W3GS_SLOTINFOJOIN(1, potential->GetSocket()->GetPort(), potential->GetExternalIP(), Slots, 0, m_Map->GetMapLayoutStyle(), m_Map->GetMapNumPlayers()));
		potential->SetDeleteMe(true);
		return;
	}

	/*if( m_MatchMaking && m_AutoStartPlayers != 0 && !m_Map->GetMapMatchMakingCategory( ).empty( ) && m_Map->GetMapOptions( ) & MAPOPT_FIXEDPLAYERSETTINGS )
	{
		// matchmaking is enabled
		// start a database query to determine the player's score
		// when the query is complete we will call EventPlayerJoinedWithScore

		m_ScoreChecks.push_back( m_GHost->m_DB->ThreadedScoreCheck( m_Map->GetMapMatchMakingCategory( ), joinPlayer->GetName( ), JoinedRealm ) );
		return;
	}*/

	// check if the player is an admin or root admin on any connected realm for determining reserved status
	// we can't just use the spoof checked realm like in EventPlayerBotCommand because the player hasn't spoof checked yet

	/*bool AnyAdminCheck = false;

	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
	{
		if( (*i)->IsAdmin( joinPlayer->GetName( ) ) || (*i)->IsRootAdmin( joinPlayer->GetName( ) ) )
		{
			AnyAdminCheck = true;
			break;
		}
	}*/

	bool Reserved = IsReserved(joinPlayer->GetName()) || (m_GHost->m_ReserveAdmins && AnyAdminCheck) || IsOwner(joinPlayer->GetName());

	// try to find a slot

	unsigned char SID = 255;
	unsigned char EnforcePID = 255;
	unsigned char EnforceSID = 0;
	CGameSlot EnforceSlot(255, 0, 0, 0, 0, 0, 0);

	if (m_SaveGame) {
		// in a saved game we enforce the player layout and the slot layout
		// unfortunately we don't know how to extract the player layout from the saved game so we use the data from a replay instead
		// the !enforcesg command defines the player layout by parsing a replay

		for (vector<PIDPlayer>::iterator i = m_EnforcePlayers.begin(); i != m_EnforcePlayers.end(); ++i) {
			if ((*i).second == joinPlayer->GetName())
				EnforcePID = (*i).first;
		}

		for (vector<CGameSlot>::iterator i = m_EnforceSlots.begin(); i != m_EnforceSlots.end(); ++i) {
			if ((*i).GetPID() == EnforcePID) {
				EnforceSlot = *i;
				break;
			}

			EnforceSID++;
		}

		if (EnforcePID == 255 || EnforceSlot.GetPID() == 255 || EnforceSID >= m_Slots.size()) {
			CONSOLE_Print("[GAME: " + m_GameName + "] player [" + joinPlayer->GetName() + "|" + potential->GetExternalIPString() + "] is trying to join the game but isn't in the enforced list");
			potential->Send(m_Protocol->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
			potential->SetDeleteMe(true);
			return;
		}

		SID = EnforceSID;
	}
	else {
		// try to find an empty slot

		SID = GetEmptySlot(false);

		if (SID == 255 && Reserved) {
			// a reserved player is trying to join the game but it's full, try to find a reserved slot

			SID = GetEmptySlot(true);

			if (SID != 255) {
				CGamePlayer* KickedPlayer = GetPlayerFromSID(SID);

				if (KickedPlayer) {
					KickedPlayer->SetDeleteMe(true);
					KickedPlayer->SetLeftReason(m_GHost->m_Language->WasKickedForReservedPlayer(joinPlayer->GetName()));
					KickedPlayer->SetLeftCode(PLAYERLEAVE_LOBBY);

					// send a playerleave message immediately since it won't normally get sent until the player is deleted which is after we send a playerjoin message
					// we don't need to call OpenSlot here because we're about to overwrite the slot data anyway

					SendAll(m_Protocol->SEND_W3GS_PLAYERLEAVE_OTHERS(KickedPlayer->GetPID(), KickedPlayer->GetLeftCode()));
					KickedPlayer->SetLeftMessageSent(true);
				}
			}
		}

		if (SID == 255 && IsOwner(joinPlayer->GetName())) {
			// the owner player is trying to join the game but it's full and we couldn't even find a reserved slot, kick the player in the lowest numbered slot
			// updated this to try to find a player slot so that we don't end up kicking a computer

			SID = 0;

			for (unsigned char i = 0; i < m_Slots.size(); ++i) {
				if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OCCUPIED && m_Slots[i].GetComputer() == 0) {
					SID = i;
					break;
				}
			}

			CGamePlayer* KickedPlayer = GetPlayerFromSID(SID);

			if (KickedPlayer) {
				KickedPlayer->SetDeleteMe(true);
				KickedPlayer->SetLeftReason(m_GHost->m_Language->WasKickedForOwnerPlayer(joinPlayer->GetName()));
				KickedPlayer->SetLeftCode(PLAYERLEAVE_LOBBY);

				// send a playerleave message immediately since it won't normally get sent until the player is deleted which is after we send a playerjoin message
				// we don't need to call OpenSlot here because we're about to overwrite the slot data anyway

				SendAll(m_Protocol->SEND_W3GS_PLAYERLEAVE_OTHERS(KickedPlayer->GetPID(), KickedPlayer->GetLeftCode()));
				KickedPlayer->SetLeftMessageSent(true);
			}
		}
	}

	if (SID >= m_Slots.size()) {
		potential->Send(m_Protocol->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
		potential->SetDeleteMe(true);
		return;
	}

	// check if the new player's name is banned but only if bot_banmethod is 0
	// this is because if bot_banmethod is 0 we need to wait to announce the ban until now because they could have been rejected because the game was full
	// this would have allowed the player to spam the chat by attempting to join the game multiple times in a row

	/*if( m_GHost->m_BanMethod == 0 )
	{
		for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); ++i )
		{
			if( (*i)->GetServer( ) == JoinedRealm )
			{
				CDBBan *Ban = (*i)->IsBannedName( joinPlayer->GetName( ) );

				if( Ban )
				{
					CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is using a banned name" );
					SendAllChat( m_GHost->m_Language->HasBannedName( joinPlayer->GetName( ) ) );
					SendAllChat( m_GHost->m_Language->UserWasBannedOnByBecause( Ban->GetServer( ), Ban->GetName( ), Ban->GetDate( ), Ban->GetAdmin( ), Ban->GetReason( ) ) );
					break;
				}
			}

			CDBBan *Ban = (*i)->IsBannedIP( potential->GetExternalIPString( ) );

			if( Ban )
			{
				CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + joinPlayer->GetName( ) + "|" + potential->GetExternalIPString( ) + "] is using a banned IP address" );
				SendAllChat( m_GHost->m_Language->HasBannedIP( joinPlayer->GetName( ), potential->GetExternalIPString( ), Ban->GetName( ) ) );
				SendAllChat( m_GHost->m_Language->UserWasBannedOnByBecause( Ban->GetServer( ), Ban->GetName( ), Ban->GetDate( ), Ban->GetAdmin( ), Ban->GetReason( ) ) );
				break;
			}
		}
	}*/

	// we have a slot for the new player
	// make room for them by deleting the virtual host player if we have to

	if (GetNumPlayers() >= 11 || EnforcePID == m_VirtualHostPID)
		DeleteVirtualHost();

	string JoinedRealm = playerInfo->GetServer();

	// turning the CPotentialPlayer into a CGamePlayer is a bit of a pain because we have to be careful not to close the socket
	// this problem is solved by setting the socket to NULL before deletion and handling the NULL case in the destructor
	// we also have to be careful to not modify the m_Potentials vector since we're currently looping through it

	CONSOLE_Print("[GAME: " + m_GameName + "] player [" + joinPlayer->GetName() + "|" + potential->GetExternalIPString() + "] joined the game");
	CGamePlayer* Player = new CGamePlayer(playerInfo, potential, m_SaveGame ? EnforcePID : GetNewPID(), JoinedRealm, joinPlayer->GetName(), joinPlayer->GetInternalIP(), Reserved);

	m_LobbyLog->AddMessage(joinPlayer->GetName() + " has joined the game.");

	// make fake LAN players as spoofed
	if (potential->m_isFakeLANPlayer) {
		Player->SetSpoofed(true);
	}

	Player->SetWhoisShouldBeSent(m_GHost->m_SpoofChecks == 1 || (m_GHost->m_SpoofChecks == 2 && AnyAdminCheck));
	m_Players.push_back(Player);
	potential->SetSocket(NULL);
	potential->SetDeleteMe(true);

	if (m_SaveGame)
		m_Slots[SID] = EnforceSlot;
	else {
		if (m_Map->GetMapOptions() & MAPOPT_CUSTOMFORCES)
			m_Slots[SID] = CGameSlot(Player->GetPID(), 255, SLOTSTATUS_OCCUPIED, 0, m_Slots[SID].GetTeam(), m_Slots[SID].GetColour(), m_Slots[SID].GetRace());
		else {
			if (m_Map->GetMapFlags() & MAPFLAG_RANDOMRACES)
				m_Slots[SID] = CGameSlot(Player->GetPID(), 255, SLOTSTATUS_OCCUPIED, 0, 12, 12, SLOTRACE_RANDOM);
			else
				m_Slots[SID] = CGameSlot(Player->GetPID(), 255, SLOTSTATUS_OCCUPIED, 0, 12, 12, SLOTRACE_RANDOM | SLOTRACE_SELECTABLE);

			// try to pick a team and colour
			// make sure there aren't too many other players already

			unsigned char NumOtherPlayers = 0;

			for (unsigned char i = 0; i < m_Slots.size(); ++i) {
				if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OCCUPIED && m_Slots[i].GetTeam() != 12)
					NumOtherPlayers++;
			}

			if (NumOtherPlayers < m_Map->GetMapNumPlayers()) {
				if (SID < m_Map->GetMapNumPlayers())
					m_Slots[SID].SetTeam(SID);
				else
					m_Slots[SID].SetTeam(0);

				m_Slots[SID].SetColour(GetNewColour());
			}
		}
	}

	// send slot info to the new player
	// the SLOTINFOJOIN packet also tells the client their assigned PID and that the join was successful

	Player->Send(m_Protocol->SEND_W3GS_SLOTINFOJOIN(Player->GetPID(), Player->GetSocket()->GetPort(), Player->GetExternalIP(), m_Slots, m_RandomSeed, m_Map->GetMapLayoutStyle(), m_Map->GetMapNumPlayers()));

	// send virtual host info and fake player info (if present) to the new player

	SendVirtualHostPlayerInfo(Player);
	SendFakePlayerInfo(Player);

	BYTEARRAY BlankIP;
	BlankIP.push_back(0);
	BlankIP.push_back(0);
	BlankIP.push_back(0);
	BlankIP.push_back(0);

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if (!(*i)->GetLeftMessageSent() && *i != Player) {
			// send info about the new player to every other player

			if ((*i)->GetSocket()) {
				if (m_GHost->m_HideIPAddresses)
					(*i)->Send(m_Protocol->SEND_W3GS_PLAYERINFO(Player->GetPID(), Player->GetName(), BlankIP, BlankIP));
				else
					(*i)->Send(m_Protocol->SEND_W3GS_PLAYERINFO(Player->GetPID(), Player->GetName(), Player->GetExternalIP(), Player->GetInternalIP()));
			}

			// send info about every other player to the new player

			if (m_GHost->m_HideIPAddresses)
				Player->Send(m_Protocol->SEND_W3GS_PLAYERINFO((*i)->GetPID(), (*i)->GetName(), BlankIP, BlankIP));
			else
				Player->Send(m_Protocol->SEND_W3GS_PLAYERINFO((*i)->GetPID(), (*i)->GetName(), (*i)->GetExternalIP(), (*i)->GetInternalIP()));
		}
	}

	// send a map check packet to the new player

	Player->Send(m_Protocol->SEND_W3GS_MAPCHECK(m_Map->GetMapPath(), m_Map->GetMapSize(), m_Map->GetMapInfo(), m_Map->GetMapCRC(), m_Map->GetMapSHA1()));

	// send slot info to everyone, so the new player gets this info twice but everyone else still needs to know the new slot layout

	SendAllSlotInfo();

	// send a welcome message

	SendWelcomeMessage(Player);

	// if spoof checks are required and we won't automatically spoof check this player then tell them how to spoof check
	// e.g. if automatic spoof checks are disabled, or if automatic spoof checks are done on admins only and this player isn't an admin

	if (m_GHost->m_RequireSpoofChecks && !Player->GetWhoisShouldBeSent()) {
		for (vector<CBNET*>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i) {
			// note: the following (commented out) line of code will crash because calling GetUniqueName( ) twice will result in two different return values
			// and unfortunately iterators are not valid if compared against different containers
			// this comment shall serve as warning to not make this mistake again since it has now been made twice before in GHost++
			// string( (*i)->GetUniqueName( ).begin( ), (*i)->GetUniqueName( ).end( ) )

			BYTEARRAY UniqueName = (*i)->GetUniqueName();

			if ((*i)->GetServer() == JoinedRealm)
				SendChat(Player, m_GHost->m_Language->SpoofCheckByWhispering(string(UniqueName.begin(), UniqueName.end())));
		}
	}

	// check for multiple IP usage

	if (m_GHost->m_CheckMultipleIPUsage) {
		string Others;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			if (Player != *i && Player->GetExternalIPString() == (*i)->GetExternalIPString()) {
				if (Others.empty())
					Others = (*i)->GetName();
				else
					Others += ", " + (*i)->GetName();
			}
		}

		// if( !Others.empty( ) )
		//	SendAllChat( m_GHost->m_Language->MultipleIPAddressUsageDetected( joinPlayer->GetName( ), Others ) );
	}

	// abort the countdown if there was one in progress

	if (m_CountDownStarted && !m_GameLoading && !m_GameLoaded) {
		SendAllChat(m_GHost->m_Language->CountDownAborted());
		m_CountDownStarted = false;
	}

	// auto lock the game

	/*if( m_GHost->m_AutoLock && !m_Locked && IsOwner( joinPlayer->GetName( ) ) )
	{
		SendAllChat( m_GHost->m_Language->GameLocked( ) );
		m_Locked = true;
	}*/

	EventPlayerEnteredLobby(Player);
	m_GHost->m_Manager->SendGamePlayerJoinedLobby(m_GameID, joinPlayer->GetName());
}

void CBaseGame::EventPlayerEnteredLobby(CGamePlayer* player)
{
	// this event is called from EventPlayerJoinedWithInfo if player joins the lobby succesfully and is NOT kicked
}

void CBaseGame::EventPlayerLeft(CGamePlayer* player, uint32_t reason)
{
	// this function is only called when a player leave packet is received, not when there's a socket error, kick, etc...

	player->SetDeleteMe(true);

	if (reason == PLAYERLEAVE_GPROXY)
		player->SetLeftReason(m_GHost->m_Language->WasUnrecoverablyDroppedFromGProxy());
	else
		player->SetLeftReason(m_GHost->m_Language->HasLeftVoluntarily());

	player->SetLeftCode(PLAYERLEAVE_LOST);

	if (!m_GameLoading && !m_GameLoaded)
		OpenSlot(GetSIDFromPID(player->GetPID()), false);
}

void CBaseGame::EventPlayerLoaded(CGamePlayer* player)
{
	CONSOLE_Print("[GAME: " + m_GameName + "] player [" + player->GetName() + "] finished loading in " + UTIL_ToString((float)(player->GetFinishedLoadingTicks() - m_StartedLoadingTicks) / 1000, 2) + " seconds");

	if (m_LoadInGame) {
		// send any buffered data to the player now
		// see the Update function for more information about why we do this
		// this includes player loaded messages, game updates, and player leave messages

		queue<BYTEARRAY>* LoadInGameData = player->GetLoadInGameData();

		while (!LoadInGameData->empty()) {
			Send(player, LoadInGameData->front());
			LoadInGameData->pop();
		}

		// start the lag screen for the new player

		bool FinishedLoading = true;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			FinishedLoading = (*i)->GetFinishedLoading();

			if (!FinishedLoading)
				break;
		}

		if (!FinishedLoading)
			Send(player, m_Protocol->SEND_W3GS_START_LAG(m_Players, true));

		// remove the new player from previously loaded players' lag screens

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			if (*i != player && (*i)->GetFinishedLoading())
				Send(*i, m_Protocol->SEND_W3GS_STOP_LAG(player));
		}

		// send a chat message to previously loaded players

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			if (*i != player && (*i)->GetFinishedLoading())
				SendChat(*i, m_GHost->m_Language->PlayerFinishedLoading(player->GetName()));
		}

		if (!FinishedLoading)
			SendChat(player, m_GHost->m_Language->PleaseWaitPlayersStillLoading());
	}
	else
		SendAll(m_Protocol->SEND_W3GS_GAMELOADED_OTHERS(player->GetPID()));
}

bool CBaseGame::EventPlayerAction(CGamePlayer* player, CIncomingAction* action)
{
	if ((!m_GameLoaded && !m_GameLoading) || action->GetLength() > 1027) {
		delete action;
		return false;
	}

	m_Actions.push(action);

	// check for players saving the game and notify everyone

	if (!action->GetAction()->empty() && (*action->GetAction())[0] == 6) {
		CONSOLE_Print("[GAME: " + m_GameName + "] player [" + player->GetName() + "] is saving the game");
		SendAllChat(m_GHost->m_Language->PlayerIsSavingTheGame(player->GetName()));
	}
	return true;
}

void CBaseGame::EventPlayerKeepAlive(CGamePlayer* player, uint32_t checkSum)
{
	if (!m_GameLoaded)
		return;

	// check for desyncs
	// however, it's possible that not every player has sent a checksum for this frame yet
	// first we verify that we have enough checksums to work with otherwise we won't know exactly who desynced

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if (!(*i)->GetDeleteMe() && (*i)->GetCheckSums()->empty())
			return;
	}

	// now we check for desyncs since we know that every player has at least one checksum waiting

	bool FoundPlayer = false;
	uint32_t FirstCheckSum = 0;

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if (!(*i)->GetDeleteMe()) {
			FoundPlayer = true;
			FirstCheckSum = (*i)->GetCheckSums()->front();
			break;
		}
	}

	if (!FoundPlayer)
		return;

	bool AddToReplay = true;

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if (!(*i)->GetDeleteMe() && (*i)->GetCheckSums()->front() != FirstCheckSum) {
			CONSOLE_Print("[GAME: " + m_GameName + "] desync detected");
			SendAllChat(m_GHost->m_Language->DesyncDetected());

			// try to figure out who desynced
			// this is complicated by the fact that we don't know what the correct game state is so we let the players vote
			// put the players into bins based on their game state

			map<uint32_t, vector<unsigned char>> Bins;

			for (vector<CGamePlayer*>::iterator j = m_Players.begin(); j != m_Players.end(); ++j) {
				if (!(*j)->GetDeleteMe())
					Bins[(*j)->GetCheckSums()->front()].push_back((*j)->GetPID());
			}

			uint32_t StateNumber = 1;
			map<uint32_t, vector<unsigned char>>::iterator LargestBin = Bins.begin();
			bool Tied = false;

			for (map<uint32_t, vector<unsigned char>>::iterator j = Bins.begin(); j != Bins.end(); ++j) {
				if ((*j).second.size() > (*LargestBin).second.size()) {
					LargestBin = j;
					Tied = false;
				}
				else if (j != LargestBin && (*j).second.size() == (*LargestBin).second.size())
					Tied = true;

				string Players;

				for (vector<unsigned char>::iterator k = (*j).second.begin(); k != (*j).second.end(); ++k) {
					CGamePlayer* Player = GetPlayerFromPID(*k);

					if (Player) {
						if (Players.empty())
							Players = Player->GetName();
						else
							Players += ", " + Player->GetName();
					}
				}

				SendAllChat(m_GHost->m_Language->PlayersInGameState(UTIL_ToString(StateNumber), Players));
				++StateNumber;
			}

			FirstCheckSum = (*LargestBin).first;

			if (Tied) {
				// there is a tie, which is unfortunate
				// the most common way for this to happen is with a desync in a 1v1 situation
				// this is not really unsolvable since the game shouldn't continue anyway so we just kick both players
				// in a 2v2 or higher the chance of this happening is very slim
				// however, we still kick every player because it's not fair to pick one or another group
				// todotodo: it would be possible to split the game at this point and create a "new" game for each game state

				CONSOLE_Print("[GAME: " + m_GameName + "] can't kick desynced players because there is a tie, kicking all players instead");
				StopPlayers(m_GHost->m_Language->WasDroppedDesync());
				AddToReplay = false;
			}
			else {
				CONSOLE_Print("[GAME: " + m_GameName + "] kicking desynced players");

				for (map<uint32_t, vector<unsigned char>>::iterator j = Bins.begin(); j != Bins.end(); ++j) {
					// kick players who are NOT in the largest bin
					// examples: suppose there are 10 players
					// the most common case will be 9v1 (e.g. one player desynced and the others were unaffected) and this will kick the single outlier
					// another (very unlikely) possibility is 8v1v1 or 8v2 and this will kick both of the outliers, regardless of whether their game states match

					if ((*j).first != (*LargestBin).first) {
						for (vector<unsigned char>::iterator k = (*j).second.begin(); k != (*j).second.end(); ++k) {
							CGamePlayer* Player = GetPlayerFromPID(*k);

							if (Player) {
								Player->SetDeleteMe(true);
								Player->SetLeftReason(m_GHost->m_Language->WasDroppedDesync());
								Player->SetLeftCode(PLAYERLEAVE_LOST);
							}
						}
					}
				}
			}

			// don't continue looking for desyncs, we already found one!

			break;
		}
	}

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if (!(*i)->GetDeleteMe())
			(*i)->GetCheckSums()->pop();
	}

	// add checksum to replay

	/* if( m_Replay && AddToReplay )
		m_Replay->AddCheckSum( FirstCheckSum ); */
}

void CBaseGame::EventPlayerChatToHost(CGamePlayer* player, CIncomingChatPlayer* chatPlayer)
{
	if (chatPlayer->GetFromPID() == player->GetPID()) {
		if (chatPlayer->GetType() == CIncomingChatPlayer ::CTH_MESSAGE || chatPlayer->GetType() == CIncomingChatPlayer ::CTH_MESSAGEEXTRA) {
			// update the owner last active time, if he hasn't been active for a long time we will unhost the game

			if (!m_GameLoading && !m_GameLoaded && IsOwner(player->GetName()))
				m_LastOwnerActiveTime = GetTime();

			// relay the chat message to other players

			if (player->GetMuted())
				SendChat(player, "No one hears you, you are muted.");

			bool Relay = !player->GetMuted();
			BYTEARRAY ExtraFlags = chatPlayer->GetExtraFlags();

			// calculate timestamp

			string MinString = UTIL_ToString((m_GameTicks / 1000) / 60);
			string SecString = UTIL_ToString((m_GameTicks / 1000) % 60);

			if (MinString.size() == 1)
				MinString.insert(0, "0");

			if (SecString.size() == 1)
				SecString.insert(0, "0");

			bool SpamBot = false;

			if (!player->GetSpoofed()) {
				CONSOLE_Print("[GAME: " + m_GameName + "] player [" + player->GetName() + "] is not spoofchecked, not relaying message");
				Relay = false;

				for (vector<CBNET*>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i) {
					if (player->GetJoinedRealm() == (*i)->GetServer()) {
						SendChat(player, "Spoof check by whispering bot with \"sc\" [ /w " + (*i)->GetBnetUserName() + " sc ]");
						break;
					}
				}
			}

			if (!ExtraFlags.empty()) {
				if (!m_GameLoaded) {
					Relay = false;
					SpamBot = true;
				}
				else if (ExtraFlags[0] == 0) {
					// this is an ingame [All] message, print it to the console

					CONSOLE_Print("[GAME: " + m_GameName + "] (" + MinString + ":" + SecString + ") [All] [" + player->GetName() + "]: " + chatPlayer->GetMessage());

					// don't relay ingame messages targeted for all players if we're currently muting all
					// note that commands will still be processed even when muting all because we only stop relaying the messages, the rest of the function is unaffected

					if (m_MuteAll) {
						Relay = false;
						SendChat(player, "No one hears you, global chat is muted.");
					}
				}
				else if (ExtraFlags[0] == 2) {
					// this is an ingame [Obs/Ref] message, print it to the console

					CONSOLE_Print("[GAME: " + m_GameName + "] (" + MinString + ":" + SecString + ") [Obs/Ref] [" + player->GetName() + "]: " + chatPlayer->GetMessage());
				}

				if (Relay) {
					// add chat message to replay
					// this includes allied chat and private chat from both teams as long as it was relayed

					if (m_Replay)
						m_Replay->AddChatMessage(chatPlayer->GetFromPID(), chatPlayer->GetFlag(), UTIL_ByteArrayToUInt32(chatPlayer->GetExtraFlags(), false), chatPlayer->GetMessage());

					m_GameLog->AddMessage(ExtraFlags[0], player->GetName(), chatPlayer->GetMessage());
				}
			}
			else {
				if (m_GameLoading || m_GameLoaded)
					Relay = false;
				else {
					// this is a lobby message, print it to the console

					CONSOLE_Print("[GAME: " + m_GameName + "] [Lobby] [" + player->GetName() + "]: " + chatPlayer->GetMessage());

					if (m_MuteLobby)
						Relay = false;

					if (Relay)
						m_LobbyLog->AddMessage(player->GetName(), chatPlayer->GetMessage());
				}
			}

			// handle bot commands

			string Message = chatPlayer->GetMessage();

			/*if( Message == "?trigger" )
				SendChat( player, m_GHost->m_Language->CommandTrigger( string( 1, m_GHost->m_CommandTrigger ) ) );
			else if( !Message.empty( ) && Message[0] == m_GHost->m_CommandTrigger )*/

			if (Message == "?trigger")
				SendChat(player, m_GHost->m_Language->CommandTrigger(string(1, m_GameCommandTrigger)));
			else if (!Message.empty() && Message[0] != m_GameCommandTrigger && Message.size() >= 2 && (Message[0] == '!' || Message[0] == '.') && Message[1] != '!' && Message[1] != '.')
				// don't display trigger info for messages like: !!, .., !., .!
				SendChat(player, m_GHost->m_Language->CommandTrigger(string(1, m_GameCommandTrigger)));
			else if (!Message.empty() && Message[0] == m_GameCommandTrigger) {
				// extract the command trigger, the command, and the payload
				// e.g. "!say hello world" -> command: "say", payload: "hello world"

				string Command;
				string Payload;
				string ::size_type PayloadStart = Message.find(" ");

				if (PayloadStart != string ::npos) {
					Command = Message.substr(1, PayloadStart - 1);
					Payload = Message.substr(PayloadStart + 1);
				}
				else
					Command = Message.substr(1);

				transform(Command.begin(), Command.end(), Command.begin(), (int (*)(int))tolower);

				// don't allow EventPlayerBotCommand to veto a previous instruction to set Relay to false
				// so if Relay is already false (e.g. because the player is muted) then it cannot be forced back to true here

				if (EventPlayerBotCommand(player, Command, Payload))
					Relay = false;
			}

			if (SpamBot) {
				CONSOLE_Print("[GAME: " + m_GameName + "] kicking spambot [" + player->GetName() + "]");
				player->SetDeleteMe(true);
				player->SetLeftReason("spambot");

				if (!m_GameLoading && !m_GameLoaded)
					player->SetLeftCode(PLAYERLEAVE_LOBBY);
				else
					player->SetLeftCode(PLAYERLEAVE_LOST);

				if (!m_GameLoading && !m_GameLoaded)
					OpenSlot(GetSIDFromPID(player->GetPID()), false);
			}

			if (Relay) {
				BYTEARRAY ToPIDs = chatPlayer->GetToPIDs();

				for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
					BYTEARRAY IgnoredPIDs = (*i)->GetIgnoredPIDs();

					for (unsigned int j = 0; j < IgnoredPIDs.size(); ++j) {
						if (IgnoredPIDs[j] == chatPlayer->GetFromPID()) {
							for (BYTEARRAY ::iterator z = ToPIDs.begin(); z != ToPIDs.end(); ++z) {
								if (*z == (*i)->GetPID()) {
									ToPIDs.erase(z);
									break;
								}
							}

							break;
						}
					}
				}
				// verify toPIDs
				BYTEARRAY vToPIDs;
				for (unsigned int i = 0; i < ToPIDs.size(); ++i) {
					CGamePlayer* toPlayer = GetPlayerFromPID(ToPIDs[i]);

					if (toPlayer) {
						vToPIDs.push_back(ToPIDs[i]);
					}
				}
				Send(ToPIDs, m_Protocol->SEND_W3GS_CHAT_FROM_HOST(chatPlayer->GetFromPID(), vToPIDs, chatPlayer->GetFlag(), chatPlayer->GetExtraFlags(), chatPlayer->GetMessage()));
			}
		}
		else if (chatPlayer->GetType() == CIncomingChatPlayer ::CTH_TEAMCHANGE && !m_CountDownStarted)
			EventPlayerChangeTeam(player, chatPlayer->GetByte());
		else if (chatPlayer->GetType() == CIncomingChatPlayer ::CTH_COLOURCHANGE && !m_CountDownStarted)
			EventPlayerChangeColour(player, chatPlayer->GetByte());
		else if (chatPlayer->GetType() == CIncomingChatPlayer ::CTH_RACECHANGE && !m_CountDownStarted)
			EventPlayerChangeRace(player, chatPlayer->GetByte());
		else if (chatPlayer->GetType() == CIncomingChatPlayer ::CTH_HANDICAPCHANGE && !m_CountDownStarted)
			EventPlayerChangeHandicap(player, chatPlayer->GetByte());
	}
}

bool CBaseGame::EventPlayerBotCommand(CGamePlayer* player, string command, string payload)
{
	bool RootAdminCheck = false;

	for (vector<CBNET*>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i) {
		if ((*i)->GetServer() == player->GetSpoofedRealm() && (*i)->IsRootAdmin(player->GetName())) {
			RootAdminCheck = true;
			break;
		}
	}

	bool AdminCheck = player->GetIsAdmin();

	if (player->GetSpoofed()) {
		if (AdminCheck || RootAdminCheck || IsOwner(player->GetName())) {
			if (RootAdminCheck)
				CONSOLE_Print("[GAME: " + m_GameName + "] root admin [" + player->GetName() + "] sent command [" + command + "] with payload [" + payload + "]");
			else if (AdminCheck)
				CONSOLE_Print("[GAME: " + m_GameName + "] admin [" + player->GetName() + "] sent command [" + command + "] with payload [" + payload + "]");
			else
				CONSOLE_Print("[GAME: " + m_GameName + "] owner [" + player->GetName() + "] sent command [" + command + "] with payload [" + payload + "]");
		}
		else
			CONSOLE_Print("[GAME: " + m_GameName + "] player [" + player->GetName() + "] sent command [" + command + "] with payload [" + payload + "]");

		EventPlayerBotCommand2(player, command, payload, RootAdminCheck, AdminCheck, IsOwner(player->GetName()));
	}
	else {
		CONSOLE_Print("[GAME: " + m_GameName + "] non-spoofchecked player [" + player->GetName() + "] sent command [" + command + "] with payload [" + payload + "]");

		for (vector<CBNET*>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i) {
			if (player->GetJoinedRealm() == (*i)->GetServer()) {
				SendChat(player, "Spoof check by whispering bot with \"sc\" [ /w " + (*i)->GetBnetUserName() + " sc ]");
				break;
			}
		}
	}

	// return true if the command itself should be hidden from other players

	return false;
}

void CBaseGame::EventPlayerBotCommand2(CGamePlayer* player, string command, string payload, bool rootadmin, bool admin, bool owner)
{
	// todotodo: don't be lazy

	string User = player->GetName();
	string Command = command;
	string Payload = payload;

	if (rootadmin || admin || owner) {
		/*****************
		 * ADMIN COMMANDS *
		 ******************/

		//
		// !ABORT (abort countdown)
		// !A
		//

		// we use "!a" as an alias for abort because you don't have much time to abort the countdown so it's useful for the abort command to be easy to type

		if ((Command == "abort" || Command == "a") && m_CountDownStarted && !m_GameLoading && !m_GameLoaded) {
			SendAllChat(m_GHost->m_Language->CountDownAborted());
			m_CountDownStarted = false;
		}

		//
		// !ADDBAN
		// !BAN
		//

		//
		// !ANNOUNCE
		//

		//
		// !AUTOSAVE
		//

		//
		// !AUTOSTART
		//

		//
		// !BANLAST
		//

		//
		// !CHECK
		//

		//
		// !CHECKBAN
		//

		//
		// !CLEARHCL
		//

		else if (Command == "clearhcl" && !m_CountDownStarted) {
			m_HCLCommandString.clear();
			SendAllChat(m_GHost->m_Language->ClearingHCL());
		}

		//
		// !CLOSE (close slot)
		//

		else if (Command == "close" && !Payload.empty() && !m_GameLoading && !m_GameLoaded) {
			// close as many slots as specified, e.g. "5 10" closes slots 5 and 10

			stringstream SS;
			SS << Payload;

			while (!SS.eof()) {
				uint32_t SID;
				SS >> SID;

				if (SS.fail()) {
					CONSOLE_Print("[GAME: " + m_GameName + "] bad input to close command");
					break;
				}
				else {
					if (GetPlayerFromSID(SID - 1) != NULL)
						m_GHost->m_Manager->SendGamePlayerLeftLobbyInform(GetPlayerFromSID(SID - 1)->GetSpoofedRealm(), GetPlayerFromSID(SID - 1)->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_OWNER_KICKED);
					CloseSlot((unsigned char)(SID - 1), true);
				}
			}
		}

		//
		// !CLOSEALL
		//

		else if (Command == "closeall" && !m_GameLoading && !m_GameLoaded)
			CloseAllSlots();

		//
		// !COMP (computer slot)
		//

		else if (Command == "comp" && !Payload.empty() && !m_GameLoading && !m_GameLoaded && !m_SaveGame) {
			// extract the slot and the skill
			// e.g. "1 2" -> slot: "1", skill: "2"

			uint32_t Slot;
			uint32_t Skill = 1;
			stringstream SS;
			SS << Payload;
			SS >> Slot;

			if (SS.fail())
				CONSOLE_Print("[GAME: " + m_GameName + "] bad input #1 to comp command");
			else {
				if (!SS.eof())
					SS >> Skill;

				if (SS.fail())
					CONSOLE_Print("[GAME: " + m_GameName + "] bad input #2 to comp command");
				else
					ComputerSlot((unsigned char)(Slot - 1), (unsigned char)Skill, true);
			}
		}

		//
		// !COMPCOLOUR (computer colour change)
		//

		else if (Command == "compcolour" && !Payload.empty() && !m_GameLoading && !m_GameLoaded && !m_SaveGame) {
			// extract the slot and the colour
			// e.g. "1 2" -> slot: "1", colour: "2"

			uint32_t Slot;
			uint32_t Colour;
			stringstream SS;
			SS << Payload;
			SS >> Slot;

			if (SS.fail())
				CONSOLE_Print("[GAME: " + m_GameName + "] bad input #1 to compcolour command");
			else {
				if (SS.eof())
					CONSOLE_Print("[GAME: " + m_GameName + "] missing input #2 to compcolour command");
				else {
					SS >> Colour;

					if (SS.fail())
						CONSOLE_Print("[GAME: " + m_GameName + "] bad input #2 to compcolour command");
					else {
						unsigned char SID = (unsigned char)(Slot - 1);

						if (!(m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) && Colour < 12 && SID < m_Slots.size()) {
							if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OCCUPIED && m_Slots[SID].GetComputer() == 1)
								ColourSlot(SID, Colour);
						}
					}
				}
			}
		}

		//
		// !COMPHANDICAP (computer handicap change)
		//

		else if (Command == "comphandicap" && !Payload.empty() && !m_GameLoading && !m_GameLoaded && !m_SaveGame) {
			// extract the slot and the handicap
			// e.g. "1 50" -> slot: "1", handicap: "50"

			uint32_t Slot;
			uint32_t Handicap;
			stringstream SS;
			SS << Payload;
			SS >> Slot;

			if (SS.fail())
				CONSOLE_Print("[GAME: " + m_GameName + "] bad input #1 to comphandicap command");
			else {
				if (SS.eof())
					CONSOLE_Print("[GAME: " + m_GameName + "] missing input #2 to comphandicap command");
				else {
					SS >> Handicap;

					if (SS.fail())
						CONSOLE_Print("[GAME: " + m_GameName + "] bad input #2 to comphandicap command");
					else {
						unsigned char SID = (unsigned char)(Slot - 1);

						if (!(m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) && (Handicap == 50 || Handicap == 60 || Handicap == 70 || Handicap == 80 || Handicap == 90 || Handicap == 100) && SID < m_Slots.size()) {
							if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OCCUPIED && m_Slots[SID].GetComputer() == 1) {
								m_Slots[SID].SetHandicap((unsigned char)Handicap);
								SendAllSlotInfo();
							}
						}
					}
				}
			}
		}

		//
		// !COMPRACE (computer race change)
		//

		else if (Command == "comprace" && !Payload.empty() && !m_GameLoading && !m_GameLoaded && !m_SaveGame) {
			// extract the slot and the race
			// e.g. "1 human" -> slot: "1", race: "human"

			uint32_t Slot;
			string Race;
			stringstream SS;
			SS << Payload;
			SS >> Slot;

			if (SS.fail())
				CONSOLE_Print("[GAME: " + m_GameName + "] bad input #1 to comprace command");
			else {
				if (SS.eof())
					CONSOLE_Print("[GAME: " + m_GameName + "] missing input #2 to comprace command");
				else {
					getline(SS, Race);
					string ::size_type Start = Race.find_first_not_of(" ");

					if (Start != string ::npos)
						Race = Race.substr(Start);

					transform(Race.begin(), Race.end(), Race.begin(), (int (*)(int))tolower);
					unsigned char SID = (unsigned char)(Slot - 1);

					if (!(m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) && !(m_Map->GetMapFlags() & MAPFLAG_RANDOMRACES) && SID < m_Slots.size()) {
						if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OCCUPIED && m_Slots[SID].GetComputer() == 1) {
							if (Race == "human") {
								m_Slots[SID].SetRace(SLOTRACE_HUMAN | SLOTRACE_SELECTABLE);
								SendAllSlotInfo();
							}
							else if (Race == "orc") {
								m_Slots[SID].SetRace(SLOTRACE_ORC | SLOTRACE_SELECTABLE);
								SendAllSlotInfo();
							}
							else if (Race == "night elf") {
								m_Slots[SID].SetRace(SLOTRACE_NIGHTELF | SLOTRACE_SELECTABLE);
								SendAllSlotInfo();
							}
							else if (Race == "undead") {
								m_Slots[SID].SetRace(SLOTRACE_UNDEAD | SLOTRACE_SELECTABLE);
								SendAllSlotInfo();
							}
							else if (Race == "random") {
								m_Slots[SID].SetRace(SLOTRACE_RANDOM | SLOTRACE_SELECTABLE);
								SendAllSlotInfo();
							}
							else
								CONSOLE_Print("[GAME: " + m_GameName + "] unknown race [" + Race + "] sent to comprace command");
						}
					}
				}
			}
		}

		//
		// !COMPTEAM (computer team change)
		//

		else if (Command == "compteam" && !Payload.empty() && !m_GameLoading && !m_GameLoaded && !m_SaveGame) {
			// extract the slot and the team
			// e.g. "1 2" -> slot: "1", team: "2"

			uint32_t Slot;
			uint32_t Team;
			stringstream SS;
			SS << Payload;
			SS >> Slot;

			if (SS.fail())
				CONSOLE_Print("[GAME: " + m_GameName + "] bad input #1 to compteam command");
			else {
				if (SS.eof())
					CONSOLE_Print("[GAME: " + m_GameName + "] missing input #2 to compteam command");
				else {
					SS >> Team;

					if (SS.fail())
						CONSOLE_Print("[GAME: " + m_GameName + "] bad input #2 to compteam command");
					else {
						unsigned char SID = (unsigned char)(Slot - 1);

						if (!(m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) && Team < 12 && SID < m_Slots.size()) {
							if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OCCUPIED && m_Slots[SID].GetComputer() == 1) {
								m_Slots[SID].SetTeam((unsigned char)(Team - 1));
								SendAllSlotInfo();
							}
						}
					}
				}
			}
		}

		//
		// !DBSTATUS
		//

		//
		// !DOWNLOAD
		// !DL
		//

		//
		// !DROP
		//

		//
		// !END
		//

		//
		// !FAKEPLAYER
		//

		//
		// !FPPAUSE
		//

		//
		// !FPRESUME
		//

		//
		// !HCL
		//

		//
		// !HOLD (hold a slot for someone)
		//

		else if (Command == "hold" && !m_GameLoading && !m_GameLoaded) {
			if (!Payload.empty()) {
				// hold as many players as specified, e.g. "Varlock Kilranin" holds players "Varlock" and "Kilranin"

				stringstream SS;
				SS << Payload;

				while (!SS.eof()) {
					string HoldName;
					SS >> HoldName;

					if (SS.fail()) {
						CONSOLE_Print("[GAME: " + m_GameName + "] bad input to hold command");
						break;
					}
					else {
						SendAllChat(m_GHost->m_Language->AddedPlayerToTheHoldList(HoldName));
						AddToReserved(HoldName);
					}
				}
			}
			else {
				std::ostringstream heldPlayersSS;
				std::copy(std::begin(m_Reserved), std::end(m_Reserved), std::ostream_iterator<string>(heldPlayersSS, " "));
				SendAllChat(heldPlayersSS.str());
			}
		}

		//
		// !KICK (kick a player)
		//

		else if (Command == "kick" && !Payload.empty() && !m_GameLoading && !m_GameLoaded) {
			CGamePlayer* LastMatch = NULL;
			uint32_t Matches = GetPlayerFromNamePartial(Payload, &LastMatch);

			if (Matches == 0)
				SendAllChat(m_GHost->m_Language->UnableToKickNoMatchesFound(Payload));
			else if (Matches == 1) {
				LastMatch->SetDeleteMe(true);
				LastMatch->SetLeftReason(m_GHost->m_Language->WasKickedByPlayer(User));

				if (!m_GameLoading && !m_GameLoaded) {
					LastMatch->SetLeftCode(PLAYERLEAVE_LOBBY);
					m_GHost->m_Manager->SendGamePlayerLeftLobbyInform(LastMatch->GetSpoofedRealm(), LastMatch->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_OWNER_KICKED);
				}
				else
					LastMatch->SetLeftCode(PLAYERLEAVE_LOST);

				if (!m_GameLoading && !m_GameLoaded)
					OpenSlot(GetSIDFromPID(LastMatch->GetPID()), false);
			}
			else
				SendAllChat(m_GHost->m_Language->UnableToKickFoundMoreThanOneMatch(Payload));
		}

		//
		// !LATENCY (set game latency)
		//

		else if (Command == "latency") {
			if (Payload.empty())
				SendAllChat(m_GHost->m_Language->LatencyIs(UTIL_ToString(m_Latency)));
			else {
				m_Latency = UTIL_ToUInt32(Payload);

				if (m_Latency <= m_GHost->m_MinLatency) {
					m_Latency = m_GHost->m_MinLatency;
					SendAllChat(m_GHost->m_Language->SettingLatencyToMinimum(UTIL_ToString(m_Latency)));
				}
				else if (m_Latency >= m_GHost->m_MaxLatency) {
					m_Latency = m_GHost->m_MaxLatency;
					SendAllChat(m_GHost->m_Language->SettingLatencyToMaximum(UTIL_ToString(m_Latency)));
				}
				else
					SendAllChat(m_GHost->m_Language->SettingLatencyTo(UTIL_ToString(m_Latency)));
			}
		}

		//
		// !_LATENCY (set game latency)
		//

		else if (Command == "_latency") {
			if (rootadmin) {
				if (Payload.empty())
					SendChat(player, m_GHost->m_Language->LatencyIs(UTIL_ToString(m_Latency)));
				else {
					m_Latency = UTIL_ToUInt32(Payload);
					SendChat(player, m_GHost->m_Language->SettingLatencyTo(UTIL_ToString(m_Latency)));
				}
			}
		}

		//
		// !_MAPCHECK
		//

		else if (Command == "_mapcheck") {
			if (rootadmin) {
				if (!m_GameLoading && !m_GameLoaded) {
					player->Send(m_Protocol->SEND_W3GS_MAPCHECK(m_Map->GetMapPath(), m_Map->GetMapSize(), m_Map->GetMapInfo(), m_Map->GetMapCRC(), m_Map->GetMapSHA1()));
					SendChat(player, "Sending mapcheck...");
					CONSOLE_Print("Sending mapcheck...");
				}
			}
		}

		//
		// !LOAD
		//

		else if (Command == "load") {
			CGamePlayer* Shortest = NULL;
			CGamePlayer* Longest = NULL;

			for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
				if (!Shortest || (*i)->GetFinishedLoadingTicks() < Shortest->GetFinishedLoadingTicks())
					Shortest = *i;

				if (!Longest || (*i)->GetFinishedLoadingTicks() > Longest->GetFinishedLoadingTicks())
					Longest = *i;
			}

			if (Shortest && Longest) {
				SendChat(player, m_GHost->m_Language->ShortestLoadByPlayer(Shortest->GetName(), UTIL_ToString((float)(Shortest->GetFinishedLoadingTicks() - m_StartedLoadingTicks) / 1000, 2)));
				SendChat(player, m_GHost->m_Language->LongestLoadByPlayer(Longest->GetName(), UTIL_ToString((float)(Longest->GetFinishedLoadingTicks() - m_StartedLoadingTicks) / 1000, 2)));
			}

			for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
				if (player->GetPID() == (*i)->GetPID()) {
					SendChat(player, m_GHost->m_Language->YourLoadingTimeWas(UTIL_ToString((float)((*i)->GetFinishedLoadingTicks() - m_StartedLoadingTicks) / 1000, 2)));
					break;
				}
			}
		}

		//
		// !LOCK
		//

		//
		// !MESSAGES
		//

		//
		// !MUTE
		//

		else if (Command == "mute") {
			if (rootadmin || admin) {
				CGamePlayer* LastMatch = NULL;
				uint32_t Matches = GetPlayerFromNamePartial(Payload, &LastMatch);

				if (Matches == 0)
					SendAllChat(m_GHost->m_Language->UnableToMuteNoMatchesFound(Payload));
				else if (Matches == 1) {
					SendAllChat(m_GHost->m_Language->MutedPlayer(LastMatch->GetName(), User));
					LastMatch->SetMuted(true);
				}
				else
					SendAllChat(m_GHost->m_Language->UnableToMuteFoundMoreThanOneMatch(Payload));
			}
		}

		//
		// !MUTEALL
		//

		else if (Command == "muteall" && m_GameLoaded) {
			SendAllChat(m_GHost->m_Language->GlobalChatMuted());
			m_MuteAll = true;
		}

		//
		// !OPEN (open slot)
		//

		else if (Command == "open" && !Payload.empty() && !m_GameLoading && !m_GameLoaded) {
			// open as many slots as specified, e.g. "5 10" opens slots 5 and 10

			stringstream SS;
			SS << Payload;

			while (!SS.eof()) {
				uint32_t SID;
				SS >> SID;

				if (SS.fail()) {
					CONSOLE_Print("[GAME: " + m_GameName + "] bad input to open command");
					break;
				}
				else {
					if (GetPlayerFromSID(SID - 1) != NULL)
						m_GHost->m_Manager->SendGamePlayerLeftLobbyInform(GetPlayerFromSID(SID - 1)->GetSpoofedRealm(), GetPlayerFromSID(SID - 1)->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_OWNER_KICKED);
					OpenSlot((unsigned char)(SID - 1), true);
					SendAllChat("Slot " + UTIL_ToString(SID) + " opened");
				}
			}
		}

		//
		// !OPENALL
		//

		else if (Command == "openall" && !m_GameLoading && !m_GameLoaded)
			OpenAllSlots();

		//
		// !OWNER (set game owner)
		//

		//
		// !PRIV (rehost as private game)
		// !GOPRIV
		//

		else if ((Command == "gopriv" || Command == "priv") && !m_CountDownStarted && !m_SaveGame) {
			if (Payload.size() <= 31) {
				if (Payload.empty()) {
					if (m_GameNameRehostCounter)
						Payload = m_GameName.substr(0, m_GameName.length() - (2 + UTIL_ToString(m_GameNameRehostCounter).size()));
					else
						Payload = m_GameName;

					++m_GameNameRehostCounter;
					Payload += " #" + UTIL_ToString(m_GameNameRehostCounter);
				}
				else
					m_GameNameRehostCounter = 0;

				m_GHost->m_Manager->SendGameNameChanged(m_GameID, 17, Payload);

				CONSOLE_Print("[GAME: " + m_GameName + "] trying to rehost as private game [" + Payload + "]");
				SendAllChat(m_GHost->m_Language->TryingToRehostAsPrivateGame(Payload));
				m_GameState = GAME_PRIVATE;
				m_LastGameName = m_GameName;
				m_GameName = Payload;
				m_HostCounter = m_GHost->m_HostCounter++;
				m_RefreshError = false;
				m_RefreshRehosted = true;

				for (vector<CBNET*>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); i++) {
					// unqueue any existing game refreshes because we're going to assume the next successful game refresh indicates that the rehost worked
					// this ignores the fact that it's possible a game refresh was just sent and no response has been received yet
					// we assume this won't happen very often since the only downside is a potential false positive

					(*i)->UnqueueGameRefreshes();
					(*i)->QueueGameUncreate();
					(*i)->QueueEnterChat();

					// we need to send the game creation message now because private games are not refreshed

					(*i)->QueueGameCreate(m_GameState, m_GameName, string(), m_Map, NULL, m_HostCounter);

					if ((*i)->GetPasswordHashType() != "pvpgn")
						(*i)->QueueEnterChat();
				}

				m_CreationTime = GetTime();
				m_LastRefreshTime = GetTime();
			}
			else
				SendAllChat("Unable to rehost game, the game name is too long.");
		}

		//
		// !PUB (rehost as public game)
		// !GOPUB
		//

		else if ((Command == "gopub" || Command == "pub") && !m_CountDownStarted && !m_SaveGame) {
			if (Payload.size() <= 31) {
				if (Payload.empty()) {
					if (m_GameNameRehostCounter)
						Payload = m_GameName.substr(0, m_GameName.length() - (2 + UTIL_ToString(m_GameNameRehostCounter).size()));
					else
						Payload = m_GameName;

					++m_GameNameRehostCounter;
					Payload += " #" + UTIL_ToString(m_GameNameRehostCounter);
				}
				else
					m_GameNameRehostCounter = 0;

				m_GHost->m_Manager->SendGameNameChanged(m_GameID, 16, Payload);

				CONSOLE_Print("[GAME: " + m_GameName + "] trying to rehost as public game [" + Payload + "]");
				SendAllChat(m_GHost->m_Language->TryingToRehostAsPublicGame(Payload));
				m_GameState = GAME_PUBLIC;
				m_LastGameName = m_GameName;
				m_GameName = Payload;
				m_HostCounter = m_GHost->m_HostCounter++;
				m_RefreshError = false;
				m_RefreshRehosted = true;

				for (vector<CBNET*>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); i++) {
					// unqueue any existing game refreshes because we're going to assume the next successful game refresh indicates that the rehost worked
					// this ignores the fact that it's possible a game refresh was just sent and no response has been received yet
					// we assume this won't happen very often since the only downside is a potential false positive

					(*i)->UnqueueGameRefreshes();
					(*i)->QueueGameUncreate();
					(*i)->QueueEnterChat();

					// the game creation message will be sent on the next refresh
				}

				m_CreationTime = GetTime();
				m_LastRefreshTime = GetTime();
			}
			else
				SendAllChat("Unable to rehost game, the game name is too long.");
		}

		//
		// !REFRESH (turn on or off refresh messages)
		//

		//
		// !SAY
		//

		//
		// !SENDLAN
		//

		//
		// !SP
		//

		else if (Command == "sp" && !m_CountDownStarted) {
			SendAllChat(m_GHost->m_Language->ShufflingPlayers());
			ShuffleSlots();
		}

		//
		// !START
		//

		else if (Command == "start" && !m_CountDownStarted) {
			if (GetTicks() - m_LastPlayerLeaveTicks >= 2000)
				StartCountDown(false);
			else
				SendAllChat(m_GHost->m_Language->CountDownAbortedSomeoneLeftRecently());
		}

		//
		// !STARTN
		//

		//
		// !SWAP (swap slots)
		//

		// else if( Command == "swap" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded )
		else if (Command == "swap" && !Payload.empty() && !m_GameLoading && !m_GameLoaded && !m_CountDownStarted) {
			uint32_t SID1;
			uint32_t SID2;
			stringstream SS;
			SS << Payload;
			SS >> SID1;

			if (SS.fail())
				CONSOLE_Print("[GAME: " + m_GameName + "] bad input #1 to swap command");
			else {
				if (SS.eof())
					CONSOLE_Print("[GAME: " + m_GameName + "] missing input #2 to swap command");
				else {
					SS >> SID2;

					if (SS.fail())
						CONSOLE_Print("[GAME: " + m_GameName + "] bad input #2 to swap command");
					else
						SwapSlots((unsigned char)(SID1 - 1), (unsigned char)(SID2 - 1));
				}
			}
		}

		//
		// !SYNCLIMIT
		// !SYNC
		//

		else if (Command == "synclimit" || Command == "sync") {
			if (Payload.empty())
				SendAllChat(m_GHost->m_Language->SyncLimitIs(UTIL_ToString(m_SyncLimit)));
			else {
				if (!m_Lagging) {
					m_SyncLimit = UTIL_ToUInt32(Payload);

					if (m_SyncLimit <= m_GHost->m_MinSyncLimit) {
						m_SyncLimit = m_GHost->m_MinSyncLimit;
						SendAllChat(m_GHost->m_Language->SettingSyncLimitToMinimum(UTIL_ToString(m_SyncLimit)));
					}
					else if (m_SyncLimit >= m_GHost->m_MaxSyncLimit) {
						m_SyncLimit = m_GHost->m_MaxSyncLimit;
						SendAllChat(m_GHost->m_Language->SettingSyncLimitToMaximum(UTIL_ToString(m_SyncLimit)));
					}
					else {
						SendAllChat(m_GHost->m_Language->SettingSyncLimitTo(UTIL_ToString(m_SyncLimit)));
					}
				}
				else {
					SendChat(player, m_GHost->m_Language->CommandDisabled());
				}
			}
		}

		//
		// !UNHOST
		//

		else if (Command == "unhost" && !m_CountDownStarted)
			m_Exiting = true;

		//
		// !UNLOCK
		//

		//
		// !UNMUTE
		//

		else if (Command == "unmute") {
			if (rootadmin || admin) {
				CGamePlayer* LastMatch = NULL;
				uint32_t Matches = GetPlayerFromNamePartial(Payload, &LastMatch);

				if (Matches == 0)
					SendAllChat(m_GHost->m_Language->UnableToMuteNoMatchesFound(Payload));
				else if (Matches == 1) {
					SendAllChat(m_GHost->m_Language->UnmutedPlayer(LastMatch->GetName(), User));
					LastMatch->SetMuted(false);
				}
				else
					SendAllChat(m_GHost->m_Language->UnableToMuteFoundMoreThanOneMatch(Payload));
			}
		}

		//
		// !UNMUTEALL
		//

		else if (Command == "unmuteall" && m_GameLoaded) {
			SendAllChat(m_GHost->m_Language->GlobalChatUnmuted());
			m_MuteAll = false;
		}

		//
		// !VIRTUALHOST
		//

		//
		// !VOTECANCEL
		//

		else if (Command == "votecancel" && !m_KickVotePlayer.empty()) {
			SendAllChat(m_GHost->m_Language->VoteKickCancelled(m_KickVotePlayer));
			m_KickVotePlayer.clear();
			m_StartedKickVoteTime = 0;
		}

		//
		// !W
		//
	}

	/*********************
	 * NON ADMIN COMMANDS *
	 *********************/

	//
	// !CHECK
	//

	if (Command == "check" && !Payload.empty()) {
		CGamePlayer* LastMatch = NULL;
		uint32_t Matches = GetPlayerFromNamePartial(Payload, &LastMatch);

		if (Matches == 0)
			SendChat(player, "Unable to check player [" + Payload + "]. No matches found.");
		else if (Matches == 1)
			SendChat(player, m_GHost->m_Language->CheckedPlayer(LastMatch->GetName(), LastMatch->GetNumPings() > 0 ? UTIL_ToString(LastMatch->GetPing(m_GHost->m_LCPings)) + "ms" : "N/A", LastMatch->GetLongFrom() + " (" + LastMatch->GetFrom() + ")", LastMatch->GetIsAdmin() ? "Yes" : "No", IsOwner(LastMatch->GetName()) ? "Yes" : "No", LastMatch->GetSpoofed() ? "Yes" : "No", LastMatch->GetSpoofedRealm().empty() ? "N/A" : LastMatch->GetSpoofedRealm(), LastMatch->GetReserved() ? "Yes" : "No", LastMatch->GetGProxy() ? "Yes" : "No"));
		else
			SendChat(player, "Unable to check player [" + Payload + "]. Found more than one match.");
	}

	//
	// !CHECKME
	//

	else if (Command == "checkme")
		SendChat(player, m_GHost->m_Language->CheckedPlayer(User, player->GetNumPings() > 0 ? UTIL_ToString(player->GetPing(m_GHost->m_LCPings)) + "ms" : "N/A", player->GetLongFrom() + " (" + player->GetFrom() + ")", admin || rootadmin ? "Yes" : "No", owner ? "Yes" : "No", player->GetSpoofed() ? "Yes" : "No", player->GetSpoofedRealm().empty() ? "N/A" : player->GetSpoofedRealm(), player->GetReserved() ? "Yes" : "No", player->GetGProxy() ? "Yes" : "No"));

	//
	// !FROM
	// !F
	//

	else if (Command == "f" || Command == "from") {
		string Froms;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			// we reverse the byte order on the IP because it's stored in network byte order

			Froms += (*i)->GetNameTerminated();
			Froms += ": (";
			// Froms += m_GHost->m_DBLocal->FromCheck( UTIL_ByteArrayToUInt32( (*i)->GetExternalIP( ), true ) );
			Froms += (*i)->GetFrom();
			Froms += ")";

			if (i != m_Players.end() - 1)
				Froms += ", ";

			if ((m_GameLoading || m_GameLoaded) && Froms.size() > 100) {
				// cut the text into multiple lines ingame

				if (owner)
					SendAllChat(Froms);
				else
					SendChat(player, Froms);

				Froms.clear();
			}
		}

		if (!Froms.empty()) {
			if (owner)
				SendAllChat(Froms);
			else
				SendChat(player, Froms);
		}
	}

	//
	// !HCL
	//

	else if ((Command == "hcl" || Command == "mode") && !m_CountDownStarted) {
		if ((rootadmin || admin || owner) && !m_CountDownStarted) {
			if (!Payload.empty()) {
				if (Payload.size() <= m_Slots.size()) {
					string HCLChars = "abcdefghijklmnopqrstuvwxyz0123456789 -=,.";

					if (Payload.find_first_not_of(HCLChars) == string ::npos) {
						m_HCLCommandString = Payload;
						SendChat(player, m_GHost->m_Language->SettingHCL(m_HCLCommandString));
					}
					else
						SendChat(player, m_GHost->m_Language->UnableToSetHCLInvalid());
				}
				else
					SendChat(player, m_GHost->m_Language->UnableToSetHCLTooLong());
			}
			else
				SendChat(player, m_GHost->m_Language->TheHCLIs(m_HCLCommandString));
		}
		else
			SendChat(player, m_GHost->m_Language->TheHCLIs(m_HCLCommandString));
	}

	//
	// !GAMEINFO
	//

	else if (Command == "gameinfo") {
		for (vector<CBNET*>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); i++) {
			if ((*i)->GetServer() == player->GetJoinedRealm()) {
				SendChat(player, "Name: [" + m_GameName + "], Type: [custom game], Owner: [" + m_OwnerName + "], Hostbot: [" + (*i)->GetBnetUserName() + "]");
				break;
			}
		}
	}

	//
	// !GAMENAME
	// !GN
	//

	else if (Command == "gamename" || Command == "gn")
		SendChat(player, "Current game name is [" + m_GameName + "].");

	//
	// !GPROXY
	//

	else if (Command == "gproxy") {
		string GProxyUsers;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			if ((*i)->GetGProxy()) {
				if (GProxyUsers.empty())
					GProxyUsers += (*i)->GetName() + ": Yes";
				else
					GProxyUsers += ", " + (*i)->GetName() + ": Yes";

				if ((m_GameLoading || m_GameLoaded) && GProxyUsers.size() > 100) {
					// cut the text into multiple lines ingame

					if (owner)
						SendAllChat(GProxyUsers);
					else
						SendChat(player, GProxyUsers);

					GProxyUsers.clear();
				}
			}
		}

		if (!GProxyUsers.empty()) {
			if (owner)
				SendAllChat(GProxyUsers);
			else
				SendChat(player, GProxyUsers);
		}
		else {
			if (owner)
				SendAllChat("Noone is using GProxy.");
			else
				SendChat(player, "Noone is using GProxy.");
		}
	}

	//
	// !OWNER (set game owner)
	//

	else if (Command == "owner") {
		if (!Payload.empty()) {
			if (rootadmin || admin || owner) {
				CGamePlayer* Player = NULL;
				uint32_t Matches = GetPlayerFromNamePartial(Payload, &Player);

				if (Matches == 0)
					SendChat(player, "No matches found for [" + Payload + "]");
				else if (Matches == 1) {
					if (!m_GHost->m_ContributorOnlyMode) {
						ChangeOwner(Player->GetName());
						SendAllChat(m_GHost->m_Language->SettingGameOwnerTo(m_OwnerName));
					}
					else
						SendChat(player, "Current game owner is [" + m_OwnerName + "]");
				}
				else
					SendChat(player, "Found more then one match for [" + Payload + "]");
			}
			else
				SendChat(player, m_GHost->m_Language->UnableToSetGameOwner(m_OwnerName));
		}
		else {
			if (m_GameLoading || m_GameLoaded) {
				// let players take ownership if owner left the game

				bool OwnerInGame = false;

				for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); i++) {
					if (IsOwner((*i)->GetName())) {
						OwnerInGame = true;
						break;
					}
				}

				if (!OwnerInGame) {
					ChangeOwner(User);
					SendAllChat(m_GHost->m_Language->SettingGameOwnerTo(m_OwnerName));
				}
				else
					SendChat(player, "Current game owner is [" + m_OwnerName + "]");
			}
			else
				SendChat(player, "Current game owner is [" + m_OwnerName + "]");
		}
	}

	//
	// !PING
	//

	else if (Command == "p" || Command == "ping") {
		// kick players with ping higher than payload if payload isn't empty
		// we only do this if the game hasn't started since we don't want to kick players from a game in progress

		uint32_t Kicked = 0;
		uint32_t KickPing = 0;

		if (!m_GameLoading && !m_GameLoaded && !Payload.empty() && (rootadmin || admin || owner))
			KickPing = UTIL_ToUInt32(Payload);

		// copy the m_Players vector so we can sort by descending ping so it's easier to find players with high pings

		vector<CGamePlayer*> SortedPlayers = m_Players;
		sort(SortedPlayers.begin(), SortedPlayers.end(), CGamePlayerSortDescByPing());
		string Pings;

		for (vector<CGamePlayer*>::iterator i = SortedPlayers.begin(); i != SortedPlayers.end(); ++i) {
			Pings += (*i)->GetNameTerminated();
			Pings += ": ";

			if ((*i)->GetNumPings() > 0) {
				Pings += UTIL_ToString((*i)->GetPing(m_GHost->m_LCPings));

				if (!m_GameLoading && !m_GameLoaded && !(*i)->GetReserved() && KickPing > 0 && (*i)->GetPing(m_GHost->m_LCPings) > KickPing) {
					(*i)->SetDeleteMe(true);
					(*i)->SetLeftReason("was kicked for excessive ping " + UTIL_ToString((*i)->GetPing(m_GHost->m_LCPings)) + " > " + UTIL_ToString(KickPing));
					(*i)->SetLeftCode(PLAYERLEAVE_LOBBY);
					SendAllChat((*i)->GetName() + " " + (*i)->GetLeftReason());
					m_GHost->m_Manager->SendGamePlayerLeftLobbyInform((*i)->GetSpoofedRealm(), (*i)->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_PING);
					OpenSlot(GetSIDFromPID((*i)->GetPID()), false);
					Kicked++;
				}

				Pings += "ms";
			}
			else
				Pings += "N/A";

			if (i != SortedPlayers.end() - 1)
				Pings += ", ";

			if ((m_GameLoading || m_GameLoaded) && Pings.size() > 100) {
				// cut the text into multiple lines ingame

				if (owner)
					SendAllChat(Pings);
				else
					SendChat(player, Pings);

				Pings.clear();
			}
		}

		if (!Pings.empty()) {
			if (owner)
				SendAllChat(Pings);
			else
				SendChat(player, Pings);
		}

		if (Kicked > 0)
			SendAllChat(m_GHost->m_Language->KickingPlayersWithPingsGreaterThan(UTIL_ToString(Kicked), UTIL_ToString(KickPing)));
	}

	//
	// !REMAKE
	// !RMK
	//

	else if ((Command == "remake" || Command == "rmk") && m_GameLoaded && !m_GameOverTime) {
		if (!m_VoteRemakeStarted) {
			if (GetNumHumanPlayers() < 2)
				SendAllChat("Unable to start vote remake, you are alone in the game.");
			else {
				m_VoteRemakeStarted = true;
				m_VoteRemakeStartedTime = GetTime();

				for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i)
					(*i)->SetRemakeVote(false);

				player->SetRemakeVote(true);

				CONSOLE_Print("[GAME: " + m_GameName + "] vote remake started by player [" + User + "]");
				SendAllChat("[" + User + "] voted to rmk [1/" + UTIL_ToString(GetNumHumanPlayers() == 2 ? 2 : (uint32_t)ceil((GetNumHumanPlayers() - 1) * (float)80 / 100)) + "]");
			}
		}
		else {
			uint32_t VotesNeeded = GetNumHumanPlayers() == 2 ? 2 : (uint32_t)ceil((GetNumHumanPlayers() - 1) * (float)80 / 100);
			uint32_t Votes = 0;

			bool AlreadyVoted = player->GetRemakeVote();
			player->SetRemakeVote(true);

			for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
				if ((*i)->GetRemakeVote())
					++Votes;
			}

			if (!AlreadyVoted) {
				CONSOLE_Print("[GAME: " + m_GameName + "] player [" + User + "] voted to remake, still " + UTIL_ToString(VotesNeeded - Votes) + " votes are needed");
				SendAllChat("[" + User + "] voted to rmk [" + UTIL_ToString(Votes) + "/" + UTIL_ToString(VotesNeeded) + "]");
			}
			else
				SendChat(player, "You already voted to rmk [" + UTIL_ToString(Votes) + "/" + UTIL_ToString(VotesNeeded) + "]");

			if (Votes >= VotesNeeded) {
				CONSOLE_Print("[GAME: " + m_GameName + "] gameover timer started (players voted to remake the game)");
				SendAllChat(UTIL_ToString(Votes) + " of " + UTIL_ToString(GetNumHumanPlayers()) + " players voted to remake the game, game is ending...");

				m_RMK = true;

				SendEndMessage();
				m_GameOverTime = GetTime();

				m_VoteRemakeStarted = false;
				m_VoteRemakeStartedTime = 0;
			}
		}
	}

	//
	// !REMAKECOUNT
	// !RMKCOUNT
	//

	else if ((Command == "remakecount" || Command == "rmkcount") && m_GameLoaded && !m_GameOverTime) {
		uint32_t VotesNeeded = GetNumHumanPlayers() == 2 ? 2 : (uint32_t)ceil((GetNumHumanPlayers() - 1) * (float)80 / 100);
		uint32_t Votes = 0;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			if ((*i)->GetRemakeVote())
				++Votes;
		}

		SendChat(player, "[" + UTIL_ToString(Votes) + "/" + UTIL_ToString(VotesNeeded) + "] players already voted to rmk the game.");
	}

	//
	// !SC
	//

	else if ((Command == "sc" || Command == "spoof" || Command == "spoofcheck") && !m_GameLoading && !m_GameLoaded) {
		for (vector<CBNET*>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); i++) {
			if (player->GetJoinedRealm() == (*i)->GetServer()) {
				// SendChat( player, "Spoof check by whispering bot with \"sc\" [ /w " + (*i)->GetUserName( ) + " sc ]" );
				if ((*i)->GetPasswordHashType() == "rgc")
					SendChat(player, "You are already spoof checked");
				else
					SendChat(player, "You are already spoof checked [ /w " + (*i)->GetBnetUserName() + " sc ]");
				break;
			}
		}
	}

	//
	// !STATS
	//

	//
	// !SD
	// !STATSDOTA
	//

	else if (Command == "statsdota" || Command == "sd") {
		if (Payload.empty())
			Payload = User;

		// players can use !statsdota only on players that are in lobby

		CGamePlayer* StatsUserPlayer = NULL;
		uint32_t Matches = GetPlayerFromNamePartial(Payload, &StatsUserPlayer);

		if (Matches == 0)
			SendChat(player, "Unable to check player. No matches found for [" + Payload + "].");
		else if (Matches == 1) {
			// players can use !statsdota only on players that are spoof checked

			if (StatsUserPlayer->GetSpoofed()) {
				CDBDiv1DPS* DotAPlayerSummary = StatsUserPlayer->GetDiv1DPS();

				if (DotAPlayerSummary) {
					if (IsOwner(User)) {
						SendAllChat("[" + StatsUserPlayer->GetName() + "] PSR: " + UTIL_ToString(DotAPlayerSummary->GetRating(), 0) + " games: " + UTIL_ToString(DotAPlayerSummary->GetTotalGames()) + " W/L: " + UTIL_ToString(DotAPlayerSummary->GetTotalWins()) + "/" + UTIL_ToString(DotAPlayerSummary->GetTotalLosses()));
						SendAllChat("H. K/D/A " + UTIL_ToString(DotAPlayerSummary->GetAvgKills(), 2) + "/" + UTIL_ToString(DotAPlayerSummary->GetAvgDeaths(), 2) + "/" + UTIL_ToString(DotAPlayerSummary->GetAvgAssists(), 2) + " C. K/D/N " + UTIL_ToString(DotAPlayerSummary->GetAvgCreepKills(), 2) + "/" + UTIL_ToString(DotAPlayerSummary->GetAvgCreepDenies(), 2) + "/" + UTIL_ToString(DotAPlayerSummary->GetAvgNeutralKills(), 2));
					}
					else {
						SendChat(player, "[" + StatsUserPlayer->GetName() + "] PSR: " + UTIL_ToString(DotAPlayerSummary->GetRating(), 0) + " games: " + UTIL_ToString(DotAPlayerSummary->GetTotalGames()) + " W/L: " + UTIL_ToString(DotAPlayerSummary->GetTotalWins()) + "/" + UTIL_ToString(DotAPlayerSummary->GetTotalLosses()));
						SendChat(player, "H. K/D/A " + UTIL_ToString(DotAPlayerSummary->GetAvgKills(), 2) + "/" + UTIL_ToString(DotAPlayerSummary->GetAvgDeaths(), 2) + "/" + UTIL_ToString(DotAPlayerSummary->GetAvgAssists(), 2) + " C. K/D/N " + UTIL_ToString(DotAPlayerSummary->GetAvgCreepKills(), 2) + "/" + UTIL_ToString(DotAPlayerSummary->GetAvgCreepDenies(), 2) + "/" + UTIL_ToString(DotAPlayerSummary->GetAvgNeutralKills(), 2));
					}
				}
				else {
					if (IsOwner(User))
						SendAllChat(m_GHost->m_Language->HasntPlayedDotAGamesWithThisBot(StatsUserPlayer->GetName()));
					else
						SendChat(player, m_GHost->m_Language->HasntPlayedDotAGamesWithThisBot(StatsUserPlayer->GetName()));
				}

				// player->SetStatsDotASentTime( GetTime( ) );
			}
			else
				SendChat(player, "Player [" + StatsUserPlayer->GetName() + "] is not spoof checked.");
		}
		else
			SendChat(player, "Unable to check player. Found more than one match for [" + Payload + "].");
	}

	//
	// !HS
	// !HIGHSCORE
	//

	else if (Command == "highscore" || Command == "hs") {
		if (Payload.empty())
			Payload = User;

		// players can use !hs only on players that are in lobby

		CGamePlayer* StatsUserPlayer = NULL;
		uint32_t Matches = GetPlayerFromNamePartial(Payload, &StatsUserPlayer);

		if (Matches == 0)
			SendChat(player, "Unable to check player. No matches found for [" + Payload + "].");
		else if (Matches == 1) {
			// players can use !hs only on players that are spoof checked

			if (StatsUserPlayer->GetSpoofed()) {
				CDBDiv1DPS* DotAPlayerSummary = StatsUserPlayer->GetDiv1DPS();

				if (DotAPlayerSummary) {
					if (IsOwner(User)) {
						SendAllChat("[" + StatsUserPlayer->GetName() + "] highest PSR: " + UTIL_ToString(DotAPlayerSummary->GetHighestRating(), 0));
					}
					else {
						SendChat(player, "[" + StatsUserPlayer->GetName() + "] highest PSR: " + UTIL_ToString(DotAPlayerSummary->GetHighestRating(), 0));
					}
				}
				else {
					if (IsOwner(User))
						SendAllChat(m_GHost->m_Language->HasntPlayedDotAGamesWithThisBot(StatsUserPlayer->GetName()));
					else
						SendChat(player, m_GHost->m_Language->HasntPlayedDotAGamesWithThisBot(StatsUserPlayer->GetName()));
				}

				// player->SetStatsDotASentTime( GetTime( ) );
			}
			else
				SendChat(player, "Player [" + StatsUserPlayer->GetName() + "] is not spoof checked.");
		}
		else
			SendChat(player, "Unable to check player. Found more than one match for [" + Payload + "].");
	}

	//
	// !GAMETIME
	// !TIME
	//

	else if (Command == "gametime" || Command == "time") {
		uint32_t Seconds;

		if (m_GameLoaded)
			Seconds = m_GameTicks / 1000;
		else
			Seconds = GetTime() - m_TrueCreationTime;

		string HourString = UTIL_ToString(Seconds / 3600);
		Seconds = Seconds % 3600;
		string MinString = UTIL_ToString(Seconds / 60);
		Seconds = Seconds % 60;
		string SecString = UTIL_ToString(Seconds);

		if (HourString.size() == 1)
			HourString.insert(0, "0");

		if (MinString.size() == 1)
			MinString.insert(0, "0");

		if (SecString.size() == 1)
			SecString.insert(0, "0");

		SendChat(player, "Game time is " + HourString + ":" + MinString + ":" + SecString);
	}

	//
	// !VERSION
	//

	else if (Command == "version") {
		if (player->GetSpoofed() && (admin || rootadmin || owner))
			SendChat(player, m_GHost->m_Language->VersionAdmin(m_GHost->m_Version));
		else
			SendChat(player, m_GHost->m_Language->VersionNotAdmin(m_GHost->m_Version));
	}

	//
	// !VOTEKICK
	//

	else if (Command == "votekick" && m_GHost->m_VoteKickAllowed && !Payload.empty() && m_GameLoaded) {
		if (!m_KickVotePlayer.empty())
			SendChat(player, m_GHost->m_Language->UnableToVoteKickAlreadyInProgress());
		else if (m_Players.size() == 2)
			SendChat(player, m_GHost->m_Language->UnableToVoteKickNotEnoughPlayers());
		else {
			CGamePlayer* LastMatch = NULL;
			uint32_t Matches = GetPlayerFromNamePartial(Payload, &LastMatch);

			if (Matches == 0)
				SendChat(player, m_GHost->m_Language->UnableToVoteKickNoMatchesFound(Payload));
			else if (Matches == 1) {
				if (LastMatch->GetReserved())
					SendChat(player, m_GHost->m_Language->UnableToVoteKickPlayerIsReserved(LastMatch->GetName()));
				else {
					m_KickVotePlayer = LastMatch->GetName();
					m_StartedKickVoteTime = GetTime();

					for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i)
						(*i)->SetKickVote(false);

					player->SetKickVote(true);
					CONSOLE_Print("[GAME: " + m_GameName + "] votekick against player [" + m_KickVotePlayer + "] started by player [" + User + "]");
					// SendAllChat( m_GHost->m_Language->StartedVoteKick( LastMatch->GetName( ), User, UTIL_ToString( (uint32_t)ceil( ( GetNumHumanPlayers( ) - 1 ) * (float)m_GHost->m_VoteKickPercentage / 100 ) - 1 ) ) );
					SendAllChat("[" + User + "] voted to kick [" + LastMatch->GetName() + "] [1/" + UTIL_ToString((uint32_t)ceil((GetNumHumanPlayers() - 1) * (float)m_GHost->m_VoteKickPercentage / 100) - 1) + "]");
					SendAllChat("RULE: !votekick command may only be used against game ruiners (intentional feed, obstructing gameplay..). Vote accordingly.");
					SendAllChat(m_GHost->m_Language->TypeYesToVote(string(1, m_GHost->m_CommandTrigger)));
				}
			}
			else
				SendChat(player, m_GHost->m_Language->UnableToVoteKickFoundMoreThanOneMatch(Payload));
		}
	}

	//
	// !VOTEKICKCOUNT
	// !YESCOUNT
	//

	else if ((Command == "votekickcount" || Command == "yescount") && m_GHost->m_VoteKickAllowed) {
		if (m_KickVotePlayer.empty())
			SendChat(player, "There is no votekick in progress.");
		else {
			uint32_t VotesNeeded = (uint32_t)ceil((GetNumHumanPlayers() - 1) * (float)m_GHost->m_VoteKickPercentage / 100);
			uint32_t Votes = 0;

			for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
				if ((*i)->GetKickVote())
					++Votes;
			}

			SendChat(player, "[" + UTIL_ToString(Votes) + "/" + UTIL_ToString(VotesNeeded) + "] players voted to kick [" + m_KickVotePlayer + "].");
		}
	}

	//
	// !YES
	//

	else if (Command == "yes" && !m_KickVotePlayer.empty() && player->GetName() != m_KickVotePlayer && !player->GetKickVote()) {
		bool AlreadyVoted = player->GetKickVote();
		player->SetKickVote(true);
		uint32_t VotesNeeded = (uint32_t)ceil((GetNumHumanPlayers() - 1) * (float)m_GHost->m_VoteKickPercentage / 100);
		uint32_t Votes = 0;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			if ((*i)->GetKickVote())
				++Votes;
		}

		if (Votes >= VotesNeeded) {
			CGamePlayer* Victim = GetPlayerFromName(m_KickVotePlayer, true);

			if (Victim) {
				Victim->SetDeleteMe(true);
				Victim->SetLeftReason(m_GHost->m_Language->WasKickedByVote());

				if (!m_GameLoading && !m_GameLoaded)
					Victim->SetLeftCode(PLAYERLEAVE_LOBBY);
				else
					Victim->SetLeftCode(PLAYERLEAVE_LOST);

				if (!m_GameLoading && !m_GameLoaded)
					OpenSlot(GetSIDFromPID(Victim->GetPID()), false);

				CONSOLE_Print("[GAME: " + m_GameName + "] votekick against player [" + m_KickVotePlayer + "] passed with " + UTIL_ToString(Votes) + "/" + UTIL_ToString(GetNumHumanPlayers()) + " votes");
				SendAllChat(m_GHost->m_Language->VoteKickPassed(m_KickVotePlayer));
			}
			else
				SendAllChat(m_GHost->m_Language->ErrorVoteKickingPlayer(m_KickVotePlayer));

			m_KickVotePlayer.clear();
			m_StartedKickVoteTime = 0;
		}
		else {
			// SendAllChat( m_GHost->m_Language->VoteKickAcceptedNeedMoreVotes( m_KickVotePlayer, User, UTIL_ToString( VotesNeeded - Votes ) ) );
			if (AlreadyVoted)
				SendChat(player, "You already voted to kick [" + m_KickVotePlayer + "] [" + UTIL_ToString(Votes) + "/" + UTIL_ToString(VotesNeeded) + "]");
			else
				SendAllChat("[" + User + "] voted to kick [" + m_KickVotePlayer + "] [" + UTIL_ToString(Votes) + "/" + UTIL_ToString(VotesNeeded) + "]");
		}
	}

	//
	// !IGNORE (with payload, see !IGNORELIST for without payload)
	//

	else if (Command == "ignore" && !Payload.empty() && m_GameLoaded) {
		CGamePlayer* LastMatch = NULL;
		uint32_t Matches = GetPlayerFromNamePartial(Payload, &LastMatch);

		if (Matches == 0)
			SendChat(player, "Unable to ignore player [" + Payload + "]. No matches found.");
		else if (Matches == 1) {
			if (player->GetName() != LastMatch->GetName()) {
				SendChat(player, "Player [" + LastMatch->GetName() + "] is now being ignored.");
				player->SetIgnorePID(LastMatch->GetPID());
			}
			else
				SendChat(player, "You can't ignore yourself.");
		}
		else
			SendChat(player, "Unable to ignore player [" + Payload + "]. Found more than one match.");
	}

	//
	// !IGNORE (without payload)
	// !IGNORELIST
	//

	else if ((Command == "ignorelist" || (Command == "ignore" && Payload.empty())) && m_GameLoaded) {
		BYTEARRAY IgnoredPIDs = player->GetIgnoredPIDs();
		string IgnoreList;

		for (unsigned int i = 0; i < IgnoredPIDs.size(); ++i) {
			CGamePlayer* Player = GetPlayerFromPID(IgnoredPIDs[i]);

			if (Player) {
				if (IgnoreList.empty())
					IgnoreList = Player->GetName();
				else
					IgnoreList += ", " + Player->GetName();
			}
		}

		if (IgnoreList.empty())
			SendChat(player, "You are not ignoring anyone.");
		else
			SendChat(player, "You are ignoring: " + IgnoreList);

		string IgnoreByList;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			BYTEARRAY IgnoredByPIDs = (*i)->GetIgnoredPIDs();

			for (unsigned int j = 0; j < IgnoredByPIDs.size(); ++j) {
				if (IgnoredByPIDs[j] == player->GetPID()) {
					if (IgnoreByList.empty())
						IgnoreByList = (*i)->GetName();
					else
						IgnoreByList += ", " + (*i)->GetName();

					break;
				}
			}
		}

		if (IgnoreByList.empty())
			SendChat(player, "Noone is ignoring you.");
		else
			SendChat(player, "You are being ignored by: " + IgnoreByList);
	}

	//
	// !UNIGNORE
	//

	else if (Command == "unignore" && !Payload.empty() && m_GameLoaded) {
		CGamePlayer* LastMatch = NULL;
		uint32_t Matches = GetPlayerFromNamePartial(Payload, &LastMatch);

		if (Matches == 0)
			SendChat(player, "Unable to unignore player [" + Payload + "]. No matches found.");
		else if (Matches == 1) {
			if (player->GetName() != LastMatch->GetName()) {
				if (player->SetUnignorePID(LastMatch->GetPID()))
					SendChat(player, "Player [" + LastMatch->GetName() + "] is no longer ignored.");
				else
					SendChat(player, "Player [" + LastMatch->GetName() + "] was not on your ignore list.");
			}
			else
				SendChat(player, "You can't unignore yourself.");
		}
		else
			SendChat(player, "Unable to unignore player [" + Payload + "]. Found more than one match.");
	}
}

void CBaseGame::EventPlayerChangeTeam(CGamePlayer* player, unsigned char team)
{
	// player is requesting a team change

	if (m_SaveGame)
		return;

	if (m_Map->GetMapOptions() & MAPOPT_CUSTOMFORCES) {
		unsigned char oldSID = GetSIDFromPID(player->GetPID());
		unsigned char newSID = GetEmptySlot(team, player->GetPID());
		SwapSlots(oldSID, newSID);
	}
	else {
		if (team > 12)
			return;

		if (team == 12) {
			if (m_Map->GetMapObservers() != MAPOBS_ALLOWED && m_Map->GetMapObservers() != MAPOBS_REFEREES)
				return;
		}
		else {
			if (team >= m_Map->GetMapNumPlayers())
				return;

			// make sure there aren't too many other players already

			unsigned char NumOtherPlayers = 0;

			for (unsigned char i = 0; i < m_Slots.size(); ++i) {
				if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OCCUPIED && m_Slots[i].GetTeam() != 12 && m_Slots[i].GetPID() != player->GetPID())
					++NumOtherPlayers;
			}

			if (NumOtherPlayers >= m_Map->GetMapNumPlayers())
				return;
		}

		unsigned char SID = GetSIDFromPID(player->GetPID());

		if (SID < m_Slots.size()) {
			m_Slots[SID].SetTeam(team);

			if (team == 12) {
				// if they're joining the observer team give them the observer colour

				m_Slots[SID].SetColour(12);
			}
			else if (m_Slots[SID].GetColour() == 12) {
				// if they're joining a regular team give them an unused colour

				m_Slots[SID].SetColour(GetNewColour());
			}

			SendAllSlotInfo();
		}
	}
}

void CBaseGame::EventPlayerChangeColour(CGamePlayer* player, unsigned char colour)
{
	// player is requesting a colour change

	if (m_SaveGame)
		return;

	if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS)
		return;

	if (colour > 11)
		return;

	unsigned char SID = GetSIDFromPID(player->GetPID());

	if (SID < m_Slots.size()) {
		// make sure the player isn't an observer

		if (m_Slots[SID].GetTeam() == 12)
			return;

		ColourSlot(SID, colour);
	}
}

void CBaseGame::EventPlayerChangeRace(CGamePlayer* player, unsigned char race)
{
	// player is requesting a race change

	if (m_SaveGame)
		return;

	if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS)
		return;

	if (m_Map->GetMapFlags() & MAPFLAG_RANDOMRACES)
		return;

	if (race != SLOTRACE_HUMAN && race != SLOTRACE_ORC && race != SLOTRACE_NIGHTELF && race != SLOTRACE_UNDEAD && race != SLOTRACE_RANDOM)
		return;

	unsigned char SID = GetSIDFromPID(player->GetPID());

	if (SID < m_Slots.size()) {
		m_Slots[SID].SetRace(race | SLOTRACE_SELECTABLE);
		SendAllSlotInfo();
	}
}

void CBaseGame::EventPlayerChangeHandicap(CGamePlayer* player, unsigned char handicap)
{
	// player is requesting a handicap change

	if (m_SaveGame)
		return;

	if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS)
		return;

	if (handicap != 50 && handicap != 60 && handicap != 70 && handicap != 80 && handicap != 90 && handicap != 100)
		return;

	unsigned char SID = GetSIDFromPID(player->GetPID());

	if (SID < m_Slots.size()) {
		m_Slots[SID].SetHandicap(handicap);
		SendAllSlotInfo();
	}
}

void CBaseGame::EventPlayerDropRequest(CGamePlayer* player)
{
	// todotodo: check that we've waited the full 45 seconds

	if (m_Lagging) {
		bool LaggerHasGProxy = false;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			if ((*i)->GetLagging() && (*i)->GetGProxy()) {
				LaggerHasGProxy = true;
				break;
			}
		}

		// don't let players drop the lagger before 2 minutes if he has GProxy

		if (LaggerHasGProxy && GetTime() - m_StartedLaggingTime < 120) {
			CONSOLE_Print("[GAME: " + m_GameName + "] player [" + player->GetName() + "] voted to drop laggers, vote is canceled, lagger is using GProxy");
			player->SetDropVote(false);
			return;
		}

		CONSOLE_Print("[GAME: " + m_GameName + "] player [" + player->GetName() + "] voted to drop laggers");
		SendAllChat(m_GHost->m_Language->PlayerVotedToDropLaggers(player->GetName()));

		// check if at least half the players voted to drop

		uint32_t Votes = 0;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			if ((*i)->GetDropVote())
				++Votes;
		}

		if ((float)Votes / m_Players.size() > 0.49)
			StopLaggers(m_GHost->m_Language->LaggedOutDroppedByVote());
	}
}

void CBaseGame::EventPlayerMapSize(CGamePlayer* player, CIncomingMapSize* mapSize)
{
	if (m_GameLoading || m_GameLoaded)
		return;

	// todotodo: the variable names here are confusing due to extremely poor design on my part

	uint32_t MapSize = UTIL_ByteArrayToUInt32(m_Map->GetMapSize(), false);

	if (mapSize->GetSizeFlag() != 1 || mapSize->GetMapSize() != MapSize) {
		// the player doesn't have the map

		if (m_GHost->m_AllowDownloads) {
			string* MapData = m_Map->GetMapData();

			if (!MapData->empty()) {
				if (!player->GetDownloadStarted() && mapSize->GetSizeFlag() == 1) {
					// inform the client that we are willing to send the map

					CONSOLE_Print("[GAME: " + m_GameName + "] map download started for player [" + player->GetName() + "]");
					Send(player, m_Protocol->SEND_W3GS_STARTDOWNLOAD(GetHostPID()));
					player->SetDownloadStarted(true);
					player->SetStartedDownloadingTicks(GetTicks());
				}
				else
					player->SetLastMapPartAcked(mapSize->GetMapSize());
			}
			else {
				player->SetDeleteMe(true);
				player->SetLeftReason("doesn't have the map and there is no local copy of the map to send");
				player->SetLeftCode(PLAYERLEAVE_LOBBY);
				SendAllChat(player->GetName() + " " + player->GetLeftReason());
				m_GHost->m_Manager->SendGamePlayerLeftLobbyInform(player->GetSpoofedRealm(), player->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_NOMAP);
				OpenSlot(GetSIDFromPID(player->GetPID()), false);
			}
		}
		else {
			player->SetDeleteMe(true);
			player->SetLeftReason("doesn't have the map and map downloads are disabled");
			player->SetLeftCode(PLAYERLEAVE_LOBBY);
			SendAllChat(player->GetName() + " " + player->GetLeftReason());
			m_GHost->m_Manager->SendGamePlayerLeftLobbyInform(player->GetSpoofedRealm(), player->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_NOMAP);
			OpenSlot(GetSIDFromPID(player->GetPID()), false);
		}
	}
	else {
		if (player->GetDownloadStarted() && !player->GetDownloadFinished()) {
			// calculate download rate

			float Seconds = (float)(GetTicks() - player->GetStartedDownloadingTicks()) / 1000;
			float Rate = (float)MapSize / 1024 / Seconds;
			CONSOLE_Print("[GAME: " + m_GameName + "] map download finished for player [" + player->GetName() + "] in " + UTIL_ToString(Seconds, 1) + " seconds");
			SendAllChat(m_GHost->m_Language->PlayerDownloadedTheMap(player->GetName(), UTIL_ToString(Seconds, 1), UTIL_ToString(Rate, 1)));
			player->SetDownloadFinished(true);
			player->SetFinishedDownloadingTime(GetTime());

			// add to database

			// m_GHost->m_Callables.push_back( m_GHost->m_DB->ThreadedDownloadAdd( m_Map->GetMapPath( ), MapSize, player->GetName( ), player->GetExternalIPString( ), player->GetSpoofed( ) ? 1 : 0, player->GetSpoofedRealm( ), GetTicks( ) - player->GetStartedDownloadingTicks( ) ) );
		}
	}

	unsigned char NewDownloadStatus = (unsigned char)((float)mapSize->GetMapSize() / MapSize * 100);
	unsigned char SID = GetSIDFromPID(player->GetPID());

	if (NewDownloadStatus > 100)
		NewDownloadStatus = 100;

	if (SID < m_Slots.size()) {
		// only send the slot info if the download status changed

		if (m_Slots[SID].GetDownloadStatus() != NewDownloadStatus) {
			m_Slots[SID].SetDownloadStatus(NewDownloadStatus);

			// we don't actually send the new slot info here
			// this is an optimization because it's possible for a player to download a map very quickly
			// if we send a new slot update for every percentage change in their download status it adds up to a lot of data
			// instead, we mark the slot info as "out of date" and update it only once in awhile (once per second when this comment was made)

			m_SlotInfoChanged = true;
		}
	}
}

void CBaseGame::EventPlayerPongToHost(CGamePlayer* player, uint32_t pong)
{
	// log ping received for debugging race conditions
	CONSOLE_Print("[GAME: " + m_GameName + "] received pong from [" + player->GetName() + "], NumPings=" + UTIL_ToString(player->GetNumPings()) + ", Spoofed=" + UTIL_ToString(player->GetSpoofed()) + ", GProxyVer=" + UTIL_ToString(player->GetXPAMGProxyVersion()));

	// autokick players with excessive pings but only if they're not reserved and we've received at least 3 pings from them
	// also don't kick anyone if the game is loading or loaded - this could happen because we send pings during loading but we stop sending them after the game is loaded
	// see the Update function for where we send pings

	if (!m_GameLoading && !m_GameLoaded && !player->GetDeleteMe() && !player->GetReserved() && player->GetNumPings() >= 3 && player->GetPing(m_GHost->m_LCPings) > m_GHost->m_AutoKickPing) {
		// send a chat message because we don't normally do so when a player leaves the lobby
		player->SetDeleteMe(true);
		player->SetLeftReason("was autokicked for excessive ping of " + UTIL_ToString(player->GetPing(m_GHost->m_LCPings)));
		player->SetLeftCode(PLAYERLEAVE_LOBBY);
		SendAllChat(player->GetName() + " " + player->GetLeftReason());
		m_GHost->m_Manager->SendGamePlayerLeftLobbyInform(player->GetSpoofedRealm(), player->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_PING);
		OpenSlot(GetSIDFromPID(player->GetPID()), false);
	}

	if (!m_GameLoading && !m_GameLoaded && player->GetSpoofed() && player->GetNumPings() == 2) {
		CBNET* Server = NULL;
		for (vector<CBNET*>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i) {
			if ((*i)->GetServer() == player->GetSpoofedRealm()) {
				Server = *i;
				break;
			}
		}
		if (Server != NULL && Server->GetXPAMGProxyVersion() != 0) {
			if (player->GetXPAMGProxyVersion() != Server->GetXPAMGProxyVersion()) {
				player->SetLeftReason("was kicked for having GProxy version of " + UTIL_ToString(player->GetXPAMGProxyVersion()) + " instead of " + UTIL_ToString(Server->GetXPAMGProxyVersion()));
				SendAllChat(player->GetName() + " " + player->GetLeftReason());
				m_GHost->m_Manager->SendGamePlayerLeftLobbyInform(player->GetSpoofedRealm(), player->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_WRONG_GPROXY);
				OpenSlot(m_GHost->m_CurrentGame->GetSIDFromPID(player->GetPID()), true);
			}
			if (this->m_GHost->m_restrictW3Version && !player->GetGpsW3VersionSent()) {
				player->SetLeftReason("was kicked for wrong W3 version and GProxy (restricted mode)");
				SendAllChat(player->GetName() + " " + player->GetLeftReason());
				m_GHost->m_Manager->SendGamePlayerLeftLobbyInform(player->GetSpoofedRealm(), player->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_WRONG_W3VESION_GPROXY);
				OpenSlot(m_GHost->m_CurrentGame->GetSIDFromPID(player->GetPID()), true);
			}
		}
	}
}

void CBaseGame::EventGameRefreshed(string server)
{
	if (m_RefreshRehosted) {
		// we're not actually guaranteed this refresh was for the rehosted game and not the previous one
		// but since we unqueue game refreshes when rehosting, the only way this can happen is due to network delay
		// it's a risk we're willing to take but can result in a false positive here

		SendAllChat(m_GHost->m_Language->RehostWasSuccessful());
		m_RefreshRehosted = false;
	}
}

void CBaseGame::EventGameStarted()
{
	m_GameStartedLoadingTime = GetTime();
	m_GHost->m_Manager->SendGameStarted(m_GameType, GetServerID(m_CreatorServer), m_GameID, m_GameState, GetNumHumanPlayers(), m_OwnerName, m_GameName, this);
	m_GHost->m_Manager->SendGameGetID(m_GameID);

	m_LobbyDuration = GetTime() - m_TrueCreationTime;

	CONSOLE_Print("[GAME: " + m_GameName + "] started loading with " + UTIL_ToString(GetNumHumanPlayers()) + " players");

	// encode the HCL command string in the slot handicaps
	// here's how it works:
	//  the user inputs a command string to be sent to the map
	//  it is almost impossible to send a message from the bot to the map so we encode the command string in the slot handicaps
	//  this works because there are only 6 valid handicaps but Warcraft III allows the bot to set up to 256 handicaps
	//  we encode the original (unmodified) handicaps in the new handicaps and use the remaining space to store a short message
	//  only occupied slots deliver their handicaps to the map and we can send one character (from a list) per handicap
	//  when the map finishes loading, assuming it's designed to use the HCL system, it checks if anyone has an invalid handicap
	//  if so, it decodes the message from the handicaps and restores the original handicaps using the encoded values
	//  the meaning of the message is specific to each map and the bot doesn't need to understand it
	//  e.g. you could send game modes, # of rounds, level to start on, anything you want as long as it fits in the limited space available
	//  note: if you attempt to use the HCL system on a map that does not support HCL the bot will drastically modify the handicaps
	//  since the map won't automatically restore the original handicaps in this case your game will be ruined

	if (!m_HCLCommandString.empty()) {
		if (m_HCLCommandString.size() <= GetSlotsOccupied()) {
			string HCLChars = "abcdefghijklmnopqrstuvwxyz0123456789 -=,.";

			if (m_HCLCommandString.find_first_not_of(HCLChars) == string ::npos) {
				unsigned char EncodingMap[256];
				unsigned char j = 0;

				for (uint32_t i = 0; i < 256; ++i) {
					// the following 7 handicap values are forbidden

					if (j == 0 || j == 50 || j == 60 || j == 70 || j == 80 || j == 90 || j == 100)
						++j;

					EncodingMap[i] = j++;
				}

				unsigned char CurrentSlot = 0;

				for (string ::iterator si = m_HCLCommandString.begin(); si != m_HCLCommandString.end(); ++si) {
					while (m_Slots[CurrentSlot].GetSlotStatus() != SLOTSTATUS_OCCUPIED)
						++CurrentSlot;

					unsigned char HandicapIndex = (m_Slots[CurrentSlot].GetHandicap() - 50) / 10;
					unsigned char CharIndex = HCLChars.find(*si);
					m_Slots[CurrentSlot++].SetHandicap(EncodingMap[HandicapIndex + CharIndex * 6]);
				}

				SendAllSlotInfo();
				CONSOLE_Print("[GAME: " + m_GameName + "] successfully encoded HCL command string [" + m_HCLCommandString + "]");
			}
			else
				CONSOLE_Print("[GAME: " + m_GameName + "] encoding HCL command string [" + m_HCLCommandString + "] failed because it contains invalid characters");
		}
		else
			CONSOLE_Print("[GAME: " + m_GameName + "] encoding HCL command string [" + m_HCLCommandString + "] failed because there aren't enough occupied slots");
	}

	// send a final slot info update if necessary
	// this typically won't happen because we prevent the !start command from completing while someone is downloading the map
	// however, if someone uses !start force while a player is downloading the map this could trigger
	// this is because we only permit slot info updates to be flagged when it's just a change in download status, all others are sent immediately
	// it might not be necessary but let's clean up the mess anyway

	if (m_SlotInfoChanged)
		SendAllSlotInfo();

	m_StartedLoadingTicks = GetTicks();
	m_LastLagScreenResetTime = GetTime();
	m_GameLoading = true;

	// since we use a fake countdown to deal with leavers during countdown the COUNTDOWN_START and COUNTDOWN_END packets are sent in quick succession
	// send a start countdown packet

	SendAll(m_Protocol->SEND_W3GS_COUNTDOWN_START());

	// remove the virtual host player

	DeleteVirtualHost();

	// send an end countdown packet

	SendAll(m_Protocol->SEND_W3GS_COUNTDOWN_END());

	// send a game loaded packet for the fake player (if present)

	if (m_FakePlayerPID != 255)
		SendAll(m_Protocol->SEND_W3GS_GAMELOADED_OTHERS(m_FakePlayerPID));

	// record the starting number of players

	m_StartPlayers = GetNumHumanPlayers();

	// close the listening socket

	delete m_Socket;
	m_Socket = NULL;

	// delete any potential players that are still hanging around

	for (vector<CPotentialPlayer*>::iterator i = m_Potentials.begin(); i != m_Potentials.end(); ++i)
		delete *i;

	m_Potentials.clear();

	// set initial values for replay

	if (m_Replay) {
		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i)
			m_Replay->AddPlayer((*i)->GetPID(), (*i)->GetName());

		if (m_FakePlayerPID != 255)
			m_Replay->AddPlayer(m_FakePlayerPID, "FakePlayer");

		m_Replay->SetSlots(m_Slots);
		m_Replay->SetRandomSeed(m_RandomSeed);
		m_Replay->SetSelectMode(m_Map->GetMapLayoutStyle());
		m_Replay->SetStartSpotCount(m_Map->GetMapNumPlayers());

		if (m_SaveGame) {
			uint32_t MapGameType = MAPGAMETYPE_SAVEDGAME;

			if (m_GameState == GAME_PRIVATE)
				MapGameType |= MAPGAMETYPE_PRIVATEGAME;

			m_Replay->SetMapGameType(MapGameType);
		}
		else {
			uint32_t MapGameType = m_Map->GetMapGameType();
			MapGameType |= MAPGAMETYPE_UNKNOWN0;

			if (m_GameState == GAME_PRIVATE)
				MapGameType |= MAPGAMETYPE_PRIVATEGAME;

			m_Replay->SetMapGameType(MapGameType);
		}

		if (!m_Players.empty()) {
			// this might not be necessary since we're going to overwrite the replay's host PID and name everytime a player leaves

			m_Replay->SetHostPID(m_Players[0]->GetPID());
			m_Replay->SetHostName(m_Players[0]->GetName());
		}
	}

	// build a stat string for use when saving the replay
	// we have to build this now because the map data is going to be deleted

	BYTEARRAY StatString;
	UTIL_AppendByteArray(StatString, m_Map->GetMapGameFlags());
	StatString.push_back(0);
	UTIL_AppendByteArray(StatString, m_Map->GetMapWidth());
	UTIL_AppendByteArray(StatString, m_Map->GetMapHeight());
	UTIL_AppendByteArray(StatString, m_Map->GetMapCRC());
	UTIL_AppendByteArray(StatString, m_Map->GetMapPath());
	UTIL_AppendByteArray(StatString, "lagabuse.com");
	StatString.push_back(0);
	UTIL_AppendByteArray(StatString, m_Map->GetMapSHA1()); // note: in replays generated by Warcraft III it stores 20 zeros for the SHA1 instead of the real thing
	StatString = UTIL_EncodeStatString(StatString);
	m_StatString = string(StatString.begin(), StatString.end());

	// delete the map data

	delete m_Map;
	m_Map = NULL;

	if (m_LoadInGame) {
		// buffer all the player loaded messages
		// this ensures that every player receives the same set of player loaded messages in the same order, even if someone leaves during loading
		// if someone leaves during loading we buffer the leave message to ensure it gets sent in the correct position but the player loaded message wouldn't get sent if we didn't buffer it now

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			for (vector<CGamePlayer*>::iterator j = m_Players.begin(); j != m_Players.end(); ++j)
				(*j)->AddLoadInGameData(m_Protocol->SEND_W3GS_GAMELOADED_OTHERS((*i)->GetPID()));
		}
	}

	// move the game to the games in progress vector

	m_GHost->m_CurrentGame = NULL;
	m_GHost->m_Games.push_back(this);

	// and finally reenter battle.net chat

	for (vector<CBNET*>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i) {
		(*i)->QueueGameUncreate();
		(*i)->QueueEnterChat();
	}
}

void CBaseGame::EventGameLoaded()
{
	CONSOLE_Print("[GAME: " + m_GameName + "] finished loading with " + UTIL_ToString(GetNumHumanPlayers()) + " players");

	// print very long (longer than 1 minute) loading times to all players

	CGamePlayer* Longest = NULL;

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if (!Longest || (*i)->GetFinishedLoadingTicks() > Longest->GetFinishedLoadingTicks())
			Longest = *i;
	}

	if (Longest && ((Longest->GetFinishedLoadingTicks() - m_StartedLoadingTicks) / 1000) > 60)
		SendAllChat(m_GHost->m_Language->LongestLoadByPlayer(Longest->GetName(), UTIL_ToString((float)(Longest->GetFinishedLoadingTicks() - m_StartedLoadingTicks) / 1000, 2)));

	// send shortest, longest, and personal load times to each player

	/*CGamePlayer *Shortest = NULL;
	CGamePlayer *Longest = NULL;

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); ++i )
	{
		if( !Shortest || (*i)->GetFinishedLoadingTicks( ) < Shortest->GetFinishedLoadingTicks( ) )
			Shortest = *i;

		if( !Longest || (*i)->GetFinishedLoadingTicks( ) > Longest->GetFinishedLoadingTicks( ) )
			Longest = *i;
	}

	if( Shortest && Longest )
	{
		SendAllChat( m_GHost->m_Language->ShortestLoadByPlayer( Shortest->GetName( ), UTIL_ToString( (float)( Shortest->GetFinishedLoadingTicks( ) - m_StartedLoadingTicks ) / 1000, 2 ) ) );
		SendAllChat( m_GHost->m_Language->LongestLoadByPlayer( Longest->GetName( ), UTIL_ToString( (float)( Longest->GetFinishedLoadingTicks( ) - m_StartedLoadingTicks ) / 1000, 2 ) ) );
	}

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); ++i )
		SendChat( *i, m_GHost->m_Language->YourLoadingTimeWas( UTIL_ToString( (float)( (*i)->GetFinishedLoadingTicks( ) - m_StartedLoadingTicks ) / 1000, 2 ) ) );*/

	// read from gameloaded.txt if available

	ifstream in;
	in.open(m_GHost->m_GameLoadedFile.c_str());

	if (!in.fail()) {
		// don't print more than 8 lines

		uint32_t Count = 0;
		string Line;

		while (!in.eof() && Count < 8) {
			getline(in, Line);

			if (Line.empty()) {
				if (!in.eof())
					SendAllChat(" ");
			}
			else
				SendAllChat(Line);

			++Count;
		}

		in.close();
	}

	SendAllChat("Lagabuse Game ID: " + UTIL_ToString(m_GameID));
}

unsigned char CBaseGame::GetSIDFromPID(unsigned char PID)
{
	if (m_Slots.size() > 255)
		return 255;

	for (unsigned char i = 0; i < m_Slots.size(); ++i) {
		if (m_Slots[i].GetPID() == PID)
			return i;
	}

	return 255;
}

CGamePlayer* CBaseGame::GetPlayerFromPID(unsigned char PID)
{
	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if (!(*i)->GetLeftMessageSent() && (*i)->GetPID() == PID)
			return *i;
	}

	return NULL;
}

CGamePlayer* CBaseGame::GetPlayerFromSID(unsigned char SID)
{
	if (SID < m_Slots.size())
		return GetPlayerFromPID(m_Slots[SID].GetPID());

	return NULL;
}

CGamePlayer* CBaseGame::GetPlayerFromName(string name, bool sensitive)
{
	if (!sensitive)
		transform(name.begin(), name.end(), name.begin(), (int (*)(int))tolower);

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if (!(*i)->GetLeftMessageSent()) {
			string TestName = (*i)->GetName();

			if (!sensitive)
				transform(TestName.begin(), TestName.end(), TestName.begin(), (int (*)(int))tolower);

			if (TestName == name)
				return *i;
		}
	}

	return NULL;
}

uint32_t CBaseGame::GetPlayerFromNamePartial(string name, CGamePlayer** player)
{
	transform(name.begin(), name.end(), name.begin(), (int (*)(int))tolower);
	uint32_t Matches = 0;
	*player = NULL;

	// try to match each player with the passed string (e.g. "Varlock" would be matched with "lock")

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if (!(*i)->GetLeftMessageSent()) {
			string TestName = (*i)->GetName();
			transform(TestName.begin(), TestName.end(), TestName.begin(), (int (*)(int))tolower);

			if (TestName.find(name) != string ::npos) {
				++Matches;
				*player = *i;

				// if the name matches exactly stop any further matching

				if (TestName == name) {
					Matches = 1;
					break;
				}
			}
		}
	}

	return Matches;
}

CGamePlayer* CBaseGame::GetPlayerFromColour(unsigned char colour)
{
	for (unsigned char i = 0; i < m_Slots.size(); ++i) {
		if (m_Slots[i].GetColour() == colour)
			return GetPlayerFromSID(i);
	}

	return NULL;
}

unsigned char CBaseGame::GetNewPID()
{
	// find an unused PID for a new player to use

	for (unsigned char TestPID = 1; TestPID < 255; ++TestPID) {
		if (TestPID == m_VirtualHostPID || TestPID == m_FakePlayerPID)
			continue;

		bool InUse = false;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			if (!(*i)->GetLeftMessageSent() && (*i)->GetPID() == TestPID) {
				InUse = true;
				break;
			}
		}

		if (!InUse)
			return TestPID;
	}

	// this should never happen

	return 255;
}

unsigned char CBaseGame::GetNewColour()
{
	// find an unused colour for a player to use

	for (unsigned char TestColour = 0; TestColour < 12; ++TestColour) {
		bool InUse = false;

		for (unsigned char i = 0; i < m_Slots.size(); ++i) {
			if (m_Slots[i].GetColour() == TestColour) {
				InUse = true;
				break;
			}
		}

		if (!InUse)
			return TestColour;
	}

	// this should never happen

	return 12;
}

BYTEARRAY CBaseGame::GetPIDs()
{
	BYTEARRAY result;

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if (!(*i)->GetLeftMessageSent())
			result.push_back((*i)->GetPID());
	}

	return result;
}

BYTEARRAY CBaseGame::GetPIDs(unsigned char excludePID)
{
	BYTEARRAY result;

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if (!(*i)->GetLeftMessageSent() && (*i)->GetPID() != excludePID)
			result.push_back((*i)->GetPID());
	}

	return result;
}

unsigned char CBaseGame::GetHostPID()
{
	// return the player to be considered the host (it can be any player) - mainly used for sending text messages from the bot
	// try to find the virtual host player first

	if (m_VirtualHostPID != 255)
		return m_VirtualHostPID;

	// try to find the fakeplayer next

	if (m_FakePlayerPID != 255)
		return m_FakePlayerPID;

	// try to find the owner player next

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if (!(*i)->GetLeftMessageSent() && IsOwner((*i)->GetName()))
			return (*i)->GetPID();
	}

	// okay then, just use the first available player

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if (!(*i)->GetLeftMessageSent())
			return (*i)->GetPID();
	}

	return 255;
}

unsigned char CBaseGame::GetEmptySlot(bool reserved)
{
	if (m_Slots.size() > 255)
		return 255;

	if (m_SaveGame) {
		// unfortunately we don't know which slot each player was assigned in the savegame
		// but we do know which slots were occupied and which weren't so let's at least force players to use previously occupied slots

		vector<CGameSlot> SaveGameSlots = m_SaveGame->GetSlots();

		for (unsigned char i = 0; i < m_Slots.size(); ++i) {
			if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OPEN && SaveGameSlots[i].GetSlotStatus() == SLOTSTATUS_OCCUPIED && SaveGameSlots[i].GetComputer() == 0)
				return i;
		}

		// don't bother with reserved slots in savegames
	}
	else {
		// look for an empty slot for a new player to occupy
		// if reserved is true then we're willing to use closed or occupied slots as long as it wouldn't displace a player with a reserved slot

		for (unsigned char i = 0; i < m_Slots.size(); ++i) {
			if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OPEN)
				return i;
		}

		if (reserved) {
			// no empty slots, but since player is reserved give them a closed slot

			for (unsigned char i = 0; i < m_Slots.size(); ++i) {
				if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_CLOSED)
					return i;
			}

			// no closed slots either, give them an occupied slot but not one occupied by another reserved player
			// first look for a player who is downloading the map and has the least amount downloaded so far

			unsigned char LeastDownloaded = 100;
			unsigned char LeastSID = 255;

			for (unsigned char i = 0; i < m_Slots.size(); ++i) {
				CGamePlayer* Player = GetPlayerFromSID(i);

				if (Player && !Player->GetReserved() && m_Slots[i].GetDownloadStatus() < LeastDownloaded) {
					LeastDownloaded = m_Slots[i].GetDownloadStatus();
					LeastSID = i;
				}
			}

			if (LeastSID != 255)
				return LeastSID;

			// nobody who isn't reserved is downloading the map, just choose the first player who isn't reserved

			for (unsigned char i = 0; i < m_Slots.size(); ++i) {
				CGamePlayer* Player = GetPlayerFromSID(i);

				if (Player && !Player->GetReserved())
					return i;
			}
		}
	}

	return 255;
}

unsigned char CBaseGame::GetEmptySlot(unsigned char team, unsigned char PID)
{
	if (m_Slots.size() > 255)
		return 255;

	// find an empty slot based on player's current slot

	unsigned char StartSlot = GetSIDFromPID(PID);

	if (StartSlot < m_Slots.size()) {
		if (m_Slots[StartSlot].GetTeam() != team) {
			// player is trying to move to another team so start looking from the first slot on that team
			// we actually just start looking from the very first slot since the next few loops will check the team for us

			StartSlot = 0;
		}

		if (m_SaveGame) {
			vector<CGameSlot> SaveGameSlots = m_SaveGame->GetSlots();

			for (unsigned char i = StartSlot; i < m_Slots.size(); ++i) {
				if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OPEN && m_Slots[i].GetTeam() == team && SaveGameSlots[i].GetSlotStatus() == SLOTSTATUS_OCCUPIED && SaveGameSlots[i].GetComputer() == 0)
					return i;
			}

			for (unsigned char i = 0; i < StartSlot; ++i) {
				if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OPEN && m_Slots[i].GetTeam() == team && SaveGameSlots[i].GetSlotStatus() == SLOTSTATUS_OCCUPIED && SaveGameSlots[i].GetComputer() == 0)
					return i;
			}
		}
		else {
			// find an empty slot on the correct team starting from StartSlot

			for (unsigned char i = StartSlot; i < m_Slots.size(); ++i) {
				if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OPEN && m_Slots[i].GetTeam() == team)
					return i;
			}

			// didn't find an empty slot, but we could have missed one with SID < StartSlot
			// e.g. in the DotA case where I am in slot 4 (yellow), slot 5 (orange) is occupied, and slot 1 (blue) is open and I am trying to move to another slot

			for (unsigned char i = 0; i < StartSlot; ++i) {
				if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OPEN && m_Slots[i].GetTeam() == team)
					return i;
			}
		}
	}

	return 255;
}

void CBaseGame::SwapSlots(unsigned char SID1, unsigned char SID2)
{
	if (SID1 < m_Slots.size() && SID2 < m_Slots.size() && SID1 != SID2) {
		CGameSlot Slot1 = m_Slots[SID1];
		CGameSlot Slot2 = m_Slots[SID2];

		if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
			// don't swap the team, colour, race, or handicap
			m_Slots[SID1] = CGameSlot(Slot2.GetPID(), Slot2.GetDownloadStatus(), Slot2.GetSlotStatus(), Slot2.GetComputer(), Slot1.GetTeam(), Slot1.GetColour(), Slot1.GetRace(), Slot2.GetComputerType(), Slot1.GetHandicap());
			m_Slots[SID2] = CGameSlot(Slot1.GetPID(), Slot1.GetDownloadStatus(), Slot1.GetSlotStatus(), Slot1.GetComputer(), Slot2.GetTeam(), Slot2.GetColour(), Slot2.GetRace(), Slot1.GetComputerType(), Slot2.GetHandicap());
		}
		else {
			// swap everything

			if (m_Map->GetMapOptions() & MAPOPT_CUSTOMFORCES) {
				// except if custom forces is set, then we don't swap teams...
				Slot1.SetTeam(m_Slots[SID2].GetTeam());
				Slot2.SetTeam(m_Slots[SID1].GetTeam());
			}

			m_Slots[SID1] = Slot2;
			m_Slots[SID2] = Slot1;
		}

		SendAllSlotInfo();
	}
}

void CBaseGame::OpenSlot(unsigned char SID, bool kick)
{
	if (SID < m_Slots.size()) {
		if (kick) {
			CGamePlayer* Player = GetPlayerFromSID(SID);

			if (Player) {
				Player->SetDeleteMe(true);
				if (Player->GetLeftReason().empty()) // so we can set custom left reasons out of this function
				{
					Player->SetLeftReason("was kicked when opening a slot");
				}
				Player->SetLeftCode(PLAYERLEAVE_LOBBY);
			}
		}

		CGameSlot Slot = m_Slots[SID];
		m_Slots[SID] = CGameSlot(0, 255, SLOTSTATUS_OPEN, 0, Slot.GetTeam(), Slot.GetColour(), Slot.GetRace());
		SendAllSlotInfo();
	}
}

void CBaseGame::CloseSlot(unsigned char SID, bool kick)
{
	if (SID < m_Slots.size()) {
		if (kick) {
			CGamePlayer* Player = GetPlayerFromSID(SID);

			if (Player) {
				Player->SetDeleteMe(true);
				Player->SetLeftReason("was kicked when closing a slot");
				Player->SetLeftCode(PLAYERLEAVE_LOBBY);
			}
		}

		CGameSlot Slot = m_Slots[SID];
		m_Slots[SID] = CGameSlot(0, 255, SLOTSTATUS_CLOSED, 0, Slot.GetTeam(), Slot.GetColour(), Slot.GetRace());
		SendAllSlotInfo();
	}
}

void CBaseGame::ComputerSlot(unsigned char SID, unsigned char skill, bool kick)
{
	if (SID < m_Slots.size() && skill < 3) {
		if (kick) {
			CGamePlayer* Player = GetPlayerFromSID(SID);

			if (Player) {
				Player->SetDeleteMe(true);
				Player->SetLeftReason("was kicked when creating a computer in a slot");
				Player->SetLeftCode(PLAYERLEAVE_LOBBY);
			}
		}

		CGameSlot Slot = m_Slots[SID];
		m_Slots[SID] = CGameSlot(0, 100, SLOTSTATUS_OCCUPIED, 1, Slot.GetTeam(), Slot.GetColour(), Slot.GetRace(), skill);
		SendAllSlotInfo();
	}
}

void CBaseGame::ColourSlot(unsigned char SID, unsigned char colour)
{
	if (SID < m_Slots.size() && colour < 12) {
		// make sure the requested colour isn't already taken

		bool Taken = false;
		unsigned char TakenSID = 0;

		for (unsigned char i = 0; i < m_Slots.size(); ++i) {
			if (m_Slots[i].GetColour() == colour) {
				TakenSID = i;
				Taken = true;
			}
		}

		if (Taken && m_Slots[TakenSID].GetSlotStatus() != SLOTSTATUS_OCCUPIED) {
			// the requested colour is currently "taken" by an unused (open or closed) slot
			// but we allow the colour to persist within a slot so if we only update the existing player's colour the unused slot will have the same colour
			// this isn't really a problem except that if someone then joins the game they'll receive the unused slot's colour resulting in a duplicate
			// one way to solve this (which we do here) is to swap the player's current colour into the unused slot

			m_Slots[TakenSID].SetColour(m_Slots[SID].GetColour());
			m_Slots[SID].SetColour(colour);
			SendAllSlotInfo();
		}
		else if (!Taken) {
			// the requested colour isn't used by ANY slot

			m_Slots[SID].SetColour(colour);
			SendAllSlotInfo();
		}
	}
}

void CBaseGame::OpenAllSlots()
{
	bool Changed = false;

	for (vector<CGameSlot>::iterator i = m_Slots.begin(); i != m_Slots.end(); ++i) {
		if ((*i).GetSlotStatus() == SLOTSTATUS_CLOSED) {
			(*i).SetSlotStatus(SLOTSTATUS_OPEN);
			Changed = true;
		}
	}

	if (Changed)
		SendAllSlotInfo();
}

void CBaseGame::CloseAllSlots()
{
	bool Changed = false;

	for (vector<CGameSlot>::iterator i = m_Slots.begin(); i != m_Slots.end(); ++i) {
		if ((*i).GetSlotStatus() == SLOTSTATUS_OPEN) {
			(*i).SetSlotStatus(SLOTSTATUS_CLOSED);
			Changed = true;
		}
	}

	if (Changed)
		SendAllSlotInfo();
}

void CBaseGame::ShuffleSlots()
{
	// we only want to shuffle the player slots
	// that means we need to prevent this function from shuffling the open/closed/computer slots too
	// so we start by copying the player slots to a temporary vector

	vector<CGameSlot> PlayerSlots;
	static std::mt19937 rng(std::random_device{}());

	for (vector<CGameSlot>::iterator i = m_Slots.begin(); i != m_Slots.end(); ++i) {
		if ((*i).GetSlotStatus() == SLOTSTATUS_OCCUPIED && (*i).GetComputer() == 0 && (*i).GetTeam() != 12)
			PlayerSlots.push_back(*i);
	}

	// now we shuffle PlayerSlots

	if (m_Map->GetMapOptions() & MAPOPT_CUSTOMFORCES) {
		// rather than rolling our own probably broken shuffle algorithm we use random_shuffle because it's guaranteed to do it properly
		// so in order to let random_shuffle do all the work we need a vector to operate on
		// unfortunately we can't just use PlayerSlots because the team/colour/race shouldn't be modified
		// so make a vector we can use

		vector<unsigned char> SIDs;

		for (unsigned char i = 0; i < PlayerSlots.size(); ++i)
			SIDs.push_back(i);

		std::shuffle(SIDs.begin(), SIDs.end(), rng);

		// now put the PlayerSlots vector in the same order as the SIDs vector

		vector<CGameSlot> Slots;

		// as usual don't modify the team/colour/race

		for (unsigned char i = 0; i < SIDs.size(); ++i)
			Slots.push_back(CGameSlot(PlayerSlots[SIDs[i]].GetPID(), PlayerSlots[SIDs[i]].GetDownloadStatus(), PlayerSlots[SIDs[i]].GetSlotStatus(), PlayerSlots[SIDs[i]].GetComputer(), PlayerSlots[i].GetTeam(), PlayerSlots[i].GetColour(), PlayerSlots[i].GetRace()));

		PlayerSlots = Slots;
	}
	else {
		// regular game
		// it's easy when we're allowed to swap the team/colour/race!

		std::shuffle(PlayerSlots.begin(), PlayerSlots.end(), rng);
	}

	// now we put m_Slots back together again

	vector<CGameSlot>::iterator CurrentPlayer = PlayerSlots.begin();
	vector<CGameSlot> Slots;

	for (vector<CGameSlot>::iterator i = m_Slots.begin(); i != m_Slots.end(); ++i) {
		if ((*i).GetSlotStatus() == SLOTSTATUS_OCCUPIED && (*i).GetComputer() == 0 && (*i).GetTeam() != 12) {
			Slots.push_back(*CurrentPlayer);
			++CurrentPlayer;
		}
		else
			Slots.push_back(*i);
	}

	m_Slots = Slots;

	// and finally tell everyone about the new slot configuration

	SendAllSlotInfo();
}

vector<unsigned char> CBaseGame::BalanceSlotsRecursive(vector<unsigned char> PlayerIDs, unsigned char* TeamSizes, double* PlayerScores, unsigned char StartTeam)
{
	// take a brute force approach to finding the best balance by iterating through every possible combination of players
	// 1.) since the number of teams is arbitrary this algorithm must be recursive
	// 2.) on the first recursion step every possible combination of players into two "teams" is checked, where the first team is the correct size and the second team contains everyone else
	// 3.) on the next recursion step every possible combination of the remaining players into two more "teams" is checked, continuing until all the actual teams are accounted for
	// 4.) for every possible combination, check the largest difference in total scores between any two actual teams
	// 5.) minimize this value by choosing the combination of players with the smallest difference

	vector<unsigned char> BestOrdering = PlayerIDs;
	double BestDifference = -1.0;

	for (unsigned char i = StartTeam; i < 12; ++i) {
		if (TeamSizes[i] > 0) {
			unsigned char Mid = TeamSizes[i];

			// the base case where only one actual team worth of players was passed to this function is handled by the behaviour of next_combination
			// in this case PlayerIDs.begin( ) + Mid will actually be equal to PlayerIDs.end( ) and next_combination will return false

			while (next_combination(PlayerIDs.begin(), PlayerIDs.begin() + Mid, PlayerIDs.end())) {
				// we're splitting the players into every possible combination of two "teams" based on the midpoint Mid
				// the first (left) team contains the correct number of players but the second (right) "team" might or might not
				// for example, it could contain one, two, or more actual teams worth of players
				// so recurse using the second "team" as the full set of players to perform the balancing on

				vector<unsigned char> BestSubOrdering = BalanceSlotsRecursive(vector<unsigned char>(PlayerIDs.begin() + Mid, PlayerIDs.end()), TeamSizes, PlayerScores, i + 1);

				// BestSubOrdering now contains the best ordering of all the remaining players (the "right team") given this particular combination of players into two "teams"
				// in order to calculate the largest difference in total scores we need to recombine the subordering with the first team

				vector<unsigned char> TestOrdering = vector<unsigned char>(PlayerIDs.begin(), PlayerIDs.begin() + Mid);
				TestOrdering.insert(TestOrdering.end(), BestSubOrdering.begin(), BestSubOrdering.end());

				// now calculate the team scores for all the teams that we know about (e.g. on subsequent recursion steps this will NOT be every possible team)

				vector<unsigned char>::iterator CurrentPID = TestOrdering.begin();
				double TeamScores[12];

				for (unsigned char j = StartTeam; j < 12; ++j) {
					TeamScores[j] = 0.0;

					for (unsigned char k = 0; k < TeamSizes[j]; ++k) {
						TeamScores[j] += PlayerScores[*CurrentPID];
						++CurrentPID;
					}
				}

				// find the largest difference in total scores between any two teams

				double LargestDifference = 0.0;

				for (unsigned char j = StartTeam; j < 12; ++j) {
					if (TeamSizes[j] > 0) {
						for (unsigned char k = j + 1; k < 12; ++k) {
							if (TeamSizes[k] > 0) {
								double Difference = abs(TeamScores[j] - TeamScores[k]);

								if (Difference > LargestDifference)
									LargestDifference = Difference;
							}
						}
					}
				}

				// and minimize it

				if (BestDifference < 0.0 || LargestDifference < BestDifference) {
					BestOrdering = TestOrdering;
					BestDifference = LargestDifference;
				}
			}
		}
	}

	return BestOrdering;
}

void CBaseGame::AddToSpoofed(string server, string name, bool sendMessage)
{
	CGamePlayer* Player = GetPlayerFromName(name, true);

	if (Player) {
		// log spoof check completion for debugging race conditions
		CONSOLE_Print("[GAME: " + m_GameName + "] spoof check completed for [" + Player->GetName() + "], NumPings=" + UTIL_ToString(Player->GetNumPings()) + ", GProxyVer=" + UTIL_ToString(Player->GetXPAMGProxyVersion()));

		Player->SetSpoofedRealm(server);
		Player->SetSpoofed(true);
		if (Player->GetNumPings() >= 2) {
			CBNET* Server = NULL;
			for (vector<CBNET*>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i) {
				if ((*i)->GetServer() == server) {
					Server = *i;
					break;
				}
			}
			if (Server != NULL && Server->GetXPAMGProxyVersion() != 0) {
				if (Player->GetXPAMGProxyVersion() != Server->GetXPAMGProxyVersion()) {
					Player->SetLeftReason("was kicked for having GProxy version of " + UTIL_ToString(Player->GetXPAMGProxyVersion()) + " instead of " + UTIL_ToString(Server->GetXPAMGProxyVersion()));
					SendAllChat(Player->GetName() + " " + Player->GetLeftReason());
					m_GHost->m_Manager->SendGamePlayerLeftLobbyInform(Player->GetSpoofedRealm(), Player->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_WRONG_GPROXY);
					OpenSlot(m_GHost->m_CurrentGame->GetSIDFromPID(Player->GetPID()), true);
				}
			}
		}

		m_LobbyLog->AddMessage("Spoof check accepted for [" + name + " : " + server + "].");

		if (sendMessage) {
			if (Player->GetSpoofedNoticeSent()) {
				for (vector<CBNET*>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i) {
					if (Player->GetJoinedRealm() == (*i)->GetServer()) {
						if ((*i)->GetPasswordHashType() == "rgc")
							SendChat(Player, "You are already spoof checked");
						else
							SendChat(Player, "You are already spoof checked [ /w " + (*i)->GetBnetUserName() + " sc ]");

						break;
					}
				}
			}
			else
				SendAllChat(m_GHost->m_Language->SpoofCheckAcceptedFor(server, name));
		}

		Player->SetSpoofedNoticeSent(true);
	}
	else {
		// Player not found - possibly a race condition where spoof check arrived before player was added to m_Players
		CONSOLE_Print("[GAME: " + m_GameName + "] WARNING: spoof check received for [" + name + "] but player not found in game - possible race condition");
	}
}

void CBaseGame::AddToReserved(string name)
{
	transform(name.begin(), name.end(), name.begin(), (int (*)(int))tolower);

	// check that the user is not already reserved

	for (vector<string>::iterator i = m_Reserved.begin(); i != m_Reserved.end(); ++i) {
		if (*i == name)
			return;
	}

	m_Reserved.push_back(name);

	// upgrade the user if they're already in the game

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		string NameLower = (*i)->GetName();
		transform(NameLower.begin(), NameLower.end(), NameLower.begin(), (int (*)(int))tolower);

		if (NameLower == name)
			(*i)->SetReserved(true);
	}
}

void CBaseGame::ChangeOwner(string name)
{
	m_OwnerName = name;

	// patch, if we don't set the new owner to the reserved list and there is no other reserved player in lobby the game
	// will hit m_LobbyTimeLimit and be auto unhosted because there is no reserved players in the lobby

	AddToReserved(m_OwnerName);

	m_GHost->m_Manager->SendGameOwnerChanged(m_GameID, m_OwnerName);
}

bool CBaseGame::IsOwner(string name)
{
	string OwnerLower = m_OwnerName;
	transform(name.begin(), name.end(), name.begin(), (int (*)(int))tolower);
	transform(OwnerLower.begin(), OwnerLower.end(), OwnerLower.begin(), (int (*)(int))tolower);
	return name == OwnerLower;
}

bool CBaseGame::IsReserved(string name)
{
	transform(name.begin(), name.end(), name.begin(), (int (*)(int))tolower);

	for (vector<string>::iterator i = m_Reserved.begin(); i != m_Reserved.end(); ++i) {
		if (*i == name)
			return true;
	}

	return false;
}

bool CBaseGame::IsDownloading()
{
	// returns true if at least one player is downloading the map

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if ((*i)->GetDownloadStarted() && !(*i)->GetDownloadFinished())
			return true;
	}

	return false;
}

bool CBaseGame::IsGameDataSaved()
{
	return true;
}

void CBaseGame::SaveGameData()
{
}

void CBaseGame::StartCountDown(bool force)
{
	if (!m_CountDownStarted) {
		// check if everyone is spoof checked and has MH protection

		string NotSpoofChecked;
		string NotPinged;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); i++) {
			if (!(*i)->GetSpoofed()) {
				if (NotSpoofChecked.empty())
					NotSpoofChecked = (*i)->GetName();
				else
					NotSpoofChecked += ", " + (*i)->GetName();
			}
		}

		if (!NotSpoofChecked.empty()) {
			SendAllChat(m_GHost->m_Language->PlayersNotYetSpoofChecked(NotSpoofChecked));

			// tell everyone how to spoof check manually

			for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); i++) {
				if (!(*i)->GetSpoofed()) {
					for (vector<CBNET*>::iterator j = m_GHost->m_BNETs.begin(); j != m_GHost->m_BNETs.end(); j++) {
						if ((*i)->GetJoinedRealm() == (*j)->GetServer())
							SendChat((*i), "Spoof check by whispering bot with \"sc\" [ /w " + (*j)->GetBnetUserName() + " sc ]");
					}
				}
			}
		}

		// check if everyone has been pinged enough (3 times) that the autokicker would have kicked them by now
		// see function EventPlayerPongToHost for the autokicker code

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			if ((*i)->GetNumPings() < 3) {
				if (NotPinged.empty())
					NotPinged = (*i)->GetName();
				else
					NotPinged += ", " + (*i)->GetName();
			}
		}

		if (!NotPinged.empty())
			SendAllChat(m_GHost->m_Language->PlayersNotYetPinged(NotPinged));

		if (force) {
			// if everyone is spoof checked start the game

			if (NotSpoofChecked.empty() && NotPinged.empty()) {
				if (!m_HCLCommandString.empty())
					SendAllChat("Game is starting, game mode is set to [" + m_HCLCommandString + "].");

				m_CountDownStarted = true;
				m_CountDownCounter = 10;
			}
		}
		else {
			// check if the HCL command string is short enough

			if (m_HCLCommandString.size() > GetSlotsOccupied()) {
				SendAllChat(m_GHost->m_Language->TheHCLIsTooLongUseForceToStart());
				return;
			}

			// check if everyone has the map

			string StillDownloading;

			for (vector<CGameSlot>::iterator i = m_Slots.begin(); i != m_Slots.end(); ++i) {
				if ((*i).GetSlotStatus() == SLOTSTATUS_OCCUPIED && (*i).GetComputer() == 0 && (*i).GetDownloadStatus() != 100) {
					CGamePlayer* Player = GetPlayerFromPID((*i).GetPID());

					if (Player) {
						if (StillDownloading.empty())
							StillDownloading = Player->GetName();
						else
							StillDownloading += ", " + Player->GetName();
					}
				}
			}

			if (!StillDownloading.empty())
				SendAllChat(m_GHost->m_Language->PlayersStillDownloading(StillDownloading));

			if (StillDownloading.empty() && NotPinged.empty() && NotSpoofChecked.empty()) {
				if (!m_HCLCommandString.empty())
					SendAllChat("Game is starting, game mode is set to [" + m_HCLCommandString + "].");

				m_CountDownStarted = true;
				m_CountDownCounter = 10;
			}
		}
	}
}

void CBaseGame::StartCountDownNow()
{
	if (!m_CountDownStarted) {
		m_CountDownStarted = true;
		m_CountDownCounter = 0;
	}
}

void CBaseGame::StartCountDownAuto(bool requireSpoofChecks)
{
	if (!m_CountDownStarted) {
		// check that all held players are present
		int hpCount = 0;
		vector<string> missingPlayers;

		for (auto i = m_Reserved.begin(); i != m_Reserved.end(); ++i) {
			string pname = (*i);
			auto itFind = find_if(m_Players.begin(), m_Players.end(), [&pname](CGamePlayer* obj) {
				string lowerName = obj->GetName();
				transform(lowerName.begin(), lowerName.end(), lowerName.begin(), (int (*)(int))tolower);
				return lowerName == pname;
			});

			if (itFind != m_Players.end()) {
				hpCount++;
			}
			else {
				missingPlayers.push_back((*i));
			}
		}

		if (hpCount < m_Reserved.size()) {
			std::ostringstream missingPlayersSS;
			std::copy(missingPlayers.begin(), missingPlayers.end(), std::ostream_iterator<string>(missingPlayersSS, " "));
			SendAllChat("Waiting for players: " + missingPlayersSS.str());

			// Check if time limit is up, unhost and ban the slackers
			int secLeft = m_GHost->m_AutoStartHeldPlayersMaxTimeInMin * 60 - (GetTime() - m_TrueCreationTime);
			if (secLeft < 0) {
				// Ban
				for (auto i = missingPlayers.begin(); i != missingPlayers.end(); ++i) {
					// m_GHost->m_Manager->SendUserWasBanned(player->GetJoinedRealm(), player->GetName(), m_MySQLGameID, string());
					CONSOLE_Print("Banned " + (*i));
				}

				// Unhost
				m_Exiting = true;
			}
			else if (secLeft < 11) {
				SendAllChat(UTIL_ToString(secLeft) + " seconds left for all players to join. Game will be unhosted and missing players banned.");
			}
			else {
				SendAllChat(UTIL_ToString(secLeft) + " seconds left for all players to join. Balance will be performed before start.");
			}
			return;
		}

		CONSOLE_Print("All held players are present for autostart");

		// check if enough players are present

		if (GetNumHumanPlayers() < m_AutoStartPlayers) {
			SendAllChat(m_GHost->m_Language->WaitingForPlayersBeforeAutoStart(UTIL_ToString(m_AutoStartPlayers), UTIL_ToString(m_AutoStartPlayers - GetNumHumanPlayers())));
			return;
		}

		// check if everyone has the map

		string StillDownloading;

		for (vector<CGameSlot>::iterator i = m_Slots.begin(); i != m_Slots.end(); ++i) {
			if ((*i).GetSlotStatus() == SLOTSTATUS_OCCUPIED && (*i).GetComputer() == 0 && (*i).GetDownloadStatus() != 100) {
				CGamePlayer* Player = GetPlayerFromPID((*i).GetPID());

				if (Player) {
					if (StillDownloading.empty())
						StillDownloading = Player->GetName();
					else
						StillDownloading += ", " + Player->GetName();
				}
			}
		}

		if (!StillDownloading.empty()) {
			SendAllChat(m_GHost->m_Language->PlayersStillDownloading(StillDownloading));
			return;
		}

		// check if everyone is spoof checked

		string NotSpoofChecked;

		if (requireSpoofChecks) {
			for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
				if (!(*i)->GetSpoofed()) {
					if (NotSpoofChecked.empty())
						NotSpoofChecked = (*i)->GetName();
					else
						NotSpoofChecked += ", " + (*i)->GetName();
				}
			}

			if (!NotSpoofChecked.empty())
				SendAllChat(m_GHost->m_Language->PlayersNotYetSpoofChecked(NotSpoofChecked));
		}

		// check if everyone has been pinged enough (3 times) that the autokicker would have kicked them by now
		// see function EventPlayerPongToHost for the autokicker code

		string NotPinged;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
			if (!(*i)->GetReserved() && (*i)->GetNumPings() < 3) {
				if (NotPinged.empty())
					NotPinged = (*i)->GetName();
				else
					NotPinged += ", " + (*i)->GetName();
			}
		}

		if (!NotPinged.empty()) {
			SendAllChat(m_GHost->m_Language->PlayersNotYetPingedAutoStart(NotPinged));
			return;
		}

		// if no problems found start the game

		if (StillDownloading.empty() && NotSpoofChecked.empty() && NotPinged.empty()) {
			m_CountDownStarted = true;
			m_CountDownCounter = 10;
		}

		// Print current slot state
		for (auto s = m_Slots.begin(); s != m_Slots.end(); ++s) {
			CONSOLE_Print(UTIL_ToString(s->GetPID()) + " " + UTIL_ToString(s->GetComputer()) + " " + UTIL_ToString(s->GetSlotStatus()));
		}

		// balance the slots in the order how the players were held
		if (!m_Reserved.empty() && m_Reserved.size() <= m_Slots.size()) {
			vector<CGameSlot> newPlayerSlots;
			vector<string> reservedCopy = m_Reserved;

			int slotCnt = 1;
			for (auto s = m_Slots.begin(); s != m_Slots.end(); ++s) {
				if (s->GetSlotStatus() == SLOTSTATUS_OCCUPIED && s->GetComputer() == 0) {
					if (!reservedCopy.empty()) {
						// Get player from front
						bool matchFound = false;
						string nextPlayerName = reservedCopy.front();
						CGamePlayer* nextPlayer = GetPlayerFromName(nextPlayerName, false);

						// Find matching slot
						for (auto ss = m_Slots.begin(); ss != m_Slots.end(); ++ss) {
							if (ss->GetPID() == nextPlayer->GetPID()) {
								newPlayerSlots.push_back((*ss));
								CONSOLE_Print("Moving player " + nextPlayerName + " with PID " + UTIL_ToString(nextPlayer->GetPID()) + " to slot " + UTIL_ToString(slotCnt));
								matchFound = true;
							}
						}
						reservedCopy.erase(reservedCopy.begin()); // erase first el
						if (!matchFound) {
							newPlayerSlots.push_back((*s));
						}
					}
					else {
						newPlayerSlots.push_back((*s));
					}
				}
				else {
					newPlayerSlots.push_back((*s));
				}
				slotCnt++;
			}

			m_Slots = newPlayerSlots;

			// Print new slot state
			for (auto s = m_Slots.begin(); s != m_Slots.end(); ++s) {
				CONSOLE_Print(UTIL_ToString(s->GetPID()) + " " + UTIL_ToString(s->GetComputer()) + " " + UTIL_ToString(s->GetSlotStatus()));
			}

			// tell everyone about the new slot configuration
			SendAllSlotInfo();
		}
		else {
			CONSOLE_Print("WARN: Balance was not performed because size of reserved was " + UTIL_ToString(m_Reserved.size()) + " and size of slots was " + UTIL_ToString(m_Slots.size()));
		}
	}
}

void CBaseGame::StopPlayers(string reason)
{
	// disconnect every player and set their left reason to the passed string
	// we use this function when we want the code in the Update function to run before the destructor (e.g. saving players to the database)
	// therefore calling this function when m_GameLoading || m_GameLoaded is roughly equivalent to setting m_Exiting = true
	// the only difference is whether the code in the Update function is executed or not

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		(*i)->SetDeleteMe(true);
		(*i)->SetLeftReason(reason);
		(*i)->SetLeftCode(PLAYERLEAVE_LOST);
	}
}

void CBaseGame::StopLaggers(string reason)
{
	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		if ((*i)->GetLagging()) {
			(*i)->SetDeleteMe(true);
			(*i)->SetLeftReason(reason);
			(*i)->SetLeftCode(PLAYERLEAVE_DISCONNECT);
		}
	}
}

void CBaseGame::CreateVirtualHost()
{
	if (m_VirtualHostPID != 255)
		return;

	m_VirtualHostPID = GetNewPID();
	BYTEARRAY IP;
	IP.push_back(0);
	IP.push_back(0);
	IP.push_back(0);
	IP.push_back(0);
	SendAll(m_Protocol->SEND_W3GS_PLAYERINFO(m_VirtualHostPID, m_VirtualHostName, IP, IP));
}

void CBaseGame::DeleteVirtualHost()
{
	if (m_VirtualHostPID == 255)
		return;

	SendAll(m_Protocol->SEND_W3GS_PLAYERLEAVE_OTHERS(m_VirtualHostPID, PLAYERLEAVE_LOBBY));
	m_VirtualHostPID = 255;
}

void CBaseGame::CreateFakePlayer()
{
	if (m_FakePlayerPID != 255)
		return;

	unsigned char SID = GetEmptySlot(false);

	if (SID < m_Slots.size()) {
		if (GetNumPlayers() >= 11)
			DeleteVirtualHost();

		m_FakePlayerPID = GetNewPID();
		BYTEARRAY IP;
		IP.push_back(0);
		IP.push_back(0);
		IP.push_back(0);
		IP.push_back(0);
		SendAll(m_Protocol->SEND_W3GS_PLAYERINFO(m_FakePlayerPID, "FakePlayer", IP, IP));
		m_Slots[SID] = CGameSlot(m_FakePlayerPID, 100, SLOTSTATUS_OCCUPIED, 0, m_Slots[SID].GetTeam(), m_Slots[SID].GetColour(), m_Slots[SID].GetRace());
		SendAllSlotInfo();
	}
}

void CBaseGame::DeleteFakePlayer()
{
	if (m_FakePlayerPID == 255)
		return;

	for (unsigned char i = 0; i < m_Slots.size(); ++i) {
		if (m_Slots[i].GetPID() == m_FakePlayerPID)
			m_Slots[i] = CGameSlot(0, 255, SLOTSTATUS_OPEN, 0, m_Slots[i].GetTeam(), m_Slots[i].GetColour(), m_Slots[i].GetRace());
	}

	SendAll(m_Protocol->SEND_W3GS_PLAYERLEAVE_OTHERS(m_FakePlayerPID, PLAYERLEAVE_LOBBY));
	SendAllSlotInfo();
	m_FakePlayerPID = 255;
}

void CBaseGame::SetFromCodes(string name, uint32_t serverID, string from, string longFrom)
{
	transform(name.begin(), name.end(), name.begin(), (int (*)(int))tolower);

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		string Name = (*i)->GetName();
		transform(Name.begin(), Name.end(), Name.begin(), (int (*)(int))tolower);

		if (Name == name) {
			(*i)->SetFrom(from);
			(*i)->SetLongFrom(longFrom);

			break;
		}
	}
}

void CBaseGame::selectOneGameEventReporter()
{

	CONSOLE_Print("Selecting the game log event sender.");

	if (!this->m_Players.empty()) {
		vector<CGamePlayer*> playersLeft;
		std::copy_if(this->m_Players.begin(), this->m_Players.end(), std::back_inserter(playersLeft), [](CGamePlayer* p) { return !p->GetDeleteMe(); });

		if (!playersLeft.empty()) {
			vector<CGamePlayer*>::iterator randIt = playersLeft.begin();
			std::advance(randIt, std::rand() % playersLeft.size());

			for (auto p : this->m_Players) {
				p->SetSelectedForGameLog(false);
			}

			(*randIt)->SetSelectedForGameLog(true);
			CONSOLE_Print("Selected new player as event reporter: " + (*randIt)->GetName());
		}
	}
	else {
		CONSOLE_Print("Nobody selected for game log event sending since player list is empty.");
	}
}