#pragma once
#include "Buffer.hpp"
#include "Pass.hpp"

class Renderer {
public:
    void init(const rv::Context& _context,
              vk::Format targetColorFormat,
              uint32_t width,
              uint32_t height);

    void createImages(uint32_t width, uint32_t height);

    void render(const rv::CommandBuffer& commandBuffer,
                const rv::ImageHandle& colorImage,
                Scene& scene);

    float getPassTimeShadow() const {
        return shadowMapPass.getRenderingTimeMs();
    }

    float getPassTimeSkybox() const {
        return skyboxPass.getRenderingTimeMs();
    }

    float getPassTimeForward() const {
        return forwardPass.getRenderingTimeMs();
    }

    float getPassTimeAA() const {
        return antiAliasingPass.getRenderingTimeMs();
    }

    float getPassTimeSSR() const {
        return ssrPass.getRenderingTimeMs();
    }

    rv::ImageHandle getShadowMap() const {
        return shadowMapImage;
    }

    vk::Format getDepthFormat() const {
        return depthFormat;
    }

    rv::ImageHandle getDepthImage() const {
        return depthImage;
    }

    // Global options
    inline static bool enableFXAA = true;
    inline static bool enableFrustumCulling = false;
    inline static bool enableSSR = true;
    inline static float exposure = 1.0f;
    inline static float ssrIntensity = 1.0f;

private:
    bool initialized = false;
    bool firstFrameRendered = false;
    const rv::Context* context = nullptr;

    rv::DescriptorSetHandle descSet;

    // Buffer
    ObjectDataBuffer objectDataBuffer;
    SceneDataBuffer sceneDataBuffer;

    // Texture
    rv::ImageHandle brdfLutTexture;
    rv::ImageHandle dummyTextures2D;
    rv::ImageHandle dummyTexturesCube;

    // Image
    vk::Format colorFormat = vk::Format::eR16G16B16A16Sfloat;
    vk::Format depthFormat = vk::Format::eD32Sfloat;
    vk::Format normalFormat = vk::Format::eR16G16B16A16Sfloat;
    vk::Format specularBrdfFormat = vk::Format::eR8G8B8A8Unorm;
    rv::ImageHandle baseColorImage;
    rv::ImageHandle compositeColorImage;
    rv::ImageHandle depthImage;
    rv::ImageHandle normalImage;
    rv::ImageHandle specularBrdfImage;

    // Shadow map pass
    ShadowMapPass shadowMapPass;
    vk::Format shadowMapFormat = vk::Format::eD32Sfloat;
    vk::Extent3D shadowMapExtent{1024, 1024, 1};
    rv::ImageHandle shadowMapImage;

    ForwardPass forwardPass;

    AntiAliasingPass antiAliasingPass;

    SkyboxPass skyboxPass;

    SSRPass ssrPass;
};
