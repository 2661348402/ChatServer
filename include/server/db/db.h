#ifndef DB_H_
#define DB_H_
#include <mysql/mysql.h>
#include <string>

class MySQL {
public:
    MySQL();
    ~MySQL();

    bool connect();
    bool connect(const std::string& host, int port,
                 const std::string& user, const std::string& password,
                 const std::string& dbname);

    bool update(const std::string& sql);
    MYSQL_RES* query(const std::string& sql);
    MYSQL* getConnection();

    std::string escape(const std::string& str);

private:
    MYSQL* _conn;
};

#endif
