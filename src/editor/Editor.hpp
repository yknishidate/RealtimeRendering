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

    EditorMessageFlags show(const rv::Context& context,
                            Scene& scene,
                            const std::vector<std::pair<std::string, float>>& cpuTimes,
                            const std::vector<std::pair<std::string, float>>& gpuTimes) {
        EditorMessageFlags message = EditorMessage::None;
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

        message |= MenuBar::show(scene);
        if (message & EditorMessage::SceneOpened) {
            scene.getCamera().setAspect(ViewportWindow::width / ViewportWindow::height);
        }

        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        if (ImGui::Begin("Misc")) {
            for (const auto& [label, time] : cpuTimes) {
                ImGui::Text(label.c_str(), time);
                ImGui::SameLine(150);
                ImGui::Text("%.3f ms", time);
            }
            for (const auto& [label, time] : gpuTimes) {
                ImGui::Text(label.c_str(), time);
                ImGui::SameLine(150);
                ImGui::Text("%.3f ms", time);
            }
            if (ImGui::Button("Recompile")) {
                // ViewportWindow::show()でImGui::Image()にDescSetを渡すと
                // Rendererを初期化したときにDescSetが古くなって壊れる
                // それを防ぐため早期リターンするが、ImGui::Begin()の数だけEnd()しておく
                ImGui::End();
                ImGui::End();
                return EditorMessage::RecompileRequested;
            }
            if (ImGui::Button("Clear scene")) {
                context.getDevice().waitIdle();
                scene.clear();
            }
            ImGui::End();
        }

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
};
