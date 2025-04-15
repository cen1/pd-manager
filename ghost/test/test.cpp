#include "gtest/gtest.h"
#include "util.h"
#include <vector>

#include "ghost.h"
#include "config.h"
#include "game_base.h"
#include "game_div1dota.h"
#include "gameprotocol.h"
#include "ghostdb.h"
#include "gameplayer.h"
#include "masl_protocol_2.h"
#include "map.h"
#include "PsrTestSample.h"

using namespace std;

PsrTestSample psrTestSamples;

BYTEARRAY getBaseAction() {
	BYTEARRAY b;

	b.push_back('0');
	b.push_back('1');
	b.push_back('2');
	b.push_back('3');
	b.push_back('4');
	b.push_back('5');

	b.push_back(0x6b);
	b.push_back(0x64);
	b.push_back(0x72);
	b.push_back(0x2e);
	b.push_back(0x78);
	b.push_back(0x00);

	return b;
}

void setData(BYTEARRAY * b, string s) {
	std::vector<unsigned char> vd1(s.c_str(), s.c_str() + s.length() + 1);
	b->insert(b->end(), vd1.begin(), vd1.end());
}

void pushInt(BYTEARRAY * b, uint32_t n) {
	b->push_back(n & 0xFF);
	b->push_back((n >> 8) & 0xFF);
	b->push_back((n >> 16) & 0xFF);
	b->push_back((n >> 24) & 0xFF);
}

