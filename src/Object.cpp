#include "Object.hpp"

#include "Scene.hpp"

glm::mat4 Transform::computeTransformMatrix() const {
    glm::mat4 T = glm::translate(glm::mat4{1.0}, translation);
    glm::mat4 R = glm::mat4_cast(rotation);
    glm::mat4 S = glm::scale(glm::mat4{1.0}, scale);
    return T * R * S;
}

glm::mat4 Transform::computeNormalMatrix() const {
    glm::mat4 R = glm::mat4_cast(rotation);
    glm::mat4 S = glm::scale(glm::mat4{1.0}, glm::vec3{1.0} / scale);
    return R * S;
}

bool Transform::showAttributes(Scene& scene) {
    bool changed = false;
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Transform")) {
        // Translation
        changed |= ImGui::DragFloat3("Translation", glm::value_ptr(translation), 0.01f);

        // Rotation
        glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(rotation));
        changed |= ImGui::DragFloat3("Rotation", glm::value_ptr(eulerAngles), 1.0f);
        rotation = glm::quat(glm::radians(eulerAngles));

        // Scale
        changed |= ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.01f);

        ImGui::TreePop();
    }
    return changed;
}

glm::vec3 DirectionalLight::getDirection() const {
    float _phi = glm::radians(phi);
    float _theta = glm::radians(theta);
    float x = sin(_theta) * sin(_phi);
    float y = cos(_theta);
    float z = sin(_theta) * cos(_phi);
    return {x, y, z};
}

bool DirectionalLight::showAttributes(Scene& scene) {
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    bool changed = false;
    if (ImGui::TreeNode("Directional light")) {
        changed |= ImGui::ColorEdit3("Color", glm::value_ptr(color));
        changed |= ImGui::DragFloat("Intensity", &intensity, 0.001f, 0.0f, 100.0f);
        changed |= ImGui::SliderFloat("Phi", &phi, -180.0f, 180.0f);
        changed |= ImGui::SliderFloat("Theta", &theta, -90.0f, 90.0f);

        changed |= ImGui::Checkbox("Shadow", &enableShadow);
        if (enableShadow) {
            changed |= ImGui::Checkbox("Frontface culling", &enableShadowCulling);
            changed |= ImGui::SliderFloat("Shadow bias", &shadowBias, 0.0f, 0.01f);
        }

        ImGui::TreePop();
    }
    return changed;
}

bool PointLight::showAttributes(Scene& scene) {
    bool changed = false;
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Point light")) {
        changed |= ImGui::ColorEdit3("Color", glm::value_ptr(color));
        changed |= ImGui::DragFloat("Intensity", &intensity, 0.001f, 0.0f, 100.0f);
        changed |= ImGui::DragFloat("Radius", &radius, 0.001f, 0.0f, 100.0f);
        ImGui::TreePop();
    }
    return changed;
}

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
        ImGui::Combo("Radiance texture", &radianceTexture, ss.str().c_str());
        ImGui::Combo("Irradiance texture", &irradianceTexture, ss.str().c_str());
        ImGui::TreePop();
    }
    return changed;
}

MeshData::MeshData(const rv::Context& context, MeshType type) {
    if (type == MeshType::Cube) {
        // Y-up, Right-hand
        glm::vec3 v0 = {-1, -1, -1};
        glm::vec3 v1 = {+1, -1, -1};
        glm::vec3 v2 = {-1, +1, -1};
        glm::vec3 v3 = {+1, +1, -1};
        glm::vec3 v4 = {-1, -1, +1};
        glm::vec3 v5 = {+1, -1, +1};
        glm::vec3 v6 = {-1, +1, +1};
        glm::vec3 v7 = {+1, +1, +1};

        glm::vec3 pX = {+1, 0, 0};
        glm::vec3 nX = {-1, 0, 0};
        glm::vec3 pY = {0, +1, 0};
        glm::vec3 nY = {0, -1, 0};
        glm::vec3 pZ = {0, 0, +1};
        glm::vec3 nZ = {0, 0, -1};
        //       2           3
        //       +-----------+
        //      /|          /|
        //    /  |        /  |
        //  6+---+-------+7  |
        //   |  0+-------+---+1
        //   |  /        |  /
        //   |/          |/
        //  4+-----------+5

        vertices = {
            {v0, nZ}, {v2, nZ}, {v1, nZ},  // Back
            {v3, nZ}, {v1, nZ}, {v2, nZ},  // Back
            {v4, pZ}, {v5, pZ}, {v6, pZ},  // Front
            {v7, pZ}, {v6, pZ}, {v5, pZ},  // Front
            {v6, pY}, {v7, pY}, {v2, pY},  // Top
            {v3, pY}, {v2, pY}, {v7, pY},  // Top
            {v0, nY}, {v1, nY}, {v4, nY},  // Bottom
            {v5, nY}, {v4, nY}, {v1, nY},  // Bottom
            {v5, pX}, {v1, pX}, {v7, pX},  // Right
            {v3, pX}, {v7, pX}, {v1, pX},  // Right
            {v0, nX}, {v4, nX}, {v2, nX},  // Left
            {v6, nX}, {v2, nX}, {v4, nX},  // Left
        };

        for (int i = 0; i < vertices.size(); i++) {
            indices.push_back(i);
        }
    } else if (type == MeshType::Plane) {
        vertices = {
            {glm::vec3{-1.0f, 0.0f, -1.0f}, glm::vec3{0.0f, 1.0f, 0.0f}, glm::vec2{0.0f, 0.0f}},
            {glm::vec3{+1.0f, 0.0f, -1.0f}, glm::vec3{0.0f, 1.0f, 0.0f}, glm::vec2{1.0f, 0.0f}},
            {glm::vec3{-1.0f, 0.0f, +1.0f}, glm::vec3{0.0f, 1.0f, 0.0f}, glm::vec2{0.0f, 1.0f}},
            {glm::vec3{+1.0f, 0.0f, +1.0f}, glm::vec3{0.0f, 1.0f, 0.0f}, glm::vec2{1.0f, 1.0f}},
        };
        indices = {0, 2, 1, 3, 1, 2};
    }
    primitives.push_back({0, static_cast<uint32_t>(indices.size()),
                          static_cast<uint32_t>(vertices.size()), nullptr});
    createBuffers(context);
}

