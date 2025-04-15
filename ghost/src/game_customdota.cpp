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
#include "game_customdota.h"
#include "masl_manager.h"
#include "masl_protocol_2.h"

#include <cmath>
#include <string.h>
#include <time.h>

//
// CCustomDotAGame
//

CCustomDotAGame :: CCustomDotAGame( CGHost *nGHost, CMap *nMap, CSaveGame *nSaveGame, uint16_t nHostPort, unsigned char nGameState, string nGameName, string nOwnerName, string nCreatorName, string nCreatorServer ) 
	: CBaseGame( nGHost, nMap, nSaveGame, nHostPort, nGameState, nGameName, nOwnerName, nCreatorName, nCreatorServer, MASL_PROTOCOL :: DB_CUSTOM_DOTA_GAME )
{
	CONSOLE_Print( "[STATSDOTA] using dota stats" );

	m_GameCommandTrigger = '.';
	
	m_Winner = 0;
	m_Min = 0;
	m_Sec = 0;
	m_CreepsSpawnedTime = 0;
	m_TreeLowHPTime = 0;
	m_ThroneLowHPTime = 0;
	m_CreepsSpawned = false;
	m_TreeLowHP = false;
	m_ThroneLowHP = false;
	m_CollectDotAStats = true;
	m_CollectDotAStatsOverTime = 0;
}

CCustomDotAGame :: ~CCustomDotAGame( )
{
	for( list<CDotAPlayer *> :: iterator i = m_DotAPlayers.begin( ); i != m_DotAPlayers.end( ); ++i )
		delete *i;
}