TEST(TestCases, DISABLED_FullTest) {

	//PID 0-255, player gets a new PID assigned, first one that is not already taken.
	//SID 0-255, slot ID, max is number of slots that map allows
	//COLOUR 1-5, 7-11

	CGHost *g = new CGHost(true);
	CMap *m = new CMap(NULL);

	CDiv1DotAGame * div1Game = new CDiv1DotAGame(g, m, NULL, 6200, '0', "testgame", "owner", "owner", "bnet");
	div1Game->SetGameLoaded(true);

	//PID ... TEAM, COLOUR (1-5, 7-11)
	CGameSlot s0(0, 0, 0, 0, 1, 1, 0);
	CGameSlot s1(1, 0, 0, 0, 1, 2, 0);
	CGameSlot s5(2, 0, 0, 0, 2, 7, 0);
	div1Game->GetSlots()->push_back(s0);
	div1Game->GetSlots()->push_back(s1);
	div1Game->GetSlots()->push_back(s5);

	BYTEARRAY mock;
	//PID
	CGamePlayer * gp0 = new CGamePlayer(NULL, NULL, NULL, 0, "bnet", "player1", mock, false);
	CGamePlayer * gp1 = new CGamePlayer(NULL, NULL, NULL, 1, "bnet", "player2", mock, false); //sends invalid data
	CGamePlayer * gp5 = new CGamePlayer(NULL, NULL, NULL, 2, "bnet", "player6", mock, false);

	CDIV1DotAPlayer * dp0 = new CDIV1DotAPlayer(div1Game, gp0->GetPID(), gp0->GetName(), 1, 1500, false);
	CDIV1DotAPlayer * dp1 = new CDIV1DotAPlayer(div1Game, gp1->GetPID(), gp1->GetName(), 1, 1500, false);
	CDIV1DotAPlayer * dp5 = new CDIV1DotAPlayer(div1Game, gp5->GetPID(), gp5->GetName(), 1, 1500, false);

	div1Game->GetDotaPlayers()->push_back(dp0);
	div1Game->GetDotaPlayers()->push_back(dp1);
	div1Game->GetDotaPlayers()->push_back(dp5);

	BYTEARRAY crc;

	{
		//Kill action, hero of player 1 dies by the hand of 6, NOT RECORDED!
		BYTEARRAY k = getBaseAction();
		setData(&k, "Data");
		setData(&k, "Hero1");
		pushInt(&k, 6);

		CIncomingAction * ia1 = new CIncomingAction(0, crc, k);
		CIncomingAction * ia2 = new CIncomingAction(2, crc, k);

		BYTEARRAY h = getBaseAction();
		setData(&h, "Data");
		setData(&h, "Hero2");
		pushInt(&h, 2);
		CIncomingAction * ia3 = new CIncomingAction(1, crc, h);

		div1Game->EventPlayerAction(gp0, ia1);
		div1Game->EventPlayerAction(gp5, ia2);
		div1Game->EventPlayerAction(gp1, ia3);
	}

	{
		//Stats action, 1 kill for sid 5
		BYTEARRAY k = getBaseAction();
		setData(&k, "7"); //stats for this COLOUR, adjusted... 6=7
		setData(&k, "1"); //action key, 1=kills
		pushInt(&k, 1); //number of kills

		CIncomingAction * ia1 = new CIncomingAction(0, crc, k);
		CIncomingAction * ia2 = new CIncomingAction(2, crc, k);

		BYTEARRAY h = getBaseAction();
		setData(&h, "7");
		setData(&h, "1");
		pushInt(&h, 1000);

		CIncomingAction * ia3 = new CIncomingAction(1, crc, h);

		BYTEARRAY h2 = getBaseAction();
		setData(&h2, "2");
		setData(&h2, "1");
		pushInt(&h2, 10000);

		CIncomingAction * ia4 = new CIncomingAction(1, crc, h2);

		div1Game->EventPlayerAction(gp0, ia1);
		div1Game->EventPlayerAction(gp5, ia2);
		div1Game->EventPlayerAction(gp1, ia3);
		div1Game->EventPlayerAction(gp1, ia4);
	}

	{
		//Stats action, 1 death for sid 1
		BYTEARRAY k = getBaseAction();
		setData(&k, "1"); //stats for this COLOUR
		setData(&k, "2"); //action key
		pushInt(&k, 1); //value

		CIncomingAction * ia1 = new CIncomingAction(0, crc, k);
		CIncomingAction * ia2 = new CIncomingAction(2, crc, k);

		div1Game->EventPlayerAction(gp0, ia1);
		div1Game->EventPlayerAction(gp5, ia2);
	}

	{
		//Stats action, 2 assists for SID 1
		BYTEARRAY k = getBaseAction();
		setData(&k, "1"); //stats for this COLOUR
		setData(&k, "5"); //action key
		pushInt(&k, 2); //value

		CIncomingAction * ia1 = new CIncomingAction(0, crc, k);
		CIncomingAction * ia2 = new CIncomingAction(2, crc, k);

		div1Game->EventPlayerAction(gp0, ia1);
		div1Game->EventPlayerAction(gp5, ia2);
	}

	//Winner action
	BYTEARRAY a1 = getBaseAction();
	setData(&a1, "Global");
	setData(&a1, "Winner");
	pushInt(&a1, 1);

	//send PID
	CIncomingAction * wa1 = new CIncomingAction(0, crc, a1);
	CIncomingAction * wa2 = new CIncomingAction(2, crc, a1);

	bool w0 = div1Game->EventPlayerAction(gp0, wa1); //sentinel
	bool w2 = div1Game->EventPlayerAction(gp5, wa2); //sentinel

	ASSERT_FALSE(w0);
	ASSERT_TRUE(w2);

	for (auto it = div1Game->GetDotaPlayers()->begin(); it != div1Game->GetDotaPlayers()->end(); it++) {
		if ((*it)->GetCurrentColor() == 1) {
			ASSERT_EQ((*it)->GetKills(), 0);
			ASSERT_EQ((*it)->GetDeaths(), 1);
			ASSERT_EQ((*it)->GetAssists(), 2);
		}
		else if ((*it)->GetCurrentColor() == 2) {
			//Player sending invalid data, has no stats
			ASSERT_EQ((*it)->GetKills(), 0);
			ASSERT_EQ((*it)->GetDeaths(), 0);
			ASSERT_EQ((*it)->GetAssists(), 0);
		}
		else if ((*it)->GetCurrentColor() == 7) {
			ASSERT_EQ((*it)->GetKills(), 1);
			ASSERT_EQ((*it)->GetDeaths(), 0);
			ASSERT_EQ((*it)->GetAssists(), 0);
		}
	}
}

