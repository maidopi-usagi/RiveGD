#include "rive_view_model_inspector.h"
#include "../scene/rive_player.h"
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/color_picker_button.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/menu_button.hpp>
#include <godot_cpp/classes/popup_menu.hpp>
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
    ClassDB::bind_method(D_METHOD("_on_add_list_item", "choice", "path", "choices"), &RiveViewModelInspector::_on_add_list_item);
    ClassDB::bind_method(D_METHOD("_on_list_item_button", "item", "column", "button", "mouse_button", "tree"), &RiveViewModelInspector::_on_list_item_button);
    ClassDB::bind_method(D_METHOD("_get_list_drag_data", "position", "tree"), &RiveViewModelInspector::_get_list_drag_data);
    ClassDB::bind_method(D_METHOD("_can_drop_list_data", "position", "data", "tree"), &RiveViewModelInspector::_can_drop_list_data);
    ClassDB::bind_method(D_METHOD("_drop_list_data", "position", "data", "tree"), &RiveViewModelInspector::_drop_list_data);
    ClassDB::bind_method(D_METHOD("_on_trigger_fired", "path"), &RiveViewModelInspector::_on_trigger_fired);
}

void RiveViewModelInspector::set_rive_control(RiveControl *p_control) {
    edit_error = String();
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
    ++ui_revision;
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
    if (!edit_error.is_empty()) {
        Label *error = memnew(Label);
        error->set_text(edit_error);
        error->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
        error->add_theme_color_override("font_color", Color(1, 0.5, 0.4));
        add_child(error);
    }

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
            summary->set_tooltip_text("Drag list rows to reorder. Changes are saved in the scene as item presets.");
            summary->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            row->add_child(summary);
            MenuButton *add = memnew(MenuButton);
            add->set_text("Add...");
            Array choices;
            const Ref<RiveFile> file = control->get_rive_file();
            const PackedStringArray models = file->get_view_model_names();
            for (int model_index = 0; model_index < models.size(); ++model_index) {
                const PackedStringArray instances = file->get_view_model_instance_names(models[model_index]);
                for (int index = 0; index <= instances.size(); ++index) {
                    Dictionary choice;
                    choice["model"] = models[model_index];
                    choice["index"] = index < instances.size() ? index : -1;
                    choice["blank"] = index == instances.size();
                    add->get_popup()->add_item(models[model_index] + String(" / ") + (index < instances.size() ? instances[index] : String("<Blank>")), choices.size());
                    choices.push_back(choice);
                }
            }
            add->set_disabled(choices.is_empty() || count >= 1000);
            add->get_popup()->connect("id_pressed", Callable(this, "_on_add_list_item").bind(path, choices));
            row->add_child(add);
            Button *clear = memnew(Button);
            clear->set_text("Clear");
            clear->set_disabled(count == 0);
            clear->set_tooltip_text("Remove all ViewModel list items (undoable).");
            clear->connect("pressed", Callable(this, "_on_clear_list").bind(path));
            row->add_child(clear);

            Tree *tree = memnew(Tree);
            tree->set_hide_root(true);
            tree->set_h_scroll_enabled(false);
            tree->set_column_clip_content(0, true);
            tree->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            const double editor_scale = EditorInterface::get_singleton() ? EditorInterface::get_singleton()->get_editor_scale() : 1.0;
            tree->set_custom_minimum_size(Size2(0, std::clamp<double>(count * 32 + 24, 48, 224) * editor_scale));
            tree->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN);
            tree->set_meta("rive_list_path", RiveViewModelInstance::canonicalize_property_path(path));
            tree->set_meta("rive_ui_revision", ui_revision);
            tree->set_tooltip_text("Drag rows to reorder. Use the row buttons to duplicate, remove, or move an item.");
            tree->set_drag_forwarding(Callable(this, "_get_list_drag_data").bind(tree),
                    Callable(this, "_can_drop_list_data").bind(tree), Callable(this, "_drop_list_data").bind(tree));
            tree->connect("button_clicked", Callable(this, "_on_list_item_button").bind(tree));
            TreeItem *root = tree->create_item();
            const auto list = control->get_view_model_instance()->get_list_property(path);
            const Ref<Theme> theme = EditorInterface::get_singleton() ? EditorInterface::get_singleton()->get_editor_theme() : Ref<Theme>();
            static const char *icons[] = {"Duplicate", "Remove", "MoveUp", "MoveDown"};
            static const char *tooltips[] = {"Duplicate item", "Remove item", "Move up", "Move down"};
            for (size_t index = 0; index < count; ++index) {
                TreeItem *item = tree->create_item(root);
                const auto instance = list->get_item(index);
                item->set_text(0, "[" + String::num_int64(index) + "] " + (instance.is_valid() ? instance->get_view_model_name() + " / " + instance->get_name() : String("<Missing>")));
                item->set_metadata(0, index);
                for (int button = 0; button < 4; ++button) {
                    if (theme.is_valid() && theme->has_icon(icons[button], "EditorIcons")) {
                        item->add_button(0, theme->get_icon(icons[button], "EditorIcons"), button,
                                (button == 2 && index == 0) || (button == 3 && index + 1 == count), tooltips[button]);
                    }
                }
            }
            add_child(tree);
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
    _commit_list(path, Array());
}

