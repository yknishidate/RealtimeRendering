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
    static int show(const Object* object) {
        int message = Message::None;

        if (ImGui::Begin("Attribute")) {
            if (object) {
                for (const auto& comp : object->getComponents() | std::views::values) {
                    comp->showAttributes();
                }
            }
            ImGui::End();
        }
        return message;
    }
};
