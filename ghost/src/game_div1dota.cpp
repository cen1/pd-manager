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

#include "game_div1dota.h"
#include "bnet.h"
#include "config.h"
#include "game_base.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "ghost.h"
#include "ghostdb.h"
#include "language.h"
#include "map.h"
#include "masl_manager.h"
#include "masl_protocol_2.h"
#include "packed.h"
#include "psr.h"
#include "regions.h"
#include "savegame.h"
#include "socket.h"
#include "util.h"

#include <cmath>
#include <string.h>
#include <time.h>

//
// sorting classes
//

class SortDescByRating {
  public:
	bool operator()(pair<string, double> Player1, pair<string, double> Player2) const
	{
		return Player1.second > Player2.second;
	}
};

class SortDescByRating2 {
  public:
	bool operator()(pair<unsigned char, double> Player1, pair<unsigned char, double> Player2) const
	{
		return Player1.second > Player2.second;
	}
};

class SortDescByRating3 {
  public:
	bool operator()(tuple<string, double, double> Player1, tuple<string, double, double> Player2) const
	{
		return std::get<1>(Player1) > std::get<1>(Player2);
	}
};

//
// CDiv1DotAGame
//

CDiv1DotAGame ::CDiv1DotAGame(CGHost* nGHost, CMap* nMap, CSaveGame* nSaveGame, uint16_t nHostPort, unsigned char nGameState, string nGameName, string nOwnerName, string nCreatorName, string nCreatorServer)
  : CBaseGame(nGHost, nMap, nSaveGame, nHostPort, nGameState, nGameName, nOwnerName, nCreatorName, nCreatorServer, MASL_PROTOCOL ::DB_DIV1_DOTA_GAME)
{
	CONSOLE_Print("[STATSDOTA] using dota stats");

	m_GameCommandTrigger = '!';

	m_Winner = 0;
	m_Min = 0;
	m_Sec = 0;
	m_CreepsSpawnedTime = 0;
	m_TreeLowHPTime = 0;
	m_ThroneLowHPTime = 0;
	m_CreepsSpawned = false;
	m_TreeLowHP = false;
	m_ThroneLowHP = false;
	m_MinPSR = 0;
	m_MaxPSR = 0;
	m_MinGames = 0;
	m_CollectDotAStats = true;
	m_CollectDotAStatsOverTime = 0;
	m_MaxWinChanceDiffGainConstant = nGHost->m_DotaMaxWinChanceDiffGainConstant;
	m_MaxWinChanceDiff = nGHost->m_DotaMaxWinChanceDiff;
	m_MaxWLDiff = nGHost->m_DotaMaxWLDiff;

	m_PSR = new CPSR();
	m_SentinelVoteFFStartedTime = 0;
	m_ScourgeVoteFFStartedTime = 0;
	m_SentinelVoteFFStarted = false;
	m_ScourgeVoteFFStarted = false;
	m_FF = false;
	m_LadderGameOver = false;
	m_AutoBanOn = true;
	m_PlayerLeftDuringLoading = false;
	m_Observers = nMap->GetMapObservers();

	m_SentinelTopRax = 3;
	m_SentinelMidRax = 3;
	m_SentinelBotRax = 3;

	m_ScourgeTopRax = 3;
	m_ScourgeMidRax = 3;
	m_ScourgeBotRax = 3;

	// Players are not yet present so we just fill the whole PID range
	for (unsigned char i = 0; i < 255; i++) {
		m_SentinelTopRaxV[i] = 3;
		m_SentinelMidRaxV[i] = 3;
		m_SentinelBotRaxV[i] = 3;
		m_ScourgeTopRaxV[i] = 3;
		m_ScourgeMidRaxV[i] = 3;
		m_ScourgeBotRaxV[i] = 3;
	}
}

CDiv1DotAGame ::~CDiv1DotAGame()
{
	for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i)
		delete *i;

	delete m_PSR;
}

