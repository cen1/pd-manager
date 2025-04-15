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
#include "bnet.h"
#include "util.h"
#include "config.h"
#include "language.h"
#include "socket.h"
#include "commandpacket.h"
#include "ghostdb.h"
#include "bncsutilinterface.h"
#include "bnetprotocol.h"
#include "masl_slavebot.h"

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

using namespace boost :: filesystem;

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
// CLoadedMap
//

CLoadedMap :: CLoadedMap( string nName, string nMap, uint32_t nLoadedTime )
{
	m_Name = nName;
	m_Map = nMap;
	m_LoadedTime = nLoadedTime;
}

CLoadedMap :: ~CLoadedMap( )
{

}

//
// CRemoteGamePlayer
//

CRemoteGamePlayer :: CRemoteGamePlayer( string nName )
{
	m_Name = nName;
	m_LeftTime = 0;
}

CRemoteGamePlayer :: ~CRemoteGamePlayer( )
{

}

//
// CRemoteGame
//

CRemoteGame :: CRemoteGame( uint32_t nBotID, uint32_t nGameID, uint32_t nCreatorServerID, uint32_t nGameState, string nOwnerName, uint32_t nNumSlots, string nGameName, string nCFGFile, uint32_t nGameType )
{
	m_BotID = nBotID;
	m_GameID = nGameID;
	m_GameType = nGameType;
	m_CreatorServerID = nCreatorServerID;
	m_GameState = nGameState;
	m_StartTime = GetTime( );
	m_NumStartPlayers = nNumSlots;
	m_GameName = nGameName;
	m_CFGFile = nCFGFile;
	m_OwnerName = nOwnerName;
	m_InLobby = true;

	for( uint32_t i = 0; i < 12; i++ )
		m_Players[i] = NULL;
}

CRemoteGame :: CRemoteGame( uint32_t nBotID, uint32_t nGameID, uint32_t nCreatorServerID, uint32_t nGameState, string nOwnerName, uint32_t nNumStartPlayers, string nSlotInfo, string nGameName, string nCFGFile, uint32_t nGameType )
{
	m_BotID = nBotID;
	m_GameID = nGameID;
	m_GameType = nGameType;
	m_CreatorServerID = nCreatorServerID;
	m_GameState = nGameState;
	m_StartTime = GetTime( );
	m_NumStartPlayers = nNumStartPlayers;
	m_GameName = nGameName;
	m_CFGFile = nCFGFile;
	m_OwnerName = nOwnerName;
	m_InLobby = false;

	for( uint32_t i = 0; i < 12; i++ )
		m_Players[i] = NULL;

	stringstream SS;
	transform( nSlotInfo.begin( ), nSlotInfo.end( ), nSlotInfo.begin( ), (int(*)(int))tolower );
	SS << nSlotInfo;

	bool ReadSlot = true;

	while( !SS.eof( ) )
	{
		uint32_t Slot;
		string Name;

		if( ReadSlot )
		{
			SS >> Slot;
			ReadSlot = false;
		}
		else
		{
			SS >> Name;
			ReadSlot = true;
			SetGamePlayer( Slot, Name );
		}
	}
}

CRemoteGame :: ~CRemoteGame( )
{
	for( uint32_t i = 0; i < 12; i++ )
	{
		if( m_Players[i] )
			delete m_Players[i];
	}
}

uint32_t CRemoteGame :: GetNumPlayers( )
{
	uint32_t Num = 0;

	for( uint32_t i = 0; i < 12; i++ )
	{
		if( m_Players[i] && m_Players[i]->GetLeftTime( ) == 0 )
			Num++;
	}

	return Num;
}

