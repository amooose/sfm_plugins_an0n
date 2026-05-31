#pragma once

#include <string>
#include <unordered_map>

class Config
{
public:
    bool Load(const char* filename);

    const char* GetString(const char* key, const char* defaultValue = "");
    int GetInt(const char* key, int defaultValue = 0);
    bool GetBool(const char* key, bool defaultValue = false);

private:
    std::unordered_map<std::string, std::string> m_Values;
};