#include "Object.hpp"

#include "Scene.hpp"
#include "WindowAdapter.hpp"

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

void Transform::showAttributes(Scene& scene) {
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
}

glm::vec3 DirectionalLight::getDirection() const {
    float _phi = glm::radians(phi);
    float _theta = glm::radians(theta);
    float x = sin(_theta) * sin(_phi);
    float y = cos(_theta);
    float z = sin(_theta) * cos(_phi);
    return {x, y, z};
}

void DirectionalLight::showAttributes(Scene& scene) {
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
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
}

void PointLight::showAttributes(Scene& scene) {
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Point light")) {
        changed |= ImGui::ColorEdit3("Color", glm::value_ptr(color));
        changed |= ImGui::DragFloat("Intensity", &intensity, 0.001f, 0.0f, 100.0f);
        changed |= ImGui::DragFloat("Radius", &radius, 0.001f, 0.0f, 100.0f);
        ImGui::TreePop();
    }
}

void AmbientLight::showAttributes(Scene& scene) {
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
    createBuffers(context);
}

void MeshData::createBuffers(const rv::Context& context) {
    vertexBuffer = context.createBuffer({
        .usage = rv::BufferUsage::Vertex,
        .memory = rv::MemoryUsage::Device,
        .size = sizeof(VertexPNUT) * vertices.size(),
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

void Mesh::computeLocalAABB() {
    glm::vec3 min = glm::vec3{FLT_MAX, FLT_MAX, FLT_MAX};
    glm::vec3 max = glm::vec3{-FLT_MAX, -FLT_MAX, -FLT_MAX};
    auto& vertices = meshData->vertices;
    auto& indices = meshData->indices;
    for (uint32_t index = firstIndex;  //
         index < firstIndex + indexCount; index++) {
        auto& vert = vertices[vertexOffset + indices[index]];
        min = glm::min(min, vert.position);
        max = glm::max(max, vert.position);
    }
    aabb = {min, max};
}

rv::AABB Mesh::getWorldAABB() const {
    // WARN: frameは受け取らない
    auto* transform = object->get<Transform>();

    rv::AABB _aabb = getLocalAABB();
    if (!transform) {
        return _aabb;
    }

    // Apply scale to the extents
    _aabb.center *= transform->scale;
    _aabb.extents *= transform->scale;

    // Rotate corners of the AABB and find min/max extents
    std::vector<glm::vec3> corners = _aabb.getCorners();

    glm::vec3 min = glm::vec3{std::numeric_limits<float>::max()};
    glm::vec3 max = -glm::vec3{std::numeric_limits<float>::max()};
    for (auto& corner : corners) {
        // Apply rotation
        glm::vec3 rotatedCorner = transform->rotation * corner;

        // Update min and max extents
        min = glm::min(min, rotatedCorner);
        max = glm::max(max, rotatedCorner);
    }

    // Compute new AABB
    rv::AABB worldAABB{min, max};

    // Apply translation
    worldAABB.center += transform->translation;
    return worldAABB;
}

void Mesh::showAttributes(Scene& scene) {
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Mesh")) {
        ImGui::Text(("Mesh data: " + meshData->name).c_str());
        ImGui::Text(("Material: " + material->name).c_str());
        changed |= ImGui::ColorEdit4("Base color", &material->baseColor[0]);
        changed |= ImGui::ColorEdit3("Emissive", &material->emissive[0]);
        changed |= ImGui::SliderFloat("Metallic", &material->metallic, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Roughness", &material->roughness, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("IOR", &material->ior, 0.01f, 5.0f);
        changed |= ImGui::Checkbox("Normal mapping", &material->enableNormalMapping);
        ImGui::TreePop();
    }
}

void Camera::showAttributes(Scene& scene) {
    if (ImGui::Combo("Type", &typeIndex, "Orbital\0FirstPerson\0", 2)) {
        // TODO: なるべく保存できる値は引き継ぐ
        if (typeIndex == 0) {
            type = Type::Orbital;
            params = OrbitalParams{};
        } else {
            type = Type::FirstPerson;
            params = FirstPersonParams{};
        }
    }

    bool inUse = scene.currentCamera == this;
    if (ImGui::Checkbox("Use", &inUse)) {
        if (inUse) {
            scene.setMainCamera(*this);
        } else {
            scene.useDefaultCamera();
        }
    }

    float fovDeg = glm::degrees(fovY);
    if (ImGui::SliderFloat("Fov Y", &fovDeg, 1.0f, 179.0f)) {
        fovY = glm::radians(fovDeg);
    }
}

void Camera::update(Scene& scene, float dt) {
    if (scene.currentCamera == this) {
        for (int key : {GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_D, GLFW_KEY_A, GLFW_KEY_SPACE}) {
            if (rv::Window::isKeyDown(key)) {
                processKey(key);
            }
        }
        glm::vec2 _mouseDragLeft = WindowAdapter::getMouseDragLeft();
        glm::vec2 _mouseDragRight = WindowAdapter::getMouseDragRight();
        processMouseDragLeft(glm::vec2{_mouseDragLeft.x, -_mouseDragLeft.y} * 0.5f);
        processMouseDragRight(glm::vec2{_mouseDragRight.x, -_mouseDragRight.y} * 0.5f);
        processMouseScroll(WindowAdapter::getMouseScroll());

        if (&scene.defaultCamera != this) {
            const rv::AABB minAABB{glm::vec3{-100.0f}, glm::vec3{100.0f}};
            rv::AABB aabb = rv::AABB::merge(minAABB, scene.getAABB());

            glm::vec3 front = getFront();
            glm::vec3 position = getPosition();
            glm::vec3 nearPoint = aabb.getFurthestCorner(-front);
            glm::vec3 farPoint = aabb.getFurthestCorner(front);
            zNear = glm::max(glm::dot(nearPoint - position, front), 0.01f);
            zFar = glm::dot(farPoint - position, front);
        }
    }

    aspect = WindowAdapter::getWidth() / WindowAdapter::getHeight();
    frustum = rv::Frustum{*this};
}
