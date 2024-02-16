#include "Pass.hpp"

void ShadowMapPass::init(const rv::Context& context,
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
        .vertexStride = sizeof(VertexPNUT),
        .vertexAttributes = VertexPNUT::getAttributeDescriptions(),
        .colorFormats = {},
        .depthFormat = shadowMapFormat,
        .cullMode = "dynamic",
    });
}

void ShadowMapPass::render(const rv::CommandBuffer& commandBuffer,
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

            commandBuffer.bindVertexBuffer(mesh->meshData->vertexBuffer);
            commandBuffer.bindIndexBuffer(mesh->meshData->indexBuffer);
            commandBuffer.drawIndexed(mesh->indexCount, 1, mesh->firstIndex, mesh->vertexOffset, 0);
        }
    }

    commandBuffer.endRendering();
    commandBuffer.endTimestamp(timer);
    commandBuffer.transitionLayout(shadowMapImage, vk::ImageLayout::eReadOnlyOptimal);
    commandBuffer.endDebugLabel();
}

void AntiAliasingPass::init(const rv::Context& context,
                            const rv::DescriptorSetHandle& _descSet,
                            vk::Format colorFormat) {
    Pass::init(context);
    descSet = _descSet;

    std::vector<rv::ShaderHandle> shaders(2);
    shaders[0] = context.createShader({
        .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "fullscreen.vert",
                                                  DEV_SHADER_DIR / "spv/fullscreen.vert.spv"),
        .stage = vk::ShaderStageFlagBits::eVertex,
    });

    shaders[1] = context.createShader({
        .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "fxaa.frag",
                                                  DEV_SHADER_DIR / "spv/fxaa.frag.spv"),
        .stage = vk::ShaderStageFlagBits::eFragment,
    });

    pipeline = context.createGraphicsPipeline({
        .descSetLayout = descSet->getLayout(),
        .vertexShader = shaders[0],
        .fragmentShader = shaders[1],
        .colorFormats = colorFormat,
    });

    timer = context.createGPUTimer({});
    initialized = true;
}

void AntiAliasingPass::render(const rv::CommandBuffer& commandBuffer,
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

void ForwardPass::init(const rv::Context& context,
                       const rv::DescriptorSetHandle& _descSet,
                       vk::Format colorFormat,
                       vk::Format depthFormat,
                       vk::Format normalFormat) {
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
        .vertexStride = sizeof(VertexPNUT),
        .vertexAttributes = VertexPNUT::getAttributeDescriptions(),
        .colorFormats = {colorFormat, normalFormat},
        .depthFormat = depthFormat,
    });
}

void ForwardPass::render(const rv::CommandBuffer& commandBuffer,
                         const rv::ImageHandle& baseColorImage,
                         const rv::ImageHandle& depthImage,
                         const rv::ImageHandle& normalImage,
                         Scene& scene,
                         bool frustumCulling) {
    vk::Extent3D extent = baseColorImage->getExtent();
    commandBuffer.beginDebugLabel("ForwardPass::render()");
    commandBuffer.bindDescriptorSet(descSet, pipeline);
    commandBuffer.bindPipeline(pipeline);

    commandBuffer.setViewport(extent.width, extent.height);
    commandBuffer.setScissor(extent.width, extent.height);
    commandBuffer.beginTimestamp(timer);
    commandBuffer.beginRendering({baseColorImage, normalImage}, depthImage, {0, 0},
                                 {extent.width, extent.height});

    int meshCount = 0;
    int visibleCount = 0;
    for (int index = 0; index < scene.getObjects().size(); index++) {
        auto& object = scene.getObjects()[index];
        Mesh* mesh = object.get<Mesh>();
        if (!mesh) {
            continue;
        }
        meshCount++;

        if (frustumCulling) {
            const auto& aabb = mesh->getWorldAABB();
            Camera* camera = scene.getMainCamera();

            if (camera && !aabb.isOnFrustum(camera->getFrustum())) {
                continue;
            }
        }
        constants.objectIndex = index;
        commandBuffer.pushConstants(pipeline, &constants);
        commandBuffer.bindVertexBuffer(mesh->meshData->vertexBuffer);
        commandBuffer.bindIndexBuffer(mesh->meshData->indexBuffer);
        commandBuffer.drawIndexed(mesh->indexCount, 1, mesh->firstIndex, mesh->vertexOffset, 0);
        visibleCount++;
    }
    if (frustumCulling) {
        spdlog::info("Visible: {} / {}", visibleCount, meshCount);
    }

    commandBuffer.endRendering();
    commandBuffer.endTimestamp(timer);

    commandBuffer.imageBarrier({baseColorImage, normalImage, depthImage},  //
                               vk::PipelineStageFlagBits::eAllGraphics,
                               vk::PipelineStageFlagBits::eAllGraphics,
                               vk::AccessFlagBits::eColorAttachmentWrite |
                                   vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                               vk::AccessFlagBits::eShaderRead);

    commandBuffer.endDebugLabel();
}

