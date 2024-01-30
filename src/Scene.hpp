#pragma once
#include <map>
#include <memory>
#include <typeindex>

#include <reactive/Scene/Camera.hpp>
#include <reactive/Scene/Mesh.hpp>

struct Component {};

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

struct Transform : public Component {
    Transform() = default;
    Transform(glm::vec3 _translation, glm::quat _rotation, glm::vec3 _scale)
        : translation{_translation}, rotation{_rotation}, scale{_scale} {}

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

    glm::mat4 computeTransformMatrix(int frame) const {
        if (keyFrames.empty()) {
            return computeTransformMatrix();
        }
        return computeTransform(frame).computeTransformMatrix();
    }

    glm::mat4 computeNormalMatrix(int frame) const {
        if (keyFrames.empty()) {
            return computeNormalMatrix();
        }
        return computeTransform(frame).computeNormalMatrix();
    }

private:
    static Transform lerp(const Transform& a, const Transform& b, float t) {
        Transform transform;
        transform.translation = glm::mix(a.translation, b.translation, t);
        transform.rotation = glm::lerp(a.rotation, b.rotation, t);
        transform.scale = glm::mix(a.scale, b.scale, t);
        return transform;
    }

    Transform computeTransform(int frame) const {
        // Handle frame out of range
        if (frame <= keyFrames.front().frame) {
            auto& keyFrame = keyFrames.front();
            return {keyFrame.translation, keyFrame.rotation, keyFrame.scale};
        }
        if (frame >= keyFrames.back().frame) {
            auto& keyFrame = keyFrames.back();
            return {keyFrame.translation, keyFrame.rotation, keyFrame.scale};
        }
        // Search frame
        for (int i = 0; i < keyFrames.size(); i++) {
            const auto& keyFrame = keyFrames[i];
            if (keyFrame.frame == frame) {
                return {keyFrame.translation, keyFrame.rotation, keyFrame.scale};
            }
            if (keyFrame.frame > frame) {
                assert(i >= 1);
                const KeyFrame& prev = keyFrames[i - 1];
                const KeyFrame& next = keyFrames[i];
                frame = prev.frame;
                float t = static_cast<float>(frame) / (next.frame - prev.frame);
                return lerp({prev.translation, prev.rotation, prev.scale},
                            {next.translation, next.rotation, next.scale}, t);
            }
        }
        assert(false && "Failed to compute transform at frame.");
        return {};
    }
};

struct DirectionalLight : public Component {
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float phi = 0.0f;
    float theta = 0.0f;

    bool enableShadow = true;
    bool enableShadowCulling = false;
    float shadowBias = 0.005f;

    glm::vec3 getDirection() const {
        float _phi = glm::radians(phi);
        float _theta = glm::radians(theta);
        float x = sin(_theta) * sin(_phi);
        float y = cos(_theta);
        float z = sin(_theta) * cos(_phi);
        return {x, y, z};
    }
};

struct PointLight : public Component {
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    float radius = 1.0f;
};

struct AmbientLight : public Component {
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
};

struct Mesh : public Component {
    rv::Mesh* mesh = nullptr;
    Material* material = nullptr;
    rv::AABB aabb;

    Mesh(rv::Mesh* mesh, Material* material) : mesh{mesh}, material{material} {
        assert(mesh && "Mesh cannot be created empty");
        glm::vec3 min = mesh->vertices[0].pos;
        glm::vec3 max = mesh->vertices[0].pos;
        for (auto& vert : mesh->vertices) {
            min = glm::min(min, vert.pos);
            max = glm::max(max, vert.pos);
        }
        aabb = rv::AABB{min, max};
    }

    rv::AABB getAABB() const {
        return aabb;
    }
};

class Object {
public:
    Object(std::string _name) : name{std::move(_name)} {}
    ~Object() = default;

    Object(const Object& other) = delete;
    Object(Object&& other) = default;