bool CDiv1DotAGame ::EventPlayerAction(CGamePlayer* player, CIncomingAction* action)
{
	bool success = CBaseGame ::EventPlayerAction(player, action);
	if (!success)
		return false;
	// give the stats class a chance to process the action

	unsigned int i = 0;
	BYTEARRAY* ActionData = action->GetAction();
	BYTEARRAY Data;
	BYTEARRAY Key;
	BYTEARRAY Value;

	// dota actions with real time replay data start with 0x6b then the null terminated string "dr.x"
	// unfortunately more than one action can be sent in a single packet and the length of each action isn't explicitly represented in the packet
	// so we have to either parse all the actions and calculate the length based on the type or we can search for an identifying sequence
	// parsing the actions would be more correct but would be a lot more difficult to write for relatively little gain
	// so we take the easy route (which isn't always guaranteed to work) and search the data for the sequence "6b 64 72 2e 78 00" and hope it identifies an action

	while (ActionData->size() >= i + 6) {
		if ((*ActionData)[i] == 0x6b && (*ActionData)[i + 1] == 0x64 && (*ActionData)[i + 2] == 0x72 && (*ActionData)[i + 3] == 0x2e && (*ActionData)[i + 4] == 0x78 && (*ActionData)[i + 5] == 0x00) {
			// we think we've found an action with real time replay data (but we can't be 100% sure)
			// next we parse out two null terminated strings and a 4 byte integer

			if (ActionData->size() >= i + 7) {
				// the first null terminated string should either be the strings "Data" or "Global" or a player id in ASCII representation, e.g. "1" or "2"

				Data = UTIL_ExtractCString(*ActionData, i + 6);

				if (ActionData->size() >= i + 8 + Data.size()) {
					// the second null terminated string should be the key

					Key = UTIL_ExtractCString(*ActionData, i + 7 + Data.size());

					if (ActionData->size() >= i + 12 + Data.size() + Key.size()) {
						// the 4 byte integer should be the value

						Value = BYTEARRAY(ActionData->begin() + i + 8 + Data.size() + Key.size(), ActionData->begin() + i + 12 + Data.size() + Key.size());
						string DataString = string(Data.begin(), Data.end());
						string KeyString = string(Key.begin(), Key.end());
						uint32_t ValueInt = UTIL_ByteArrayToUInt32(Value, false);

						// CONSOLE_Print("[STATS] " + DataString + ", " + KeyString + ", " + UTIL_ToString(ValueInt));

						if (DataString == "Data") {
							// these are received during the game
							// you could use these to calculate killing sprees and double or triple kills (you'd have to make up your own time restrictions though)
							// you could also build a table of "who killed who" data

							if (KeyString.size() >= 5 && KeyString.substr(0, 4) == "Hero") {
								// a hero died
								// Example:
								// DataString = "Data"
								// KeyString = "Hero3"
								// ValueInt = 10
								// Player with colour 10, has killed player with colour 3.

								string VictimColourString = KeyString.substr(4);
								uint32_t VictimColour = UTIL_ToUInt32(VictimColourString);

								// ValueInt can be more than 11 in some cases (ValueInt is 12 when player gets killed by neutral creeps)
								// if the game is with observers, observer player could have color 12 and GetPlayerFromColour could return observer player
								// observer can't be a killer

								CDIV1DotAPlayer* Killer = GetDIV1DotAPlayerFromLobbyColor(ValueInt);
								CDIV1DotAPlayer* Victim = GetDIV1DotAPlayerFromLobbyColor(VictimColour);

								if (Victim && !Victim->GetHasLeftTheGame() && m_CollectDotAStats) {
									Victim->addDeathsV(player);
								}

								if (ValueInt == 0) {
									// the Sentinel has killed a player

									if (Victim) {
										CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] the Sentinel killed player [" + Victim->GetName() + "]");
										m_GameLog->AddMessage(player, "The Sentinel killed " + Victim->GetName());
									}
								}
								else if (ValueInt == 6) {
									// the Scourge has killed a player

									if (Victim) {
										CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] the Scourge killed player [" + Victim->GetName() + "]");
										m_GameLog->AddMessage(player, "The Scourge killed " + Victim->GetName());
									}
								}
								else if (ValueInt == 12) {
									// Neutral creeps have killed a player

									if (Victim) {
										CONSOLE_Print("[STATSDOTA: " + GetGameName() + "] Neutral creeps killed player [" + Victim->GetName() + "]");
										m_GameLog->AddMessage(player, "Neutral creeps killed " + Victim->GetName());
									}
								}
								else {
									// player has killed a player
									if (Killer && Victim) {
										if (!Killer->GetHasLeftTheGame() && !Victim->GetHasLeftTheGame()) {
											// both players are still in the game

											if (ValueInt == VictimColour) {
												// the player has killed himself

												CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] player [" + Killer->GetName() + "] killed himself.");
												m_GameLog->AddMessage(player, Killer->GetName() + " killed himself");
											}
											else if (Killer->GetCurrentTeam() == Victim->GetCurrentTeam()) {
												// a player has denied a hero

												CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] player [" + Killer->GetName() + "] denied player [" + Victim->GetName() + "]");
												m_GameLog->AddMessage(player, Killer->GetName() + " denied " + Victim->GetName());
											}
											else {
												// a player has killed an enemy hero

												CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] player [" + Killer->GetName() + "] killed player [" + Victim->GetName() + "]");
												m_GameLog->AddMessage(player, Killer->GetName() + " killed " + Victim->GetName());

												if (m_CollectDotAStats) {
													Killer->addKillsV(player);
												}
											}
										}
										else if (!Victim->GetHasLeftTheGame()) {
											// leaver has killed a player
										}
										else if (!Killer->GetHasLeftTheGame()) {
											// player has killed a leaver
										}
									}
								}
							}
							else if (KeyString.size() >= 7 && KeyString.substr(0, 6) == "Assist") {
								// an assist has been made
								// Example:
								// DataString = "Data"
								// KeyString = "Assist8"
								// ValueInt = 3
								// Player, with colour 8, has assisted in the death of a player with colour 3

								string AssisterColourString = KeyString.substr(6);
								uint32_t AssisterColour = UTIL_ToUInt32(AssisterColourString);

								CDIV1DotAPlayer* Assister = GetDIV1DotAPlayerFromLobbyColor(AssisterColour);

								if (Assister && !Assister->GetHasLeftTheGame() && m_CollectDotAStats) {
									Assister->addAssistsV(player);
								}
							}
							else if (KeyString.size() >= 6 && KeyString.substr(0, 5) == "Level") {
								// hero got a new level
								// Example:
								// DataString = "Data"
								// KeyString = "Level2"
								// ValueInt = 1
								// Player, with colour 1, has got a lavel 2 on his hero

								string LevelString = KeyString.substr(5);
								uint32_t Level = UTIL_ToUInt32(LevelString);

								CDIV1DotAPlayer* Player = GetDIV1DotAPlayerFromLobbyColor(ValueInt);

								if (Player && !Player->GetHasLeftTheGame() && m_CollectDotAStats)
									Player->SetLevel(Level);
							}
							else if (KeyString.size() >= 5 && KeyString.substr(0, 3) == "PUI") {
								// item has been picked up
								// Example:
								// DataString = "Data"
								// KeyString = "PUI_3"
								// ValueInt = 1227895385
								// Player with colour 3 has picked up item with code 1227895385

								/*string PlayerColourString = KeyString.substr( 4 );
								uint32_t PlayerColour = UTIL_ToUInt32( PlayerColourString );

								CDIV1DotAPlayer *Player = GetDIV1DotAPlayerFromLobbyColor( PlayerColour );

								if( Player && !Player->GetHasLeftTheGame( ) && m_CollectDotAStats )
									Player->PickUpItem( string( Value.rbegin( ), Value.rend( ) ) );

								if( Player )
									CONSOLE_Print( Player->GetName( ) + "->PickUpItem( " + string( Value.rbegin( ), Value.rend( ) ) + " )" );*/
							}
							else if (KeyString.size() >= 5 && KeyString.substr(0, 3) == "DRI") {
								// item has been dropped
								// Example:
								// DataString = "Data"
								// KeyString = "DRI_3"
								// ValueInt = 1227895385
								// Player with colour 3 has dropped item with code 1227895385

								/*string PlayerColourString = KeyString.substr( 4 );
								uint32_t PlayerColour = UTIL_ToUInt32( PlayerColourString );

								CDIV1DotAPlayer *Player = GetDIV1DotAPlayerFromLobbyColor( PlayerColour );

								if( Player && !Player->GetHasLeftTheGame( ) && m_CollectDotAStats )
									Player->DropItem( string( Value.rbegin( ), Value.rend( ) ) );

								if( Player )
									CONSOLE_Print( Player->GetName( ) + "->DropItem( " + string( Value.rbegin( ), Value.rend( ) ) + " )" );*/
							}
							else if (KeyString.size() >= 8 && KeyString.substr(0, 7) == "Courier") {
								// a courier died
								// Example:
								// DataString = "Data"
								// KeyString = "Courier10"
								// ValueInt = 3
								// Player, with colour 3, has killed a courier, that is bought by a player with colour 10.

								string VictimColourString = KeyString.substr(7);
								uint32_t VictimColour = UTIL_ToUInt32(VictimColourString);
								CDIV1DotAPlayer* Killer = GetDIV1DotAPlayerFromLobbyColor(ValueInt);
								CDIV1DotAPlayer* Victim = GetDIV1DotAPlayerFromLobbyColor(VictimColour);

								if (Killer && !Killer->GetHasLeftTheGame() && m_CollectDotAStats)
									Killer->addCourierKillsV(player);

								if (ValueInt == 0) {
									if (Victim) {
										CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] the Sentinel killed a courier owned by player [" + Victim->GetName() + "]");
										m_GameLog->AddMessage(player, "The Sentinel killed " + Victim->GetName() + "'s courier");
									}
								}
								else if (ValueInt == 6) {
									if (Victim) {
										CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] the Scourge killed a courier owned by player [" + Victim->GetName() + "]");
										m_GameLog->AddMessage(player, "The Scourge killed " + Victim->GetName() + "'s courier");
									}
								}
								else if (ValueInt == 12) {
									if (Victim) {
										CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] Neutral creeps killed a courier owned by player [" + Victim->GetName() + "]");
										m_GameLog->AddMessage(player, "Neutral creeps killed " + Victim->GetName() + "'s courier");
									}
								}
								else {
									if (Killer && Victim) {
										CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] player [" + Killer->GetName() + "] killed a courier owned by player [" + Victim->GetName() + "]");
										m_GameLog->AddMessage(player, Killer->GetName() + " killed " + Victim->GetName() + "'s courier");
									}
								}
							}
							else if (KeyString.size() >= 8 && KeyString.substr(0, 5) == "Tower") {
								// a tower died
								// Example:
								// DataString = "Data"
								// KeyString = "Tower111"
								// ValueInt = 5
								// Player, with colour 5, has destroyed the middle level 1 scourge tower
								// first digit - Sentinel (0) or Scourge (1)
								// second digit - Level of tower (1, 2, 3 or 4)
								// third digit - Top (0), Middle (1) or Bottom (2)

								string Alliance = KeyString.substr(5, 1);
								string Level = KeyString.substr(6, 1);
								string Side = KeyString.substr(7, 1);
								CDIV1DotAPlayer* Killer = GetDIV1DotAPlayerFromLobbyColor(ValueInt);
								string AllianceString;
								string SideString;

								if (Alliance == "0")
									AllianceString = "Sentinel";
								else if (Alliance == "1")
									AllianceString = "Scourge";
								else
									AllianceString = "unknown";

								if (Side == "0")
									SideString = "top";
								else if (Side == "1")
									SideString = "mid";
								else if (Side == "2")
									SideString = "bottom";
								else
									SideString = "unknown";

								if (Level == "3") {
									// level 3 tower destroyed

									if (Alliance == "0") {
										// sentinel

										if (Side == "0")
											this->subtractScourgeTopRaxV(player);
										else if (Side == "1")
											this->subtractScourgeMidRaxV(player);
										else if (Side == "2")
											this->subtractScourgeBotRaxV(player);
									}
									else if (Alliance == "1") {
										// scourge

										if (Side == "0")
											this->subtractScourgeTopRaxV(player);
										else if (Side == "1")
											this->subtractScourgeMidRaxV(player);
										else if (Side == "2")
											this->subtractScourgeBotRaxV(player);
									}
								}

								if (ValueInt == 0) {
									CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] the Sentinel destroyed a level [" + Level + "] " + AllianceString + " tower (" + SideString + ")");
									m_GameLog->AddMessage(player, "The Sentinel destroyed the " + SideString + " level " + Level + " " + AllianceString + " tower");
								}
								else if (ValueInt == 6) {
									CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] the Scourge destroyed a level [" + Level + "] " + AllianceString + " tower (" + SideString + ")");
									m_GameLog->AddMessage(player, "The Scourge destroyed the " + SideString + " level " + Level + " " + AllianceString + " tower");
								}
								else {
									if (Killer) {
										if (UTIL_ToUInt32(Alliance) != Killer->GetCurrentTeam()) {
											// a player has killed an enemy tower

											if (!Killer->GetHasLeftTheGame() && m_CollectDotAStats) {
												Killer->addTowerKillsV(player);
											}

											CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] player [" + Killer->GetName() + "] destroyed a level [" + Level + "] " + AllianceString + " tower (" + SideString + ")");
											m_GameLog->AddMessage(player, Killer->GetName() + " destroyed the " + SideString + " level " + Level + " " + AllianceString + " tower");
										}
										else {
											// a player has denied an ally tower

											CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] player [" + Killer->GetName() + "] denied a level [" + Level + "] " + AllianceString + " tower (" + SideString + ")");
											m_GameLog->AddMessage(player, Killer->GetName() + " denied the " + SideString + " level " + Level + " " + AllianceString + " tower");
										}
									}
								}
							}
							else if (KeyString.size() >= 6 && KeyString.substr(0, 3) == "Rax") {
								// a rax died
								// Example:
								// DataString = "Data"
								// KeyString = "Rax111"
								// ValueInt = 3
								// A player, with colour 3, has destroyed the Middle Scourge Ranged rax
								// first digit - Sentinel (0) or Scourge (1)
								// second digit - Top (0), Middle (1) or Bottom (2)
								// third digit - Melee (0) or Ranged (1)

								string Alliance = KeyString.substr(3, 1);
								string Side = KeyString.substr(4, 1);
								string Type = KeyString.substr(5, 1);
								CDIV1DotAPlayer* Killer = GetDIV1DotAPlayerFromLobbyColor(ValueInt);
								string AllianceString;
								string SideString;
								string TypeString;

								if (Alliance == "0")
									AllianceString = "Sentinel";
								else if (Alliance == "1")
									AllianceString = "Scourge";
								else
									AllianceString = "unknown";

								if (Side == "0")
									SideString = "top";
								else if (Side == "1")
									SideString = "mid";
								else if (Side == "2")
									SideString = "bottom";
								else
									SideString = "unknown";

								if (Type == "0")
									TypeString = "melee";
								else if (Type == "1")
									TypeString = "ranged";
								else
									TypeString = "unknown";

								if (Alliance == "0") {
									// sentinel

									if (Side == "0")
										this->subtractSentinelTopRaxV(player);
									else if (Side == "1")
										this->subtractSentinelMidRaxV(player);
									else if (Side == "2")
										this->subtractSentinelBotRaxV(player);
								}
								else if (Alliance == "1") {
									// scourge

									if (Side == "0")
										this->subtractScourgeTopRaxV(player);
									else if (Side == "1")
										this->subtractScourgeMidRaxV(player);
									else if (Side == "2")
										this->subtractScourgeBotRaxV(player);
								}

								if (ValueInt == 0) {
									CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] the Sentinel destroyed a " + TypeString + " " + AllianceString + " rax (" + SideString + ")");
									m_GameLog->AddMessage(player, "The Sentinel destroyed the " + SideString + " " + AllianceString + " " + TypeString + " rax");
								}
								else if (ValueInt == 6) {
									CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] the Scourge destroyed a " + TypeString + " " + AllianceString + " rax (" + SideString + ")");
									m_GameLog->AddMessage(player, "The Scourge destroyed the " + SideString + " " + AllianceString + " " + TypeString + " rax");
								}
								else {
									if (Killer) {
										if (UTIL_ToUInt32(Alliance) != Killer->GetCurrentTeam()) {
											// player killed enemy rax

											if (!Killer->GetHasLeftTheGame() && m_CollectDotAStats) {
												Killer->addRaxKillsV(player);
											}

											CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] player [" + Killer->GetName() + "] destroyed a " + TypeString + " " + AllianceString + " rax (" + SideString + ")");
											m_GameLog->AddMessage(player, Killer->GetName() + " destroyed the " + SideString + " " + AllianceString + " " + TypeString + " rax");
										}
										else {
											// player denied allied rax

											CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] player [" + Killer->GetName() + "] denied a " + TypeString + " " + AllianceString + " rax (" + SideString + ")");
											m_GameLog->AddMessage(player, Killer->GetName() + " denied the " + SideString + " " + AllianceString + " " + TypeString + " rax");
										}
									}
								}
							}
							else if (KeyString.size() >= 4 && KeyString.substr(0, 3) == "CSK") {
								// information about the creep kills of a player
								// this data is sent every 3.5 minutes
								// this data is sent for the first time 3 minutes after the first creeps spawn (not confirmed)
								// Example:
								// DataString = "Data"
								// KeyString = "CSK1"
								// ValueInt = 15
								// A player, with colour 1, has killed 15 creeps when this action is sent

								string PlayerColourString = KeyString.substr(3);
								uint32_t PlayerColour = UTIL_ToUInt32(PlayerColourString);

								CDIV1DotAPlayer* Player = GetDIV1DotAPlayerFromLobbyColor(PlayerColour);

								if (Player && !Player->GetHasLeftTheGame() && m_CollectDotAStats) {
									Player->setCreepKillsV(player, ValueInt);
								}
							}
							else if (KeyString.size() >= 4 && KeyString.substr(0, 3) == "CSD") {
								// information about the creep denies of a player
								// this data is sent every 3.5 minutes
								// this data is sent for the first time 3 minutes after the first creeps spawn (not confirmed)
								// Example:
								// DataString = "Data"
								// KeyString = "CSD1"
								// ValueInt = 15
								// A player, with colour 1, has denied 15 creeps when this action is sent

								string PlayerColourString = KeyString.substr(3);
								uint32_t PlayerColour = UTIL_ToUInt32(PlayerColourString);

								CDIV1DotAPlayer* Player = GetDIV1DotAPlayerFromLobbyColor(PlayerColour);

								if (Player && !Player->GetHasLeftTheGame() && m_CollectDotAStats)
									Player->setCreepDeniesV(player, ValueInt);
							}
							else if (KeyString.size() >= 3 && KeyString.substr(0, 2) == "NK") {
								// information about the neutral kills of a player
								// this data is sent every 3.5 minutes
								// this data is sent for the first time 3 minutes after the first creeps spawn (not confirmed)
								// Example:
								// DataString = "Data"
								// KeyString = "NK1"
								// ValueInt = 15
								// A player, with colour 1, has killed 15 neutrals when this action is sent

								string PlayerColourString = KeyString.substr(2);
								uint32_t PlayerColour = UTIL_ToUInt32(PlayerColourString);

								CDIV1DotAPlayer* Player = GetDIV1DotAPlayerFromLobbyColor(PlayerColour);

								if (Player && !Player->GetHasLeftTheGame() && m_CollectDotAStats)
									Player->setNeutralKillsV(player, ValueInt);
							}
							else if (KeyString.size() >= 6 && KeyString.substr(0, 6) == "Throne") {
								// the frozen throne got hurt

								CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] the Frozen Throne is now at " + UTIL_ToString(ValueInt) + "% HP");
								m_GameLog->AddMessage(player, "The Frozen Throne is now at " + UTIL_ToString(ValueInt) + "% HP");

								if (ValueInt <= 50) {
									m_ThroneLowHP = true;
									m_ThroneLowHPTime = m_GameTicks / 1000;

									// sometimes DotA map doesn't send us the end (tree/throne destroyed) game information which will turn OFF autoban so we're forced to do that here

									m_AutoBanOn = false;
								}
							}
							else if (KeyString.size() >= 4 && KeyString.substr(0, 4) == "Tree") {
								// the world tree got hurt

								CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] the World Tree is now at " + UTIL_ToString(ValueInt) + "% HP");
								m_GameLog->AddMessage(player, "The World Tree is now at " + UTIL_ToString(ValueInt) + "% HP");

								if (ValueInt <= 50) {
									m_TreeLowHP = true;
									m_TreeLowHPTime = m_GameTicks / 1000;

									// sometimes DotA map doesn't send us the end (tree/throne destroyed) game information which will turn OFF autoban so we're forced to do that here

									m_AutoBanOn = false;
								}
							}
							else if (KeyString.size() >= 2 && KeyString.substr(0, 2) == "CK") {
								// a player disconnected
							}
							else if (KeyString.size() >= 9 && KeyString.substr(0, 9) == "GameStart") {
								// game start, creeps spawned

								CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] creeps spawned");
								m_GameLog->AddMessage(player, "-- Creeps Spawned --");

								m_CreepsSpawnedTime = m_GameTicks / 1000;
								m_CreepsSpawned = true;
							}
							else if (KeyString.size() >= 4 && KeyString.substr(0, 4) == "Mode") {
								// mode selected

								CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] game mode is [" + KeyString.substr(4) + "]");
								m_Mode1 = KeyString.substr(4);
							}
						}
						else if (DataString == "Global") {
							// these are only received at the end of the game

							if (KeyString == "Winner") {
								// Value 1 -> sentinel
								// Value 2 -> scourge

								CONSOLE_Print(player, "[GAME: " + m_GameName + "] gameover timer started (stats class reported game over)");
								SendEndMessage();
								m_GameOverTime = GetTime();

								// don't set the winner if the game is being remaked or someone already won by forfeit

								if (m_CollectDotAStats) {
									m_Winner = ValueInt;
									// don't set m_CollectDotAStats to false here since we wan't to collect more actions, that are sent directly after "Winner" action, when someone destroys tree/throne
									// m_CollectDotAStats = false;
									m_CollectDotAStatsOverTime = m_GameTicks / 1000;
								}

								m_LadderGameOver = true;
								m_AutoBanOn = false;

								if (m_Winner == 1)
									CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] detected winner: Sentinel");
								else if (m_Winner == 2)
									CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] detected winner: Scourge");
								else
									CONSOLE_Print(player, "[STATSDOTA: " + GetGameName() + "] detected winner: " + UTIL_ToString(ValueInt));
							}
							else if (KeyString == "m")
								m_Min = ValueInt;
							else if (KeyString == "s")
								m_Sec = ValueInt;
						}
						else if (DataString.size() <= 2 && DataString.find_first_not_of("1234567890") == string ::npos) {
							// these are only received at the end of the game

							uint32_t ID = UTIL_ToUInt32(DataString);

							if ((ID >= 1 && ID <= 5) || (ID >= 7 && ID <= 11)) {
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

								CDIV1DotAPlayer* Player = GetDIV1DotAPlayerFromLobbyColor(ID);

								if (KeyString == "1") {
									if (Player && !Player->GetHasLeftTheGame() && m_CollectDotAStats) {
										Player->setKillsV(player, ValueInt);
									}
								}
								else if (KeyString == "2") {
									if (Player && !Player->GetHasLeftTheGame() && m_CollectDotAStats) {
										Player->setDeathsV(player, ValueInt);
									}
								}
								else if (KeyString == "3") {
									if (Player && !Player->GetHasLeftTheGame() && m_CollectDotAStats) {
										Player->setCreepKillsV(player, ValueInt);
									}
								}
								else if (KeyString == "4") {
									if (Player && !Player->GetHasLeftTheGame() && m_CollectDotAStats) {
										Player->setCreepDeniesV(player, ValueInt);
									}
								}
								else if (KeyString == "5") {
									if (Player && !Player->GetHasLeftTheGame() && m_CollectDotAStats) {
										Player->setAssistsV(player, ValueInt);
									}
								}
								else if (KeyString == "6") {
									if (Player && !Player->GetHasLeftTheGame() && m_CollectDotAStats) {
										Player->SetGold(ValueInt);
									}
								}
								else if (KeyString == "7") {
									if (Player && !Player->GetHasLeftTheGame() && m_CollectDotAStats) {
										Player->setNeutralKillsV(player, ValueInt);
									}
								}

								// item actions below are sent when player leaves the game (someone said they are not always sent?, dunno why) or when the game finishes (tree/throne is destroyed)

								// we want to know what items player had when he left the game, NOT what items he had when the game ended
								// because his team mates already sold/took his items probably

								// if we run the IF blocks below that would give us accurate item status as well
								// EXCEPT in those instances when the data has not been sent after player left

								else if (KeyString == "8_0") {
									if (Player && !Player->GetItemsSent())
										Player->SetItem(0, string(Value.rbegin(), Value.rend()));
								}
								else if (KeyString == "8_1") {
									if (Player && !Player->GetItemsSent())
										Player->SetItem(1, string(Value.rbegin(), Value.rend()));
								}
								else if (KeyString == "8_2") {
									if (Player && !Player->GetItemsSent())
										Player->SetItem(2, string(Value.rbegin(), Value.rend()));
								}
								else if (KeyString == "8_3") {
									if (Player && !Player->GetItemsSent())
										Player->SetItem(3, string(Value.rbegin(), Value.rend()));
								}
								else if (KeyString == "8_4") {
									if (Player && !Player->GetItemsSent())
										Player->SetItem(4, string(Value.rbegin(), Value.rend()));
								}
								else if (KeyString == "8_5") {
									if (Player && !Player->GetItemsSent())
										Player->SetItem(5, string(Value.rbegin(), Value.rend()));

									if (Player)
										Player->SetItemsSent(true);
								}
								else if (KeyString == "9") {
									if (Player && !Player->GetHasLeftTheGame() && m_CollectDotAStats)
										Player->SetHero(string(Value.rbegin(), Value.rend()));
								}
								else if (KeyString == "id") {
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

										for( vector<CDiv1DotAPlayer *> :: iterator i = m_DotAPlayers.begin( ); i != m_DotAPlayers.end( ); ++i )
										{
											if( (*i)->GetColor( ) == ID )
											{
												(*i)->SetTeam( 1 );
												break;
											}
										}

										CONSOLE_Print( "ID = " + UTIL_ToString( ID ) + ", ValueInt = " + UTIL_ToString( ValueInt ) + ", Setting team = 1" );
									}
									else
									{
										m_DBDotAPlayers[ID]->SetNewColour( ValueInt );
										m_DBDotAPlayers[ID]->SetTeam( 0 );

										for( vector<CDiv1DotAPlayer *> :: iterator i = m_DotAPlayers.begin( ); i != m_DotAPlayers.end( ); ++i )
										{
											if( (*i)->GetColor( ) == ID )
											{
												(*i)->SetTeam( 0 );
												break;
											}
										}

										CONSOLE_Print( "ID = " + UTIL_ToString( ID ) + ", ValueInt = " + UTIL_ToString( ValueInt ) + ", Setting team = 0" );
									}*/

									if (Player) {
										if (ValueInt >= 6) {
											Player->setNewColorV(player, ValueInt + 1);
											CONSOLE_Print(player, "ID = " + UTIL_ToString(ID) + ", ValueInt = " + UTIL_ToString(ValueInt) + ", Setting team = 1");
										}
										else {
											Player->setNewColorV(player, ValueInt);
											CONSOLE_Print(player, "ID = " + UTIL_ToString(ID) + ", ValueInt = " + UTIL_ToString(ValueInt) + ", Setting team = 0");
										}
									}
								}
							}
						}

						i += 12 + Data.size() + Key.size();
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

