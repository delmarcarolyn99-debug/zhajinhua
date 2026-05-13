#include<iostream>
#include<string>
#include<websocketpp/config/asio_no_tls.hpp>
#include<websocketpp/server.hpp>
#include<nlohmann/json.hpp>
#include<windows.h>
#include<functional>

#pragma comment(lib,"ws2_32.lib")

#include"card.h"
#include"room.h"
#include"player.h"

typedef websocketpp::server<websocketpp::config::asio>server;
using json=nlohmann::json;
Room gameRoom;

// 发送游戏状态给客户端
void send_game_state(server* s, websocketpp::connection_hdl hdl) {
	if (gameRoom.players.empty()) {
		return;
	}
	
	json response;
	response["action"] = "update_state";
	response["pot"] = gameRoom.pot;
	response["current_turn"] = gameRoom.players[gameRoom.currentPlayerIndex].name;
	response["is_game_over"] = gameRoom.isGameOver;
	
	json players_array = json::array();
	for (auto& player : gameRoom.players) {
		json p_json;
		p_json["name"] = player.name;
		p_json["balance"] = player.balance;
		p_json["folded"] = player.hasFolded;
		p_json["looked"] = player.hasLooked;
		
		// 游戏结束后或看牌后才返回手牌信息
		if (gameRoom.isGameOver || player.hasLooked) {
			std::string hand_str = "";
			for (auto& card : player.hand) {
				hand_str += card.toString() + " ";
			}
			p_json["hand"] = hand_str;
			p_json["score"] = gameRoom.calculateHandScore(player.hand);
		}
		else {
			p_json["hand"] = "???";
			p_json["score"] = -1;
		}
		players_array.push_back(p_json);
	}
	response["players"] = players_array;
	
	s->send(hdl, response.dump(), websocketpp::frame::opcode::text);
}

