#include "MainWindow.hpp"

#include "MainApp.hpp"

float MainWindow::getWidth() {
    if (play) {
        return static_cast<float>(app->getWindowWidth());
    } else {
        return ViewportWindow::width;
    }
}

float MainWindow::getHeight() {
    if (play) {
        return static_cast<float>(app->getWindowHeight());
    } else {
        return ViewportWindow::height;
    }
}

glm::vec2 MainWindow::getMouseDragLeft() {
    if (play) {
        return app->getMouseDragLeft();
    } else {
        return ViewportWindow::dragDeltaLeft;
    }
}

glm::vec2 MainWindow::getMouseDragRight() {
    if (play) {
        return app->getMouseDragRight();
    } else {
        return ViewportWindow::dragDeltaRight;
    }
}

float MainWindow::getMouseScroll() {
    if (play) {
        return app->getMouseScroll();
    } else {
        return ViewportWindow::mouseScroll;
    }
}
