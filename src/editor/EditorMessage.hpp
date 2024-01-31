#pragma once

#include <type_traits>

enum class EditorMessage {
    None = 0,
    RecompileRequested = 1 << 0,
};

inline EditorMessage operator|(EditorMessage a, EditorMessage b) {
    return static_cast<EditorMessage>(static_cast<std::underlying_type_t<EditorMessage>>(a) |
                                      static_cast<std::underlying_type_t<EditorMessage>>(b));
}

inline EditorMessage operator&(EditorMessage a, EditorMessage b) {
    return static_cast<EditorMessage>(static_cast<std::underlying_type_t<EditorMessage>>(a) &
                                      static_cast<std::underlying_type_t<EditorMessage>>(b));
}

inline EditorMessage operator^(EditorMessage a, EditorMessage b) {
    return static_cast<EditorMessage>(static_cast<std::underlying_type_t<EditorMessage>>(a) ^
                                      static_cast<std::underlying_type_t<EditorMessage>>(b));
}

inline EditorMessage operator~(EditorMessage a) {
    return static_cast<EditorMessage>(~static_cast<std::underlying_type_t<EditorMessage>>(a));
}

inline EditorMessage& operator|=(EditorMessage& a, EditorMessage b) {
    a = static_cast<EditorMessage>(static_cast<std::underlying_type_t<EditorMessage>>(a) |
                                   static_cast<std::underlying_type_t<EditorMessage>>(b));
    return a;
}
