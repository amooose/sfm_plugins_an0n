#pragma once
#include <string>
#include <unordered_map>
#include <filesystem>

class Config
{
public:
    bool Load(const char* filename);
    const char* GetString(const char* key, const char* defaultValue = "");
    int         GetInt(const char* key, int defaultValue = 0);
    bool        GetBool(const char* key, bool defaultValue = true);
    void        SetBool(const char* key, bool value);

private:
    void AppendDefault(const char* key, const char* value);

    std::unordered_map<std::string, std::string> m_Values;
    std::filesystem::path m_ConfigFile;
};