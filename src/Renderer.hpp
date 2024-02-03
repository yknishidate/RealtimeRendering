#pragma once
#include "Pass.hpp"

class Renderer {
public:
    void init(const rv::Context& _context, RenderImages& images) {
        context = &_context;

        shadowMapImage = context->createImage({
            .usage = rv::ImageUsage::DepthAttachment | vk::ImageUsageFlagBits::eSampled,
            .extent = shadowMapExtent,
            .format = shadowMapFormat,
            .aspect = vk::ImageAspectFlagBits::eDepth,
            .debugName = "ShadowMapPass::depthImage",
        });

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

        dummyTextures2D = context->createImage({
            .usage = rv::ImageUsage::Sampled,
            .format = vk::Format::eB8G8R8A8Unorm,
            .debugName = "dummyTextures2D",
        });
        dummyTexturesCube = context->createImage({
            .usage = rv::ImageUsage::Sampled,
            .format = vk::Format::eB8G8R8A8Unorm,
            .isCubemap = true,
            .debugName = "dummyTextures2D",
        });
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
                    {"textures2D", dummyTextures2D},
                    {"texturesCube", dummyTexturesCube},
                },
        });

        forwardPass.init(*context, descSet, images.colorFormat, images.depthFormat);
        shadowMapPass.init(*context, descSet, shadowMapFormat);
        antiAliasingPass.init(*context, descSet, images.colorFormat);

        initialized = true;
    }

    void render(const rv::CommandBuffer& commandBuffer,
                const rv::ImageHandle& colorImage,
                RenderImages& images,
                Scene& scene,
                int frame) {
        assert(initialized);

        vk::Extent3D extent = colorImage->getExtent();
        if (extent != images.baseColorImage->getExtent()) {
            context->getDevice().waitIdle();
            uint32_t width = extent.width;
            uint32_t height = extent.height;
            images.createImages(*context, width, height);
            descSet->set("baseColorImage", images.baseColorImage);
            descSet->update();
            scene.getCamera().setAspect(static_cast<float>(width) / static_cast<float>(height));
        }

        if (scene.getStatus() & rv::SceneStatus::Texture2DAdded) {
            assert(!scene.getTextures2D().empty());
            std::vector<rv::ImageHandle> textures2D;
            for (auto& tex : scene.getTextures2D()) {
                textures2D.push_back(tex.image);
            }
            descSet->set("textures2D", textures2D);
            descSet->update();
        }
        if (scene.getStatus() & rv::SceneStatus::TextureCubeAdded) {
            assert(!scene.getTexturesCube().empty());
            std::vector<rv::ImageHandle> texturesCube;
            for (auto& tex : scene.getTexturesCube()) {
                texturesCube.push_back(tex.image);
            }
            descSet->set("texturesCube", texturesCube);
            descSet->update();
        }
        scene.resetStatus();

        commandBuffer.clearColorImage(colorImage, {0.1f, 0.1f, 0.1f, 1.0f});
        commandBuffer.clearColorImage(images.baseColorImage, {0.1f, 0.1f, 0.1f, 1.0f});
        commandBuffer.clearDepthStencilImage(images.depthImage, 1.0f, 0);
        commandBuffer.imageBarrier({colorImage, images.baseColorImage, images.depthImage},  //
                                   vk::PipelineStageFlagBits::eTransfer,
                                   vk::PipelineStageFlagBits::eAllGraphics,
                                   vk::AccessFlagBits::eTransferWrite,
                                   vk::AccessFlagBits::eColorAttachmentWrite |
                                       vk::AccessFlagBits::eDepthStencilAttachmentWrite);

        // Update buffer
        // NOTE: Shadow map用の行列も更新するのでShadow map passより先に計算
        rv::Camera& camera = scene.getCamera();
        sceneUniform.cameraViewProj = camera.getProj() * camera.getView();

        sceneUniform.screenResolution.x = static_cast<float>(extent.width);
        sceneUniform.screenResolution.y = static_cast<float>(extent.height);
        sceneUniform.enableFXAA = static_cast<int>(enableFXAA);

        Object* dirLightObj = scene.findObject<DirectionalLight>();
        const DirectionalLight* dirLight = dirLightObj->get<DirectionalLight>();
        if (dirLightObj) {
            sceneUniform.existDirectionalLight = 1;
            sceneUniform.lightDirection.xyz = dirLight->getDirection();
            sceneUniform.lightColorIntensity.xyz = dirLight->color * dirLight->intensity;
            sceneUniform.shadowViewProj = shadowMapPass.getViewProj(*dirLight, scene.getAABB());
            sceneUniform.shadowBias = dirLight->shadowBias;
        }
        if (Object* ambLightObj = scene.findObject<AmbientLight>()) {
            auto* light = ambLightObj->get<AmbientLight>();
            sceneUniform.ambientColorIntensity.xyz = light->color * light->intensity;
        }

        auto& objects = scene.getObjects();
        for (size_t index = 0; index < objects.size(); index++) {
            auto& object = objects[index];
            auto* mesh = object.get<Mesh>();
            auto* transform = object.get<Transform>();
            if (!mesh) {
                continue;
            }
            if (Material* material = mesh->material) {
                objectStorage[index].baseColor = material->baseColor;
            }
            if (transform) {
                const auto& model = transform->computeTransformMatrix(frame);
                objectStorage[index].modelMatrix = model;
                objectStorage[index].normalMatrix = transform->computeNormalMatrix(frame);
            }
        }

        // TODO: DeviceHostに変更
        commandBuffer.copyBuffer(sceneUniformBuffer, &sceneUniform);
        commandBuffer.copyBuffer(objectStorageBuffer, objectStorage.data());
        commandBuffer.bufferBarrier(
            {sceneUniformBuffer, objectStorageBuffer},  //
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics,
            vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead);

        // Shadow pass
        if (dirLightObj && dirLight->enableShadow) {
            sceneUniform.enableShadowMapping = 1;
            shadowMapPass.render(commandBuffer, shadowMapImage, scene, *dirLight);
        } else {
            sceneUniform.enableShadowMapping = 0;
        }

        // Forward pass
        forwardPass.render(commandBuffer, images.baseColorImage, images.depthImage, objects);

        // AA pass
        antiAliasingPass.render(commandBuffer, images.baseColorImage, colorImage);

        commandBuffer.transitionLayout(colorImage, vk::ImageLayout::eGeneral);

        firstFrameRendered = true;
    }

    std::vector<std::pair<std::string, float>> getRenderTimes() {
        float shadowTime = shadowMapPass.getRenderingTimeMs();
        float forwardTime = forwardPass.getRenderingTimeMs();
        float aaTime = antiAliasingPass.getRenderingTimeMs();
        return {
            {"GPU time", shadowTime + forwardTime + aaTime},
            {"  Shadow map", shadowTime},
            {"  Forward", forwardTime},
            {"  FXAA", aaTime},
        };
    }

    rv::ImageHandle getShadowMap() const {
        return shadowMapImage;
    }

    // Global options
    inline static bool enableFXAA = true;

private:
    bool initialized = false;
    bool firstFrameRendered = false;
    const rv::Context* context = nullptr;

    rv::DescriptorSetHandle descSet;

    // Buffer
    int maxObjectCount = 1000;
    SceneData sceneUniform{};
    std::vector<ObjectData> objectStorage{};
    rv::BufferHandle sceneUniformBuffer;
    rv::BufferHandle objectStorageBuffer;

    // Image
    rv::ImageHandle dummyTextures2D;
    rv::ImageHandle dummyTexturesCube;

    // Shadow map pass
    ShadowMapPass shadowMapPass;
    vk::Format shadowMapFormat = vk::Format::eD32Sfloat;
    vk::Extent3D shadowMapExtent{1024, 1024, 1};
    rv::ImageHandle shadowMapImage;

    // Forward pass
    ForwardPass forwardPass;

    // AA pass
    AntiAliasingPass antiAliasingPass;
};