TEST(TestCases, DISABLED_1v1Test) {

	CGHost *g = new CGHost(true);
	CMap *m = new CMap(NULL);

	CDiv1DotAGame * div1Game = new CDiv1DotAGame(g, m, NULL, 6200, '0', "testgame", "owner", "owner", "bnet");
	div1Game->SetGameLoaded(true);

	//PID ... TEAM, COLOUR (1-5, 7-11)
	CGameSlot s0(0, 0, 0, 0, 1, 1, 0);
	CGameSlot s5(2, 0, 0, 0, 2, 7, 0);
	div1Game->GetSlots()->push_back(s0);
	div1Game->GetSlots()->push_back(s5);

	BYTEARRAY mock;
	//PID
	CGamePlayer * gp0 = new CGamePlayer(NULL, NULL, NULL, 0, "bnet", "player1", mock, false);
	CGamePlayer * gp5 = new CGamePlayer(NULL, NULL, NULL, 2, "bnet", "player6", mock, false);

	CDIV1DotAPlayer * dp0 = new CDIV1DotAPlayer(div1Game, gp0->GetPID(), gp0->GetName(), 1, 1500, false);
	CDIV1DotAPlayer * dp5 = new CDIV1DotAPlayer(div1Game, gp5->GetPID(), gp5->GetName(), 1, 1500, false);

	div1Game->GetDotaPlayers()->push_back(dp0);
	div1Game->GetDotaPlayers()->push_back(dp5);

	BYTEARRAY crc;

	{
		//Stats action, 1 kill for sid 5
		BYTEARRAY k = getBaseAction();
		setData(&k, "7"); //stats for this COLOUR, adjusted... 6=7
		setData(&k, "1"); //action key, 1=kills
		pushInt(&k, 1); //number of kills

		CIncomingAction * ia1 = new CIncomingAction(0, crc, k);
		CIncomingAction * ia2 = new CIncomingAction(2, crc, k);


		div1Game->EventPlayerAction(gp0, ia1);
		div1Game->EventPlayerAction(gp5, ia2);
	}

	//Winner action
	BYTEARRAY a1 = getBaseAction();
	setData(&a1, "Global");
	setData(&a1, "Winner");
	pushInt(&a1, 1);

	//send PID
	CIncomingAction * wa1 = new CIncomingAction(0, crc, a1);
	CIncomingAction * wa2 = new CIncomingAction(2, crc, a1);

	bool w0 = div1Game->EventPlayerAction(gp0, wa1);
	bool w2 = div1Game->EventPlayerAction(gp5, wa2);

	ASSERT_FALSE(w0);
	ASSERT_TRUE(w2);

	for (auto it = div1Game->GetDotaPlayers()->begin(); it != div1Game->GetDotaPlayers()->end(); it++) {
		if ((*it)->GetCurrentColor() == 1) {
			ASSERT_EQ((*it)->GetKills(), 0);
			ASSERT_EQ((*it)->GetDeaths(), 0);
			ASSERT_EQ((*it)->GetAssists(), 0);
		}
		else if ((*it)->GetCurrentColor() == 7) {
			ASSERT_EQ((*it)->GetKills(), 1);
			ASSERT_EQ((*it)->GetDeaths(), 0);
			ASSERT_EQ((*it)->GetAssists(), 0);
		}
	}
}

TEST(TestCases, DISABLED_SplitDecisionWinnerTest) {

	CGHost *g = new CGHost(true);
	CMap *m = new CMap(NULL);

	CDiv1DotAGame * div1Game = new CDiv1DotAGame(g, m, NULL, 6200, '0', "splitdecision", "owner", "owner", "bnet");
	div1Game->SetGameLoaded(true);

	//PID ... TEAM, COLOUR (1-5, 7-11)
	CGameSlot s0(0, 0, 0, 0, 1, 1, 0);
	CGameSlot s1(1, 0, 0, 0, 1, 2, 0);
	CGameSlot s5(2, 0, 0, 0, 2, 7, 0);
	div1Game->GetSlots()->push_back(s0);
	div1Game->GetSlots()->push_back(s1);
	div1Game->GetSlots()->push_back(s5);

	BYTEARRAY mock;
	//PID
	CGamePlayer * gp0 = new CGamePlayer(NULL, NULL, NULL, 0, "bnet", "player1", mock, false);
	CGamePlayer * gp1 = new CGamePlayer(NULL, NULL, NULL, 1, "bnet", "player2", mock, false); //sends invalid data
	CGamePlayer * gp5 = new CGamePlayer(NULL, NULL, NULL, 2, "bnet", "player6", mock, false);

	CDIV1DotAPlayer * dp0 = new CDIV1DotAPlayer(div1Game, gp0->GetPID(), gp0->GetName(), 1, 1500, false);
	CDIV1DotAPlayer * dp1 = new CDIV1DotAPlayer(div1Game, gp1->GetPID(), gp1->GetName(), 1, 1500, false);
	CDIV1DotAPlayer * dp5 = new CDIV1DotAPlayer(div1Game, gp5->GetPID(), gp5->GetName(), 1, 1500, false);

	div1Game->GetDotaPlayers()->push_back(dp0);
	div1Game->GetDotaPlayers()->push_back(dp1);
	div1Game->GetDotaPlayers()->push_back(dp5);

	BYTEARRAY crc;

	//Winner action
	BYTEARRAY a1 = getBaseAction();
	setData(&a1, "Global");
	setData(&a1, "Winner");
	pushInt(&a1, 1);

	//send PID
	CIncomingAction * wa1 = new CIncomingAction(0, crc, a1);

	BYTEARRAY a2 = getBaseAction();
	setData(&a2, "Global");
	setData(&a2, "Winner");
	pushInt(&a2, 2);

	CIncomingAction * wa3 = new CIncomingAction(1, crc, a2);

	bool w0 = div1Game->EventPlayerAction(gp0, wa1); //sentinel
	bool w1 = div1Game->EventPlayerAction(gp1, wa3); //scourge

	ASSERT_FALSE(w0);
	ASSERT_TRUE(w1); //split decision
}

