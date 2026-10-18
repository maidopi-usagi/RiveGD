#ifndef RIVE_VIEW_MODEL_PRESET_H
#define RIVE_VIEW_MODEL_PRESET_H

#include "rive_file.h"
#include "../scene/rive_view_model.h"
#include <unordered_set>

class RiveViewModelPreset : public Resource {
    GDCLASS(RiveViewModelPreset, Resource);
    String view_model_name;
    String instance_name;
    int instance_index = -1;
    bool blank = false;
    Dictionary values;
    Ref<RiveViewModelInstance> instantiate_recursive(const Ref<RiveFile> &p_file,
            const String &p_artboard, const String &p_inferred_model,
            std::unordered_set<const RiveViewModelPreset *> &p_active, int p_depth) const;
protected:
    static void _bind_methods();
public:
    static Ref<RiveViewModelPreset> from_instance(const Ref<RiveViewModelInstance> &p_instance);
    static Ref<RiveViewModelPreset> _capture_instance(const Ref<RiveViewModelInstance> &p_instance,
            const Ref<RiveViewModelInstance> &p_property_root, const String &p_prefix);
    void set_view_model_name(const String &p_name);
    String get_view_model_name() const { return view_model_name; }
    void set_instance_name(const String &p_name);
    String get_instance_name() const { return instance_name; }
    void set_instance_index(int p_index);
    int get_instance_index() const { return instance_index; }
    void set_blank(bool p_blank);
    bool is_blank() const { return blank; }
    void set_values(const Dictionary &p_values);
    Dictionary get_values() const { return values.duplicate(true); }
    Ref<RiveViewModelInstance> instantiate(const Ref<RiveFile> &p_file, const String &p_artboard_name = "") const;
};

#endif