void CDiv1DotAGame ::EventPlayerBotCommand2(CGamePlayer* player, string command, string payload, bool rootadmin, bool admin, bool owner)
{
	// todotodo: don't be lazy

	string User = player->GetName();
	string Command = command;
	string Payload = payload;

	bool Command1Found = true;
	bool Command2Found = true;

	if (rootadmin || admin || owner) {
		/*****************
		 * ADMIN COMMANDS *
		 ******************/

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

		/*if( Command == "banlast" && m_GameLoaded && !m_GameOverTime && m_Scored && m_BanLastDotAPlayer )
		{
			if( m_LastVoteKickedPlayerName == m_BanLastDotAPlayer->GetName( ) )
			{
				if( !payload.empty( ) )
				{
					CDiv1DotAPlayer *Admin = GetDotAPlayerFromPID( player->GetPID( ) );

					if( Admin )
					{
						SendAllChat( "[" + m_BanLastDotAPlayer->GetName( ) + "] was banned by [" + Admin->GetName( ) + "] because [" + payload + "]" );
						m_BanLastDotAPlayer->SetBanned( true );
						m_GHost->m_Manager->SendDIV1PlayerWasBanned( Admin->GetServerID(), Admin->GetName( ), m_BanLastDotAPlayer->GetServerID( ), m_BanLastDotAPlayer->GetName( ), m_MySQLGameID, payload );

						m_BanLastDotAPlayer = NULL;
					}
				}
				else
					SendAllChat( "Please write a reason why are you banning that player (example: !banlast leaver)" );
			}
			else
				SendAllChat( "You can't ban players that were votekicked, please request ban on the forum (www.playdota.eu)." );
		}*/

		//
		// !BALANCE for non owner. Cached info.
		//

		if (Command == "balance" && !m_GameLoading && !m_GameLoaded && !m_CountDownStarted) {
			if (m_CachedBalanceString.empty()) {
				set<unsigned char> LockedSlots;

				BalanceSlots2(LockedSlots);

				vector<PairedPlayerRating> team1;
				vector<PairedPlayerRating> team2;

				for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
					string Name = (*i)->GetName();

					if ((*i)->GetCurrentTeam() == 0)
						team1.push_back(make_pair(Name, (*i)->GetRating()));
					else if ((*i)->GetCurrentTeam() == 1)
						team2.push_back(make_pair(Name, (*i)->GetRating()));
				}

				if (m_GHost->m_UseNewPSRFormula) {
					m_PSR->CalculatePSR_New(team1, team2);
				}
				else {
					m_PSR->CalculatePSR(team1, team2, m_MaxWinChanceDiffGainConstant);
				}

				std::tuple<double, double, double> diff = getWLDiff();
				string t1wr = UTIL_ToString(std::get<0>(diff), 0);
				string t2wr = UTIL_ToString(std::get<1>(diff), 0);

				m_CachedBalanceString = "Sentinel avg. PSR: " + UTIL_ToString(m_PSR->GetTeamAvgPSR(0), 0) + " (" + UTIL_ToString(m_PSR->GetTeamWinPerc(0), 0) + "% to win, avg WR=" + t1wr + "%), Scourge avg. PSR: " + UTIL_ToString(m_PSR->GetTeamAvgPSR(1), 0) + " (" + UTIL_ToString(m_PSR->GetTeamWinPerc(1), 0) + "% to win, avg WR=" + t2wr + "%)";
			}

			SendAllChat(m_CachedBalanceString);
		}

		else if (Command == "checkbalance" && !m_GameLoading && !m_GameLoaded && !m_CountDownStarted) {
			recalcPSR();

			// Max win chance diff to prevent stacking
			if (m_MaxWinChanceDiff > 0) {
				double diff = abs(m_PSR->GetTeamWinPerc(0) - m_PSR->GetTeamWinPerc(1));
				SendChat(player, "Difference between team win chances is " + UTIL_ToString(diff, 0) + ", max allowed is " + UTIL_ToString(m_MaxWinChanceDiff));
			}

			// Max WL diff to prevent stacking
			if (m_MaxWLDiff > 0) {
				std::tuple<double, double, double> diff = getWLDiff();
				double ddiff = std::get<2>(diff);
				SendChat(player, "Difference between W/L ratio of teams is " + UTIL_ToString(ddiff, 1) + ", max allowed is " + UTIL_ToString(m_MaxWLDiff));
			}

			if (m_MaxWinChanceDiff == 0 && m_MaxWLDiff == 0) {
				SendChat(player, "Feature is disabled");
			}
		}

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

		else if (Command == "comp") {
		}

		//
		// !COMPCOLOUR (computer colour change)
		//

		else if (Command == "compcolour") {
		}

		//
		// !COMPHANDICAP (computer handicap change)
		//

		else if (Command == "comphandicap") {
		}

		//
		// !COMPRACE (computer race change)
		//

		else if (Command == "comprace") {
		}

		//
		// !COMPTEAM (computer team change)
		//

		else if (Command == "compteam") {
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
		// !FROM
		// !F
		//

		//
		// !GAMES
		//

		else if (Command == "games" && !m_GameLoading && !m_GameLoaded && !m_CountDownStarted && !Payload.empty()) {
			uint32_t games = UTIL_ToUInt32(Payload);
			uint32_t Kicked = 0;
			m_MinGames = games;
			for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
				CGamePlayer* GamePlayer = GetPlayerFromPID((*i)->GetPID());
				if (GamePlayer) {
					// CONSOLE_Print("[DEBUG] Checking player "+GamePlayer->GetName());
					if (!GamePlayer->GetReserved()) {
						// CONSOLE_Print("[DEBUG] ..not reserved");
						CDBDiv1DPS* DotAPlayerSummary = GamePlayer->GetDiv1DPS();
						if (DotAPlayerSummary) {
							// CONSOLE_Print("[DEBUG] DPS ok");
							if (games > DotAPlayerSummary->GetTotalGames()) {
								// CONSOLE_Print("[DEBUG] does not have enough games");
								GamePlayer->SetDeleteMe(true);
								GamePlayer->SetLeftReason("was kicked for low number of games " + UTIL_ToString(DotAPlayerSummary->GetTotalGames()) + " < " + UTIL_ToString(games));
								GamePlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
								m_GHost->m_Manager->SendGamePlayerLeftLobbyInform(GamePlayer->GetSpoofedRealm(), GamePlayer->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_MIN_GAMES);
								OpenSlot(GetSIDFromPID(GamePlayer->GetPID()), false);
								++Kicked;
							}
						}
						else {
							// CONSOLE_Print("[DEBUG] has NO games");
							GamePlayer->SetDeleteMe(true);
							GamePlayer->SetLeftReason("was kicked for not playing any games");
							GamePlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
							m_GHost->m_Manager->SendGamePlayerLeftLobbyInform(GamePlayer->GetSpoofedRealm(), GamePlayer->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_MIN_GAMES);
							OpenSlot(GetSIDFromPID(GamePlayer->GetPID()), false);
							++Kicked;
						}
					}
				}
			}

			SendAllChat("Kicking " + UTIL_ToString(Kicked) + " players with less GAMES than " + UTIL_ToString(games) + ".");
		}

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

		else if (Command == "lock" && !m_GameLoading && !m_GameLoaded && !m_CountDownStarted) {
			if (Payload.empty()) {
				string LockedPlayers;

				for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
					if ((*i)->GetLocked()) {
						if (LockedPlayers.empty())
							LockedPlayers += (*i)->GetName();
						else
							LockedPlayers += ", " + (*i)->GetName();

						if ((m_GameLoading || m_GameLoaded) && LockedPlayers.size() > 100) {
							// cut the text into multiple lines ingame

							SendChat(player, LockedPlayers);

							LockedPlayers.clear();
						}
					}
				}

				if (!LockedPlayers.empty())
					SendChat(player, LockedPlayers);
			}
			else {
				CGamePlayer* Player = NULL;
				uint32_t Matches = GetPlayerFromNamePartial(Payload, &Player);

				if (Matches == 0)
					SendChat(player, "Unable to lock player, no matches found for [" + Payload + "].");
				else if (Matches == 1) {
					CDIV1DotAPlayer* DotAPlayer = GetDIV1DotAPlayerFromPID(Player->GetPID());

					if (DotAPlayer)
						DotAPlayer->SetLocked(true);

					SendChat(player, "Player [" + Player->GetName() + "] has been locked.");
				}
				else
					SendChat(player, "Unable to lock player, found more then one match for [" + Payload + "].");
			}
		}

		//
		// !MAX
		// !MAXPSR
		//

		else if ((Command == "max" || Command == "maxpsr") && !m_GameLoading && !m_GameLoaded && !m_CountDownStarted && !Payload.empty()) {
			uint32_t MaxPSR = UTIL_ToUInt32(Payload);
			uint32_t Kicked = 0;
			m_MaxPSR = MaxPSR;

			for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
				if (MaxPSR > 0 && (*i)->GetRating() > MaxPSR) {
					CGamePlayer* GamePlayer = GetPlayerFromPID((*i)->GetPID());

					if (GamePlayer) {
						if (!GamePlayer->GetReserved()) {
							GamePlayer->SetDeleteMe(true);
							GamePlayer->SetLeftReason("was kicked for high PSR " + UTIL_ToString((*i)->GetRating(), 0) + " > " + UTIL_ToString(MaxPSR));
							GamePlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
							m_GHost->m_Manager->SendGamePlayerLeftLobbyInform(GamePlayer->GetSpoofedRealm(), GamePlayer->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_MAX_PSR);
							OpenSlot(GetSIDFromPID(GamePlayer->GetPID()), false);
							++Kicked;
						}
					}
				}
			}

			SendAllChat("Kicking " + UTIL_ToString(Kicked) + " players with PSR greater than " + UTIL_ToString(MaxPSR) + ".");
		}

		//
		// !MESSAGES
		//

		//
		// !MIN
		// !MINPSR
		//

		else if ((Command == "min" || Command == "minpsr") && !m_GameLoading && !m_GameLoaded && !m_CountDownStarted && !Payload.empty()) {
			uint32_t MinPSR = UTIL_ToUInt32(Payload);
			uint32_t Kicked = 0;
			m_MinPSR = MinPSR;

			for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
				if (MinPSR > 0 && (*i)->GetRating() < MinPSR) {
					CGamePlayer* GamePlayer = GetPlayerFromPID((*i)->GetPID());

					if (GamePlayer) {
						if (!GamePlayer->GetReserved()) {
							GamePlayer->SetDeleteMe(true);
							GamePlayer->SetLeftReason("was kicked for low PSR " + UTIL_ToString((*i)->GetRating(), 0) + " < " + UTIL_ToString(MinPSR));
							GamePlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
							m_GHost->m_Manager->SendGamePlayerLeftLobbyInform(GamePlayer->GetSpoofedRealm(), GamePlayer->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_MIN_PSR);
							OpenSlot(GetSIDFromPID(GamePlayer->GetPID()), false);
							++Kicked;
						}
					}
				}
			}

			SendAllChat("Kicking " + UTIL_ToString(Kicked) + " players with PSR lower than " + UTIL_ToString(MinPSR) + ".");
		}

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

		else if ((Command == "gopub" || Command == "pub") && !m_CountDownStarted && !m_SaveGame) {
			// we can't allow changing game state to public if this is private game with observer slots
			// or can we? ...
			// if( m_Observers != MASL_PROTOCOL :: NORMAL_MAP_OBSERVER_VALUE )
			//	SendChat( player, "You can't host public DotA ladder games with observers." );

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
			else {
				SendAllChat("Unable to rehost game, the game name is too long.");
			}
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

		//
		// !START
		//

		else if (Command == "start" && !m_CountDownStarted) {
			// if the player sent "!start force" skip the checks and start the countdown
			// otherwise check that the game is ready to start

			if (m_HCLCommandString.empty())
				SendChat(player, "Mode must be set before you start, use " + string(1, m_GameCommandTrigger) + "hcl <DotA mode>");
			else {
				uint32_t NumPlayers = 0;

				for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
					// count only players (non observers)

					if ((*i)->GetCurrentTeam() == 0 || (*i)->GetCurrentTeam() == 1)
						++NumPlayers;
				}

				if (NumPlayers == 10) {
					// Calculate PSR for debugging reasons
					recalcPSR();

					bool allowStart = true;

					// Enforce max win chance diff to prevent stacking
					if (m_MaxWinChanceDiff > 0) {
						double diff = abs(m_PSR->GetTeamWinPerc(0) - m_PSR->GetTeamWinPerc(1));
						CONSOLE_Print("WC diff is " + UTIL_ToString(diff, 0));
						if (diff > m_MaxWinChanceDiff) {
							SendAllChat("This game requires manual balancing. Difference between team win chances is " + UTIL_ToString(diff, 0) + " but max allowed is " + UTIL_ToString(m_MaxWinChanceDiff));
							allowStart = false;
						}
					}

					// Enforce max WL diff to prevent stacking
					if (m_MaxWLDiff > 0) {
						std::tuple<double, double, double> diff = getWLDiff();
						double ddiff = std::get<2>(diff);

						CONSOLE_Print("WL diff is " + UTIL_ToString(ddiff, 0));
						if (ddiff > m_MaxWLDiff) {
							SendAllChat("This game requires manual balancing. Difference between W/L ratio of teams is " + UTIL_ToString(ddiff, 1) + " but max allowed is " + UTIL_ToString(m_MaxWLDiff));
							allowStart = false;
						}
					}

					// Go
					if (allowStart) {
						if (Payload == "force")
							StartCountDown(true);
						else {
							if (GetTicks() - m_LastPlayerLeaveTicks >= 2000)
								StartCountDown(false);
							else
								SendAllChat(m_GHost->m_Language->CountDownAbortedSomeoneLeftRecently());
						}
					}
				}
				else if (NumPlayers < 10 && rootadmin && Payload == "test") {
					StartCountDown(true);
				}
				else {
					SendChat(player, "You need 10 players to start DotA ladder games.");
					SendChat(player, "Create game with .dota command to play non-ladder games.");
				}
			}
		}

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

		else if (Command == "unlock" && !Payload.empty() && !m_GameLoading && !m_GameLoaded && !m_CountDownStarted) {
			CGamePlayer* Player = NULL;
			uint32_t Matches = GetPlayerFromNamePartial(Payload, &Player);

			if (Matches == 0)
				SendChat(player, "Unable to unlock player, no matches found for [" + Payload + "].");
			else if (Matches == 1) {
				CDIV1DotAPlayer* DotAPlayer = GetDIV1DotAPlayerFromPID(Player->GetPID());

				if (DotAPlayer && DotAPlayer->GetLocked()) {
					DotAPlayer->SetLocked(false);
					SendChat(player, "Player [" + Player->GetName() + "] has been unlocked.");

					m_CachedBalanceString.clear();
				}
				else
					SendChat(player, "Player [" + Player->GetName() + "] is not locked.");
			}
			else
				SendChat(player, "Unable to unlock player, found more then one match for [" + Payload + "].");
		}

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
	// !AUTOBAN
	// !CANILEAVE
	//
	if ((Command == "autoban" || Command == "canileave") && m_GameLoaded) {
		if (m_AutoBanOn) {
			if (m_GHost->m_ReplaceAutobanWithPSRPenalty)
				SendChat(player, "PSR penalty is ON, if you leave you will lose multiples of PSR.");
			else
				SendChat(player, "Autoban is ON, if you leave you will get autobanned.");
		}
		else {
			if (m_GHost->m_ReplaceAutobanWithPSRPenalty)
				SendChat(player, "PSR penalty is OFF, you can leave.");
			else
				SendChat(player, "Autoban is OFF, you can leave.");

			if (!m_LadderGameOver)
				SendChat(player, "This game will be saved in your ladder stats, you should play till the tree/throne is destroyed.");
		}
	}

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

	else if ((Command == "forfeit" || Command == "ff") && m_GameLoaded && !m_LadderGameOver) {
		CDIV1DotAPlayer* DotAPlayer = GetDIV1DotAPlayerFromPID(player->GetPID());

		if (DotAPlayer) {
			if (m_CreepsSpawned && ((m_GameTicks / 1000) - m_CreepsSpawnedTime) > 1500) {
				if (DotAPlayer->GetCurrentTeam() == 0) {
					if (!m_SentinelVoteFFStarted) {
						if (GetNumHumanPlayers() < 2)
							SendAllChat("Unable to start vote forfeit, you are alone in the game.");
						else {
							m_SentinelVoteFFStarted = true;
							m_SentinelVoteFFStartedTime = GetTime();

							uint32_t VotesNeeded = 0;

							for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
								if ((*i)->GetCurrentTeam() == 0 && !(*i)->GetHasLeftTheGame()) {
									// new vote FF started so reset FF votes of players that are still in the game

									(*i)->SetFFVote(false);
									++VotesNeeded;
								}
							}

							DotAPlayer->SetFFVote(true);

							CONSOLE_Print("[GAME: " + m_GameName + "] vote forfeit started by player [" + User + "] from Sentinel");
							SendAllChat("[" + User + "] voted to forfeit [1/" + UTIL_ToString(VotesNeeded) + " Sentinel]");

							if (1 >= VotesNeeded) {
								CONSOLE_Print("[GAME: " + m_GameName + "] Sentinel players voted to forfeit the game");
								SendAllChat("All Sentinel players voted to forfeit the game, Scourge won!");

								SendAllAutobanOFF();

								// SendAllChat( "Stats saving will now stop, you can leave or continue playing for fun." );
								// SendAllChat( "Use !gameinfo command for more information." );

								// SendEndMessage( );
								// m_GameOverTime = GetTime( );

								m_SentinelVoteFFStarted = false;
								m_SentinelVoteFFStartedTime = 0;

								m_Winner = 2;
								m_CollectDotAStats = false;
								m_CollectDotAStatsOverTime = m_GameTicks / 1000;
								m_FF = true;
								m_LadderGameOver = true;
								m_AutoBanOn = false;
							}
						}
					}
					else {
						uint32_t VotesNeeded = 0;
						uint32_t Votes = 0;

						for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
							if ((*i)->GetCurrentTeam() == 0 && !(*i)->GetHasLeftTheGame()) {
								++VotesNeeded;

								if ((*i)->GetFFVote())
									++Votes;
							}
						}

						if (!DotAPlayer->GetFFVote()) {
							CONSOLE_Print("[GAME: " + m_GameName + "] player [" + User + "] from Sentinel voted to forfeit, still " + UTIL_ToString(VotesNeeded - (Votes + 1)) + " votes are needed");
							SendAllChat("[" + User + "] voted to forfeit [" + UTIL_ToString(Votes + 1) + "/" + UTIL_ToString(VotesNeeded) + " Sentinel]");

							DotAPlayer->SetFFVote(true);
							++Votes;
						}
						else
							SendChat(player, "You already voted to forfeit [" + UTIL_ToString(Votes) + "/" + UTIL_ToString(VotesNeeded) + " Sentinel]");

						if (Votes >= VotesNeeded) {
							CONSOLE_Print("[GAME: " + m_GameName + "] gameover timer started (Sentinel players voted to forfeit the game)");
							SendAllChat("All Sentinel players voted to forfeit the game, Scourge won!");

							SendAllAutobanOFF();

							// SendAllChat( "Stats saving will now stop, you can leave or continue playing for fun." );
							// SendAllChat( "Use !gameinfo command for more information." );

							// SendEndMessage( );
							// m_GameOverTime = GetTime( );

							m_SentinelVoteFFStarted = false;
							m_SentinelVoteFFStartedTime = 0;

							m_Winner = 2;
							m_CollectDotAStats = false;
							m_CollectDotAStatsOverTime = m_GameTicks / 1000;
							m_FF = true;
							m_LadderGameOver = true;
							m_AutoBanOn = false;
						}
					}
				}
				else if (DotAPlayer->GetCurrentTeam() == 1) {
					if (!m_ScourgeVoteFFStarted) {
						if (GetNumHumanPlayers() < 2)
							SendAllChat("Unable to start vote forfeit, you are alone in the game.");
						else {
							m_ScourgeVoteFFStarted = true;
							m_ScourgeVoteFFStartedTime = GetTime();

							uint32_t VotesNeeded = 0;

							for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
								if ((*i)->GetCurrentTeam() == 1 && !(*i)->GetHasLeftTheGame()) {
									// new vote FF started so reset FF votes of players that are still in the game

									(*i)->SetFFVote(false);
									++VotesNeeded;
								}
							}

							DotAPlayer->SetFFVote(true);

							CONSOLE_Print("[GAME: " + m_GameName + "] vote forfeit started by player [" + User + "] from Scourge");
							SendAllChat("[" + User + "] voted to forfeit [1/" + UTIL_ToString(VotesNeeded) + " Scourge]");

							if (1 >= VotesNeeded) {
								CONSOLE_Print("[GAME: " + m_GameName + "] gameover timer started (Scourge players voted to forfeit the game)");
								SendAllChat("All Scourge players voted to forfeit the game, Sentinel won!");

								SendAllAutobanOFF();

								// SendAllChat( "Stats saving will now stop, you can leave or continue playing for fun." );
								// SendAllChat( "Use !gameinfo command for more information." );

								// SendEndMessage( );
								// m_GameOverTime = GetTime( );

								m_ScourgeVoteFFStarted = false;
								m_ScourgeVoteFFStartedTime = 0;

								m_Winner = 1;
								m_CollectDotAStats = false;
								m_CollectDotAStatsOverTime = m_GameTicks / 1000;
								m_FF = true;
								m_LadderGameOver = true;
								m_AutoBanOn = false;
							}
						}
					}
					else {
						uint32_t VotesNeeded = 0;
						uint32_t Votes = 0;

						for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
							if ((*i)->GetCurrentTeam() == 1 && !(*i)->GetHasLeftTheGame()) {
								++VotesNeeded;

								if ((*i)->GetFFVote())
									++Votes;
							}
						}

						if (!DotAPlayer->GetFFVote()) {
							CONSOLE_Print("[GAME: " + m_GameName + "] player [" + User + "] from Scourge voted to forfeit, still " + UTIL_ToString(VotesNeeded - (Votes + 1)) + " votes are needed");
							SendAllChat("[" + User + "] voted to forfeit [" + UTIL_ToString(Votes + 1) + "/" + UTIL_ToString(VotesNeeded) + " Scourge]");

							DotAPlayer->SetFFVote(true);
							++Votes;
						}
						else
							SendChat(player, "You already voted to forfeit [" + UTIL_ToString(Votes) + "/" + UTIL_ToString(VotesNeeded) + " Scourge]");

						if (Votes >= VotesNeeded) {
							CONSOLE_Print("[GAME: " + m_GameName + "] gameover timer started (Scourge players voted to forfeit the game)");
							SendAllChat("All Scourge players voted to forfeit the game, Sentinel won!");

							SendAllAutobanOFF();

							// SendAllChat( "Stats saving will now stop, you can leave or continue playing for fun." );
							// SendAllChat( "Use !gameinfo command for more information." );

							// SendEndMessage( );
							// m_GameOverTime = GetTime( );

							m_ScourgeVoteFFStarted = false;
							m_ScourgeVoteFFStartedTime = 0;

							m_Winner = 1;
							m_CollectDotAStats = false;
							m_CollectDotAStatsOverTime = m_GameTicks / 1000;
							m_FF = true;
							m_LadderGameOver = true;
							m_AutoBanOn = false;
						}
					}
				}
			}
			else {
				/*int WaitTime = 1500 - ( ( m_GameTicks / 1000 ) - m_CreepsSpawnedTime );

				if( WaitTime < 60 )
					SendChat( player, "You have to wait less than a minute to FF." );
				else if( WaitTime < 120 )
					SendChat( player, "You have to wait 1 minute more to FF." );
				else
					SendChat( player, "You have to wait " + UTIL_ToString( WaitTime / 60 ) + " minutes more to FF." );*/

				SendChat(player, "You have to wait after " + UTIL_ToString(1500 / 60) + " minutes to FF.");
			}
		}
	}

	//
	// !FORFEITCOUNT
	// !FFCOUNT
	//

	else if ((Command == "forfeitcount" || Command == "ffcount") && m_GameLoaded && !m_GameOverTime) {
		if (!m_SentinelVoteFFStarted)
			SendChat(player, "Noone from Sentinel voted to forfeit.");
		else {
			uint32_t VotesNeeded = 0;
			uint32_t Votes = 0;

			for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
				if ((*i)->GetCurrentTeam() == 0 && !(*i)->GetHasLeftTheGame()) {
					++VotesNeeded;

					if ((*i)->GetFFVote())
						++Votes;
				}
			}

			SendChat(player, "[" + UTIL_ToString(Votes) + "/" + UTIL_ToString(VotesNeeded) + "] Sentinel players voted to forfeit.");
		}

		if (!m_ScourgeVoteFFStarted)
			SendChat(player, "Noone from Scourge voted to forfeit.");
		else {
			uint32_t VotesNeeded = 0;
			uint32_t Votes = 0;

			for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
				if ((*i)->GetCurrentTeam() == 1 && !(*i)->GetHasLeftTheGame()) {
					++VotesNeeded;

					if ((*i)->GetFFVote())
						++Votes;
				}
			}

			SendChat(player, "[" + UTIL_ToString(Votes) + "/" + UTIL_ToString(VotesNeeded) + "] Scourge players voted to forfeit.");
		}
	}

	//
	// !HCL
	//

	// only allow allowed DotA modes as HCL for div1 games

	else if ((Command == "hcl" || Command == "mode") && !m_CountDownStarted) {
		if ((rootadmin || admin || owner) && !m_CountDownStarted) {
			string HCLCommandString = Payload;
			transform(HCLCommandString.begin(), HCLCommandString.end(), HCLCommandString.begin(), (int (*)(int))tolower);

			UTIL_Replace(HCLCommandString, "-", "");

			if (!HCLCommandString.empty()) {
				if ((m_Map->GetMapHCLPrefix().size() + HCLCommandString.size()) <= m_Slots.size()) {
					string HCLChars = "abcdefghijklmnopqrstuvwxyz0123456789 -=,.";

					if (HCLCommandString.find_first_not_of(HCLChars) == string ::npos) {
						// check is this DotA mode allowed in our ladder game

						uint32_t NumModes = HCLCommandString.size() / 2;
						string InvalidModes;

						for (int i = 0; i < NumModes; ++i) {
							string Mode = HCLCommandString.substr(i * 2, 2);

							if (m_GHost->m_DotAAllowedModes.find(Mode) == m_GHost->m_DotAAllowedModes.end())
								InvalidModes += Mode;
						}

						if (InvalidModes.empty()) {
							// up to this point we assumed user entered even number of characters for DotA mode

							if (HCLCommandString.size() % 2) {
								// DotA mode can't consist of odd number of characters

								SendChat(player, "Invalid DotA mode for ladder games");
								SendChat(player, "Create game with .dota command to play non-ladder game");
							}
							else {
								m_HCLCommandString = m_Map->GetMapHCLPrefix() + HCLCommandString;
								SendChat(player, m_GHost->m_Language->SettingHCL(m_HCLCommandString));
							}
						}
						else {
							SendChat(player, "Invalid DotA mode for ladder games: -" + InvalidModes);
							SendChat(player, "Create game with .dota command to play non-ladder game");
						}
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
				SendChat(player, "Name: [" + m_GameName + "], Type: [DotA ladder game], Owner: [" + m_OwnerName + "], Hostbot: [" + (*i)->GetBnetUserName() + "]");
				break;
			}
		}
	}

	//
	// !GAMENAME
	// !GN
	//

	//
	// !LOCKED
	//

	else if (Command == "locked") {
		string LockedPlayers = "Locked players: ";

		for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
			if ((*i)->GetLocked()) {
				if (LockedPlayers.empty())
					LockedPlayers += (*i)->GetName();
				else
					LockedPlayers += ", " + (*i)->GetName();

				if ((m_GameLoading || m_GameLoaded) && LockedPlayers.size() > 100) {
					// cut the text into multiple lines ingame

					SendChat(player, LockedPlayers);

					LockedPlayers.clear();
				}
			}
		}

		if (!LockedPlayers.empty())
			SendChat(player, LockedPlayers);
	}

	//
	// !RALL
	// !RANKALL
	// !RATEALL
	//

	else if (Command == "rall" || Command == "rankall" || Command == "rateall") {
		vector<std::tuple<string, double, double>> Ratings;

		for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {

			CGamePlayer* GamePlayer = GetPlayerFromPID((*i)->GetPID());

			if (GamePlayer) {
				CDBDiv1DPS* DotAPlayerSummary = GamePlayer->GetDiv1DPS();

				if (DotAPlayerSummary) {
					uint32_t gamesPlayed = DotAPlayerSummary->GetTotalWins() + DotAPlayerSummary->GetTotalLosses();
					double wr = 0.35;
					if (gamesPlayed >= 10) {
						wr = DotAPlayerSummary->GetTotalWins() / (double)gamesPlayed;
					}

					Ratings.push_back(make_tuple((*i)->GetName(), (*i)->GetRating(), wr));
				}
				else {
					Ratings.push_back(make_tuple((*i)->GetName(), (*i)->GetRating(), 0.0));
				}
			}
		}

		sort(Ratings.begin(), Ratings.end(), SortDescByRating3());

		string RatingsSummary;

		for (vector<tuple<string, double, double>>::iterator i = Ratings.begin(); i != Ratings.end(); ++i) {
			string name = std::get<0>(*i);
			double psr = std::get<1>(*i);
			double wr = (std::get<2>(*i)) * 100;

			if (RatingsSummary.empty()) {
				RatingsSummary = name + " (" + UTIL_ToString(psr, 0) + ")";
				// RatingsSummary = name + " (PSR=" + UTIL_ToString(psr, 0) + ", WR="+ UTIL_ToString(wr, 0) +"%)";
			}
			else {
				RatingsSummary += ", " + name + " (" + UTIL_ToString(psr, 0) + ")";
				// RatingsSummary += ", " + name + " (PSR=" + UTIL_ToString(psr, 0) + ", WR="+ UTIL_ToString(wr, 0) +"%)";
			}

			if ((m_GameLoading || m_GameLoaded) && RatingsSummary.size() > 100) {
				// cut the text into multiple lines ingame

				if (owner)
					SendAllChat(RatingsSummary);
				else
					SendChat(player, RatingsSummary);

				RatingsSummary.clear();
			}
		}

		if (!RatingsSummary.empty()) {
			if (owner)
				SendAllChat(RatingsSummary);
			else
				SendChat(player, RatingsSummary);
		}
	}

	//
	// !RATING
	// !R
	// !PSR
	//

	else if (Command == "r" || Command == "rating" || Command == "psr") {
		CGamePlayer* Player = NULL;
		uint32_t Matches;

		if (Payload.empty()) {
			Player = player;
			Matches = 1;
		}
		else
			Matches = GetPlayerFromNamePartial(Payload, &Player);

		if (Matches == 0)
			SendChat(player, "Unable to check player. No matches found for [" + Payload + "].");
		else if (Matches == 1) {
			// players can check only players that are spoof checked

			if (Player->GetSpoofed()) {
				CDIV1DotAPlayer* DotAPlayer = GetDIV1DotAPlayerFromPID(Player->GetPID());

				if (DotAPlayer) {
					if (DotAPlayer->GetCurrentTeam() != 0 && DotAPlayer->GetCurrentTeam() != 1) {
						// this is an observer?

						if (owner)
							SendAllChat(DotAPlayer->GetName() + "'s PSR is " + UTIL_ToString(DotAPlayer->GetRating(), 0));
						else {
							if (Payload.empty())
								SendChat(player, "Your PSR is " + UTIL_ToString(DotAPlayer->GetRating(), 0));
							else
								SendChat(player, DotAPlayer->GetName() + "'s PSR is " + UTIL_ToString(DotAPlayer->GetRating(), 0));
						}
					}
					else {
						// this is sentinel/scourge player

						vector<PairedPlayerRating> team1;
						vector<PairedPlayerRating> team2;

						for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
							string Name = (*i)->GetName();

							if ((*i)->GetCurrentTeam() == 0)
								team1.push_back(make_pair(Name, (*i)->GetRating()));
							else if ((*i)->GetCurrentTeam() == 1)
								team2.push_back(make_pair(Name, (*i)->GetRating()));
						}

						if (MASL_PROTOCOL::IsDotAEmMode(m_HCLCommandString))
							m_PSR->SetBaseKFactor(8);

						if (m_GHost->m_UseNewPSRFormula) {
							m_PSR->CalculatePSR_New(team1, team2);
						}
						else {
							m_PSR->CalculatePSR(team1, team2, m_MaxWinChanceDiffGainConstant);
						}

						if (owner)
							SendAllChat(DotAPlayer->GetName() + "'s PSR is " + UTIL_ToString(DotAPlayer->GetRating(), 0) + ", estimated +" + UTIL_ToString(m_PSR->GetPlayerGainLoss(DotAPlayer->GetName()).first, 0) + "/-" + UTIL_ToString(m_PSR->GetPlayerGainLoss(DotAPlayer->GetName()).second, 0));
						else {
							if (Payload.empty())
								SendChat(player, "Your PSR is " + UTIL_ToString(DotAPlayer->GetRating(), 0) + ", estimated +" + UTIL_ToString(m_PSR->GetPlayerGainLoss(DotAPlayer->GetName()).first, 0) + "/-" + UTIL_ToString(m_PSR->GetPlayerGainLoss(DotAPlayer->GetName()).second, 0));
							else
								SendChat(player, DotAPlayer->GetName() + "'s PSR is " + UTIL_ToString(DotAPlayer->GetRating(), 0) + ", estimated +" + UTIL_ToString(m_PSR->GetPlayerGainLoss(DotAPlayer->GetName()).first, 0) + "/-" + UTIL_ToString(m_PSR->GetPlayerGainLoss(DotAPlayer->GetName()).second, 0));
						}
					}
				}
			}
			else
				SendChat(player, "Player [" + Player->GetName() + "] is not spoof checked.");
		}
		else
			SendChat(player, "Unable to check player. Found more than one match for [" + Payload + "].");
	}

	//
	// !RATINGS
	// !RS
	//

	else if (Command == "rs" || Command == "ratings") {
		vector<PairedPlayerRating> team1;
		vector<PairedPlayerRating> team2;

		for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
			string Name = (*i)->GetName();

			if ((*i)->GetCurrentTeam() == 0)
				team1.push_back(make_pair(Name, (*i)->GetRating()));
			else if ((*i)->GetCurrentTeam() == 1)
				team2.push_back(make_pair(Name, (*i)->GetRating()));
		}

		if (m_GHost->m_UseNewPSRFormula) {
			m_PSR->CalculatePSR_New(team1, team2);
		}
		else {
			m_PSR->CalculatePSR(team1, team2, m_MaxWinChanceDiffGainConstant);
		}

		std::tuple<double, double, double> diff = getWLDiff();
		string t1wr = UTIL_ToString(std::get<0>(diff), 0);
		string t2wr = UTIL_ToString(std::get<1>(diff), 0);

		string txt = "Sentinel avg. PSR: " + UTIL_ToString(m_PSR->GetTeamAvgPSR(0), 0) + "(" + UTIL_ToString(m_PSR->GetTeamWinPerc(0), 0) + "% to win, WR=" + t1wr + "%), Scourge avg. PSR: " + UTIL_ToString(m_PSR->GetTeamAvgPSR(1), 0) + "(" + UTIL_ToString(m_PSR->GetTeamWinPerc(1), 0) + "% to win, WR=" + t2wr + "%)";

		if (owner)
			SendAllChat(txt);
		else
			SendChat(player, txt);
	}

	//
	// !OWNER (set game owner)
	//

	//
	// !REMAKE
	// !RMK
	//

	else if (Command == "remake" || Command == "rmk") {
		// fix for !rmk being called twice, once here and once in game_base

		if (m_GameLoaded && !m_LadderGameOver) {
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
					CONSOLE_Print("[GAME: " + m_GameName + "] players voted to remake the game");
					SendAllChat("Vote remake passed, " + UTIL_ToString(Votes) + " of " + UTIL_ToString(GetNumHumanPlayers()) + " players voted to remake the game.");
					SendAllChat("This game won't affect your ladder stats, game result will be a draw.");

					SendAllAutobanOFF();

					// SendEndMessage( );
					// m_GameOverTime = GetTime( );

					m_VoteRemakeStarted = false;
					m_VoteRemakeStartedTime = 0;

					m_Winner = 0;
					m_CollectDotAStats = false;
					m_CollectDotAStatsOverTime = m_GameTicks / 1000;
					m_RMK = true;
					m_LadderGameOver = true;
					m_AutoBanOn = false;
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
				m_LastVoteKickedPlayerName = Victim->GetName();

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
	// !REGION
	//

	else if (Command == "region") {
		SendChat(player, Regions::toString(m_GHost->m_region));
	}

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

	if (!Command1Found && !Command2Found)
		CBaseGame ::EventPlayerBotCommand2(player, command, payload, rootadmin, admin, owner);
}

void CDiv1DotAGame ::EventPlayerEnteredLobby(CGamePlayer* player)
{
	CBaseGame ::EventPlayerEnteredLobby(player);

	m_CachedBalanceString.clear();

	double Rating = MASL_PROTOCOL ::DB_DIV1_STARTING_PSR;
	CDBDiv1DPS* DIV1DotAPlayerSummary = player->GetDiv1DPS();

	if (DIV1DotAPlayerSummary)
		Rating = DIV1DotAPlayerSummary->GetRating();

	bool Locked = false;

	if (IsOwner(player->GetName()))
		Locked = true;

	m_DotAPlayers.push_back(new CDIV1DotAPlayer(this, player->GetPID(), player->GetName(), GetServerID(player->GetJoinedRealm()), Rating, Locked));
}

void CDiv1DotAGame ::EventPlayerDeleted(CGamePlayer* player)
{
	CBaseGame ::EventPlayerDeleted(player);

	m_CachedBalanceString.clear();

	if (m_GameLoading || m_GameLoaded) {
		// player left while game loading or game in progress

		if (m_GameLoading && !m_GameLoaded) {
			// player left while game loading

			m_PlayerLeftDuringLoading = true;

			m_Winner = 0;
			m_CollectDotAStats = false;
			m_CollectDotAStatsOverTime = m_GameTicks / 1000;
			m_LadderGameOver = true;
			m_AutoBanOn = false;
		}
		else if (m_GameLoaded) {
			// player left while game is in progress

			this->selectOneGameEventReporter();

			CDIV1DotAPlayer* DotAPlayer = NULL;
			uint32_t NumSentinelPlayers = 0;
			uint32_t NumScourgePlayers = 0;

			for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
				if (player->GetPID() == (*i)->GetPID()) {
					(*i)->SetHasLeftTheGame(true);
					DotAPlayer = *i;
				}

				if (!(*i)->GetHasLeftTheGame()) {
					if ((*i)->GetCurrentTeam() == 0)
						++NumSentinelPlayers;
					else if ((*i)->GetCurrentTeam() == 1)
						++NumScourgePlayers;
				}
			}

			if (!m_LadderGameOver && !m_TreeLowHP && !m_ThroneLowHP && DotAPlayer && (DotAPlayer->GetCurrentTeam() == 0 || DotAPlayer->GetCurrentTeam() == 1))
				// give negative PSR to all leavers (even if autoban is OFF) while ladder game isn't over and tree/throne not on very low HP
				DotAPlayer->SetRecvNegativePSR(true);

			if (!m_LadderGameOver && m_AutoBanOn && DotAPlayer && (DotAPlayer->GetCurrentTeam() == 0 || DotAPlayer->GetCurrentTeam() == 1)) {
				if (!m_CreepsSpawned) {
					// player left the game before creeps spawned

					if (DotAPlayer->GetName() != m_LastVoteKickedPlayerName) {
						DotAPlayer->SetBanned(true);
						m_GHost->m_Manager->SendUserWasBanned(player->GetJoinedRealm(), player->GetName(), m_MySQLGameID, string());

						const string penalty = m_GHost->m_ReplaceAutobanWithPSRPenalty ? " penalized with multiples of PSR." : " was autobanned.";

						SendAllChat(player->GetName() + " " + player->GetLeftReason() + ", " + DotAPlayer->GetName() + penalty);
					}
					else
						SendAllChat(player->GetName() + " " + player->GetLeftReason() + ".");

					SendAllChat("This game won't affect your ladder stats, game result will be a draw.");

					SendAllAutobanOFF();

					m_Winner = 0;
					m_CollectDotAStats = false;
					m_CollectDotAStatsOverTime = m_GameTicks / 1000;
					m_LadderGameOver = true;
					m_AutoBanOn = false;
				}
				else if (m_CreepsSpawned && ((m_GameTicks / 1000) - m_CreepsSpawnedTime) < m_GHost->m_msGameTimeBeforeAutoban) {
					// player left the game under 5 minutes DotA time

					if (DotAPlayer->GetName() != m_LastVoteKickedPlayerName) {
						DotAPlayer->SetBanned(true);
						m_GHost->m_Manager->SendUserWasBanned(player->GetJoinedRealm(), player->GetName(), m_MySQLGameID, string());

						const string penalty = m_GHost->m_ReplaceAutobanWithPSRPenalty ? " penalized with multiples of PSR." : " was autobanned.";

						SendAllChat(player->GetName() + " " + player->GetLeftReason() + ", " + DotAPlayer->GetName() + penalty);
					}
					else
						SendAllChat(player->GetName() + " " + player->GetLeftReason() + ".");

					SendAllChat("This game won't affect your ladder stats, game result will be a draw.");

					SendAllAutobanOFF();

					m_Winner = 0;
					m_CollectDotAStats = false;
					m_CollectDotAStatsOverTime = m_GameTicks / 1000;
					m_LadderGameOver = true;
					m_AutoBanOn = false;
				}
				else {
					if (DotAPlayer->GetCurrentTeam() == 0 && NumSentinelPlayers == 4) {
						// sentinel player left, sentinel team is now 4 players

						if (DotAPlayer->GetName() != m_LastVoteKickedPlayerName) {
							DotAPlayer->SetBanned(true);
							m_GHost->m_Manager->SendUserWasBanned(player->GetJoinedRealm(), player->GetName(), m_MySQLGameID, string());

							const string penalty = m_GHost->m_ReplaceAutobanWithPSRPenalty ? " penalized with multiples of PSR." : " was autobanned.";
							SendAllChat(player->GetName() + " " + player->GetLeftReason() + ", " + DotAPlayer->GetName() + penalty);
						}
						else
							SendAllChat(player->GetName() + " " + player->GetLeftReason() + ".");

						SendAllAutobanON();
					}
					else if (DotAPlayer->GetCurrentTeam() == 1 && NumScourgePlayers == 4) {
						// scourge player left, scourge team is now 4 players

						if (DotAPlayer->GetName() != m_LastVoteKickedPlayerName) {
							DotAPlayer->SetBanned(true);
							m_GHost->m_Manager->SendUserWasBanned(player->GetJoinedRealm(), player->GetName(), m_MySQLGameID, string());

							const string penalty = m_GHost->m_ReplaceAutobanWithPSRPenalty ? " penalized with multiples of PSR." : " was autobanned.";
							SendAllChat(player->GetName() + " " + player->GetLeftReason() + ", " + DotAPlayer->GetName() + penalty);
						}
						else
							SendAllChat(player->GetName() + " " + player->GetLeftReason() + ".");

						SendAllAutobanON();
					}
					else if (DotAPlayer->GetCurrentTeam() == 0 && NumSentinelPlayers == 3) {
						// sentinel player left, sentinel team is now 3 players

						if (DotAPlayer->GetName() != m_LastVoteKickedPlayerName) {
							DotAPlayer->SetBanned(true);
							m_GHost->m_Manager->SendUserWasBanned(player->GetJoinedRealm(), player->GetName(), m_MySQLGameID, string());

							const string penalty = m_GHost->m_ReplaceAutobanWithPSRPenalty ? " penalized with multiples of PSR." : " was autobanned.";
							SendAllChat(player->GetName() + " " + player->GetLeftReason() + ", " + DotAPlayer->GetName() + penalty);
						}
						else
							SendAllChat(player->GetName() + " " + player->GetLeftReason() + ".");

						if (((m_GameTicks / 1000) - m_CreepsSpawnedTime) > 900 ||
							m_SentinelTopRax == 0 || m_SentinelMidRax == 0 || m_SentinelBotRax == 0 || m_ScourgeTopRax == 0 || m_ScourgeMidRax == 0 || m_ScourgeBotRax == 0) {

							SendAllChat("This game will be saved in your ladder stats, you should play till the end.");
						}
						else {
							SendAllChat("This game won't affect your ladder stats, game result will be a draw.");

							SendAllAutobanOFF();

							m_Winner = 0;
							m_CollectDotAStats = false;
							m_CollectDotAStatsOverTime = m_GameTicks / 1000;
							m_LadderGameOver = true;
						}

						m_AutoBanOn = false;
					}
					else if (DotAPlayer->GetCurrentTeam() == 1 && NumScourgePlayers == 3) {
						// scourge player left, scourge team is now 3 players

						if (DotAPlayer->GetName() != m_LastVoteKickedPlayerName) {
							DotAPlayer->SetBanned(true);
							m_GHost->m_Manager->SendUserWasBanned(player->GetJoinedRealm(), player->GetName(), m_MySQLGameID, string());

							const string penalty = m_GHost->m_ReplaceAutobanWithPSRPenalty ? " penalized with multiples of PSR." : " was autobanned.";

							SendAllChat(player->GetName() + " " + player->GetLeftReason() + ", " + DotAPlayer->GetName() + penalty);
						}
						else
							SendAllChat(player->GetName() + " " + player->GetLeftReason() + ".");

						if (((m_GameTicks / 1000) - m_CreepsSpawnedTime) > 900 ||
							m_SentinelTopRax == 0 || m_SentinelMidRax == 0 || m_SentinelBotRax == 0 || m_ScourgeTopRax == 0 || m_ScourgeMidRax == 0 || m_ScourgeBotRax == 0) {
							// ingame time is past 15 minutes or one side of sentinel/scourge base has been destroyed

							SendAllChat("This game will be saved in your ladder stats, you should play till the end.");
						}
						else {
							SendAllChat("This game won't affect your ladder stats, game result will be a draw.");

							SendAllAutobanOFF();

							m_Winner = 0;
							m_CollectDotAStats = false;
							m_CollectDotAStatsOverTime = m_GameTicks / 1000;
							m_LadderGameOver = true;
						}

						m_AutoBanOn = false;
					}
				}
			}
			else
				SendAllChat(player->GetName() + " " + player->GetLeftReason() + ".");
		}
	}
	else {
		// player left lobby

		for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
			if (player->GetPID() == (*i)->GetPID()) {
				delete *i;
				m_DotAPlayers.erase(i);
				break;
			}
		}
	}

	// abort vote FF

	m_SentinelVoteFFStarted = false;
	m_ScourgeVoteFFStartedTime = 0;

	m_ScourgeVoteFFStarted = false;
	m_ScourgeVoteFFStartedTime = 0;
}