void simulateKill(CDIV1DotAPlayer * killer, int newKills,
	CDIV1DotAPlayer * victim, int newDeaths,
	vector<CGamePlayer *> allPlayers,
	CDiv1DotAGame * div1Game) {

	BYTEARRAY crc;

	//Kill
	{
		BYTEARRAY k = getBaseAction();
		setData(&k, "Data");
		setData(&k, "Hero" + UTIL_ToString(victim->GetCurrentColor()));
		pushInt(&k, killer->GetCurrentColor());

		for (auto it = allPlayers.begin(); it != allPlayers.end(); ++it) {
			CIncomingAction ia((*it)->GetPID(), crc, k);
			div1Game->EventPlayerAction(*it, &ia);
		}
	}

	//Stats - kill
	{
		BYTEARRAY k = getBaseAction();
		setData(&k, UTIL_ToString(killer->GetCurrentColor())); //stats for this COLOUR, adjusted... 6=7
		setData(&k, "1"); //action key, 1=kills
		pushInt(&k, newKills); //number of kills

		for (auto it = allPlayers.begin(); it != allPlayers.end(); ++it) {
			CIncomingAction ia((*it)->GetPID(), crc, k);
			div1Game->EventPlayerAction(*it, &ia);
		}
	}

	//Stats - death
	{
		//Stats action, 1 death for player 1
		BYTEARRAY k = getBaseAction();
		setData(&k, UTIL_ToString(victim->GetCurrentColor())); //stats for this COLOUR
		setData(&k, "2"); //action key
		pushInt(&k, newDeaths); //value

		for (auto it = allPlayers.begin(); it != allPlayers.end(); ++it) {
			CIncomingAction ia((*it)->GetPID(), crc, k);
			div1Game->EventPlayerAction(*it, &ia);
		}
	}
}

