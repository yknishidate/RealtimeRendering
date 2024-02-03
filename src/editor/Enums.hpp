#pragma once

#include <type_traits>

namespace rv {
enum class EditorMessage {
    None = 0,
    RecompileRequested = 1 << 0,
};

enum class SceneStatus {
    None = 0,
    ObjectAdded = 1 << 0,
    TextureAdded = 1 << 1,
};

template <typename Enum>
std::enable_if_t<std::is_enum<Enum>::value, Enum> operator|(Enum a, Enum b) {
    return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(a) |
                             static_cast<std::underlying_type_t<Enum>>(b));
}

template <typename Enum>
std::enable_if_t<std::is_enum<Enum>::value, Enum> operator&(Enum a, Enum b) {
    return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(a) &
                             static_cast<std::underlying_type_t<Enum>>(b));
}

template <typename Enum>
std::enable_if_t<std::is_enum<Enum>::value, Enum> operator^(Enum a, Enum b) {
    return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(a) ^
                             static_cast<std::underlying_type_t<Enum>>(b));
}

template <typename Enum>
std::enable_if_t<std::is_enum<Enum>::value, Enum> operator~(Enum a) {
    return static_cast<Enum>(~static_cast<std::underlying_type_t<Enum>>(a));
}

template <typename Enum>
std::enable_if_t<std::is_enum<Enum>::value, Enum&> operator|=(Enum& a, Enum b) {
    a = static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(a) |
                          static_cast<std::underlying_type_t<Enum>>(b));
    return a;
}
}  // namespace rv
