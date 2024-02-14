#define TINYGLTF_IMPLEMENTATION
#include "Scene.hpp"

void Scene::init(const rv::Context& _context) {
    context = &_context;

    objects.reserve(maxObjectCount);

    int count = static_cast<int>(MeshType::COUNT);
    templateMeshData.reserve(count);
    for (int type = 0; type < count; type++) {
        templateMeshData.emplace_back(*context, static_cast<MeshType>(type));
    }
}

Object& Scene::addObject(const std::string& name) {
    assert(objects.size() < maxObjectCount);

    int index = 0;
    std::string newName = name;
    while (findObject(newName)) {
        newName = name + std::format(" {}", index++);
    }

    objects.emplace_back(std::move(newName));
    return objects.back();
}

void Scene::loadFromGltf(const std::filesystem::path& filepath) {
    context->getDevice().waitIdle();
    clear();

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    auto extension = filepath.extension();
    bool ret = false;
    if (extension == ".gltf") {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath.string());
    } else if (extension == ".glb") {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filepath.string());
    }
    if (!warn.empty()) {
        std::cerr << "Warn: " << warn.c_str() << std::endl;
    }
    if (!err.empty()) {
        std::cerr << "Err: " << err.c_str() << std::endl;
    }
    if (!ret) {
        throw std::runtime_error("Failed to parse glTF: " + filepath.string());
    }

    loadTextures(model);
    loadMaterials(model);
    loadNodes(model);
    spdlog::info("Loaded glTF file: {}", filepath.string());
    spdlog::info("  Texture: {}", textures2D.size());
    spdlog::info("  Material: {}", materials.size());
    spdlog::info("  Node: {}", objects.size());
}

void Scene::loadTextures(tinygltf::Model& gltfModel) {
    for (size_t i = 0; i < gltfModel.textures.size(); ++i) {
        const tinygltf::Texture& texture = gltfModel.textures[i];

        // texture.source はイメージのインデックスを指す
        if (texture.source >= 0) {
            const tinygltf::Image& image = gltfModel.images[texture.source];
            textures2D.push_back({});
            Texture& tex = textures2D.back();
            tex.name = image.name;
            if (tex.name.empty()) {
                tex.name = std::format("Image {}", textures2D.size());
            }

            // TODO:
            // 本来ならここでUnormかSrgbかを正しく指定することで、シェーダ側での色空間変換を省略するべき
            // ただし、Texture本体には色空間の情報はなく、マテリアル側から指定されるため、
            // 読み込みを遅延する必要がある
            tex.image = context->createImage({
                .usage = rv::ImageUsage::Sampled,
                .extent = {static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height),
                           1},
                .format = vk::Format::eR8G8B8A8Unorm,
                .debugName = tex.name,
            });
            tex.image->createImageView();
            tex.image->createSampler();

            rv::BufferHandle buffer = context->createBuffer({
                .usage = rv::BufferUsage::Staging,
                .memory = rv::MemoryUsage::Host,
                .size = image.image.size(),
                .debugName = "Scene::loadTextures::buffer",
            });

            buffer->copy(image.image.data());

            context->oneTimeSubmit([&](auto commandBuffer) {
                commandBuffer->transitionLayout(tex.image, vk::ImageLayout::eTransferDstOptimal);
                commandBuffer->copyBufferToImage(buffer, tex.image);
                commandBuffer->transitionLayout(tex.image, vk::ImageLayout::eShaderReadOnlyOptimal);
            });

            IconManager::addIcon(tex.name, tex.image);

            status |= SceneStatus::Texture2DAdded;
        }
    }
}

void Scene::loadMaterials(tinygltf::Model& gltfModel) {
    for (auto& mat : gltfModel.materials) {
        Material material;

        // Base color
        if (mat.values.contains("baseColorTexture")) {
            material.baseColorTextureIndex = mat.values["baseColorTexture"].TextureIndex();
        }
        if (mat.values.contains("baseColorFactor")) {
            material.baseColor = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
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
        } else {
            // glTF では metallicFactor が含まれない場合、デフォルトで 1 となる
            // struct 側ではデフォルトで 0 にしておきたいためここで処理
            material.metallic = 1.0f;
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
            material.emissiveTextureIndex = mat.additionalValues["emissiveTexture"].TextureIndex();
        }

        // Occlusion
        if (mat.additionalValues.contains("occlusionTexture")) {
            material.occlusionTextureIndex =
                mat.additionalValues["occlusionTexture"].TextureIndex();
        }

        materials.push_back(material);
    }
}

