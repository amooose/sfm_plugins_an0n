#include "config.h"
#include <fstream>
#include <filesystem>
#include <cstdlib>

bool Config::Load(const char* filename)
{
    m_Values.clear();

    namespace fs = std::filesystem;

    fs::path configDir =
        fs::current_path() /
        "workshop" /
        "addons" /
        "config";

    if (!fs::exists(configDir))
        fs::create_directories(configDir);

    fs::path configFile = configDir / filename;

    if (!fs::exists(configFile))
    {
        std::ofstream out(configFile);

        out <<
            "opt1_memclear=1\n"
            "opt2_noKeyLag=1\n"
            "opt3_reduceFKLag=1\n"
            "opt4_timelineFPSBoost=1\n"
            "opt5_enginePump2ms=1\n";
    }

    std::ifstream file(configFile);
    if (!file.is_open())
        return false;

    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        size_t pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        m_Values[line.substr(0, pos)] = line.substr(pos + 1);
    }

    return true;
}

const char* Config::GetString(const char* key, const char* defaultValue)
{
    auto it = m_Values.find(key);
    return (it != m_Values.end()) ? it->second.c_str() : defaultValue;
}

int Config::GetInt(const char* key, int defaultValue)
{
    auto it = m_Values.find(key);
    return (it != m_Values.end()) ? atoi(it->second.c_str()) : defaultValue;
}

bool Config::GetBool(const char* key, bool defaultValue)
{
    auto it = m_Values.find(key);
    return (it != m_Values.end()) ? atoi(it->second.c_str()) != 0 : defaultValue;
}