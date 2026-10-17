#include "rive_view_model_inspector.h"
#include "../scene/rive_player.h"
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/color_picker_button.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_inspector.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/editor_resource_picker.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <rive/viewmodel/viewmodel_instance.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include <rive/viewmodel/viewmodel_instance_string.hpp>
#include <rive/viewmodel/viewmodel_instance_boolean.hpp>
#include <rive/viewmodel/viewmodel_instance_trigger.hpp>
#include <rive/viewmodel/viewmodel_instance_enum.hpp>
#include <rive/viewmodel/viewmodel_instance_color.hpp>
#include <rive/viewmodel/viewmodel_property_enum.hpp>
#include <rive/viewmodel/data_enum.hpp>
#include <rive/viewmodel/data_enum_value.hpp>
#include <rive/viewmodel/viewmodel_instance_viewmodel.hpp>
#include <rive/viewmodel/viewmodel_instance_list.hpp>
#include <rive/viewmodel/viewmodel_instance_artboard.hpp>
#include <rive/viewmodel/viewmodel_instance_asset_image.hpp>
#include <rive/viewmodel/viewmodel_instance_asset_font.hpp>
#include <rive/viewmodel/viewmodel_instance_asset_blob.hpp>
#include <rive/viewmodel/viewmodel_instance_symbol_list_index.hpp>
#include <rive/viewmodel/viewmodel_property.hpp>
#include <rive/text/font_hb.hpp>
#include <algorithm>

using namespace godot;

namespace {
template <typename T>
rive::ViewModelInstanceValue *view_model_value_ptr(T *p_value) {
    return p_value;
}

template <typename T>
rive::ViewModelInstanceValue *view_model_value_ptr(const rive::rcp<T> &p_value) {
    return p_value.get();
}
}

void RiveViewModelInspector::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_rive_control", "control"), &RiveViewModelInspector::set_rive_control);
    ClassDB::bind_method(D_METHOD("_on_prev_page"), &RiveViewModelInspector::_on_prev_page);
    ClassDB::bind_method(D_METHOD("_on_next_page"), &RiveViewModelInspector::_on_next_page);
    ClassDB::bind_method(D_METHOD("_queue_refresh"), &RiveViewModelInspector::_queue_refresh);
    ClassDB::bind_method(D_METHOD("_refresh"), &RiveViewModelInspector::_refresh);
    ClassDB::bind_method(D_METHOD("_on_reset_property", "path"), &RiveViewModelInspector::_on_reset_property);
    
    ClassDB::bind_method(D_METHOD("_on_number_changed", "value", "path"), &RiveViewModelInspector::_on_number_changed);
    ClassDB::bind_method(D_METHOD("_on_string_changed", "value", "path"), &RiveViewModelInspector::_on_string_changed);
    ClassDB::bind_method(D_METHOD("_on_bool_changed", "value", "path"), &RiveViewModelInspector::_on_bool_changed);
    ClassDB::bind_method(D_METHOD("_on_color_changed", "value", "path"), &RiveViewModelInspector::_on_color_changed);
    ClassDB::bind_method(D_METHOD("_on_enum_changed", "value", "path", "indices"), &RiveViewModelInspector::_on_enum_changed);
    ClassDB::bind_method(D_METHOD("_on_artboard_changed", "value", "path", "names"), &RiveViewModelInspector::_on_artboard_changed);
    ClassDB::bind_method(D_METHOD("_on_resource_changed", "resource", "path"), &RiveViewModelInspector::_on_resource_changed);
    ClassDB::bind_method(D_METHOD("_on_resource_selected", "resource", "inspect"), &RiveViewModelInspector::_on_resource_selected);
    ClassDB::bind_method(D_METHOD("_on_clear_list", "path"), &RiveViewModelInspector::_on_clear_list);
    ClassDB::bind_method(D_METHOD("_on_trigger_fired", "path"), &RiveViewModelInspector::_on_trigger_fired);
}