void CDiv1DotAGame ::EventGameStarted()
{
	CBaseGame ::EventGameStarted();

	CONSOLE_Print("Div1 dota game started event.");

	m_Mode1 = m_HCLCommandString;

	// while the game is in lobby CDiv1DotAPlayer->GetTeam( ) calls CGamePlayer from CBaseGame to get lobby team/color
	// when the game is in progress CDiv1DotAPlayer->GetTeam( ) returns internal member value that we initially set here
	// map stats code could change CDiv1DotAPlayer's team/color during game

	for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i) {
		for (list<CDIV1DotAPlayer*>::iterator j = m_DotAPlayers.begin(); j != m_DotAPlayers.end(); ++j) {
			if ((*i)->GetPID() == (*j)->GetPID()) {
				unsigned char SID = GetSIDFromPID((*i)->GetPID());
				unsigned char Team = 255;
				unsigned char Color = 255;

				if (SID < m_Slots.size()) {
					Team = m_Slots[SID].GetTeam();
					Color = m_Slots[SID].GetColour();
				}

				if (Team == 12) {
					// don't waste space with observers, no DotA stats is collected for them in DIV1 DotA games

					delete *j;
					m_DotAPlayers.erase(j);
				}
				else {
					(*j)->SetLobbyColor(Color);
					(*j)->SetNewColor(Color);
				}

				break;
			}
		}
	}
}

