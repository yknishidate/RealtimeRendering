#pragma once
#include "editor/AssetWindow.hpp"
#include "editor/AttributeWindow.hpp"
#include "editor/Enums.hpp"
#include "editor/MenuBar.hpp"
#include "editor/SceneWindow.hpp"
#include "editor/ViewportWindow.hpp"

class Editor {
public:
    void init(const rv::Context& context, vk::Format _colorFormat) {
        colorFormat = _colorFormat;

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

    void beginCpuUpdate() {
        updateTimer.restart();
    }

    void endCpuUpdate() {
        cpuUpdateTime = updateTimer.elapsedInMilli();
    }

    void beginCpuRender() {
        renderTimer.restart();
    }

    void endCpuRender() {
        cpuRenderTime = renderTimer.elapsedInMilli();
    }

    static void showTime(const char* label, float time) {
        ImGui::Text(label);
        ImGui::SameLine(150);
        ImGui::Text("%6.3f ms", time);
    }

    EditorMessageFlags showMiscWindow(const rv::Context& context,
                                      Scene& scene,
                                      Renderer& renderer) {
        EditorMessageFlags message = EditorMessage::None;
        if (ImGui::Begin("Misc")) {
            showTime("CPU time", cpuUpdateTime + cpuRenderTime);
            showTime("  Update", cpuUpdateTime);
            showTime("  Render", cpuRenderTime);

            float shadowTime = renderer.getPassTimeShadow();
            float skyTime = renderer.getPassTimeSkybox();
            float forwardTime = renderer.getPassTimeForward();
            float ssrTime = renderer.getPassTimeSSR();
            float aaTime = renderer.getPassTimeAA();

            showTime("GPU time", shadowTime + skyTime + forwardTime + ssrTime + aaTime);
            showTime("  Shadow map", shadowTime);
            showTime("  Skybox", skyTime);
            showTime("  Forward", forwardTime);
            showTime("  SSR", ssrTime);
            showTime("  FXAA", aaTime);

            if (ImGui::Button("Recompile")) {
                message = EditorMessage::RecompileRequested;
            }
            if (ImGui::Button("Clear scene")) {
                context.getDevice().waitIdle();
                scene.clear();
            }
            ImGui::End();
        }
        return message;
    }

    EditorMessageFlags show(const rv::Context& context, Scene& scene, Renderer& renderer) {
        EditorMessageFlags message = EditorMessage::None;
        if (needsRecreateViewportImage()) {
            context.getDevice().waitIdle();
            createViewportImage(context);
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

        message |= MenuBar::show(scene);

        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        message |= showMiscWindow(context, scene, renderer);

        SceneWindow::show(scene, &selectedObject);
        AttributeWindow::show(scene, selectedObject);
        ViewportWindow::show(scene, imguiDescSet, selectedObject);
        AssetWindow::show(context, scene);

        ImGui::End();
        return message;
    }

    bool needsRecreateViewportImage() const {
        vk::Extent3D extent = viewportImage->getExtent();
        return extent.width != static_cast<uint32_t>(ViewportWindow::width) ||  //
               extent.height != static_cast<uint32_t>(ViewportWindow::height);
    }

    void createViewportImage(const rv::Context& context) {
        width = ViewportWindow::width;
        height = ViewportWindow::height;

        viewportImage = context.createImage({
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
                     vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc |
                     vk::ImageUsageFlagBits::eColorAttachment,
            .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
            .format = colorFormat,
            .debugName = "ViewportRenderer::colorImage",
        });
        viewportImage->createImageView();
        viewportImage->createSampler();

        // Create desc set
        ImGui_ImplVulkan_RemoveTexture(imguiDescSet);
        imguiDescSet = ImGui_ImplVulkan_AddTexture(
            viewportImage->getSampler(), viewportImage->getView(), VK_IMAGE_LAYOUT_GENERAL);

        context.oneTimeSubmit([&](auto commandBuffer) {
            commandBuffer->transitionLayout(viewportImage, vk::ImageLayout::eGeneral);
        });
    }

    rv::ImageHandle getViewportImage() const {
        return viewportImage;
    }

    // Image
    float width = 0.0f;
    float height = 0.0f;
    rv::ImageHandle viewportImage{};
    vk::DescriptorSet imguiDescSet{};
    vk::Format colorFormat{};

    // Editor
    Object* selectedObject = nullptr;

    rv::CPUTimer updateTimer;
    rv::CPUTimer renderTimer;
    float cpuUpdateTime = 0.0f;
    float cpuRenderTime = 0.0f;
};
