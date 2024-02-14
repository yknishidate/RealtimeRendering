#pragma once
#include "Pass.hpp"

class Renderer {
public:
    void init(const rv::Context& _context,
              vk::Format targetColorFormat,
              uint32_t width,
              uint32_t height);

    void createImages(uint32_t width, uint32_t height);

    void updateBuffers(const rv::CommandBuffer& commandBuffer, vk::Extent3D extent, Scene& scene);

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
    inline static float exposure = 1.0f;

private:
    void updateObjectData(const Object& object, uint32_t index);

    bool initialized = false;
    bool firstFrameRendered = false;
    const rv::Context* context = nullptr;

    rv::DescriptorSetHandle descSet;

    // Buffer
    int maxObjectCount = 1000;
    SceneData sceneUniform{};
    std::vector<ObjectData> objectStorage{};
    rv::BufferHandle sceneUniformBuffer;
    rv::BufferHandle objectStorageBuffer;

    // Image
    rv::ImageHandle brdfLutTexture;
    rv::ImageHandle dummyTextures2D;
    rv::ImageHandle dummyTexturesCube;
    vk::Format colorFormat = vk::Format::eR16G16B16A16Sfloat;
    vk::Format depthFormat = vk::Format::eD32Sfloat;
    rv::ImageHandle baseColorImage;
    rv::ImageHandle depthImage;

    // Shadow map pass
    ShadowMapPass shadowMapPass;
    vk::Format shadowMapFormat = vk::Format::eD32Sfloat;
    vk::Extent3D shadowMapExtent{1024, 1024, 1};
    rv::ImageHandle shadowMapImage;

    // Forward pass
    ForwardPass forwardPass;

    // AA pass
    AntiAliasingPass antiAliasingPass;

    // Skybox pass
    SkyboxPass skyboxPass;
};