void CDiv1DotAGame::EventGameLoaded()
{
	CBaseGame ::EventGameLoaded();

	CONSOLE_Print("Div1 dota game loaded event.");

	if (m_PlayerLeftDuringLoading) {
		SendAllChat("Player left the game during loading! This game won't affect your ladder stats, game result will be a draw.");

		SendAllAutobanOFF();
	}
	else {
		SendAllChat("If you leave before the tree/throne is destroyed you risk getting autobanned or penalized with PSR loss, type " + string(1, m_GameCommandTrigger) + "autoban for more info.");
	}

	// Select a single player to become the event sender. By default all are selected.
	this->selectOneGameEventReporter();
}

void CDiv1DotAGame ::SwapSlots(unsigned char SID1, unsigned char SID2)
{
	CBaseGame ::SwapSlots(SID1, SID2);

	// don't clear the cache if player changed position in the same team, only when player moves to another team

	if ((SID1 >= 0 && SID1 <= 4 && SID2 >= 5 && SID2 <= 9) ||
		(SID1 >= 5 && SID1 <= 9 && SID2 >= 0 && SID2 <= 4)) {
		m_CachedBalanceString.clear();
	}
}

void CDiv1DotAGame ::ChangeOwner(string name)
{
	CBaseGame ::ChangeOwner(name);

	transform(name.begin(), name.end(), name.begin(), (int (*)(int))tolower);

	for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
		string Name = (*i)->GetName();
		transform(Name.begin(), Name.end(), Name.begin(), (int (*)(int))tolower);

		if (Name == name) {
			(*i)->SetLocked(true);
			break;
		}
	}
}

