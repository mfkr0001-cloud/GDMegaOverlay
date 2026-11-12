#include "JsonPatches.h"
#include "../GUI/GUI.h"
#include "../util.hpp"
#include "../Settings.hpp"
#include <fstream>
#include <imgui.h>

#include <Geode/Geode.hpp>

using namespace geode::prelude;

void JsonPatches::init()
{
	// Load the patch definitions from their actual resource directory. On Android
	// the definitions are still loaded so their controls remain visible, but the
	// original opcode addresses are desktop-only and must never be applied there.
	auto load_group = [](const std::string& relativePath, JsonPatches::patch_group_type& group)
	{
		const auto filePath = Mod::get()->getResourcesDir() / relativePath;
		std::ifstream file(filePath);
		if (!file.is_open()) {
			log::error("GDMegaOverlay: unable to open patch definitions: {}", filePath.string());
			return;
		}

		nlohmann::json document;
		try {
			file >> document;
		}
		catch (const nlohmann::json::exception& error) {
			log::error("GDMegaOverlay: invalid patch definitions in {}: {}", relativePath, error.what());
			return;
		}

		if (!document.is_object() || !document.contains("mods") || !document["mods"].is_array()) {
			log::error("GDMegaOverlay: patch definitions have no mods array: {}", relativePath);
			return;
		}

		for (const auto& item : document["mods"]) {
			if (!item.is_object() || !item.contains("name") || !item["name"].is_string())
				continue;

			JPatch patch{};
			patch.description = item.value("description", std::string{});

#if !defined(__ANDROID__)
			if (item.contains("opcodes") && item["opcodes"].is_array()) {
				for (const auto& opcode : item["opcodes"]) {
					if (!opcode.is_object() || !opcode.contains("address") || !opcode.contains("on"))
						continue;
					try {
						const auto address = std::stoul(opcode["address"].get<std::string>(), nullptr, 16);
						const auto bytes = util::hexToBytes(opcode["on"].get<std::string>());
						auto result = Mod::get()->patch(reinterpret_cast<void*>(base::get() + address), bytes);
						if (result.isOk()) {
							auto* nativePatch = result.unwrap();
							nativePatch->disable();
							patch.patches.push_back(nativePatch);
						}
						else {
							log::error("GDMegaOverlay: failed to prepare patch '{}': {}", item["name"].get<std::string>(), result.err());
						}
					}
					catch (const std::exception& error) {
						log::error("GDMegaOverlay: invalid opcode in '{}': {}", item["name"].get<std::string>(), error.what());
					}
				}
			}
#endif
			const std::string name = item["name"].get<std::string>();
			group.emplace(name, std::move(patch));
#if !defined(__ANDROID__)
			JsonPatches::togglePatch(group, name);
#endif
		}
	};

	load_group("hacks/bypass.json", JsonPatches::bypass);
	load_group("hacks/creator.json", JsonPatches::creator);
	load_group("hacks/global.json", JsonPatches::global);
	load_group("hacks/level.json", JsonPatches::level);
	load_group("hacks/player.json", JsonPatches::player);
}

void JsonPatches::togglePatch(JsonPatches::patch_group_type& patchGroup, const std::string& name)
{
	auto found = patchGroup.find(name);
	if (found == patchGroup.end() || found->second.patches.empty())
		return;
	auto patches = found->second.patches;

	if (Settings::get<bool>("patch/" + name, false))
	{
		for (const auto& patch : patches)
			if (!patch->isEnabled())
				patch->enable();
	}
	else
	{
		for (const auto& patch : patches)
			if (patch->isEnabled())
				patch->disable();
	}
}

void JsonPatches::togglePatch(JsonPatches::patch_group_type& patchGroup, const std::string& name, bool value)
{
	Mod::get()->setSavedValue<bool>("patch/" + name, value);

	togglePatch(patchGroup, name);
}

void JsonPatches::drawFromPatches(JsonPatches::patch_group_type& patchGroup)
{
	for (const auto& [name, jpatch] : patchGroup)
	{
#if defined(__ANDROID__)
		// Show the real patch entries, but do not present desktop opcode patches
		// as working Android toggles. Their addresses are not valid on Android.
		ImGui::BeginDisabled();
		GUI::checkbox(name, "patch/" + name, false);
		ImGui::EndDisabled();
		GUI::tooltip(jpatch.description + "\nUnavailable on Android: this option uses desktop-specific memory addresses.");
#else
		if (GUI::checkbox(name, "patch/" + name, false))
			togglePatch(patchGroup, name);
		GUI::tooltip(jpatch.description);
#endif
	}
}
