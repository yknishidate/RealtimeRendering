#pragma once
#include "editor/AssetWindow.hpp"
#include "editor/AttributeWindow.hpp"
#include "editor/MenuBar.hpp"
#include "editor/SceneWindow.hpp"
#include "editor/ViewportWindow.hpp"

class Editor {
public:
    void init(const rv::Context& context) {
        // Editor
        IconManager::loadIcons(context);
        ViewportWindow::width = 1920;
        ViewportWindow::height = 1080;

        // Image
        createViewportImage(context);
    }

    void shutdown() {
        IconManager::clearIcons();
    }

    void show(const rv::Context& context, Scene& scene, Renderer& renderer) {
        if (needsRecreateViewportImage()) {
            context.getDevice().waitIdle();
            createViewportImage(context);
            scene.getCamera().setAspect(ViewportWindow::width / ViewportWindow::height);
        }

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
        windowFlags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        windowFlags |= ImGuiWindowFlags_MenuBar;

        ImGui::Begin("DockSpace", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        MenuBar::show(scene);

        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        if (ImGui::Begin("Misc")) {
            ImGui::Text("CPU time: %f ms", updateTime + renderTime);
            ImGui::Text("  Update: %f ms", updateTime);
            ImGui::Text("  Render: %f ms", renderTime);

            ImGui::Text("GPU time: %f ms", renderer.getRenderingTimeMs());
            if (ImGui::Button("Recompile")) {
                context.getDevice().waitIdle();
                renderer.init(context);
                ViewportWindow::setAuxiliaryImage(renderer.getShadowMap());
            }
            ImGui::End();
        }

        SceneWindow::show(scene, &selectedObject);
        AttributeWindow::show(selectedObject);
        ViewportWindow::show(scene, imguiDescSet, selectedObject);
        AssetWindow::show(context, scene);

        ImGui::End();
    }

    bool needsRecreateViewportImage() const {
        vk::Extent3D extent = colorImage->getExtent();
        return extent.width != static_cast<uint32_t>(ViewportWindow::width) ||  //
               extent.height != static_cast<uint32_t>(ViewportWindow::height);
    }

    void createViewportImage(const rv::Context& context) {
        width = ViewportWindow::width;
        height = ViewportWindow::height;

        colorImage = context.createImage({
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
                     vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc |
                     vk::ImageUsageFlagBits::eColorAttachment,
            .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
            .format = vk::Format::eB8G8R8A8Unorm,
            .debugName = "ViewportRenderer::colorImage",
        });

        // Create desc set
        ImGui_ImplVulkan_RemoveTexture(imguiDescSet);
        imguiDescSet = ImGui_ImplVulkan_AddTexture(colorImage->getSampler(), colorImage->getView(),
                                                   VK_IMAGE_LAYOUT_GENERAL);

        depthImage = context.createImage({
            .usage = rv::ImageUsage::DepthAttachment,
            .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
            .format = vk::Format::eD32Sfloat,
            .aspect = vk::ImageAspectFlagBits::eDepth,
            .debugName = "ViewportRenderer::depthImage",
        });

        context.oneTimeSubmit([&](auto commandBuffer) {
            commandBuffer->transitionLayout(colorImage, vk::ImageLayout::eGeneral);
            commandBuffer->transitionLayout(depthImage, vk::ImageLayout::eDepthAttachmentOptimal);
        });
    }

    rv::ImageHandle getColorImage() const {
        return colorImage;
    }

    rv::ImageHandle getDepthImage() const {
        return depthImage;
    }

    void setUpdateTime(float time) {
        updateTime = time;
    }

    void setRenderTime(float time) {
        renderTime = time;
    }

    // Image
    float width = 0.0f;
    float height = 0.0f;
    rv::ImageHandle colorImage;
    rv::ImageHandle depthImage;
    vk::DescriptorSet imguiDescSet;

    // Editor
    Object* selectedObject = nullptr;

    // Timer
    float updateTime = 0.0f;
    float renderTime = 0.0f;
};