bool CDiv1DotAGame ::IsGameDataSaved()
{
	return m_Saved;
}

void CDiv1DotAGame ::SaveGameData()
{
	CONSOLE_Print("[GAME: " + m_GameName + "] saving game data");

	time_t Now = time(NULL);
	char Time[17];
	memset(Time, 0, sizeof(char) * 17);
	strftime(Time, sizeof(char) * 17, "%Y-%m-%d %H-%M", localtime(&Now));
	string MinString = UTIL_ToString((m_GameTicks / 1000) / 60);
	string SecString = UTIL_ToString((m_GameTicks / 1000) % 60);

	if (MinString.size() == 1)
		MinString.insert(0, "0");

	if (SecString.size() == 1)
		SecString.insert(0, "0");

	m_ReplayName = UTIL_FileSafeName(string(Time) + " " + m_GameName + " (" + MinString + "m" + SecString + "s).w3g");

	string LobbyLog;

	if (m_LobbyLog)
		LobbyLog = m_LobbyLog->GetGameLog();

	string GameLog;

	if (m_GameLog)
		GameLog = m_GameLog->GetGameLog();

	// fix for when dota map doesn't send end game info (sometimes it doesn't send low HP ancient info as well)
	// most often only end game data is not sent, 10% msg is skiped sometimes, 25% msg is skiped rarely

	if (!m_LadderGameOver && m_Winner == 0 && (m_TreeLowHP || m_ThroneLowHP))
		m_Winner = (m_TreeLowHPTime > m_ThroneLowHPTime) ? 2 : 1;

	bool Scored = (m_Winner == 0 ? false : true);

	stringstream GameInfo;

	GameInfo << m_Winner;
	GameInfo << " " << m_CreepsSpawnedTime;
	GameInfo << " " << m_CollectDotAStatsOverTime;
	GameInfo << " " << m_Min;
	GameInfo << " " << m_Sec;
	// GameInfo << " " << m_Mode1;
	// GameInfo << " " << m_Mode2;
	MASL_PROTOCOL ::SSWriteString(GameInfo, m_Mode1);
	MASL_PROTOCOL ::SSWriteString(GameInfo, m_Mode2);

	GameInfo << " " << m_DotAPlayers.size(); // uint32_t DBDotAPlayersSize;

	for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
		GameInfo << " " << (uint32_t)(*i)->GetLobbyColor();
		GameInfo << " " << (*i)->GetLevel();
		GameInfo << " " << (*i)->GetKills();
		GameInfo << " " << (*i)->GetDeaths();
		GameInfo << " " << (*i)->GetCreepKills();
		GameInfo << " " << (*i)->GetCreepDenies();
		GameInfo << " " << (*i)->GetAssists();
		GameInfo << " " << (*i)->GetGold();
		GameInfo << " " << (*i)->GetNeutralKills();
		MASL_PROTOCOL ::SSWriteString(GameInfo, (*i)->GetItem(0));
		MASL_PROTOCOL ::SSWriteString(GameInfo, (*i)->GetItem(1));
		MASL_PROTOCOL ::SSWriteString(GameInfo, (*i)->GetItem(2));
		MASL_PROTOCOL ::SSWriteString(GameInfo, (*i)->GetItem(3));
		MASL_PROTOCOL ::SSWriteString(GameInfo, (*i)->GetItem(4));
		MASL_PROTOCOL ::SSWriteString(GameInfo, (*i)->GetItem(5));
		MASL_PROTOCOL ::SSWriteString(GameInfo, (*i)->GetHero());
		GameInfo << " " << (uint32_t)(*i)->GetCurrentColor();
		GameInfo << " " << (*i)->GetTowerKills();
		GameInfo << " " << (*i)->GetRaxKills();
		GameInfo << " " << (*i)->GetCourierKills();
	}

	GameInfo << " " << Scored;
	GameInfo << " " << m_FF;
	GameInfo << " " << m_DotAPlayers.size();

	for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
		GameInfo << " " << (*i)->GetName();
		GameInfo << " " << (*i)->GetServerID();
		GameInfo << " " << (uint32_t)(*i)->GetLobbyColor();
		GameInfo << " " << (uint32_t)(*i)->GetLobbyTeam();
		GameInfo << " " << (*i)->GetBanned();
		GameInfo << " " << (*i)->GetRecvNegativePSR();
	}

	m_GHost->m_Manager->SendGameSave(m_MySQLGameID, m_LobbyDuration, LobbyLog, GameLog, m_ReplayName, m_MapPath, m_GameName, m_OwnerName, m_GameTicks / 1000, m_GameState, m_CreatorName, m_CreatorServer, m_DBGamePlayers, m_RMK, m_GameType, GameInfo.str());

	m_Saved = true;
}

