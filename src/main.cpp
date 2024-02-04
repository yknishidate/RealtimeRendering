
#include <future>
#include <reactive/App.hpp>

#include <nlohmann/json.hpp>

#include "RenderImages.hpp"
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

        images.createImages(context, width, height);

        scene.createTemplateMeshData(context);
        // scene.loadFromGltf(context, DEV_ASSET_DIR / "models" / "Sphere.gltf");
        scene.loadFromJson(context, DEV_ASSET_DIR / "scenes" / "two_boxes.json");

        renderer.init(context, images);
        viewportRenderer.init(context);

        editor.init(context);
        ViewportWindow::setAuxiliaryImage(renderer.getShadowMap());

        spdlog::info("Started: {} ms", timer.elapsedInMilli());
    }

    void onKey(int key, int scancode, int action, int mods) override {
        if (key == GLFW_KEY_P && action == GLFW_PRESS) {
            play = !play;
        }
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            terminate();
        }
    }

    void onUpdate() override {
        updateTimer.restart();

        auto& camera = scene.getCamera();
        for (int key : {GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_D, GLFW_KEY_A, GLFW_KEY_SPACE}) {
            if (isKeyDown(key)) {
                camera.processKey(key);
            }
        }

        // TODO: 入力を抽象化する
        if (play) {
            static glm::vec2 lastCursorPos{0.0f};
            glm::vec2 cursorPos = getCursorPos();
            glm::vec2 cursorOffset = cursorPos - lastCursorPos;
            lastCursorPos = cursorPos;
            if (isMouseButtonDown(GLFW_MOUSE_BUTTON_1)) {
                camera.processDragDelta(glm::vec2{cursorOffset.x, -cursorOffset.y} * 0.5f);
            }
            camera.processMouseScroll(getMouseWheel().y);
            resetMouseWheel();
        } else {
            camera.processDragDelta(ViewportWindow::dragDelta * 0.5f);
            camera.processMouseScroll(ViewportWindow::mouseScroll);
        }
        frame++;
        cpuTimes[1].second = updateTimer.elapsedInMilli();
    }

    void onRender(const rv::CommandBufferHandle& commandBuffer) override {
        renderTimer.restart();
        commandBuffer->clearColorImage(getCurrentColorImage(), {0.0f, 0.0f, 0.0f, 1.0f});

        if (play) {
            renderer.render(*commandBuffer, getCurrentColorImage(), images, scene, frame);
        } else {
            cpuTimes[0].second = cpuTimes[1].second + cpuTimes[2].second;
            if (editor.show(context, scene, cpuTimes, renderer.getRenderTimes()) ==
                rv::EditorMessage::RecompileRequested) {
                context.getDevice().waitIdle();
                renderer.init(context, images);
                ViewportWindow::setAuxiliaryImage(renderer.getShadowMap());
            }
            renderer.render(*commandBuffer, editor.getViewportImage(), images, scene, frame);
            viewportRenderer.render(*commandBuffer, editor.getViewportImage(), images, scene);
        }
        cpuTimes[2].second = renderTimer.elapsedInMilli();
    }

    int frame = 0;
    Scene scene;
    Renderer renderer;
    ViewportRenderer viewportRenderer;
    Editor editor;
    bool play = false;
    rv::CPUTimer updateTimer;
    rv::CPUTimer renderTimer;
    RenderImages images;
    std::vector<std::pair<std::string, float>> cpuTimes{
        {"CPU time", 0.0f},
        {"  Update", 0.0f},
        {"  Render", 0.0f},
    };
};

int main() {
    try {
        MainApp app{};
        app.run();
    } catch (const std::exception& e) {
        spdlog::error(e.what());
    }
}
