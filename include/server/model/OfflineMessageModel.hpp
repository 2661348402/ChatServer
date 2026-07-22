#ifndef OFFLINEMSG_H_
#define OFFLINEMSG_H_

#include <vector>
#include <string>

class OfflineMsgModel {
public:
    bool insert(int userid, const std::string& message);
    bool insertBatch(const std::vector<int>& userids, const std::string& message);
    std::vector<std::string> query(int userid);
    bool remove(int userid);
};

#endif
