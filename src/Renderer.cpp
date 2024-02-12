#include "Renderer.hpp"

void Renderer::init(const rv::Context& _context,
                    RenderImages& images,
                    vk::Format targetColorFormat) {
    context = &_context;

    shadowMapImage = context->createImage({
        .usage = rv::ImageUsage::DepthAttachment | vk::ImageUsageFlagBits::eSampled,
        .extent = shadowMapExtent,
        .format = shadowMapFormat,
        .debugName = "ShadowMapPass::depthImage",
    });
    shadowMapImage->createImageView(vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eDepth);
    shadowMapImage->createSampler();

    sceneUniformBuffer = context->createBuffer({
        .usage = rv::BufferUsage::Uniform,
        .memory = rv::MemoryUsage::Device,
        .size = sizeof(SceneData),
        .debugName = "Renderer::sceneUniformBuffer",
    });

    objectStorage.resize(maxObjectCount);
    objectStorageBuffer = context->createBuffer({
        .usage = rv::BufferUsage::Storage,
        .memory = rv::MemoryUsage::Device,
        .size = sizeof(ObjectData) * objectStorage.size(),
        .debugName = "Renderer::objectStorageBuffer",
    });

    // シェーダリフレクションのために適当なシェーダを作成する
    // TODO: DescSetに合わせ、全てのシェーダを一か所で管理する
    rv::ShaderHandle reflectionShaderVert = context->createShader({
        .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "standard.vert",
                                                  DEV_SHADER_DIR / "spv/standard.vert.spv"),
        .stage = vk::ShaderStageFlagBits::eVertex,
    });
    rv::ShaderHandle reflectionShaderFrag = context->createShader({
        .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "standard.frag",
                                                  DEV_SHADER_DIR / "spv/standard.frag.spv"),
        .stage = vk::ShaderStageFlagBits::eFragment,
    });

    brdfLutTexture =
        rv::Image::loadFromFile(*context, DEV_ASSET_DIR / "environments" / "tex_brdflut.png", 1,
                                vk::Filter::eLinear, vk::SamplerAddressMode::eClampToEdge);

    dummyTextures2D = context->createImage({
        .usage = rv::ImageUsage::Sampled,
        .format = vk::Format::eB8G8R8A8Unorm,
        .debugName = "dummyTextures2D",
    });
    dummyTexturesCube = context->createImage({
        .usage = rv::ImageUsage::Sampled,
        .format = vk::Format::eB8G8R8A8Unorm,
        .debugName = "dummyTextures2D",
    });
    dummyTextures2D->createImageView();
    dummyTextures2D->createSampler();
    dummyTexturesCube->createImageView();
    dummyTexturesCube->createSampler();

    context->oneTimeSubmit([&](rv::CommandBufferHandle commandBuffer) {
        commandBuffer->transitionLayout(shadowMapImage, vk::ImageLayout::eReadOnlyOptimal);
        commandBuffer->transitionLayout(dummyTextures2D, vk::ImageLayout::eReadOnlyOptimal);
        commandBuffer->transitionLayout(dummyTexturesCube, vk::ImageLayout::eReadOnlyOptimal);
    });

    descSet = context->createDescriptorSet({
        .shaders = {reflectionShaderVert, reflectionShaderFrag},
        .buffers =
            {
                {"SceneBuffer", sceneUniformBuffer},
                {"ObjectBuffer", objectStorageBuffer},
            },
        .images =
            {
                {"shadowMap", shadowMapImage},
                {"baseColorImage", images.baseColorImage},
                {"textures2D", 100u},
                {"texturesCube", 100u},
                {"brdfLutTexture", brdfLutTexture},
            },
    });
    descSet->set("textures2D", dummyTextures2D);
    descSet->set("texturesCube", dummyTexturesCube);
    descSet->update();

    try {
        skyboxPass.init(*context, descSet, images.colorFormat);
        shadowMapPass.init(*context, descSet, shadowMapFormat);
        forwardPass.init(*context, descSet, images.colorFormat, images.depthFormat);
        antiAliasingPass.init(*context, descSet, targetColorFormat);
    } catch (const std::exception& e) {
        spdlog::error(e.what());
        std::abort();
    }

    initialized = true;
    firstFrameRendered = false;
}

