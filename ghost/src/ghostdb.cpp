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
#include "ghostdb.h"
#include "game_base.h"
#include "gameplayer.h"

//
// CDBPlayer
//

CDBPlayer :: ~CDBPlayer( )
{
	delete m_GPS;
	delete m_Div1DPS;
}

//
// CDotAPlayer
//

CDotAPlayer :: CDotAPlayer( CBaseGame *nGame, unsigned char nPID, string nName, uint32_t nServerID ) : m_Game( nGame ), m_PID( nPID ), m_LobbyColor( 255 ), m_NewColor( 255 ), m_Level( 1 ), m_Kills( 0 ), m_Deaths( 0 ), m_CreepKills( 0 ), m_CreepDenies( 0 ), m_Assists( 0 ), m_Gold( 0 ), m_NeutralKills( 0 ), m_TowerKills( 0 ), m_RaxKills( 0 ), m_CourierKills( 0 ), m_ServerID( nServerID ), m_Name( nName ), m_Hero( string( ) ), m_HasLeftTheGame( false ), m_ItemsSent( false )
{
	m_Items[0] = string( );
	m_Items[1] = string( );
	m_Items[2] = string( );
	m_Items[3] = string( );
	m_Items[4] = string( );
	m_Items[5] = string( );
}

CDotAPlayer :: ~CDotAPlayer( )
{

}

unsigned char CDotAPlayer :: GetLobbyTeam( )
{
	if( m_LobbyColor == 255 )
	{
		// game is still in lobby
		const unsigned char SID = m_Game->GetSIDFromPID( m_PID );

		if( SID < m_Game->m_Slots.size( ) )
			return m_Game->m_Slots[SID].GetTeam( );
	}
	else if( m_LobbyColor >= 1 && m_LobbyColor <= 5 )
	{
		// sentinel player

		return 0;
	}
	else if( m_LobbyColor >= 7 && m_LobbyColor <= 11 )
	{
		// scourge player

		return 1;
	}

	// observer
	return 12;
}

unsigned char CDotAPlayer :: GetLobbyColor( )
{
	if( m_LobbyColor == 255 )
	{
		// game is still in lobby
		const unsigned char SID = m_Game->GetSIDFromPID( m_PID );
		
		if( SID < m_Game->m_Slots.size( ) )
			return m_Game->m_Slots[SID].GetColour( );
	}

	return m_LobbyColor;
}

unsigned char CDotAPlayer :: GetCurrentTeam( )
{
	if( m_NewColor == 255 )
	{
		// game is still in lobby
		const unsigned char SID = m_Game->GetSIDFromPID( m_PID );

		if( SID < m_Game->m_Slots.size( ) )
			return m_Game->m_Slots[SID].GetTeam( );
	}
	else if( m_NewColor >= 1 && m_NewColor <= 5 )
	{
		// sentinel player
		return 0;
	}
	else if( m_NewColor >= 7 && m_NewColor <= 11 )
	{
		// scourge player
		return 1;
	}

	// observer
	return 12;
}

unsigned char CDotAPlayer :: GetCurrentColor( )
{
	if( m_NewColor == 255 )
	{
		// game is still in lobby

		const unsigned char SID = m_Game->GetSIDFromPID( m_PID );
		
		if( SID < m_Game->m_Slots.size( ) )
			return m_Game->m_Slots[SID].GetColour( );
	}

	return m_NewColor;
}

string CDotAPlayer :: GetItem( unsigned int i )
{
	if( i < 6 )
		return m_Items[i];

	return string( );
}

void CDotAPlayer :: SetItem( unsigned int i, string item )
{
	if( i < 6 )
		m_Items[i] = item;
}

void CDotAPlayer :: PickUpItem( string item )
{
	/*for( unsigned int i = 0; i < 6; ++i )
	{
		if( m_Items[i].empty( ) )
		{
			m_Items[i] = item;
			return;
		}
	}*/

	uint32_t Seconds = GetTime( ) - m_Game->m_GameLoadedTime;

	m_ItemsLog += "1 " + UTIL_ToString( Seconds ) + " " + item + "\n";
}

void CDotAPlayer :: DropItem( string item )
{
	/*for( unsigned int i = 0; i < 6; ++i )
	{
		if( m_Items[i] == item )
		{
			m_Items[i] = string( );
			return;
		}
	}*/

	uint32_t Seconds = GetTime( ) - m_Game->m_GameLoadedTime;

	m_ItemsLog += "2 " + UTIL_ToString( Seconds ) + " " + item + "\n";
}

//Vote based setters
void CDotAPlayer::setNewColorV(CGamePlayer *reporter, unsigned char nColor) {
	m_NewColorV[reporter->GetPID()] = nColor;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_NewColorV);
	SetNewColor(newValue);
}

void CDotAPlayer::setLevelV(CGamePlayer *reporter, uint32_t reportedValue) {
	m_LevelV[reporter->GetPID()] = reportedValue;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_LevelV);
	SetLevel(newValue);
}

void CDotAPlayer::setCreepKillsV(CGamePlayer *reporter, uint32_t reportedValue) {
	m_CreepKillsV[reporter->GetPID()] = reportedValue;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_CreepKillsV);
	SetCreepKills(newValue);
}

void CDotAPlayer::setCreepDeniesV(CGamePlayer *reporter, uint32_t reportedValue) {
	m_CreepDeniesV[reporter->GetPID()] = reportedValue;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_CreepDeniesV);
	SetCreepDenies(newValue);
}

void CDotAPlayer::setNeutralKillsV(CGamePlayer *reporter, uint32_t reportedValue) {
	m_NeutralKillsV[reporter->GetPID()] = reportedValue;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_NeutralKillsV);
	SetNeutralKills(newValue);
}

void CDotAPlayer::setKillsV(CGamePlayer *reporter, uint32_t reportedValue) {
	m_KillsV[reporter->GetPID()] = reportedValue;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_KillsV);
	SetKills(newValue);
}

void CDotAPlayer::setDeathsV(CGamePlayer *reporter, uint32_t reportedValue) {
	m_DeathsV[reporter->GetPID()] = reportedValue;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_DeathsV);
	SetDeaths(newValue);
}

void CDotAPlayer::setAssistsV(CGamePlayer *reporter, uint32_t reportedValue) {
	m_AssistsV[reporter->GetPID()] = reportedValue;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_AssistsV);
	SetAssists(newValue);
}


void CDotAPlayer::addKillsV(CGamePlayer *reporter) {
	m_KillsV[reporter->GetPID()]++;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_KillsV);
	SetKills(newValue);
}

void CDotAPlayer::addDeathsV(CGamePlayer *reporter) {
	m_DeathsV[reporter->GetPID()]++;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_DeathsV);
	SetDeaths(newValue);
}

void CDotAPlayer::addAssistsV(CGamePlayer *reporter) {
	m_AssistsV[reporter->GetPID()]++;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_AssistsV);
	SetAssists(newValue);
}

void CDotAPlayer::addTowerKillsV(CGamePlayer *reporter) {
	m_TowerKillsV[reporter->GetPID()]++;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_TowerKillsV);
	SetTowerKills(newValue);
}

void CDotAPlayer::addRaxKillsV(CGamePlayer *reporter) {
	m_RaxKillsV[reporter->GetPID()]++;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_RaxKillsV);
	SetRaxKills(newValue);
}

void CDotAPlayer::addCourierKillsV(CGamePlayer *reporter) {
	m_CourierKillsV[reporter->GetPID()]++;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_CourierKillsV);
	SetCourierKills(newValue);
}