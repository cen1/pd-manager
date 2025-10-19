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

#include "bnet_tour.h"
#include "bncsutilinterface.h"
#include "bnet.h"
#include "bnetprotocol.h"
#include "commandpacket.h"
#include "config.h"
#include "ghost.h"
#include "ghostdb.h"
#include "language.h"
#include "masl_manager.h"
#include "masl_protocol_2.h"
#include "masl_slavebot.h"
#include "socket.h"
#include "util.h"

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

using namespace boost ::filesystem;

//
// CTourBNET
//

void CTourBNET ::ProcessChatEvent(CIncomingChatEvent* chatEvent)
{
	CBNETProtocol ::IncomingChatEvent Event = chatEvent->GetChatEvent();
	bool Whisper = (Event == CBNETProtocol ::EID_WHISPER);
	string User = chatEvent->GetUser();
	string Message = chatEvent->GetChatMessage();

	if (Event == CBNETProtocol ::EID_WHISPER || Event == CBNETProtocol ::EID_TALK) {
		if (Event == CBNETProtocol ::EID_WHISPER) {
			CONSOLE_Print("[WHISPER: " + m_ServerAlias + "] [" + User + "] " + Message);
			m_GHost->EventBNETWhisper(this, User, Message);
		}
		else {
			CONSOLE_Print("[LOCAL: " + m_ServerAlias + "] [" + User + "] " + Message);
			m_GHost->EventBNETChat(this, User, Message);
		}

		CDBPlayer* Player = NULL;

		for (list<CPlayers*>::iterator i = m_GHost->m_Players.begin(); i != m_GHost->m_Players.end(); ++i) {
			if ((*i)->GetServerID() == m_ServerID) {
				Player = (*i)->GetPlayer(User);
				break;
			}
		}

		// delete Player object later if it's temporary object

		bool DeletePlayer = false;

		if (!Player) {
			Player = new CDBPlayer(0, 0, 1);
			DeletePlayer = true;
		}

		if (IsRootAdmin(User))
			Player->SetAccessLevel(9001);

		// handle bot commands

		if (!Message.empty() && Message[0] == m_CommandTrigger) {
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

			// CONSOLE_Print( "[BNET: " + m_ServerAlias + "] admin [" + User + "] sent command [" + Message + "]" );
			CONSOLE_Print("[BNET: " + m_ServerAlias + "] user [" + User + " (" + UTIL_ToString(Player->GetAccessLevel()) + ")] sent command [" + Message + "]");

			bool IsAdmin = false;

			if (Player)
				IsAdmin = Player->GetIsAdmin();

			if (IsRootAdmin(User) || IsAdmin) {
				//
				// !ADD
				//

				if (Command == "add") {
					bool found = false;
					string playerName;
					if (!Payload.empty()) {
						playerName = Payload;
						transform(playerName.begin(), playerName.end(), playerName.begin(), (int (*)(int))tolower);
						for (vector<string>::iterator i = m_TourPlayers.begin(); !found && i != m_TourPlayers.end(); ++i) {
							if (playerName == *i)
								found = true;
						}
						for (vector<string>::iterator i = m_TourPlayersWaiting.begin(); !found && i != m_TourPlayersWaiting.end(); ++i) {
							if (playerName == *i)
								found = true;
						}
						if (found)
							SendChatCommand("Player " + playerName + " is already in the tournament.", User);
						else {
							m_TourPlayersWaiting.push_back(playerName);
							SendChatCommand("Player " + playerName + " successfully added to the tournament.", User);
						}
					}
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

				else if ((Command == "ban" || Command == "cban") && !Payload.empty()) {
					string Victim;
					string Reason;

					stringstream SS;
					SS << Payload;
					SS >> Victim;

					if (!SS.eof()) {
						getline(SS, Reason);
						SendChatCommand("/ban " + Victim + " " + User + ": " + Reason + ".");
					}
					else
						SendChatCommand("/ban " + Victim + " You were banned by " + User + ".");
				}

				//
				// !CHANNEL (change channel)
				//

				else if (Command == "channel" && !Payload.empty()) {
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

				else if ((Command == "unban" || Command == "cunban") && !Payload.empty()) {
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

				// !COM

				// !COMSTATUS

				//
				// !DEVOICE
				//

				else if (Command == "devoice" && !Payload.empty()) {
					SendChatCommand("/devoice " + Payload);
				}

				//
				// !DEVOICEME
				//

				else if (Command == "devoiceme")
					SendChatCommand("/devoice " + User);

				//
				// !ENABLE
				//

				//
				// !EXIT
				// !QUIT
				//

				//
				// !KICK
				// !CKICK
				//

				else if ((Command == "kick" || Command == "ckick") && !Payload.empty()) {
					string Victim;
					string Reason;

					stringstream SS;
					SS << Payload;
					SS >> Victim;

					if (!SS.eof()) {
						getline(SS, Reason);
						SendChatCommand("/kick " + Victim + " " + User + ": " + Reason + ".");
					}
					else
						SendChatCommand("/kick " + Victim + " You were kicked by " + User + ".");
				}

				//
				// !MAXGAMES
				//

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

				else if (Command == "say" && !Payload.empty()) {
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

				else if (Command == "voice" && !Payload.empty()) {
					SendChatCommand("/voice " + Payload);
				}

				//
				// !VOP (permanent voice)
				//

				else if (Command == "vop" && !Payload.empty()) {
					if (IsRootAdmin(User))
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
			}

			//
			// !CHECKALL
			//

			if (Command == "_checkall") {
				if (Payload.empty())
					Payload = User;

				CONSOLE_Print("m_BanGroup.size( ) = " + UTIL_ToString(m_GHost->m_BanGroup.size()));
				CONSOLE_Print("m_BanGroup.capacity( ) = " + UTIL_ToString(m_GHost->m_BanGroup.capacity()));
				CONSOLE_Print("m_DotAPlayerSummary.size( ) = " + UTIL_ToString(m_GHost->m_DotAPlayerSummary.size()));
				CONSOLE_Print("m_DotAPlayerSummary.capacity( ) = " + UTIL_ToString(m_GHost->m_DotAPlayerSummary.capacity()));
				CONSOLE_Print("m_Players.size( ) = " + UTIL_ToString(m_GHost->m_Players.size()));
			}

			//
			// !SLAVES
			//

			if (Command == "_slaves") {
				m_GHost->m_Manager->PrintSlaveInfo();
			}

			//
			// !CHECKBAN
			//

			else if (Command == "checkban") {
				string Name = Payload.empty() ? User : Payload;
				string NameLower = Name;
				transform(NameLower.begin(), NameLower.end(), NameLower.begin(), (int (*)(int))tolower);
				CDBPlayer* CheckbanPlayer = NULL;

				if (Payload.empty())
					CheckbanPlayer = Player;
				else {
					for (list<CPlayers*>::iterator i = m_GHost->m_Players.begin(); i != m_GHost->m_Players.end(); ++i) {
						if ((*i)->GetServerID() == m_ServerID) {
							CheckbanPlayer = (*i)->GetPlayer(NameLower);
							break;
						}
					}
				}

				if (CheckbanPlayer) {
					CDBBanGroup* BanGroup = CheckbanPlayer->GetBanGroup();

					if (BanGroup && BanGroup->GetIsBanned()) {
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

						if (BanGroup->GetDuration()) {
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

							if (Months) {
								CheckbanStr += " " + UTIL_ToString(Months) + " months";
								if (Days)
									CheckbanStr += " " + UTIL_ToString(Days) + " days";
							}
							else {
								if (Days) {
									CheckbanStr += " " + UTIL_ToString(Days) + " days";
									if (Hours)
										CheckbanStr += " " + UTIL_ToString(Hours) + " hours";
								}
								else {
									if (Hours) {
										CheckbanStr += " " + UTIL_ToString(Hours) + " hours";
										if (Minutes)
											CheckbanStr += " " + UTIL_ToString(Minutes) + " minutes";
									}
									else {
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
						SendChatCommand("https://dota.eurobattle.net/stats/player/" + UTIL_ToString(m_ServerID) + "/" + NameLower, User);
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

			//
			// !GAMES
			//

			else if (Command == "games")
				SendChatCommand(m_GHost->m_Manager->GetGamesDescription(m_ServerID), User);

			//
			// !GNAMES
			//

			//
			// !GOPRIV
			// !PRIV
			//

			//
			// !GOPRIVOBS
			// !PRIVOBS
			//

			//
			// !GOPUB
			// !PUB
			//

			//
			// !INFO
			//

			if (Command == "info") {
				string CheckPlayerName = User;
				CDBPlayer* CheckPlayer = Player;

				if (!Payload.empty()) {
					CheckPlayerName = Payload;
					CheckPlayer = NULL;

					for (list<CPlayers*>::iterator i = m_GHost->m_Players.begin(); i != m_GHost->m_Players.end(); ++i) {
						if ((*i)->GetServerID() == m_ServerID) {
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

			//
			// !LOAD
			//

			//
			// !LOBBY
			//

			//
			// !NAMES
			//

			//
			// !OUT
			// !REMOVE
			//

			else if (Command == "out" || Command == "remove") {
				bool found = false;
				string playerName;
				if (!IsAdmin || Payload.empty())
					playerName = User;
				else
					playerName = Payload;
				transform(playerName.begin(), playerName.end(), playerName.begin(), (int (*)(int))tolower);

				for (vector<string>::iterator i = m_TourPlayers.begin(); i != m_TourPlayers.end(); ++i) {
					if (playerName == *i) {
						found = true;
						m_TourPlayers.erase(i);
						break;
					}
				}
				for (vector<string>::iterator i = m_TourPlayersWaiting.begin(); i != m_TourPlayersWaiting.end(); ++i) {
					if (playerName == *i) {
						found = true;
						m_TourPlayersWaiting.erase(i);
						break;
					}
				}
				if (found)
					SendChatCommand("Player " + playerName + " successfully removed from the tournament.", User);
				else
					SendChatCommand("Player " + playerName + " is not in the tournament.", User);
			}

			//
			// !OUTALL
			//

			else if (Command == "outall") {
				if (IsAdmin) {
					m_TourPlayers.clear();
					m_TourPlayersWaiting.clear();
					SendChatCommand("Cleared the players signed for the tournament.", User);
				}
			}

			//
			// !POOL
			//

			else if (Command == "pool") {
				if (m_TourPlayers.empty())
					SendChatCommand("There are no players signed for the tournament.", User);
				else {
					SendChatCommand("There are " + UTIL_ToString(m_TourPlayers.size()) + " players signed for the tournament.", User);
					string message = m_TourPlayers.front();
					for (vector<string>::iterator i = m_TourPlayers.begin() + 1; i != m_TourPlayers.end(); ++i) {
						if (message.size() > 120) {
							SendChatCommand(message, User);
							message = *i;
						}
						else
							message += ", " + *i;
					}
					SendChatCommand(message, User);
				}
			}

			//
			// !QUEUE
			//

			//
			// !READY
			//

			else if (Command == "ready") {
				bool found = false;
				string playerName = User;
				transform(playerName.begin(), playerName.end(), playerName.begin(), (int (*)(int))tolower);
				for (vector<string>::iterator i = m_TourPlayers.begin(); i != m_TourPlayers.end(); ++i) {
					if (playerName == *i) {
						found = true;
						break;
					}
				}
				if (found) {
					SendChatCommand("You are already signed for the tournament.", User);
				}
				else {
					found = false;
					vector<string>::iterator i;
					for (i = m_TourPlayersWaiting.begin(); i != m_TourPlayersWaiting.end(); ++i) {
						if (playerName == *i) {
							found = true;
							break;
						}
					}
					if (!found) {
						SendChatCommand("You need to be approved by a moderator in order to join the tournament.", User);
					}
					else {
						m_TourPlayersWaiting.erase(i);
						m_TourPlayers.push_back(playerName);
						SendChatCommand("You have successfully signed for the tournament.", User);
					}
				}
			}

			//
			// !STATSDOTA
			// !SD
			//

			else if (Command == "statsdota" || Command == "sd") {
				CDBDotAPlayerSummary* DotAPlayerSummary = NULL;
				string TargetUser;

				if (Payload.empty()) {
					if (Player)
						DotAPlayerSummary = Player->GetDotAPlayerSummary();

					TargetUser = User;
				}
				else {
					CDBPlayer* TargetPlayer = NULL;

					for (list<CPlayers*>::iterator i = m_GHost->m_Players.begin(); i != m_GHost->m_Players.end(); ++i) {
						if ((*i)->GetServerID() == m_ServerID) {
							CDBPlayer* TargetPlayer = (*i)->GetPlayer(Payload);

							if (TargetPlayer)
								DotAPlayerSummary = TargetPlayer->GetDotAPlayerSummary();

							break;
						}
					}

					TargetUser = Payload;
				}

				if (DotAPlayerSummary) {
					SendChatCommand("[" + TargetUser + "] PSR: " + UTIL_ToString(DotAPlayerSummary->GetRating(), 0) + " games: " + UTIL_ToString(DotAPlayerSummary->GetTotalGames()) + " W/L: " + UTIL_ToString(DotAPlayerSummary->GetTotalWins()) + "/" + UTIL_ToString(DotAPlayerSummary->GetTotalLosses()), User);
					SendChatCommand("H. K/D/A " + UTIL_ToString(DotAPlayerSummary->GetAvgKills(), 2) + "/" + UTIL_ToString(DotAPlayerSummary->GetAvgDeaths(), 2) + "/" + UTIL_ToString(DotAPlayerSummary->GetAvgAssists(), 2) + " C. K/D/N " + UTIL_ToString(DotAPlayerSummary->GetAvgCreepKills(), 2) + "/" + UTIL_ToString(DotAPlayerSummary->GetAvgCreepDenies(), 2) + "/" + UTIL_ToString(DotAPlayerSummary->GetAvgNeutralKills(), 2), User);
				}
				else
					SendChatCommand(m_GHost->m_Language->HasntPlayedDotAGamesWithThisBot(TargetUser), User);
			}

			//
			// !UNQUEUE
			//

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
	else if (Event == CBNETProtocol ::EID_CHANNEL) {
		// keep track of current channel so we can rejoin it after hosting a game

		CONSOLE_Print("[BNET: " + m_ServerAlias + "] joined channel [" + Message + "]");
		m_CurrentChannel = Message;

		// clean the old player list from the previous channel when the bot joins a new channel or
		// when the bot rejoins the server and same channel (after DC, server restart, ...)

		m_PlayersInChannel.clear();
	}
	else if (Event == CBNETProtocol ::EID_INFO) {
		CONSOLE_Print("[INFO: " + m_ServerAlias + "] " + Message);

		// extract the first word which we hope is the username
		// this is not necessarily true though since info messages also include channel MOTD's and such

		string UserName;
		string ::size_type Split = Message.find(" ");

		if (Split != string ::npos)
			UserName = Message.substr(0, Split);
		else
			UserName = Message.substr(0);
	}
	else if (Event == CBNETProtocol ::EID_ERROR)
		CONSOLE_Print("[ERROR: " + m_ServerAlias + "] " + Message);
	else if (Event == CBNETProtocol ::EID_EMOTE) {
		CONSOLE_Print("[EMOTE: " + m_ServerAlias + "] [" + User + "] " + Message);
		m_GHost->EventBNETEmote(this, User, Message);
	}
	else if (Event == CBNETProtocol ::EID_SHOWUSER) {
		// we joined a new channel and this player was already in it

		string Name = User;
		transform(Name.begin(), Name.end(), Name.begin(), (int (*)(int))tolower);

		// add player to the channel players list

		m_PlayersInChannel.push_back(Name);
	}
	else if (Event == CBNETProtocol ::EID_JOIN) {
		// player joined our channel

		string Name = User;
		transform(Name.begin(), Name.end(), Name.begin(), (int (*)(int))tolower);

		// add player to the channel players list

		m_PlayersInChannel.push_back(Name);
	}
	else if (Event == CBNETProtocol ::EID_LEAVE) {
		// player left our channel

		string Name = User;
		transform(Name.begin(), Name.end(), Name.begin(), (int (*)(int))tolower);

		// unqueue the game when player leaves the channel

		/*if( m_GHost->m_Manager->HasQueuedGame( this, Name ) )
		{
			SendChatCommand( "Your game was unqueued since you left the channel.", Name );
			m_GHost->m_Manager->UnqueueGame( this, Name );
		}*/

		// remove player from the channel players list

		for (list<string>::iterator i = m_PlayersInChannel.begin(); i != m_PlayersInChannel.end();) {
			if ((*i) == Name)
				i = m_PlayersInChannel.erase(i);
			else
				++i;
		}
	}
}