TEST(TestCases, FfTest) {

	//PID 0-255, player gets a new PID assigned, first one that is not already taken.
	//SID 0-255, slot ID, max is number of slots that map allows
	//COLOUR 1-5, 7-11

	CGHost *g = new CGHost(true);
	g->m_DidYouKnowEnabled = false;
	CMap *m = new CMap(NULL);

	CDiv1DotAGame * div1Game = new CDiv1DotAGame(g, m, NULL, 6200, '0', "testgame", "owner", "owner", "bnet");
	div1Game->SetGameLoaded(true);

	//          PID          T  C
	CGameSlot s0(0, 0, 0, 0, 1, 1, 0);
	CGameSlot s1(1, 0, 0, 0, 1, 2, 0);
	CGameSlot s2(2, 0, 0, 0, 1, 3, 0);
	CGameSlot s3(3, 0, 0, 0, 1, 4, 0);
	CGameSlot s4(4, 0, 0, 0, 1, 5, 0);
	CGameSlot s5(5, 0, 0, 0, 2, 7, 0);
	CGameSlot s6(6, 0, 0, 0, 2, 8, 0);
	CGameSlot s7(7, 0, 0, 0, 2, 9, 0);
	CGameSlot s8(8, 0, 0, 0, 2, 10, 0);
	CGameSlot s9(9, 0, 0, 0, 2, 11, 0);

	div1Game->GetSlots()->push_back(s0);
	div1Game->GetSlots()->push_back(s1);
	div1Game->GetSlots()->push_back(s2);
	div1Game->GetSlots()->push_back(s3);
	div1Game->GetSlots()->push_back(s4);
	div1Game->GetSlots()->push_back(s5);
	div1Game->GetSlots()->push_back(s6);
	div1Game->GetSlots()->push_back(s7);
	div1Game->GetSlots()->push_back(s8);
	div1Game->GetSlots()->push_back(s9);

	BYTEARRAY mock;

	CGamePlayer * gp0 = new CGamePlayer(NULL, NULL, NULL, 0, "bnet", "player1", mock, false);
	CGamePlayer * gp1 = new CGamePlayer(NULL, NULL, NULL, 1, "bnet", "player2", mock, false);
	CGamePlayer * gp2 = new CGamePlayer(NULL, NULL, NULL, 2, "bnet", "player3", mock, false);
	CGamePlayer * gp3 = new CGamePlayer(NULL, NULL, NULL, 3, "bnet", "player4", mock, false);
	CGamePlayer * gp4 = new CGamePlayer(NULL, NULL, NULL, 4, "bnet", "player5", mock, false);
	CGamePlayer * gp5 = new CGamePlayer(NULL, NULL, NULL, 5, "bnet", "player6", mock, false);
	CGamePlayer * gp6 = new CGamePlayer(NULL, NULL, NULL, 6, "bnet", "player7", mock, false);
	CGamePlayer * gp7 = new CGamePlayer(NULL, NULL, NULL, 7, "bnet", "player8", mock, false);
	CGamePlayer * gp8 = new CGamePlayer(NULL, NULL, NULL, 8, "bnet", "player9", mock, false);
	CGamePlayer * gp9 = new CGamePlayer(NULL, NULL, NULL, 9, "bnet", "player10", mock, false);

	vector<CGamePlayer *> gameplayers;
	gameplayers.push_back(gp0);
	gameplayers.push_back(gp1);
	gameplayers.push_back(gp2);
	gameplayers.push_back(gp3);
	gameplayers.push_back(gp4);
	gameplayers.push_back(gp5);
	gameplayers.push_back(gp6);
	gameplayers.push_back(gp7);
	gameplayers.push_back(gp8);
	gameplayers.push_back(gp9);

	CDIV1DotAPlayer * dp0 = new CDIV1DotAPlayer(div1Game, gp0->GetPID(), gp0->GetName(), 1, 1500, false);
	CDIV1DotAPlayer * dp1 = new CDIV1DotAPlayer(div1Game, gp1->GetPID(), gp1->GetName(), 1, 1500, false);
	CDIV1DotAPlayer * dp2 = new CDIV1DotAPlayer(div1Game, gp2->GetPID(), gp2->GetName(), 1, 1500, false);
	CDIV1DotAPlayer * dp3 = new CDIV1DotAPlayer(div1Game, gp3->GetPID(), gp3->GetName(), 1, 1500, false);
	CDIV1DotAPlayer * dp4 = new CDIV1DotAPlayer(div1Game, gp4->GetPID(), gp4->GetName(), 1, 1500, false);
	CDIV1DotAPlayer * dp5 = new CDIV1DotAPlayer(div1Game, gp5->GetPID(), gp5->GetName(), 1, 1500, false);
	CDIV1DotAPlayer * dp6 = new CDIV1DotAPlayer(div1Game, gp6->GetPID(), gp6->GetName(), 1, 1500, false);
	CDIV1DotAPlayer * dp7 = new CDIV1DotAPlayer(div1Game, gp7->GetPID(), gp7->GetName(), 1, 1500, false);
	CDIV1DotAPlayer * dp8 = new CDIV1DotAPlayer(div1Game, gp8->GetPID(), gp8->GetName(), 1, 1500, false);
	CDIV1DotAPlayer * dp9 = new CDIV1DotAPlayer(div1Game, gp9->GetPID(), gp9->GetName(), 1, 1500, false);

	dp0->SetLobbyColor(1);
	dp1->SetLobbyColor(2);
	dp2->SetLobbyColor(3);
	dp3->SetLobbyColor(4);
	dp4->SetLobbyColor(5);
	dp5->SetLobbyColor(7);
	dp6->SetLobbyColor(8);
	dp7->SetLobbyColor(9);
	dp8->SetLobbyColor(10);
	dp9->SetLobbyColor(11);

	div1Game->GetDotaPlayers()->push_back(dp0);
	div1Game->GetDotaPlayers()->push_back(dp1);
	div1Game->GetDotaPlayers()->push_back(dp2);
	div1Game->GetDotaPlayers()->push_back(dp3);
	div1Game->GetDotaPlayers()->push_back(dp4);
	div1Game->GetDotaPlayers()->push_back(dp5);
	div1Game->GetDotaPlayers()->push_back(dp6);
	div1Game->GetDotaPlayers()->push_back(dp7);
	div1Game->GetDotaPlayers()->push_back(dp8);
	div1Game->GetDotaPlayers()->push_back(dp9);

	/*
	SIMULATING THE FOLLOWING SCENARIO
	SID	K D A CK CD NK NewColor
	0:  0 0 0 0  0  0
	1:  1 1 1 1  1  1
	2:  2 2 2 2  2  2
	3:  3 3 3 3  3  3
	4:  4 4 4 4  4  4

	5:  1 2 3 4  5  6
	6:  2 3 4 5  6  7
	7:  3 0 5 6  7  8
	8:  4 5 6 7  8  9
	9:  0 0 9 9  9  9
	*/

	//TODO: simulate assists and other stats

	//P0 0-0

	//P1 1-1
	simulateKill(dp5, 1, dp1, 1, gameplayers, div1Game);

	//P2 2-2
	simulateKill(dp6, 1, dp2, 1, gameplayers, div1Game);
	simulateKill(dp6, 2, dp2, 2, gameplayers, div1Game);

	//P3 3-3
	simulateKill(dp7, 1, dp3, 1, gameplayers, div1Game);
	simulateKill(dp7, 2, dp3, 2, gameplayers, div1Game);
	simulateKill(dp7, 3, dp3, 3, gameplayers, div1Game);

	//P4 4-4
	simulateKill(dp8, 1, dp4, 1, gameplayers, div1Game);
	simulateKill(dp8, 2, dp4, 2, gameplayers, div1Game);
	simulateKill(dp8, 3, dp4, 3, gameplayers, div1Game);
	simulateKill(dp8, 4, dp4, 4, gameplayers, div1Game);

	//P5 1-2
	simulateKill(dp2, 1, dp5, 1, gameplayers, div1Game);
	simulateKill(dp2, 2, dp5, 2, gameplayers, div1Game);

	//P6 2-3
	simulateKill(dp3, 1, dp6, 1, gameplayers, div1Game);
	simulateKill(dp3, 2, dp6, 2, gameplayers, div1Game);
	simulateKill(dp3, 3, dp6, 3, gameplayers, div1Game);

	//P7 3-0

	//P8 4-5
	simulateKill(dp4, 1, dp8, 1, gameplayers, div1Game);
	simulateKill(dp4, 2, dp8, 2, gameplayers, div1Game);
	simulateKill(dp4, 3, dp8, 3, gameplayers, div1Game);
	simulateKill(dp4, 4, dp8, 4, gameplayers, div1Game);
	simulateKill(dp1, 1, dp8, 5, gameplayers, div1Game);

	//P9 0-0

	for (auto it = div1Game->GetDotaPlayers()->begin(); it != div1Game->GetDotaPlayers()->end(); it++) {
		if ((*it)->GetPID() == 0) {
			ASSERT_EQ((*it)->GetKills(), 0);
			ASSERT_EQ((*it)->GetDeaths(), 0);
			ASSERT_EQ((*it)->GetAssists(), 0);
		}
		else if ((*it)->GetPID() == 1) {
			ASSERT_EQ((*it)->GetKills(), 1);
			ASSERT_EQ((*it)->GetDeaths(), 1);
			ASSERT_EQ((*it)->GetAssists(), 0);
		}
		else if ((*it)->GetPID() == 2) {
			ASSERT_EQ((*it)->GetKills(), 2);
			ASSERT_EQ((*it)->GetDeaths(), 2);
			ASSERT_EQ((*it)->GetAssists(), 0);
		}
		else if ((*it)->GetPID() == 3) {
			ASSERT_EQ((*it)->GetKills(), 3);
			ASSERT_EQ((*it)->GetDeaths(), 3);
			ASSERT_EQ((*it)->GetAssists(), 0);
		}
		else if ((*it)->GetPID() == 4) {
			ASSERT_EQ((*it)->GetKills(), 4);
			ASSERT_EQ((*it)->GetDeaths(), 4);
			ASSERT_EQ((*it)->GetAssists(), 0);
		}
		else if ((*it)->GetPID() == 5) {
			ASSERT_EQ((*it)->GetKills(), 1);
			ASSERT_EQ((*it)->GetDeaths(), 2);
			ASSERT_EQ((*it)->GetAssists(), 0 );
		}
		else if ((*it)->GetPID() == 6) {
			ASSERT_EQ((*it)->GetKills(), 2);
			ASSERT_EQ((*it)->GetDeaths(), 3);
			ASSERT_EQ((*it)->GetAssists(), 0);
		}
		else if ((*it)->GetPID() == 7) {
			ASSERT_EQ((*it)->GetKills(), 3);
			ASSERT_EQ((*it)->GetDeaths(), 0);
			ASSERT_EQ((*it)->GetAssists(), 0);
		}
		else if ((*it)->GetPID() == 8) {
			ASSERT_EQ((*it)->GetKills(), 4);
			ASSERT_EQ((*it)->GetDeaths(), 5);
			ASSERT_EQ((*it)->GetAssists(), 0);
		}
		else if ((*it)->GetPID() == 9) {
			ASSERT_EQ((*it)->GetKills(), 0);
			ASSERT_EQ((*it)->GetDeaths(), 0);
			ASSERT_EQ((*it)->GetAssists(), 0);
		}
	}
}

