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
    const std::string vertCode = R"(
    #version 450
    layout(location = 0) in vec3 inPosition;
    layout(location = 1) in vec3 inNormal;
    layout(location = 2) in vec2 inTexCoord;
    layout(location = 0) out vec3 outNormal;

    layout(push_constant) uniform PushConstants {
        mat4 viewProj;
        mat4 model;
        vec3 color;
    };

    void main() {
        gl_Position = viewProj * model * vec4(inPosition, 1);
        outNormal = inNormal;
    })";

    const std::string fragCode = R"(
    #version 450
    layout(location = 0) in vec3 inNormal;
    layout(location = 0) out vec4 outColor;

    layout(push_constant) uniform PushConstants {
        mat4 viewProj;
        mat4 model;
        vec3 color;
    };

    void main() {
        vec3 lightDir = normalize(vec3(1, 2, -3));
        vec3 diffuse = color * (dot(lightDir, inNormal) * 0.5 + 0.5);
        outColor = vec4(diffuse, 1.0);
    })";

    void init(const rv::Context& _context) {
        context = &_context;

        std::vector<rv::ShaderHandle> shaders(2);
        shaders[0] = context->createShader({
            .code = rv::Compiler::compileToSPV(vertCode, vk::ShaderStageFlagBits::eVertex),
            .stage = vk::ShaderStageFlagBits::eVertex,
        });

        shaders[1] = context->createShader({
            .code = rv::Compiler::compileToSPV(fragCode, vk::ShaderStageFlagBits::eFragment),
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
                const rv::Camera& camera,
                int frame) {
        commandBuffer.clearColorImage(colorImage, {0.05f, 0.05f, 0.05f, 1.0f});
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
        glm::mat4 viewProj = camera.getProj() * camera.getView();
        pushConstants.viewProj = viewProj;
        for (auto& node : scene.nodes) {
            pushConstants.model = node.computeTransformMatrix(frame);
            pushConstants.color = node.material->baseColor.xyz;
            rv::Mesh* mesh = node.mesh;
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
