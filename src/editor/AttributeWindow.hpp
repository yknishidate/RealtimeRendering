#pragma once
#include <imgui.h>

#include "Scene.hpp"

namespace Message {
enum Type {
    None = 0,
    TransformChanged = 1 << 0,
    MaterialChanged = 1 << 1,
    CameraChanged = 1 << 2,
    TextureAdded = 1 << 3,
};
}

class AttributeWindow {
public:
    void init(const rv::Context& _context, Scene& _scene, IconManager& _iconManager) {
        using namespace std::literals::string_literals;
        context = &_context;
        scene = &_scene;
        iconManager = &_iconManager;
        iconManager->addIcon(*context, "asset_texture",
                             (DEV_ASSET_DIR / "icons/asset_texture.png"s).string());
    }

    bool showTransform(Object* object) const {
        Transform* transform = object->get<Transform>();
        if (!transform) {
            return false;
        }

        bool changed = false;
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Transform")) {
            // Translation
            changed |=
                ImGui::DragFloat3("Translation", glm::value_ptr(transform->translation), 0.01f);

            // Rotation
            glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(transform->rotation));
            changed |= ImGui::DragFloat3("Rotation", glm::value_ptr(eulerAngles), 1.0f);
            transform->rotation = glm::quat(glm::radians(eulerAngles));

            // Scale
            changed |= ImGui::DragFloat3("Scale", glm::value_ptr(transform->scale), 0.01f);

            ImGui::TreePop();
        }
        return changed;
    }

    bool showMesh(const Object* object) const {
        const Mesh* mesh = object->get<Mesh>();
        if (!mesh) {
            return false;
        }

        bool changed = false;
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Mesh")) {
            // Mesh
            ImGui::Text(("Mesh: " + mesh->mesh->name).c_str());

            // Material
            Material* material = mesh->material;
            if (!material) {
                ImGui::Text("Material: Empty");
                return false;
            }

            ImGui::Text(("Material: " + material->name).c_str());
            changed |= ImGui::ColorEdit4("Base color", &material->baseColor[0]);

            // Base color texture
            std::string name = "asset_texture";
            if (material->baseColorTextureIndex != -1) {
                name = scene->getTextures()[material->baseColorTextureIndex].name;
            }
            iconManager->showDroppableIcon(
                name, "", 100.0f, ImVec4(0, 0, 0, 1), [] {},
                [&](const char* droppedName) {
                    for (int i = 0; i < scene->getTextures().size(); i++) {
                        Texture& texture = scene->getTextures()[i];
                        if (std::strcmp(texture.name.c_str(), droppedName) == 0) {
                            material->baseColorTextureIndex = i;
                            spdlog::info("[UI] Apply base color texture: {}", droppedName);
                            changed = true;
                        }
                    }
                });

            ImGui::Spacing();

            changed |= ImGui::ColorEdit3("Emissive", &material->emissive[0]);
            changed |= ImGui::SliderFloat("Metallic", &material->metallic, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Roughness", &material->roughness, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("IOR", &material->ior, 0.01f, 5.0f);

            ImGui::TreePop();
        }
        return changed;
    }

    void showDirectionalLight(Object* object) const {
        DirectionalLight* light = object->get<DirectionalLight>();
        if (!light) {
            return;
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Directional light")) {
            ImGui::ColorEdit3("Color", glm::value_ptr(light->color));
            ImGui::DragFloat("Intensity", &light->intensity, 0.001f, 0.0f, 100.0f);
            ImGui::SliderFloat("Phi", &light->phi, -180.0f, 180.0f);
            ImGui::SliderFloat("Theta", &light->theta, -90.0f, 90.0f);
            ImGui::TreePop();
        }
    }

    int show(Object* object) const {
        ImGui::Begin("Attribute");
        int message = Message::None;
        if (object) {
            // Transform
            if (showTransform(object)) {
                spdlog::info("Transform changed");
                message |= Message::TransformChanged;
            }

            // Mesh
            if (showMesh(object)) {
                message |= Message::MaterialChanged;
            }

            showDirectionalLight(object);
        }
        ImGui::End();
        return message;
    }

    const rv::Context* context = nullptr;
    Scene* scene = nullptr;
    IconManager* iconManager = nullptr;
};
