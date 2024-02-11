#include "MainApp.hpp"

int main() {
    try {
        MainApp app{};
        app.run();
    } catch (const std::exception& e) {
        spdlog::error(e.what());
    }
}
