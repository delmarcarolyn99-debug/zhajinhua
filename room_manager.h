#pragma once
#include "room.h"
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <map>
#include <vector>
#include <string>
#include <memory>

typedef websocketpp::server<websocketpp::config::asio> server;
typedef websocketpp::connection_hdl connection_hdl;

// 房间信息结构
struct RoomInfo {
    std::string roomId;          // 房间ID（唯一标识）
    Room room;                   // 房间游戏实例
    RoomStatus status;           // 房间状态
    int playerCount;             // 当前玩家数
    std::vector<connection_hdl> playerConnections;  // 玩家连接句柄
    
    RoomInfo() : status(RoomStatus::WAITING), playerCount(0) {}
};

class RoomManager {
public:
    RoomManager();
    ~RoomManager();
    
    // 随机匹配玩家到房间
    RoomInfo* matchPlayer(connection_hdl hdl, const std::string& playerName);
    
    // 获取所有房间列表
    std::vector<RoomInfo*> getRoomList();
    
    // 向房间内所有玩家广播状态
    void broadcastToRoom(server* s, RoomInfo* roomInfo);
    
    // 玩家离开房间
    void leaveRoom(connection_hdl hdl);
    
    // 获取玩家所在的房间
    RoomInfo* getPlayerRoom(connection_hdl hdl);
    
    // 广播房间列表给所有玩家
    void broadcastRoomList(server* s);

private:
    std::map<std::string, RoomInfo*> rooms;  // 房间ID -> 房间信息
    std::map<connection_hdl, std::string, std::owner_less<connection_hdl>> playerToRoom;  // 玩家连接 -> 房间ID
    
    // 创建新房间
    RoomInfo* createRoom();
    
    // 生成唯一房间ID
    std::string generateRoomId();
    
    // 查找有空位的房间
    RoomInfo* findAvailableRoom();
    
    // 构建游戏状态JSON
    std::string buildGameState(RoomInfo* roomInfo);
};
