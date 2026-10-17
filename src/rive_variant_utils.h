#ifndef RIVE_VARIANT_UTILS_H
#define RIVE_VARIANT_UTILS_H

#include <godot_cpp/variant/dictionary.hpp>

namespace rive_integration {

inline void assign_deep_dictionary_copy(godot::Dictionary &p_target, const godot::Dictionary &p_source) {
    // The pinned godot-cpp Dictionary move assignment leaks the previous value.
    const godot::Dictionary snapshot = p_source.duplicate(true);
    p_target = snapshot;
}

}

#endif
