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
                const Scene& scene,
                int frame) {
        commandBuffer.clearColorImage(colorImage, {0.0f, 0.0f, 0.0f, 1.0f});
        commandBuffer.clearDepthStencilImage(depthImage, 1.0f, 0);

        // Update buffer
        glm::mat4 viewProj = scene.camera.getProj() * scene.camera.getView();

        sceneUniform.viewProj = viewProj;
        sceneUniform.lightDirection.xyz = glm::normalize(glm::vec3{1, 2, 3});
        sceneUniform.lightColorIntensity.xyz = glm::vec3{1.0f};
        sceneUniform.ambientColorIntensity.xyz = glm::vec3{0.0f};

        for (size_t index = 0; index < scene.objects.size(); index++) {
            auto& object = scene.objects[index];
            if (!object.mesh) {
                continue;
            }
            if (Material* material = object.mesh.value().material) {
                objectStorage[index].baseColor = material->baseColor;
                objectStorage[index].transformMatrix =
                    object.transform->computeTransformMatrix(frame);
                objectStorage[index].normalMatrix = object.transform->computeNormalMatrix(frame);
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

        for (int index = 0; index < scene.objects.size(); index++) {
            auto& object = scene.objects[index];
            if (!object.mesh) {
                continue;
            }
            if (rv::Mesh* mesh = object.mesh.value().mesh) {
                standardConstants.objectIndex = index;
                commandBuffer.pushConstants(pipeline, &standardConstants);
                commandBuffer.drawIndexed(mesh->vertexBuffer, mesh->indexBuffer,
                                          mesh->getIndicesCount());
            }
        }

        commandBuffer.endRendering();
        commandBuffer.endTimestamp(timer);
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
