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

    int show(Object* object) const {
        ImGui::Begin("Attribute");
        int message = Message::None;
        if (object) {
            for (const auto& comp : object->getComponents() | std::views::values) {
                comp->showAttributes();
            }
        }
        ImGui::End();
        return message;
    }

    const rv::Context* context = nullptr;
    Scene* scene = nullptr;
    IconManager* iconManager = nullptr;
};
