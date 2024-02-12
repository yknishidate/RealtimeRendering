#include "MainWindow.hpp"

#include "MainApp.hpp"
#include "reactive/Window.hpp"

float MainWindow::getWidth() {
    if (play) {
        return static_cast<float>(rv::Window::getWidth());
    } else {
        return ViewportWindow::width;
    }
}

float MainWindow::getHeight() {
    if (play) {
        return static_cast<float>(rv::Window::getHeight());
    } else {
        return ViewportWindow::height;
    }
}

glm::vec2 MainWindow::getMouseDragLeft() {
    if (play) {
        return rv::Window::getMouseDragLeft();
    } else {
        return ViewportWindow::dragDeltaLeft;
    }
}

glm::vec2 MainWindow::getMouseDragRight() {
    if (play) {
        return rv::Window::getMouseDragRight();
    } else {
        return ViewportWindow::dragDeltaRight;
    }
}

float MainWindow::getMouseScroll() {
    if (play) {
        return rv::Window::getMouseScroll();
    } else {
        return ViewportWindow::mouseScroll;
    }
}