bool RiveViewModelInspector::_snapshot_list(const String &p_path, Array &r_items) {
    RiveControl *control = _get_control();
    const auto vm = control ? control->get_view_model_instance() : Ref<RiveViewModelInstance>();
    const auto list = vm.is_valid() ? vm->get_list_property(p_path) : Ref<RiveViewModelList>();
    if (list.is_null() || list->get_size() > 1000) return false;
    for (int index = 0; index < list->get_size(); ++index) {
        auto preset = RiveViewModelPreset::_capture_instance(list->get_item(index), vm, p_path + String(".") + String::num_int64(index));
        if (preset.is_null() || preset->instantiate(control->get_rive_file()).is_null()) {
            edit_error = "This list contains a cyclic, oversized, or runtime-only value that cannot be saved as an item preset.";
            _queue_refresh();
            return false;
        }
        r_items.push_back(preset);
    }
    return true;
}

void RiveViewModelInspector::_commit_list(const String &p_path, const Array &p_items) {
    const String path = RiveViewModelInstance::canonicalize_property_path(p_path);
    if (RiveControl *control = _get_control()) {
        Dictionary values = control->get_property_values().duplicate(true);
        Array keys = values.keys();
        for (int index = 0; index < keys.size(); ++index) {
            String key = keys[index];
            if (key.begins_with(path + String("[")) || key.begins_with(path + String("."))) {
                values.erase(key);
            }
        }
        values[path] = p_items;
        edit_error = String();
        _commit_values(values, path, false);
    }
    _queue_refresh();
}

void RiveViewModelInspector::_on_add_list_item(int p_choice, String p_path, Array p_choices) {
    RiveControl *control = _get_control();
    if (!control || p_choice < 0 || p_choice >= p_choices.size()) return;
    const Dictionary choice = p_choices[p_choice];
    Ref<RiveViewModelPreset> item;
    item.instantiate();
    item->set_view_model_name(choice["model"]);
    item->set_instance_index(choice["index"]);
    item->set_blank(choice["blank"]);
    if (item->instantiate(control->get_rive_file()).is_null()) {
        edit_error = "The selected ViewModel cannot be instantiated safely.";
        _queue_refresh();
        return;
    }
    Array items;
    if (!_snapshot_list(p_path, items) || items.size() >= 1000) return;
    items.push_back(item);
    _commit_list(p_path, items);
}

void RiveViewModelInspector::_on_list_item_button(TreeItem *p_item, int p_column, int p_button, int p_mouse_button, Tree *p_tree) {
    if (!p_item || !p_tree || p_tree->get_parent() != this || uint64_t(p_tree->get_meta("rive_ui_revision")) != ui_revision) return;
    const String path = p_tree->get_meta("rive_list_path");
    const int index = p_item->get_metadata(0);
    Array items;
    if (!_snapshot_list(path, items) || index < 0 || index >= items.size()) return;
    if (p_button == 0 && items.size() < 1000) {
        const auto vm = _get_control()->get_view_model_instance();
        const auto list = vm->get_list_property(path);
        const auto duplicate = RiveViewModelPreset::_capture_instance(list->get_item(index), vm, path + String(".") + String::num_int64(index));
        if (duplicate.is_null()) return;
        items.insert(index + 1, duplicate);
    }
    else if (p_button == 1) items.remove_at(index);
    else if ((p_button == 2 && index > 0) || (p_button == 3 && index + 1 < items.size())) {
        const int destination = index + (p_button == 2 ? -1 : 1);
        const Variant item = items[index];
        items.remove_at(index);
        items.insert(destination, item);
    } else return;
    _commit_list(path, items);
}

