#pragma once
#include <imgui.h>
#include <nfd.h>

#include "IconManager.hpp"
#include "Scene.hpp"

class AssetWindow {
public:
    void init(const rv::Context& _context, Scene& _scene) {
        context = &_context;
        scene = &_scene;
    }

    void importTexture(const char* filepath) const {
        Texture texture;
        texture.name = "Texture " + std::to_string(scene->getTextures().size());
        texture.filepath = filepath;
        std::filesystem::path extension = std::filesystem::path{filepath}.extension();
        if (extension == ".jpg" || extension == ".png") {
            spdlog::info("Load image");
            texture.image = rv::Image::loadFromFile(*context, texture.filepath);
        } else if (extension == ".hdr") {
            spdlog::info("Load HDR image");
            texture.image = rv::Image::loadFromFileHDR(*context, texture.filepath);
        }
        scene->getTextures().push_back(texture);
        IconManager::addIcon(texture.name, texture.image);
    }

    void openImportDialog() const {
        nfdchar_t* outPath = NULL;
        nfdresult_t result = NFD_OpenDialog("png,jpg,hdr", NULL, &outPath);
        if (result == NFD_OKAY) {
            importTexture(outPath);
            free(outPath);
        }
    }

    void show() const {
        if (ImGui::Begin("Asset")) {
            // Show icons
            float padding = 16.0f;
            float thumbnailSize = 100.0f;
            float cellSize = thumbnailSize + padding;
            float panelWidth = ImGui::GetContentRegionAvail().x;
            int columnCount = std::max(static_cast<int>(panelWidth / cellSize), 1);
            ImGui::Columns(columnCount, 0, false);

            for (auto& mesh : scene->getMeshes()) {
                IconManager::showDraggableIcon("asset_mesh", mesh.name, thumbnailSize,
                                               ImVec4(0, 0, 0, 1));
            }

            for (auto& material : scene->getMaterials()) {
                IconManager::showDraggableIcon("asset_material", material.name, thumbnailSize,
                                               ImVec4(0, 0, 0, 1));
            }
            for (auto& texture : scene->getTextures()) {
                IconManager::showDraggableIcon(texture.name, texture.name, thumbnailSize,
                                               ImVec4(0, 0, 0, 1));
            }

            ImGui::Columns(1);

            // Show menu
            if (ImGui::BeginPopupContextWindow("Asset menu")) {
                if (ImGui::MenuItem("Import texture")) {
                    openImportDialog();
                }
                ImGui::EndPopup();
            }

            ImGui::End();
        }
    }

    const rv::Context* context = nullptr;
    Scene* scene = nullptr;
};