void Renderer::updateObjectData(const Object& object, uint32_t index) {
    auto* mesh = object.get<Mesh>();
    auto* transform = object.get<Transform>();
    if (!mesh) {
        return;
    }

    // TODO: マテリアル情報はバッファを分けてGPU側でインデックス参照する
    // TODO: materialIndexはpushConstantでもいいかも
    if (Material* material = mesh->material) {
        // clang-format off
            objectStorage[index].baseColor = material->baseColor;
            objectStorage[index].emissive.xyz = material->emissive;
            objectStorage[index].metallic = material->metallic;
            objectStorage[index].roughness = material->roughness;
            objectStorage[index].ior = material->ior;
            objectStorage[index].baseColorTextureIndex = material->baseColorTextureIndex;
            objectStorage[index].metallicRoughnessTextureIndex = material->metallicRoughnessTextureIndex;
            objectStorage[index].normalTextureIndex = material->normalTextureIndex;
            objectStorage[index].occlusionTextureIndex = material->occlusionTextureIndex;
            objectStorage[index].emissiveTextureIndex = material->emissiveTextureIndex;
            objectStorage[index].enableNormalMapping = static_cast<int>(material->enableNormalMapping);
        // clang-format on
    }
    if (transform) {
        const auto& model = transform->computeTransformMatrix();
        objectStorage[index].modelMatrix = model;
        objectStorage[index].normalMatrix = transform->computeNormalMatrix();
    }
}

void Renderer::updateBuffers(const rv::CommandBuffer& commandBuffer,
                             vk::Extent3D extent,
                             Scene& scene) {
    // Update buffer
    // NOTE: Shadow map用の行列も更新するのでShadow map passより先に計算
    Camera& camera = scene.getCamera();
    sceneUniform.cameraView = camera.getView();
    sceneUniform.cameraProj = camera.getProj();
    sceneUniform.cameraViewProj = camera.getProj() * camera.getView();
    sceneUniform.cameraPos.xyz = camera.getPosition();

    sceneUniform.screenResolution.x = static_cast<float>(extent.width);
    sceneUniform.screenResolution.y = static_cast<float>(extent.height);
    sceneUniform.enableFXAA = static_cast<int>(enableFXAA);
    sceneUniform.exposure = exposure;

    if (Object* dirLightObj = scene.findObject<DirectionalLight>()) {
        DirectionalLight* dirLight = dirLightObj->get<DirectionalLight>();
        sceneUniform.existDirectionalLight = 1;
        sceneUniform.lightDirection.xyz = dirLight->getDirection();
        sceneUniform.lightColorIntensity.xyz = dirLight->color;
        sceneUniform.lightColorIntensity.w = dirLight->intensity;
        sceneUniform.shadowViewProj = shadowMapPass.getViewProj(*dirLight, scene.getAABB());
        sceneUniform.shadowBias = dirLight->shadowBias;
        sceneUniform.enableShadowMapping = dirLight->enableShadow;
    } else {
        sceneUniform.existDirectionalLight = 0;
        sceneUniform.enableShadowMapping = false;
    }
    if (Object* ambLightObj = scene.findObject<AmbientLight>()) {
        auto* light = ambLightObj->get<AmbientLight>();
        sceneUniform.ambientColorIntensity.xyz = light->color;
        sceneUniform.ambientColorIntensity.w = light->intensity;
        sceneUniform.irradianceTexture = light->irradianceTexture;
        sceneUniform.radianceTexture = light->radianceTexture;
    }

    auto& objects = scene.getObjects();
    for (uint32_t index : scene.getUpdatedObjectIndices()) {
        auto& object = objects[index];
        updateObjectData(object, index);
    }

    // TODO: DeviceHostに変更
    commandBuffer.copyBuffer(sceneUniformBuffer, &sceneUniform);
    commandBuffer.copyBuffer(objectStorageBuffer, objectStorage.data());
    commandBuffer.bufferBarrier(
        {sceneUniformBuffer, objectStorageBuffer},  //
        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics,
        vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead);
}

