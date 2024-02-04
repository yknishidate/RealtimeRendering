#pragma once
#include <imgui.h>
#include <nfd.h>

#include "IconManager.hpp"
#include "Scene.hpp"

class AssetWindow {
public:
    static void importTexture(const rv::Context& context,
                              Scene& scene,
                              const std::filesystem::path& filepath) {
        Texture texture;
        texture.name = filepath.filename().string();
        texture.filepath = filepath.string();

        std::filesystem::path extension = filepath.extension();
        if (extension == ".jpg" || extension == ".png") {
            spdlog::info("Load image");
            texture.image = rv::Image::loadFromFile(context, texture.filepath);
        } else if (extension == ".hdr") {
            spdlog::info("Load HDR image");
            texture.image = rv::Image::loadFromFileHDR(context, texture.filepath);
        } else if (extension == ".ktx") {
            spdlog::info("Load HDR image");
            texture.image = rv::Image::loadFromKTX(context, texture.filepath);
        }

        if (texture.image->getViewType() == vk::ImageViewType::e2D) {
            scene.addTexture2D(texture);
            IconManager::addIcon(texture.name, texture.image);
        } else if (texture.image->getViewType() == vk::ImageViewType::eCube) {
            scene.addTextureCube(texture);
        } else {
            assert(false);
        }
    }

    static void openImportDialog(const rv::Context& context, Scene& scene) {
        nfdchar_t* outPath = nullptr;
        nfdresult_t result = NFD_OpenDialog("png,jpg,hdr,ktx", nullptr, &outPath);
        if (result == NFD_OKAY) {
            importTexture(context, scene, outPath);
            free(outPath);
        }
    }

    static void show(const rv::Context& context, Scene& scene) {
        if (ImGui::Begin("Asset")) {
            // Show icons
            float padding = 16.0f;
            float thumbnailSize = 100.0f;
            float cellSize = thumbnailSize + padding;
            float panelWidth = ImGui::GetContentRegionAvail().x;
            int columnCount = std::max(static_cast<int>(panelWidth / cellSize), 1);
            ImGui::Columns(columnCount, 0, false);

            for (auto& mesh : scene.getMeshData()) {
                IconManager::showDraggableIcon("asset_mesh", mesh.name, thumbnailSize,
                                               ImVec4(0, 0, 0, 1));
            }

            for (auto& material : scene.getMaterials()) {
                IconManager::showDraggableIcon("asset_material", material.name, thumbnailSize,
                                               ImVec4(0, 0, 0, 1));
            }

            for (auto& texture : scene.getTextures2D()) {
                IconManager::showDraggableIcon(texture.name, texture.name, thumbnailSize,
                                               ImVec4(0, 0, 0, 1));
            }
            for (auto& texture : scene.getTexturesCube()) {
                // TODO: プレビューのサポート
                IconManager::showDraggableIcon("asset_texture", texture.name, thumbnailSize,
                                               ImVec4(0, 0, 0, 1));
            }

            ImGui::Columns(1);

            // Show menu
            if (ImGui::BeginPopupContextWindow("Asset menu")) {
                if (ImGui::MenuItem("Import texture")) {
                    openImportDialog(context, scene);
                }
                ImGui::EndPopup();
            }

            ImGui::End();
        }
    }
};
