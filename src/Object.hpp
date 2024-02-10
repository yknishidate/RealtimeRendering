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

struct VertexP {
    glm::vec3 position;

    static auto getAttributeDescriptions() -> std::vector<rv::VertexAttributeDescription> {
        return {
            {offsetof(VertexP, position), vk::Format::eR32G32B32Sfloat},
        };
    }
};

struct VertexPN {
    glm::vec3 position;
    glm::vec3 normal;

    static auto getAttributeDescriptions() -> std::vector<rv::VertexAttributeDescription> {
        return {
            {offsetof(VertexPN, position), vk::Format::eR32G32B32Sfloat},
            {offsetof(VertexPN, normal), vk::Format::eR32G32B32Sfloat},
        };
    }
};

struct VertexPNUT {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec4 tangent;

    static auto getAttributeDescriptions() -> std::vector<rv::VertexAttributeDescription> {
        return {
            {offsetof(VertexPNUT, position), vk::Format::eR32G32B32Sfloat},
            {offsetof(VertexPNUT, normal), vk::Format::eR32G32B32Sfloat},
            {offsetof(VertexPNUT, texCoord), vk::Format::eR32G32Sfloat},
            {offsetof(VertexPNUT, tangent), vk::Format::eR32G32B32A32Sfloat},
        };
    }
};

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
    float roughness{1.0f};
    float ior{1.5f};

    int baseColorTextureIndex{-1};
    int metallicRoughnessTextureIndex{-1};
    int normalTextureIndex{-1};
    int occlusionTextureIndex{-1};
    int emissiveTextureIndex{-1};
    bool enableNormalMapping{false};
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

    glm::mat4 computeTransformMatrix() const;

    glm::mat4 computeNormalMatrix() const;

    bool showAttributes(Scene& scene) override;
};

struct DirectionalLight : Component {
    glm::vec3 getDirection() const;

    bool showAttributes(Scene& scene) override;

    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float phi = 0.0f;
    float theta = 0.0f;

    bool enableShadow = true;
    bool enableShadowCulling = false;
    float shadowBias = 0.005f;
};

struct PointLight final : Component {
    bool showAttributes(Scene& scene) override;

    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float radius = 1.0f;
};

struct AmbientLight final : Component {
    bool showAttributes(Scene& scene) override;

    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    int irradianceTexture = -1;
    int radianceTexture = -1;
};

enum class MeshType {
    Cube,
    Plane,
    COUNT,
};

struct MeshData {
    rv::BufferHandle vertexBuffer;
    rv::BufferHandle indexBuffer;
    std::vector<VertexPNUT> vertices;
    std::vector<uint32_t> indices;
    std::string name;

    MeshData() = default;

    MeshData(const rv::Context& context, MeshType type);

    void createBuffers(const rv::Context& context);
};

struct Mesh final : Component {
    void computeLocalAABB();

    rv::AABB getLocalAABB() const {
        return aabb;
    }

    rv::AABB getWorldAABB() const;

    bool showAttributes(Scene& scene) override;

    uint32_t firstIndex{};
    uint32_t indexCount{};
    uint32_t vertexOffset{};
    uint32_t vertexCount{};
    MeshData* meshData = nullptr;
    Material* material = nullptr;
    rv::AABB aabb{};
};

class Texture {
public:
    std::string name;
    std::string filepath;
    rv::ImageHandle image;
};

// WARN:
// 多重継承なので要注意！
// 本来 rv::Camera と別にカメラクラスを作るべきだが、
// コード簡略化とrv側のメンテのために継承とする
// rv::Camera を包含するとクラス外での扱いが複雑化するため却下
struct Camera final : rv::Camera, Component {
    Camera() = default;

    Camera(rv::Camera::Type _type, float _aspect) : rv::Camera{_type, _aspect} {}

    bool showAttributes(Scene& scene) override {
        bool changed = false;
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
        return changed;
    }

    int typeIndex = 0;
    rv::Frustum frustum{};
};
