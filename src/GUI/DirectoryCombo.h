#pragma once

#include <filesystem>

#include "Geode/utils/file.hpp"

namespace GUI
{
class DirectoryCombo {

private:
    std::filesystem::path m_directory;
    std::string m_name = "";
    std::string m_selectedFile = "";
    std::string m_settingName = "";
    int m_selectedIndex = 0;
public:
    DirectoryCombo(std::string name, std::filesystem::path directory, std::string settingName);
    DirectoryCombo() = default;

    bool draw();
    void setSelectedFilePath(std::filesystem::path path);

    std::string getSelectedFile();
    std::filesystem::path getSelectedFilePath();
};
}