void RiveViewModelInspector::set_rive_control(RiveControl *p_control) {
    const Callable refresh_callback(this, "_queue_refresh");
    if (RiveControl *previous = _get_control()) {
        if (previous->is_connected("property_values_changed", refresh_callback)) {
            previous->disconnect("property_values_changed", refresh_callback);
        }
        if (previous->is_connected("loaded", refresh_callback)) previous->disconnect("loaded", refresh_callback);
        if (previous->is_connected("view_model_changed", refresh_callback)) previous->disconnect("view_model_changed", refresh_callback);
    }
    if (p_control) {
        control_id = p_control->get_instance_id();
    } else {
        control_id = ObjectID();
    }
    if (p_control && p_control->has_signal("property_values_changed")) {
        p_control->connect("property_values_changed", refresh_callback);
        p_control->connect("loaded", refresh_callback);
        p_control->connect("view_model_changed", refresh_callback);
    }
    current_page = 0;
    _update_ui();
}

RiveControl *RiveViewModelInspector::_get_control() const {
    if (control_id.is_null()) return nullptr;
    Object *obj = ObjectDB::get_instance(control_id);
    return Object::cast_to<RiveControl>(obj);
}

void RiveViewModelInspector::_collect_properties(rive::ViewModelInstance *vm, String prefix, int indent, std::vector<PropertyEntry> &out_list, std::vector<rive::ViewModelInstance *> &ancestors) {
    if (!vm) return;
    if (std::find(ancestors.begin(), ancestors.end(), vm) != ancestors.end()) {
        out_list.push_back({nullptr, prefix, "Reference", indent, "Cyclic ViewModel reference"});
        return;
    }
    if (indent >= 32 || out_list.size() >= 1000) {
        out_list.push_back({nullptr, prefix, "More properties", indent, "Inspector traversal limit reached"});
        return;
    }
    ancestors.push_back(vm);

    const auto& props = vm->propertyValues();
    for (const auto &entry : props) {
        if (out_list.size() >= 1000) {
            out_list.push_back({nullptr, prefix, "More properties", indent, "Inspector traversal limit reached"});
            break;
        }
        rive::ViewModelInstanceValue *value = view_model_value_ptr(entry);
        if (!value) continue;

        String name = String::utf8(value->name().c_str());
        String path = prefix.is_empty() ? name : prefix + "." + name;

        out_list.push_back({value, path, name, indent, ""});

        if (value->is<rive::ViewModelInstanceViewModel>()) {
            if (rive::ViewModelInstanceViewModel *child_vm_prop = value->as<rive::ViewModelInstanceViewModel>()) {
                rive::rcp<rive::ViewModelInstance> child_vm = child_vm_prop->referenceViewModelInstance();
                if (child_vm) {
                    _collect_properties(child_vm.get(), path, indent + 1, out_list, ancestors);
                }
            }
        } else if (value->is<rive::ViewModelInstanceList>()) {
            auto items = value->as<rive::ViewModelInstanceList>()->listItems();
            for (size_t index = 0; index < items.size() && out_list.size() < 1000; ++index) {
                String item_path = path + "[" + String::num_int64(index) + "]";
                out_list.push_back({nullptr, item_path, "[" + String::num_int64(index) + "]", indent + 1, "ViewModel list item"});
                if (items[index] && items[index]->viewModelInstance()) {
                    _collect_properties(items[index]->viewModelInstance().get(), item_path, indent + 2, out_list, ancestors);
                }
            }
        }
    }
    ancestors.pop_back();
}

