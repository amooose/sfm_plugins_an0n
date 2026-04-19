#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <regex>
#include <windows.h>
namespace fs = std::filesystem;

bool cleanupPlugins(const std::string& vdfFilePath) {
    std::ifstream vdfFile(vdfFilePath);
    if (!vdfFile.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(vdfFile)), std::istreambuf_iterator<char>());
    vdfFile.close();

    std::regex dllRegex("\"([^\"]+\\.dll)\"");
    std::smatch match;

    if (!std::regex_search(content, match, dllRegex)) {
        return false;
    }

    fs::path targetFilePath(match[1].str());
    fs::path vdfDir = fs::path(vdfFilePath).parent_path();
    fs::path searchDir = vdfDir;

    if (!fs::exists(searchDir)) return false;

    std::string prefix = "timelineprecisionpatch_";

    //Delete older plugin .dlls since SFM wont clean up non .dll files
    for (const auto& entry : fs::directory_iterator(searchDir)) {
        std::string filename = entry.path().filename().string();

        if (filename.rfind(prefix, 0) == 0 && entry.path().filename() != targetFilePath.filename()) {
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

