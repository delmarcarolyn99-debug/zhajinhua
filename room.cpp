#include"room.h"
#include<iostream>
#include<algorithm>
#include<random>
#include<chrono>
Room::Room() : pot(0), currentPlayerIndex(0), isGameOver(false) {}
void Room::addPlayer(Player p) {
	players.push_back(p);
	// 更新玩家映射
	playerMap[players.back().name] = &players.back();
}

Player* Room::findPlayerByName(const std::string& name) {
	auto it = playerMap.find(name);
	if (it != playerMap.end()) {
		return it->second;
	}
	return nullptr;
}
void Room::initDeck() {
	deck.clear();
	for (int s = 1; s <= 4; s++) {
		for (int v = 2; v <= 14; v++) {
			deck.push_back(Card(static_cast<Suit>(s), v));
		}
	}
	unsigned seed=std::chrono::system_clock::now().time_since_epoch().count();
	std::default_random_engine engine(seed);
	//使用随机数引擎打乱牌序
	std::shuffle(deck.begin(), deck.end(), engine);
}
void Room::dealCards() {
	for (auto& player : players) {
		player.hand.clear();
		for (int i = 0; i < CARDS_PER_PLAYER; i++) {
			if (!deck.empty()) {
				player.receivecard(deck.back());
				deck.pop_back();
			}
		}
	}
}
int Room::calculateHandScore(std::vector<Card>hand) {
	if (hand.size() != 3) {
		return 0;
	}
	std::sort(hand.begin(), hand.end(), [](const Card& a, const Card& b) {
		return a.value > b.value;
		});
	int v1 = hand[0].value;
	int v2 = hand[1].value;
	int v3 = hand[2].value;
	bool isFlush = (hand[0].suit == hand[1].suit && hand[1].suit == hand[2].suit);
	
	// 判断顺子：普通顺子 或 A-2-3特殊顺子
	bool isStraight = (v1 - 1 == v2 && v2 - 1 == v3);
	// 处理 A-2-3 顺子 (A=14, 2=2, 3=3)
	bool isSpecialStraight = (v1 == 14 && v2 == 3 && v3 == 2);
	if (isSpecialStraight) {
		isStraight = true;
		// 对于A-2-3顺子，重新设置v1为3（作为最大牌），用于后续计分
		v1 = 3;
		v2 = 2;
		v3 = 1;  // A在A-2-3中算作1
	}
	HandType type = HandType::HIGH_CARD;
	if (v1 == v2 && v2 == v3) {
		type = HandType::LEOPARD;
	}
	else if (isFlush && isStraight) {
		type = HandType::STRAIGHT_FLUSH;
	}
	else if (isFlush) {
		type = HandType::FLUSH;
	}
	else if (isStraight) {
		type = HandType::STRAIGHT;
	}
	else if (v1 == v2 || v2 == v3) {
		type = HandType::PAIR;
	}
	int baseScore = static_cast<int>(type) * 1000000;
	int extraScore = 0;
	if (type == HandType::PAIR) {
		int pairVal = (v1 == v2) ? v1 : v2;
		int singleVal = (v1 == v2) ? v3 : v1;
		extraScore = pairVal * 128 + singleVal;
	}
	else {
		extraScore = v1 * 128 + v2 * 16 + v3;
	}
	return baseScore + extraScore;
}
void Room::printPlayerHands() {
	for (const auto& player : players) {
		std::cout << player.name << "的牌：";
		for (const auto& card : player.hand) {
			std::cout << card.toString() << " ";
		}
		int score = calculateHandScore(player.hand);
		std::cout<<"|获取点数："<<score<<std::endl;
	}
}
void Room::determineWinner() {
	if (players.empty()) {
		std::cout << "【系统】没有玩家，无法判定赢家。" << std::endl;
		return;
	}

	// 检查是否只有一个玩家未弃牌
	int aliveCount = getAliveCount();
	if (aliveCount == 1) {
		for (auto& player : players) {
			if (!player.hasFolded) {
				player.balance += pot;
				std::cout << "\n============================\n";
				std::cout << "【系统判定】其他玩家都已弃牌，" << player.name << "自动获胜！" << std::endl;
				std::cout << "赢得彩池：" << pot << " 筹码" << std::endl;
				std::cout << "当前余额：" << player.balance << " 筹码" << std::endl;
				std::cout << "=============================\n" << std::endl;
				pot = 0;
				isGameOver = true;
				return;
			}
		}
	}

	// 多个玩家未弃牌，比较牌型
	int maxScore = -1;
	std::string winnerName = "无";
	Player* winner = nullptr;
	
	for (auto& player : players) {
		if (player.hasFolded) {
			continue;
		}
		int score = calculateHandScore(player.hand);
		if (score > maxScore) {
			maxScore = score;
			winner = &player;
			winnerName = player.name;
		}
	}
	
	if (winner != nullptr) {
		winner->balance += pot;
		std::cout << "\n============================\n";
		std::cout << "【系统判定】本局的最终赢家是：" << winnerName << "!!!" << std::endl;
		std::cout << "赢得彩池：" << pot << " 筹码" << std::endl;
		std::cout << "当前余额：" << winner->balance << " 筹码" << std::endl;
		std::cout << "=============================\n" << std::endl;
		pot = 0;
	}
	isGameOver = true;
}

