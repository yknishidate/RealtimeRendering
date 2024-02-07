#pragma once
#include <reactive/reactive.hpp>

#include "Scene.hpp"

#include "../shader/standard.glsl"

class Pass {
public:
    void init(const rv::Context& context) {
        timer = context.createGPUTimer({});
        initialized = true;
    }

    float getRenderingTimeMs() const {
        assert(initialized);
        return timer->elapsedInMilli();
    }

protected:
    bool initialized = false;
    rv::GPUTimerHandle timer;
};

class ShadowMapPass final : public Pass {
public:
    void init(const rv::Context& context,
              const rv::DescriptorSetHandle& _descSet,
              vk::Format shadowMapFormat);

    void render(const rv::CommandBuffer& commandBuffer,
                const rv::ImageHandle& shadowMapImage,
                Scene& scene,
                const DirectionalLight& light) const;

    glm::mat4 getViewProj(const DirectionalLight& light, const rv::AABB& aabb) const;

private:
    rv::DescriptorSetHandle descSet;
    rv::GraphicsPipelineHandle pipeline;
};

class AntiAliasingPass final : public Pass {
public:
    void init(const rv::Context& context,
              const rv::DescriptorSetHandle& _descSet,
              vk::Format colorFormat);

    void render(const rv::CommandBuffer& commandBuffer,
                const rv::ImageHandle& srcImage,
                const rv::ImageHandle& dstImage) const;

private:
    rv::DescriptorSetHandle descSet;
    rv::GraphicsPipelineHandle pipeline;
};

class ForwardPass final : public Pass {
public:
    void init(const rv::Context& context,
              const rv::DescriptorSetHandle& _descSet,
              vk::Format colorFormat,
              vk::Format depthFormat);

    void render(const rv::CommandBuffer& commandBuffer,
                const rv::ImageHandle& baseColorImage,
                const rv::ImageHandle& depthImage,
                std::vector<Object>& objects);

private:
    StandardConstants constants;
    rv::DescriptorSetHandle descSet;
    rv::GraphicsPipelineHandle pipeline;
};

class SkyboxPass final : public Pass {
public:
    void init(const rv::Context& context,
              const rv::DescriptorSetHandle& _descSet,
              vk::Format colorFormat);

    void render(const rv::CommandBuffer& commandBuffer,
                const rv::ImageHandle& baseColorImage,
                const MeshData& cubeMesh);

private:
    rv::DescriptorSetHandle descSet;
    rv::GraphicsPipelineHandle pipeline;
};
