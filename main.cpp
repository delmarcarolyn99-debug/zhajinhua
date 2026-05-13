#include<iostream>
#include<string>
#include<websocketpp/config/asio_no_tls.hpp>
#include<websocketpp/server.hpp>
#include<nlohmann/json.hpp>
#include<windows.h>
#include<functional>
#include<random>

#pragma comment(lib,"ws2_32.lib")

#include"card.h"
#include"room.h"
#include"player.h"
#include"room_manager.h"

typedef websocketpp::server<websocketpp::config::asio>server;
using json=nlohmann::json;
RoomManager roomManager;

// 生成随机玩家名字
std::string generatePlayerName() {
    std::string names[] = {"张三", "李四", "王五", "赵六", "孙七", "周八", "吴九", "郑十"};
    static int nameIndex = 0;
    std::string name = names[nameIndex % 8] + std::to_string(nameIndex / 8 + 1);
    nameIndex++;
    return name;
}

// 错误消息发送
void send_error(server* s, websocketpp::connection_hdl hdl, const std::string& message) {
    json response;
    response["action"] = "error";
    response["message"] = message;
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
		else if (received_json["action"] == "join") {
			// 玩家加入房间
			std::string playerName = received_json.value("player_name", generatePlayerName());
			
			RoomInfo* roomInfo = roomManager.matchPlayer(hdl, playerName);
			
			// 发送加入成功消息
			json response;
			response["action"] = "join_success";
			response["room_id"] = roomInfo->roomId;
			response["status"] = static_cast<int>(roomInfo->status);
			response["player_name"] = playerName;
			s->send(hdl, response.dump(), websocketpp::frame::opcode::text);
			
			// 如果房间未满，发送等待消息
			if (roomInfo->playerCount < 3) {
				json waitMsg;
				waitMsg["action"] = "waiting";
				waitMsg["message"] = "等待其他玩家加入... (" + std::to_string(roomInfo->playerCount) + "/3)";
				s->send(hdl, waitMsg.dump(), websocketpp::frame::opcode::text);
			}
			// 如果房间已满，自动开始游戏
			else {
				// 初始化游戏状态
				roomInfo->room.initDeck();
				roomInfo->room.pot = 0;
				roomInfo->room.currentPlayerIndex = 0;
				roomInfo->room.isGameOver = false;
				
				// 清空手牌和状态
				for (auto& p : roomInfo->room.players) {
					p.hand.clear();
					p.hasFolded = false;
					p.hasLooked = false;
				}
				
				// 发牌
				roomInfo->room.dealCards();
				
				// 收取底注
				for (auto& p : roomInfo->room.players) {
					p.balance -= ANTE;
					roomInfo->room.pot += ANTE;
				}
				
				std::cout << "【系统】房间 " << roomInfo->roomId << " 游戏开始！" << std::endl;
				std::cout << "【系统】奖池: " << roomInfo->room.pot << std::endl;
				
				// 广播游戏状态
				roomManager.broadcastToRoom(s, roomInfo);
			}
			
			// 广播房间列表
			roomManager.broadcastRoomList(s);
		}
		else if (received_json["action"] == "deal") {
			// 多房间模式下不再需要单独的deal动作
			send_error(s, hdl, "多房间模式下请使用join加入房间");
		}
		else if (received_json["action"] == "start_game") {
			// 多房间模式下自动开始，不需要手动start
			send_error(s, hdl, "多房间模式下游戏自动开始");
		}
		else if (received_json["action"] == "bet") {
			RoomInfo* roomInfo = roomManager.getPlayerRoom(hdl);
			if (!roomInfo) {
				send_error(s, hdl, "你还没有加入房间");
				return;
			}
			
			if (roomInfo->status != RoomStatus::PLAYING) {
				send_error(s, hdl, "游戏尚未开始");
				return;
			}
			
			if (received_json.find("amount") == received_json.end()) {
				send_error(s, hdl, "缺少amount参数");
				return;
			}
			
			int amount = received_json["amount"];
			if (amount <= 0) {
				send_error(s, hdl, "下注金额必须大于0");
				return;
			}
			
			int currIdx = roomInfo->room.currentPlayerIndex;
			if (currIdx < 0 || currIdx >= roomInfo->room.players.size()) {
				send_error(s, hdl, "当前玩家索引无效");
				return;
			}
			
			if (roomInfo->room.players[currIdx].balance < amount) {
				send_error(s, hdl, "余额不足");
				return;
			}
			
			roomInfo->room.players[currIdx].balance -= amount;
			roomInfo->room.pot += amount;
			std::cout << "【系统】" << roomInfo->room.players[currIdx].name << " 下注 " << amount << std::endl;
			
			roomInfo->room.nextTurn();
			roomManager.broadcastToRoom(s, roomInfo);
		}
		else if (received_json["action"] == "fold") {
			RoomInfo* roomInfo = roomManager.getPlayerRoom(hdl);
			if (!roomInfo) {
				send_error(s, hdl, "你还没有加入房间");
				return;
			}
			
			int currIdx = roomInfo->room.currentPlayerIndex;
			if (currIdx < 0 || currIdx >= roomInfo->room.players.size()) {
				send_error(s, hdl, "当前玩家索引无效");
				return;
			}
			
			if (roomInfo->room.players[currIdx].hasFolded) {
				send_error(s, hdl, "你已经弃牌");
				return;
			}
			
			roomInfo->room.players[currIdx].hasFolded = true;
			std::cout << "【系统】" << roomInfo->room.players[currIdx].name << " 弃牌" << std::endl;
			
			roomInfo->room.nextTurn();
			roomManager.broadcastToRoom(s, roomInfo);
		}
		else if (received_json["action"] == "look") {
			RoomInfo* roomInfo = roomManager.getPlayerRoom(hdl);
			if (!roomInfo) {
				send_error(s, hdl, "你还没有加入房间");
				return;
			}
			
			int currIdx = roomInfo->room.currentPlayerIndex;
			if (currIdx < 0 || currIdx >= roomInfo->room.players.size()) {
				send_error(s, hdl, "当前玩家索引无效");
				return;
			}
			
			std::cout << "【系统】" << roomInfo->room.players[currIdx].name << " 看牌" << std::endl;
			roomInfo->room.players[currIdx].lookatcards();
			
			roomManager.broadcastToRoom(s, roomInfo);
		}
		else if (received_json["action"] == "compare") {
			RoomInfo* roomInfo = roomManager.getPlayerRoom(hdl);
			if (!roomInfo) {
				send_error(s, hdl, "你还没有加入房间");
				return;
			}
			
			if (received_json.find("target_player") == received_json.end()) {
				send_error(s, hdl, "缺少target_player参数");
				return;
			}
			
			int currIdx = roomInfo->room.currentPlayerIndex;
			int targetIdx = received_json["target_player"];
			
			if (currIdx < 0 || currIdx >= roomInfo->room.players.size()) {
				send_error(s, hdl, "当前玩家索引无效");
				return;
			}
			
			if (targetIdx < 0 || targetIdx >= roomInfo->room.players.size()) {
				send_error(s, hdl, "目标玩家索引无效");
				return;
			}
			
			if (currIdx == targetIdx) {
				send_error(s, hdl, "不能与自己比牌");
				return;
			}
			
			if (roomInfo->room.players[currIdx].balance < COMPARE_COST) {
				send_error(s, hdl, "比牌需要 " + std::to_string(COMPARE_COST) + " 筹码，余额不足");
				return;
			}
			
			roomInfo->room.players[currIdx].balance -= COMPARE_COST;
			roomInfo->room.pot += COMPARE_COST;
			
			int winnerIdx = -1;
			if (roomInfo->room.compareHands(currIdx, targetIdx, winnerIdx)) {
				// 检查是否只剩一个玩家
				if (roomInfo->room.getAliveCount() == 1) {
					roomInfo->room.isGameOver = true;
					roomInfo->room.determineWinner();
				} else {
					roomInfo->room.nextTurn();
				}
			}
			
			roomManager.broadcastToRoom(s, roomInfo);
		}
	}
	catch (json::parse_error& e) {
		std::cout << "【错误】JSON解析失败：" << e.what() << std::endl;
	}
}

// 连接关闭回调
void on_close(server* s, websocketpp::connection_hdl hdl) {
	std::cout << "【系统】玩家断开连接" << std::endl;
	roomManager.leaveRoom(hdl);
	roomManager.broadcastRoomList(s);
}

int main() {
	SetConsoleOutputCP(CP_UTF8);
	server game_server;
	try {
		game_server.init_asio();
		game_server.set_message_handler(std::bind(&on_message, &game_server, std::placeholders::_1, std::placeholders::_2));
		game_server.set_close_handler(std::bind(&on_close, &game_server, std::placeholders::_1));
		game_server.listen(9003);
		game_server.start_accept();
		std::cout << "炸金花WebSocket服务器已启动！" << std::endl;
		std::cout << "正在监听端口9003，等待网页端连接……" << std::endl;
		std::cout << "【提示】多房间模式已启用，支持并发游戏" << std::endl;
		game_server.run();
	}
	catch (websocketpp::exception const& e) {
		std::cout << "服务器启动失败：" << e.what() << std::endl;
	}
	return 0;
}
