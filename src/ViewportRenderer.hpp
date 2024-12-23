#pragma once
#include <glm/gtx/matrix_decompose.hpp>

#include <reactive/reactive.hpp>

#include "Scene.hpp"

class LineDrawer {
    struct PushConstants {
        glm::mat4 mvp{};
        glm::vec3 color{};
    };

public:
    void createPipeline(const rv::Context& context,
                        vk::Format colorFormat,
                        vk::Format depthFormat) {
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
            .colorFormats = colorFormat,
            .depthFormat = depthFormat,
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
        commandBuffer.bindDescriptorSet(pipeline, descSet);
        commandBuffer.bindPipeline(pipeline);

        PushConstants pushConstants{};
        pushConstants.mvp = mvp;
        pushConstants.color = color;
        commandBuffer.setLineWidth(lineWidth);
        commandBuffer.pushConstants(pipeline, &pushConstants);
        commandBuffer.bindVertexBuffer(mesh.vertexBuffer);
        commandBuffer.bindIndexBuffer(mesh.indexBuffer);
        commandBuffer.drawIndexed(mesh.getIndicesCount());
    }

    rv::GraphicsPipelineHandle pipeline;
    rv::DescriptorSetHandle descSet;
};

class ViewportRenderer {
public:
    void init(const rv::Context& _context, vk::Format colorFormat, vk::Format depthFormat) {
        context = &_context;

        lineDrawer.createPipeline(*context, colorFormat, depthFormat);

        rv::PlaneLineMeshCreateInfo gridInfo;
        gridInfo.width = 100.0f;
        gridInfo.height = 100.0f;
        gridInfo.widthSegments = 20;
        gridInfo.heightSegments = 20;
        mainGridMesh = rv::Mesh::createPlaneLineMesh(*context, gridInfo);

        gridInfo.widthSegments = 100;
        gridInfo.heightSegments = 100;
        subGridMesh = rv::Mesh::createPlaneLineMesh(*context, gridInfo);

        // FIX: DeviceHostにすると確保に失敗する
        std::vector<rv::Vertex> vertices = {{{0.0f, 0.0f, 0.0f}}, {{0.0f, 1.0f, 0.0f}}};
        std::vector<uint32_t> indices = {0, 1};
        singleLineMesh = {*context, rv::MeshUsage::Graphics, rv::MemoryUsage::Device,
                          vertices, indices, "ViewportRenderer::singleLineMesh"};

        cubeLineMesh = rv::Mesh::createCubeLineMesh(*context, {.usage = rv::MeshUsage::Graphics, .name = "ViewportRenderer::cubeLineMesh"});
    }

    void drawAABB(const rv::CommandBuffer& commandBuffer,
                  const rv::AABB& aabb,
                  const glm::mat4& viewProj) const {
        glm::mat4 scale = glm::scale(glm::mat4{1.0f}, aabb.extents);
        glm::mat4 translate = glm::translate(glm::mat4{1.0f}, aabb.center);
        glm::mat4 model = translate * scale;
        lineDrawer.draw(commandBuffer, cubeLineMesh, viewProj * model,  //
                        glm::vec3{0.0f, 0.5f, 0.0f}, 2.0f);
    }

    void render(const rv::CommandBuffer& commandBuffer,
                const rv::ImageHandle& colorImage,
                const rv::ImageHandle& depthImage,
                Scene& scene) const {
        vk::Extent3D extent = colorImage->getExtent();
        commandBuffer.beginDebugLabel("ViewportRender");
        commandBuffer.beginRendering(colorImage, depthImage, {0, 0}, {extent.width, extent.height});

        Camera* camera = &scene.getDefaultCamera();
        if (scene.isMainCameraAvailable()) {
            camera = scene.getMainCamera();
        }
        glm::mat4 viewProj = camera->getProj() * camera->getView();

        commandBuffer.setViewport(extent.width, extent.height);
        commandBuffer.setScissor(extent.width, extent.height);

        // Draw grid
        if (isGridVisible) {
            lineDrawer.draw(commandBuffer, mainGridMesh, viewProj,  //
                            glm::vec3{0.4f, 0.4f, 0.4f}, 2.0f);
            lineDrawer.draw(commandBuffer, subGridMesh, viewProj,  //
                            glm::vec3{0.2f, 0.2f, 0.2f}, 1.0f);
        }

        // Draw scene objects
        for (auto& object : scene.getObjects()) {
            // Draw directional light
            if (isLightVisible) {
                if (const DirectionalLight* light = object.get<DirectionalLight>()) {
                    lineDrawer.draw(commandBuffer, singleLineMesh,
                                    viewProj * light->getRotationMatrix(),
                                    glm::vec3{0.7f, 0.7f, 0.7f}, 2.0f);
                }
            }

            // Draw camera
            if (isCameraVisible) {
                const Camera* _camera = object.get<Camera>();
                if (_camera && camera != _camera) {
                    glm::mat4 invProj = _camera->getInvProj();
                    glm::mat4 invView = _camera->getInvView();
                    glm::mat4 model = invView * invProj;
                    lineDrawer.draw(commandBuffer, cubeLineMesh, viewProj * model,  //
                                    glm::vec3{1.0f, 1.0f, 1.0f}, 2.0f);
                }
            }

            // Draw AABB
            if (isObjectAABBVisible) {
                if (const Mesh* mesh = object.get<Mesh>()) {
                    drawAABB(commandBuffer, mesh->getWorldAABB(), viewProj);
                }
            }
        }

        // Draw scene AABB
        if (isSceneAABBVisible) {
            drawAABB(commandBuffer, scene.getAABB(), viewProj);
        }

        commandBuffer.endRendering();
        commandBuffer.endDebugLabel();
    }

    const rv::Context* context = nullptr;

    // Line drawer
    LineDrawer lineDrawer{};
    rv::Mesh mainGridMesh{};
    rv::Mesh subGridMesh{};
    rv::Mesh singleLineMesh{};
    rv::Mesh cubeLineMesh{};

    inline static bool isObjectAABBVisible = false;
    inline static bool isSceneAABBVisible = false;
    inline static bool isGridVisible = true;
    inline static bool isLightVisible = true;
    inline static bool isCameraVisible = true;
};
