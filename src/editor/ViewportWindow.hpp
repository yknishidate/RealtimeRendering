#pragma once
#include <ImGuizmo.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <glm/gtx/matrix_decompose.hpp>

#include "Scene.hpp"
#include "reactive/reactive.hpp"

class LineDrawer {
    struct PushConstants {
        glm::mat4 viewProj{};
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
              const glm::mat4& viewProj,
              const glm::vec3& color,
              float lineWidth) {
        commandBuffer.bindDescriptorSet(descSet, pipeline);
        commandBuffer.bindPipeline(pipeline);

        pushConstants.viewProj = viewProj;
        pushConstants.color = color;
        commandBuffer.setLineWidth(lineWidth);
        commandBuffer.pushConstants(pipeline, &pushConstants);
        commandBuffer.drawIndexed(mesh.vertexBuffer, mesh.indexBuffer, mesh.getIndicesCount());
    }

    rv::GraphicsPipelineHandle pipeline;
    rv::DescriptorSetHandle descSet;
    PushConstants pushConstants{};
};

class ViewportWindow {
public:
    void init(const rv::Context& _context,
              IconManager& _iconManager,
              uint32_t _width,
              uint32_t _height) {
        context = &_context;
        iconManager = &_iconManager;

        createImages(_width, _height);

        lineDrawer.createPipeline(*context);

        rv::PlaneLineMeshCreateInfo gridInfo;
        gridInfo.width = 20.0f;
        gridInfo.height = 20.0f;
        gridInfo.widthSegments = 20;
        gridInfo.heightSegments = 20;
        mainGridMesh = rv::Mesh::createPlaneLineMesh(*context, gridInfo);

        gridInfo.widthSegments = 100;
        gridInfo.heightSegments = 100;
        subGridMesh = rv::Mesh::createPlaneLineMesh(*context, gridInfo);
    }