void Renderer::render(const rv::CommandBuffer& commandBuffer,
                      const rv::ImageHandle& colorImage,
                      RenderImages& images,
                      Scene& scene) {
    assert(initialized);

    bool shouldUpdate = false;
    vk::Extent3D extent = colorImage->getExtent();
    if (extent != images.baseColorImage->getExtent()) {
        context->getDevice().waitIdle();
        uint32_t width = extent.width;
        uint32_t height = extent.height;
        images.createImages(*context, width, height);
        descSet->set("baseColorImage", images.baseColorImage);
        shouldUpdate = true;
    }

    if (scene.getStatus() & SceneStatus::Cleared) {
        sceneUniform = SceneData{};
        std::ranges::fill(objectStorage, ObjectData{});
        descSet->set("textures2D", dummyTextures2D);
        descSet->set("texturesCube", dummyTexturesCube);
        shouldUpdate = true;
    }

    if (!firstFrameRendered || scene.getStatus() & SceneStatus::Texture2DAdded) {
        if (!scene.getTextures2D().empty()) {
            std::vector<rv::ImageHandle> textures2D;
            for (auto& tex : scene.getTextures2D()) {
                textures2D.push_back(tex.image);
            }
            descSet->set("textures2D", textures2D);
            shouldUpdate = true;
            spdlog::info("Update desc set for texture 2D");
        }
    }
    if (!firstFrameRendered || scene.getStatus() & SceneStatus::TextureCubeAdded) {
        if (!scene.getTexturesCube().empty()) {
            std::vector<rv::ImageHandle> texturesCube;
            for (auto& tex : scene.getTexturesCube()) {
                texturesCube.push_back(tex.image);
            }
            descSet->set("texturesCube", texturesCube);
            shouldUpdate = true;
            spdlog::info("Update desc set for texture cube");
        }
    }
    if (shouldUpdate) {
        descSet->update();
    }
    scene.resetStatus();

    updateBuffers(commandBuffer, extent, scene);

    commandBuffer.clearColorImage(colorImage, {0.1f, 0.1f, 0.1f, 1.0f});
    commandBuffer.clearColorImage(images.baseColorImage, {0.1f, 0.1f, 0.1f, 1.0f});
    commandBuffer.clearDepthStencilImage(images.depthImage, 1.0f, 0);
    commandBuffer.imageBarrier({colorImage, images.baseColorImage, images.depthImage},  //
                               vk::PipelineStageFlagBits::eTransfer,
                               vk::PipelineStageFlagBits::eAllGraphics,
                               vk::AccessFlagBits::eTransferWrite,
                               vk::AccessFlagBits::eColorAttachmentWrite |
                                   vk::AccessFlagBits::eDepthStencilAttachmentWrite);

    // Shadow pass
    if (Object* dirLightObj = scene.findObject<DirectionalLight>()) {
        DirectionalLight* dirLight = dirLightObj->get<DirectionalLight>();
        if (dirLight->enableShadow) {
            sceneUniform.enableShadowMapping = 1;
            shadowMapPass.render(commandBuffer, shadowMapImage, scene, *dirLight);
        } else {
            sceneUniform.enableShadowMapping = 0;
        }
    } else {
        sceneUniform.enableShadowMapping = 0;
    }

    // Skybox pass
    if (scene.findObject<AmbientLight>()) {
        skyboxPass.render(commandBuffer, images.baseColorImage, scene.getCubeMesh());
    }

    // Forward pass
    forwardPass.render(commandBuffer, images.baseColorImage, images.depthImage, scene,
                       enableFrustumCulling);

    // AA pass
    antiAliasingPass.render(commandBuffer, images.baseColorImage, colorImage);

    commandBuffer.transitionLayout(colorImage, vk::ImageLayout::eGeneral);

    firstFrameRendered = true;
}