string CRemoteGame :: GetDescription( )
{
	/*
	if( m_InLobby )
		return ( m_GameName + " : " + m_OwnerName );
	else
		return ( m_GameName + " : " + m_OwnerName + " : " + UTIL_ToString( m_CurrentPlayers ) + "/" + UTIL_ToString( m_StartPlayers ) + " : " + UTIL_ToString( ( GetTime( ) - m_StartTime ) / 60 ) + "m" );
	*/

	return m_GameName + " : " + m_OwnerName + " : " + UTIL_ToString( GetNumPlayers( ) ) + "/" + UTIL_ToString( m_NumStartPlayers ) + " : " + UTIL_ToString( ( GetTime( ) - m_StartTime ) / 60 ) + "m" + ( m_CFGFile.empty( ) ? "" : " : " + m_CFGFile );
}

CRemoteGamePlayer *CRemoteGame :: GetGamePlayer( uint32_t slot )
{
	if( slot != 0 && slot <= 12 )
		return m_Players[slot-1];
	else
		return NULL;
}

CRemoteGamePlayer *CRemoteGame :: GetGamePlayer( string lowercasename )
{
	for( uint32_t i = 0; i < 12; i++ )
	{
		if( m_Players[i] != NULL && m_Players[i]->GetName( ) == lowercasename )
			return m_Players[i];
	}

	return NULL;
}

string CRemoteGame :: GetNames( )
{
	string Names = string( );

	if( m_InLobby )
	{
		for( uint32_t i = 0; i < 12; i++ )
		{
			if( m_Players[i] != NULL )
				Names += " " + m_Players[i]->GetName( );
		}

		if( Names.empty( ) )
			Names = "There are no players in lobby [" + m_GameName + "]";
		else {
			string legacyMap = "";
			CONSOLE_Print(this->GetCFGFile());
			if (this->GetCFGFile().find("map_legacy")!=string::npos) {
				legacyMap = "(v6) ";
			}
			Names = legacyMap+"[" + UTIL_ToString(GetNumPlayers()) + "/" + UTIL_ToString(m_NumStartPlayers) + " : " + m_GameName + "]" + Names;
		}
	}
	else
	{
		for( uint32_t i = 0; i < 12; i++ )
		{
			if( m_Players[i] != NULL )
			{
				if( m_Players[i]->GetLeftTime( ) )
					Names += " " + UTIL_ToString( i + 1 ) + ". " + m_Players[i]->GetName( ) + "(" + UTIL_ToString( m_Players[i]->GetLeftTime( ) / 60 ) + "m)";
				else
					Names += " " + UTIL_ToString( i + 1 ) + ". " + m_Players[i]->GetName( );
			}
		}

		if( Names.empty( ) )
			Names = "There are no players in game [" + m_GameName + "]";
		else
			Names = "[" + UTIL_ToString( GetNumPlayers( ) ) + "/" + UTIL_ToString( m_NumStartPlayers ) + " : " + m_GameName + "]" + Names;
	}

	return Names;
}

void CRemoteGame :: SetGamePlayer( uint32_t slot, string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	if( slot != 0 && slot <= 12 )
	{
		slot = slot - 1;

		m_Players[slot] = new CRemoteGamePlayer( name );
	}
}

void CRemoteGame :: SetGamePlayer( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( uint32_t i = 0; i < 12; i++ )
	{
		if( m_Players[i] == NULL )
		{
			m_Players[i] = new CRemoteGamePlayer( name );
			return;
		}
	}
}

void CRemoteGame :: DelGamePlayer( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( uint32_t i = 0; i < 12; i++ )
	{
		if( m_Players[i] && m_Players[i]->GetName( ) == name )
		{
			delete m_Players[i];
			m_Players[i] = NULL;
			return;
		}
	}
}


void CRemoteGame :: SetPlayerLeft( string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );

	for( uint32_t i = 0; i < 12; i++ )
	{
		if( m_Players[i] != NULL )
		{
			// save minutes when player left
			if( m_Players[i]->GetName( ) == name )
			{
				m_Players[i]->SetLeftTime( ( GetTime( ) - m_StartTime ) / 60 );
				break;
			}
		}
	}
}
