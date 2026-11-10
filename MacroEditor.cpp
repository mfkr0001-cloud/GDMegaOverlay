#include "MacroEditor.h"
#include "GUI/GUI.h"

#include "Macrobot/Macrobot.h"

const char* const buttons[] = {"JUMP", "LEFT", "RIGHT"};

void MacroEditor::renderWindow()
{
    static int selected = -1;

    if(ImGui::Button("Sort"))
    {
        std::sort(Macrobot::macro.inputs.begin(), Macrobot::macro.inputs.end(), [](const Macrobot::Action& a, const Macrobot::Action& b) { return a.frame < b.frame; });
        selected = -1;
    }

    ImVec2 child_size = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("##ScrollingRegion", {child_size.x * 0.25f, child_size.y}, true);

    ImGuiListClipper clipper;
	clipper.Begin(Macrobot::macro.inputs.size());

	while (clipper.Step())
	{
		for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
		{
            auto frame = &Macrobot::macro.inputs[row];
            if(ImGui::Selectable((std::to_string(frame->frame) + "##" + std::to_string(row)).c_str(), selected == row))
                selected = row;
	    }
	}

    ImGui::EndChild();


    ImGui::SameLine();


    ImGui::BeginChild("##Editor", {child_size.x * 0.74f, child_size.y}, true);

    if(selected == -1)
    {
        ImGui::Text("Select an input to edit it");
        ImGui::EndChild();
        return;
    }

    Macrobot::Action&input = Macrobot::macro.inputs[selected];

    if(ImGui::InputScalar("Frame", ImGuiDataType_U32, &input.frame) && input.correction)
        input.correction->frame = input.frame;
    
    ImGui::Combo("Button", &input.button, buttons, IM_ARRAYSIZE(buttons));

    if(GUI::checkbox("Player 2", &input.player2) && input.correction)
        input.correction->player2 = input.player2;
    
    GUI::checkbox("Press", &input.down);

    if(!input.correction)
    {
        ImGui::EndChild();
        return;
    }

    ImGui::InputScalar("X Velocity", ImGuiDataType_Double, &input.correction->checkpoint.xVel);
    ImGui::InputScalar("Y Velocity", ImGuiDataType_Double, &input.correction->checkpoint.yVel);

    ImGui::InputFloat("X Position", &input.correction->checkpoint.xPos);
    ImGui::InputFloat("Y Position", &input.correction->checkpoint.yPos);

    ImGui::InputFloat("Rotation", &input.correction->checkpoint.rotation);
    ImGui::InputFloat("Rotation Rate", &input.correction->checkpoint.rotationRate);

    ImGui::EndChild();
}