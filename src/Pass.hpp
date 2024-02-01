#pragma once
#include <reactive/reactive.hpp>

#include "RenderImages.hpp"
#include "Scene.hpp"

#include "../shader/standard.glsl"

class Pass {
public:
    void init(const rv::Context& context) {
        timer = context.createGPUTimer({});
        initialized = true;
    }

    float getRenderingTimeMs() {
        assert(initialized);
        if (isFirstRenderTimeRequest) {
            isFirstRenderTimeRequest = false;
            return 0.0f;
        }
        return timer->elapsedInMilli();
    }

protected:
    bool initialized = false;
    bool isFirstRenderTimeRequest = true;
    rv::GPUTimerHandle timer;
};

class ShadowMapPass final : public Pass {
public:
    void init(const rv::Context& context,
              const rv::DescriptorSetHandle& _descSet,
              vk::Format shadowMapFormat) {
        Pass::init(context);

        descSet = _descSet;

        std::vector<rv::ShaderHandle> shaders(2);
        shaders[0] = context.createShader({
            .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "shadow_map.vert",
                                                      DEV_SHADER_DIR / "spv/shadow_map.vert.spv"),
            .stage = vk::ShaderStageFlagBits::eVertex,
        });

        shaders[1] = context.createShader({
            .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "shadow_map.frag",
                                                      DEV_SHADER_DIR / "spv/shadow_map.frag.spv"),
            .stage = vk::ShaderStageFlagBits::eFragment,
        });

        pipeline = context.createGraphicsPipeline({
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
    }

    void render(const rv::CommandBuffer& commandBuffer,
                const rv::ImageHandle& shadowMapImage,
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
    rv::DescriptorSetHandle descSet;
    rv::GraphicsPipelineHandle pipeline;
};

class AntiAliasingPass final : public Pass {
public:
    void init(const rv::Context& context,
              const rv::DescriptorSetHandle& _descSet,
              vk::Format colorFormat) {
        Pass::init(context);
        descSet = _descSet;

        std::vector<rv::ShaderHandle> shaders(2);
        try {
            shaders[0] = context.createShader({
                .code = rv::Compiler::compileOrReadShader(
                    DEV_SHADER_DIR / "fullscreen.vert", DEV_SHADER_DIR / "spv/fullscreen.vert.spv"),
                .stage = vk::ShaderStageFlagBits::eVertex,
            });

            shaders[1] = context.createShader({
                .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "fxaa.frag",
                                                          DEV_SHADER_DIR / "spv/fxaa.frag.spv"),
                .stage = vk::ShaderStageFlagBits::eFragment,
            });
        } catch (const std::exception& e) {
            spdlog::error(e.what());
            std::abort();
        }

        pipeline = context.createGraphicsPipeline({
            .descSetLayout = descSet->getLayout(),
            .vertexShader = shaders[0],
            .fragmentShader = shaders[1],
            .colorFormats = colorFormat,
        });

        timer = context.createGPUTimer({});
        initialized = true;
    }

    void render(const rv::CommandBuffer& commandBuffer,
                const rv::ImageHandle& srcImage,
                const rv::ImageHandle& dstImage) const {
        assert(initialized);
        vk::Extent3D extent = srcImage->getExtent();
        commandBuffer.transitionLayout(srcImage, vk::ImageLayout::eGeneral);
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
        commandBuffer.endDebugLabel();
    }

private:
    rv::DescriptorSetHandle descSet;
    rv::GraphicsPipelineHandle pipeline;
};

class ForwardPass final : public Pass {
public:
    void init(const rv::Context& context,
              const rv::DescriptorSetHandle& _descSet,
              vk::Format colorFormat,
              vk::Format depthFormat) {
        Pass::init(context);

        descSet = _descSet;

        std::vector<rv::ShaderHandle> shaders(2);
        shaders[0] = context.createShader({
            .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "standard.vert",
                                                      DEV_SHADER_DIR / "spv/standard.vert.spv"),
            .stage = vk::ShaderStageFlagBits::eVertex,
        });

        shaders[1] = context.createShader({
            .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "standard.frag",
                                                      DEV_SHADER_DIR / "spv/standard.frag.spv"),
            .stage = vk::ShaderStageFlagBits::eFragment,
        });

        pipeline = context.createGraphicsPipeline({
            .descSetLayout = descSet->getLayout(),
            .pushSize = sizeof(StandardConstants),
            .vertexShader = shaders[0],
            .fragmentShader = shaders[1],
            .vertexStride = sizeof(rv::Vertex),
            .vertexAttributes = rv::Vertex::getAttributeDescriptions(),
            .colorFormats = colorFormat,
            .depthFormat = depthFormat,
        });
    }

    void render(const rv::CommandBuffer& commandBuffer,
                const rv::ImageHandle& baseColorImage,
                const rv::ImageHandle& depthImage,
                std::vector<Object>& objects) {
        vk::Extent3D extent = baseColorImage->getExtent();
        commandBuffer.beginDebugLabel("Renderer::render()");
        commandBuffer.bindDescriptorSet(descSet, pipeline);
        commandBuffer.bindPipeline(pipeline);

        commandBuffer.setViewport(extent.width, extent.height);
        commandBuffer.setScissor(extent.width, extent.height);
        commandBuffer.beginTimestamp(timer);
        commandBuffer.beginRendering(baseColorImage, depthImage, {0, 0},
                                     {extent.width, extent.height});

        for (int index = 0; index < objects.size(); index++) {
            auto& object = objects[index];
            Mesh* mesh = object.get<Mesh>();
            if (!mesh) {
                continue;
            }
            if (mesh->mesh) {
                constants.objectIndex = index;
                commandBuffer.pushConstants(pipeline, &constants);
                commandBuffer.drawIndexed(mesh->mesh->vertexBuffer, mesh->mesh->indexBuffer,
                                          mesh->mesh->getIndicesCount());
            }
        }

        commandBuffer.endRendering();
        commandBuffer.endTimestamp(timer);

        commandBuffer.imageBarrier({baseColorImage, depthImage},  //
                                   vk::PipelineStageFlagBits::eAllGraphics,
                                   vk::PipelineStageFlagBits::eAllGraphics,
                                   vk::AccessFlagBits::eColorAttachmentWrite |
                                       vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                                   vk::AccessFlagBits::eShaderRead);

        commandBuffer.endDebugLabel();
    }

private:
    StandardConstants constants;
    rv::DescriptorSetHandle descSet;
    rv::GraphicsPipelineHandle pipeline;
};
