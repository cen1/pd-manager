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

#ifndef GAME_CUSTOMDOTA_H
#define GAME_CUSTOMDOTA_H

//
// CCustomDotAGame
//

class CDBDotAPlayer;
class CDotAPlayer;

class CCustomDotAGame : public CBaseGame
{
protected:
	list<CDotAPlayer *> m_DotAPlayers;
	string m_Mode1;
	string m_Mode2;
	uint32_t m_Winner;
	uint32_t m_Min;
	uint32_t m_Sec;
	uint32_t m_CreepsSpawnedTime;
	uint32_t m_TreeLowHPTime;
	uint32_t m_ThroneLowHPTime;
	bool m_CreepsSpawned;
	bool m_TreeLowHP;
	bool m_ThroneLowHP;

	// CollectDotAStats variables describe the time when game result was determined (when !rmk or !ff passed, tree/throne destroyed, game became a draw because of the leaver)

	bool m_CollectDotAStats;					// if this is false, DotA stats will be parsed but no more stats will be saved in DotA player objects
	uint32_t m_CollectDotAStatsOverTime;		// GetTime when we stoped collecting DotA stats

public:
	CCustomDotAGame( CGHost *nGHost, CMap *nMap, CSaveGame *nSaveGame, uint16_t nHostPort, unsigned char nGameState, string nGameName, string nOwnerName, string nCreatorName, string nCreatorServer );
	virtual ~CCustomDotAGame( );

	virtual bool EventPlayerAction( CGamePlayer *player, CIncomingAction *action );
	virtual void EventPlayerBotCommand2( CGamePlayer *player, string command, string payload, bool rootadmin, bool admin, bool owner );
	virtual void EventGameStarted( );
	virtual void EventGameLoaded();

	virtual bool IsGameDataSaved( );
	virtual void SaveGameData( );

	CDotAPlayer *GetDotAPlayerFromPID( unsigned char PID );
	CDotAPlayer *GetDotAPlayerFromLobbyColor( unsigned char color );
};

#endif
