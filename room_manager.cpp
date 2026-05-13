#include "room_manager.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

RoomManager::RoomManager() {
}

RoomManager::~RoomManager() {
    // 清理所有房间
    for (auto& pair : rooms) {
        delete pair.second;
    }
    rooms.clear();
}

// 生成唯一房间ID
std::string RoomManager::generateRoomId() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    return "ROOM_" + std::to_string(timestamp);
}

// 创建新房间
RoomInfo* RoomManager::createRoom() {
    RoomInfo* roomInfo = new RoomInfo();
    roomInfo->roomId = generateRoomId();
    roomInfo->status = RoomStatus::WAITING;
    roomInfo->playerCount = 0;
    
    rooms[roomInfo->roomId] = roomInfo;
    std::cout << "【系统】创建新房间: " << roomInfo->roomId << std::endl;
    
    return roomInfo;
}

// 查找有空位的房间
RoomInfo* RoomManager::findAvailableRoom() {
    for (auto& pair : rooms) {
        RoomInfo* room = pair.second;
        // 查找WAITING状态且玩家数<3的房间
        if (room->status == RoomStatus::WAITING && room->playerCount < 3) {
            return room;
        }
    }
    return nullptr;
}

// 随机匹配玩家到房间
RoomInfo* RoomManager::matchPlayer(connection_hdl hdl, const std::string& playerName) {
    // 1. 查找有空位的房间
    RoomInfo* room = findAvailableRoom();
    
    // 2. 如果没有，创建新房间
    if (!room) {
        room = createRoom();
    }
    
    // 3. 添加玩家到房间
    room->room.addPlayer(Player(playerName, INITIAL_BALANCE));
    room->playerConnections.push_back(hdl);
    room->playerCount++;
    playerToRoom[hdl] = room->roomId;
    
    std::cout << "【系统】玩家 " << playerName << " 加入房间 " << room->roomId 
              << " (当前人数: " << room->playerCount << "/3)" << std::endl;
    
    // 4. 如果房间已满3人，状态改为PLAYING
    if (room->playerCount == 3) {
        room->status = RoomStatus::PLAYING;
        std::cout << "【系统】房间 " << room->roomId << " 已满，准备开始游戏" << std::endl;
    }
    
    return room;
}

// 获取玩家所在的房间
RoomInfo* RoomManager::getPlayerRoom(connection_hdl hdl) {
    auto it = playerToRoom.find(hdl);
    if (it == playerToRoom.end()) {
        return nullptr;
    }
    
    std::string roomId = it->second;
    auto roomIt = rooms.find(roomId);
    if (roomIt == rooms.end()) {
        return nullptr;
    }
    
    return roomIt->second;
}

// 玩家离开房间
void RoomManager::leaveRoom(connection_hdl hdl) {
    auto it = playerToRoom.find(hdl);
    if (it == playerToRoom.end()) {
        return;
    }
    
    std::string roomId = it->second;
    auto roomIt = rooms.find(roomId);
    if (roomIt == rooms.end()) {
        return;
    }
    
    RoomInfo* room = roomIt->second;
    
    // 简化处理：不清理vector，只减少计数
    // vector会在房间销毁时自动清理
    room->playerCount--;
    playerToRoom.erase(it);
    
    std::cout << "【系统】玩家离开房间 " << roomId << " (剩余人数: " << room->playerCount << ")" << std::endl;
    
    // 如果房间为空，删除房间
    if (room->playerCount == 0) {
        rooms.erase(roomIt);
        delete room;
        std::cout << "【系统】房间 " << roomId << " 已删除" << std::endl;
    }
}

// 构建游戏状态JSON
std::string RoomManager::buildGameState(RoomInfo* roomInfo) {
    if (!roomInfo || roomInfo->room.players.empty()) {
        return "";
    }
    
    json response;
    response["action"] = "update_state";
    response["room_id"] = roomInfo->roomId;
    response["room_status"] = static_cast<int>(roomInfo->status);
    response["pot"] = roomInfo->room.pot;
    
    if (roomInfo->status == RoomStatus::PLAYING && !roomInfo->room.players.empty()) {
        // 安全检查currentPlayerIndex
        if (roomInfo->room.currentPlayerIndex >= 0 && 
            roomInfo->room.currentPlayerIndex < (int)roomInfo->room.players.size()) {
            response["current_turn"] = roomInfo->room.players[roomInfo->room.currentPlayerIndex].name;
        } else {
            response["current_turn"] = roomInfo->room.players[0].name; // 默认第一个玩家
        }
    } else {
        response["current_turn"] = "等待中";
    }
    
    response["is_game_over"] = roomInfo->room.isGameOver;
    
    json players_array = json::array();
    for (auto& player : roomInfo->room.players) {
        json p_json;
        p_json["name"] = player.name;
        p_json["balance"] = player.balance;
        p_json["folded"] = player.hasFolded;
        p_json["looked"] = player.hasLooked;
        
        // 游戏结束后或看牌后才返回手牌信息
        if (roomInfo->room.isGameOver || player.hasLooked) {
            std::string hand_str = "";
            for (auto& card : player.hand) {
                hand_str += card.toString() + " ";
            }
            p_json["hand"] = hand_str;
            p_json["score"] = roomInfo->room.calculateHandScore(player.hand);
        }
        else {
            p_json["hand"] = "???";
            p_json["score"] = -1;
        }
        players_array.push_back(p_json);
    }
    response["players"] = players_array;
    
    return response.dump();
}

// 向房间内所有玩家广播状态
void RoomManager::broadcastToRoom(server* s, RoomInfo* roomInfo) {
    if (!roomInfo) return;
    
    std::string msg = buildGameState(roomInfo);
    if (msg.empty()) return;
    
    for (auto& hdl : roomInfo->playerConnections) {
        try {
            s->send(hdl, msg, websocketpp::frame::opcode::text);
        } catch (...) {
            std::cout << "【错误】发送消息失败" << std::endl;
        }
    }
}

// 获取所有房间列表
std::vector<RoomInfo*> RoomManager::getRoomList() {
    std::vector<RoomInfo*> list;
    for (auto& pair : rooms) {
        list.push_back(pair.second);
    }
    return list;
}

// 广播房间列表给所有玩家
void RoomManager::broadcastRoomList(server* s) {
    json response;
    response["action"] = "room_list_update";
    
    json roomArray = json::array();
    for (auto& pair : rooms) {
        RoomInfo* room = pair.second;
        json roomJson;
        roomJson["room_id"] = room->roomId;
        roomJson["player_count"] = room->playerCount;
        roomJson["status"] = static_cast<int>(room->status);
        roomJson["pot"] = room->room.pot;
        
        // 添加玩家名字列表
        json nameArray = json::array();
        for (auto& player : room->room.players) {
            nameArray.push_back(player.name);
        }
        roomJson["players"] = nameArray;
        
        roomArray.push_back(roomJson);
    }
    
    response["rooms"] = roomArray;
    
    // 发送给所有连接的玩家
    std::string msg = response.dump();
    for (auto& pair : rooms) {
        for (auto& hdl : pair.second->playerConnections) {
            try {
                s->send(hdl, msg, websocketpp::frame::opcode::text);
            } catch (...) {
                std::cout << "【错误】发送房间列表失败" << std::endl;
            }
        }
    }
}
