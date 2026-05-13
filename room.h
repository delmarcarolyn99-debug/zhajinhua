#pragma once
#include<vector>
#include<string>
#include<map>
#include"player.h"
#include"card.h"
// 游戏常量定义
const int ANTE = 10;              // 底注
const int INITIAL_BALANCE = 1000; // 初始筹码
const int COMPARE_COST = 20;      // 比牌费用
const int CARDS_PER_PLAYER = 3;   // 每个玩家的手牌数
const int DEFAULT_PLAYERS = 3;    // 默认玩家数


enum class HandType {
	HIGH_CARD = 1,
	//普通牌型，只看手牌最大值，若相同比花色，花色比较再card.h中有定义
	PAIR=2,
	//对子，如果都是对子，值大的赢，或者花色大的赢，若双方花色值相同，则比较单牌的大小
	STRAIGHT=3,
	//顺子，不看花色，只看大小，连续数字则是顺子
	FLUSH=4,
	//同花色不连，比最大牌
	STRAIGHT_FLUSH=5,
	//同花顺
	LEOPARD=6,
	//豹子牌，三张一样
};//牌型大小：6>5>……>2>1
class Room {
public:
	int pot;
	int currentPlayerIndex;
	bool isGameOver;
	void startgame();
	void nextTurn();
	int getAliveCount();
	std::vector<Player> players;
	std::vector<Card> deck;
	std::map<std::string, Player*> playerMap;  // 玩家名字到指针的映射
	Room();
	void addPlayer(Player p);
	Player* findPlayerByName(const std::string& name);  // 根据名字查找玩家
	void initDeck();
	void dealCards();
	int calculateHandScore(std::vector<Card>hand);
	void printPlayerHands();
	void determineWinner();
	bool compareHands(int playerIdx1, int playerIdx2, int& winnerIdx);  // 比牌功能
};