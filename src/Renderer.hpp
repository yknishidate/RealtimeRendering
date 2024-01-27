#pragma once
#include <reactive/reactive.hpp>

#include "Scene.hpp"

class Renderer {
    struct PushConstants {
        glm::mat4 viewProj{};
        glm::mat4 model{};
        glm::vec3 color{};
    };

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

        descSet = context->createDescriptorSet({
            .shaders = shaders,
        });

        pipeline = context->createGraphicsPipeline({
            .descSetLayout = descSet->getLayout(),
            .pushSize = sizeof(PushConstants),
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

        commandBuffer.beginDebugLabel("Renderer::render()");
        commandBuffer.bindDescriptorSet(descSet, pipeline);
        commandBuffer.bindPipeline(pipeline);
        commandBuffer.transitionLayout(colorImage, vk::ImageLayout::eGeneral);

        vk::Extent3D extent = colorImage->getExtent();
        commandBuffer.setViewport(extent.width, extent.height);
        commandBuffer.setScissor(extent.width, extent.height);
        commandBuffer.beginTimestamp(timer);
        commandBuffer.beginRendering(colorImage, depthImage, {0, 0}, {extent.width, extent.height});
        // Draw scene
        glm::mat4 viewProj = scene.camera.getProj() * scene.camera.getView();
        pushConstants.viewProj = viewProj;
        for (auto& object : scene.objects) {
            pushConstants.model = object.computeTransformMatrix(frame);
            pushConstants.color = object.material->baseColor.xyz;
            rv::Mesh* mesh = object.mesh;
            if (mesh) {
                commandBuffer.pushConstants(pipeline, &pushConstants);
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
    PushConstants pushConstants{};
    rv::DescriptorSetHandle descSet;
    rv::GraphicsPipelineHandle pipeline;
    rv::GPUTimerHandle timer;
};