MeshData::MeshData(const rv::Context& context,
                   std::vector<rv::Vertex> _vertices,
                   std::vector<uint32_t> _indices,
                   std::vector<Primitive> _primitives,
                   std::string _name)
    : vertices{std::move(_vertices)},
      indices{std::move(_indices)},
      primitives{std::move(_primitives)},
      name{std::move(_name)} {
    createBuffers(context);
}

void MeshData::createBuffers(const rv::Context& context) {
    vertexBuffer = context.createBuffer({
        .usage = rv::BufferUsage::Vertex,
        .memory = rv::MemoryUsage::Device,
        .size = sizeof(rv::Vertex) * vertices.size(),
        .debugName = name + "::vertexBuffer",
    });

    indexBuffer = context.createBuffer({
        .usage = rv::BufferUsage::Index,
        .memory = rv::MemoryUsage::Device,
        .size = sizeof(uint32_t) * indices.size(),
        .debugName = name + "::indexBuffer",
    });

    context.oneTimeSubmit([&](rv::CommandBufferHandle commandBuffer) {
        commandBuffer->copyBuffer(vertexBuffer, vertices.data());
        commandBuffer->copyBuffer(indexBuffer, indices.data());
    });
}

rv::AABB Mesh::getLocalAABB() const {
    glm::vec3 min = glm::vec3{FLT_MAX, FLT_MAX, FLT_MAX};
    glm::vec3 max = glm::vec3{-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (auto& prim : primitives) {
        auto& vertices = prim.meshData->vertices;
        auto& indices = prim.meshData->indices;
        for (uint32_t index = prim.firstIndex;  //
             index < prim.firstIndex + prim.indexCount; index++) {
            auto& vert = vertices[indices[index]];
            min = glm::min(min, vert.pos);
            max = glm::max(max, vert.pos);
        }
    }
    return {min, max};
}

rv::AABB Mesh::getWorldAABB() const {
    // WARN: frameは受け取らない
    auto* transform = object->get<Transform>();

    rv::AABB aabb = getLocalAABB();
    if (!transform) {
        return aabb;
    }

    // Apply scale to the extents
    aabb.center *= transform->scale;
    aabb.extents *= transform->scale;

    // Rotate corners of the AABB and find min/max extents
    std::vector<glm::vec3> corners = aabb.getCorners();

    glm::vec3 min = glm::vec3{std::numeric_limits<float>::max()};
    glm::vec3 max = -glm::vec3{std::numeric_limits<float>::max()};
    for (auto& corner : corners) {
        // Apply rotation
        glm::vec3 rotatedCorner = transform->rotation * (corner - aabb.center);

        // Update min and max extents
        min = glm::min(min, rotatedCorner);
        max = glm::max(max, rotatedCorner);
    }

    // Compute new AABB
    rv::AABB worldAABB{aabb.center + min, aabb.center + max};

    // Apply translation
    worldAABB.center += transform->translation;
    return worldAABB;
}

bool Mesh::showAttributes(Scene& scene) {
    bool changed = false;
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Mesh")) {
        for (auto& prim : primitives) {
            if (auto* meshData = prim.meshData) {
                ImGui::Text(("Mesh data: " + meshData->name).c_str());
            }
            if (auto* material = prim.material) {
                ImGui::Text(("Material: " + material->name).c_str());
                changed |= ImGui::ColorEdit4("Base color", &prim.material->baseColor[0]);
                changed |= ImGui::ColorEdit3("Emissive", &prim.material->emissive[0]);
                changed |= ImGui::SliderFloat("Metallic", &prim.material->metallic, 0.0f, 1.0f);
                changed |= ImGui::SliderFloat("Roughness", &prim.material->roughness, 0.0f, 1.0f);
                changed |= ImGui::SliderFloat("IOR", &material->ior, 0.01f, 5.0f);
            }
        }
        ImGui::TreePop();
    }
    return changed;
}
