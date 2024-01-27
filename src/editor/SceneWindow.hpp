#pragma once
#include <imgui.h>

#include "Scene.hpp"

class SceneWindow {
public:
    void show(Scene& scene, Object** selectedObject) const {
        ImGui::Begin("Scene");

        for (auto& object : scene.objects) {
            // Set flag
            int flag = ImGuiTreeNodeFlags_OpenOnArrow;
            if (&object == *selectedObject) {
                flag = flag | ImGuiTreeNodeFlags_Selected;
            }

            // Show object
            bool open = ImGui::TreeNodeEx(object.name.c_str(), flag);
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