void RiveViewModelInspector::_update_ui() {
    while (get_child_count() > 0) {
        Node *child = get_child(0);
        remove_child(child);
        child->queue_free();
    }

    RiveControl *control = _get_control();
    if (!control) return;

    Ref<RivePlayer> player = control->get_rive_player();
    if (player.is_null()) return;

    rive::ViewModelInstance *vm = player->get_view_model_instance();
    if (!vm) return;

    std::vector<PropertyEntry> all_props;
    std::vector<rive::ViewModelInstance *> ancestors;
    _collect_properties(vm, "", 0, all_props, ancestors);

    if (structure_only) {
        all_props.erase(std::remove_if(all_props.begin(), all_props.end(), [](const PropertyEntry &entry) {
            auto *value = entry.value;
            return value && (value->is<rive::ViewModelInstanceNumber>() || value->is<rive::ViewModelInstanceString>() ||
                    value->is<rive::ViewModelInstanceBoolean>() || value->is<rive::ViewModelInstanceColor>() ||
                    value->is<rive::ViewModelInstanceEnum>() || value->is<rive::ViewModelInstanceArtboard>() ||
                    value->is<rive::ViewModelInstanceAssetImage>() || value->is<rive::ViewModelInstanceAssetFont>());
        }), all_props.end());
        if (all_props.empty()) return;
    }

    Label *header = memnew(Label);
    header->set_text(structure_only ? "Rive Events / Structure" : "Rive DataBinding");
    header->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    if (EditorInterface::get_singleton()) {
        header->add_theme_font_override("font", EditorInterface::get_singleton()->get_editor_theme()->get_font("bold", "EditorFonts"));
    }
    add_child(header);

    if (all_props.empty()) {
        Label *empty = memnew(Label);
        empty->set_text("No properties found.");
        empty->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        add_child(empty);
        return;
    }

    int total_items = (int)all_props.size();
    int total_pages = (total_items + items_per_page - 1) / items_per_page;
    
    if (current_page >= total_pages) current_page = total_pages - 1;
    if (current_page < 0) current_page = 0;

    int start_idx = current_page * items_per_page;
    int end_idx = std::min(start_idx + items_per_page, total_items);

    Dictionary current_values;
    if (control) {
        for (const PropertyEntry &entry : all_props) {
            if (control->has_property_override(entry.path)) current_values[entry.path] = control->get_property_override(entry.path);
        }
    }

    for (int i = start_idx; i < end_idx; ++i) {
        const PropertyEntry &entry = all_props[i];
        rive::ViewModelInstanceValue *value = entry.value;
        String path = entry.path;

        HBoxContainer *row = memnew(HBoxContainer);
        add_child(row);

        if (entry.indent_level > 0) {
            Control *spacer = memnew(Control);
            spacer->set_custom_minimum_size(Size2(entry.indent_level * 15, 0));
            row->add_child(spacer);
        }

        Label *label = memnew(Label);
        label->set_text(entry.name);
        label->set_tooltip_text(path);
        label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        row->add_child(label);

        if (!value) {
            Label *notice = memnew(Label);
            notice->set_text(entry.notice);
            notice->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            row->add_child(notice);
            continue;
        }

        if (value->is<rive::ViewModelInstanceNumber>()) {
            SpinBox *editor = memnew(SpinBox);
            editor->set_step(0.01);
            editor->set_min(-1000000);
            editor->set_max(1000000);
            editor->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            
            float val = value->as<rive::ViewModelInstanceNumber>()->propertyValue();
            if (current_values.has(path)) val = current_values[path];
            editor->set_value(val);

            editor->connect("value_changed", Callable(this, "_on_number_changed").bind(path));
            row->add_child(editor);
        }
        else if (value->is<rive::ViewModelInstanceString>()) {
            LineEdit *editor = memnew(LineEdit);
            editor->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            
            String val = String::utf8(value->as<rive::ViewModelInstanceString>()->propertyValue().c_str());
            if (current_values.has(path)) val = current_values[path];
            editor->set_text(val);

            editor->connect("text_changed", Callable(this, "_on_string_changed").bind(path));
            row->add_child(editor);
        }
        else if (value->is<rive::ViewModelInstanceBoolean>()) {
            CheckBox *editor = memnew(CheckBox);
            
            bool val = value->as<rive::ViewModelInstanceBoolean>()->propertyValue();
            if (current_values.has(path)) val = current_values[path];
            editor->set_pressed(val);

            editor->connect("toggled", Callable(this, "_on_bool_changed").bind(path));
            row->add_child(editor);
        }
        else if (value->is<rive::ViewModelInstanceColor>()) {
            ColorPickerButton *editor = memnew(ColorPickerButton);
            editor->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            editor->set_custom_minimum_size(Size2(0, 20));
            
            uint32_t argb = value->as<rive::ViewModelInstanceColor>()->propertyValue();
            float a = ((argb >> 24) & 0xFF) / 255.0f;
            float r = ((argb >> 16) & 0xFF) / 255.0f;
            float g = ((argb >> 8) & 0xFF) / 255.0f;
            float b = (argb & 0xFF) / 255.0f;
            Color val(r, g, b, a);
            
            if (current_values.has(path)) val = current_values[path];
            editor->set_pick_color(val);

            editor->connect("color_changed", Callable(this, "_on_color_changed").bind(path));
            row->add_child(editor);
        }
        else if (value->is<rive::ViewModelInstanceEnum>()) {
            rive::ViewModelInstanceEnum *enum_val = value->as<rive::ViewModelInstanceEnum>();
            OptionButton *editor = memnew(OptionButton);
            editor->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            
            bool found_values = false;
            PackedInt32Array indices;
            rive::ViewModelProperty *prop_base = enum_val->viewModelProperty();
            if (prop_base && prop_base->is<rive::ViewModelPropertyEnum>()) {
                rive::ViewModelPropertyEnum *vm_prop = prop_base->as<rive::ViewModelPropertyEnum>();
                if (vm_prop) {
                    rive::DataEnum *data_enum = vm_prop->dataEnum();
                    if (data_enum) {
                        std::vector<rive::DataEnumValue *> values = data_enum->values();
                        for (size_t i = 0; i < values.size(); ++i) {
                            if (values[i]) {
                                editor->add_item(String::utf8(values[i]->key().c_str()), i);
                                indices.append(i);
                                found_values = true;
                            }
                        }
                    }
                }
            }
            
            if (!found_values) {
                editor->add_item("No Options Found");
                editor->set_disabled(true);
            }
            
            int val = enum_val->propertyValue();
            if (current_values.has(path)) {
                Variant saved_value = current_values[path];
                if (saved_value.get_type() == Variant::STRING && prop_base && prop_base->is<rive::ViewModelPropertyEnum>()) {
                    String name = saved_value;
                    val = prop_base->as<rive::ViewModelPropertyEnum>()->valueIndex(name.utf8().get_data());
                } else if (saved_value.get_type() == Variant::INT) {
                    val = saved_value;
                }
            }

            editor->select(editor->get_item_index(val));

            editor->connect("item_selected", Callable(this, "_on_enum_changed").bind(path, indices));
            row->add_child(editor);
        }
        else if (value->is<rive::ViewModelInstanceArtboard>()) {
            OptionButton *editor = memnew(OptionButton);
            editor->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            PackedStringArray names;
            names.append("");
            editor->add_item("<None>");
            PackedStringArray artboards = control->get_artboard_list();
            for (int index = 0; index < artboards.size(); ++index) {
                names.append(artboards[index]);
                editor->add_item(artboards[index]);
            }

            String selected;
            Ref<RiveViewModelInstance> vm_wrapper = player->get_rive_view_model_instance();
            Ref<RiveViewModelArtboard> artboard_property = vm_wrapper.is_valid() ? vm_wrapper->get_artboard_property(path) : Ref<RiveViewModelArtboard>();
            if (artboard_property.is_valid()) {
                selected = artboard_property->get_value();
            }
            if (current_values.has(path) && Variant(current_values[path]).get_type() == Variant::STRING) {
                selected = current_values[path];
            }
            int selected_index = names.find(selected);
            if (selected_index < 0) {
                names.append(selected);
                editor->add_item(selected + " (unavailable)");
                selected_index = names.size() - 1;
                editor->set_item_disabled(selected_index, true);
            }
            editor->select(selected_index);
            editor->connect("item_selected", Callable(this, "_on_artboard_changed").bind(path, names));
            row->add_child(editor);
        }
        else if (value->is<rive::ViewModelInstanceAssetImage>() || value->is<rive::ViewModelInstanceAssetFont>()) {
            const bool is_image = value->is<rive::ViewModelInstanceAssetImage>();
            EditorResourcePicker *editor = memnew(EditorResourcePicker);
            editor->set_base_type(is_image ? "Texture2D" : "FontFile");
            editor->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            editor->set_tooltip_text(is_image ? "Choose a Texture2D to override the Rive image. Reset restores its embedded value." : "Choose a FontFile to override the Rive font. Reset restores its embedded value.");
            if (current_values.has(path)) {
                Variant saved_value = current_values[path];
                if (saved_value.get_type() == Variant::OBJECT) {
                    Ref<Resource> resource = saved_value;
                    editor->set_edited_resource(resource);
                }
            }
            editor->connect("resource_changed", Callable(this, "_on_resource_changed").bind(path));
            editor->connect("resource_selected", Callable(this, "_on_resource_selected"));
            row->add_child(editor);
        }
        else if (value->is<rive::ViewModelInstanceList>()) {
            const size_t count = value->as<rive::ViewModelInstanceList>()->listItems().size();
            Label *summary = memnew(Label);
            summary->set_text(String::num_int64(count) + " items");
            summary->set_tooltip_text(count == 0 ? "Empty ViewModel list. Use its script API to append a compatible existing ViewModel instance." : "List item properties are displayed below as list[index].property.");
            summary->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            row->add_child(summary);
            Button *clear = memnew(Button);
            clear->set_text("Clear");
            clear->set_disabled(count == 0);
            clear->set_tooltip_text("Remove all ViewModel list items (undoable).");
            clear->connect("pressed", Callable(this, "_on_clear_list").bind(path));
            row->add_child(clear);
        }
        else if (value->is<rive::ViewModelInstanceAssetBlob>() || value->is<rive::ViewModelInstanceSymbolListIndex>()) {
            Label *summary = memnew(Label);
            const auto vm_wrapper = player->get_rive_view_model_instance();
            if (value->is<rive::ViewModelInstanceAssetBlob>()) {
                const PackedByteArray bytes = vm_wrapper->get_property_value(path);
                summary->set_text(String::num_int64(bytes.size()) + " bytes");
                summary->set_tooltip_text("Binary asset. Edit its PackedByteArray in the RiveControl properties or use get_blob_property().");
            } else {
                summary->set_text(String::num_int64(int64_t(vm_wrapper->get_property_value(path))) + " (read-only)");
                summary->set_tooltip_text("The runtime owns this list index; list operations update it automatically.");
            }
            summary->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            row->add_child(summary);
        }
        else if (value->is<rive::ViewModelInstanceTrigger>()) {
            Button *editor = memnew(Button);
            editor->set_text("Fire");
            editor->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            
            editor->connect("pressed", Callable(this, "_on_trigger_fired").bind(path));
            row->add_child(editor);
        }
        else if (value->is<rive::ViewModelInstanceViewModel>()) {
            label->add_theme_color_override("font_color", Color(0.7, 0.7, 0.7));
        }
        else {
            Label *unsupported = memnew(Label);
            unsupported->set_text("Unsupported type " + String::num_int64(value->coreType()) + " (read-only)");
            unsupported->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            row->add_child(unsupported);
        }

        if (current_values.has(path) && !value->is<rive::ViewModelInstanceTrigger>()) {
            Button *reset = memnew(Button);
            reset->set_text("Reset");
            reset->set_tooltip_text("Remove this saved override and restore the value from the Rive file.");
            reset->connect("pressed", Callable(this, "_on_reset_property").bind(path));
            row->add_child(reset);
        }
    }

    if (total_pages > 1) {
        HBoxContainer *nav = memnew(HBoxContainer);
        nav->set_alignment(BoxContainer::ALIGNMENT_CENTER);
        add_child(nav);

        Button *prev = memnew(Button);
        prev->set_text("<");
        prev->set_disabled(current_page == 0);
        prev->connect("pressed", Callable(this, "_on_prev_page"));
        nav->add_child(prev);

        Label *page_lbl = memnew(Label);
        page_lbl->set_text(String::num(current_page + 1) + " / " + String::num(total_pages));
        nav->add_child(page_lbl);

        Button *next = memnew(Button);
        next->set_text(">");
        next->set_disabled(current_page >= total_pages - 1);
        next->connect("pressed", Callable(this, "_on_next_page"));
        nav->add_child(next);
    }
}

