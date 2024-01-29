#pragma once
#include <reactive/reactive.hpp>

#include "Scene.hpp"

#include "../shader/shadow_map.glsl"
#include "../shader/standard.glsl"

class ShadowMapPass {
public:
    void init(const rv::Context& _context, vk::Format shadowMapFormat) {
        context = &_context;

        std::vector<rv::ShaderHandle> shaders(2);
        shaders[0] = context->createShader({
            .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "shadow_map.vert",
                                                      DEV_SHADER_DIR / "spv/shadow_map.vert.spv"),
            .stage = vk::ShaderStageFlagBits::eVertex,
        });

        shaders[1] = context->createShader({
            .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "shadow_map.frag",
                                                      DEV_SHADER_DIR / "spv/shadow_map.frag.spv"),
            .stage = vk::ShaderStageFlagBits::eFragment,
        });

        descSet = context->createDescriptorSet({
            .shaders = shaders,
        });

        pipeline = context->createGraphicsPipeline({
            .descSetLayout = descSet->getLayout(),
            .pushSize = sizeof(ShadowMapConstants),
            .vertexShader = shaders[0],
            .fragmentShader = shaders[1],
            .vertexStride = sizeof(rv::Vertex),
            .vertexAttributes = rv::Vertex::getAttributeDescriptions(),
            .colorFormats = {},
            .depthFormat = shadowMapFormat,
        });

        timer = context->createGPUTimer({});
        initialized = true;
    }

    void render(const rv::CommandBuffer& commandBuffer,
                rv::ImageHandle shadowMapImage,
                Scene& scene,
                Object& lightObj,
                int frame) {
        assert(initialized);
        vk::Extent3D extent = shadowMapImage->getExtent();
        commandBuffer.clearDepthStencilImage(shadowMapImage, 1.0f, 0);
        commandBuffer.transitionLayout(shadowMapImage, vk::ImageLayout::eDepthAttachmentOptimal);

        commandBuffer.beginDebugLabel("ShadowMapPass::render()");
        commandBuffer.bindDescriptorSet(descSet, pipeline);
        commandBuffer.bindPipeline(pipeline);

        commandBuffer.setViewport(extent.width, extent.height);
        commandBuffer.setScissor(extent.width, extent.height);
        commandBuffer.beginTimestamp(timer);
        commandBuffer.beginRendering(rv::ImageHandle{}, shadowMapImage, {0, 0},
                                     {extent.width, extent.height});

        DirectionalLight* light = lightObj.get<DirectionalLight>();
        glm::mat4 proj = glm::ortho<float>(-10, 10, -10, 10, -10, 20);
        glm::mat4 view = glm::lookAt(light->getDirection(), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        shadowViewProj = proj * view;
        auto& objects = scene.getObjects();
        for (auto& object : objects) {
            if (Mesh* mesh = object.get<Mesh>()) {
                Transform* transform = object.get<Transform>();
                const auto& model = transform->computeTransformMatrix(frame);
                constants.mvp = shadowViewProj * model;
                commandBuffer.pushConstants(pipeline, &constants);
                commandBuffer.drawIndexed(mesh->mesh->vertexBuffer, mesh->mesh->indexBuffer,
                                          mesh->mesh->getIndicesCount());
            }
        }

        commandBuffer.endRendering();
        commandBuffer.endTimestamp(timer);
        commandBuffer.transitionLayout(shadowMapImage, vk::ImageLayout::eReadOnlyOptimal);
        commandBuffer.endDebugLabel();
    }

    float getRenderingTimeMs() const {
        assert(initialized);
        return timer->elapsedInMilli();
    }

    glm::mat4 getBiasedViewProj() const {
        assert(initialized);
        return biasMatrix * shadowViewProj;
    }

private:
    bool initialized = false;
    const rv::Context* context = nullptr;
    ShadowMapConstants constants{};

    rv::DescriptorSetHandle descSet;
    rv::GraphicsPipelineHandle pipeline;
    rv::GPUTimerHandle timer;

    glm::mat4 shadowViewProj{};

    // NOTE:
    // x =        remap(x, (-1, +1), (0, 1))
    // y = invert(remap(y, (-1, +1), (0, 1)))
    // z = z
    static constexpr glm::mat4 biasMatrix{0.5f, 0.0f,  0.0f, 0.0f,  //
                                          0.0f, -0.5f, 0.0f, 0.0f,  //
                                          0.0f, 0.0f,  1.0f, 0.0f,  //
                                          0.5f, 0.5f,  0.0f, 1.0f};
};

class Renderer {
public:
    void init(const rv::Context& _context) {
        context = &_context;

        shadowMapPass.init(*context, shadowMapFormat);

        shadowMapImage = context->createImage({
            .usage = rv::ImageUsage::DepthAttachment | vk::ImageUsageFlagBits::eSampled,
            .extent = shadowMapExtent,
            .format = shadowMapFormat,
            .aspect = vk::ImageAspectFlagBits::eDepth,
            .debugName = "ShadowMapPass::depthImage",
        });

        context->oneTimeSubmit([&](rv::CommandBufferHandle commandBuffer) {
            commandBuffer->transitionLayout(shadowMapImage, vk::ImageLayout::eReadOnlyOptimal);
        });

        std::vector<rv::ShaderHandle> shaders(2);
        shaders[0] = context->createShader({
            .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "standard.vert",
                                                      DEV_SHADER_DIR / "spv/standard.vert.spv"),
            .stage = vk::ShaderStageFlagBits::eVertex,
        });