void on_message(server* s, websocketpp::connection_hdl hdl, server::message_ptr msg) {
	std::string payload = msg->get_payload();
	std::cout << "【收到客户端消息】:" << payload << std::endl;
	try {
		json received_json = json::parse(payload);
		if (received_json["action"] == "hello") {
			json response;
			response["action"] = "hello_reply";
			response["message"] = "你好！C++炸金花服务器已准备就绪！";
			s->send(hdl, response.dump(), websocketpp::frame::opcode::text);
		}
		else if (received_json["action"] == "deal") {
			// 发牌功能：初始化玩家并发牌，但不开始游戏
			gameRoom.players.clear();
			gameRoom.playerMap.clear();
			gameRoom.addPlayer(Player("张三", INITIAL_BALANCE));
			gameRoom.addPlayer(Player("李四", INITIAL_BALANCE));
			gameRoom.addPlayer(Player("王五", INITIAL_BALANCE));
			std::cout << "【系统】创建新游戏，3位玩家加入" << std::endl;
			
			// 重置游戏状态
			gameRoom.initDeck();
			
			// 先清空旧手牌和状态
			for (auto& p : gameRoom.players) {
				p.hand.clear();
				p.hasFolded = false;
				p.hasLooked = false;
			}
			
			// 然后发新牌
			gameRoom.dealCards();
			
			gameRoom.pot = 0;
			gameRoom.currentPlayerIndex = 0;
			gameRoom.isGameOver = false;
			
			std::cout << "【系统】发牌完成！等待开始游戏" << std::endl;
			
			// 发送游戏状态给客户端
			send_game_state(s, hdl);
		}
		else if (received_json["action"] == "start_game") {
			// 开始游戏：收取底注并开始回合
			if (gameRoom.players.empty()) {
				std::cout << "【错误】请先发牌再开始游戏。" << std::endl;
				return;
			}
			
			// 收取底注
			for (auto& p : gameRoom.players) {
				p.balance -= ANTE;
				gameRoom.pot += ANTE;
				p.hasFolded = false;
				p.hasLooked = false;
				std::cout << "【系统】" << p.name << " 支付底注 " << ANTE << " 筹码，余额: " << p.balance << std::endl;
			}
			
			gameRoom.currentPlayerIndex = 0;
			std::cout << "【系统】游戏开始！彩池：" << gameRoom.pot << " 筹码，当前回合: " << gameRoom.players[0].name << std::endl;
			
			// 发送游戏状态给客户端
			send_game_state(s, hdl);
		}
		else if (received_json["action"] == "bet") {
			// 验证玩家列表是否为空
			if (gameRoom.players.empty()) {
				std::cout << "【错误】没有玩家在游戏中。" << std::endl;
				return;
			}
			
			// 验证amount字段是否存在
			if (received_json.find("amount") == received_json.end()) {
				std::cout << "【错误】缺少amount参数。" << std::endl;
				return;
			}
			
			int amount = received_json["amount"];
			
			// 验证金额是否合法
			if (amount <= 0) {
				std::cout << "【错误】下注金额必须大于0。" << std::endl;
				return;
			}
			
			int currIdx = gameRoom.currentPlayerIndex;
			
			// 验证索引是否合法
			if (currIdx < 0 || currIdx >= gameRoom.players.size()) {
				std::cout << "【错误】当前玩家索引无效。" << std::endl;
				return;
			}
			
			// 验证玩家余额是否足够
			if (gameRoom.players[currIdx].balance < amount) {
				std::cout << "【错误】玩家 " << gameRoom.players[currIdx].name 
					<< " 余额不足，当前余额：" << gameRoom.players[currIdx].balance 
					<< "，尝试下注：" << amount << std::endl;
				return;
			}
			
			gameRoom.players[currIdx].balance -= amount;
			gameRoom.pot += amount;
			std::cout << "【系统】" << gameRoom.players[currIdx].name << " 下注 " << amount << "，彩池：" << gameRoom.pot << std::endl;
			gameRoom.nextTurn();
			
			// 发送状态
			send_game_state(s, hdl);
		}
		else if (received_json["action"] == "fold") {
			// 验证玩家列表是否为空
			if (gameRoom.players.empty()) {
				std::cout << "【错误】没有玩家在游戏中。" << std::endl;
				return;
			}
			
			int currIdx = gameRoom.currentPlayerIndex;
			
			// 验证索引是否合法
			if (currIdx < 0 || currIdx >= gameRoom.players.size()) {
				std::cout << "【错误】当前玩家索引无效。" << std::endl;
				return;
			}
			
			// 验证玩家是否已经弃牌
			if (gameRoom.players[currIdx].hasFolded) {
				std::cout << "【错误】玩家 " << gameRoom.players[currIdx].name << " 已经弃牌。" << std::endl;
				return;
			}
			
			gameRoom.players[currIdx].hasFolded = true;
			std::cout << "【系统】" << gameRoom.players[currIdx].name << " 弃牌。" << std::endl;
			gameRoom.nextTurn();
			
			// 发送状态
			send_game_state(s, hdl);
		}
		else if (received_json["action"] == "look") {
			// 看牌功能
			if (gameRoom.players.empty()) {
				std::cout << "【错误】没有玩家在游戏中。" << std::endl;
				return;
			}
			
			int currIdx = gameRoom.currentPlayerIndex;
			if (currIdx < 0 || currIdx >= gameRoom.players.size()) {
				std::cout << "【错误】当前玩家索引无效。" << std::endl;
				return;
			}
			
			std::cout << "【系统】" << gameRoom.players[currIdx].name << " 请求看牌" << std::endl;
			gameRoom.players[currIdx].lookatcards();
			
			// 发送状态
			send_game_state(s, hdl);
		}
		else if (received_json["action"] == "compare") {
			// 比牌功能
			if (gameRoom.players.empty()) {
				std::cout << "【错误】没有玩家在游戏中。" << std::endl;
				return;
			}
			
			if (received_json.find("target_player") == received_json.end()) {
				std::cout << "【错误】缺少target_player参数。" << std::endl;
				return;
			}
			
			int currIdx = gameRoom.currentPlayerIndex;
			int targetIdx = received_json["target_player"];
			
			if (currIdx < 0 || currIdx >= gameRoom.players.size()) {
				std::cout << "【错误】当前玩家索引无效。" << std::endl;
				return;
			}
			
			if (targetIdx < 0 || targetIdx >= gameRoom.players.size()) {
				std::cout << "【错误】目标玩家索引无效。" << std::endl;
				return;
			}
			
			if (currIdx == targetIdx) {
				std::cout << "【错误】不能与自己比牌。" << std::endl;
				return;
			}
			
			// 比牌需要支付双倍注
			if (gameRoom.players[currIdx].balance < COMPARE_COST) {
				std::cout << "【错误】比牌需要 " << COMPARE_COST << " 筹码，余额不足。" << std::endl;
				return;
			}
			
			gameRoom.players[currIdx].balance -= COMPARE_COST;
			gameRoom.pot += COMPARE_COST;
			
			int winnerIdx = -1;
			// 比牌后检查游戏是否结束
			if (gameRoom.compareHands(currIdx, targetIdx, winnerIdx)) {
				// 检查是否只剩一个玩家
				if (gameRoom.getAliveCount() == 1) {
					gameRoom.isGameOver = true;
					gameRoom.determineWinner();
				} else {
					gameRoom.nextTurn();
				}
			}
			
			// 发送状态
			send_game_state(s, hdl);
		}
	}
	catch (json::parse_error& e) {
		std::cout << "【错误】JSON解析失败：" << e.what() << std::endl;
	}
}
int main() {
	SetConsoleOutputCP(CP_UTF8);
	server game_server;
	try {
		game_server.init_asio();
		game_server.set_message_handler(std::bind(&on_message, &game_server, std::placeholders::_1, std::placeholders::_2));
		game_server.listen(9003);
		game_server.start_accept();
		std::cout << "炸金花WebSocket服务器已启动！" << std::endl;
		std::cout << "正在监听端口9003，等待网页端连接……" << std::endl;
		game_server.run();
	}
	catch (websocketpp::exception const& e) {
		std::cout << "服务器启动失败：" << e.what() << std::endl;
	}
	return 0;
}