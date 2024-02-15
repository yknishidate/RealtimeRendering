#include "Renderer.hpp"

void Renderer::init(const rv::Context& _context,
                    vk::Format targetColorFormat,
                    uint32_t width,
                    uint32_t height) {
    context = &_context;

    createImages(width, height);

    sceneDataBuffer.init(*context);
    objectDataBuffer.init(*context);

    shadowMapImage = context->createImage({
        .usage = rv::ImageUsage::DepthAttachment | vk::ImageUsageFlagBits::eSampled,
        .extent = shadowMapExtent,
        .format = shadowMapFormat,
        .debugName = "ShadowMapPass::depthImage",
    });
    shadowMapImage->createImageView(vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eDepth);
    shadowMapImage->createSampler();

    // シェーダリフレクションのために適当なシェーダを作成する
    // TODO: DescSetに合わせ、全てのシェーダを一か所で管理する
    rv::ShaderHandle reflectionShaderVert = context->createShader({
        .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "standard.vert",
                                                  DEV_SHADER_DIR / "spv/standard.vert.spv"),
        .stage = vk::ShaderStageFlagBits::eVertex,
    });
    rv::ShaderHandle reflectionShaderFrag = context->createShader({
        .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "standard.frag",
                                                  DEV_SHADER_DIR / "spv/standard.frag.spv"),
        .stage = vk::ShaderStageFlagBits::eFragment,
    });

    brdfLutTexture =
        rv::Image::loadFromFile(*context, DEV_ASSET_DIR / "environments" / "tex_brdflut.png", 1,
                                vk::Filter::eLinear, vk::SamplerAddressMode::eClampToEdge);

    dummyTextures2D = context->createImage({
        .usage = rv::ImageUsage::Sampled,
        .format = vk::Format::eB8G8R8A8Unorm,
        .debugName = "dummyTextures2D",
    });
    dummyTexturesCube = context->createImage({
        .usage = rv::ImageUsage::Sampled,
        .format = vk::Format::eB8G8R8A8Unorm,
        .debugName = "dummyTextures2D",
    });
    dummyTextures2D->createImageView();
    dummyTextures2D->createSampler();
    dummyTexturesCube->createImageView();
    dummyTexturesCube->createSampler();

    context->oneTimeSubmit([&](rv::CommandBufferHandle commandBuffer) {
        commandBuffer->transitionLayout(shadowMapImage, vk::ImageLayout::eReadOnlyOptimal);
        commandBuffer->transitionLayout(dummyTextures2D, vk::ImageLayout::eReadOnlyOptimal);
        commandBuffer->transitionLayout(dummyTexturesCube, vk::ImageLayout::eReadOnlyOptimal);
    });

    descSet = context->createDescriptorSet({
        .shaders = {reflectionShaderVert, reflectionShaderFrag},
        .buffers =
            {
                {"SceneBuffer", sceneDataBuffer.buffer},
                {"ObjectBuffer", objectDataBuffer.buffer},
            },
        .images =
            {
                {"shadowMap", shadowMapImage},
                {"baseColorImage", baseColorImage},
                {"textures2D", 100u},
                {"texturesCube", 100u},
                {"brdfLutTexture", brdfLutTexture},
            },
    });
    descSet->set("textures2D", dummyTextures2D);
    descSet->set("texturesCube", dummyTexturesCube);
    descSet->update();

    try {
        skyboxPass.init(*context, descSet, colorFormat);
        shadowMapPass.init(*context, descSet, shadowMapFormat);
        forwardPass.init(*context, descSet, colorFormat, depthFormat);
        antiAliasingPass.init(*context, descSet, targetColorFormat);
    } catch (const std::exception& e) {
        spdlog::error(e.what());
        std::abort();
    }

    initialized = true;
    firstFrameRendered = false;
}

