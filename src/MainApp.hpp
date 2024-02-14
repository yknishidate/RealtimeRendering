#pragma once
#include "RenderImages.hpp"
#include "Renderer.hpp"
#include "Scene.hpp"
#include "ViewportRenderer.hpp"
#include "WindowAdapter.hpp"
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
            WindowAdapter::play = !WindowAdapter::play;
        }
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            terminate();
        }
    }

    void onUpdate(float dt) override {
        if (!WindowAdapter::play) {
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

        // Editor
        if (!WindowAdapter::play) {
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

        if (!WindowAdapter::play) {
            editor.endCpuUpdate();
        }
    }

    void onRender(const rv::CommandBufferHandle& commandBuffer) override {
        if (WindowAdapter::play) {
            commandBuffer->clearColorImage(getCurrentColorImage(), {0.0f, 0.0f, 0.0f, 1.0f});
            renderer.render(*commandBuffer, getCurrentColorImage(), images, scene);
        } else {
            editor.beginCpuRender();
            commandBuffer->clearColorImage(getCurrentColorImage(), {0.0f, 0.0f, 0.0f, 1.0f});
            renderer.render(*commandBuffer, editor.getViewportImage(), images, scene);
            viewportRenderer.render(*commandBuffer, editor.getViewportImage(), images.depthImage,
                                    scene);
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
