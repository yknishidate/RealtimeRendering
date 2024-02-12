#pragma once
#include <tiny_gltf.h>
#include "Object.hpp"
#include "reactive/Scene/Camera.hpp"

class Scene {
    friend struct Camera;

public:
    void init(const rv::Context& _context);

    Object& addObject(const std::string& name);

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

    const Object* findObject(const std::string& name) const;

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

    void update(float dt) {
        if (currentCamera == &defaultCamera) {
            defaultCamera.update(*this, dt);
        }

        for (auto& object : objects) {
            for (auto& comp : object.getComponents() | std::views::values) {
                comp->update(*this, dt);
            }
        }

        computeAABB();
    }

    void loadFromGltf(const std::filesystem::path& filepath);

    void loadTextures(tinygltf::Model& gltfModel);

    void loadMaterials(tinygltf::Model& gltfModel);

    void loadMesh(tinygltf::Model& gltfModel, tinygltf::Primitive& gltfPrimitive, Mesh& mesh);

    void loadNodes(tinygltf::Model& gltfModel);

    void loadFromJson(const std::filesystem::path& filepath);

    std::vector<Object>& getObjects() {
        return objects;
    }

    Camera& getCamera() {
        assert(currentCamera);
        return *currentCamera;
    }

    Camera& getDefaultCamera() {
        return defaultCamera;
    }

    void setMainCamera(Camera& camera) {
        currentCamera = &camera;
    }

    void useDefaultCamera() {
        currentCamera = &defaultCamera;
    }

    const MeshData& getCubeMesh() {
        return templateMeshData[static_cast<int>(MeshType::Cube)];
    }

    const MeshData& getMeshData() {
        return meshData;
    }

    const std::vector<Material>& getMaterials() {
        return materials;
    }

    const std::vector<Texture>& getTextures2D() {
        return textures2D;
    }

    const std::vector<Texture>& getTexturesCube() {
        return texturesCube;
    }

    void addTexture2D(const Texture& tex) {
        textures2D.push_back(tex);
        status |= SceneStatus::Texture2DAdded;
    }

    void addTextureCube(const Texture& tex) {
        texturesCube.push_back(tex);
        status |= SceneStatus::TextureCubeAdded;
    }

    void computeAABB() {
        for (auto& obj : objects) {
            if (const Mesh* mesh = obj.get<Mesh>()) {
                aabb = rv::AABB::merge(aabb, mesh->getWorldAABB());
            }
        }
    }

    rv::AABB getAABB() const {
        return aabb;
    }

    SceneStatusFlags getStatus() const {
        return status;
    }

    void resetStatus() {
        status = SceneStatus::None;
    }

    void clear() {
        objects.clear();
        objects.reserve(maxObjectCount);

        currentCamera = &defaultCamera;

        meshData = MeshData{};
        materials.clear();
        textures2D.clear();
        texturesCube.clear();
        status = SceneStatus::Cleared;
    }

private:
    const rv::Context* context = nullptr;

    // vectorの再アロケートが起きると外部で持っている要素へのポインタが壊れるため
    // 事前に大きなサイズでメモリ確保しておく。
    // ポインタではなく別のハンドルやIDで参照させれば再アロケートも可能。
    // もしくは外部のポインタを強制的に一度nullptrにしても良いが、一般的に難しい。
    int maxObjectCount = 10000;
    std::vector<Object> objects{};

    Camera defaultCamera{rv::Camera::Type::Orbital};
    Camera* currentCamera = &defaultCamera;

    std::vector<MeshData> templateMeshData{};

    // 全ての頂点とインデックス
    MeshData meshData;

    std::vector<Material> materials{};
    std::vector<Texture> textures2D{};
    std::vector<Texture> texturesCube{};

    rv::AABB aabb{};

    SceneStatusFlags status = SceneStatus::None;
};

inline const Object* Scene::findObject(const std::string& name) const {
    for (auto& obj : objects) {
        if (obj.getName() == name) {
            return &obj;
        }
    }
    return nullptr;
}
