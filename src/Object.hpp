#pragma once
#include <map>
#include <memory>
#include <ranges>
#include <typeindex>

#include <reactive/reactive.hpp>

#include "editor/Enums.hpp"
#include "editor/IconManager.hpp"

class Object;
class Scene;

struct Component {
    Component() = default;
    virtual ~Component() = default;

    Component(const Component&) = delete;
    Component(Component&&) = default;

    Component& operator=(const Component&) = delete;
    Component& operator=(Component&&) = default;

    virtual void update(float dt) {}
    virtual bool showAttributes(Scene& scene) = 0;

    Object* object = nullptr;
};

class Object final {
public:
    Object(std::string _name) : name{std::move(_name)} {}
    ~Object() = default;

    Object(const Object& other) = delete;
    Object(Object&& other) = default;

    Object& operator=(const Object& other) = delete;
    Object& operator=(Object&& other) = default;

    void update(float dt) {
        for (auto& comp : components | std::views::values) {
            comp->update(dt);
        }
    }

    template <typename T, typename... Args>
    T& add(Args&&... args) {
        const std::type_info& info = typeid(T);
        const std::type_index& index = {info};
        if (components.contains(index)) {
            spdlog::warn("{} is already added.", info.name());
            return *static_cast<T*>(components.at(index).get());
        }
        components[index] = std::make_unique<T>(std::forward<Args>(args)...);
        components[index]->object = this;
        return *static_cast<T*>(components.at(index).get());
    }

    template <typename T>
    T* get() {
        const std::type_info& info = typeid(T);
        const std::type_index& index = {info};
        if (components.contains(index)) {
            return static_cast<T*>(components.at(index).get());
        }
        return nullptr;
    }

    template <typename T>
    const T* get() const {
        const std::type_info& info = typeid(T);
        const std::type_index& index = {info};
        if (components.contains(index)) {
            return static_cast<T*>(components.at(index).get());
        }
        return nullptr;
    }

    std::string getName() const {
        return name;
    }

    const auto& getComponents() const {
        return components;
    }

private:
    std::string name;
    std::map<std::type_index, std::unique_ptr<Component>> components;
};

struct Material {
    glm::vec4 baseColor{1.0f};
    glm::vec3 emissive{0.0f};
    float metallic{0.0f};
    float roughness{0.0f};
    float ior{1.5f};

    int baseColorTextureIndex{-1};
    int metallicRoughnessTextureIndex{-1};
    int normalTextureIndex{-1};
    int occlusionTextureIndex{-1};
    int emissiveTextureIndex{-1};
    std::string name;
};

