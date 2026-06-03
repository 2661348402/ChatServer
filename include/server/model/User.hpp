#ifndef USER_H_
#define USER_H_
#include <string>

class User {
public:
    User() : id(-1), name(""), password(""), state("offline") {}
    User(int id, std::string name, std::string pwd, std::string state)
        : id(id), name(name), password(pwd), state(state) {}

    void setId(int id)               { this->id = id; }
    void setName(std::string name)   { this->name = name; }
    void setPwd(std::string pwd)     { this->password = pwd; }
    void setState(std::string state) { this->state = state; }

    int getId() const                { return id; }
    std::string getName() const      { return name; }
    std::string getPwd() const       { return password; }
    std::string getState() const     { return state; }

private:
    int id;
    std::string name;
    std::string password;
    std::string state;
};

#endif
