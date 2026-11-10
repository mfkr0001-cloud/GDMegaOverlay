#include <filesystem>
#include "DirectoryCombo.h"
#include "Settings.hpp"
#include "GUI.h"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace GUI
{
DirectoryCombo::DirectoryCombo(std::string name, std::filesystem::path directory, std::string settingName)
{
    m_directory = directory;
    m_name = name;
    m_settingName = settingName;

    std::filesystem::path targetPath = Settings::get<std::string>(m_settingName, "");

    std::vector<std::string> files;
    for (const auto& entry : std::filesystem::directory_iterator(m_directory))
        files.push_back(entry.path().filename().string());

    int i = 0;
    for (const auto& entry : std::filesystem::directory_iterator(m_directory))
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
    for (const auto& entry : std::filesystem::directory_iterator(m_directory))
        files.push_back(entry.path().filename().string());

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
        Mod::get()->setSavedValue<std::string>(m_settingName, getSelectedFilePath().string());
        return true;
    }

    return false;
}

std::string DirectoryCombo::getSelectedFile()
{
    return m_selectedFile;
}

std::filesystem::path DirectoryCombo::getSelectedFilePath()
{
    return m_directory / m_selectedFile;
}


void DirectoryCombo::setSelectedFilePath(std::filesystem::path path)
{
    int i = 0;
    for (const auto& entry : std::filesystem::directory_iterator(m_directory))
    {
        if(entry.path() == path)
        {
            m_selectedIndex = i;
            m_selectedFile = entry.path().filename().string();
            Mod::get()->setSavedValue<std::string>(m_settingName, getSelectedFilePath().string());
            Mod::get()->saveData();
        }
        
        i++;
    }
}

}
