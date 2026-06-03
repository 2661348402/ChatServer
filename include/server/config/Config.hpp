#ifndef CONFIG_HPP_
#define CONFIG_HPP_

#include <string>
#include <unordered_map>

class Config {
public:
    static Config& instance();

    void load(const std::string& filepath);

    std::string get(const std::string& key, const std::string& defaultValue = "") const;
    int getInt(const std::string& key, int defaultValue = 0) const;

private:
    Config() = default;
    std::unordered_map<std::string, std::string> _data;
};

#endif
