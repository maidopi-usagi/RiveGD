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
#include <rive/viewmodel/viewmodel_property.hpp>

using namespace godot;

void RiveViewModelInspector::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_on_prev_page"), &RiveViewModelInspector::_on_prev_page);
    ClassDB::bind_method(D_METHOD("_on_next_page"), &RiveViewModelInspector::_on_next_page);
    
    ClassDB::bind_method(D_METHOD("_on_number_changed", "value", "path"), &RiveViewModelInspector::_on_number_changed);
    ClassDB::bind_method(D_METHOD("_on_string_changed", "value", "path"), &RiveViewModelInspector::_on_string_changed);
    ClassDB::bind_method(D_METHOD("_on_bool_changed", "value", "path"), &RiveViewModelInspector::_on_bool_changed);
    ClassDB::bind_method(D_METHOD("_on_color_changed", "value", "path"), &RiveViewModelInspector::_on_color_changed);
    ClassDB::bind_method(D_METHOD("_on_enum_changed", "value", "path"), &RiveViewModelInspector::_on_enum_changed);
    ClassDB::bind_method(D_METHOD("_on_trigger_fired", "path"), &RiveViewModelInspector::_on_trigger_fired);
}

void RiveViewModelInspector::set_rive_control(RiveControl *p_control) {
    if (p_control) {
        control_id = p_control->get_instance_id();
    } else {
        control_id = ObjectID();
    }
    _update_ui();
}

RiveControl *RiveViewModelInspector::_get_control() const {
    if (control_id.is_null()) return nullptr;
    Object *obj = ObjectDB::get_instance(control_id);
    return Object::cast_to<RiveControl>(obj);
}

void RiveViewModelInspector::_collect_properties(rive::ViewModelInstance *vm, String prefix, int indent, std::vector<PropertyEntry> &out_list) {
    if (!vm) return;

    std::vector<rive::ViewModelInstanceValue *> props = vm->propertyValues();
    for (auto value : props) {
        if (!value) continue;

        String name = value->name().c_str();
        String path = prefix.is_empty() ? name : prefix + "." + name;

        out_list.push_back({value, path, name, indent});

        if (value->is<rive::ViewModelInstanceViewModel>()) {
            if (rive::ViewModelInstanceViewModel *child_vm_prop = value->as<rive::ViewModelInstanceViewModel>()) {
                rive::rcp<rive::ViewModelInstance> child_vm = child_vm_prop->referenceViewModelInstance();
                if (child_vm) {
                    _collect_properties(child_vm.get(), path, indent + 1, out_list);
                }
            }
        }
    }
}

void RiveViewModelInspector::_update_ui() {
    // Clear existing children
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

    // Header
    Label *header = memnew(Label);
    header->set_text("Rive DataBinding");
    header->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    header->add_theme_font_override("font", EditorInterface::get_singleton()->get_editor_theme()->get_font("bold", "EditorFonts"));
    add_child(header);

    // Collect all properties
    std::vector<PropertyEntry> all_props;
    _collect_properties(vm, "", 0, all_props);

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
        current_values = control->get_property_values();
    }

    for (int i = start_idx; i < end_idx; ++i) {
        const PropertyEntry &entry = all_props[i];
        rive::ViewModelInstanceValue *value = entry.value;
        String path = entry.path;

        HBoxContainer *row = memnew(HBoxContainer);
        add_child(row);

        // Indentation
        if (entry.indent_level > 0) {
            Control *spacer = memnew(Control);
            spacer->set_custom_minimum_size(Size2(entry.indent_level * 15, 0));
            row->add_child(spacer);
        }

        Label *label = memnew(Label);
        label->set_text(entry.name);
        label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        row->add_child(label);

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
            
            String val = value->as<rive::ViewModelInstanceString>()->propertyValue().c_str();
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
        else if (rive::ViewModelInstanceEnum *enum_val = value->as<rive::ViewModelInstanceEnum>()) {
            OptionButton *editor = memnew(OptionButton);
            editor->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            
            bool found_values = false;
            rive::ViewModelProperty *prop_base = enum_val->viewModelProperty();
            if (prop_base && prop_base->is<rive::ViewModelPropertyEnum>()) {
                rive::ViewModelPropertyEnum *vm_prop = prop_base->as<rive::ViewModelPropertyEnum>();
                if (vm_prop) {
                    rive::DataEnum *data_enum = vm_prop->dataEnum();
                    if (data_enum) {
                        std::vector<rive::DataEnumValue *> values = data_enum->values();
                        for (size_t i = 0; i < values.size(); ++i) {
                            if (values[i]) {
                                editor->add_item(values[i]->key().c_str(), i);
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
            if (current_values.has(path)) val = current_values[path];
            
            if (val >= 0 && val < editor->get_item_count()) {
                editor->select(val);
            } else if (editor->get_item_count() > 0) {
                editor->select(0);
            }

            editor->connect("item_selected", Callable(this, "_on_enum_changed").bind(path));
            row->add_child(editor);
        }
        else if (value->is<rive::ViewModelInstanceTrigger>()) {
            Button *editor = memnew(Button);
            editor->set_text("Fire");
            editor->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            
            editor->connect("pressed", Callable(this, "_on_trigger_fired").bind(path));
            row->add_child(editor);
        }
        else if (value->is<rive::ViewModelInstanceViewModel>()) {
            // Group header is just the label we added above
            // Maybe make it bold or different color
            label->add_theme_color_override("font_color", Color(0.7, 0.7, 0.7));
        }
    }

    // Pagination Controls
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
    RiveControl *control = _get_control();
    if (control) {
        Dictionary values = control->get_property_values();
        values[path] = value;
        control->set_property_values(values);
    }
}

void RiveViewModelInspector::_on_string_changed(String value, String path) {
    RiveControl *control = _get_control();
    if (control) {
        Dictionary values = control->get_property_values();
        values[path] = value;
        control->set_property_values(values);
    }
}

void RiveViewModelInspector::_on_bool_changed(bool value, String path) {
    RiveControl *control = _get_control();
    if (control) {
        Dictionary values = control->get_property_values();
        values[path] = value;
        control->set_property_values(values);
    }
}

void RiveViewModelInspector::_on_color_changed(Color value, String path) {
    RiveControl *control = _get_control();
    if (control) {
        Dictionary values = control->get_property_values();
        values[path] = value;
        control->set_property_values(values);
    }
}

void RiveViewModelInspector::_on_enum_changed(int value, String path) {
    RiveControl *control = _get_control();
    if (control) {
        Dictionary values = control->get_property_values();
        values[path] = value;
        control->set_property_values(values);
    }
}

void RiveViewModelInspector::_on_trigger_fired(String path) {
    RiveControl *control = _get_control();
    if (control) {
        control->fire_trigger(path);
    }
}
