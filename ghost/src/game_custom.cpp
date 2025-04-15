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
#include "ghostdb.h"
#include "bnet.h"
#include "map.h"
#include "packed.h"
#include "savegame.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "game_base.h"
#include "game_custom.h"
#include "masl_manager.h"
#include "masl_protocol_2.h"

#include <cmath>
#include <string.h>
#include <time.h>

//
// CCustomGame
//

CCustomGame :: CCustomGame( CGHost *nGHost, CMap *nMap, CSaveGame *nSaveGame, uint16_t nHostPort, unsigned char nGameState, string nGameName, string nOwnerName, string nCreatorName, string nCreatorServer ) 
	: CBaseGame( nGHost, nMap, nSaveGame, nHostPort, nGameState, nGameName, nOwnerName, nCreatorName, nCreatorServer, MASL_PROTOCOL :: DB_CUSTOM_GAME )
{
	m_GameCommandTrigger = '.';
}

CCustomGame :: ~CCustomGame( )
{

}

bool CCustomGame :: IsGameDataSaved( )
{
	return m_Saved;
}

void CCustomGame :: SaveGameData( )
{
	CONSOLE_Print( "[GAME: " + m_GameName + "] saving game data" );

	time_t Now = time( NULL );
	char Time[17];
	memset( Time, 0, sizeof( char ) * 17 );
	strftime( Time, sizeof( char ) * 17, "%Y-%m-%d %H-%M", localtime( &Now ) );
	string MinString = UTIL_ToString( ( m_GameTicks / 1000 ) / 60 );
	string SecString = UTIL_ToString( ( m_GameTicks / 1000 ) % 60 );

	if( MinString.size( ) == 1 )
		MinString.insert( 0, "0" );

	if( SecString.size( ) == 1 )
		SecString.insert( 0, "0" );

	m_ReplayName = UTIL_FileSafeName( string( Time ) + " " + m_GameName + " (" + MinString + "m" + SecString + "s).w3g" );

	string LobbyLog;

	if( m_LobbyLog )
		LobbyLog = m_LobbyLog->GetGameLog( );

	string GameLog;

	if( m_GameLog )
		GameLog = m_GameLog->GetGameLog( );

	m_GHost->m_Manager->SendGameSave( m_MySQLGameID, m_LobbyDuration, LobbyLog, GameLog, m_ReplayName, m_MapPath, m_GameName, m_OwnerName, m_GameTicks / 1000, m_GameState, m_CreatorName, m_CreatorServer, m_DBGamePlayers, m_RMK, m_GameType, string( ) );
	
	m_Saved = true;
}
