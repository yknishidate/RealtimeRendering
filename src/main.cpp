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
              .style = rv::UIStyle::Gray,
          }) {}

    void onShutdown() override {
        editor.shutdown();
    }

    void onStart() override {
        rv::CPUTimer timer;
        std::filesystem::create_directories(DEV_SHADER_DIR / "spv");

        images.createImages(context, width, height);

        scene.init(context);
        scene.loadFromJson(DEV_ASSET_DIR / "scenes" / "pbr_helmet.json");

        renderer.init(context, images, swapchain->getFormat());
        viewportRenderer.init(context, swapchain->getFormat(), images.depthFormat);

        editor.init(context, swapchain->getFormat());
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
        if (!play) {
            editor.beginCpuUpdate();
        }

        if (pendingRecompile) {
            context.getDevice().waitIdle();
            renderer.init(context, images, swapchain->getFormat());
            ViewportWindow::setAuxiliaryImage(renderer.getShadowMap());
            pendingRecompile = false;
        }

        if (isKeyDown(GLFW_KEY_LEFT_CONTROL) && isKeyDown(GLFW_KEY_O)) {
            MenuBar::openScene(scene);
            scene.getCamera().setAspect(ViewportWindow::width / ViewportWindow::height);
        }

        auto& camera = scene.getCamera();
        for (int key : {GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_D, GLFW_KEY_A, GLFW_KEY_SPACE}) {
            if (isKeyDown(key)) {
                camera.processKey(key);
            }
        }

        if (play) {
            glm::vec2 _mouseDrag = getMouseDrag();
            camera.processDragDelta(glm::vec2{_mouseDrag.x, -_mouseDrag.y} * 0.5f);
            camera.processMouseScroll(getMouseScroll());
        } else {
            glm::vec2 _mouseDrag = ViewportWindow::dragDelta;
            camera.processDragDelta(glm::vec2{_mouseDrag.x, -_mouseDrag.y} * 0.5f);
            camera.processMouseScroll(ViewportWindow::mouseScroll);
        }
        frame++;

        if (!play) {
            editor.endCpuUpdate();
        }
    }

    void onRender(const rv::CommandBufferHandle& commandBuffer) override {
        if (play) {
            commandBuffer->clearColorImage(getCurrentColorImage(), {0.0f, 0.0f, 0.0f, 1.0f});
            renderer.render(*commandBuffer, getCurrentColorImage(), images, scene);
        } else {
            editor.beginCpuRender();
            commandBuffer->clearColorImage(getCurrentColorImage(), {0.0f, 0.0f, 0.0f, 1.0f});
            auto message = editor.show(context, scene, renderer);
            if (message & EditorMessage::RecompileRequested) {
                pendingRecompile = true;
            }
            if (message & EditorMessage::WindowResizeRequested) {
                context.getDevice().waitIdle();
                uint32_t _width = MenuBar::getWindowWidth();
                uint32_t _height = MenuBar::getWindowHeight();
                if (_width != 0 && _height != 0) {
                    setWindowSize(_width, _height);
                }
            }

            renderer.render(*commandBuffer, editor.getViewportImage(), images, scene);
            viewportRenderer.render(*commandBuffer, editor.getViewportImage(), images, scene);
            editor.endCpuRender();
        }
    }

    Scene scene;
    RenderImages images;
    Renderer renderer;
    ViewportRenderer viewportRenderer;
    Editor editor;
    int frame = 0;
    bool play = false;
    bool pendingRecompile = false;
};

int main() {
    try {
        MainApp app{};
        app.run();
    } catch (const std::exception& e) {
        spdlog::error(e.what());
    }
}
