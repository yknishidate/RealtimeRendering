#pragma once
#include <imgui.h>

#include "Renderer.hpp"
#include "Scene.hpp"
#include "ViewportRenderer.hpp"
#include "ViewportWindow.hpp"

class MenuBar {
public:
    // TODO:
    static void openScene(Scene& scene) {
        nfdchar_t* outPath = nullptr;
        nfdresult_t result = NFD_OpenDialog("json,gltf", nullptr, &outPath);
        if (result == NFD_OKAY) {
            std::filesystem::path filepath = {outPath};
            if (filepath.extension() == ".gltf") {
                scene.loadFromGltf(std::filesystem::path{outPath});
            } else if (filepath.extension() == ".json") {
                scene.loadFromJson(std::filesystem::path{outPath});
            }
            free(outPath);
        }
    }

    static void show(Scene& scene) {
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
                    if (ImGui::MenuItem("Ambient light")) {
                        if (!scene.findObject<AmbientLight>()) {
                            scene.addObject("Ambient light").add<AmbientLight>();
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
                if (ImGui::BeginMenu("Viewport")) {
                    ImGui::Checkbox("Grid", &ViewportRenderer::isGridVisible);
                    ImGui::Checkbox("Scene AABB", &ViewportRenderer::isSceneAABBVisible);
                    ImGui::Checkbox("Object AABB", &ViewportRenderer::isObjectAABBVisible);
                    ImGui::Checkbox("Gizmo", &ViewportWindow::isGizmoVisible);
                    ImGui::Checkbox("Tool bar", &ViewportWindow::isToolBarVisible);
                    ImGui::Checkbox("Auxiliary image", &ViewportWindow::isAuxiliaryImageVisible);
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Renderer")) {
                    ImGui::Checkbox("FXAA", &Renderer::enableFXAA);
                    ImGui::DragFloat("Exposure", &Renderer::exposure, 0.01f, 0.0f);
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }
    }
};
