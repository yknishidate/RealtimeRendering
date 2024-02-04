#pragma once
#include <map>
#include <memory>
#include <typeindex>

#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

#include <reactive/Scene/Camera.hpp>
#include <reactive/Scene/Mesh.hpp>

#include "editor/Enums.hpp"
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

struct Primitive {
    uint32_t firstIndex;
    uint32_t indexCount;
    uint32_t vertexCount;
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

    bool showAttributes() override {
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

                size_t positionByteOffset = positionAccessor->byteOffset +
                                            positionBufferView->byteOffset +
                                            i * positionBufferView->byteStride;
                vertex.pos = *reinterpret_cast<const glm::vec3*>(
                    &(gltfModel.buffers[positionBufferView->buffer].data[positionByteOffset]));

                if (normalBufferView) {
                    size_t normalByteOffset = normalAccessor->byteOffset +
                                              normalBufferView->byteOffset +
                                              i * normalBufferView->byteStride;
                    vertex.normal = *reinterpret_cast<const glm::vec3*>(
                        &(gltfModel.buffers[normalBufferView->buffer].data[normalByteOffset]));
                }

                if (texCoordBufferView) {
                    size_t texCoordByteOffset = texCoordAccessor->byteOffset +
                                                texCoordBufferView->byteOffset +
                                                i * texCoordBufferView->byteStride;
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

            // vertexBuffers.push_back(context.createBuffer({
            //     .usage = BufferUsage::Vertex,
            //     .memory = MemoryUsage::Device,
            //     .size = sizeof(Vertex) * vertices.size(),
            //     .debugName = std::format("vertexBuffers[{}]", vertexBuffers.size()).c_str(),
            // }));
            // indexBuffers.push_back(context.createBuffer({
            //     .usage = BufferUsage::Index,
            //     .memory = MemoryUsage::Device,
            //     .size = sizeof(uint32_t) * indices.size(),
            //     .debugName = std::format("indexBuffers[{}]", indexBuffers.size()).c_str(),
            // }));
            // context.oneTimeSubmit([&](CommandBufferHandle commandBuffer) {
            //     commandBuffer->copyBuffer(vertexBuffers.back(), vertices.data());
            //     commandBuffer->copyBuffer(indexBuffers.back(), indices.data());
            // });

            // vertexCounts.push_back(static_cast<uint32_t>(vertices.size()));
            // triangleCounts.push_back(static_cast<uint32_t>(indices.size() / 3));

            // materialIndices.push_back(gltfPrimitive.material);
            //}
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
                objects.emplace_back(gltfNode.name);
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
        std::ifstream jsonFile(filepath);
        if (!jsonFile.is_open()) {
            throw std::runtime_error("Failed to open scene file.");
        }
        nlohmann::json json;
        jsonFile >> json;

        if (json.contains("textures")) {
            for (auto& _texture : json["textures"]) {
                std::filesystem::path sceneDir = filepath.parent_path();
                std::filesystem::path texturePath =
                    sceneDir / std::filesystem::path{std::string{_texture}};

                Texture texture{};
                texture.name = texturePath.filename().string();
                texture.filepath = texturePath.string();
                texture.image = rv::Image::loadFromKTX(context, texturePath.string());

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
                // auto& mesh = obj.add<Mesh>(meshes[object["mesh"]]);
                auto& mesh = obj.add<Mesh>();
                mesh.meshData = &templateMeshData[object["mesh"]];

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
