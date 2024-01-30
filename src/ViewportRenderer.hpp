#pragma once
#include <imgui_impl_vulkan.h>

#include <glm/gtx/matrix_decompose.hpp>

#include "Scene.hpp"
#include "reactive/reactive.hpp"

class LineDrawer {
    struct PushConstants {
        glm::mat4 mvp{};
        glm::vec3 color{};
    };

public:
    void createPipeline(const rv::Context& context) {
        std::vector<uint32_t> vertSpvCode = rv::Compiler::compileOrReadShader(  //
            DEV_SHADER_DIR / "viewport_line.vert",                              //
            DEV_SHADER_DIR / "spv" / "viewport_line.vert.spv");

        std::vector<uint32_t> fragSpvCode = rv::Compiler::compileOrReadShader(  //
            DEV_SHADER_DIR / "viewport_line.frag",                              //
            DEV_SHADER_DIR / "spv" / "viewport_line.frag.spv");

        std::vector<rv::ShaderHandle> shaders(2);
        shaders[0] = context.createShader({
            .code = vertSpvCode,
            .stage = vk::ShaderStageFlagBits::eVertex,
        });

        shaders[1] = context.createShader({
            .code = fragSpvCode,
            .stage = vk::ShaderStageFlagBits::eFragment,
        });

        descSet = context.createDescriptorSet({
            .shaders = shaders,
        });

        pipeline = context.createGraphicsPipeline({
            .descSetLayout = descSet->getLayout(),
            .pushSize = sizeof(PushConstants),
            .vertexShader = shaders[0],
            .fragmentShader = shaders[1],
            .vertexStride = sizeof(rv::Vertex),
            .vertexAttributes = rv::Vertex::getAttributeDescriptions(),
            .colorFormats = vk::Format::eR8G8B8A8Unorm,
            .topology = vk::PrimitiveTopology::eLineList,
            .polygonMode = vk::PolygonMode::eLine,
            .lineWidth = "dynamic",
        });
    }

    void draw(const rv::CommandBuffer& commandBuffer,
              const rv::Mesh& mesh,
              const glm::mat4& mvp,
              const glm::vec3& color,
              float lineWidth) const {
        commandBuffer.bindDescriptorSet(descSet, pipeline);
        commandBuffer.bindPipeline(pipeline);

        PushConstants pushConstants{};
        pushConstants.mvp = mvp;
        pushConstants.color = color;
        commandBuffer.setLineWidth(lineWidth);
        commandBuffer.pushConstants(pipeline, &pushConstants);
        commandBuffer.drawIndexed(mesh.vertexBuffer, mesh.indexBuffer, mesh.getIndicesCount());
    }

    rv::GraphicsPipelineHandle pipeline;
    rv::DescriptorSetHandle descSet;
};

class ViewportRenderer {
public:
    void init(const rv::Context& _context, uint32_t _width, uint32_t _height) {
        context = &_context;

        createImages(_width, _height);

        lineDrawer.createPipeline(*context);

        rv::PlaneLineMeshCreateInfo gridInfo;
        gridInfo.width = 100.0f;
        gridInfo.height = 100.0f;
        gridInfo.widthSegments = 20;
        gridInfo.heightSegments = 20;
        mainGridMesh = rv::Mesh::createPlaneLineMesh(*context, gridInfo);

        gridInfo.widthSegments = 100;
        gridInfo.heightSegments = 100;
        subGridMesh = rv::Mesh::createPlaneLineMesh(*context, gridInfo);

        std::vector<rv::Vertex> vertices(2);
        std::vector<uint32_t> indices = {0, 1};
        singleLineMesh = rv::Mesh{*context, rv::MemoryUsage::DeviceHost, vertices, indices,
                                  "ViewportRenderer::singleLineMesh"};

        cubeLineMesh = rv::Mesh::createCubeLineMesh(*context, {"ViewportRenderer::cubeLineMesh"});
    }

