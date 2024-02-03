#pragma once
#include <editor/Enums.hpp>
#include <map>
#include <memory>
#include <typeindex>

#include <reactive/Scene/Camera.hpp>
#include <reactive/Scene/Mesh.hpp>

#include "editor/IconManager.hpp"

class Object;

struct Component {
    Component() = default;
    virtual ~Component() = default;

    Component(const Component&) = delete;
    Component(Component&&) = default;

    Component& operator=(const Component&) = delete;
    Component& operator=(Component&&) = default;

    virtual void update(float dt) {}
    virtual bool showAttributes() = 0;

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

    bool showAttributes() override {
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

    bool showAttributes() override {
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
    bool showAttributes() override {
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
    bool showAttributes() override {
        bool changed = false;
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Ambient light")) {
            changed |= ImGui::ColorEdit3("Color", glm::value_ptr(color));
            changed |= ImGui::DragFloat("Intensity", &intensity, 0.001f, 0.0f, 100.0f);
            ImGui::Text("Texture: %d", textureIndex);
            ImGui::TreePop();
        }
        return changed;
    }

    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    int textureIndex = -1;
};

struct Mesh final : Component {
    Mesh(rv::Mesh& _mesh) : mesh{&_mesh} {}

    rv::AABB getLocalAABB() const {
        glm::vec3 min = mesh->vertices[0].pos;
        glm::vec3 max = mesh->vertices[0].pos;
        for (auto& vert : mesh->vertices) {
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

    bool showAttributes() override {
        bool changed = false;
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Mesh")) {
            // Mesh
            ImGui::Text(("Mesh: " + mesh->name).c_str());

            // Material
            if (!material) {
                ImGui::Text("Material: Empty");
                return false;
            }

            ImGui::Text(("Material: " + material->name).c_str());
            changed |= ImGui::ColorEdit4("Base color", &material->baseColor[0]);
            changed |= ImGui::ColorEdit3("Emissive", &material->emissive[0]);
            changed |= ImGui::SliderFloat("Metallic", &material->metallic, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Roughness", &material->roughness, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("IOR", &material->ior, 0.01f, 5.0f);
            ImGui::TreePop();
        }
        return changed;
    }

    rv::Mesh* mesh = nullptr;
    Material* material = nullptr;
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

    void createPrimitiveMeshes() {
        cubeMesh = rv::Mesh::createCubeMesh(*context, {});
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

        if (json.contains("textures")) {
            for (auto& _texture : json["textures"]) {
                std::filesystem::path sceneDir = filepath.parent_path();
                std::filesystem::path texturePath =
                    sceneDir / std::filesystem::path{std::string{_texture}};

                Texture texture{};
                texture.name = texturePath.filename().string();
                texture.filepath = texturePath.string();
                texture.image = rv::Image::loadFromKTX(*context, texturePath.string());

                if (texture.image->getViewType() == vk::ImageViewType::e2D) {
                    textures2D.push_back(std::move(texture));
                    IconManager::addIcon(texture.name, texture.image);
                    status |= rv::SceneStatus::Texture2DAdded;
                } else if (texture.image->getViewType() == vk::ImageViewType::eCube) {
                    // TODO: アイコンサポート
                    texturesCube.push_back(std::move(texture));
                    status |= rv::SceneStatus::TextureCubeAdded;
                } else {
                    // TODO: サポート
                    assert(false);
                }
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

            // NOTE: objはcopy, moveされるとcomponentが持つポインタが壊れるため注意
            objects.emplace_back(object["name"]);
            Object& obj = objects.back();
            status |= rv::SceneStatus::ObjectAdded;

            if (object.contains("translation")) {
                auto& transform = obj.add<Transform>();
                transform.translation.x = object["translation"][0];
                transform.translation.y = object["translation"][1];
                transform.translation.z = object["translation"][2];
            }

            if (object.contains("rotation")) {
                auto* transform = obj.get<Transform>();
                if (!transform) {
                    transform = &obj.add<Transform>();
                }
                transform->rotation.x = object["rotation"][0];
                transform->rotation.y = object["rotation"][1];
                transform->rotation.z = object["rotation"][2];
                transform->rotation.w = object["rotation"][3];
            }

            if (object.contains("scale")) {
                auto* transform = obj.get<Transform>();
                if (!transform) {
                    transform = &obj.add<Transform>();
                }
                transform->scale.x = object["scale"][0];
                transform->scale.y = object["scale"][1];
                transform->scale.z = object["scale"][2];
            }

            if (object["type"] == "Mesh") {
                assert(object.contains("mesh"));
                auto& mesh = obj.add<Mesh>(meshes[object["mesh"]]);

                if (object.contains("material")) {
                    mesh.material = &materials[object["material"]];
                }
            } else if (object["type"] == "DirectionalLight") {
                if (findObject<DirectionalLight>()) {
                    spdlog::warn("Only one directional light can exist in a scene");
                    continue;
                }

                auto& light = obj.add<DirectionalLight>();
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

                auto& light = obj.add<AmbientLight>();
                if (object.contains("color")) {
                    light.color.x = object["color"][0];
                    light.color.y = object["color"][1];
                    light.color.z = object["color"][2];
                }
                if (object.contains("intensity")) {
                    light.intensity = object["intensity"];
                }
                if (object.contains("texture")) {
                    light.textureIndex = object["texture"];
                }
            } else {
                assert(false && "Not implemented");
            }
        }

        if (json.contains("camera")) {
            const auto& _camera = json["camera"];
            if (_camera["type"] == "Orbital") {
                camera = rv::Camera{rv::Camera::Type::Orbital, 1920.0f / 1080.0f};
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
                camera = rv::Camera{rv::Camera::Type::FirstPerson, 1920.0f / 1080.0f};
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

    rv::Mesh& getCubeMesh() {
        return cubeMesh;
    }

    std::vector<rv::Mesh>& getMeshes() {
        return meshes;
    }

    std::vector<Material>& getMaterials() {
        return materials;
    }

    std::vector<Texture>& getTextures2D() {
        return textures2D;
    }

    std::vector<Texture>& getTexturesCube() {
        return texturesCube;
    }

    rv::AABB getAABB() const {
        rv::AABB aabb{};
        for (auto& obj : objects) {
            if (const Mesh* mesh = obj.get<Mesh>()) {
                aabb = rv::AABB::merge(aabb, mesh->getWorldAABB());
            }
        }
        return aabb;
    }

    rv::SceneStatusFlags getStatus() const {
        return status;
    }

    void resetStatus() {
        status = rv::SceneStatus::None;
    }

    void clear() {
        objects.clear();
        objects.reserve(maxObjectCount);
        camera = rv::Camera{rv::Camera::Type::Orbital, 1.0f};
        meshes.clear();
        materials.clear();
        textures2D.clear();
        texturesCube.clear();
        status = rv::SceneStatus::Cleared;
    }

private:
    const rv::Context* context = nullptr;

    // vectorの再アロケートが起きると外部で持っている要素へのポインタが壊れるため
    // 事前に大きなサイズでメモリ確保しておく。
    // ポインタではなく別のハンドルやIDで参照させれば再アロケートも可能。
    // もしくは外部のポインタを強制的に一度nullptrにしても良いが、一般的に難しい。
    int maxObjectCount = 10000;
    std::vector<Object> objects;
    rv::Camera camera{};

    rv::Mesh cubeMesh;
    std::vector<rv::Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures2D;
    std::vector<Texture> texturesCube;

    rv::SceneStatusFlags status = rv::SceneStatus::None;
};
