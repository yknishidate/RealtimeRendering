
#include <future>
#include <reactive/App.hpp>

#include <nlohmann/json.hpp>

#include "Renderer.hpp"
#include "Scene.hpp"
#include "ViewportRenderer.hpp"
#include "editor/Editor.hpp"

class MainApp final : public rv::App {
public:
    MainApp()
        : App({
              .width = 2560,
              .height = 1440,
              .title = "Main app",
              .vsync = false,
              .layers = {rv::Layer::Validation, rv::Layer::FPSMonitor},
              .extensions = {rv::Extension::RayTracing},
              .style = rv::UIStyle::Gray,
          }) {}

    virtual ~MainApp() = default;

    MainApp(const MainApp&) = delete;
    MainApp& operator=(const MainApp&) = delete;

    MainApp(MainApp&&) = delete;
    MainApp& operator=(MainApp&&) = delete;

    void onShutdown() override {
        editor.shutdown();
    }

    void onStart() override {
        rv::CPUTimer timer;
        std::filesystem::create_directories(DEV_SHADER_DIR / "spv");

        // Scene
        scene.setContext(context);
        scene.loadFromJson(DEV_ASSET_DIR / "scenes" / "two_boxes.json");

        // Renderer
        renderer.init(context);
        viewportRenderer.init(context);

        // Editor
        editor.init(context);
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

        if (play) {
            static glm::vec2 lastCursorPos{0.0f};
            glm::vec2 cursorPos = getCursorPos();
            glm::vec2 cursorOffset = cursorPos - lastCursorPos;
            lastCursorPos = cursorPos;
            if (isMouseButtonDown(GLFW_MOUSE_BUTTON_1)) {
                camera.processDragDelta({cursorOffset.x, -cursorOffset.y});
            }
            camera.processMouseScroll(getMouseWheel().y);
            resetMouseWheel();
        } else {
            camera.processDragDelta(ViewportWindow::dragDelta);
            camera.processMouseScroll(ViewportWindow::mouseScroll);
        }
        frame++;
    }

    void onRender(const rv::CommandBufferHandle& commandBuffer) override {
        commandBuffer->clearColorImage(getCurrentColorImage(), {0.0f, 0.0f, 0.0f, 1.0f});
        commandBuffer->clearDepthStencilImage(getDefaultDepthImage(), 1.0f, 0);

        if (play) {
            renderer.render(*commandBuffer,  //
                            getCurrentColorImage(),
                            getDefaultDepthImage(),  //
                            scene, frame);
        } else {
            editor.show(context, scene, renderer);
            renderer.render(*commandBuffer,  //
                            editor.getCurrentColorImage(),
                            editor.getCurrentDepthImage(),  //
                            scene, frame);
            viewportRenderer.render(*commandBuffer,                 //
                                    editor.getCurrentColorImage(),  //
                                    editor.getCurrentDepthImage(),  //
                                    scene);
        }
    }

    int frame = 0;
    Scene scene;
    Renderer renderer;
    ViewportRenderer viewportRenderer;
    Editor editor;
    bool play = true;
};

int main() {
    try {
        MainApp app{};
        app.run();
    } catch (const std::exception& e) {
        spdlog::error(e.what());
    }
}
