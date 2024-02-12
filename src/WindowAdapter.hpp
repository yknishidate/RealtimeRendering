#pragma once
#include "editor/ViewportWindow.hpp"
#include "reactive/Window.hpp"

// エディタモードとプレイモードによる差分を吸収する
class WindowAdapter {
public:
    static float getWidth() {
        if (play) {
            return static_cast<float>(rv::Window::getWidth());
        } else {
            return ViewportWindow::width;
        }
    }

    static float getHeight() {
        if (play) {
            return static_cast<float>(rv::Window::getHeight());
        } else {
            return ViewportWindow::height;
        }
    }

    static glm::vec2 getMouseDragLeft() {
        if (play) {
            return rv::Window::getMouseDragLeft();
        } else {
            return ViewportWindow::dragDeltaLeft;
        }
    }

    static glm::vec2 getMouseDragRight() {
        if (play) {
            return rv::Window::getMouseDragRight();
        } else {
            return ViewportWindow::dragDeltaRight;
        }
    }

    static float getMouseScroll() {
        if (play) {
            return rv::Window::getMouseScroll();
        } else {
            return ViewportWindow::mouseScroll;
        }
    }

    static inline bool play = false;
};
