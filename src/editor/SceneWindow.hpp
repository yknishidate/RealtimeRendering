#pragma once
#include <imgui.h>

#include "Scene.hpp"

class SceneWindow {
public:
    static void show(Scene& scene, Object** selectedObject) {
        ImGui::Begin("Scene");

        for (auto& object : scene.getObjects()) {
            // Set flag
            int flag = ImGuiTreeNodeFlags_OpenOnArrow;
            if (&object == *selectedObject) {
                flag = flag | ImGuiTreeNodeFlags_Selected;
            }

            // Show object
            bool open = ImGui::TreeNodeEx(object.getName().c_str(), flag);
            if (ImGui::IsItemClicked()) {
                *selectedObject = &object;
            }
            if (open) {
                ImGui::TreePop();
            }
        }

        ImGui::End();
    }
};
