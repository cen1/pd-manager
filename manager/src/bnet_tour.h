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

#ifndef BNET_TOUR_H
#define BNET_TOUR_H

#include "bnet.h"
#include "ghost.h"
#include <string>

using namespace std;

//
// CTourBNET
//

class CTourBNET : public CBNET {
  private:
	vector<string> m_TourPlayers;
	vector<string> m_TourPlayersWaiting;

  public:
	CTourBNET(CGHost* nGHost, string nServer, string nServerAlias, string nBNLSServer, uint16_t nBNLSPort, uint32_t nBNLSWardenCookie, string nCDKeyROC, string nCDKeyTFT, string nCountryAbbrev, string nCountry, string nUserName, string nUserPassword, string nFirstChannel, string nRootAdmin, char nCommandTrigger, bool nPublicCommands, unsigned char nWar3Version, BYTEARRAY nEXEVersion, BYTEARRAY nEXEVersionHash, string nPasswordHashType, uint32_t nMaxMessageLength, uint32_t nServerID)
	  : CBNET(nGHost, nServer, nServerAlias, nBNLSServer, nBNLSPort, nBNLSWardenCookie, nCDKeyROC, nCDKeyTFT, nCountryAbbrev, nCountry, nUserName, nUserPassword, nFirstChannel, nRootAdmin, nCommandTrigger, nPublicCommands, nWar3Version, nEXEVersion, nEXEVersionHash, nPasswordHashType, nMaxMessageLength, nServerID)
	{
	}
	~CTourBNET() {}

	// processing functions

	virtual void ProcessChatEvent(CIncomingChatEvent* chatEvent);

	// functions to send packets to battle.net

	// other functions
};

#endif
