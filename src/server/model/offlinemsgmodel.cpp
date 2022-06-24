#include "offlinemsgmodel.hpp"
#include "db.hpp"

void OfflineMsg::insert(int userid, string msg)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into OfflineMessage values(%d,'%s')", userid, msg.c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}
//删除用户得离线消息
void OfflineMsg::remove(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "delete from OfflineMessage where userid = %d", userid);

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}
//查询用户得离线消息
vector<string> OfflineMsg::query(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "select message from OfflineMessage where userid = %d", userid);
    vector<string> vec;
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
                vec.push_back(row[0]);
            }
            mysql_free_result(res);
            return vec;
        }
    }
    return vec;
}