bool CCustomDotAGame :: EventPlayerAction( CGamePlayer *player, CIncomingAction *action )
{
	bool success = CBaseGame :: EventPlayerAction( player, action );
	if (!success) return false;
	// give the stats class a chance to process the action

	unsigned int i = 0;
	BYTEARRAY *ActionData = action->GetAction( );
	BYTEARRAY Data;
	BYTEARRAY Key;
	BYTEARRAY Value;

	// dota actions with real time replay data start with 0x6b then the null terminated string "dr.x"
	// unfortunately more than one action can be sent in a single packet and the length of each action isn't explicitly represented in the packet
	// so we have to either parse all the actions and calculate the length based on the type or we can search for an identifying sequence
	// parsing the actions would be more correct but would be a lot more difficult to write for relatively little gain
	// so we take the easy route (which isn't always guaranteed to work) and search the data for the sequence "6b 64 72 2e 78 00" and hope it identifies an action

	while( ActionData->size( ) >= i + 6 )
	{
		if( (*ActionData)[i] == 0x6b && (*ActionData)[i + 1] == 0x64 && (*ActionData)[i + 2] == 0x72 && (*ActionData)[i + 3] == 0x2e && (*ActionData)[i + 4] == 0x78 && (*ActionData)[i + 5] == 0x00 )
		{
			// we think we've found an action with real time replay data (but we can't be 100% sure)
			// next we parse out two null terminated strings and a 4 byte integer

			if( ActionData->size( ) >= i + 7 )
			{
				// the first null terminated string should either be the strings "Data" or "Global" or a player id in ASCII representation, e.g. "1" or "2"

				Data = UTIL_ExtractCString( *ActionData, i + 6 );

				if( ActionData->size( ) >= i + 8 + Data.size( ) )
				{
					// the second null terminated string should be the key

					Key = UTIL_ExtractCString( *ActionData, i + 7 + Data.size( ) );

					if( ActionData->size( ) >= i + 12 + Data.size( ) + Key.size( ) )
					{
						// the 4 byte integer should be the value

						Value = BYTEARRAY( ActionData->begin( ) + i + 8 + Data.size( ) + Key.size( ), ActionData->begin( ) + i + 12 + Data.size( ) + Key.size( ) );
						string DataString = string( Data.begin( ), Data.end( ) );
						string KeyString = string( Key.begin( ), Key.end( ) );
						uint32_t ValueInt = UTIL_ByteArrayToUInt32( Value, false );

						//CONSOLE_Print( "[STATS] " + DataString + ", " + KeyString + ", " + UTIL_ToString( ValueInt ) );
						//CONSOLE_Print( "[STATS] " + DataString + ", " + KeyString + ", " + UTIL_ToString( ValueInt ) );

						if( DataString == "Data" )
						{
							// these are received during the game
							// you could use these to calculate killing sprees and double or triple kills (you'd have to make up your own time restrictions though)
							// you could also build a table of "who killed who" data

							if( KeyString.size( ) >= 5 && KeyString.substr( 0, 4 ) == "Hero" )
							{
								// a hero died
								// Example:
								// DataString = "Data"
								// KeyString = "Hero3"
								// ValueInt = 10
								// Player with colour 10, has killed player with colour 3.

								string VictimColourString = KeyString.substr( 4 );
								uint32_t VictimColour = UTIL_ToUInt32( VictimColourString );

								// ValueInt can be more than 11 in some cases (ValueInt is 12 when player gets killed by neutral creeps)
								// if the game is with observers, observer player could have color 12 and GetPlayerFromColour could return observer player
								// observer can't be a killer

								CDotAPlayer *Killer = GetDotAPlayerFromLobbyColor( ValueInt );
								CDotAPlayer *Victim = GetDotAPlayerFromLobbyColor( VictimColour );

								if( Victim && !Victim->GetHasLeftTheGame( ) && m_CollectDotAStats )
									Victim->SetDeaths( Victim->GetDeaths( ) + 1 );

								if( ValueInt == 0 )
								{
									// the Sentinel has killed a player

									if( Victim )
									{
										CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] the Sentinel killed player [" + Victim->GetName( ) + "]" );
										m_GameLog->AddMessage( "The Sentinel killed " + Victim->GetName( ) );
									}
								}
								else if( ValueInt == 6 )
								{
									// the Scourge has killed a player

									if( Victim )
									{
										CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] the Scourge killed player [" + Victim->GetName( ) + "]" );
										m_GameLog->AddMessage( "The Scourge killed " + Victim->GetName( ) );
									}
								}
								else if( ValueInt == 12 )
								{
									// Neutral creeps have killed a player

									if( Victim )
									{
										CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] Neutral creeps killed player [" + Victim->GetName( ) + "]" );
										m_GameLog->AddMessage( "Neutral creeps killed " + Victim->GetName( ) );
									}
								}
								else
								{
									// player has killed a player

									if( Killer && Victim )
									{
										if( !Killer->GetHasLeftTheGame( ) && !Victim->GetHasLeftTheGame( ) )
										{
											// both players are still in the game

											if ( ValueInt == VictimColour )
											{
												// the player has killed himself

												CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] player [" + Killer->GetName( ) + "] killed himself." );
												m_GameLog->AddMessage( Killer->GetName( ) + " killed himself" );
											}
											else if( Killer->GetCurrentTeam( ) == Victim->GetCurrentTeam( ) )
											{
												// a player has denied a hero

												CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] player [" + Killer->GetName( ) + "] denied player [" + Victim->GetName( ) + "]" );
												m_GameLog->AddMessage( Killer->GetName( ) + " denied " + Victim->GetName( ) );
											}
											else
											{
												// a player has killed an enemy hero

												CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] player [" + Killer->GetName( ) + "] killed player [" + Victim->GetName( ) + "]" );
												m_GameLog->AddMessage( Killer->GetName( ) + " killed " + Victim->GetName( ) );

												if( m_CollectDotAStats )
													Killer->SetKills( Killer->GetKills( ) + 1 );
											}
										}
										else if( !Victim->GetHasLeftTheGame( ) )
										{
											// leaver has killed a player
										}
										else if( !Killer->GetHasLeftTheGame( ) )
										{
											// player has killed a leaver
										}
									}
								}
							}
							else if( KeyString.size( ) >= 7 && KeyString.substr( 0, 6 ) == "Assist" )
							{
								// an assist has been made
								// Example:
								// DataString = "Data"
								// KeyString = "Assist8"
								// ValueInt = 3
								// Player, with colour 8, has assisted in the death of a player with colour 3

								string AssisterColourString = KeyString.substr( 6 );
								uint32_t AssisterColour = UTIL_ToUInt32( AssisterColourString );

								CDotAPlayer *Assister = GetDotAPlayerFromLobbyColor( AssisterColour );

								if( Assister && !Assister->GetHasLeftTheGame( ) && m_CollectDotAStats )
									Assister->SetAssists( Assister->GetAssists( ) + 1 );
							}
							else if( KeyString.size( ) >= 6 && KeyString.substr( 0, 5 ) == "Level" )
							{
								// hero got a new level
								// Example:
								// DataString = "Data"
								// KeyString = "Level2"
								// ValueInt = 1
								// Player, with colour 1, has got a lavel 2 on his hero

								string LevelString = KeyString.substr( 5 );
								uint32_t Level = UTIL_ToUInt32( LevelString );

								CDotAPlayer *Player = GetDotAPlayerFromLobbyColor( ValueInt );

								if( Player && !Player->GetHasLeftTheGame( ) && m_CollectDotAStats )
									Player->SetLevel( Level );
							}
							else if( KeyString.size( ) >= 5 && KeyString.substr( 0, 3 ) == "PUI" )
							{
								// item has been picked up
								// Example:
								// DataString = "Data"
								// KeyString = "PUI_3"
								// ValueInt = 1227895385
								// Player with colour 3 has picked up item with code 1227895385

								/*string PlayerColourString = KeyString.substr( 4 );
								uint32_t PlayerColour = UTIL_ToUInt32( PlayerColourString );

								CDotAPlayer *Player = GetDotAPlayerFromLobbyColor( PlayerColour );

								if( Player && !Player->GetHasLeftTheGame( ) && m_CollectDotAStats )
									Player->PickUpItem( string( Value.rbegin( ), Value.rend( ) ) );

								if( Player )
									CONSOLE_Print( Player->GetName( ) + "->PickUpItem( " + string( Value.rbegin( ), Value.rend( ) ) + " )" );*/
							}
							else if( KeyString.size( ) >= 5 && KeyString.substr( 0, 3 ) == "DRI" )
							{
								// item has been dropped
								// Example:
								// DataString = "Data"
								// KeyString = "DRI_3"
								// ValueInt = 1227895385
								// Player with colour 3 has dropped item with code 1227895385

								/*string PlayerColourString = KeyString.substr( 4 );
								uint32_t PlayerColour = UTIL_ToUInt32( PlayerColourString );

								CDotAPlayer *Player = GetDotAPlayerFromLobbyColor( PlayerColour );

								if( Player && !Player->GetHasLeftTheGame( ) && m_CollectDotAStats )
									Player->DropItem( string( Value.rbegin( ), Value.rend( ) ) );

								if( Player )
									CONSOLE_Print( Player->GetName( ) + "->DropItem( " + string( Value.rbegin( ), Value.rend( ) ) + " )" );*/
							}
							else if( KeyString.size( ) >= 8 && KeyString.substr( 0, 7 ) == "Courier" )
							{
								// a courier died
								// Example:
								// DataString = "Data"
								// KeyString = "Courier10"
								// ValueInt = 3
								// Player, with colour 3, has killed a courier, that is bought by a player with colour 10.

								string VictimColourString = KeyString.substr( 7 );
								uint32_t VictimColour = UTIL_ToUInt32( VictimColourString );
								CDotAPlayer *Killer = GetDotAPlayerFromLobbyColor( ValueInt );
								CDotAPlayer *Victim = GetDotAPlayerFromLobbyColor( VictimColour );

								if( Killer && !Killer->GetHasLeftTheGame( ) && m_CollectDotAStats )
									Killer->SetCourierKills( Killer->GetCourierKills( ) + 1 );

								if( ValueInt == 0 )
								{
									if( Victim )
									{
										CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] the Sentinel killed a courier owned by player [" + Victim->GetName( ) + "]" );
										m_GameLog->AddMessage( "The Sentinel killed " + Victim->GetName( ) + "'s courier" );
									}
								}
								else if( ValueInt == 6 )
								{
									if( Victim )
									{
										CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] the Scourge killed a courier owned by player [" + Victim->GetName( ) + "]" );
										m_GameLog->AddMessage( "The Scourge killed " + Victim->GetName( ) + "'s courier" );
									}
								}
								else if( ValueInt == 12 )
								{
									if( Victim )
									{
										CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] Neutral creeps killed a courier owned by player [" + Victim->GetName( ) + "]" );
										m_GameLog->AddMessage( "Neutral creeps killed " + Victim->GetName( ) + "'s courier" );
									}
								}
								else
								{
									if( Killer && Victim )
									{
										CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] player [" + Killer->GetName( ) + "] killed a courier owned by player [" + Victim->GetName( ) + "]" );
										m_GameLog->AddMessage( Killer->GetName( ) + " killed " + Victim->GetName( ) + "'s courier" );
									}
								}
							}
							else if( KeyString.size( ) >= 8 && KeyString.substr( 0, 5 ) == "Tower" )
							{
								// a tower died
								// Example:
								// DataString = "Data"
								// KeyString = "Tower111"
								// ValueInt = 5
								// Player, with colour 5, has destroyed the middle level 1 scourge tower
								// first digit - Sentinel (0) or Scourge (1)
								// second digit - Level of tower (1, 2, 3 or 4)
								// third digit - Top (0), Middle (1) or Bottom (2)

								string Alliance = KeyString.substr( 5, 1 );
								string Level = KeyString.substr( 6, 1 );
								string Side = KeyString.substr( 7, 1 );
								CDotAPlayer *Killer = GetDotAPlayerFromLobbyColor( ValueInt );
								string AllianceString;
								string SideString;

								if( Alliance == "0" )
									AllianceString = "Sentinel";
								else if( Alliance == "1" )
									AllianceString = "Scourge";
								else
									AllianceString = "unknown";

								if( Side == "0" )
									SideString = "top";
								else if( Side == "1" )
									SideString = "mid";
								else if( Side == "2" )
									SideString = "bottom";
								else
									SideString = "unknown";

								if( ValueInt == 0 )
								{
									CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] the Sentinel destroyed a level [" + Level + "] " + AllianceString + " tower (" + SideString + ")" );
									m_GameLog->AddMessage( "The Sentinel destroyed the " + SideString + " level " + Level + " " + AllianceString + " tower" );
								}
								else if( ValueInt == 6 )
								{
									CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] the Scourge destroyed a level [" + Level + "] " + AllianceString + " tower (" + SideString + ")" );
									m_GameLog->AddMessage( "The Scourge destroyed the " + SideString + " level " + Level + " " + AllianceString + " tower" );
								}
								else
								{
									if( Killer )
									{
										if( UTIL_ToUInt32( Alliance ) != Killer->GetCurrentTeam( ) )
										{
											// a player has killed an enemy tower

											if( !Killer->GetHasLeftTheGame( ) && m_CollectDotAStats )
												Killer->SetTowerKills( Killer->GetTowerKills( ) + 1 );
									
											CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] player [" + Killer->GetName( ) + "] destroyed a level [" + Level + "] " + AllianceString + " tower (" + SideString + ")" );
											m_GameLog->AddMessage( Killer->GetName( ) + " destroyed the " + SideString + " level " + Level + " " + AllianceString + " tower" );
										}
										else
										{
											// a player has denied an ally tower

											CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] player [" + Killer->GetName( ) + "] denied a level [" + Level + "] " + AllianceString + " tower (" + SideString + ")" );
											m_GameLog->AddMessage( Killer->GetName( ) + " denied the " + SideString + " level " + Level + " " + AllianceString + " tower" );
										}
									}
								}
							}
							else if( KeyString.size( ) >= 6 && KeyString.substr( 0, 3 ) == "Rax" )
							{
								// a rax died
								// Example:
								// DataString = "Data"
								// KeyString = "Rax111"
								// ValueInt = 3
								// A player, with colour 3, has destroyed the Middle Scourge Ranged rax
								// first digit - Sentinel (0) or Scourge (1)
								// second digit - Top (0), Middle (1) or Bottom (2)
								// third digit - Melee (0) or Ranged (1)

								string Alliance = KeyString.substr( 3, 1 );
								string Side = KeyString.substr( 4, 1 );
								string Type = KeyString.substr( 5, 1 );
								CDotAPlayer *Killer = GetDotAPlayerFromLobbyColor( ValueInt );
								string AllianceString;
								string SideString;
								string TypeString;

								if( Alliance == "0" )
									AllianceString = "Sentinel";
								else if( Alliance == "1" )
									AllianceString = "Scourge";
								else
									AllianceString = "unknown";

								if( Side == "0" )
									SideString = "top";
								else if( Side == "1" )
									SideString = "mid";
								else if( Side == "2" )
									SideString = "bottom";
								else
									SideString = "unknown";

								if( Type == "0" )
									TypeString = "melee";
								else if( Type == "1" )
									TypeString = "ranged";
								else
									TypeString = "unknown";

								if( ValueInt == 0 )
								{
									CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] the Sentinel destroyed a " + TypeString + " " + AllianceString + " rax (" + SideString + ")" );
									m_GameLog->AddMessage( "The Sentinel destroyed the " + SideString + " " + AllianceString + " " + TypeString + " rax" );
								}
								else if( ValueInt == 6 )
								{
									CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] the Scourge destroyed a " + TypeString + " " + AllianceString + " rax (" + SideString + ")" );
									m_GameLog->AddMessage( "The Scourge destroyed the " + SideString + " " + AllianceString + " " + TypeString + " rax" );
								}
								else
								{
									if( Killer )
									{
										if( UTIL_ToUInt32( Alliance ) != Killer->GetCurrentTeam( ) )
										{
											// player killed enemy rax

											if( !Killer->GetHasLeftTheGame( ) && m_CollectDotAStats )
												Killer->SetRaxKills( Killer->GetRaxKills( ) + 1 );

											CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] player [" + Killer->GetName( ) + "] destroyed a " + TypeString + " " + AllianceString + " rax (" + SideString + ")" );
											m_GameLog->AddMessage( Killer->GetName( ) + " destroyed the " + SideString + " " + AllianceString + " " + TypeString + " rax" );
										}
										else
										{
											// player denied allied rax

											CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] player [" + Killer->GetName( ) + "] denied a " + TypeString + " " + AllianceString + " rax (" + SideString + ")" );
											m_GameLog->AddMessage( Killer->GetName( ) + " denied the " + SideString + " " + AllianceString + " " + TypeString + " rax" );
										}
									}
								}
							}
							else if( KeyString.size( ) >= 4  && KeyString.substr( 0, 3 ) == "CSK" )
							{
								// information about the creep kills of a player
								// this data is sent every 3.5 minutes
								// this data is sent for the first time 3 minutes after the first creeps spawn (not confirmed)
								// Example:
								// DataString = "Data"
								// KeyString = "CSK1"
								// ValueInt = 15
								// A player, with colour 1, has killed 15 creeps when this action is sent

								string PlayerColourString = KeyString.substr( 3 );
								uint32_t PlayerColour = UTIL_ToUInt32( PlayerColourString );

								CDotAPlayer *Player = GetDotAPlayerFromLobbyColor( PlayerColour );

								if( Player && !Player->GetHasLeftTheGame( ) && m_CollectDotAStats )
									Player->SetCreepKills( ValueInt );
							}
							else if( KeyString.size( ) >= 4  && KeyString.substr( 0, 3 ) == "CSD" )
							{
								// information about the creep denies of a player
								// this data is sent every 3.5 minutes
								// this data is sent for the first time 3 minutes after the first creeps spawn (not confirmed)
								// Example:
								// DataString = "Data"
								// KeyString = "CSD1"
								// ValueInt = 15
								// A player, with colour 1, has denied 15 creeps when this action is sent

								string PlayerColourString = KeyString.substr( 3 );
								uint32_t PlayerColour = UTIL_ToUInt32( PlayerColourString );

								CDotAPlayer *Player = GetDotAPlayerFromLobbyColor( PlayerColour );

								if( Player && !Player->GetHasLeftTheGame( ) && m_CollectDotAStats )
									Player->SetCreepDenies( ValueInt );
							}
							else if( KeyString.size( ) >= 3  && KeyString.substr( 0, 2 ) == "NK" )
							{
								// information about the neutral kills of a player
								// this data is sent every 3.5 minutes
								// this data is sent for the first time 3 minutes after the first creeps spawn (not confirmed)
								// Example:
								// DataString = "Data"
								// KeyString = "NK1"
								// ValueInt = 15
								// A player, with colour 1, has killed 15 neutrals when this action is sent

								string PlayerColourString = KeyString.substr( 2 );
								uint32_t PlayerColour = UTIL_ToUInt32( PlayerColourString );

								CDotAPlayer *Player = GetDotAPlayerFromLobbyColor( PlayerColour );

								if( Player && !Player->GetHasLeftTheGame( ) && m_CollectDotAStats )
									Player->SetNeutralKills( ValueInt );
							}
							else if( KeyString.size( ) >= 6 && KeyString.substr( 0, 6 ) == "Throne" )
							{
								// the frozen throne got hurt

								CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] the Frozen Throne is now at " + UTIL_ToString( ValueInt ) + "% HP" );
								m_GameLog->AddMessage( "The Frozen Throne is now at " + UTIL_ToString( ValueInt ) + "% HP" );

								if( ValueInt <= 50 )
								{
									m_ThroneLowHP = true;
									m_ThroneLowHPTime = m_GameTicks / 1000;
								}
							}
							else if( KeyString.size( ) >= 4 && KeyString.substr( 0, 4 ) == "Tree" )
							{
								// the world tree got hurt

								CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] the World Tree is now at " + UTIL_ToString( ValueInt ) + "% HP" );
								m_GameLog->AddMessage( "The World Tree is now at " + UTIL_ToString( ValueInt ) + "% HP" );

								if( ValueInt <= 50 )
								{
									m_TreeLowHP = true;
									m_TreeLowHPTime = m_GameTicks / 1000;
								}
							}
							else if( KeyString.size( ) >= 2 && KeyString.substr( 0, 2 ) == "CK" )
							{
								// a player disconnected
							}
							else if( KeyString.size( ) >= 9 && KeyString.substr( 0, 9 ) == "GameStart" )
							{
								// game start, creeps spawned

								CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] creeps spawned" );
								m_GameLog->AddMessage( "-- Creeps Spawned --" );

								m_CreepsSpawnedTime = m_GameTicks / 1000;
								m_CreepsSpawned = true;
							}
							else if( KeyString.size( ) >= 4 && KeyString.substr( 0, 4 ) == "Mode" )
							{
								// mode selected

								CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] game mode is [" + KeyString.substr( 4 ) + "]" );
								m_Mode1 = KeyString.substr( 4 );
							}
						}
						else if( DataString == "Global" )
						{
							// these are only received at the end of the game

							if( KeyString == "Winner" )
							{
								// Value 1 -> sentinel
								// Value 2 -> scourge

								CONSOLE_Print( "[GAME: " + m_GameName + "] gameover timer started (stats class reported game over)" );
								SendEndMessage( );
								m_GameOverTime = GetTime( );

								// don't set the winner if the game is being remaked

								if( m_CollectDotAStats )
								{
									m_Winner = ValueInt;
									// don't set this to false here since we wan't to collect more actions, that are sent after "Winner" action, when someone destroys tree/throne
									//m_CollectDotAStats = false;
									m_CollectDotAStatsOverTime = m_GameTicks / 1000;
								}

								if( m_Winner == 1 )
									CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] detected winner: Sentinel" );
								else if( m_Winner == 2 )
									CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] detected winner: Scourge" );
								else
									CONSOLE_Print( "[STATSDOTA: " + GetGameName( ) + "] detected winner: " + UTIL_ToString( ValueInt ) );
							}
							else if( KeyString == "m" )
								m_Min = ValueInt;
							else if( KeyString == "s" )
								m_Sec = ValueInt;
						}
						else if( DataString.size( ) <= 2 && DataString.find_first_not_of( "1234567890" ) == string :: npos )
						{
							// these are only received at the end of the game

							uint32_t ID = UTIL_ToUInt32( DataString );

							if( ( ID >= 1 && ID <= 5 ) || ( ID >= 7 && ID <= 11 ) )
							{
								// Key "1"		-> Kills
								// Key "2"		-> Deaths
								// Key "3"		-> Creep Kills
								// Key "4"		-> Creep Denies
								// Key "5"		-> Assists
								// Key "6"		-> Current Gold
								// Key "7"		-> Neutral Kills
								// Key "8_0"	-> Item 1
								// Key "8_1"	-> Item 2
								// Key "8_2"	-> Item 3
								// Key "8_3"	-> Item 4
								// Key "8_4"	-> Item 5
								// Key "8_5"	-> Item 6
								// Key "id"		-> ID (1-5 for sentinel, 6-10 for scourge, accurate after using -sp and/or -switch)

								CDotAPlayer *Player = GetDotAPlayerFromLobbyColor( ID );

								if( KeyString == "1" )
								{
									if( Player && !Player->GetHasLeftTheGame( ) && m_CollectDotAStats )
										Player->SetKills( ValueInt );
								}
								else if( KeyString == "2" )
								{
									if( Player && !Player->GetHasLeftTheGame( ) && m_CollectDotAStats )
										Player->SetDeaths( ValueInt );
								}
								else if( KeyString == "3" )
								{
									if( Player && !Player->GetHasLeftTheGame( ) && m_CollectDotAStats )
										Player->SetCreepKills( ValueInt );
								}
								else if( KeyString == "4" )
								{
									if( Player && !Player->GetHasLeftTheGame( ) && m_CollectDotAStats )
										Player->SetCreepDenies( ValueInt );
								}
								else if( KeyString == "5" )
								{
									if( Player && !Player->GetHasLeftTheGame( ) && m_CollectDotAStats )
										Player->SetAssists( ValueInt );
								}
								else if( KeyString == "6" )
								{
									if( Player && !Player->GetHasLeftTheGame( ) && m_CollectDotAStats )
										Player->SetGold( ValueInt );
								}
								else if( KeyString == "7" )
								{
									if( Player && !Player->GetHasLeftTheGame( ) && m_CollectDotAStats )
										Player->SetNeutralKills( ValueInt );
								}

								// item actions below are sent when player leaves the game (someone said they are not always sent?, dunno why) or when the game finishes (tree/throne is destroyed)
								
								// we want to know what items player had when he left the game, NOT what items he had when the game ended
								// because his team mates already sold/took his items probably

								// if we run the IF blocks below that would give us accurate item status as well
								// EXCEPT in those instances when the data has not been sent after player left

								else if( KeyString == "8_0" )
								{
									if( Player && !Player->GetItemsSent( ) )
										Player->SetItem( 0, string( Value.rbegin( ), Value.rend( ) ) );
								}
								else if( KeyString == "8_1" )
								{
									if( Player && !Player->GetItemsSent( ) )
										Player->SetItem( 1, string( Value.rbegin( ), Value.rend( ) ) );
								}
								else if( KeyString == "8_2" )
								{
									if( Player && !Player->GetItemsSent( ) )
										Player->SetItem( 2, string( Value.rbegin( ), Value.rend( ) ) );
								}
								else if( KeyString == "8_3" )
								{
									if( Player && !Player->GetItemsSent( ) )
										Player->SetItem( 3, string( Value.rbegin( ), Value.rend( ) ) );
								}
								else if( KeyString == "8_4" )
								{
									if( Player && !Player->GetItemsSent( ) )
										Player->SetItem( 4, string( Value.rbegin( ), Value.rend( ) ) );
								}
								else if( KeyString == "8_5" )
								{
									if( Player && !Player->GetItemsSent( ) )
										Player->SetItem( 5, string( Value.rbegin( ), Value.rend( ) ) );

									if( Player )
										Player->SetItemsSent( true );
								}
								else if( KeyString == "9" )
								{
									if( Player && !Player->GetHasLeftTheGame( ) && m_CollectDotAStats )
										Player->SetHero( string( Value.rbegin( ), Value.rend( ) ) );
								}
								else if( KeyString == "id" )
								{
									// DotA sends id values from 1-10 with 1-5 being sentinel players and 6-10 being scourge players
									// unfortunately the actual player colours are from 1-5 and from 7-11 so we need to deal with this case here

									/*if( ValueInt >= 6 )
										m_DBDotAPlayers[ID]->SetNewColour( ValueInt + 1 );
									else
										m_DBDotAPlayers[ID]->SetNewColour( ValueInt );*/

									/*if( ValueInt >= 6 )
									{
										m_DBDotAPlayers[ID]->SetNewColour( ValueInt + 1 );
										m_DBDotAPlayers[ID]->SetTeam( 1 );

										CONSOLE_Print( "ID = " + UTIL_ToString( ID ) + ", ValueInt = " + UTIL_ToString( ValueInt ) + ", Setting team = 1" );
									}
									else
									{
										m_DBDotAPlayers[ID]->SetNewColour( ValueInt );
										m_DBDotAPlayers[ID]->SetTeam( 0 );

										CONSOLE_Print( "ID = " + UTIL_ToString( ID ) + ", ValueInt = " + UTIL_ToString( ValueInt ) + ", Setting team = 0" );
									}*/

									if( Player )
									{
										if( ValueInt >= 6 )
										{
											Player->SetNewColor( ValueInt + 1 );
											CONSOLE_Print( "ID = " + UTIL_ToString( ID ) + ", ValueInt = " + UTIL_ToString( ValueInt ) + ", Setting team = 1" );
										}
										else
										{
											Player->SetNewColor( ValueInt );
											CONSOLE_Print( "ID = " + UTIL_ToString( ID ) + ", ValueInt = " + UTIL_ToString( ValueInt ) + ", Setting team = 0" );
										}
									}
								}
							}
						}

						i += 12 + Data.size( ) + Key.size( );
					}
					else
						++i;
				}
				else
					++i;
			}
			else
				++i;
		}
		else
			++i;
	}

	/*if( m_Winner != 0 && m_GameOverTime == 0 )
	{
		CONSOLE_Print( "[GAME: " + m_GameName + "] gameover timer started (stats class reported game over)" );
		SendEndMessage( );
		m_GameOverTime = GetTime( );
	}*/
	return success;
}

