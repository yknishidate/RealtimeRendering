#include "Object.hpp"

#include "Scene.hpp"

bool AmbientLight::showAttributes(Scene& scene) {
    bool changed = false;
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Ambient light")) {
        changed |= ImGui::ColorEdit3("Color", glm::value_ptr(color));
        changed |= ImGui::DragFloat("Intensity", &intensity, 0.001f, 0.0f, 100.0f);

        std::stringstream ss;
        for (auto& tex : scene.getTexturesCube()) {
            ss << tex.name << '\0';
        }
        ImGui::Combo("Texture", &textureCube, ss.str().c_str());
        ImGui::TreePop();
    }
    return changed;
}
