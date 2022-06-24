#include "friendmodel.hpp"
#include "db.hpp"

void FriendModel::insert(int userid, int friendid)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into Friend values(%d,%d)", userid,friendid);

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

//返回用户的好友列表
vector<User> FriendModel::query(int userid)
{
        char sql[1024] = {0};
    sprintf(sql, "select a.id,a.name,a.state from User a inner join Friend b on b.friendid = a.id where b.userid = %d", userid);
    vector<User> vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            //把用户的多条离线消息放入vector中返回
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(user);
            }
            mysql_free_result(res);
            return vec;
        }
    }
    return vec;
}