void CCustomDotAGame :: EventPlayerBotCommand2( CGamePlayer *player, string command, string payload, bool rootadmin, bool admin, bool owner )
{
	// todotodo: don't be lazy

	string User = player->GetName( );
	string Command = command;
	string Payload = payload;

	bool Command1Found = true;
	bool Command2Found = true;

	if( rootadmin || admin || owner )
	{
		/*****************
		* ADMIN COMMANDS *
		******************/

		// dummy IF block, since there's no commands here
		// this is fail safe if i forget on the else { command_found = false } block further down

		if( false )
			;

		//
		// !ABORT (abort countdown)
		// !A
		//

		// we use "!a" as an alias for abort because you don't have much time to abort the countdown so it's useful for the abort command to be easy to type

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
		// !BALANCE
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

		//
		// !CLOSE (close slot)
		//

		//
		// !CLOSEALL
		//

		//
		// !COMP (computer slot)
		//

		//
		// !COMPCOLOUR (computer colour change)
		//

		//
		// !COMPHANDICAP (computer handicap change)
		//

		//
		// !COMPRACE (computer race change)
		//

		//
		// !COMPTEAM (computer team change)
		//

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
		// !FROM
		// !F
		//

		//
		// !HCL
		//

		//
		// !HOLD (hold a slot for someone)
		//

		//
		// !KICK (kick a player)
		//

		//
		// !LATENCY (set game latency)
		//

		//
		// !LOCK
		//

		//
		// !MAX
		// !MAXPSR
		//

		//
		// !MESSAGES
		//

		//
		// !MIN
		// !MINPSR
		//

		//
		// !MUTE
		//

		//
		// !MUTEALL
		//

		//
		// !OPEN (open slot)
		//

		//
		// !OPENALL
		//

		//
		// !OWNER (set game owner)
		//

		//
		// !PING
		//

		//
		// !PRIV (rehost as private game)
		//

		//
		// !PUB (rehost as public game)
		//

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

		//
		// !START
		//
		
		//
		// !STARTN
		//

		//
		// !SWAP (swap slots)
		//

		//
		// !SYNCLIMIT
		//

		//
		// !UNHOST
		//

		//
		// !UNLOCK
		//

		//
		// !UNMUTE
		//

		//
		// !UNMUTEALL
		//

		//
		// !VIRTUALHOST
		//

		//
		// !VOTECANCEL
		//

		//
		// !W
		//

		else
			Command1Found = false;
	}
	else
		Command1Found = false;

	/*********************
	* NON ADMIN COMMANDS *
	*********************/

	//
	// !CHECK
	//

	//
	// !CHECKME
	//

	//
	// !FORFEIT
	// !FF
	//

	//
	// !FORFEITCOUNT
	// !FFCOUNT
	//

	//
	// !HCL
	//

	//
	// !GAMEINFO
	//

	if( Command == "gameinfo" )
	{
		for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == player->GetJoinedRealm( ) )
			{
				SendChat( player, "Name: [" + m_GameName + "], Type: [custom DotA game], Owner: [" + m_OwnerName + "], Hostbot: [" + (*i)->GetBnetUserName( ) + "]" );
				break;
			}
		}
	}

	//
	// !GAMENAME
	// !GN
	//

	//
	// !RALL
	// !RANKALL
	// !RATEALL
	//

	//
	// !RATING
	// !R
	// !PSR
	//

	//
	// !RATINGS
	// !RS
	//

	//
	// !OWNER (set game owner)
	//

	//
	// !REMAKE
	// !RMK
	//

	else if( Command == "remake" || Command == "rmk" )
	{
		if( m_GameLoaded && !m_GameOverTime )
		{
			if( !m_VoteRemakeStarted )
			{
				if( GetNumHumanPlayers( ) < 2 )
					SendAllChat( "Unable to start vote remake, you are alone in the game." );
				else
				{
					m_VoteRemakeStarted = true;
					m_VoteRemakeStartedTime = GetTime( );

					for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); ++i )
						(*i)->SetRemakeVote( false );

					player->SetRemakeVote( true );

					CONSOLE_Print( "[GAME: " + m_GameName + "] vote remake started by player [" + User + "]" );
					SendAllChat( "[" + User + "] voted to rmk [1/" + UTIL_ToString( GetNumHumanPlayers( ) == 2 ? 2 : (uint32_t)ceil( ( GetNumHumanPlayers( ) - 1 ) * (float)80 / 100 ) ) + "]" );
				}
			}
			else
			{
				uint32_t VotesNeeded = GetNumHumanPlayers( ) == 2 ? 2 : (uint32_t)ceil( ( GetNumHumanPlayers( ) - 1 ) * (float)80 / 100 );
				uint32_t Votes = 0;

				bool AlreadyVoted = player->GetRemakeVote( );
				player->SetRemakeVote( true );

				for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); ++i )
				{
					if( (*i)->GetRemakeVote( ) )
						++Votes;
				}

				if( !AlreadyVoted )
				{
					CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + User + "] voted to remake, still " + UTIL_ToString( VotesNeeded - Votes ) + " votes are needed" );
					SendAllChat( "[" + User + "] voted to rmk [" + UTIL_ToString( Votes ) + "/" + UTIL_ToString( VotesNeeded ) + "]" );
				}
				else
					SendChat( player, "You already voted to rmk [" + UTIL_ToString( Votes ) + "/" + UTIL_ToString( VotesNeeded ) + "]" );

				if( Votes >= VotesNeeded )
				{
					CONSOLE_Print( "[GAME: " + m_GameName + "] gameover timer started (players voted to remake the game)" );
					SendAllChat( UTIL_ToString( Votes ) + " of " + UTIL_ToString( GetNumHumanPlayers( ) ) + " players voted to remake the game, game is ending..." );

					m_RMK = true;

					SendEndMessage( );
					m_GameOverTime = GetTime( );

					m_CollectDotAStats = false;
					m_CollectDotAStatsOverTime = m_GameTicks / 1000;

					m_VoteRemakeStarted = false;
					m_VoteRemakeStartedTime = 0;
				}
			}
		}
	}

	//
	// !REMAKECOUNT
	// !RMKCOUNT
	//

	//
	// !SC
	//

	//
	// !STATS
	//

	//
	// !VERSION
	//

	//
	// !VOTEKICK
	//

	//
	// !VOTEKICKCOUNT
	// !YESCOUNT
	//

	//
	// !YES
	//

	//
	// !I
	// !IGNORE
	//

	//
	// !IGNORES
	// !IGNORELIST
	//

	//
	// !UNI
	// !UNIGNORE
	//

	else
		Command2Found = false;

	// if the issued command is not found here look into the base command list

	if( !Command1Found && !Command2Found )
		CBaseGame :: EventPlayerBotCommand2( player, command, payload, rootadmin, admin, owner );
}