TEST(TestCases, PSRTest) {

	for (int i = 0; i < psrTestSamples.size(); i++) {
		CPSR cpsr;

		cout << "Sample # " << i << endl << endl;

		cout << psrTestSamples.getSampleDescription(i) << endl << endl;

		CPSR cpsr1;

		vector<unsigned char> pids = { 1, 2, 3, 4, 5, 7, 8, 9, 10, 11 };

		vector<PairedPlayerRating> team1;
		vector<PairedPlayerRating> team2;
		vector<CDIV1DotAPlayer *> dotaPlayers;

		for (int j = 0; j < 10; j++) {
			//*nGame, unsigned char nPID, string nName, uint32_t nServerID, double nRating, bool nLocked
			CDIV1DotAPlayer *p = new CDIV1DotAPlayer(NULL, pids[j],
				psrTestSamples.getSamples()[i][j].first, 0, psrTestSamples.getSamples()[i][j].second->GetRating(), false);

			//Hardcode : player 1 is locked (host)
			if (j == 0) {
				p->SetLocked(true);
			}

			if (j <= 4)
				team1.push_back(make_pair(p->GetName(), p->GetRating()));
			else
				team2.push_back(make_pair(p->GetName(), p->GetRating()));

			dotaPlayers.push_back(p);
		}

		uint32_t maxDiff = 0;
		if (i == 1)
			maxDiff = 22;

		cpsr1.CalculatePSR(team1, team2, maxDiff);

		cout << "T1AVP " << cpsr1.GetTeamAvgPSR(0) << endl;
		cout << "T2AVP " << cpsr1.GetTeamAvgPSR(1) << endl;

		cout << "T1WP " << cpsr1.GetTeamWinPerc(0) << endl;
		cout << "T2WP " << cpsr1.GetTeamWinPerc(1) << endl;

		int c = 0;
		for (auto it = dotaPlayers.begin(); it != dotaPlayers.end(); ++it) {

			double gain = cpsr1.GetPlayerGainLoss((*it)->GetName()).first;
			double loss = cpsr1.GetPlayerGainLoss((*it)->GetName()).second;

			cout << (*it)->GetName() << ": " << UTIL_ToString((*it)->GetRating(), 0) + ", +"
				+ UTIL_ToString(gain, 0) + "/-"
				+ UTIL_ToString(loss, 0)
				<< std::endl;

			if (i == 0) {
				if (c == 0) {
					ASSERT_EQ(gain, 3.0);
					ASSERT_EQ(loss, 2.0);
				}
				else if (c == 1) {
					ASSERT_EQ(gain, 7.0);
					ASSERT_EQ(loss, 6.0);
				}
				else if (c == 2) {
					ASSERT_EQ(gain, 9.0);
					ASSERT_EQ(loss, 9.0);
				}
				else if (c == 3) {
					ASSERT_EQ(gain, 14.0);
					ASSERT_EQ(loss, 13.0);
				}
				else if (c == 4) {
					ASSERT_EQ(gain, 7.0);
					ASSERT_EQ(loss, 6.0);
				}
				else if (c == 5) {
					ASSERT_EQ(gain, 5.0);
					ASSERT_EQ(loss, 4.0);
				}
				else if (c == 6) {
					ASSERT_EQ(gain, 15.0);
					ASSERT_EQ(loss, 13.0);
				}
				else if (c == 7) {
					ASSERT_EQ(gain, 5.0);
					ASSERT_EQ(loss, 3.0);
				}
				else if (c == 8) {
					ASSERT_EQ(gain, 7.0);
					ASSERT_EQ(loss, 6.0);
				}
				else if (c == 9) {
					ASSERT_EQ(gain, 11.0);
					ASSERT_EQ(loss, 10.0);
				}
			}

			c++;
		}
	}
}

