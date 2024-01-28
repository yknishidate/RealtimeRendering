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
    enum class Type {
        Empty,
        Mesh,
        DirectionalLight,
        PointLight,
    };

    std::string name;
    Type type = Type::Empty;

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
    // TODO: privateにする
    std::vector<Object> objects;
    rv::Camera camera;

    std::vector<rv::Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;

    Object& addDirectionalLight() {
        Object object;
        object.type = Object::Type::DirectionalLight;
        object.directionalLight = DirectionalLight{};
        objects.push_back(object);
        return objects.back();
    }

    Object& addPointLight() {
        Object object;
        object.type = Object::Type::PointLight;
        object.pointLight = PointLight{};
        objects.push_back(object);
        return objects.back();
    }

    std::optional<Object> findObject(Object::Type type) const {
        for (auto& object : objects) {
            if (object.type == type) {
                return object;
            }
        }
        return std::nullopt;
    }

    uint32_t countObjects(Object::Type type) const {
        uint32_t count = 0;
        for (auto& object : objects) {
            if (object.type == type) {
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
            Object _object{};

            assert(object.contains("name"));
            _object.name = object["name"];

            if (object["type"] == "Mesh") {
                assert(object.contains("mesh"));
                _object.type = Object::Type::Mesh;

                _object.mesh = Mesh{};
                _object.mesh.value().mesh = &meshes[object["mesh"]];
            } else {
                assert(false && "Not implemented");
            }

            if (object.contains("material")) {
                if (_object.mesh.has_value()) {
                    _object.mesh.value().material = &materials[object["material"]];
                }
            }

            if (object.contains("translation")) {
                if (!_object.transform.has_value()) {
                    _object.transform = Transform{};
                }
                _object.transform->translation.x = object["translation"][0];
                _object.transform->translation.y = object["translation"][1];
                _object.transform->translation.z = object["translation"][2];
            }

            if (object.contains("rotation")) {
                if (!_object.transform.has_value()) {
                    _object.transform = Transform{};
                }
                _object.transform->rotation.x = object["rotation"][0];
                _object.transform->rotation.y = object["rotation"][1];
                _object.transform->rotation.z = object["rotation"][2];
                _object.transform->rotation.w = object["rotation"][3];
            }

            if (object.contains("scale")) {
                if (!_object.transform.has_value()) {
                    _object.transform = Transform{};
                }
                _object.transform->scale.x = object["scale"][0];
                _object.transform->scale.y = object["scale"][1];
                _object.transform->scale.z = object["scale"][2];
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
};