void SkyboxPass::init(const rv::Context& context,
                      const rv::DescriptorSetHandle& _descSet,
                      vk::Format colorFormat) {
    Pass::init(context);

    descSet = _descSet;

    std::vector<rv::ShaderHandle> shaders(2);
    shaders[0] = context.createShader({
        .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "skybox.vert",
                                                  DEV_SHADER_DIR / "spv/skybox.vert.spv"),
        .stage = vk::ShaderStageFlagBits::eVertex,
    });

    shaders[1] = context.createShader({
        .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "skybox.frag",
                                                  DEV_SHADER_DIR / "spv/skybox.frag.spv"),
        .stage = vk::ShaderStageFlagBits::eFragment,
    });

    pipeline = context.createGraphicsPipeline({
        .descSetLayout = descSet->getLayout(),
        .vertexShader = shaders[0],
        .fragmentShader = shaders[1],
        .vertexStride = sizeof(VertexPNUT),
        .vertexAttributes = VertexPNUT::getAttributeDescriptions(),
        .colorFormats = colorFormat,
    });
}

void SkyboxPass::render(const rv::CommandBuffer& commandBuffer,
                        const rv::ImageHandle& baseColorImage,
                        const MeshData& cubeMesh) {
    vk::Extent3D extent = baseColorImage->getExtent();
    commandBuffer.beginDebugLabel("SkyboxPass::render()");
    commandBuffer.bindDescriptorSet(descSet, pipeline);
    commandBuffer.bindPipeline(pipeline);

    commandBuffer.setViewport(extent.width, extent.height);
    commandBuffer.setScissor(extent.width, extent.height);
    commandBuffer.beginTimestamp(timer);
    commandBuffer.beginRendering(baseColorImage, nullptr, {0, 0}, {extent.width, extent.height});

    commandBuffer.bindVertexBuffer(cubeMesh.vertexBuffer);
    commandBuffer.bindIndexBuffer(cubeMesh.indexBuffer);
    commandBuffer.drawIndexed(static_cast<uint32_t>(cubeMesh.indices.size()));

    commandBuffer.endRendering();
    commandBuffer.endTimestamp(timer);

    commandBuffer.imageBarrier(
        baseColorImage,  //
        vk::PipelineStageFlagBits::eAllGraphics, vk::PipelineStageFlagBits::eAllGraphics,
        vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead);

    commandBuffer.endDebugLabel();
}

void SSRPass::init(const rv::Context& context,
                   const rv::DescriptorSetHandle& _descSet,
                   vk::Format colorFormat) {
    Pass::init(context);
    descSet = _descSet;

    std::vector<rv::ShaderHandle> shaders(2);
    shaders[0] = context.createShader({
        .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "fullscreen.vert",
                                                  DEV_SHADER_DIR / "spv/fullscreen.vert.spv"),
        .stage = vk::ShaderStageFlagBits::eVertex,
    });

    shaders[1] = context.createShader({
        .code = rv::Compiler::compileOrReadShader(DEV_SHADER_DIR / "ssr.frag",
                                                  DEV_SHADER_DIR / "spv/ssr.frag.spv"),
        .stage = vk::ShaderStageFlagBits::eFragment,
    });

    pipeline = context.createGraphicsPipeline({
        .descSetLayout = descSet->getLayout(),
        .vertexShader = shaders[0],
        .fragmentShader = shaders[1],
        .colorFormats = colorFormat,
    });

    timer = context.createGPUTimer({});
    initialized = true;
}

void SSRPass::render(const rv::CommandBuffer& commandBuffer,
                     const rv::ImageHandle& srcColorImage,
                     const rv::ImageHandle& srcNormalImage,
                     const rv::ImageHandle& srcDepthImage,
                     const rv::ImageHandle& dstImage) const {
    assert(initialized);
    vk::Extent3D extent = srcColorImage->getExtent();
    commandBuffer.transitionLayout(srcColorImage, vk::ImageLayout::eGeneral);
    commandBuffer.transitionLayout(srcNormalImage, vk::ImageLayout::eGeneral);
    commandBuffer.transitionLayout(srcDepthImage, vk::ImageLayout::eGeneral);
    commandBuffer.transitionLayout(dstImage, vk::ImageLayout::eColorAttachmentOptimal);

    commandBuffer.beginDebugLabel("SSRPass::render()");
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