void createPSRSamples() {

	{
		//Sample 0
		pair<string, CDBDiv1DPS*> p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, averagePlayer, newPlayer, newStrongPlayer,
			badPlayer, badHighPSRPlayer, goodHighPSRPlayer, badLowPSRPlayer, goodLowPSRPlayer, veryStrongPlayer;
		vector<pair<string, CDBDiv1DPS*>> players;

		string description = "Game: https://dota.eurobattle.net/la/forum/index.php?action=gameinfo;sa=game;gid=6355821";

		// rating, highest_rating, games, wins, loses, k, d, ck, cd, a, nk

		//Sentinel team
		p1 = make_pair("magdos", new CDBDiv1DPS(1736, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
		p2 = make_pair("we[n]dy", new CDBDiv1DPS(1652, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
		p3 = make_pair("juhero", new CDBDiv1DPS(1614, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
		p4 = make_pair("dejanmatic", new CDBDiv1DPS(1546, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
		p5 = make_pair("the_miracle", new CDBDiv1DPS(1653, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));

		//Scourge team
		p6 = make_pair("cresovotopce", new CDBDiv1DPS(1694, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
		p7 = make_pair("exser.", new CDBDiv1DPS(1539, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
		p8 = make_pair("cant_be_stopped", new CDBDiv1DPS(1708, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
		p9 = make_pair("[nfs]highlander", new CDBDiv1DPS(1654, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
		p10 = make_pair("ggplayer..", new CDBDiv1DPS(1589, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));

		players = { p1, p2, p3, p4, p5, p6, p7, p8, p9, p10 };

		psrTestSamples.addSample(players, description);
	}

	{
		//Sample 1
		pair<string, CDBDiv1DPS*> p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, averagePlayer, newPlayer, newStrongPlayer,
			badPlayer, badHighPSRPlayer, goodHighPSRPlayer, badLowPSRPlayer, goodLowPSRPlayer, veryStrongPlayer;
		vector<pair<string, CDBDiv1DPS*>> players;

		string description = "Game: https://dota.eurobattle.net/la/forum/index.php?action=gameinfo;sa=game;gid=6387923";

		p1 = make_pair("miley_cyrus_", new CDBDiv1DPS(1600, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
		p2 = make_pair("smoke_123", new CDBDiv1DPS(1600, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
		p3 = make_pair("gorgona", new CDBDiv1DPS(1600, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
		p4 = make_pair("z789456", new CDBDiv1DPS(1600, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
		p5 = make_pair("iernesto94", new CDBDiv1DPS(1800, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));

		//Scourge 1620
		p6 = make_pair("bazarci", new CDBDiv1DPS(1600, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
		p7 = make_pair("sero", new CDBDiv1DPS(1600, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
		p8 = make_pair("crackhe", new CDBDiv1DPS(1600, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
		p9 = make_pair("ghosteagle", new CDBDiv1DPS(1600, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
		p10 = make_pair("noob^^stalker", new CDBDiv1DPS(1600, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));

		players = { p1, p2, p3, p4, p5, p6, p7, p8, p9, p10 };

		psrTestSamples.addSample(players, description);
	}
}


int main(int argc, char** argv) {
	testing::InitGoogleTest(&argc, argv);

	//Init
	createPSRSamples();

	//Run
	int ret = RUN_ALL_TESTS();
	//cin.get();

	return ret;
}