CDIV1DotAPlayer* CDiv1DotAGame ::GetDIV1DotAPlayerFromPID(unsigned char PID)
{
	for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
		if ((*i)->GetPID() == PID)
			return *i;
	}

	return NULL;
}

CDIV1DotAPlayer* CDiv1DotAGame ::GetDIV1DotAPlayerFromSID(unsigned char SID)
{
	if (SID < m_Slots.size())
		return GetDIV1DotAPlayerFromPID(m_Slots[SID].GetPID());

	return NULL;
}

CDIV1DotAPlayer* CDiv1DotAGame ::GetDIV1DotAPlayerFromLobbyColor(unsigned char color)
{
	for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
		if ((*i)->GetLobbyColor() == color)
			return *i;
	}

	return NULL;
}

void CDiv1DotAGame ::BalanceSlots()
{
	if (m_Players.size() < 2 || m_GameLoading || m_GameLoaded)
		return;

	vector<pair<unsigned char, double>> Players;
	unsigned char OwnerPID = 0;

	for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
		if ((*i)->GetCurrentTeam() == 0 || (*i)->GetCurrentTeam() == 1) {
			Players.push_back(make_pair((*i)->GetPID(), (*i)->GetRating()));

			if (IsOwner((*i)->GetName()))
				OwnerPID = (*i)->GetPID();
		}
	}

	sort(Players.begin(), Players.end(), SortDescByRating2());

	vector<unsigned char> Team1;
	vector<unsigned char> Team2;
	uint32_t OwnerTeam = 0;

	for (unsigned int i = 0; i < Players.size(); ++i) {
		if (i % 2) {
			Team1.push_back(Players[i].first);

			if (Players[i].first == OwnerPID)
				OwnerTeam = 1;
		}
		else {
			Team2.push_back(Players[i].first);

			if (Players[i].first == OwnerPID)
				OwnerTeam = 2;
		}
	}

	if (OwnerTeam == 1) {
		for (unsigned int i = 0; i < Team1.size(); ++i)
			SwapSlots(GetSIDFromPID(Team1[i]), i);

		for (unsigned int i = 0; i < Team2.size(); ++i)
			SwapSlots(GetSIDFromPID(Team2[i]), i + 5);
	}
	else {
		for (unsigned int i = 0; i < Team2.size(); ++i)
			SwapSlots(GetSIDFromPID(Team2[i]), i);

		for (unsigned int i = 0; i < Team1.size(); ++i)
			SwapSlots(GetSIDFromPID(Team1[i]), i + 5);
	}
}