struct Transform final : Component {
    glm::vec3 translation = {0.0f, 0.0f, 0.0f};
    glm::quat rotation = {1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale = {1.0f, 1.0f, 1.0f};

    struct KeyFrame {
        int frame;
        glm::vec3 translation = {0.0f, 0.0f, 0.0f};
        glm::quat rotation = {1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale = {1.0f, 1.0f, 1.0f};
    };

    std::vector<KeyFrame> keyFrames;

    glm::mat4 computeTransformMatrix() const {
        glm::mat4 T = glm::translate(glm::mat4{1.0}, translation);
        glm::mat4 R = glm::mat4_cast(rotation);
        glm::mat4 S = glm::scale(glm::mat4{1.0}, scale);
        return T * R * S;
    }

    glm::mat4 computeNormalMatrix() const {
        glm::mat4 R = glm::mat4_cast(rotation);
        glm::mat4 S = glm::scale(glm::mat4{1.0}, glm::vec3{1.0} / scale);
        return R * S;
    }

    glm::mat4 computeTransformMatrix(int /*frame*/) const {
        // if (keyFrames.empty()) {
        return computeTransformMatrix();
        //}
        // return computeTransform(frame).computeTransformMatrix();
    }

    glm::mat4 computeNormalMatrix(int /*frame*/) const {
        // if (keyFrames.empty()) {
        return computeNormalMatrix();
        //}
        // return computeTransform(frame).computeNormalMatrix();
    }

    bool showAttributes(Scene& scene) override {
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

private:
    // static Transform lerp(const Transform& a, const Transform& b, float t) {
    //     Transform transform{};
    //     transform.translation = glm::mix(a.translation, b.translation, t);
    //     transform.rotation = glm::lerp(a.rotation, b.rotation, t);
    //     transform.scale = glm::mix(a.scale, b.scale, t);
    //     transform.object = a.object;
    //     return transform;
    // }

    // Transform computeTransform(int frame) const {
    //     // Handle frame out of range
    //     if (frame <= keyFrames.front().frame) {
    //         auto& keyFrame = keyFrames.front();
    //         Transform trans{};
    //         trans.translation = keyFrame.translation;
    //         trans.rotation = keyFrame.rotation;
    //         trans.scale = keyFrame.scale;
    //         trans.object = object;
    //         return trans;
    //     }
    //     if (frame >= keyFrames.back().frame) {
    //         auto& keyFrame = keyFrames.back();
    //         Transform trans{};
    //         trans.translation = keyFrame.translation;
    //         trans.rotation = keyFrame.rotation;
    //         trans.scale = keyFrame.scale;
    //         trans.object = object;
    //         return trans;
    //     }
    //     // Search frame
    //     for (size_t i = 0; i < keyFrames.size(); i++) {
    //         const auto& keyFrame = keyFrames[i];
    //         if (keyFrame.frame == frame) {
    //             Transform trans{};
    //             trans.translation = keyFrame.translation;
    //             trans.rotation = keyFrame.rotation;
    //             trans.scale = keyFrame.scale;
    //             trans.object = object;
    //             return trans;
    //         }
    //         if (keyFrame.frame > frame) {
    //             assert(i >= 1);
    //             const KeyFrame& prev = keyFrames[i - 1];
    //             const KeyFrame& next = keyFrames[i];
    //             frame = prev.frame;
    //             float t = static_cast<float>(frame) / static_cast<float>(next.frame -
    //             prev.frame);

    //            Transform prevTrans{};
    //            prevTrans.translation = prev.translation;
    //            prevTrans.rotation = prev.rotation;
    //            prevTrans.scale = prev.scale;
    //            prevTrans.object = object;

    //            Transform nextTrans{};
    //            nextTrans.translation = next.translation;
    //            nextTrans.rotation = next.rotation;
    //            nextTrans.scale = next.scale;
    //            nextTrans.object = object;

    //            return lerp(prevTrans, nextTrans, t);
    //        }
    //    }
    //    assert(false && "Failed to compute transform at frame.");
    //    return {};
    //}
};

struct DirectionalLight : Component {
    glm::vec3 getDirection() const {
        float _phi = glm::radians(phi);
        float _theta = glm::radians(theta);
        float x = sin(_theta) * sin(_phi);
        float y = cos(_theta);
        float z = sin(_theta) * cos(_phi);
        return {x, y, z};
    }

    bool showAttributes(Scene& scene) override {
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

    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float phi = 0.0f;
    float theta = 0.0f;

    bool enableShadow = true;
    bool enableShadowCulling = false;
    float shadowBias = 0.005f;
};

struct PointLight final : Component {
    bool showAttributes(Scene& scene) override {
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

    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float radius = 1.0f;
};

struct AmbientLight final : Component {
    bool showAttributes(Scene& scene) override;

    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    int textureCube = -1;
};

struct Primitive {
    uint32_t firstIndex{};
    uint32_t indexCount{};
    uint32_t vertexCount{};
    Material* material = nullptr;
};

enum class MeshType {
    Cube,
    Plane,
    COUNT,
};

struct MeshData {
    rv::BufferHandle vertexBuffer;
    rv::BufferHandle indexBuffer;
    std::vector<rv::Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Primitive> primitives;
    std::string name;

    MeshData(const rv::Context& context, MeshType type) {
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

    MeshData(const rv::Context& context,  //
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

    void createBuffers(const rv::Context& context) {
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
};

struct Mesh final : Component {
    rv::AABB getLocalAABB() const {
        glm::vec3 min = meshData->vertices[0].pos;
        glm::vec3 max = meshData->vertices[0].pos;
        for (auto& vert : meshData->vertices) {
            min = glm::min(min, vert.pos);
            max = glm::max(max, vert.pos);
        }
        return {min, max};
    }

    rv::AABB getWorldAABB() const {
        // WARN: frameは受け取らない
        auto* transform = object->get<Transform>();

        rv::AABB aabb = getLocalAABB();
        if (!transform) {
            return aabb;
        }

        // Apply scale to the extents
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
        rv::AABB worldAABB{min, max};

        // Apply translation
        worldAABB.center += transform->translation;
        return worldAABB;
    }

    bool showAttributes(Scene& scene) override {
        bool changed = false;
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Mesh")) {
            // Mesh
            ImGui::Text(("Mesh: " + meshData->name).c_str());

            // Material
            for (auto& prim : meshData->primitives) {
                if (auto* material = prim.material) {
                    ImGui::Text(("Material: " + material->name).c_str());
                    changed |= ImGui::ColorEdit4("Base color", &prim.material->baseColor[0]);
                    changed |= ImGui::ColorEdit3("Emissive", &prim.material->emissive[0]);
                    changed |= ImGui::SliderFloat("Metallic", &prim.material->metallic, 0.0f, 1.0f);
                    changed |=
                        ImGui::SliderFloat("Roughness", &prim.material->roughness, 0.0f, 1.0f);
                    changed |= ImGui::SliderFloat("IOR", &material->ior, 0.01f, 5.0f);
                }
            }
            ImGui::TreePop();
        }
        return changed;
    }

    MeshData* meshData = nullptr;
};

class Texture {
public:
    std::string name;
    std::string filepath;
    rv::ImageHandle image;
};
