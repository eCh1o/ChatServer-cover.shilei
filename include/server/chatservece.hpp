#ifndef CHATSERVERCE_H
#define CHATSERVERCE_H
#include "json.hpp"
#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include <mutex>
#include "usermodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "redis.hpp"
#include "offlinemsgmodel.hpp"
using json = nlohmann::json;
using namespace std;
using namespace muduo;
using namespace muduo::net;

using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp)>;

//聊天服务器业务类
class ChatServerce
{
public:
    //获取单例对象接口函数
    static ChatServerce *instance();
    //登录业务
    void login(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //处理注册业务
    void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //一对一聊天业务
    void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //添加好友业务
    void addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //创建群组业务
    void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //加入群组业务
    void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //群组聊天业务
    void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //用户登出业务
    void logOut(const TcpConnectionPtr &conn, json &js, Timestamp time);
    MsgHandler getHandeler(int msgid);
    //从redis消息队列中获取订阅的消息
    void handleRedisSubscribeMessage(int userid, string msg);
    //服务器异常退出，数据重置方法
    void reset();
    //处理客户端一场退出
    void clientClpseException(const TcpConnectionPtr &);

private:
    ChatServerce();
    //存储消息id和其对应业务处理方法的表
    unordered_map<int, MsgHandler> _handlerMap;
    //定义互斥锁，保证_userConnectionMap的线程安全
    mutex _connMutex;
    //存储在线用户的通信连接
    unordered_map<int, TcpConnectionPtr> _userConnectionMap;
    //数据操作类对象
    UserModel _userModel;
    FriendModel _friendModel;
    OfflineMsg _offlineMsgModel;
    GroupModel _groupModel;

    // redis操作对象
    Redis _redis;
};

#endif