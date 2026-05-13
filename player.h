#pragma once
#include<string>
#include<vector>
#include"card.h"
class Player {
public:
	std::string name;
	//玩家名字
	int balance;
	//玩家筹码
	bool hasFolded;
	//是否弃牌，弃牌后放弃下注，固定失去进入牌局的筹码(入场费)，先定30，若不弃牌，则选择跟注
	bool hasLooked;
	//是否看牌，若不看牌，获胜则获得2倍奖金，未获胜则输掉2倍奖金，
	// 若看牌则可以选择跟注或弃牌
	std::vector<Card> hand;
	//三张手牌，此处为私人牌，不公开
	Player(std::string player_name, int init_balance);
	//构造函数，初始化玩家信息，主要有id与筹码
	void receivecard(Card c);
	//抽牌
	void lookatcards();
	//看牌
	void fold();
	//弃牌
	bool placeBet(int amount);
	//下注，返回是否成功下注
	void printStatus()const;
	//打印玩家信息
};
	