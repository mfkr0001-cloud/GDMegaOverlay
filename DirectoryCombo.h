#pragma once

#include "Geode/utils/file.hpp"

namespace GUI
{
class DirectoryCombo {

private:
    ghc::filesystem::path m_directory;
    std::string m_name = "";
    std::string m_selectedFile = "";
    std::string m_settingName = "";
    int m_selectedIndex = 0;
public:
    DirectoryCombo(std::string name, ghc::filesystem::path directory, std::string settingName);
    DirectoryCombo() = default;

    bool draw();
    void setSelectedFilePath(ghc::filesystem::path path);

    std::string getSelectedFile();
    ghc::filesystem::path getSelectedFilePath();
};
}