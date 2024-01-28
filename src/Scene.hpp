#pragma once
#include <reactive/Scene/Camera.hpp>
#include <reactive/Scene/Mesh.hpp>

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

struct Transform {
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

struct DirectionalLight {
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float phi = 0.0f;
    float theta = 0.0f;

    glm::vec3 getDirection() const {
        float _phi = glm::radians(phi);
        float _theta = glm::radians(theta);
        float x = sin(_theta) * sin(_phi);
        float y = cos(_theta);
        float z = sin(_theta) * cos(_phi);
        return {x, y, z};
    }
};

struct PointLight {
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    float radius = 1.0f;
};

struct Mesh {
    rv::Mesh* mesh = nullptr;
    Material* material = nullptr;
};

class Object {
public:
    Object(std::string _name) : name{std::move(_name)} {}

    template <typename T>
    T& add() {
        if constexpr (std::is_same<T, Transform>()) {
            transform = Transform{};
            return transform.value();
        }
        if constexpr (std::is_same<T, Mesh>()) {
            mesh = Mesh{};
            return mesh.value();
        }
        if constexpr (std::is_same<T, DirectionalLight>()) {
            directionalLight = DirectionalLight{};
            return directionalLight.value();
        }
        if constexpr (std::is_same<T, PointLight>()) {
            pointLight = PointLight{};
            return pointLight.value();
        }
    }

    template <typename T>
    const T* get() const {
        if constexpr (std::is_same<T, Transform>()) {
            if (transform) {
                return &transform.value();
            }
        }
        if constexpr (std::is_same<T, Mesh>()) {
            if (mesh) {
                return &mesh.value();
            }
        }
        if constexpr (std::is_same<T, DirectionalLight>()) {
            if (directionalLight) {
                return &directionalLight.value();
            }
        }
        if constexpr (std::is_same<T, PointLight>()) {
            if (pointLight) {
                return &pointLight.value();
            }
        }
        return nullptr;
    }

    template <typename T>
    T* get() {
        if constexpr (std::is_same<T, Transform>()) {
            if (transform) {
                return &transform.value();
            }
        }
        if constexpr (std::is_same<T, Mesh>()) {
            if (mesh) {
                return &mesh.value();
            }
        }
        if constexpr (std::is_same<T, DirectionalLight>()) {
            if (directionalLight) {
                return &directionalLight.value();
            }
        }
        if constexpr (std::is_same<T, PointLight>()) {
            if (pointLight) {
                return &pointLight.value();
            }
        }
        return nullptr;
    }

    std::string getName() const {
        return name;
    }

private:
    std::string name;
    std::optional<Transform> transform;
    std::optional<Mesh> mesh;  // TODO: 空で構築出来ないようにコンストラクタを導入
    std::optional<DirectionalLight> directionalLight;
    std::optional<PointLight> pointLight;
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

    void loadFromJson(const rv::Context& context, const std::filesystem::path& filepath) {
        std::ifstream jsonFile(filepath);
        if (!jsonFile.is_open()) {
            throw std::runtime_error("Failed to open scene file.");
        }
        nlohmann::json json;
        jsonFile >> json;

        for (const auto& mesh : json["meshes"]) {
            if (mesh["type"] == "Cube") {
                meshes.push_back(rv::Mesh::createCubeMesh(context, {}));
            } else if (mesh["type"] == "Plane") {
                rv::PlaneMeshCreateInfo createInfo{
                    .width = mesh["width"],
                    .height = mesh["height"],
                    .widthSegments = mesh["widthSegments"],
                    .heightSegments = mesh["heightSegments"],
                };
                meshes.push_back(rv::Mesh::createPlaneMesh(context, createInfo));
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
                auto& mesh = _object.add<Mesh>();
                mesh.mesh = &meshes[object["mesh"]];
            } else {
                assert(false && "Not implemented");
            }

            if (object.contains("material")) {
                if (Mesh* mesh = _object.get<Mesh>()) {
                    mesh->material = &materials[object["material"]];
                }
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
            objects.push_back(_object);
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

private:
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
