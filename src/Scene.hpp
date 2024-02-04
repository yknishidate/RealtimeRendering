#pragma once
#include <map>
#include <memory>
#include <typeindex>

#include <tiny_gltf.h>
#include <nlohmann/json.hpp>

#include "Object.hpp"

class Scene {
public:
    Scene() {
        objects.reserve(maxObjectCount);
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

    void createTemplateMeshData(const rv::Context& context) {
        int count = static_cast<int>(MeshType::COUNT);
        templateMeshData.reserve(count);
        for (int type = 0; type < count; type++) {
            templateMeshData.emplace_back(context, static_cast<MeshType>(type));
        }
    }

    void loadFromGltf(const rv::Context& context, const std::filesystem::path& filepath) {
        context.getDevice().waitIdle();
        clear();

        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;

        bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath.string());
        if (!warn.empty()) {
            std::cerr << "Warn: " << warn.c_str() << std::endl;
        }
        if (!err.empty()) {
            std::cerr << "Err: " << err.c_str() << std::endl;
        }
        if (!ret) {
            throw std::runtime_error("Failed to parse glTF: " + filepath.string());
        }

        loadMaterials(model);
        loadMeshes(context, model);
        loadNodes(model);
        spdlog::info("Loaded glTF file: {}", filepath.string());
        spdlog::info("  Material: {}", materials.size());
        spdlog::info("  Node: {}", objects.size());
        spdlog::info("  Mesh: {}", meshData.size());
    }

    void loadMaterials(tinygltf::Model& gltfModel) {
        for (auto& mat : gltfModel.materials) {
            Material material;

            // Base color
            if (mat.values.contains("baseColorTexture")) {
                material.baseColorTextureIndex = mat.values["baseColorTexture"].TextureIndex();
            }
            if (mat.values.contains("baseColorFactor")) {
                material.baseColor =
                    glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
            }

            // Metallic / Roughness
            if (mat.values.contains("metallicRoughnessTexture")) {
                material.metallicRoughnessTextureIndex =
                    mat.values["metallicRoughnessTexture"].TextureIndex();
            }
            if (mat.values.contains("roughnessFactor")) {
                material.roughness = static_cast<float>(mat.values["roughnessFactor"].Factor());
            }
            if (mat.values.contains("metallicFactor")) {
                material.metallic = static_cast<float>(mat.values["metallicFactor"].Factor());
            }

            // Normal
            if (mat.additionalValues.contains("normalTexture")) {
                material.normalTextureIndex = mat.additionalValues["normalTexture"].TextureIndex();
            }

            // Emissive
            material.emissive[0] = static_cast<float>(mat.emissiveFactor[0]);
            material.emissive[1] = static_cast<float>(mat.emissiveFactor[1]);
            material.emissive[2] = static_cast<float>(mat.emissiveFactor[2]);
            if (mat.additionalValues.contains("emissiveTexture")) {
                material.emissiveTextureIndex =
                    mat.additionalValues["emissiveTexture"].TextureIndex();
            }

            materials.push_back(material);
        }
    }

