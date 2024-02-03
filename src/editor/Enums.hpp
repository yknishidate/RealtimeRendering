#pragma once

#include <type_traits>

namespace rv {

template <typename Enum>
class Flags {
public:
    using underlying_type = std::underlying_type_t<Enum>;

private:
    underlying_type value;

public:
    Flags(Enum v) : value(static_cast<underlying_type>(v)) {}
    Flags(underlying_type v) : value(v) {}

    // ビット演算の結果をboolに変換する演算子
    operator bool() const {
        return value != static_cast<underlying_type>(Enum::None);
    }

    // Flags同士のビットOR演算
    friend Flags operator|(Enum e1, Enum e2) {
        return Flags(static_cast<underlying_type>(e1) | static_cast<underlying_type>(e2));
    }

    friend Flags operator&(Enum e1, Enum e2) {
        return Flags(static_cast<underlying_type>(e1) & static_cast<underlying_type>(e2));
    }

    friend Flags operator~(Enum e) {
        return Flags(~static_cast<underlying_type>(e));
    }

    friend Flags operator|(Flags e1, Flags e2) {
        return Flags(e1.value | e2.value);
    }

    friend Flags operator^(Flags e1, Flags e2) {
        return Flags(e1.value ^ e2.value);
    }

    Flags operator&(Enum e) const {
        return Flags(static_cast<Enum>(value & static_cast<underlying_type>(e)));
    }

    Flags operator|(Enum e) const {
        return Flags(static_cast<Enum>(value | static_cast<underlying_type>(e)));
    }

    Flags& operator|=(Enum e) {
        value |= static_cast<underlying_type>(e);
        return *this;
    }
};

enum class EditorMessage {
    None = 0,
    RecompileRequested = 1 << 0,
};

enum class SceneStatus {
    None = 0,
    ObjectAdded = 1 << 0,
    Texture2DAdded = 1 << 1,
    TextureCubeAdded = 1 << 2,
};

using EditorMessageFlags = Flags<EditorMessage>;
using SceneStatusFlags = Flags<SceneStatus>;

}  // namespace rv
