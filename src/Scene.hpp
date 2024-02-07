#pragma once
#include <tiny_gltf.h>
#include "Object.hpp"

class Scene {
public:
    void init(const rv::Context& _context);

    Object& addObject(std::string name);

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

    void loadFromGltf(const std::filesystem::path& filepath);

    void loadTextures(tinygltf::Model& gltfModel);

    void loadMaterials(tinygltf::Model& gltfModel);

    void loadMeshes(tinygltf::Model& gltfModel);

    void loadNodes(tinygltf::Model& gltfModel);

    void loadFromJson(const std::filesystem::path& filepath);

    std::vector<Object>& getObjects() {
        return objects;
    }

    rv::Camera& getCamera() {
        return camera;
    }

    const MeshData& getCubeMesh() {
        return templateMeshData[static_cast<int>(MeshType::Cube)];
    }

    const std::vector<MeshData>& getMeshData() {
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

    rv::AABB getAABB() const {
        rv::AABB aabb{};
        for (auto& obj : objects) {
            if (const Mesh* mesh = obj.get<Mesh>()) {
                aabb = rv::AABB::merge(aabb, mesh->getWorldAABB());
            }
        }
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
        camera = rv::Camera{rv::Camera::Type::Orbital, 1.0f};
        meshData.clear();
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
    rv::Camera camera{rv::Camera::Type::Orbital, 1.0f};

    std::vector<MeshData> templateMeshData{};
    std::vector<MeshData> meshData{};
    std::vector<Material> materials{};
    std::vector<Texture> textures2D{};
    std::vector<Texture> texturesCube{};

    SceneStatusFlags status = SceneStatus::None;
};
