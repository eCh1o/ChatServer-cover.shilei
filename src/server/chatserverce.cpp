#include "chatservece.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <vector>
#include <map>
using namespace muduo;
using namespace std;

ChatServerce *ChatServerce::instance()
{
    static ChatServerce serverce;
    return &serverce;
}
//处理登录业务
void ChatServerce::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"];
    string pwd = js["password"];
    User user = _userModel.query(id);
    if (user.getId() == id && user.getPwd() == pwd)
    {
        if (user.getState() == "online")
        {
            //该用户已经登录，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "this account is loged";
            conn->send(response.dump());
        }
        else
        {
            //登陆成功，记录用户连接
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnectionMap.insert({id, conn});
            }

            // id用户登录成功后,向redis订阅channel(id)
            _redis.subscribe(id);
            //登陆成功，更新用户状态信息
            user.setState("online");
            _userModel.updateState(user);
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();
            //查询用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if (!vec.empty())
            {
                response["offlinemsg"] = vec;
                //读取离线消息后，删除掉离线消息
                _offlineMsgModel.remove(id);
            }
            //      查询该用户的好友信息并返回
            vector<User> userVec = _friendModel.query(id);
            if (!userVec.empty())
            {
                vector<string> vec2;
                for (User &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }
            //查询用户的群组信息
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if (!groupuserVec.empty())
            {
                // group:[{groupid:[xxx,xxx,xxx,xxx]}]
                vector<string> groupV;
                for (Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }
                response["groups"] = groupV;
            }
            conn->send(response.dump());
        }
    }
    else
    {
        //登陆失败,该用户不存在
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "password error";
        conn->send(response.dump());
    }
}
//处理注册业务
void ChatServerce::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];
    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    if (state)
    {
        //注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}

//服务器异常断开，数据重置的方法
void ChatServerce::reset()
{
    //把所有online状态的用户，设置为offline
    _userModel.resetState();
}

MsgHandler ChatServerce::getHandeler(int msgid)
{
    auto it = _handlerMap.find(msgid);
    if (it == _handlerMap.end())
    {
        // LOG_ERROR<<"msgid:"<<msgid<<"can not find handler!!";
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time)
        {
            LOG_ERROR << "msgid:" << msgid << "can not find handeler";
        };
    }
    else
    {
        return _handlerMap[msgid];
    }
}

//构造
ChatServerce::ChatServerce()
{
    _handlerMap.insert({LOGIN_MSG, bind(&ChatServerce::login, this, _1, _2, _3)});
    _handlerMap.insert({REG_MSG, bind(&ChatServerce::reg, this, _1, _2, _3)});
    _handlerMap.insert({ONE_CHAT_MSG, bind(&ChatServerce::oneChat, this, _1, _2, _3)});
    _handlerMap.insert({ADD_FRIEND_MSG, bind(&ChatServerce::addFriend, this, _1, _2, _3)});
    _handlerMap.insert({CREATE_GROUP_MSG, bind(&ChatServerce::createGroup, this, _1, _2, _3)});
    _handlerMap.insert({GROUP_CHAT_MSG, bind(&ChatServerce::groupChat, this, _1, _2, _3)});
    _handlerMap.insert({ADD_GROUP_MSG, bind(&ChatServerce::addGroup, this, _1, _2, _3)});
    _handlerMap.insert({LOG_OUT_MSG, bind(&ChatServerce::logOut, this, _1, _2, _3)});

    //连接redis服务器
    if (_redis.connect())
    {
        _redis.init_notify_handler(bind(&ChatServerce::handleRedisSubscribeMessage, this, _1, _2));
    }
}

//客户端异常断开
void ChatServerce::clientClpseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnectionMap.begin(); it != _userConnectionMap.end(); it++)
        {
            if (it->second == conn)
            {
                //从map表删除用户的连接信息
                user.setId(it->first);
                _userConnectionMap.erase(it);
                break;
            }
        }
    }

    _redis.unsubscribe(user.getId());
    //更新用户的状态信息
    if (user.getId() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
}

//一对一聊天业务
void ChatServerce::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["toid"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnectionMap.find(toid);
        if (it != _userConnectionMap.end())
        {
            // toid在线，发送消息
            it->second->send(js.dump());
            return;
        }
    }
    //查询用户是否在线
    User user = _userModel.query(toid);
    if (user.getState() == "online")
    {
        _redis.publish(toid, js.dump());
        return;
    }
    // toid不在线，存储离线消息
    _offlineMsgModel.insert(toid, js.dump());
}

//添加好友业务
void ChatServerce::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    //存储好友信息
    _friendModel.insert(userid, friendid);
}

//创建群组业务
void ChatServerce::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];
    //存储新创建的群组信息
    Group group(-1, name, desc);
    if (_groupModel.createGroup(group))
    {
        //存储群组创建人的信息
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}

//加入群组业务
void ChatServerce::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}

//群组聊天业务
void ChatServerce::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec)
    {
        auto it = _userConnectionMap.find(id);
        if (it != _userConnectionMap.end())
        {
            it->second->send(js.dump());
        }
        else
        {
            //查询toid是否在线
            User user = _userModel.query(id);
            if (user.getState() == "online")
            {
                _redis.publish(id, js.dump());
            }
            else
            {
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}
//用户登出业务
void ChatServerce::logOut(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);

        auto it = _userConnectionMap.find(userid);
        if (it != _userConnectionMap.end())
        {
            _userConnectionMap.erase(it);
        }
    }
    //用户下线，在redis中取消订阅通道
    _redis.unsubscribe(userid);
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}

//从redis消息队列中获取订阅的消息
void ChatServerce::handleRedisSubscribeMessage(int userid, string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnectionMap.find(userid);
    if (it != _userConnectionMap.end())
    {
        it->second->send(msg);
        return;
    }
    _offlineMsgModel.insert(userid, msg);
}