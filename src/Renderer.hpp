#pragma once
#include <reactive/reactive.hpp>

#include "RenderImages.hpp"
#include "Scene.hpp"

#include "../shader/standard.glsl"

class ShadowMapPass {
public:
    void init(const rv::Context& _context,
              rv::DescriptorSetHandle _descSet,
              vk::Format shadowMapFormat) {
        context = &_context;
        descSet = _descSet;

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

        pipeline = context->createGraphicsPipeline({
            .descSetLayout = descSet->getLayout(),
            .pushSize = sizeof(StandardConstants),
            .vertexShader = shaders[0],
            .fragmentShader = shaders[1],
            .vertexStride = sizeof(rv::Vertex),
            .vertexAttributes = rv::Vertex::getAttributeDescriptions(),
            .colorFormats = {},
            .depthFormat = shadowMapFormat,
            .cullMode = "dynamic",
        });

        timer = context->createGPUTimer({});
        initialized = true;
    }

    void render(const rv::CommandBuffer& commandBuffer,
                rv::ImageHandle shadowMapImage,
                Scene& scene,
                const DirectionalLight& light) const {
        assert(initialized);
        vk::Extent3D extent = shadowMapImage->getExtent();
        commandBuffer.clearDepthStencilImage(shadowMapImage, 1.0f, 0);
        commandBuffer.transitionLayout(shadowMapImage, vk::ImageLayout::eDepthAttachmentOptimal);

        commandBuffer.beginDebugLabel("ShadowMapPass::render()");
        commandBuffer.bindDescriptorSet(descSet, pipeline);
        commandBuffer.bindPipeline(pipeline);

        commandBuffer.setViewport(extent.width, extent.height);
        commandBuffer.setScissor(extent.width, extent.height);
        commandBuffer.setCullMode(light.enableShadowCulling ? vk::CullModeFlagBits::eFront
                                                            : vk::CullModeFlagBits::eNone);
        commandBuffer.beginTimestamp(timer);
        commandBuffer.beginRendering(rv::ImageHandle{}, shadowMapImage, {0, 0},
                                     {extent.width, extent.height});

        StandardConstants constants;
        auto& objects = scene.getObjects();
        for (size_t i = 0; i < objects.size(); i++) {
            if (Mesh* mesh = objects[i].get<Mesh>()) {
                constants.objectIndex = static_cast<int>(i);
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

    glm::mat4 getViewProj(const DirectionalLight& light, const rv::AABB& aabb) const {
        // Transform AABB to light space
        std::vector<glm::vec3> corners = aabb.getCorners();
        glm::vec3 center = aabb.center;
        glm::vec3 dir = light.getDirection();
        glm::vec3 furthestCorner = aabb.getFurthestCorner(dir);
        float length = glm::dot(furthestCorner, dir);
        glm::mat4 view = glm::lookAt(center, center - dir * length, glm::vec3(0, 1, 0));

        // Initialize bounds
        glm::vec3 minBounds = glm::vec3(FLT_MAX);
        glm::vec3 maxBounds = glm::vec3(-FLT_MAX);

        for (const auto& corner : corners) {
            glm::vec3 transformedCorner = glm::vec3(view * glm::vec4(corner, 1.0f));
            minBounds = glm::min(minBounds, transformedCorner);
            maxBounds = glm::max(maxBounds, transformedCorner);
        }

        // Calculate orthographic projection bounds
        float scaling = 1.05f;
        float left = minBounds.x * scaling;
        float right = maxBounds.x * scaling;
        float bottom = minBounds.y * scaling;
        float top = maxBounds.y * scaling;
        float near = minBounds.z * scaling;
        float far = maxBounds.z * scaling;

        // Create orthographic projection matrix
        glm::mat4 proj = glm::ortho<float>(left, right, bottom, top, near, far);
        return proj * view;
    }

private:
    bool initialized = false;
    const rv::Context* context = nullptr;

    rv::DescriptorSetHandle descSet;
    rv::GraphicsPipelineHandle pipeline;
    rv::GPUTimerHandle timer;
};

class AntiAliasingPass {
public:
    void init(const rv::Context& context, const rv::ImageHandle& srcImage) {
        std::vector<rv::ShaderHandle> shaders(2);
        shaders[0] = context.createShader({
            .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "fullscreen.vert",
                                                      DEV_SHADER_DIR / "spv/fullscreen.vert.spv"),
            .stage = vk::ShaderStageFlagBits::eVertex,
        });

        shaders[1] = context.createShader({
            .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "fxaa.frag",
                                                      DEV_SHADER_DIR / "spv/fxaa.frag.spv"),
            .stage = vk::ShaderStageFlagBits::eFragment,
        });

        descSet = context.createDescriptorSet({
            .shaders = shaders,
            .images = {{"colorImage", srcImage}},
        });

        pipeline = context.createGraphicsPipeline({
            .descSetLayout = descSet->getLayout(),
            .vertexShader = shaders[0],
            .fragmentShader = shaders[1],
            .colorFormats = srcImage->getFormat(),
        });

        timer = context.createGPUTimer({});
        initialized = true;
    }

    void render(const rv::CommandBuffer& commandBuffer,
                const rv::ImageHandle& srcImage,
                const rv::ImageHandle& dstImage) const {
        assert(initialized);
        vk::Extent3D extent = srcImage->getExtent();
        commandBuffer.transitionLayout(srcImage, vk::ImageLayout::eShaderReadOnlyOptimal);
        commandBuffer.transitionLayout(dstImage, vk::ImageLayout::eColorAttachmentOptimal);

        commandBuffer.beginDebugLabel("AntiAliasingPass::render()");
        commandBuffer.bindDescriptorSet(descSet, pipeline);
        commandBuffer.bindPipeline(pipeline);

        commandBuffer.setViewport(extent.width, extent.height);
        commandBuffer.setScissor(extent.width, extent.height);
        commandBuffer.beginTimestamp(timer);
        commandBuffer.beginRendering(dstImage, {}, {0, 0}, {extent.width, extent.height});

        commandBuffer.draw(3, 1, 0, 0);

        commandBuffer.endRendering();
        commandBuffer.endTimestamp(timer);
        // commandBuffer.transitionLayout(srcImage, vk::ImageLayout::eReadOnlyOptimal);
        // commandBuffer.transitionLayout(dstImage, vk::ImageLayout::eReadOnlyOptimal);
        commandBuffer.endDebugLabel();
    }

    float getRenderingTimeMs() const {
        assert(initialized);
        return timer->elapsedInMilli();
    }

private:
    bool initialized = false;
    rv::DescriptorSetHandle descSet;
    rv::GraphicsPipelineHandle pipeline;
    rv::GPUTimerHandle timer;
};

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
            .colorFormats = images.colorFormat,
            .depthFormat = images.depthFormat,
        });

        shadowMapPass.init(*context, descSet, shadowMapFormat);

        timer = context->createGPUTimer({});
        initialized = true;
    }

    void render(const rv::CommandBuffer& commandBuffer,
                const rv::ImageHandle& colorImage,
                RenderImages& images,
                Scene& scene,
                int frame) {
        assert(initialized);

        if (colorImage->getExtent() != images.baseColorImage->getExtent()) {
            context->getDevice().waitIdle();
            uint32_t width = colorImage->getExtent().width;
            uint32_t height = colorImage->getExtent().height;
            images.createImages(*context, width, height);
            scene.getCamera().setAspect(static_cast<float>(width) / static_cast<float>(height));
        }

        commandBuffer.clearColorImage(colorImage, {0.1f, 0.1f, 0.1f, 1.0f});
        commandBuffer.clearDepthStencilImage(images.depthImage, 1.0f, 0);
        commandBuffer.imageBarrier({colorImage, images.depthImage},  //
                                   vk::PipelineStageFlagBits::eTransfer,
                                   vk::PipelineStageFlagBits::eAllGraphics,
                                   vk::AccessFlagBits::eTransferWrite,
                                   vk::AccessFlagBits::eColorAttachmentWrite |
                                       vk::AccessFlagBits::eDepthStencilAttachmentWrite);

        // Update buffer
        // NOTE: Shadow map用の行列も更新するのでShadow map passより先に計算
        rv::Camera& camera = scene.getCamera();
        sceneUniform.cameraViewProj = camera.getProj() * camera.getView();

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

        // Draw scene
        commandBuffer.beginDebugLabel("Renderer::render()");
        commandBuffer.bindDescriptorSet(descSet, pipeline);
        commandBuffer.bindPipeline(pipeline);
        commandBuffer.transitionLayout(colorImage, vk::ImageLayout::eGeneral);

        vk::Extent3D extent = colorImage->getExtent();
        commandBuffer.setViewport(extent.width, extent.height);
        commandBuffer.setScissor(extent.width, extent.height);
        commandBuffer.beginTimestamp(timer);
        commandBuffer.beginRendering(colorImage, images.depthImage, {0, 0},
                                     {extent.width, extent.height});

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

        commandBuffer.imageBarrier({colorImage, images.depthImage},  //
                                   vk::PipelineStageFlagBits::eAllGraphics,
                                   vk::PipelineStageFlagBits::eAllGraphics,
                                   vk::AccessFlagBits::eColorAttachmentWrite |
                                       vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                                   vk::AccessFlagBits::eShaderRead);

        commandBuffer.endDebugLabel();

        firstFrameRendered = true;
    }

    float getRenderingTimeMs() const {
        assert(initialized);
        if (!firstFrameRendered) {
            return 0.0f;
        }
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
    bool firstFrameRendered = false;
    const rv::Context* context = nullptr;
    StandardConstants standardConstants{};
    rv::DescriptorSetHandle descSet;
    rv::GraphicsPipelineHandle pipeline;
    rv::GPUTimerHandle timer;

    // TODO: separate standard pass
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
