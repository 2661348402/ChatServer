#include "db.h"
#include <iostream>
#include <string>
#include <muduo/base/Logging.h>
static string server = "127.0.0.1";
static string user = "root";
static string password = "123456";
static string dbname = "chat";

// 构造函数：初始化连接句柄
MySQL::MySQL()
{
    // 分配MYSQL对象，初始化句柄
    _conn = mysql_init(nullptr);
    if (_conn == nullptr)
    {
        LOG_ERROR << "mysql_init 初始化失败！";
    }
}

// 析构函数：释放数据库连接
MySQL::~MySQL()
{
    if (_conn != nullptr)
    {
        // 关闭连接
        mysql_close(_conn);
    }
}

// 连接数据库
bool MySQL::connect()
{
    // 这里请根据你的数据库信息修改！！！
    // 参数：连接句柄、主机、用户名、密码、数据库名、端口、unix_socket、client_flag
    MYSQL* p = mysql_real_connect(_conn,
                                  server.c_str(),    // 数据库IP
                                    user.c_str(),         // 用户名
                                  password.c_str(),       // 密码
                                  dbname.c_str(),       // 数据库名
                                  3306,           // 端口
                                  nullptr,
                                  0);
    if (p == nullptr)
    {
        LOG_ERROR << "数据库连接失败：" << mysql_error(_conn);
        return false;
    }

    // 设置客户端字符集为utf8，支持中文
    mysql_query(_conn, "set names utf8");
    return true;
}

// 更新操作（insert、update、delete）
bool MySQL::update(string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        cerr << "更新操作失败：" << mysql_error(_conn) << endl;
        cerr << "失败SQL：" << sql << endl;
        return false;
    }
    return true;
}

// 查询操作（select）
MYSQL_RES* MySQL::query(string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        cerr << "查询操作失败：" << mysql_error(_conn) << endl;
        cerr << "失败SQL：" << sql << endl;
        return nullptr;
    }
    // 获取查询结果集
    return mysql_store_result(_conn);
}

MYSQL* MySQL::getConnection(){
    return _conn;
}