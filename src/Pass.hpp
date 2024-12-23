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
              vk::Format depthFormat,
              vk::Format specularBrdfFormat,
              vk::Format normalFormat);

    void render(const rv::CommandBuffer& commandBuffer,
                const rv::ImageHandle& baseColorImage,
                const rv::ImageHandle& depthImage,
                const rv::ImageHandle& specularBrdfImage,
                const rv::ImageHandle& normalImage,
                Scene& scene,
                bool frustumCulling,
                bool enableSorting);

private:
    StandardConstants constants;
    rv::DescriptorSetHandle descSet;
    rv::GraphicsPipelineHandle pipeline;
    int meshCount = 0;
    int visibleCount = 0;
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

class SSRPass final : public Pass {
public:
    void init(const rv::Context& context,
              const rv::DescriptorSetHandle& _descSet,
              vk::Format colorFormat);

    void render(const rv::CommandBuffer& commandBuffer,
                const rv::ImageHandle& srcColorImage,
                const rv::ImageHandle& srcNormalImage,
                const rv::ImageHandle& srcDepthImage,
                const rv::ImageHandle& dstImage) const;

private:
    rv::DescriptorSetHandle descSet;
    rv::GraphicsPipelineHandle pipeline;
};
