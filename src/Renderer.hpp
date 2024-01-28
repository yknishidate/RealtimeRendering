#pragma once
#include <reactive/reactive.hpp>

#include "Scene.hpp"

#include "../shader/standard.glsl"

class Renderer {
public:
    void init(const rv::Context& _context) {
        context = &_context;

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
                    {"Scene", sceneUniformBuffer},
                    {"ObjectBuffer", objectStorageBuffer},
                },
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
    }

    void render(const rv::CommandBuffer& commandBuffer,
                const rv::ImageHandle& colorImage,
                const rv::ImageHandle& depthImage,
                Scene& scene,
                int frame) {
        commandBuffer.clearColorImage(colorImage, {0.0f, 0.0f, 0.0f, 1.0f});
        commandBuffer.clearDepthStencilImage(depthImage, 1.0f, 0);
        commandBuffer.imageBarrier(
            {colorImage, depthImage},  //
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics,
            vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eColorAttachmentWrite);

        // Update buffer
        rv::Camera& camera = scene.getCamera();
        glm::mat4 viewProj = camera.getProj() * camera.getView();

        sceneUniform.viewProj = viewProj;
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
                objectStorage[index].transformMatrix = transform->computeTransformMatrix(frame);
                objectStorage[index].normalMatrix = transform->computeNormalMatrix(frame);
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

        commandBuffer.imageBarrier(
            {colorImage, depthImage},  //
            vk::PipelineStageFlagBits::eAllGraphics, vk::PipelineStageFlagBits::eAllGraphics,
            vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead);

        commandBuffer.endDebugLabel();
    }

    float getRenderingTimeMs() const {
        return timer->elapsedInMilli();
    }

private:
    const rv::Context* context = nullptr;
    StandardConstants standardConstants{};
    rv::DescriptorSetHandle descSet;
    rv::GraphicsPipelineHandle pipeline;
    rv::GPUTimerHandle timer;

    int maxObjectCount = 1000;
    SceneData sceneUniform{};
    std::vector<ObjectData> objectStorage{};
    rv::BufferHandle sceneUniformBuffer;
    rv::BufferHandle objectStorageBuffer;
};
