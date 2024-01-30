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
        createViewportImages(context);
    }

    void shutdown() {
        IconManager::clearIcons();
    }

    void show(const rv::Context& context, Scene& scene, Renderer& renderer) {
        if (needsRecreateViewportImages()) {
            context.getDevice().waitIdle();
            createViewportImages(context);
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
            ImGui::Text("Total frame: %f ms", cpuTimer.elapsedInMilli());
            cpuTimer.restart();

            ImGui::Text("Rendering: %f ms", renderer.getRenderingTimeMs());
            if (ImGui::Button("Recompile")) {
                context.getDevice().waitIdle();
                renderer.init(context);
                ViewportWindow::setAuxiliaryImage(renderer.getShadowMap());
            }
            ImGui::End();
        }

        SceneWindow::show(scene, &selectedObject);
        AttributeWindow::show(selectedObject);
        ViewportWindow::show(scene, imguiDescSets[currentImageIndex], selectedObject);
        AssetWindow::show(context, scene);

        ImGui::End();
    }

    void advanceImageIndex() {
        currentImageIndex = (currentImageIndex + 1) % imageCount;
    }

    bool needsRecreateViewportImages() const {
        vk::Extent3D extent = colorImages[currentImageIndex]->getExtent();
        return extent.width != static_cast<uint32_t>(ViewportWindow::width) ||  //
               extent.height != static_cast<uint32_t>(ViewportWindow::height);
    }

    void createViewportImages(const rv::Context& context) {
        width = ViewportWindow::width;
        height = ViewportWindow::height;

        for (int i = 0; i < imageCount; i++) {
            colorImages[i] = context.createImage({
                .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
                         vk::ImageUsageFlagBits::eTransferDst |
                         vk::ImageUsageFlagBits::eTransferSrc |
                         vk::ImageUsageFlagBits::eColorAttachment,
                .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                //.format = vk::Format::eR8G8B8A8Unorm,
                .format = vk::Format::eB8G8R8A8Unorm,
                .debugName = "ViewportRenderer::colorImage",
            });

            // Create desc set
            ImGui_ImplVulkan_RemoveTexture(imguiDescSets[i]);
            imguiDescSets[i] = ImGui_ImplVulkan_AddTexture(
                colorImages[i]->getSampler(), colorImages[i]->getView(), VK_IMAGE_LAYOUT_GENERAL);

            depthImages[i] = context.createImage({
                .usage = rv::ImageUsage::DepthAttachment,
                .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                .format = vk::Format::eD32Sfloat,
                .aspect = vk::ImageAspectFlagBits::eDepth,
                .debugName = "ViewportRenderer::depthImage",
            });
        }

        context.oneTimeSubmit([&](auto commandBuffer) {
            for (int i = 0; i < imageCount; i++) {
                commandBuffer->transitionLayout(colorImages[i], vk::ImageLayout::eGeneral);
                commandBuffer->transitionLayout(depthImages[i],
                                                vk::ImageLayout::eDepthAttachmentOptimal);
            }
        });
    }

    rv::ImageHandle getCurrentColorImage() const {
        return colorImages[currentImageIndex];
    }

    rv::ImageHandle getCurrentDepthImage() const {
        return depthImages[currentImageIndex];
    }

    // Image
    static constexpr int imageCount = 3;
    int currentImageIndex = 0;
    float width = 0.0f;
    float height = 0.0f;
    std::array<rv::ImageHandle, imageCount> colorImages;
    std::array<rv::ImageHandle, imageCount> depthImages;
    std::array<vk::DescriptorSet, imageCount> imguiDescSets;

    // Editor
    Object* selectedObject = nullptr;

    // Timer
    rv::CPUTimer cpuTimer;
};