    bool editTransform(const rv::Camera& camera, glm::mat4& matrix) const {
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();

        ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y,  // break
                          ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

        glm::mat4 cameraProjection = camera.getProj();
        glm::mat4 cameraView = camera.getView();
        return ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection),
                                    currentGizmoOperation, ImGuizmo::LOCAL, glm::value_ptr(matrix));
    }

    void createImages(uint32_t _width, uint32_t _height) {
        width = static_cast<float>(_width);
        height = static_cast<float>(_height);

        colorImage = context->createImage({
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
                     vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc |
                     vk::ImageUsageFlagBits::eColorAttachment,
            .extent = {_width, _height, 1},
            .format = vk::Format::eR8G8B8A8Unorm,
            .debugName = "ViewportWindow::colorImage",
        });

        // Create desc set
        ImGui_ImplVulkan_RemoveTexture(imguiDescSet);
        imguiDescSet = ImGui_ImplVulkan_AddTexture(colorImage->getSampler(), colorImage->getView(),
                                                   VK_IMAGE_LAYOUT_GENERAL);

        depthImage = context->createImage({
            .usage = rv::ImageUsage::DepthAttachment,
            .extent = {_width, _height, 1},
            .format = vk::Format::eD32Sfloat,
            .aspect = vk::ImageAspectFlagBits::eDepth,
            .debugName = "ViewportWindow::depthImage",
        });

        context->oneTimeSubmit([&](auto commandBuffer) {
            commandBuffer->transitionLayout(colorImage, vk::ImageLayout::eGeneral);
            commandBuffer->transitionLayout(depthImage, vk::ImageLayout::eDepthAttachmentOptimal);
        });
    }

    bool processMouseInput() {
        bool changed = false;
        if (ImGui::IsWindowFocused() && !ImGuizmo::IsUsing()) {
            dragDelta.x = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x * 0.5f;
            dragDelta.y = -ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y * 0.5f;
            mouseScroll = ImGui::GetIO().MouseWheel;
            if (dragDelta.x != 0.0 || dragDelta.y != 0.0 || mouseScroll != 0) {
                changed = true;
            }
        }
        ImGui::ResetMouseDragDelta();
        return changed;
    }

    bool showGizmo(Scene& scene, Object* selectedObject, int frame) const {
        if (!selectedObject) {
            return false;
        }
        Transform* transform = selectedObject->get<Transform>();
        if (!transform) {
            return false;
        }

        bool changed = false;
        glm::mat4 model = transform->computeTransformMatrix(frame);
        changed |= editTransform(scene.getCamera(), model);

        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(model, transform->scale, transform->rotation, transform->translation, skew,
                       perspective);
        return changed;
    }

    void showToolIcon(const std::string& name, float thumbnailSize, ImGuizmo::OPERATION operation) {
        ImVec4 bgColor = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
        if (currentGizmoOperation == operation || iconManager->isHover(thumbnailSize)) {
            bgColor = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
        }
        iconManager->showIcon(name, "", thumbnailSize, bgColor,
                              [&]() { currentGizmoOperation = operation; });
    }

    void showToolBar(ImVec2 viewportPos) {
        ImGui::SetCursorScreenPos(ImVec2(viewportPos.x + 10, viewportPos.y + 15));
        ImGui::BeginChild("Toolbar", ImVec2(180, 60), false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration);

        float padding = 1.0f;
        float thumbnailSize = 50.0f;
        float cellSize = thumbnailSize + padding;
        float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount = static_cast<int>(panelWidth / cellSize);
        ImGui::Columns(columnCount, 0, false);

        showToolIcon("manip_translate", thumbnailSize, ImGuizmo::TRANSLATE);
        showToolIcon("manip_rotate", thumbnailSize, ImGuizmo::ROTATE);
        showToolIcon("manip_scale", thumbnailSize, ImGuizmo::SCALE);

        ImGui::Columns(1);
        ImGui::EndChild();
    }

    int show(Scene& scene, Object* selectedObject, int frame) {
        int message = Message::None;
        if (ImGui::Begin("Viewport")) {
            if (processMouseInput()) {
                message |= Message::CameraChanged;
            }

            ImVec2 windowPos = ImGui::GetCursorScreenPos();
            ImVec2 windowSize = ImGui::GetContentRegionAvail();
            width = windowSize.x;
            height = windowSize.y;
            ImGui::Image(imguiDescSet, windowSize, ImVec2(0, 1), ImVec2(1, 0));

            showToolBar(windowPos);
            if (showGizmo(scene, selectedObject, frame)) {
                message |= Message::TransformChanged;
            }

            ImGui::End();
        }
        return message;
    }

    void drawContents(const rv::CommandBuffer& commandBuffer, Scene& scene) {
        vk::Extent3D extent = colorImage->getExtent();
        commandBuffer.beginRendering(colorImage, depthImage, {0, 0}, {extent.width, extent.height});

        const rv::Camera& camera = scene.getCamera();
        glm::mat4 viewProj = camera.getProj() * camera.getView();

        commandBuffer.setViewport(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        commandBuffer.setScissor(static_cast<uint32_t>(width), static_cast<uint32_t>(height));

        // Draw grid
        lineDrawer.draw(commandBuffer, mainGridMesh, viewProj, glm::vec3{0.4f, 0.4f, 0.4f}, 2.0f);
        lineDrawer.draw(commandBuffer, subGridMesh, viewProj, glm::vec3{0.2f, 0.2f, 0.2f}, 1.0f);

        // Draw scene objects
        for (auto& object : scene.getObjects()) {
            if (const DirectionalLight* light = object.get<DirectionalLight>()) {
            }
        }

        commandBuffer.endRendering();
    }

    bool needsRecreate() const {
        vk::Extent3D extent = colorImage->getExtent();
        return static_cast<uint32_t>(width) != extent.width ||
               static_cast<uint32_t>(height) != extent.height;
    }

    const rv::Context* context = nullptr;
    IconManager* iconManager = nullptr;

    // Input
    glm::vec2 dragDelta = {0.0f, 0.0f};
    float mouseScroll = 0.0f;

    // Image
    float width = 0.0f;
    float height = 0.0f;
    rv::ImageHandle colorImage;
    rv::ImageHandle depthImage;
    vk::DescriptorSet imguiDescSet;

    // Line drawer
    LineDrawer lineDrawer;
    rv::Mesh mainGridMesh;
    rv::Mesh subGridMesh;

    // Gizmo
    ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
};