Variant RiveViewModelInspector::_get_list_drag_data(Vector2 p_position, Tree *p_tree) {
    RiveControl *control = _get_control();
    if (!control || !p_tree || p_tree->get_parent() != this) return Variant();
    TreeItem *item = p_tree->get_item_at_position(p_position);
    if (!item) return Variant();
    const String path = p_tree->get_meta("rive_list_path");
    const auto list = control->get_view_model_instance()->get_list_property(path);
    const int index = item->get_metadata(0);
    if (list.is_null() || uint64_t(p_tree->get_meta("rive_ui_revision")) != ui_revision) return Variant();
    Dictionary data;
    data["type"] = "rive_view_model_list_item";
    data["inspector"] = get_instance_id();
    data["tree"] = p_tree->get_instance_id();
    data["revision"] = ui_revision;
    data["path"] = path;
    data["index"] = index;
    data["item"] = list->get_item(index);
    Label *preview = memnew(Label);
    preview->set_text(item->get_text(0));
    p_tree->set_drag_preview(preview);
    return data;
}

int RiveViewModelInspector::_get_list_drop_index(Vector2 p_position, const Variant &p_data, Tree *p_tree) const {
    if (!p_tree || p_tree->get_parent() != this || p_data.get_type() != Variant::DICTIONARY) return -1;
    const Dictionary data = p_data;
    for (const char *field : {"inspector", "tree", "revision", "index"}) {
        if (data.get(field, Variant()).get_type() != Variant::INT) return -1;
    }
    if (String(data.get("type", String())) != "rive_view_model_list_item" ||
            uint64_t(data.get("inspector", 0)) != get_instance_id() ||
            uint64_t(data.get("tree", 0)) != p_tree->get_instance_id() ||
            uint64_t(data.get("revision", 0)) != ui_revision ||
            data.get("path", String()) != p_tree->get_meta("rive_list_path")) return -1;
    RiveControl *control = _get_control();
    const auto vm = control ? control->get_view_model_instance() : Ref<RiveViewModelInstance>();
    const auto list = vm.is_valid() ? vm->get_list_property(data["path"]) : Ref<RiveViewModelList>();
    const int64_t from = data.get("index", -1);
    if (list.is_null() || from < 0 || from >= list->get_size()) return -1;
    const Ref<RiveViewModelInstance> source = data.get("item", Variant());
    const auto current = list.is_valid() ? list->get_item(from) : Ref<RiveViewModelInstance>();
    if (source.is_null() || current.is_null() || source->_get_native_instance() != current->_get_native_instance()) return -1;
    TreeItem *target = p_tree->get_item_at_position(p_position);
    const int section = p_tree->get_drop_section_at_position(p_position);
    if (!target || (section != -1 && section != 1)) return -1;
    int destination = int(target->get_metadata(0)) + (section == 1 ? 1 : 0);
    if (from < destination) --destination;
    return destination == from || destination < 0 || destination >= list->get_size() ? -1 : destination;
}

bool RiveViewModelInspector::_can_drop_list_data(Vector2 p_position, Variant p_data, Tree *p_tree) {
    return _get_list_drop_index(p_position, p_data, p_tree) >= 0;
}

void RiveViewModelInspector::_drop_list_data(Vector2 p_position, Variant p_data, Tree *p_tree) {
    const int destination = _get_list_drop_index(p_position, p_data, p_tree);
    if (destination < 0) return;
    const Dictionary data = p_data;
    Array items;
    if (!_snapshot_list(data["path"], items)) return;
    const int from = data["index"];
    const Variant item = items[from];
    items.remove_at(from);
    items.insert(destination, item);
    _commit_list(data["path"], items);
}

void RiveViewModelInspector::_queue_refresh() {
    if (!applying_edit && !refresh_queued) {
        ++ui_revision;
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

void RiveViewModelInspector::_commit_values(const Dictionary &values, const String &path, bool p_merge) {
    RiveControl *control = _get_control();
    if (!control) return;

    const Dictionary previous = control->get_property_values().duplicate(true);
    const Dictionary updated = values.duplicate(true);
    applying_edit = true;
    EditorInterface *editor = EditorInterface::get_singleton();
    EditorUndoRedoManager *undo_redo = editor ? editor->get_editor_undo_redo() : nullptr;
    if (undo_redo) {
        const String action_name = "Set Rive " + path + " (" + String::num_uint64(control->get_instance_id()) + ")";
        undo_redo->create_action(action_name, p_merge ? UndoRedo::MERGE_ENDS : UndoRedo::MERGE_DISABLE, control);
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
