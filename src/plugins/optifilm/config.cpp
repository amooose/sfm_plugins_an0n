#include "config.h"
#include <fstream>
#include <filesystem>
#include <cstdlib>

bool Config::Load(const char* filename)
{
    m_Values.clear();
    namespace fs = std::filesystem;

    fs::path configDir = fs::current_path() / "workshop" / "addons" / "config";
    if (!fs::exists(configDir))
        fs::create_directories(configDir);

    m_ConfigFile = configDir / filename;

    if (!fs::exists(m_ConfigFile))
    {
        std::ofstream out(m_ConfigFile);
        out <<
            "opt1_memclear=1\n"
            "opt2_noKeyLag=1\n"
            "opt3_reduceFKLag=1\n"
            "opt4_timelineFPSBoost=1\n"
            "opt5_enginePump2ms=1\n"
            "opt6_skipResMsg=1\n";
    }

    std::ifstream file(m_ConfigFile);
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

void Config::AppendDefault(const char* key, const char* value)
{
    m_Values[key] = value;

    std::string contents;
    {
        std::ifstream in(m_ConfigFile, std::ios::binary);
        std::ostringstream ss;
        ss << in.rdbuf();
        contents = ss.str();
    }

    std::ofstream out(m_ConfigFile, std::ios::app);
    if (!contents.empty() && contents.back() != '\n')
        out << '\n';
    out << key << "=" << value << "\n";
}

const char* Config::GetString(const char* key, const char* defaultValue)
{
    auto it = m_Values.find(key);
    if (it != m_Values.end())
        return it->second.c_str();

    AppendDefault(key, defaultValue);
    return m_Values[key].c_str();
}

int Config::GetInt(const char* key, int defaultValue)
{
    auto it = m_Values.find(key);
    if (it != m_Values.end())
        return atoi(it->second.c_str());

    AppendDefault(key, std::to_string(defaultValue).c_str());
    return defaultValue;
}

bool Config::GetBool(const char* key, bool defaultValue)
{
    auto it = m_Values.find(key);
    if (it != m_Values.end())
        return atoi(it->second.c_str()) != 0;

    AppendDefault(key, defaultValue ? "1" : "0");
    return defaultValue;
}

void Config::SetBool(const char* key, bool value)
{
    m_Values[key] = value ? "1" : "0";

    std::ofstream out(m_ConfigFile);
    for (auto& pair : m_Values)
        out << pair.first << "=" << pair.second << "\n";
}