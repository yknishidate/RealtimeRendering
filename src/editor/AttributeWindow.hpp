#pragma once
#include <imgui.h>

#include "Scene.hpp"

class AttributeWindow {
public:
    // NOTE: object is nullable
    static void show(const Object* object) {
        if (ImGui::Begin("Attribute")) {
            if (object) {
                for (const auto& comp : object->getComponents() | std::views::values) {
                    comp->showAttributes();
                }
            }
            ImGui::End();
        }
    }
};
