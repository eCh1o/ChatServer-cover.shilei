#ifndef OFFLINEMSGMODEL_H
#define OFFLINEMSGMODEL_H

using namespace std;
#include<vector>
#include<string>
//提供离线消息表得接口方法
class OfflineMsg
{
public:
    //存储用户得离线消息
    void insert(int userid,string msg);
    //删除用户得离线消息
    void remove(int userid);
    //查询用户得离线消息
    vector<string> query(int userid);

private:
};

#endif