
#ifndef CUSTOM_GAME_H
#define CUSTOM_GAME_H

class CCustomGame;

//
// CSlaveBot
//

class CCustomGame {
  private:
	string m_Command;
	string m_Map;

  public:
	CCustomGame(string nCommand, string nMap);

	string GetCommand() { return m_Command; }
	string GetMap() { return m_Map; }

	void SetCommand() { m_Command = nCommand; }
	void SetMap() { m_Map = nMap; }
};

#endif
