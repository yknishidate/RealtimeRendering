#pragma once
#include <ImGuizmo.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <glm/gtx/matrix_decompose.hpp>

#include <reactive/reactive.hpp>

#include "IconManager.hpp"
#include "Scene.hpp"

class ViewportWindow {
public:
    static bool editTransform(const rv::Camera& camera, glm::mat4& matrix) {
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();

        ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y,  // break
                          ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

        glm::mat4 cameraProjection = camera.getProj();
        glm::mat4 cameraView = camera.getView();
        return ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection),
                                    currentGizmoOperation, ImGuizmo::LOCAL, glm::value_ptr(matrix));
    }

    static bool processMouseInput() {
        bool changed = false;
        if (ImGui::IsWindowFocused() && !ImGuizmo::IsUsing()) {
            dragDelta.x = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x;
            dragDelta.y = -ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y;
            if (dragDelta.x != 0.0f || dragDelta.y != 0.0f) {
                changed = true;
            }
        }
        if (!ImGuizmo::IsUsing()) {
            mouseScroll = ImGui::GetIO().MouseWheel;
            if (mouseScroll != 0.0f) {
                changed = true;
            }
        }
        ImGui::ResetMouseDragDelta();
        return changed;
    }

    static bool showGizmo(Scene& scene, Object* selectedObject) {
        // TODO: support animation

        if (!selectedObject) {
            return false;
        }
        Transform* transform = selectedObject->get<Transform>();
        if (!transform) {
            return false;
        }

        bool changed = false;
        glm::mat4 model = transform->computeTransformMatrix();
        changed |= editTransform(scene.getCamera(), model);

        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(model, transform->scale, transform->rotation, transform->translation, skew,
                       perspective);
        return changed;
    }

    static void showToolIcon(const std::string& name,
                             float thumbnailSize,
                             ImGuizmo::OPERATION operation) {
        ImVec4 bgColor = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
        if (currentGizmoOperation == operation || IconManager::isHover(thumbnailSize)) {
            bgColor = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
        }
        IconManager::showIcon(name, "", thumbnailSize, bgColor,
                              [&]() { currentGizmoOperation = operation; });
    }

    static void showToolBar(ImVec2 viewportPos) {
        ImGui::SetCursorScreenPos(ImVec2(viewportPos.x + 10, viewportPos.y + 15));
        ImGui::BeginChild("Toolbar", ImVec2(180, 60), false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration);

        float padding = 1.0f;
        float thumbnailSize = 50.0f;
        float cellSize = thumbnailSize + padding;
        float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount = static_cast<int>(panelWidth / cellSize);
        ImGui::Columns(columnCount, 0, false);

        showToolIcon("manip_translate", thumbnailSize, ImGuizmo::TRANSLATE);
        showToolIcon("manip_rotate", thumbnailSize, ImGuizmo::ROTATE);
        showToolIcon("manip_scale", thumbnailSize, ImGuizmo::SCALE);

        ImGui::Columns(1);
        ImGui::EndChild();
    }

    static void showAuxiliaryImage(ImVec2 viewportPos) {
        if (!auxiliaryDescSet) {
            return;
        }

        float imageWidth = 300.0f;
        float imageHeight = 300.0f / auxiliaryAspect;
        float padding = 10.0f;
        float cursorX = viewportPos.x + width - imageWidth - padding;
        float cursorY = viewportPos.y + height - imageHeight - padding;
        ImGui::SetCursorScreenPos(ImVec2(cursorX, cursorY));

        ImGui::Image(auxiliaryDescSet, ImVec2(imageWidth, imageHeight));
    }

    static void show(Scene& scene, vk::DescriptorSet image, Object* selectedObject) {
        // TODO: support animation
        if (ImGui::Begin("Viewport")) {
            processMouseInput();

            // Viewport
            ImVec2 windowPos = ImGui::GetCursorScreenPos();
            ImVec2 windowSize = ImGui::GetContentRegionAvail();
            width = windowSize.x;
            height = windowSize.y;
            ImGui::Image(image, windowSize);

            if (isAuxiliaryImageVisible) {
                showAuxiliaryImage(windowPos);
            }

            if (isToolBarVisible) {
                showToolBar(windowPos);
            }

            if (isGizmoVisible) {
                showGizmo(scene, selectedObject);
            }

            ImGui::End();
        }
    }

    static void setAuxiliaryImage(const rv::ImageHandle& image) {
        ImGui_ImplVulkan_RemoveTexture(auxiliaryDescSet);
        auxiliaryDescSet = ImGui_ImplVulkan_AddTexture(  //
            image->getSampler(), image->getView(), static_cast<VkImageLayout>(image->getLayout()));
        auxiliaryAspect = static_cast<float>(image->getExtent().width) /
                          static_cast<float>(image->getExtent().height);
    }

    // Options
    inline static bool isAuxiliaryImageVisible = true;
    inline static bool isToolBarVisible = true;
    inline static bool isGizmoVisible = true;

    // Input
    inline static glm::vec2 dragDelta = {0.0f, 0.0f};
    inline static float mouseScroll = 0.0f;

    // Image
    inline static float width = 0.0f;
    inline static float height = 0.0f;

    inline static vk::DescriptorSet auxiliaryDescSet;
    inline static float auxiliaryAspect;

    // Gizmo
    inline static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
};
