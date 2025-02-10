#pragma once

#include <string>
#include <fstream>
#include <cstdlib>

inline void loadEnvFile(const std::string& path = ".env") {
    std::ifstream file(path);
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // 跳过空行和注释行
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // 查找第一个等号
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // 去除首尾空格
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t\r") + 1);

        // 如果值被引号包围，去除引号
        if (value.size() >= 2 && (value[0] == '"' || value[0] == '\'') && 
            value[0] == value[value.size()-1]) {
            value = value.substr(1, value.size() - 2);
        }

        // 只在环境变量不存在时设置
        if (getenv(key.c_str()) == nullptr) {
            setenv(key.c_str(), value.c_str(), 0);
        }
    }
}

inline std::string getEnvVar(const std::string& key, const std::string& defaultValue) {
    const char* val = getenv(key.c_str());
    return val ? std::string(val) : defaultValue;
}