    void createImages(uint32_t _width, uint32_t _height) {
        width = static_cast<float>(_width);
        height = static_cast<float>(_height);

        for (int i = 0; i < imageCount; i++) {
            colorImages[i] = context->createImage({
                .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
                         vk::ImageUsageFlagBits::eTransferDst |
                         vk::ImageUsageFlagBits::eTransferSrc |
                         vk::ImageUsageFlagBits::eColorAttachment,
                .extent = {_width, _height, 1},
                .format = vk::Format::eR8G8B8A8Unorm,
                .debugName = "ViewportRenderer::colorImage",
            });

            // Create desc set
            ImGui_ImplVulkan_RemoveTexture(imguiDescSets[i]);
            imguiDescSets[i] = ImGui_ImplVulkan_AddTexture(
                colorImages[i]->getSampler(), colorImages[i]->getView(), VK_IMAGE_LAYOUT_GENERAL);

            depthImages[i] = context->createImage({
                .usage = rv::ImageUsage::DepthAttachment,
                .extent = {_width, _height, 1},
                .format = vk::Format::eD32Sfloat,
                .aspect = vk::ImageAspectFlagBits::eDepth,
                .debugName = "ViewportRenderer::depthImage",
            });
        }

        context->oneTimeSubmit([&](auto commandBuffer) {
            for (int i = 0; i < imageCount; i++) {
                commandBuffer->transitionLayout(colorImages[i], vk::ImageLayout::eGeneral);
                commandBuffer->transitionLayout(depthImages[i],
                                                vk::ImageLayout::eDepthAttachmentOptimal);
            }
        });
    }

    void drawAABB(const rv::CommandBuffer& commandBuffer, rv::AABB aabb, glm::mat4 viewProj) const {
        glm::mat4 scale = glm::scale(glm::mat4{1.0f}, aabb.extents);
        glm::mat4 translate = glm::translate(glm::mat4{1.0f}, aabb.center);
        glm::mat4 model = translate * scale;
        lineDrawer.draw(commandBuffer, cubeLineMesh, viewProj * model,  //
                        glm::vec3{0.0f, 0.5f, 0.0f}, 2.0f);
    }

    void drawContents(const rv::CommandBuffer& commandBuffer, Scene& scene) const {
        vk::Extent3D extent = colorImages[currentImageIndex]->getExtent();
        commandBuffer.beginRendering(colorImages[currentImageIndex], depthImages[currentImageIndex],
                                     {0, 0}, {extent.width, extent.height});

        const rv::Camera& camera = scene.getCamera();
        glm::mat4 viewProj = camera.getProj() * camera.getView();

        commandBuffer.setViewport(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        commandBuffer.setScissor(static_cast<uint32_t>(width), static_cast<uint32_t>(height));

        // Draw grid
        lineDrawer.draw(commandBuffer, mainGridMesh, viewProj, glm::vec3{0.4f, 0.4f, 0.4f}, 2.0f);
        lineDrawer.draw(commandBuffer, subGridMesh, viewProj, glm::vec3{0.2f, 0.2f, 0.2f}, 1.0f);

        // Draw scene objects
        for (auto& object : scene.getObjects()) {
            // Draw directional light
            if (const DirectionalLight* light = object.get<DirectionalLight>()) {
                std::vector<rv::Vertex> vertices(2);
                vertices[0].pos = glm::vec3{0.0f};
                vertices[1].pos = light->getDirection() * 5.0f;
                singleLineMesh.vertexBuffer->copy(vertices.data());
                lineDrawer.draw(commandBuffer, singleLineMesh, viewProj,
                                glm::vec3{0.7f, 0.7f, 0.7f}, 2.0f);
            }

            // Draw AABB
            drawAABB(commandBuffer, object.getAABB(), viewProj);
        }

        // Draw scene AABB
        drawAABB(commandBuffer, scene.getAABB(), viewProj);

        commandBuffer.endRendering();
    }

    bool needsRecreate() const {
        vk::Extent3D extent = colorImages[currentImageIndex]->getExtent();
        return static_cast<uint32_t>(width) != extent.width ||
               static_cast<uint32_t>(height) != extent.height;
    }

    rv::ImageHandle getCurrentColorImage() const {
        return colorImages[currentImageIndex];
    }

    rv::ImageHandle getCurrentDepthImage() const {
        return depthImages[currentImageIndex];
    }

    vk::DescriptorSet getCurrentImageDescSet() const {
        return imguiDescSets[currentImageIndex];
    }

    void advanceImageIndex() {
        currentImageIndex = (currentImageIndex + 1) % imageCount;
    }

    const rv::Context* context = nullptr;

    // Image
    static constexpr int imageCount = 3;
    int currentImageIndex = 0;
    float width = 0.0f;
    float height = 0.0f;
    std::array<rv::ImageHandle, imageCount> colorImages;
    std::array<rv::ImageHandle, imageCount> depthImages;
    std::array<vk::DescriptorSet, imageCount> imguiDescSets;

    // Line drawer
    LineDrawer lineDrawer;
    rv::Mesh mainGridMesh;
    rv::Mesh subGridMesh;
    rv::Mesh singleLineMesh;
    rv::Mesh cubeLineMesh;
};
