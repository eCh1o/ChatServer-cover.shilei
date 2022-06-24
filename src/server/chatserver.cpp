#include "chatserver.hpp"
#include"chatservece.hpp"
#include"json.hpp"
using json = nlohmann::json;

ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg)
    : _server(loop, listenAddr, nameArg), _loop(loop)
{
    //注册连接回调
    _server.setConnectionCallback(bind(&ChatServer::onConnection,this,_1));
    //注册消息回调
    _server.setMessageCallback(bind(&ChatServer::onMessage,this,_1,_2,_3));
    //设置线程数量
    _server.setThreadNum(4);
}
//启动服务
void ChatServer::start()
{
    _server.start();
}
//连接回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    if(!conn->connected())
    {
        ChatServerce::instance()->clientClpseException(conn);
        conn->shutdown();
    }
}
//读写事件回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buffer,
                           Timestamp time)
{
    string buf = buffer->retrieveAllAsString();
    //数据的反序列化
    json js = json::parse(buf);
    auto msgHandler = ChatServerce::instance()->getHandeler(js["msgid"].get<int>());
    //回调绑定好的事件处理器，来执行相应的业务处理
    msgHandler(conn,js,time);
}
