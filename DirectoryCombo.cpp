#include "DirectoryCombo.h"
#include "Settings.hpp"
#include "GUI.h"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace GUI
{
DirectoryCombo::DirectoryCombo(std::string name, ghc::filesystem::path directory, std::string settingName)
{
    m_directory = directory;
    m_name = name;
    m_settingName = settingName;

    ghc::filesystem::path targetPath = Settings::get<std::string>(m_settingName, "");

    std::vector<std::string> files;
    for (const auto& entry : ghc::filesystem::directory_iterator(m_directory))
        files.push_back(string::wideToUtf8(entry.path().filename().wstring()));

    int i = 0;
    for (const auto& entry : ghc::filesystem::directory_iterator(m_directory))
    {
        if(entry.path() == targetPath)
            m_selectedIndex = i;
        
        i++;
    }

    m_selectedFile = files.at(m_selectedIndex);
}

bool DirectoryCombo::draw()
{
    if(!GUI::shouldRender())
        return false;

    std::vector<std::string> files;
    for (const auto& entry : ghc::filesystem::directory_iterator(m_directory))
        files.push_back(string::wideToUtf8(entry.path().filename().wstring()));

    if(ImGui::Combo(m_name.c_str(), &m_selectedIndex, [](void* vec, int idx, const char** out_text)
    {
		std::vector<std::string>* vector = reinterpret_cast<std::vector<std::string>*>(vec);
		if (idx < 0 || idx >= vector->size())
			return false;
		*out_text = vector->at(idx).c_str();
		return true;
	}, reinterpret_cast<void*>(&files), files.size()))
    {
        m_selectedFile = files.at(m_selectedIndex);
        Mod::get()->setSavedValue<std::string>(m_settingName, string::wideToUtf8(getSelectedFilePath().wstring()));
        return true;
    }

    return false;
}

std::string DirectoryCombo::getSelectedFile()
{
    return m_selectedFile;
}

ghc::filesystem::path DirectoryCombo::getSelectedFilePath()
{
    return m_directory / m_selectedFile;
}


void DirectoryCombo::setSelectedFilePath(ghc::filesystem::path path)
{
    int i = 0;
    for (const auto& entry : ghc::filesystem::directory_iterator(m_directory))
    {
        if(entry.path() == path)
        {
            m_selectedIndex = i;
            m_selectedFile = string::wideToUtf8(entry.path().filename().wstring());
            Mod::get()->setSavedValue<std::string>(m_settingName, string::wideToUtf8(getSelectedFilePath().wstring()));
            Mod::get()->saveData();
        }
        
        i++;
    }
}

}