#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <muduo/base/Logging.h>

Config& Config::instance() {
    static Config cfg;
    return cfg;
}

void Config::load(const std::string& filepath) {
    _data.clear();
    std::ifstream file(filepath);
    if (!file.is_open()) {
        LOG_ERROR << "Failed to open config file: " << filepath;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        // trim trailing \r (CRLF)
        if (!value.empty() && value.back() == '\r') value.pop_back();
        if (!key.empty()) {
            _data[key] = value;
        }
    }
    LOG_INFO << "Config loaded: " << _data.size() << " entries";
}

std::string Config::get(const std::string& key, const std::string& defaultValue) const {
    auto it = _data.find(key);
    return (it != _data.end()) ? it->second : defaultValue;
}

int Config::getInt(const std::string& key, int defaultValue) const {
    auto it = _data.find(key);
    if (it == _data.end()) return defaultValue;
    try {
        return std::stoi(it->second);
    } catch (...) {
        return defaultValue;
    }
}
