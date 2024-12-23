#pragma once
#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <utility>

#include "reactive/Graphics/Context.hpp"
#include "reactive/Graphics/Image.hpp"

class IconManager {
public:
    static void loadIcons(const rv::Context& context) {
        // Manipulate
        addIcon(context, "manip_translate", (DEV_ASSET_DIR / "icons/manip_translate.png").string());
        addIcon(context, "manip_rotate", (DEV_ASSET_DIR / "icons/manip_rotate.png").string());
        addIcon(context, "manip_scale", (DEV_ASSET_DIR / "icons/manip_scale.png").string());

        // Asset
        addIcon(context, "asset_mesh", (DEV_ASSET_DIR / "icons/asset_mesh.png").string());
        addIcon(context, "asset_material", (DEV_ASSET_DIR / "icons/asset_material.png").string());
        addIcon(context, "asset_texture", (DEV_ASSET_DIR / "icons/asset_texture.png").string());
    }

    static void clearIcons() {
        icons.clear();
    }

    static bool isHover(float thumbnailSize) {
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 buttonMin = ImGui::GetCursorScreenPos();
        ImVec2 buttonMax = ImVec2(buttonMin.x + thumbnailSize, buttonMin.y + thumbnailSize);
        return mousePos.x >= buttonMin.x && mousePos.y >= buttonMin.y &&
               mousePos.x <= buttonMax.x && mousePos.y <= buttonMax.y;
    }

    static ImVec2 getImageSize(const std::string& iconName) {
        vk::Extent3D extent = icons[iconName].image->getExtent();
        return {static_cast<float>(extent.width), static_cast<float>(extent.height)};
    }

    static std::pair<ImVec2, ImVec2> computeCenterCroppedUVs(ImVec2 imageSize) {
        float aspectRatioImage = imageSize.x / imageSize.y;
        float aspectRatioButton = 1.0f;

        ImVec2 uv0 = ImVec2(0, 0);
        ImVec2 uv1 = ImVec2(1, 1);

        if (aspectRatioImage > aspectRatioButton) {
            float offset = (1.0f - aspectRatioButton / aspectRatioImage) * 0.5f;
            uv0.x += offset;
            uv1.x -= offset;
        } else {
            float offset = (1.0f - aspectRatioImage / aspectRatioButton) * 0.5f;
            uv0.y += offset;
            uv1.y -= offset;
        }
        return {uv0, uv1};
    }

    static void showIcon(
        const std::string& iconName,
        const std::string& itemName,
        float thumbnailSize,
        ImVec4 bgColor,
        const std::function<void()>& callback = [] {}) {
        auto [uv0, uv1] = computeCenterCroppedUVs(getImageSize(iconName));
        ImTextureID textureId = (ImTextureID)(VkDescriptorSet)icons[iconName].descSet;
        ImGui::PushID(itemName.c_str());
        if (ImGui::ImageButton(iconName.c_str(), textureId, {thumbnailSize, thumbnailSize}, uv0, uv1, bgColor)) {
            callback();
        }
        ImGui::PopID();

        if (!itemName.empty()) {
            ImGui::TextWrapped(itemName.c_str());
        }
        ImGui::NextColumn();
    }

    static void showDraggableIcon(
        const std::string& iconName,
        const std::string& itemName,
        float thumbnailSize,
        ImVec4 bgColor,
        const std::function<void()>& callback = [] {}) {
        auto [uv0, uv1] = computeCenterCroppedUVs(getImageSize(iconName));
        ImTextureID textureId = (ImTextureID)(VkDescriptorSet)icons[iconName].descSet;
        ImGui::PushID(itemName.c_str());
        if (ImGui::ImageButton(iconName.c_str(), textureId, {thumbnailSize, thumbnailSize}, uv0, uv1, bgColor)) {
            callback();
        }

        // Draggable
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("StringType", itemName.c_str(), itemName.size() + 1);
            ImGui::Text(itemName.c_str());
            ImGui::EndDragDropSource();
        }
        ImGui::PopID();

        if (!itemName.empty()) {
            ImGui::TextWrapped(itemName.c_str());
        }
        ImGui::NextColumn();
    }

    static void showDroppableIcon(
        const std::string& iconName,
        const std::string& itemName,
        float thumbnailSize,
        ImVec4 bgColor,
        const std::function<void()>& callback = [] {},
        const std::function<void(const char*)>& dropCallback = [](const char*) {}) {
        auto [uv0, uv1] = computeCenterCroppedUVs(getImageSize(iconName));
        ImTextureID textureId = (ImTextureID)(VkDescriptorSet)icons[iconName].descSet;
        ImGui::PushID(itemName.c_str());
        if (ImGui::ImageButton(iconName.c_str(), textureId, {thumbnailSize, thumbnailSize}, uv0, uv1, bgColor)) {
            callback();
        }

        // Droppable
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("StringType")) {
                dropCallback(static_cast<const char*>(payload->Data));
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::PopID();

        if (!itemName.empty()) {
            ImGui::TextWrapped(itemName.c_str());
        }
        ImGui::NextColumn();
    }

    // With loading. For UI icon
    static void addIcon(const rv::Context& context,
                        const std::string& name,
                        const std::string& filepath) {
        icons[name].image = rv::Image::loadFromFile(context, filepath);
        icons[name].descSet = ImGui_ImplVulkan_AddTexture(  //
            icons[name].image->getSampler(), icons[name].image->getView(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // Without loading. For texture
    static void addIcon(const std::string& name, rv::ImageHandle texture) {
        // WARN: this image has ownership
        icons[name].image = std::move(texture);
        icons[name].descSet = ImGui_ImplVulkan_AddTexture(  //
            icons[name].image->getSampler(), icons[name].image->getView(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    struct IconData {
        rv::ImageHandle image;
        vk::DescriptorSet descSet;
    };

    inline static std::unordered_map<std::string, IconData> icons;
};