void Scene::loadMesh(tinygltf::Model& gltfModel, tinygltf::Primitive& gltfPrimitive, Mesh& mesh) {
    // WARN: Since different attributes may refer to the same data, creating a
    // vertex/index buffer for each attribute will result in data duplication.
    auto& vertices = meshData.vertices;
    auto& indices = meshData.indices;

    size_t vertexOffset = vertices.size();
    size_t indexOffset = indices.size();

    // Vertex attributes
    auto& attributes = gltfPrimitive.attributes;

    assert(attributes.contains("POSITION"));
    int positionIndex = attributes.find("POSITION")->second;
    tinygltf::Accessor* positionAccessor = &gltfModel.accessors[positionIndex];
    tinygltf::BufferView* positionBufferView = &gltfModel.bufferViews[positionAccessor->bufferView];

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

    bool hasTangent = false;
    tinygltf::Accessor* tangentAccessor = nullptr;
    tinygltf::BufferView* tangentBufferView = nullptr;
    if (attributes.contains("TANGENT")) {
        hasTangent = true;
        int tangentIndex = attributes.find("TANGENT")->second;
        tangentAccessor = &gltfModel.accessors[tangentIndex];
        tangentBufferView = &gltfModel.bufferViews[tangentAccessor->bufferView];
    }

    // Loop over the vertices
    for (size_t i = 0; i < positionAccessor->count; i++) {
        VertexPNUT vertex{};

        // NOTE:
        // byteStride が 0 の場合、データは密に詰まっている
        size_t positionByteStride = positionBufferView->byteStride;
        if (positionByteStride == 0) {
            positionByteStride = sizeof(glm::vec3);
        }

        size_t positionByteOffset =
            positionAccessor->byteOffset + positionBufferView->byteOffset + i * positionByteStride;
        vertex.position = *reinterpret_cast<const glm::vec3*>(
            &(gltfModel.buffers[positionBufferView->buffer].data[positionByteOffset]));

        if (normalBufferView) {
            size_t normalByteStride = normalBufferView->byteStride;
            if (normalByteStride == 0) {
                normalByteStride = sizeof(glm::vec3);
            }
            size_t normalByteOffset =
                normalAccessor->byteOffset + normalBufferView->byteOffset + i * normalByteStride;
            vertex.normal = *reinterpret_cast<const glm::vec3*>(
                &(gltfModel.buffers[normalBufferView->buffer].data[normalByteOffset]));
        }

        if (texCoordBufferView) {
            size_t texCoordByteStride = texCoordBufferView->byteStride;
            if (texCoordByteStride == 0) {
                texCoordByteStride = sizeof(glm::vec2);
            }
            size_t texCoordByteOffset = texCoordAccessor->byteOffset +
                                        texCoordBufferView->byteOffset + i * texCoordByteStride;
            vertex.texCoord = *reinterpret_cast<const glm::vec2*>(
                &(gltfModel.buffers[texCoordBufferView->buffer].data[texCoordByteOffset]));
        }

        if (tangentAccessor) {
            size_t tangentByteStride = tangentBufferView->byteStride;
            if (tangentByteStride == 0) {
                tangentByteStride = sizeof(glm::vec4);
            }
            size_t tangentByteOffset =
                tangentAccessor->byteOffset + tangentBufferView->byteOffset + i * tangentByteStride;
            vertex.tangent = *reinterpret_cast<const glm::vec4*>(
                &(gltfModel.buffers[tangentBufferView->buffer].data[tangentByteOffset]));
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
            std::cerr << "Index component type " << accessor.componentType << " not supported!"
                      << std::endl;
            return;
    }

    mesh.meshData = &meshData;
    if (gltfPrimitive.material != -1) {
        mesh.material = &materials[gltfPrimitive.material];
        mesh.material->enableNormalMapping = hasTangent && mesh.material->normalTextureIndex != -1;
    }
    mesh.firstIndex = static_cast<uint32_t>(indexOffset);
    mesh.vertexOffset = static_cast<uint32_t>(vertexOffset);
    mesh.indexCount = static_cast<uint32_t>(indices.size() - indexOffset);
    mesh.vertexCount = static_cast<uint32_t>(vertices.size() - vertexOffset);
    mesh.computeLocalAABB();
}

void Scene::loadNodes(tinygltf::Model& gltfModel) {
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
            auto& gltfMesh = gltfModel.meshes.at(gltfNode.mesh);

            for (auto& gltfPrimitive : gltfMesh.primitives) {
                std::string name = gltfMesh.name;
                if (name.empty()) {
                    name = std::format("Object {}", objects.size());
                }

                objects.emplace_back(name);
                Object& obj = objects.back();

                // Mesh
                Mesh& mesh = obj.add<Mesh>();
                loadMesh(gltfModel, gltfPrimitive, mesh);

                // Transform
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
    meshData.createBuffers(*context);
}

void Scene::loadFromJson(const std::filesystem::path& filepath) {
    // TODO: cameraの初期化後にViewportかSwapchainのアスペクト比を取得
    // TODO: レンダラー側のバッファのクリア
    context->getDevice().waitIdle();
    clear();

    std::ifstream jsonFile(filepath);
    if (!jsonFile.is_open()) {
        throw std::runtime_error("Failed to open scene file.");
    }
    nlohmann::json json;
    jsonFile >> json;

    if (json.contains("gltf")) {
        std::filesystem::path gltfPath = json["gltf"];
        if (!gltfPath.empty()) {
            loadFromGltf(filepath.parent_path() / gltfPath);
        }
    }

    if (json.contains("texturesCube")) {
        for (auto& _texture : json["texturesCube"]) {
            std::filesystem::path sceneDir = filepath.parent_path();
            std::filesystem::path texturePath =
                sceneDir / std::filesystem::path{std::string{_texture}};

            Texture texture{};
            texture.name = texturePath.filename().string();
            texture.filepath = texturePath.string();
            texture.image = rv::Image::loadFromKTX(*context, texturePath.string());

            // TODO: アイコンサポート
            assert(texture.image->getViewType() == vk::ImageViewType::eCube);
            texturesCube.push_back(std::move(texture));
            status |= SceneStatus::TextureCubeAdded;
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
        status |= SceneStatus::ObjectAdded;

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
            auto& mesh = obj.add<Mesh>();

            if (object["mesh"] == "Cube") {
                mesh.meshData = &templateMeshData[static_cast<int>(MeshType::Cube)];
            } else if (object["mesh"] == "Plane") {
                mesh.meshData = &templateMeshData[static_cast<int>(MeshType::Plane)];
            }
            mesh.firstIndex = 0;
            mesh.indexCount = static_cast<uint32_t>(mesh.meshData->indices.size());
            mesh.vertexCount = static_cast<uint32_t>(mesh.meshData->vertices.size());
            mesh.computeLocalAABB();

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
            if (object.contains("irradianceTexture")) {
                light.irradianceTexture = object["irradianceTexture"];
            }
            if (object.contains("radianceTexture")) {
                light.radianceTexture = object["radianceTexture"];
            }
        } else {
            assert(false && "Not implemented");
        }
    }

    if (json.contains("camera")) {
        const auto& _camera = json["camera"];

        objects.emplace_back("Camera");
        Object& obj = objects.back();
        Camera& camera = obj.add<Camera>();
        mainCamera = &camera;
        isMainCameraActive = true;
        status |= SceneStatus::ObjectAdded;

        if (_camera["type"] == "Orbital") {
            camera.setType(rv::Camera::Type::Orbital);
            if (_camera.contains("target")) {
                auto& target = _camera["target"];
                camera.setTarget({target[0], target[1], target[2]});
            }
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
            camera.setType(rv::Camera::Type::FirstPerson);
        }
        if (_camera.contains("fovY")) {
            camera.setFovY(glm::radians(static_cast<float>(_camera["fovY"])));
        }
    }
}