void CCustomDotAGame :: EventGameStarted( )
{
	CBaseGame :: EventGameStarted( );

	CONSOLE_Print("Custom dota game loaded event.");

	m_Mode1 = m_HCLCommandString;

	// while the game is in lobby CDotAPlayer->GetTeam( ) calls CGamePlayer from CBaseGame to get lobby team/color
	// when the game is in progress CDotAPlayer->GetTeam( ) returns internal member value that we initially set here
	// map stats code could change CDotAPlayer's team/color during game

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); ++i )
	{
		unsigned char SID = GetSIDFromPID( (*i)->GetPID( ) );
		unsigned char Team = 255;
		unsigned char Color = 255;

		if( SID < m_Slots.size( ) )
		{
			Team = m_Slots[SID].GetTeam( );
			Color = m_Slots[SID].GetColour( );
		}

		if( Team == 12 )
		{
			// don't waste space with observers, no DotA stats is collected for them in custom DotA games

			continue;
		}

		CDotAPlayer *DotAPlayer = new CDotAPlayer( this, (*i)->GetPID( ), (*i)->GetName( ), GetServerID( (*i)->GetSpoofedRealm( ) ) );
		DotAPlayer->SetLobbyColor( Color );
		DotAPlayer->SetNewColor( Color );

		m_DotAPlayers.push_back( DotAPlayer );
	}
}