    void loadMeshes(const rv::Context& context, tinygltf::Model& gltfModel) {
        for (int gltfMeshIndex = 0; gltfMeshIndex < gltfModel.meshes.size(); gltfMeshIndex++) {
            auto& gltfMesh = gltfModel.meshes.at(gltfMeshIndex);

            // Create a vector to store the vertices
            std::vector<rv::Vertex> vertices{};
            std::vector<uint32_t> indices{};

            // TODO: all prims
            const auto& gltfPrimitive = gltfMesh.primitives[0];
            // for (const auto& gltfPrimitive : gltfMesh.primitives) {
            //  WARN: Since different attributes may refer to the same data, creating a
            //  vertex/index buffer for each attribute will result in data duplication.

            // Vertex attributes
            auto& attributes = gltfPrimitive.attributes;

            assert(attributes.contains("POSITION"));
            int positionIndex = attributes.find("POSITION")->second;
            tinygltf::Accessor* positionAccessor = &gltfModel.accessors[positionIndex];
            tinygltf::BufferView* positionBufferView =
                &gltfModel.bufferViews[positionAccessor->bufferView];

            tinygltf::Accessor* normalAccessor = nullptr;
            tinygltf::BufferView* normalBufferView = nullptr;
            if (attributes.contains("NORMAL")) {
                int normalIndex = attributes.find("NORMAL")->second;
                normalAccessor = &gltfModel.accessors[normalIndex];
                normalBufferView = &gltfModel.bufferViews[normalAccessor->bufferView];
            }

            tinygltf::Accessor* texCoordAccessor = nullptr;
            tinygltf::BufferView* texCoordBufferView = nullptr;
            if (attributes.contains("TEXCOORD_0")) {
                int texCoordIndex = attributes.find("TEXCOORD_0")->second;
                texCoordAccessor = &gltfModel.accessors[texCoordIndex];
                texCoordBufferView = &gltfModel.bufferViews[texCoordAccessor->bufferView];
            }

            // vertices.resize(vertices.size() + positionAccessor->count);

            // Loop over the vertices
            for (size_t i = 0; i < positionAccessor->count; i++) {
                rv::Vertex vertex;

                // NOTE:
                // byteStride が 0 の場合、データは密に詰まっている
                size_t positionByteStride = positionBufferView->byteStride;
                if (positionByteStride == 0) {
                    positionByteStride = sizeof(glm::vec3);
                }

                size_t positionByteOffset = positionAccessor->byteOffset +
                                            positionBufferView->byteOffset + i * positionByteStride;
                vertex.pos = *reinterpret_cast<const glm::vec3*>(
                    &(gltfModel.buffers[positionBufferView->buffer].data[positionByteOffset]));

                if (normalBufferView) {
                    size_t normalByteStride = normalBufferView->byteStride;
                    if (normalByteStride == 0) {
                        normalByteStride = sizeof(glm::vec3);
                    }
                    size_t normalByteOffset = normalAccessor->byteOffset +
                                              normalBufferView->byteOffset + i * normalByteStride;
                    vertex.normal = *reinterpret_cast<const glm::vec3*>(
                        &(gltfModel.buffers[normalBufferView->buffer].data[normalByteOffset]));
                }

                if (texCoordBufferView) {
                    size_t texCoordByteStride = texCoordBufferView->byteStride;
                    if (texCoordByteStride == 0) {
                        texCoordByteStride = sizeof(glm::vec2);
                    }
                    size_t texCoordByteOffset = texCoordAccessor->byteOffset +
                                                texCoordBufferView->byteOffset +
                                                i * texCoordByteStride;
                    vertex.texCoord = *reinterpret_cast<const glm::vec2*>(
                        &(gltfModel.buffers[texCoordBufferView->buffer].data[texCoordByteOffset]));
                }

                vertices.push_back(vertex);
            }

            // Get indices
            auto& accessor = gltfModel.accessors[gltfPrimitive.indices];
            auto& bufferView = gltfModel.bufferViews[accessor.bufferView];
            auto& buffer = gltfModel.buffers[bufferView.buffer];

            size_t indicesCount = accessor.count;
            switch (accessor.componentType) {
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                    uint32_t* buf = new uint32_t[indicesCount];
                    size_t size = indicesCount * sizeof(uint32_t);
                    memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], size);
                    for (size_t i = 0; i < indicesCount; i++) {
                        indices.push_back(buf[i]);
                    }
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                    uint16_t* buf = new uint16_t[indicesCount];
                    size_t size = indicesCount * sizeof(uint16_t);
                    memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], size);
                    for (size_t i = 0; i < indicesCount; i++) {
                        indices.push_back(buf[i]);
                    }
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                    uint8_t* buf = new uint8_t[indicesCount];
                    size_t size = indicesCount * sizeof(uint8_t);
                    memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], size);
                    for (size_t i = 0; i < indicesCount; i++) {
                        indices.push_back(buf[i]);
                    }
                    break;
                }
                default:
                    std::cerr << "Index component type " << accessor.componentType
                              << " not supported!" << std::endl;
                    return;
            }

            std::vector<Primitive> primitives(1);
            if (gltfPrimitive.material != -1) {
                primitives[0].material = &materials[gltfPrimitive.material];
            }
            primitives[0].firstIndex = 0;
            primitives[0].indexCount = static_cast<uint32_t>(indices.size());
            primitives[0].vertexCount = static_cast<uint32_t>(vertices.size());
            meshData.emplace_back(context, vertices, indices, primitives, gltfMesh.name);
        }
    }

    void loadNodes(tinygltf::Model& gltfModel) {
        for (auto& gltfNode : gltfModel.nodes) {
            // if (gltfNode.camera != -1) {
            //     if (!gltfNode.translation.empty()) {
            //         cameraTranslation = glm::vec3{gltfNode.translation[0],
            //         -gltfNode.translation[1],
            //                                       gltfNode.translation[2]};
            //     }
            //     if (!gltfNode.rotation.empty()) {
            //         cameraRotation = glm::quat{static_cast<float>(gltfNode.rotation[3]),
            //                                    static_cast<float>(-gltfNode.rotation[0]),
            //                                    static_cast<float>(gltfNode.rotation[1]),
            //                                    static_cast<float>(gltfNode.rotation[2])};
            //     }

            //    tinygltf::Camera camera = gltfModel.cameras[gltfNode.camera];
            //    cameraYFov = static_cast<float>(camera.perspective.yfov);
            //    cameraExists = true;
            //    nodes.push_back(Node{});
            //    continue;
            //}

            // if (gltfNode.skin != -1) {
            //     nodes.push_back(Node{});
            //     continue;
            // }

            if (gltfNode.mesh != -1) {
                std::string name = gltfNode.name;
                if (name.empty()) {
                    name = std::format("Object {}", objects.size());
                }

                objects.emplace_back(name);
                Object& obj = objects.back();

                Mesh& mesh = obj.add<Mesh>();
                mesh.meshData = &meshData[gltfNode.mesh];

                Transform& trans = obj.add<Transform>();

                if (!gltfNode.translation.empty()) {
                    trans.translation = glm::vec3{gltfNode.translation[0],  //
                                                  gltfNode.translation[1],  //
                                                  gltfNode.translation[2]};
                }

                if (!gltfNode.rotation.empty()) {
                    trans.rotation = glm::quat{static_cast<float>(gltfNode.rotation[3]),
                                               static_cast<float>(gltfNode.rotation[0]),
                                               static_cast<float>(gltfNode.rotation[1]),
                                               static_cast<float>(gltfNode.rotation[2])};
                }

                if (!gltfNode.scale.empty()) {
                    trans.scale = glm::vec3{gltfNode.scale[0],  //
                                            gltfNode.scale[1],  //
                                            gltfNode.scale[2]};
                }
            }
        }
    }

    void loadFromJson(const rv::Context& context, const std::filesystem::path& filepath) {
        // TODO: cameraの初期化後にViewportかSwapchainのアスペクト比を取得
        // TODO: レンダラー側のバッファのクリア
        context.getDevice().waitIdle();
        clear();

        std::ifstream jsonFile(filepath);
        if (!jsonFile.is_open()) {
            throw std::runtime_error("Failed to open scene file.");
        }
        nlohmann::json json;
        jsonFile >> json;

        if (json.contains("texturesCube")) {
            for (auto& _texture : json["texturesCube"]) {
                std::filesystem::path sceneDir = filepath.parent_path();
                std::filesystem::path texturePath =
                    sceneDir / std::filesystem::path{std::string{_texture}};

                Texture texture{};
                texture.name = texturePath.filename().string();
                texture.filepath = texturePath.string();
                texture.image = rv::Image::loadFromKTX(context, texturePath.string());

                // TODO: アイコンサポート
                assert(texture.image->getViewType() == vk::ImageViewType::eCube);
                texturesCube.push_back(std::move(texture));
                status |= rv::SceneStatus::TextureCubeAdded;
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
                // auto& mesh = obj.add<Mesh>(meshes[object["mesh"]]);
                auto& mesh = obj.add<Mesh>();
                if (object["mesh"] == "Cube") {
                    mesh.meshData = &templateMeshData[static_cast<int>(MeshType::Cube)];
                } else if (object["mesh"] == "Plane") {
                    mesh.meshData = &templateMeshData[static_cast<int>(MeshType::Plane)];
                }

                if (object.contains("material")) {
                    mesh.meshData->primitives[0].material = &materials[object["material"]];
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
                if (object.contains("texturesCube")) {
                    light.textureCube = object["texturesCube"];
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

    MeshData& getCubeMesh() {
        return templateMeshData[static_cast<int>(MeshType::Cube)];
    }

    std::vector<MeshData>& getMeshData() {
        return meshData;
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
        meshData.clear();
        materials.clear();
        textures2D.clear();
        texturesCube.clear();
        status = rv::SceneStatus::Cleared;
    }

private:
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

    rv::SceneStatusFlags status = rv::SceneStatus::None;
};
