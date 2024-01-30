
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

        scene.setContext(context);
        scene.loadFromJson(DEV_ASSET_DIR / "scenes" / "two_boxes.json");

        IconManager::loadIcons(context);

        viewportRenderer.init(context, 1920, 1080);
        renderer.init(context);
        ViewportWindow::setAuxiliaryImage(renderer.getShadowMap());

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

    void onRender(const rv::CommandBufferHandle& commandBuffer) override {
        if (viewportRenderer.needsRecreate()) {
            context.getDevice().waitIdle();
            viewportRenderer.createImages(static_cast<uint32_t>(ViewportWindow::width),
                                          static_cast<uint32_t>(ViewportWindow::height));
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

        MenuBar::show(scene, &ViewportWindow::isWidgetsVisible);

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
        message |= ViewportWindow::show(scene, viewportRenderer.getCurrentImageDescSet(),
                                        selectedObject, frame);
        AssetWindow::show(context, scene);

        renderer.render(*commandBuffer,  //
                        viewportRenderer.getCurrentColorImage(),
                        viewportRenderer.getCurrentDepthImage(), scene, frame);
        viewportRenderer.drawContents(*commandBuffer, scene);
        viewportRenderer.advanceImageIndex();

        ImGui::End();
    }

    // Scene
    int frame = 0;
    Scene scene;

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