void Renderer::createImages(uint32_t width, uint32_t height) {
    baseColorImage = context->createImage({
        .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
                 vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc |
                 vk::ImageUsageFlagBits::eColorAttachment,
        .extent = {width, height, 1},
        .format = colorFormat,
        .debugName = "ViewportRenderer::colorImage",
    });
    baseColorImage->createImageView();
    baseColorImage->createSampler();

    depthImage = context->createImage({
        .usage = rv::ImageUsage::DepthAttachment,
        .extent = {width, height, 1},
        .format = depthFormat,
        .debugName = "ViewportRenderer::depthImage",
    });
    depthImage->createImageView(vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eDepth);
    depthImage->createSampler();

    context->oneTimeSubmit([&](rv::CommandBufferHandle commandBuffer) {
        commandBuffer->transitionLayout(baseColorImage, vk::ImageLayout::eGeneral);
        commandBuffer->transitionLayout(depthImage, vk::ImageLayout::eDepthAttachmentOptimal);
    });
}

void Renderer::render(const rv::CommandBuffer& commandBuffer,
                      const rv::ImageHandle& colorImage,
                      Scene& scene) {
    assert(initialized);

    bool shouldUpdate = false;
    vk::Extent3D extent = colorImage->getExtent();
    if (extent != baseColorImage->getExtent()) {
        context->getDevice().waitIdle();
        uint32_t width = extent.width;
        uint32_t height = extent.height;
        createImages(width, height);
        descSet->set("baseColorImage", baseColorImage);
        shouldUpdate = true;
    }

    if (scene.getStatus() & SceneStatus::Cleared) {
        sceneDataBuffer.clear();
        objectDataBuffer.clear();
        descSet->set("textures2D", dummyTextures2D);
        descSet->set("texturesCube", dummyTexturesCube);
        shouldUpdate = true;
    }

    if (!firstFrameRendered || scene.getStatus() & SceneStatus::Texture2DAdded) {
        if (!scene.getTextures2D().empty()) {
            std::vector<rv::ImageHandle> textures2D;
            for (auto& tex : scene.getTextures2D()) {
                textures2D.push_back(tex.image);
            }
            descSet->set("textures2D", textures2D);
            shouldUpdate = true;
            spdlog::info("Update desc set for texture 2D");
        }
    }
    if (!firstFrameRendered || scene.getStatus() & SceneStatus::TextureCubeAdded) {
        if (!scene.getTexturesCube().empty()) {
            std::vector<rv::ImageHandle> texturesCube;
            for (auto& tex : scene.getTexturesCube()) {
                texturesCube.push_back(tex.image);
            }
            descSet->set("texturesCube", texturesCube);
            shouldUpdate = true;
            spdlog::info("Update desc set for texture cube");
        }
    }
    if (shouldUpdate) {
        descSet->update();
    }
    scene.resetStatus();

    objectDataBuffer.update(commandBuffer, scene);
    sceneDataBuffer.update(commandBuffer, scene, extent, enableFXAA, exposure);

    commandBuffer.clearColorImage(colorImage, {0.1f, 0.1f, 0.1f, 1.0f});
    commandBuffer.clearColorImage(baseColorImage, {0.1f, 0.1f, 0.1f, 1.0f});
    commandBuffer.clearDepthStencilImage(depthImage, 1.0f, 0);
    commandBuffer.imageBarrier({colorImage, baseColorImage, depthImage},  //
                               vk::PipelineStageFlagBits::eTransfer,
                               vk::PipelineStageFlagBits::eAllGraphics,
                               vk::AccessFlagBits::eTransferWrite,
                               vk::AccessFlagBits::eColorAttachmentWrite |
                                   vk::AccessFlagBits::eDepthStencilAttachmentWrite);

    // Shadow pass
    if (Object* dirLightObj = scene.findObject<DirectionalLight>()) {
        DirectionalLight* dirLight = dirLightObj->get<DirectionalLight>();
        if (dirLight->enableShadow) {
            shadowMapPass.render(commandBuffer, shadowMapImage, scene, *dirLight);
        }
    }

    // Skybox pass
    if (scene.findObject<AmbientLight>()) {
        skyboxPass.render(commandBuffer, baseColorImage, scene.getCubeMesh());
    }

    // Forward pass
    forwardPass.render(commandBuffer, baseColorImage, depthImage, scene, enableFrustumCulling);

    // AA pass
    antiAliasingPass.render(commandBuffer, baseColorImage, colorImage);

    commandBuffer.transitionLayout(colorImage, vk::ImageLayout::eGeneral);

    firstFrameRendered = true;
}
