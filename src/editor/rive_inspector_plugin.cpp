#include "rive_inspector_plugin.h"
#include "rive_view_model_inspector.h"
#include "../scene/rive_control.h"
#include "../scene/rive_player.h"
#include "../rive_constants.h"
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_inspector.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <rive/text/font_hb.hpp>

using namespace godot;

void RiveInspectorPlugin::_bind_methods() {}

void RiveResourceEditorProperty::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_on_resource_changed", "resource"), &RiveResourceEditorProperty::_on_resource_changed);
    ClassDB::bind_method(D_METHOD("_on_resource_selected", "resource", "inspect"), &RiveResourceEditorProperty::_on_resource_selected);
}

RiveResourceEditorProperty::RiveResourceEditorProperty() {
    picker = memnew(EditorResourcePicker);
    picker->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    add_child(picker);
    add_focusable(picker);
    picker->connect("resource_changed", Callable(this, "_on_resource_changed"));
    picker->connect("resource_selected", Callable(this, "_on_resource_selected"));
}

void RiveResourceEditorProperty::setup(bool p_font_property) {
    font_property = p_font_property;
    picker->set_base_type(font_property ? "FontFile" : "Texture2D");
    picker->set_tooltip_text("Choose a resource override. Clearing or reverting restores the authored Rive/preset default.");
}

void RiveResourceEditorProperty::_update_property() {
    Object *object = get_edited_object();
    if (!object) return;
    updating = true;
    Ref<Resource> resource = object->get(get_edited_property());
    picker->set_edited_resource(resource);
    updating = false;
}

void RiveResourceEditorProperty::_set_read_only(bool p_read_only) { picker->set_editable(!p_read_only); }

void RiveResourceEditorProperty::_on_resource_changed(const Ref<Resource> &p_resource) {
    if (updating || is_read_only()) return;
    bool valid = true;
    if (p_resource.is_valid()) {
        if (font_property) {
            Ref<FontFile> font = p_resource;
            const PackedByteArray bytes = font.is_valid() ? font->get_data() : PackedByteArray();
            valid = !bytes.is_empty();
            if (valid) valid = static_cast<bool>(HBFont::Decode(rive::Span<const uint8_t>(bytes.ptr(), bytes.size())));
        } else {
            Ref<Texture2D> texture = p_resource;
            Ref<Image> image = texture.is_valid() ? texture->get_image() : Ref<Image>();
            valid = image.is_valid() && !image->is_empty();
        }
    }
    if (!valid) {
        UtilityFunctions::printerr("Rive Inspector rejected an invalid resource for '", get_edited_property(), "'.");
        _update_property();
        return;
    }
    emit_changed(get_edited_property(), p_resource);
}

void RiveResourceEditorProperty::_on_resource_selected(const Ref<Resource> &p_resource, bool p_inspect) {
    if (p_resource.is_valid() && p_inspect && EditorInterface::get_singleton()) EditorInterface::get_singleton()->edit_resource(p_resource);
}

bool RiveInspectorPlugin::_can_handle(Object *p_object) const {
    return Object::cast_to<RiveControl>(p_object) != nullptr;
}

void RiveInspectorPlugin::_parse_begin(Object *p_object) {
    RiveControl *control = Object::cast_to<RiveControl>(p_object);
    if (!control) return;

    RiveViewModelInspector *inspector = memnew(RiveViewModelInspector);
    inspector->set_structure_only(true);
    inspector->set_rive_control(control);
    add_custom_control(inspector);
}

bool RiveInspectorPlugin::_parse_property(Object *p_object, Variant::Type p_type, const String &p_name, PropertyHint p_hint_type,
        const String &p_hint_string, BitField<PropertyUsageFlags> p_usage_flags, bool p_wide) {
    if (!p_name.begins_with(RiveConstants::PROPERTY_PREFIX) || p_type != Variant::OBJECT ||
            (p_hint_string != "Texture2D" && p_hint_string != "FontFile")) return false;
    RiveResourceEditorProperty *editor = memnew(RiveResourceEditorProperty);
    editor->setup(p_hint_string == "FontFile");
    add_property_editor(p_name, editor);
    return true;
}
