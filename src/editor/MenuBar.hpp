#pragma once
#include <imgui.h>

#include "Scene.hpp"

class MenuBar {
public:
    void openScene(Scene& scene) const {
        nfdchar_t* outPath = nullptr;
        nfdresult_t result = NFD_OpenDialog("json", nullptr, &outPath);
        if (result == NFD_OKAY) {
            scene.loadFromJson(std::filesystem::path{outPath});
            free(outPath);
        }
    }

    void show(Scene& scene, bool* isWidgetsVisible) const {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open..", "Ctrl+O")) {
                    openScene(scene);
                }
                if (ImGui::MenuItem("Save", "Ctrl+S")) { /* Do stuff */
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Create")) {
                if (ImGui::BeginMenu("Light")) {
                    if (ImGui::MenuItem("Directional light")) {
                        if (!scene.findObject<DirectionalLight>()) {
                            scene.addObject("Directional light").add<DirectionalLight>();
                        }
                    }
                    if (ImGui::MenuItem("Point light")) {
                        scene.addObject("Point light").add<PointLight>();
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Option")) {
                ImGui::Checkbox("Viewport widgets", isWidgetsVisible);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
    }
};
