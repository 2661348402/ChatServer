#ifndef SHA256_HPP_
#define SHA256_HPP_

#include <string>

class SHA256 {
public:
    static std::string hash(const std::string& input);
};

#endif
