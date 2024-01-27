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

    static Transform lerp(const Transform& a, const Transform& b, float t) {
        Transform transform;
        transform.translation = glm::mix(a.translation, b.translation, t);
        transform.rotation = glm::lerp(a.rotation, b.rotation, t);
        transform.scale = glm::mix(a.scale, b.scale, t);
        return transform;
    }
};

struct KeyFrame {
    int frame;
    Transform transform;
};

class Light {
public:
    enum class Type {
        Directional,
        Point,
        Ambient,
    };

    // Common
    glm::vec3 color;
    float intensity;

    // Directional
    glm::vec3 direction;

    // Point
    glm::vec3 position;
    float radius;
};

class Object {
public:
    Transform computeTransformAtFrame(int frame) const {
        // Handle frame out of range
        if (frame <= keyFrames.front().frame) {
            return keyFrames.front().transform;
        }
        if (frame >= keyFrames.back().frame) {
            return keyFrames.back().transform;
        }

        // Search frame
        for (int i = 0; i < keyFrames.size(); i++) {
            const auto& keyFrame = keyFrames[i];
            if (keyFrame.frame == frame) {
                return keyFrame.transform;
            }

            if (keyFrame.frame > frame) {
                assert(i >= 1);
                const KeyFrame& prev = keyFrames[i - 1];
                const KeyFrame& next = keyFrames[i];
                frame = prev.frame;
                float t = static_cast<float>(frame) / (next.frame - prev.frame);

                return Transform::lerp(prev.transform, next.transform, t);
            }
        }
        assert(false && "Failed to compute transform at frame.");
        return {};
    }

    glm::mat4 computeTransformMatrix(int frame) const {
        if (keyFrames.empty()) {
            return transform.computeTransformMatrix();
        }
        return computeTransformAtFrame(frame).computeTransformMatrix();
    }

    glm::mat4 computeNormalMatrix(int frame) const {
        if (keyFrames.empty()) {
            return transform.computeNormalMatrix();
        }
        return computeTransformAtFrame(frame).computeNormalMatrix();
    }

    enum class Type {
        Mesh,
    };

    std::string name;
    Type type = Type::Mesh;
    rv::Mesh* mesh = nullptr;
    Material* material = nullptr;
    Transform transform;
    std::vector<KeyFrame> keyFrames;
};

class Texture {
public:
    std::string name;
    std::string filepath;
    rv::ImageHandle image;
};

class Scene {
public:
    std::vector<Object> objects;
    std::vector<rv::Mesh> meshes;
    std::vector<Material> materials;
    std::vector<rv::Camera> cameras;
    std::vector<Texture> textures;
    rv::Camera camera;

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
                _object.mesh = &meshes[object["mesh"]];
            } else {
                assert(false && "Not implemented");
            }

            if (object.contains("material")) {
                _object.material = &materials[object["material"]];
            }

            if (object.contains("translation")) {
                _object.transform.translation.x = object["translation"][0];
                _object.transform.translation.y = object["translation"][1];
                _object.transform.translation.z = object["translation"][2];
            }

            if (object.contains("rotation")) {
                _object.transform.rotation.x = object["rotation"][0];
                _object.transform.rotation.y = object["rotation"][1];
                _object.transform.rotation.z = object["rotation"][2];
                _object.transform.rotation.w = object["rotation"][3];
            }

            if (object.contains("scale")) {
                _object.transform.scale.x = object["scale"][0];
                _object.transform.scale.y = object["scale"][1];
                _object.transform.scale.z = object["scale"][2];
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