void RiveViewModelInspector::_on_prev_page() {
    if (current_page > 0) {
        current_page--;
        _update_ui();
    }
}

void RiveViewModelInspector::_on_next_page() {
    current_page++;
    _update_ui();
}

void RiveViewModelInspector::_on_number_changed(double value, String path) {
    _commit_value(path, value);
}

void RiveViewModelInspector::_on_string_changed(String value, String path) {
    _commit_value(path, value);
}

void RiveViewModelInspector::_on_bool_changed(bool value, String path) {
    _commit_value(path, value);
}

void RiveViewModelInspector::_on_color_changed(Color value, String path) {
    _commit_value(path, value);
}

void RiveViewModelInspector::_on_enum_changed(int value, String path, PackedInt32Array indices) {
    if (value >= 0 && value < indices.size()) {
        _commit_value(path, indices[value]);
    }
}

void RiveViewModelInspector::_on_artboard_changed(int value, String path, PackedStringArray names) {
    if (value >= 0 && value < names.size()) {
        _commit_value(path, names[value]);
    }
}

void RiveViewModelInspector::_on_resource_changed(Ref<Resource> resource, String path) {
    RiveControl *control = _get_control();
    Ref<RiveViewModelInstance> vm = control ? control->get_view_model_instance() : Ref<RiveViewModelInstance>();
    if (vm.is_null()) return;

    const bool is_font = vm->get_font_property(path).is_valid();
    const bool is_image = vm->get_image_property(path).is_valid();
    bool valid = is_font || is_image;
    if (resource.is_valid()) {
        if (is_font) {
            Ref<FontFile> font = resource;
            valid = font.is_valid();
            if (valid) {
                const PackedByteArray bytes = font->get_data();
                valid = !bytes.is_empty();
                if (valid) {
                    auto decoded = HBFont::Decode(rive::Span<const uint8_t>(bytes.ptr(), bytes.size()));
                    valid = static_cast<bool>(decoded);
                }
            }
        } else if (is_image) {
            Ref<Texture2D> texture = resource;
            Ref<Image> image = texture.is_valid() ? texture->get_image() : Ref<Image>();
            valid = image.is_valid() && !image->is_empty();
        }
    }
    if (!valid) {
        UtilityFunctions::printerr("Rive Inspector rejected an invalid resource for '", path, "'.");
        _queue_refresh();
        return;
    }
    _commit_value(path, resource);
}