void CDiv1DotAGame ::BalanceSlots2(const set<unsigned char>& locked_slots)
{
	if (m_DotAPlayers.size() < 2 || m_GameLoading || m_GameLoaded)
		return;

	list<CBalanceSlot> SlotsToBalance;
	set<unsigned char> TakenSlots;

	for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
		// skip observers (team != 0 and team != 1)
		if ((*i)->GetCurrentTeam() != 0 && (*i)->GetCurrentTeam() != 1)
			continue;

		bool Locked = false;
		unsigned char SID = GetSIDFromPID((*i)->GetPID());

		if ((*i)->GetLocked() || locked_slots.find(SID) != locked_slots.end())
			Locked = true;

		if (Locked)
			TakenSlots.insert(SID);

		SlotsToBalance.push_back(CBalanceSlot(Locked, (*i)->GetPID(), (*i)->GetCurrentTeam(), (*i)->GetRating()));
	}

	BalanceSlots2Recursive(SlotsToBalance);

	for (list<CBalanceSlot>::iterator i = SlotsToBalance.begin(); i != SlotsToBalance.end(); ++i) {
		if ((*i).m_Locked)
			continue;

		unsigned char j;
		unsigned char end;

		if ((*i).m_Team == 0) {
			j = 0;
			end = 5;
		}
		else if ((*i).m_Team == 1) {
			j = 5;
			end = 10;
		}

		for (; j < end; ++j) {
			if (TakenSlots.find(j) == TakenSlots.end()) {
				SwapSlots(GetSIDFromPID((*i).m_PID), j);
				TakenSlots.insert(j);
				break;
			}
		}
	}
}

unsigned int CDiv1DotAGame ::BalanceSlots2Recursive(list<CBalanceSlot>& slots_to_balance)
{
	static unsigned int call = 0;
	++call;
	unsigned int current_call = call;
	// cout << "call #" << call << endl;

	unsigned int team1_players_count = 0;
	unsigned int team2_players_count = 0;
	int team1_rating = 0;
	int team2_rating = 0;
	int rating_diff = 0;
	int next_rating_diff = 999999;

	// teams static variable must be resetted in the last function call execution
	static unsigned int teams = 0;
	unsigned int current_teams = teams++;

	int binary_position = 0;

	for (list<CBalanceSlot>::iterator i = slots_to_balance.begin(); i != slots_to_balance.end(); ++i) {
		if ((current_teams & (int)pow(2.0, binary_position)) == 0) {
			++team1_players_count;
			team1_rating += (*i).m_Rating;
		}
		else {
			++team2_players_count;
			team2_rating += (*i).m_Rating;
		}

		// cout << "teams:" << teams << " pow(2, binary_position):" << pow(2.0, binary_position) << endl;

		++binary_position;
	}

	if (team1_players_count == 0 || team2_players_count == 0 || team1_players_count > 5 || team2_players_count > 5)
		rating_diff = 999999;
	else
		rating_diff = abs(team1_rating - team2_rating);

	// cout << "team2_players_count:" << team2_players_count << " slots_to_balance.size( ):" << slots_to_balance.size( ) << endl;

	if (team2_players_count < slots_to_balance.size()) {
		next_rating_diff = BalanceSlots2Recursive(slots_to_balance);
	}
	else {
		// cout << "this should be the last call, call #" << call << endl;
		teams = 0; // this is the last function call execution, reset static variable
	}

	if (rating_diff < next_rating_diff) {
		binary_position = 0;

		// cout << "call #" << current_call << endl;
		// cout << "rating_diff:" << rating_diff << " next_rating_diff:" << next_rating_diff << endl;

		for (list<CBalanceSlot>::iterator i = slots_to_balance.begin(); i != slots_to_balance.end(); ++i) {
			if ((current_teams & (int)pow(2.0, binary_position)) == 0) {
				if ((*i).m_Locked && (*i).m_Team == 1) {
					// this team layout is invalid
					// this player is locked to the scourge team, he can't be swaped to sentinel

					rating_diff = 999999;
					break;
				}

				(*i).m_Team = 0;
			}
			else {
				if ((*i).m_Locked && (*i).m_Team == 0) {
					// this team layout is invalid
					// this player is locked to the sentinel team, he can't be swaped to scourge

					rating_diff = 999999;
					break;
				}

				(*i).m_Team = 1;
			}

			++binary_position;
		}
	}

	return rating_diff < next_rating_diff ? rating_diff : next_rating_diff;
}

void CDiv1DotAGame ::CheckMinPSR()
{
	if (m_MinPSR != 0) {
		for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
			if ((*i)->GetRating() < m_MinPSR) {
				CGamePlayer* GamePlayer = GetPlayerFromPID((*i)->GetPID());

				if (GamePlayer) {
					if (!GamePlayer->GetReserved()) {
						GamePlayer->SetDeleteMe(true);
						GamePlayer->SetLeftReason("was kicked for low PSR " + UTIL_ToString((*i)->GetRating(), 0) + " < " + UTIL_ToString(m_MinPSR));
						GamePlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
						m_GHost->m_Manager->SendGamePlayerLeftLobbyInform(GamePlayer->GetSpoofedRealm(), GamePlayer->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_MIN_PSR);
						OpenSlot(GetSIDFromPID(GamePlayer->GetPID()), false);
						SendAllChat("Kicking player " + GamePlayer->GetName() + " with PSR lower than " + UTIL_ToString(m_MinPSR) + ".");
					}
				}
			}
		}
	}
}
void CDiv1DotAGame ::CheckMaxPSR()
{
	if (m_MaxPSR != 0) {
		for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
			if ((*i)->GetRating() > m_MaxPSR) {
				CGamePlayer* GamePlayer = GetPlayerFromPID((*i)->GetPID());

				if (GamePlayer) {
					if (!GamePlayer->GetReserved()) {
						GamePlayer->SetDeleteMe(true);
						GamePlayer->SetLeftReason("was kicked for high PSR " + UTIL_ToString((*i)->GetRating(), 0) + " > " + UTIL_ToString(m_MaxPSR));
						GamePlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
						m_GHost->m_Manager->SendGamePlayerLeftLobbyInform(GamePlayer->GetSpoofedRealm(), GamePlayer->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_MAX_PSR);
						OpenSlot(GetSIDFromPID(GamePlayer->GetPID()), false);
						SendAllChat("Kicking player " + GamePlayer->GetName() + " with PSR greater than " + UTIL_ToString(m_MaxPSR) + ".");
					}
				}
			}
		}
	}
}
void CDiv1DotAGame ::CheckMinGames(string User)
{
	if (m_MinGames != 0) {
		CGamePlayer* GamePlayer = GetPlayerFromName(User, true);
		if (GamePlayer) {
			if (!GamePlayer->GetReserved()) {
				CDBDiv1DPS* DotAPlayerSummary = GamePlayer->GetDiv1DPS();
				if (DotAPlayerSummary) {
					if (DotAPlayerSummary->GetTotalGames() < m_MinGames) {
						GamePlayer->SetDeleteMe(true);
						GamePlayer->SetLeftReason("was kicked for low number of games " + UTIL_ToString(DotAPlayerSummary->GetTotalGames()) + " < " + UTIL_ToString(m_MinGames));
						GamePlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
						m_GHost->m_Manager->SendGamePlayerLeftLobbyInform(GamePlayer->GetSpoofedRealm(), GamePlayer->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_MIN_GAMES);
						OpenSlot(GetSIDFromPID(GamePlayer->GetPID()), false);
					}
				}
				else {
					GamePlayer->SetDeleteMe(true);
					GamePlayer->SetLeftReason("was kicked for not playing any games");
					GamePlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
					m_GHost->m_Manager->SendGamePlayerLeftLobbyInform(GamePlayer->GetSpoofedRealm(), GamePlayer->GetName(), MASL_PROTOCOL::STM_PLAYERLEAVE_LOBBY_MIN_GAMES);
					OpenSlot(GetSIDFromPID(GamePlayer->GetPID()), false);
				}
			}
		}
	}
}

void CDiv1DotAGame::subtractSentinelTopRaxV(CGamePlayer* reporter)
{
	m_SentinelTopRaxV[reporter->GetPID()]--;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_SentinelTopRaxV);
	m_SentinelTopRax = newValue;
}

void CDiv1DotAGame::subtractSentinelMidRaxV(CGamePlayer* reporter)
{
	m_SentinelMidRaxV[reporter->GetPID()]--;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_SentinelMidRaxV);
	m_SentinelMidRax = newValue;
}

void CDiv1DotAGame::subtractSentinelBotRaxV(CGamePlayer* reporter)
{
	m_SentinelBotRaxV[reporter->GetPID()]--;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_SentinelBotRaxV);
	m_SentinelBotRax = newValue;
}

void CDiv1DotAGame::subtractScourgeTopRaxV(CGamePlayer* reporter)
{
	m_ScourgeTopRaxV[reporter->GetPID()]--;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_ScourgeTopRaxV);
	m_ScourgeTopRax = newValue;
}

void CDiv1DotAGame::subtractScourgeMidRaxV(CGamePlayer* reporter)
{
	m_ScourgeMidRaxV[reporter->GetPID()]--;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_ScourgeMidRaxV);
	m_ScourgeMidRax = newValue;
}

void CDiv1DotAGame::subtractScourgeBotRaxV(CGamePlayer* reporter)
{
	m_ScourgeBotRaxV[reporter->GetPID()]--;
	uint32_t newValue = UTIL_GetMostFrequentValue(&m_ScourgeBotRaxV);
	m_ScourgeBotRax = newValue;
}

void CDiv1DotAGame::recalcPSR()
{
	vector<PairedPlayerRating> team1;
	vector<PairedPlayerRating> team2;

	for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {
		string Name = (*i)->GetName();

		if ((*i)->GetCurrentTeam() == 0)
			team1.push_back(make_pair(Name, (*i)->GetRating()));
		else if ((*i)->GetCurrentTeam() == 1)
			team2.push_back(make_pair(Name, (*i)->GetRating()));
	}

	if (m_GHost->m_UseNewPSRFormula) {
		m_PSR->CalculatePSR_New(team1, team2);
	}
	else {
		m_PSR->CalculatePSR(team1, team2, m_MaxWinChanceDiffGainConstant);
	}
}

std::tuple<double, double, double> CDiv1DotAGame::getWLDiff()
{

	double tWL[2] = { 0.0, 0.0 };
	int t1Cnt = 0;
	int t2Cnt = 0;

	for (list<CDIV1DotAPlayer*>::iterator i = m_DotAPlayers.begin(); i != m_DotAPlayers.end(); ++i) {

		CGamePlayer* GamePlayer = GetPlayerFromPID((*i)->GetPID());

		if (GamePlayer) {
			CDBDiv1DPS* DotAPlayerSummary = GamePlayer->GetDiv1DPS();
			int team = (*i)->GetCurrentTeam();

			if (DotAPlayerSummary) {

				if (team == 0 || team == 1) {
					uint32_t gamesPlayed = DotAPlayerSummary->GetTotalWins() + DotAPlayerSummary->GetTotalLosses();

					if (gamesPlayed >= 10) {
						tWL[team] += (DotAPlayerSummary->GetTotalWins() / (double)gamesPlayed);
					}
					else {
						tWL[team] += 0.35; // Some kind of WL approximation for a new player I guess
					}
				}
			}
			else {
				tWL[team] += 0.35;
			}

			if (team == 0)
				t1Cnt++;
			if (team == 1)
				t2Cnt++;
		}
		else {
			CONSOLE_Print("ERROR: GamePlayer invalid when calculating WL diff.");
			return make_tuple(0, 0, 0); // error?
		}
	}

	CONSOLE_Print("T1 WL is " + UTIL_ToString(tWL[0], 1) + " with " + UTIL_ToString(t1Cnt) + " players");
	CONSOLE_Print("T2 WL is " + UTIL_ToString(tWL[1], 1) + " with " + UTIL_ToString(t2Cnt) + " players");

	if (t1Cnt == 0)
		tWL[0] = 0;
	else
		tWL[0] = 100 * tWL[0] / t1Cnt;

	if (t2Cnt == 0)
		tWL[1] = 0;
	else
		tWL[1] = 100 * tWL[1] / t2Cnt;

	CONSOLE_Print("T1 WL average is " + UTIL_ToString(tWL[0], 1));
	CONSOLE_Print("T2 WL average is " + UTIL_ToString(tWL[1], 1));

	double res = abs(tWL[0] - tWL[1]);

	CONSOLE_Print("Calculated WL diff is " + UTIL_ToString(res, 1));

	return std::make_tuple(tWL[0], tWL[1], res);
}