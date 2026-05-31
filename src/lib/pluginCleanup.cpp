#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <regex>
#include <windows.h>
namespace fs = std::filesystem;

bool cleanupPlugins(const std::string& pluginName) {
    std::filesystem::path vdfPath =
        std::filesystem::current_path() /
        (std::string("workshop/addons/") + pluginName + ".vdf");
    std::ifstream vdfFile(vdfPath);
    if (!vdfFile.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(vdfFile)), std::istreambuf_iterator<char>());
    vdfFile.close();

    std::regex dllRegex("\"([^\"]+\\.dll)\"");
    std::smatch match;

    if (!std::regex_search(content, match, dllRegex)) {
        return false;
    }

    fs::path targetFilePath(match[1].str());
    fs::path vdfDir = fs::path(vdfPath).parent_path();
    fs::path searchDir = vdfDir;

    if (!fs::exists(searchDir)) return false;

    
    std::string suff = ".dll";
    std::string pluginPrefix = pluginName + "_";

    //Delete older plugin .dlls since SFM wont clean up non .dll files
    for (const auto& entry : fs::directory_iterator(searchDir)) {
        std::string filename = entry.path().filename().string();

        if (filename.rfind(pluginPrefix, 0) == 0 && entry.path().extension().string().rfind(suff, 0) == 0 && entry.path().filename() != targetFilePath.filename()) {
            try {
                fs::remove(entry.path());
            }
            catch (const fs::filesystem_error& e) {

                //couldnt delete
            }
        }
    }

    return true;
}