void RiveViewModelInspector::_on_resource_selected(Ref<Resource> resource, bool inspect) {
    if (resource.is_valid() && EditorInterface::get_singleton()) {
        EditorInterface::get_singleton()->edit_resource(resource);
    }
}

void RiveViewModelInspector::_on_clear_list(String path) {
    path = RiveViewModelInstance::canonicalize_property_path(path);
    if (RiveControl *control = _get_control()) {
        Dictionary values = control->get_property_values().duplicate(true);
        Array keys = values.keys();
        for (int index = 0; index < keys.size(); ++index) {
            String key = keys[index];
            if (key.begins_with(path + "[") || key.begins_with(path + ".")) {
                values.erase(key);
            }
        }
        values[path] = Array();
        _commit_values(values, path);
    }
    _queue_refresh();
}

void RiveViewModelInspector::_queue_refresh() {
    if (!applying_edit && !refresh_queued) {
        refresh_queued = true;
        call_deferred("_refresh");
    }
}

void RiveViewModelInspector::_refresh() {
    refresh_queued = false;
    _update_ui();
}

void RiveViewModelInspector::_commit_value(const String &path, const Variant &value) {
    if (RiveControl *control = _get_control()) {
        const String key = RiveViewModelInstance::canonicalize_property_path(path);
        Dictionary values = control->get_property_values().duplicate(true);
        if (values.has(key) && Variant(values[key]) == value) {
            return;
        }
        values[key] = value;
        _commit_values(values, path);
    }
}

