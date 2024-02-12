#pragma once

// エディタモードとプレイモードによる差分を吸収し
// グローバルアクセスを提供する
class MainWindow {
public:
    static float getWidth();

    static float getHeight();

    static glm::vec2 getMouseDragLeft();

    static glm::vec2 getMouseDragRight();

    static float getMouseScroll();

    static inline bool play = false;
};
