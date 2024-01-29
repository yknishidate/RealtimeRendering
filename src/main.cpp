
#include <future>
#include <reactive/App.hpp>

#include <nlohmann/json.hpp>

#include "Renderer.hpp"
#include "Scene.hpp"
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
              .layers = {rv::Layer::Validation},
              .extensions = {rv::Extension::RayTracing},
          }) {}

    void onStart() override {
        std::filesystem::create_directories(DEV_SHADER_DIR / "spv");

        rv::CPUTimer timer;

        scene.setContext(context);
        scene.loadFromJson(DEV_ASSET_DIR / "scenes" / "two_boxes.json");

        iconManager.init(context);
        assetWindow.init(context, scene, iconManager);
        viewportWindow.init(context, iconManager, 1920, 1080);
        attributeWindow.init(context, scene, iconManager);

        renderer.init(context);
        viewportWindow.setAuxiliaryImage(renderer.getShadowMap());

        spdlog::info("Started: {} ms", timer.elapsedInMilli());
    }

    void onUpdate() override {
        auto& camera = scene.getCamera();
        for (int key : {GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_D, GLFW_KEY_A, GLFW_KEY_SPACE}) {
            if (isKeyDown(key)) {
                camera.processKey(key);
            }
        }

        camera.processDragDelta(viewportWindow.dragDelta);
        camera.processMouseScroll(viewportWindow.mouseScroll);
        frame++;
    }

    void onRender(const rv::CommandBufferHandle& commandBuffer) override {
        if (viewportWindow.needsRecreate()) {
            context.getDevice().waitIdle();
            viewportWindow.createImages(static_cast<uint32_t>(viewportWindow.width),
                                        static_cast<uint32_t>(viewportWindow.height));
            scene.getCamera().setAspect(viewportWindow.width / viewportWindow.height);
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

        menuBar.show(scene, &viewportWindow.isWidgetsVisible);

        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        if (frame > 1) {
            if (ImGui::Begin("Misc")) {
                ImGui::Text("Rendering: %f ms", renderer.getRenderingTimeMs());
                if (ImGui::Button("Recompile")) {
                    context.getDevice().waitIdle();
                    renderer.init(context);
                    viewportWindow.setAuxiliaryImage(renderer.getShadowMap());
                }
                ImGui::End();
            }
        }

        sceneWindow.show(scene, &selectedObject);
        int message = Message::None;
        message |= attributeWindow.show(selectedObject);
        message |= viewportWindow.show(scene, selectedObject, frame);
        assetWindow.show();

        renderer.render(*commandBuffer,  //
                        viewportWindow.getCurrentColorImage(),
                        viewportWindow.getCurrentDepthImage(), scene, frame);
        viewportWindow.drawContents(*commandBuffer, scene);
        viewportWindow.advanceImageIndex();

        ImGui::End();
    }

    // Scene
    int frame = 0;
    Scene scene;

    // Renderer
    Renderer renderer;

    // Editor
    Object* selectedObject = nullptr;
    SceneWindow sceneWindow;
    ViewportWindow viewportWindow;
    AttributeWindow attributeWindow;
    AssetWindow assetWindow;
    IconManager iconManager;
    MenuBar menuBar;
};

int main() {
    try {
        Editor app{};
        app.run();
    } catch (const std::exception& e) {
        spdlog::error(e.what());
    }
}
