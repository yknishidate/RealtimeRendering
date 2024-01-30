#pragma once
#include <imgui.h>

#include "Scene.hpp"

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
