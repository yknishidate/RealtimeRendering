#pragma once
#include <imgui.h>

#include "Renderer.hpp"
#include "Scene.hpp"
#include "ViewportRenderer.hpp"
#include "ViewportWindow.hpp"

class MenuBar {
public:
    // TODO:
    static EditorMessage openScene(Scene& scene) {
        NFD::UniquePath outPath;
        nfdfilteritem_t filterItem[1] = {{"Scene", "json,gltf,glb"}};
        nfdresult_t result = NFD::OpenDialog(outPath, filterItem, 1);
        if (result == NFD_OKAY) {
            std::filesystem::path filepath = {outPath.get()};
            if (filepath.extension() == ".gltf" || filepath.extension() == ".glb") {
                scene.loadFromGltf(std::filesystem::path{outPath.get()});
            } else if (filepath.extension() == ".json") {
                scene.loadFromJson(std::filesystem::path{outPath.get()});
            }
            return EditorMessage::SceneOpened;
        }
        return EditorMessage::None;
    }

    static EditorMessage show(Scene& scene) {
        EditorMessage message = EditorMessage::None;
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open..", "Ctrl+O")) {
                    message = openScene(scene);
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
                if (ImGui::BeginMenu("Window")) {
                    if (ImGui::Combo("Size", &windowSizeIndex,
                                     "-\0"
                                     "1280x720\0"
                                     "1920x1080\0"
                                     "2560x1440\0")) {
                        message = EditorMessage::WindowResizeRequested;
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Viewport")) {
                    ImGui::Checkbox("Grid", &ViewportRenderer::isGridVisible);
                    ImGui::Checkbox("Scene AABB", &ViewportRenderer::isSceneAABBVisible);
                    ImGui::Checkbox("Object AABB", &ViewportRenderer::isObjectAABBVisible);
                    ImGui::Checkbox("Light", &ViewportRenderer::isLightVisible);
                    ImGui::Checkbox("Gizmo", &ViewportWindow::isGizmoVisible);
                    ImGui::Checkbox("Tool bar", &ViewportWindow::isToolBarVisible);
                    ImGui::Checkbox("Auxiliary image", &ViewportWindow::isAuxiliaryImageVisible);
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Renderer")) {
                    ImGui::Checkbox("FXAA", &Renderer::enableFXAA);
                    ImGui::Checkbox("SSR", &Renderer::enableSSR);
                    if (Renderer::enableSSR) {
                        ImGui::DragFloat("SSR intensity", &Renderer::ssrIntensity, 0.01f);
                    }
                    ImGui::Checkbox("Frustum culling", &Renderer::enableFrustumCulling);
                    ImGui::Checkbox("Sorting", &Renderer::enableSorting);
                    ImGui::DragFloat("Exposure", &Renderer::exposure, 0.01f, 0.0f);
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }
        return message;
    }

    static uint32_t getWindowWidth() {
        return windowSizes[windowSizeIndex].first;
    }

    static uint32_t getWindowHeight() {
        return windowSizes[windowSizeIndex].second;
    }

    inline static std::vector<std::pair<uint32_t, uint32_t>> windowSizes = {
        {0, 0},
        {1280, 720},
        {1920, 1080},
        {2560, 1440},
    };

    inline static int windowSizeIndex = 0;
};