void CCustomDotAGame::EventGameLoaded() {
	CBaseGame::EventGameLoaded();

	CONSOLE_Print("Custom dota game loaded event.");

	//Select a single player to become the event sender. By default all are selected.
	this->selectOneGameEventReporter();
}

bool CCustomDotAGame :: IsGameDataSaved( )
{
	return m_Saved;
}

void CCustomDotAGame :: SaveGameData( )
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

	stringstream GameInfo;

	GameInfo << m_Winner;
	GameInfo << " " << m_CreepsSpawnedTime;
	GameInfo << " " << m_CollectDotAStatsOverTime;
	GameInfo << " " << m_Min;
	GameInfo << " " << m_Sec;
	//GameInfo << " " << m_Mode1;
	//GameInfo << " " << m_Mode2;
	MASL_PROTOCOL :: SSWriteString( GameInfo, m_Mode1 );
	MASL_PROTOCOL :: SSWriteString( GameInfo, m_Mode2 );

	GameInfo << " " << m_DotAPlayers.size( );	// uint32_t DBDotAPlayersSize;

	for( list<CDotAPlayer *> :: iterator i = m_DotAPlayers.begin( ); i != m_DotAPlayers.end( ); ++i )
	{
		GameInfo << " " << (uint32_t)(*i)->GetLobbyColor( );
		GameInfo << " " << (*i)->GetLevel( );
		GameInfo << " " << (*i)->GetKills( );
		GameInfo << " " << (*i)->GetDeaths( );
		GameInfo << " " << (*i)->GetCreepKills( );
		GameInfo << " " << (*i)->GetCreepDenies( );
		GameInfo << " " << (*i)->GetAssists( );
		GameInfo << " " << (*i)->GetGold( );
		GameInfo << " " << (*i)->GetNeutralKills( );
		MASL_PROTOCOL :: SSWriteString( GameInfo, (*i)->GetItem( 0 ) );
		MASL_PROTOCOL :: SSWriteString( GameInfo, (*i)->GetItem( 1 ) );
		MASL_PROTOCOL :: SSWriteString( GameInfo, (*i)->GetItem( 2 ) );
		MASL_PROTOCOL :: SSWriteString( GameInfo, (*i)->GetItem( 3 ) );
		MASL_PROTOCOL :: SSWriteString( GameInfo, (*i)->GetItem( 4 ) );
		MASL_PROTOCOL :: SSWriteString( GameInfo, (*i)->GetItem( 5 ) );
		MASL_PROTOCOL :: SSWriteString( GameInfo, (*i)->GetHero( ) );
		GameInfo << " " << (uint32_t)(*i)->GetCurrentColor( );
		GameInfo << " " << (*i)->GetTowerKills( );
		GameInfo << " " << (*i)->GetRaxKills( );
		GameInfo << " " << (*i)->GetCourierKills( );
	}

	m_GHost->m_Manager->SendGameSave( m_MySQLGameID, m_LobbyDuration, LobbyLog, GameLog, m_ReplayName, m_MapPath, m_GameName, m_OwnerName, m_GameTicks / 1000, m_GameState, m_CreatorName, m_CreatorServer, m_DBGamePlayers, m_RMK, m_GameType, GameInfo.str( ) );
	
	m_Saved = true;
}

CDotAPlayer *CCustomDotAGame :: GetDotAPlayerFromPID( unsigned char PID )
{
	for( list<CDotAPlayer *> :: iterator i = m_DotAPlayers.begin( ); i != m_DotAPlayers.end( ); ++i )
	{
		if( (*i)->GetPID( ) == PID )
			return *i;
	}

	return NULL;
}

CDotAPlayer *CCustomDotAGame :: GetDotAPlayerFromLobbyColor( unsigned char color )
{
	for( list<CDotAPlayer *> :: iterator i = m_DotAPlayers.begin( ); i != m_DotAPlayers.end( ); ++i )
	{
		if( (*i)->GetLobbyColor( ) == color )
			return *i;
	}

	return NULL;
}
