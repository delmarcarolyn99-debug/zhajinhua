#include"player.h"
#include<iostream>
Player::Player(std::string playerName, int initialBalance) {
	name = playerName;
	balance=initialBalance;
	hasFolded = false;
	hasLooked = false;
}
void Player::receivecard(Card c) {
	hand.push_back(c);
}
void Player::lookatcards() {
	if (!hasFolded && !hasLooked) {
		hasLooked = true;
		std::cout << "【系统】" << name << "选择了看牌!" << std::endl;
	}
}
void Player::fold() {
	if (!hasFolded) {
		hasFolded = true;
		std::cout << "【系统】" <<std::endl<< name << "选择了弃牌!" << std::endl;
	}
}
bool Player::placeBet(int amount) {
	if (hasFolded) {
		std::cout << "【错误】"<<std::endl << name << "已经弃牌，无法下注。" << std::endl;
		return false;
	}
	if (balance >= amount) {
		balance-=amount;
		std::cout<<"【下注】"<<std::endl<<name<<"下注了"<<amount<<"筹码。当前剩余："<<balance<<std::endl;
		return true;
	}
	else if(balance>0){
		std::cout << "【余额不足】" <<std::endl<< name << "想要下注" << amount << ",但只剩下" << balance << "筹码！" << std::endl;
		std::cout<<"是否选择<ALL IN>?(y/n): ";
		char choice;
		std::cin >> choice;
		if (choice == 'y' || choice == 'Y') {
			int allInAmount = balance;
			balance = 0;
			std::cout << "【系统】" << name << "选择了全下！下注金额：" << allInAmount << std::endl;
			return true;
		}
		else {
			std::cout << "【系统】" << name << "取消了全下。" << std::endl;
			return false;
		}
	}
	else {
		std::cout << "【错误】" << name << "筹码已用完，无法下注。" << std::endl;
		return false;
	}
}
void Player::printStatus() const {
	std::string stateStr = hasFolded ? "已弃牌" : (hasLooked ? "已看牌（明）" : "未看牌（暗）");
	std::cout << "玩家：" << name << "|筹码：" << balance << "|状态：" << stateStr << std::endl;
}