void RiveViewModelInspector::_commit_values(const Dictionary &values, const String &path) {
    RiveControl *control = _get_control();
    if (!control) return;

    const Dictionary previous = control->get_property_values().duplicate(true);
    const Dictionary updated = values.duplicate(true);
    applying_edit = true;
    EditorInterface *editor = EditorInterface::get_singleton();
    EditorUndoRedoManager *undo_redo = editor ? editor->get_editor_undo_redo() : nullptr;
    if (undo_redo) {
        const String action_name = "Set Rive " + path + " (" + String::num_uint64(control->get_instance_id()) + ")";
        undo_redo->create_action(action_name, UndoRedo::MERGE_ENDS, control);
        undo_redo->add_do_method(control, "set_property_values", updated);
        undo_redo->add_undo_method(control, "set_property_values", previous);
        undo_redo->commit_action();
    } else {
        control->set_property_values(updated);
    }
    applying_edit = false;
}

void RiveViewModelInspector::_on_reset_property(String path) {
    if (RiveControl *control = _get_control()) {
        Dictionary values = control->get_property_values().duplicate(true);
        values.erase(RiveViewModelInstance::canonicalize_property_path(path));
        _commit_values(values, path);
        _queue_refresh();
    }
}

void RiveViewModelInspector::_on_trigger_fired(String path) {
    RiveControl *control = _get_control();
    if (control) {
        control->fire_trigger(path);
    }
}
