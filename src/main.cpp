
#include <future>
#include <reactive/App.hpp>

#include <nlohmann/json.hpp>

#include "Renderer.hpp"
#include "Scene.hpp"
#include "ViewportRenderer.hpp"
#include "editor/AssetWindow.hpp"
#include "editor/AttributeWindow.hpp"
#include "editor/MenuBar.hpp"
#include "editor/SceneWindow.hpp"
#include "editor/ViewportWindow.hpp"

class Editor : public rv::App {
public:
    Editor()
        : App({
              .width = 2560,
              .height = 1440,
              .title = "Reactive Editor",
              .vsync = false,
              .layers = {rv::Layer::Validation, rv::Layer::FPSMonitor},
              .extensions = {rv::Extension::RayTracing},
              .style = rv::UIStyle::Gray,
          }) {}

    void onShutdown() override {
        IconManager::clearIcons();
    }

    void onStart() override {
        std::filesystem::create_directories(DEV_SHADER_DIR / "spv");

        rv::CPUTimer timer;

        // Scene
        scene.setContext(context);
        scene.loadFromJson(DEV_ASSET_DIR / "scenes" / "two_boxes.json");

        // Renderer
        viewportRenderer.init(context);
        renderer.init(context);

        // Editor
        IconManager::loadIcons(context);
        ViewportWindow::setAuxiliaryImage(renderer.getShadowMap());
        ViewportWindow::width = 1920;
        ViewportWindow::height = 1080;

        // Image
        createViewportImages();

        spdlog::info("Started: {} ms", timer.elapsedInMilli());
    }

    void onUpdate() override {
        auto& camera = scene.getCamera();
        for (int key : {GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_D, GLFW_KEY_A, GLFW_KEY_SPACE}) {
            if (isKeyDown(key)) {
                camera.processKey(key);
            }
        }

        camera.processDragDelta(ViewportWindow::dragDelta);
        camera.processMouseScroll(ViewportWindow::mouseScroll);
        frame++;
    }

    bool needsRecreateViewportImages() const {
        vk::Extent3D extent = colorImages[currentImageIndex]->getExtent();
        return extent.width != static_cast<uint32_t>(ViewportWindow::width) ||  //
               extent.height != static_cast<uint32_t>(ViewportWindow::height);
    }

    void onRender(const rv::CommandBufferHandle& commandBuffer) override {
        if (needsRecreateViewportImages()) {
            context.getDevice().waitIdle();
            createViewportImages();
            scene.getCamera().setAspect(ViewportWindow::width / ViewportWindow::height);
        }
        commandBuffer->clearColorImage(getCurrentColorImage(), {0.0f, 0.0f, 0.0f, 1.0f});

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

            if (frame > 1) {
                ImGui::Text("Rendering: %f ms", renderer.getRenderingTimeMs());
            }
            if (ImGui::Button("Recompile")) {
                context.getDevice().waitIdle();
                renderer.init(context);
                ViewportWindow::setAuxiliaryImage(renderer.getShadowMap());
            }
            ImGui::End();
        }

        SceneWindow::show(scene, &selectedObject);
        int message = Message::None;
        message |= AttributeWindow::show(selectedObject);
        message |=
            ViewportWindow::show(scene, imguiDescSets[currentImageIndex], selectedObject, frame);
        AssetWindow::show(context, scene);

        renderer.render(*commandBuffer,                                                  //
                        colorImages[currentImageIndex], depthImages[currentImageIndex],  //
                        scene, frame);
        viewportRenderer.render(*commandBuffer,  //
                                colorImages[currentImageIndex], depthImages[currentImageIndex],
                                scene);

        currentImageIndex = (currentImageIndex + 1) % imageCount;

        ImGui::End();
    }

    void createViewportImages() {
        width = ViewportWindow::width;
        height = ViewportWindow::height;

        for (int i = 0; i < imageCount; i++) {
            colorImages[i] = context.createImage({
                .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage |
                         vk::ImageUsageFlagBits::eTransferDst |
                         vk::ImageUsageFlagBits::eTransferSrc |
                         vk::ImageUsageFlagBits::eColorAttachment,
                .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                .format = vk::Format::eR8G8B8A8Unorm,
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

    // Scene
    int frame = 0;
    Scene scene;

    // Image
    static constexpr int imageCount = 3;
    int currentImageIndex = 0;
    float width = 0.0f;
    float height = 0.0f;
    std::array<rv::ImageHandle, imageCount> colorImages;
    std::array<rv::ImageHandle, imageCount> depthImages;
    std::array<vk::DescriptorSet, imageCount> imguiDescSets;

    // Renderer
    Renderer renderer;
    ViewportRenderer viewportRenderer;

    // Editor
    Object* selectedObject = nullptr;

    // Misc
    rv::CPUTimer cpuTimer;
};

int main() {
    try {
        Editor app{};
        app.run();
    } catch (const std::exception& e) {
        spdlog::error(e.what());
    }
}