// 比牌功能：比较两个玩家的手牌，返回true表示比牌成功，winnerIdx返回赢家索引
bool Room::compareHands(int playerIdx1, int playerIdx2, int& winnerIdx) {
	if (playerIdx1 < 0 || playerIdx1 >= players.size() || 
	    playerIdx2 < 0 || playerIdx2 >= players.size()) {
		std::cout << "【错误】比牌玩家索引无效。" << std::endl;
		return false;
	}
	
	if (players[playerIdx1].hasFolded || players[playerIdx2].hasFolded) {
		std::cout << "【错误】不能与已弃牌的玩家比牌。" << std::endl;
		return false;
	}
	
	int score1 = calculateHandScore(players[playerIdx1].hand);
	int score2 = calculateHandScore(players[playerIdx2].hand);
	
	std::cout << "\n【比牌】" << players[playerIdx1].name << " vs " << players[playerIdx2].name << std::endl;
	std::cout << players[playerIdx1].name << " 的牌：";
	for (auto& card : players[playerIdx1].hand) {
		std::cout << card.toString() << " ";
	}
	std::cout << "（分数：" << score1 << "）" << std::endl;
	
	std::cout << players[playerIdx2].name << " 的牌：";
	for (auto& card : players[playerIdx2].hand) {
		std::cout << card.toString() << " ";
	}
	std::cout << "（分数：" << score2 << "）" << std::endl;
	
	if (score1 > score2) {
		winnerIdx = playerIdx1;
		std::cout << "【结果】" << players[playerIdx1].name << " 获胜！" << players[playerIdx2].name << " 被淘汰！" << std::endl;
		players[playerIdx2].hasFolded = true;
	}
	else if (score2 > score1) {
		winnerIdx = playerIdx2;
		std::cout << "【结果】" << players[playerIdx2].name << " 获胜！" << players[playerIdx1].name << " 被淘汰！" << std::endl;
		players[playerIdx1].hasFolded = true;
	}
	else {
		// 平局情况，发起比牌的玩家输（规则可以自定义）
		winnerIdx = playerIdx2;
		std::cout << "【结果】平局！" << players[playerIdx1].name << " 被淘汰！（发起者输）" << std::endl;
		players[playerIdx1].hasFolded = true;
	}
	
	return true;
}
void Room::startgame() {
	initDeck();
	dealCards();
	pot = 0;
	currentPlayerIndex = 0;
	isGameOver = false;
	for (auto& p : players) {
		if (p.placeBet(10)) {
			pot += 10;
		}
	}
}
int Room::getAliveCount() {
	int count = 0;
	for (const auto& p : players) {
		if (!p.hasFolded)count++;
	}
	return count;
}
void Room::nextTurn() {
	if (isGameOver) return;
	
	int aliveCount = getAliveCount();
	
	// 如果没有人或只有一个人活着，游戏结束
	if (aliveCount <= 1) {
		isGameOver = true;
		determineWinner();
		return;
	}
	
	// 找到下一个未弃牌的玩家
	int attempts = 0;
	int maxAttempts = players.size();
	
	do {
		currentPlayerIndex = (currentPlayerIndex + 1) % players.size();
		attempts++;
		
		// 防止死循环
		if (attempts > maxAttempts) {
			std::cout << "【错误】无法找到下一个玩家，游戏结束。" << std::endl;
			isGameOver = true;
			determineWinner();
			return;
		}
	} 
	while (players[currentPlayerIndex].hasFolded);
	
	std::cout << "【系统】轮到 " << players[currentPlayerIndex].name << " 操作" << std::endl;
}