    template <typename T, typename... Args>
    T& add(Args&&... args) {
        const std::type_info& info = typeid(T);
        const std::type_index& index = {info};
        if (components.contains(index)) {
            spdlog::warn("{} is already added.", info.name());
            return *static_cast<T*>(components.at(index).get());
        }
        components[index] = std::make_unique<T>(std::forward<Args>(args)...);
        return *static_cast<T*>(components.at(index).get());
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

    template <typename T>
    T* get() {
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

    // TODO:
    // あまり Object 内に実装を入れたくない
    // 複数の Component が連携する実装の管理方法を検討する
    rv::AABB getAABB() const {
        // WARN: frameは受け取らない
        // TODO: transformは常に現在フレームの状態を持っていてほしい
        auto* mesh = get<Mesh>();
        auto* transform = get<Transform>();
        if (!mesh) {
            return {};
        }

        rv::AABB aabb = mesh->getAABB();
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
        rv::AABB transformedAABB(min, max);

        // Apply translation
        transformedAABB.center += transform->translation;
        return transformedAABB;
    }

private:
    std::string name;
    std::map<std::type_index, std::unique_ptr<Component>> components;
};

class Texture {
public:
    std::string name;
    std::string filepath;
    rv::ImageHandle image;
};

class Scene {
public:
    Scene() {
        objects.reserve(maxObjectCount);
    }

    void setContext(const rv::Context& _context) {
        context = &_context;
    }

    // TODO: 名前被りを解決
    Object& addObject(std::string name) {
        assert(objects.size() < maxObjectCount);

        objects.emplace_back(std::move(name));
        return objects.back();
    }

    template <typename T>
    Object* findObject() {
        for (auto& object : objects) {
            if (object.get<T>()) {
                return &object;
            }
        }
        return nullptr;
    }

    template <typename T>
    const Object* findObject() const {
        for (auto& object : objects) {
            if (object.get<T>()) {
                return &object;
            }
        }
        return nullptr;
    }

    template <typename T>
    uint32_t countObjects() const {
        uint32_t count = 0;
        for (auto& object : objects) {
            if (object.get<T>()) {
                count++;
            }
        }
        return count;
    }

    void loadFromJson(const std::filesystem::path& filepath) {
        assert(context && "Set context before load scene.");

        std::ifstream jsonFile(filepath);
        if (!jsonFile.is_open()) {
            throw std::runtime_error("Failed to open scene file.");
        }
        nlohmann::json json;
        jsonFile >> json;

        for (const auto& mesh : json["meshes"]) {
            if (mesh["type"] == "Cube") {
                meshes.push_back(rv::Mesh::createCubeMesh(*context, {}));
            } else if (mesh["type"] == "Plane") {
                rv::PlaneMeshCreateInfo createInfo{
                    .width = mesh["width"],
                    .height = mesh["height"],
                    .widthSegments = mesh["widthSegments"],
                    .heightSegments = mesh["heightSegments"],
                };
                meshes.push_back(rv::Mesh::createPlaneMesh(*context, createInfo));
            }
        }

        for (const auto& material : json["materials"]) {
            if (material["type"] == "Standard") {
                const auto& baseColor = material["baseColor"];
                materials.push_back(Material{
                    .baseColor = {baseColor[0], baseColor[1], baseColor[2], baseColor[3]},
                    .name = material["name"],
                });
            } else {
                assert(false && "Not implemented");
            }
        }

        for (const auto& object : json["objects"]) {
            assert(object.contains("name"));

            Object _object{object["name"]};

            if (object["type"] == "Mesh") {
                assert(object.contains("mesh"));
                auto& mesh = _object.add<Mesh>(&meshes[object["mesh"]], nullptr);

                if (object.contains("material")) {
                    mesh.material = &materials[object["material"]];
                }
            } else if (object["type"] == "DirectionalLight") {
                if (findObject<DirectionalLight>()) {
                    spdlog::warn("Only one directional light can exist in a scene");
                    continue;
                }

                auto& light = _object.add<DirectionalLight>();
                if (object.contains("color")) {
                    light.color.x = object["color"][0];
                    light.color.y = object["color"][1];
                    light.color.z = object["color"][2];
                }
                if (object.contains("intensity")) {
                    light.intensity = object["intensity"];
                }
                if (object.contains("phi")) {
                    light.phi = object["phi"];
                }
                if (object.contains("theta")) {
                    light.theta = object["theta"];
                }
            } else if (object["type"] == "AmbientLight") {
                if (findObject<AmbientLight>()) {
                    spdlog::warn("Only one ambient light can exist in a scene");
                    continue;
                }

                auto& light = _object.add<AmbientLight>();
                if (object.contains("color")) {
                    light.color.x = object["color"][0];
                    light.color.y = object["color"][1];
                    light.color.z = object["color"][2];
                }
                if (object.contains("intensity")) {
                    light.intensity = object["intensity"];
                }
            } else {
                assert(false && "Not implemented");
            }

            if (object.contains("translation")) {
                auto& transform = _object.add<Transform>();
                transform.translation.x = object["translation"][0];
                transform.translation.y = object["translation"][1];
                transform.translation.z = object["translation"][2];
            }

            if (object.contains("rotation")) {
                auto* transform = _object.get<Transform>();
                if (!transform) {
                    transform = &_object.add<Transform>();
                }
                transform->rotation.x = object["rotation"][0];
                transform->rotation.y = object["rotation"][1];
                transform->rotation.z = object["rotation"][2];
                transform->rotation.w = object["rotation"][3];
            }

            if (object.contains("scale")) {
                auto* transform = _object.get<Transform>();
                if (!transform) {
                    transform = &_object.add<Transform>();
                }
                transform->scale.x = object["scale"][0];
                transform->scale.y = object["scale"][1];
                transform->scale.z = object["scale"][2];
            }

            objects.push_back(std::move(_object));
        }

        if (json.contains("camera")) {
            const auto& _camera = json["camera"];
            if (_camera["type"] == "Orbital") {
                camera = rv::Camera(nullptr, rv::Camera::Type::Orbital, 1920.0f / 1080.0f);
                if (_camera.contains("distance")) {
                    camera.setDistance(_camera["distance"]);
                }
                if (_camera.contains("phi")) {
                    camera.setPhi(_camera["phi"]);
                }
                if (_camera.contains("theta")) {
                    camera.setTheta(_camera["theta"]);
                }
            } else if (_camera["type"] == "FirstPerson") {
                camera = rv::Camera(nullptr, rv::Camera::Type::FirstPerson, 1920.0f / 1080.0f);
            }
            if (_camera.contains("fovY")) {
                camera.setFovY(glm::radians(static_cast<float>(_camera["fovY"])));
            }
        }
    }

    std::vector<Object>& getObjects() {
        return objects;
    }

    rv::Camera& getCamera() {
        return camera;
    }

    std::vector<rv::Mesh>& getMeshes() {
        return meshes;
    }

    std::vector<Material>& getMaterials() {
        return materials;
    }

    std::vector<Texture>& getTextures() {
        return textures;
    }

    rv::AABB getAABB() const {
        rv::AABB aabb = objects.front().getAABB();
        for (auto& obj : objects) {
            aabb = rv::AABB::merge(aabb, obj.getAABB());
        }
        return aabb;
    }

private:
    const rv::Context* context = nullptr;

    // vectorの再アロケートが起きると外部で持っている要素へのポインタが壊れるため
    // 事前に大きなサイズでメモリ確保しておく。
    // ポインタではなく別のハンドルやIDで参照させれば再アロケートも可能
    int maxObjectCount = 10000;
    std::vector<Object> objects;
    rv::Camera camera{};

    std::vector<rv::Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;
};
