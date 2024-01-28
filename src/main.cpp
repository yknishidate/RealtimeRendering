
#include <future>
#include <reactive/App.hpp>

#include <nlohmann/json.hpp>

#include "Renderer.hpp"
#include "Scene.hpp"
#include "editor/AssetWindow.hpp"
#include "editor/AttributeWindow.hpp"
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
        rv::CPUTimer timer;
        scene.loadFromJson(context, DEV_ASSET_DIR / "scenes" / "two_boxes.json");

        iconManager.init(context);
        assetWindow.init(context, scene, iconManager);
        viewportWindow.init(context, iconManager, 1920, 1080);
        attributeWindow.init(context, scene, iconManager);

        renderer.init(context);
        spdlog::info("Started: {} ms", timer.elapsedInMilli());
    }

    void onUpdate() override {
        for (int key : {GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_D, GLFW_KEY_A, GLFW_KEY_SPACE}) {
            if (isKeyDown(key)) {
                scene.camera.processKey(key);
            }
        }

        scene.camera.processDragDelta(viewportWindow.dragDelta);
        scene.camera.processMouseScroll(viewportWindow.mouseScroll);
        frame++;
    }

    void onRender(const rv::CommandBufferHandle& commandBuffer) override {
        if (viewportWindow.needsRecreate()) {
            context.getDevice().waitIdle();
            viewportWindow.createImages(static_cast<uint32_t>(viewportWindow.width),
                                        static_cast<uint32_t>(viewportWindow.height));
            scene.camera.setAspect(viewportWindow.width / viewportWindow.height);
        }
        static bool dockspaceOpen = true;
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

        if (dockspaceOpen) {
            ImGui::Begin("DockSpace", &dockspaceOpen, windowFlags);
            ImGui::PopStyleVar(3);

            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */
                    }
                    if (ImGui::MenuItem("Save", "Ctrl+S")) { /* Do stuff */
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Edit")) {
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Create")) {
                    if (ImGui::MenuItem("Directional light")) {
                        if (!scene.findObject(Object::Type::DirectionalLight)) {
                            scene.addDirectionalLight();
                        }
                    }
                    if (ImGui::MenuItem("Point light")) {
                        scene.addPointLight();
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

            if (frame > 1) {
                if (ImGui::Begin("Performance")) {
                    ImGui::Text("Rendering: %f ms", renderer.getRenderingTimeMs());
                    ImGui::End();
                }
            }

            sceneWindow.show(scene, &selectedObject);
            int message = Message::None;
            message |= attributeWindow.show(selectedObject);
            message |= viewportWindow.show(scene, selectedObject, frame);
            assetWindow.show();

            renderer.render(*commandBuffer, viewportWindow.colorImage, viewportWindow.depthImage,
                            scene, frame);
            viewportWindow.drawGrid(*commandBuffer, scene.camera);

            ImGui::End();
        }
    }

    // Scene
    Scene scene;
    int frame = 0;

    // Renderer
    Renderer renderer;

    // ImGui
    Object* selectedObject = nullptr;

    // Editor
    SceneWindow sceneWindow;
    ViewportWindow viewportWindow;
    AttributeWindow attributeWindow;
    AssetWindow assetWindow;
    IconManager iconManager;
};

int main() {
    try {
        Editor app{};
        app.run();
    } catch (const std::exception& e) {
        spdlog::error(e.what());
    }
}