        shaders[1] = context->createShader({
            .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "standard.frag",
                                                      DEV_SHADER_DIR / "spv/standard.frag.spv"),
            .stage = vk::ShaderStageFlagBits::eFragment,
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

        descSet = context->createDescriptorSet({
            .shaders = shaders,
            .buffers =
                {
                    {"SceneBuffer", sceneUniformBuffer},
                    {"ObjectBuffer", objectStorageBuffer},
                },
            .images = {{"shadowMap", shadowMapImage}},
        });

        pipeline = context->createGraphicsPipeline({
            .descSetLayout = descSet->getLayout(),
            .pushSize = sizeof(StandardConstants),
            .vertexShader = shaders[0],
            .fragmentShader = shaders[1],
            .vertexStride = sizeof(rv::Vertex),
            .vertexAttributes = rv::Vertex::getAttributeDescriptions(),
            .colorFormats = vk::Format::eR8G8B8A8Unorm,
        });

        timer = context->createGPUTimer({});
        initialized = true;
    }

    void render(const rv::CommandBuffer& commandBuffer,
                const rv::ImageHandle& colorImage,
                const rv::ImageHandle& depthImage,
                Scene& scene,
                int frame) {
        assert(initialized);
        if (Object* lightObj = scene.findObject<DirectionalLight>()) {
            sceneUniform.existDirectionalLight = 1;
            sceneUniform.enableShadowMapping = 1;
            shadowMapPass.render(commandBuffer, shadowMapImage, scene, *lightObj, frame);
        }

        commandBuffer.clearColorImage(colorImage, {0.0f, 0.0f, 0.0f, 1.0f});
        commandBuffer.clearDepthStencilImage(depthImage, 1.0f, 0);
        commandBuffer.imageBarrier({colorImage, depthImage},  //
                                   vk::PipelineStageFlagBits::eTransfer,
                                   vk::PipelineStageFlagBits::eAllGraphics,
                                   vk::AccessFlagBits::eTransferWrite,
                                   vk::AccessFlagBits::eColorAttachmentWrite |
                                       vk::AccessFlagBits::eDepthStencilAttachmentWrite);

        // Update buffer
        rv::Camera& camera = scene.getCamera();
        glm::mat4 viewProj = camera.getProj() * camera.getView();

        if (const Object* obj = scene.findObject<DirectionalLight>()) {
            const DirectionalLight* light = obj->get<DirectionalLight>();
            sceneUniform.lightDirection.xyz = light->getDirection();
            sceneUniform.lightColorIntensity.xyz = light->color * light->intensity;
        }
        sceneUniform.ambientColorIntensity.xyz = glm::vec3{0.0f};

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
                objectStorage[index].mvpMatrix = viewProj * model;
                objectStorage[index].normalMatrix = transform->computeNormalMatrix(frame);
                if (sceneUniform.enableShadowMapping) {
                    objectStorage[index].biasedShadowMatrix =
                        shadowMapPass.getBiasedViewProj() * model;
                }
            }
        }

        commandBuffer.copyBuffer(sceneUniformBuffer, &sceneUniform);
        commandBuffer.copyBuffer(objectStorageBuffer, objectStorage.data());

        // Draw scene
        commandBuffer.beginDebugLabel("Renderer::render()");
        commandBuffer.bindDescriptorSet(descSet, pipeline);
        commandBuffer.bindPipeline(pipeline);
        commandBuffer.transitionLayout(colorImage, vk::ImageLayout::eGeneral);

        vk::Extent3D extent = colorImage->getExtent();
        commandBuffer.setViewport(extent.width, extent.height);
        commandBuffer.setScissor(extent.width, extent.height);
        commandBuffer.beginTimestamp(timer);
        commandBuffer.beginRendering(colorImage, depthImage, {0, 0}, {extent.width, extent.height});

        for (int index = 0; index < objects.size(); index++) {
            auto& object = objects[index];
            Mesh* mesh = object.get<Mesh>();
            if (!mesh) {
                continue;
            }
            if (mesh->mesh) {
                standardConstants.objectIndex = index;
                commandBuffer.pushConstants(pipeline, &standardConstants);
                commandBuffer.drawIndexed(mesh->mesh->vertexBuffer, mesh->mesh->indexBuffer,
                                          mesh->mesh->getIndicesCount());
            }
        }

        commandBuffer.endRendering();
        commandBuffer.endTimestamp(timer);

        commandBuffer.imageBarrier({colorImage, depthImage},  //
                                   vk::PipelineStageFlagBits::eAllGraphics,
                                   vk::PipelineStageFlagBits::eAllGraphics,
                                   vk::AccessFlagBits::eColorAttachmentWrite |
                                       vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                                   vk::AccessFlagBits::eShaderRead);

        commandBuffer.endDebugLabel();
    }

    float getRenderingTimeMs() const {
        assert(initialized);
        return timer->elapsedInMilli();
    }

    rv::ImageHandle getShadowMap() const {
        return shadowMapImage;
    }

private:
    // NOTE:
    // RendererはSwapchainに直接書きこむか
    // Viewport用の画像に書きこむか分からなくてもいいように
    // 外部からアタッチメントを受け取る

    // NOTE:
    // 内部で利用するバッファやイメージはRendererが一元管理する

    bool initialized = false;
    const rv::Context* context = nullptr;
    StandardConstants standardConstants{};
    rv::DescriptorSetHandle descSet;
    rv::GraphicsPipelineHandle pipeline;
    rv::GPUTimerHandle timer;

    // Standard pass
    int maxObjectCount = 1000;
    SceneData sceneUniform{};
    std::vector<ObjectData> objectStorage{};
    rv::BufferHandle sceneUniformBuffer;
    rv::BufferHandle objectStorageBuffer;

    // Shadow map pass
    ShadowMapPass shadowMapPass;
    vk::Format shadowMapFormat = vk::Format::eD32Sfloat;
    vk::Extent3D shadowMapExtent{1024, 1024, 1};
    rv::ImageHandle shadowMapImage;
};
