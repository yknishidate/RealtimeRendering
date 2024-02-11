#pragma once

class MainApp;

// TODO:
// rv側でWindowをAppから分離した方がいいかも
// ImGuiとGLFWの吸収はどこかで必要になるが、
// Appに依存するよりは綺麗に書けそう

// エディタモードとプレイモードによる差分を吸収し
// グローバルアクセスを提供する
class MainWindow {
public:
    static void init(MainApp& _app) {
        app = &_app;
    }

    static float getWidth();

    static float getHeight();

    static glm::vec2 getMouseDragLeft();

    static glm::vec2 getMouseDragRight();

    static float getMouseScroll();

    static inline MainApp* app = nullptr;
    static inline bool play = false;
};
