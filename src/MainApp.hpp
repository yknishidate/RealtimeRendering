#pragma once
#include "MainWindow.hpp"
#include "RenderImages.hpp"
#include "Renderer.hpp"
#include "Scene.hpp"
#include "ViewportRenderer.hpp"
#include "editor/Editor.hpp"
#include "reactive/Window.hpp"

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

        images.createImages(context, rv::Window::getWidth(), rv::Window::getHeight());

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
            MainWindow::play = !MainWindow::play;
        }
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            terminate();
        }
    }

    void onUpdate(float dt) override {
        if (!MainWindow::play) {
            editor.beginCpuUpdate();
        }

        if (pendingRecompile) {
            context.getDevice().waitIdle();
            renderer.init(context, images, swapchain->getFormat());
            ViewportWindow::setAuxiliaryImage(renderer.getShadowMap());
            pendingRecompile = false;
        }

        if (rv::Window::isKeyDown(GLFW_KEY_LEFT_CONTROL) && rv::Window::isKeyDown(GLFW_KEY_O)) {
            MenuBar::openScene(scene);
        }

        // Camera
        auto& camera = scene.getCamera();
        for (int key : {GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_D, GLFW_KEY_A, GLFW_KEY_SPACE}) {
            if (rv::Window::isKeyDown(key)) {
                camera.processKey(key);
            }
        }
        glm::vec2 _mouseDragLeft = MainWindow::getMouseDragLeft();
        glm::vec2 _mouseDragRight = MainWindow::getMouseDragRight();
        camera.processMouseDragLeft(glm::vec2{_mouseDragLeft.x, -_mouseDragLeft.y} * 0.5f);
        camera.processMouseDragRight(glm::vec2{_mouseDragRight.x, -_mouseDragRight.y} * 0.5f);
        camera.processMouseScroll(MainWindow::getMouseScroll());

        // Editor
        if (!MainWindow::play) {
            auto message = editor.show(context, scene, renderer);
            if (message & EditorMessage::RecompileRequested) {
                pendingRecompile = true;
            }
            if (message & EditorMessage::WindowResizeRequested) {
                context.getDevice().waitIdle();
                uint32_t _width = MenuBar::getWindowWidth();
                uint32_t _height = MenuBar::getWindowHeight();
                if (_width != 0 && _height != 0) {
                    rv::Window::setSize(_width, _height);
                }
            }
        }

        // Update
        scene.update(dt);

        frame++;

        if (!MainWindow::play) {
            editor.endCpuUpdate();
        }
    }

    void onRender(const rv::CommandBufferHandle& commandBuffer) override {
        if (MainWindow::play) {
            commandBuffer->clearColorImage(getCurrentColorImage(), {0.0f, 0.0f, 0.0f, 1.0f});
            renderer.render(*commandBuffer, getCurrentColorImage(), images, scene);
        } else {
            editor.beginCpuRender();
            commandBuffer->clearColorImage(getCurrentColorImage(), {0.0f, 0.0f, 0.0f, 1.0f});
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
    bool pendingRecompile = false;
};
