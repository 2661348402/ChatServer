#ifndef USER_H_
#define USER_H_
#include <string>
using namespace std;

class User
{
private:
    int id;
    string name;
    string password;
    string state;   // 

public:
    // 无参构造（设置默认值）
    User() 
    {
        id = -1;
        name = "";          // 默认空名字
        password = "";      // 默认空密码
        state = "offline";  // 默认离线状态（关键！）
    }
    // 全参构造
    User(int id, string name, string pwd, string state)
    {
        this->id = id;
        this->name = name;
        this->password = pwd;
        this->state = state;
    }


    // set
    void setId(int id)               { this->id = id; }
    void setName(string name)        { this->name = name; }
    void setPwd(string pwd)          { this->password = pwd; }
    void setState(string state)         { this->state = state; }

    // get
    int getId() const                { return id; }
    string getName() const           { return name; }
    string getPwd() const            { return password; }
    string getState() const             { return state; }
};

#endif