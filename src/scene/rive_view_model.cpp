#include "rive_view_model.h"
#include "../renderer/rive_render_registry.h"
#include "../renderer/rive_texture_factory.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/image.hpp>

#include <rive/viewmodel/viewmodel_instance.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include <rive/viewmodel/viewmodel_instance_string.hpp>
#include <rive/viewmodel/viewmodel_instance_boolean.hpp>
#include <rive/viewmodel/viewmodel_instance_color.hpp>
#include <rive/viewmodel/viewmodel_instance_enum.hpp>
#include <rive/viewmodel/viewmodel_instance_trigger.hpp>
#include <rive/viewmodel/viewmodel_instance_viewmodel.hpp>
#include <rive/viewmodel/viewmodel_instance_asset_image.hpp>
#include <rive/viewmodel/viewmodel_property_enum.hpp>
#include <rive/viewmodel/data_enum.hpp>
#include <rive/viewmodel/data_enum_value.hpp>
#include <rive/factory.hpp>

using namespace godot;

// --- RiveViewModelProperty ---

void RiveViewModelProperty::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_property_name"), &RiveViewModelProperty::get_property_name);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "property_name"), "", "get_property_name");
}

// --- RiveViewModelNumber ---

void RiveViewModelNumber::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_value", "value"), &RiveViewModelNumber::set_value);
    ClassDB::bind_method(D_METHOD("get_value"), &RiveViewModelNumber::get_value);
    
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "value"), "set_value", "get_value");
    ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::FLOAT, "new_value")));
}

void RiveViewModelNumber::_init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceNumber* p_number) {
    instance_ref = p_owner;
    instance_number = p_number;
}

void RiveViewModelNumber::set_value(float p_value) {
    if (instance_number) {
        if (instance_number->propertyValue() != p_value) {
            instance_number->propertyValue(p_value);
            emit_signal("value_changed", p_value);
        }
    }
}

float RiveViewModelNumber::get_value() const {
    if (instance_number) {
        return instance_number->propertyValue();
    }
    return 0.0f;
}

String RiveViewModelNumber::get_property_name() const {
    if (instance_number) {
        return String(instance_number->name().c_str());
    }
    return "";
}

// --- RiveViewModelString ---

void RiveViewModelString::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_value", "value"), &RiveViewModelString::set_value);
    ClassDB::bind_method(D_METHOD("get_value"), &RiveViewModelString::get_value);
    
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "value"), "set_value", "get_value");
    ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::STRING, "new_value")));
}

void RiveViewModelString::_init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceString* p_string) {
    instance_ref = p_owner;
    instance_string = p_string;
}

void RiveViewModelString::set_value(const String& p_value) {
    if (instance_string) {
        std::string val = p_value.utf8().get_data();
        if (instance_string->propertyValue() != val) {
            instance_string->propertyValue(val);
            emit_signal("value_changed", p_value);
        }
    }
}

String RiveViewModelString::get_value() const {
    if (instance_string) {
        return String(instance_string->propertyValue().c_str());
    }
    return "";
}

String RiveViewModelString::get_property_name() const {
    if (instance_string) {
        return String(instance_string->name().c_str());
    }
    return "";
}

// --- RiveViewModelBoolean ---

void RiveViewModelBoolean::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_value", "value"), &RiveViewModelBoolean::set_value);
    ClassDB::bind_method(D_METHOD("get_value"), &RiveViewModelBoolean::get_value);
    
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "value"), "set_value", "get_value");
    ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::BOOL, "new_value")));
}

void RiveViewModelBoolean::_init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceBoolean* p_boolean) {
    instance_ref = p_owner;
    instance_boolean = p_boolean;
}

void RiveViewModelBoolean::set_value(bool p_value) {
    if (instance_boolean) {
        if (instance_boolean->propertyValue() != p_value) {
            instance_boolean->propertyValue(p_value);
            emit_signal("value_changed", p_value);
        }
    }
}

bool RiveViewModelBoolean::get_value() const {
    if (instance_boolean) {
        return instance_boolean->propertyValue();
    }
    return false;
}

String RiveViewModelBoolean::get_property_name() const {
    if (instance_boolean) {
        return String(instance_boolean->name().c_str());
    }
    return "";
}

// --- RiveViewModelColor ---

void RiveViewModelColor::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_value", "value"), &RiveViewModelColor::set_value);
    ClassDB::bind_method(D_METHOD("get_value"), &RiveViewModelColor::get_value);
    
    ADD_PROPERTY(PropertyInfo(Variant::COLOR, "value"), "set_value", "get_value");
    ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::COLOR, "new_value")));
}

void RiveViewModelColor::_init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceColor* p_color) {
    instance_ref = p_owner;
    instance_color = p_color;
}

void RiveViewModelColor::set_value(Color p_value) {
    if (instance_color) {
        uint32_t a = (uint32_t)(p_value.a * 255.0f);
        uint32_t r = (uint32_t)(p_value.r * 255.0f);
        uint32_t g = (uint32_t)(p_value.g * 255.0f);
        uint32_t b = (uint32_t)(p_value.b * 255.0f);
        uint32_t argb = (a << 24) | (r << 16) | (g << 8) | b;
        
        if (instance_color->propertyValue() != argb) {
            instance_color->propertyValue(argb);
            emit_signal("value_changed", p_value);
        }
    }
}

Color RiveViewModelColor::get_value() const {
    if (instance_color) {
        uint32_t argb = instance_color->propertyValue();
        float a = ((argb >> 24) & 0xFF) / 255.0f;
        float r = ((argb >> 16) & 0xFF) / 255.0f;
        float g = ((argb >> 8) & 0xFF) / 255.0f;
        float b = (argb & 0xFF) / 255.0f;
        return Color(r, g, b, a);
    }
    return Color();
}

String RiveViewModelColor::get_property_name() const {
    if (instance_color) {
        return String(instance_color->name().c_str());
    }
    return "";
}

// --- RiveViewModelEnum ---

void RiveViewModelEnum::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_value", "value"), &RiveViewModelEnum::set_value);
    ClassDB::bind_method(D_METHOD("get_value"), &RiveViewModelEnum::get_value);
    ClassDB::bind_method(D_METHOD("set_value_by_name", "name"), &RiveViewModelEnum::set_value_by_name);
    ClassDB::bind_method(D_METHOD("get_value_name"), &RiveViewModelEnum::get_value_name);
    ClassDB::bind_method(D_METHOD("get_values"), &RiveViewModelEnum::get_values);
    
    ADD_PROPERTY(PropertyInfo(Variant::INT, "value"), "set_value", "get_value");
    ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::INT, "new_value")));
}

void RiveViewModelEnum::_init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceEnum* p_enum) {
    instance_ref = p_owner;
    instance_enum = p_enum;
}

void RiveViewModelEnum::set_value(int p_value) {
    if (instance_enum) {
        if (instance_enum->propertyValue() != (uint32_t)p_value) {
            instance_enum->value((uint32_t)p_value);
            emit_signal("value_changed", p_value);
        }
    }
}

int RiveViewModelEnum::get_value() const {
    if (instance_enum) {
        return (int)instance_enum->propertyValue();
    }
    return 0;
}

void RiveViewModelEnum::set_value_by_name(const String& p_name) {
    if (!instance_enum) return;
    
    rive::ViewModelProperty* prop_base = instance_enum->viewModelProperty();
    if (prop_base && prop_base->is<rive::ViewModelPropertyEnum>()) {
        rive::ViewModelPropertyEnum* vm_prop = prop_base->as<rive::ViewModelPropertyEnum>();
        if (vm_prop) {
            rive::DataEnum* data_enum = vm_prop->dataEnum();
            if (data_enum) {
                std::string name_str = p_name.utf8().get_data();
                std::vector<rive::DataEnumValue*> values = data_enum->values();
                for (size_t i = 0; i < values.size(); ++i) {
                    if (values[i] && values[i]->key() == name_str) {
                        set_value((int)i);
                        return;
                    }
                }
            }
        }
    }
}

String RiveViewModelEnum::get_value_name() const {
    if (!instance_enum) return "";
    
    int idx = get_value();
    rive::ViewModelProperty* prop_base = instance_enum->viewModelProperty();
    if (prop_base && prop_base->is<rive::ViewModelPropertyEnum>()) {
        rive::ViewModelPropertyEnum* vm_prop = prop_base->as<rive::ViewModelPropertyEnum>();
        if (vm_prop) {
            rive::DataEnum* data_enum = vm_prop->dataEnum();
            if (data_enum) {
                std::vector<rive::DataEnumValue*> values = data_enum->values();
                if (idx >= 0 && idx < (int)values.size() && values[idx]) {
                    return String(values[idx]->key().c_str());
                }
            }
        }
    }
    return "";
}

PackedStringArray RiveViewModelEnum::get_values() const {
    PackedStringArray arr;
    if (!instance_enum) return arr;
    
    rive::ViewModelProperty* prop_base = instance_enum->viewModelProperty();
    if (prop_base && prop_base->is<rive::ViewModelPropertyEnum>()) {
        rive::ViewModelPropertyEnum* vm_prop = prop_base->as<rive::ViewModelPropertyEnum>();
        if (vm_prop) {
            rive::DataEnum* data_enum = vm_prop->dataEnum();
            if (data_enum) {
                std::vector<rive::DataEnumValue*> values = data_enum->values();
                for (size_t i = 0; i < values.size(); ++i) {
                    if (values[i]) {
                        arr.push_back(String(values[i]->key().c_str()));
                    }
                }
            }
        }
    }
    return arr;
}

String RiveViewModelEnum::get_property_name() const {
    if (instance_enum) {
        return String(instance_enum->name().c_str());
    }
    return "";
}

// --- RiveViewModelTrigger ---

void RiveViewModelTrigger::_bind_methods() {
    ClassDB::bind_method(D_METHOD("fire"), &RiveViewModelTrigger::fire);
    ADD_SIGNAL(MethodInfo("triggered"));
}

void RiveViewModelTrigger::_init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceTrigger* p_trigger) {
    instance_ref = p_owner;
    instance_trigger = p_trigger;
}

void RiveViewModelTrigger::fire() {
    if (instance_trigger) {
        instance_trigger->trigger();
        emit_signal("triggered");
    }
}

String RiveViewModelTrigger::get_property_name() const {
    if (instance_trigger) {
        return String(instance_trigger->name().c_str());
    }
    return "";
}

// --- RiveViewModelImage ---

void RiveViewModelImage::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_value", "texture"), &RiveViewModelImage::set_value);
    // No get_value for now as it's hard to reconstruct Texture2D from RenderImage
}

void RiveViewModelImage::_init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceAssetImage* p_image) {
    instance_ref = p_owner;
    instance_image = p_image;
}

void RiveViewModelImage::set_value(const Ref<Texture2D>& p_texture) {
    if (!instance_image) return;
    
    if (p_texture.is_null()) {
        // TODO: Set null image?
        return;
    }

    rive::rcp<rive::RenderImage> render_image = rive_integration::RiveTextureFactory::make_image(p_texture);
    
    if (render_image) {
        instance_image->value(render_image.get());
    }
}

String RiveViewModelImage::get_property_name() const {
    if (instance_image) {
        return String(instance_image->name().c_str());
    }
    return "";
}

// --- RiveViewModelInstance ---

void RiveViewModelInstance::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_number_property", "path"), &RiveViewModelInstance::get_number_property);
    ClassDB::bind_method(D_METHOD("get_string_property", "path"), &RiveViewModelInstance::get_string_property);
    ClassDB::bind_method(D_METHOD("get_boolean_property", "path"), &RiveViewModelInstance::get_boolean_property);
    ClassDB::bind_method(D_METHOD("get_color_property", "path"), &RiveViewModelInstance::get_color_property);
    ClassDB::bind_method(D_METHOD("get_enum_property", "path"), &RiveViewModelInstance::get_enum_property);
    ClassDB::bind_method(D_METHOD("get_trigger_property", "path"), &RiveViewModelInstance::get_trigger_property);
    ClassDB::bind_method(D_METHOD("get_image_property", "path"), &RiveViewModelInstance::get_image_property);
}

void RiveViewModelInstance::_init(rive::rcp<rive::ViewModelInstance> p_instance) {
    instance = p_instance;
}

static rive::ViewModelInstance *resolve_view_model_instance(rive::ViewModelInstance *root, const String &path, String &out_property_name)
{
    if (!root)
        return nullptr;

    PackedStringArray parts = path.split(".");
    rive::ViewModelInstance *current = root;

    for (int i = 0; i < parts.size() - 1; ++i)
    {
        rive::ViewModelInstanceValue *prop = current->propertyValue(parts[i].utf8().get_data());
        if (!prop)
            return nullptr;

        rive::ViewModelInstanceViewModel *vm_prop = prop->as<rive::ViewModelInstanceViewModel>();
        if (!vm_prop)
            return nullptr;

        current = vm_prop->referenceViewModelInstance().get();
        if (!current)
            return nullptr;
    }

    out_property_name = parts[parts.size() - 1];
    return current;
}

Ref<RiveViewModelNumber> RiveViewModelInstance::get_number_property(const String& p_path) {
    if (!instance) return nullptr;
    if (property_cache.has(p_path)) {
        Ref<RiveViewModelProperty> prop = property_cache[p_path];
        if (prop.is_valid() && Object::cast_to<RiveViewModelNumber>(prop.ptr())) {
            return Ref<RiveViewModelNumber>(Object::cast_to<RiveViewModelNumber>(prop.ptr()));
        }
    }
    String prop_name;
    rive::ViewModelInstance* target_vm = resolve_view_model_instance(instance.get(), p_path, prop_name);
    if (target_vm) {
        rive::ViewModelInstanceValue* prop = target_vm->propertyValue(prop_name.utf8().get_data());
        if (prop && prop->is<rive::ViewModelInstanceNumber>()) {
            Ref<RiveViewModelNumber> ret;
            ret.instantiate();
            ret->_init(instance, prop->as<rive::ViewModelInstanceNumber>());
            property_cache[p_path] = ret;
            return ret;
        }
    }
    return nullptr;
}

Ref<RiveViewModelString> RiveViewModelInstance::get_string_property(const String& p_path) {
    if (!instance) return nullptr;
    if (property_cache.has(p_path)) {
        Ref<RiveViewModelProperty> prop = property_cache[p_path];
        if (prop.is_valid() && Object::cast_to<RiveViewModelString>(prop.ptr())) {
            return Ref<RiveViewModelString>(Object::cast_to<RiveViewModelString>(prop.ptr()));
        }
    }
    String prop_name;
    rive::ViewModelInstance* target_vm = resolve_view_model_instance(instance.get(), p_path, prop_name);
    if (target_vm) {
        rive::ViewModelInstanceValue* prop = target_vm->propertyValue(prop_name.utf8().get_data());
        if (prop && prop->is<rive::ViewModelInstanceString>()) {
            Ref<RiveViewModelString> ret;
            ret.instantiate();
            ret->_init(instance, prop->as<rive::ViewModelInstanceString>());
            property_cache[p_path] = ret;
            return ret;
        }
    }
    return nullptr;
}

Ref<RiveViewModelBoolean> RiveViewModelInstance::get_boolean_property(const String& p_path) {
    if (!instance) return nullptr;
    if (property_cache.has(p_path)) {
        Ref<RiveViewModelProperty> prop = property_cache[p_path];
        if (prop.is_valid() && Object::cast_to<RiveViewModelBoolean>(prop.ptr())) {
            return Ref<RiveViewModelBoolean>(Object::cast_to<RiveViewModelBoolean>(prop.ptr()));
        }
    }
    String prop_name;
    rive::ViewModelInstance* target_vm = resolve_view_model_instance(instance.get(), p_path, prop_name);
    if (target_vm) {
        rive::ViewModelInstanceValue* prop = target_vm->propertyValue(prop_name.utf8().get_data());
        if (prop && prop->is<rive::ViewModelInstanceBoolean>()) {
            Ref<RiveViewModelBoolean> ret;
            ret.instantiate();
            ret->_init(instance, prop->as<rive::ViewModelInstanceBoolean>());
            property_cache[p_path] = ret;
            return ret;
        }
    }
    return nullptr;
}

Ref<RiveViewModelColor> RiveViewModelInstance::get_color_property(const String& p_path) {
    if (!instance) return nullptr;
    if (property_cache.has(p_path)) {
        Ref<RiveViewModelProperty> prop = property_cache[p_path];
        if (prop.is_valid() && Object::cast_to<RiveViewModelColor>(prop.ptr())) {
            return Ref<RiveViewModelColor>(Object::cast_to<RiveViewModelColor>(prop.ptr()));
        }
    }
    String prop_name;
    rive::ViewModelInstance* target_vm = resolve_view_model_instance(instance.get(), p_path, prop_name);
    if (target_vm) {
        rive::ViewModelInstanceValue* prop = target_vm->propertyValue(prop_name.utf8().get_data());
        if (prop && prop->is<rive::ViewModelInstanceColor>()) {
            Ref<RiveViewModelColor> ret;
            ret.instantiate();
            ret->_init(instance, prop->as<rive::ViewModelInstanceColor>());
            property_cache[p_path] = ret;
            return ret;
        }
    }
    return nullptr;
}

Ref<RiveViewModelEnum> RiveViewModelInstance::get_enum_property(const String& p_path) {
    if (!instance) return nullptr;
    if (property_cache.has(p_path)) {
        Ref<RiveViewModelProperty> prop = property_cache[p_path];
        if (prop.is_valid() && Object::cast_to<RiveViewModelEnum>(prop.ptr())) {
            return Ref<RiveViewModelEnum>(Object::cast_to<RiveViewModelEnum>(prop.ptr()));
        }
    }
    String prop_name;
    rive::ViewModelInstance* target_vm = resolve_view_model_instance(instance.get(), p_path, prop_name);
    if (target_vm) {
        rive::ViewModelInstanceValue* prop = target_vm->propertyValue(prop_name.utf8().get_data());
        if (prop && prop->is<rive::ViewModelInstanceEnum>()) {
            Ref<RiveViewModelEnum> ret;
            ret.instantiate();
            ret->_init(instance, prop->as<rive::ViewModelInstanceEnum>());
            property_cache[p_path] = ret;
            return ret;
        }
    }
    return nullptr;
}

Ref<RiveViewModelTrigger> RiveViewModelInstance::get_trigger_property(const String& p_path) {
    if (!instance) return nullptr;
    if (property_cache.has(p_path)) {
        Ref<RiveViewModelProperty> prop = property_cache[p_path];
        if (prop.is_valid() && Object::cast_to<RiveViewModelTrigger>(prop.ptr())) {
            return Ref<RiveViewModelTrigger>(Object::cast_to<RiveViewModelTrigger>(prop.ptr()));
        }
    }
    String prop_name;
    rive::ViewModelInstance* target_vm = resolve_view_model_instance(instance.get(), p_path, prop_name);
    if (target_vm) {
        rive::ViewModelInstanceValue* prop = target_vm->propertyValue(prop_name.utf8().get_data());
        if (prop && prop->is<rive::ViewModelInstanceTrigger>()) {
            Ref<RiveViewModelTrigger> ret;
            ret.instantiate();
            ret->_init(instance, prop->as<rive::ViewModelInstanceTrigger>());
            property_cache[p_path] = ret;
            return ret;
        }
    }
    return nullptr;
}

Ref<RiveViewModelImage> RiveViewModelInstance::get_image_property(const String& p_path) {
    if (!instance) return nullptr;
    if (property_cache.has(p_path)) {
        Ref<RiveViewModelProperty> prop = property_cache[p_path];
        if (prop.is_valid() && Object::cast_to<RiveViewModelImage>(prop.ptr())) {
            return Ref<RiveViewModelImage>(Object::cast_to<RiveViewModelImage>(prop.ptr()));
        }
    }
    String prop_name;
    rive::ViewModelInstance* target_vm = resolve_view_model_instance(instance.get(), p_path, prop_name);
    if (target_vm) {
        rive::ViewModelInstanceValue* prop = target_vm->propertyValue(prop_name.utf8().get_data());
        if (prop && prop->is<rive::ViewModelInstanceAssetImage>()) {
            Ref<RiveViewModelImage> ret;
            ret.instantiate();
            ret->_init(instance, prop->as<rive::ViewModelInstanceAssetImage>());
            property_cache[p_path] = ret;
            return ret;
        }
    }
    return nullptr;
}

bool RiveViewModelInstance::_set(const StringName &p_name, const Variant &p_value) {
    if (!instance) return false;
    
    String name = p_name;
    rive::ViewModelInstanceValue* prop = instance->propertyValue(name.utf8().get_data());
    
    if (!prop) return false;
    
    if (prop->is<rive::ViewModelInstanceNumber>()) {
        prop->as<rive::ViewModelInstanceNumber>()->propertyValue((float)p_value);
        return true;
    } else if (prop->is<rive::ViewModelInstanceString>()) {
        prop->as<rive::ViewModelInstanceString>()->propertyValue(String(p_value).utf8().get_data());
        return true;
    } else if (prop->is<rive::ViewModelInstanceBoolean>()) {
        prop->as<rive::ViewModelInstanceBoolean>()->propertyValue((bool)p_value);
        return true;
    } else if (prop->is<rive::ViewModelInstanceColor>()) {
        Color c = p_value;
        uint32_t a = (uint32_t)(c.a * 255.0f);
        uint32_t r = (uint32_t)(c.r * 255.0f);
        uint32_t g = (uint32_t)(c.g * 255.0f);
        uint32_t b = (uint32_t)(c.b * 255.0f);
        uint32_t argb = (a << 24) | (r << 16) | (g << 8) | b;
        prop->as<rive::ViewModelInstanceColor>()->propertyValue(argb);
        return true;
    } else if (prop->is<rive::ViewModelInstanceEnum>()) {
        prop->as<rive::ViewModelInstanceEnum>()->value((uint32_t)p_value);
        return true;
    } else if (prop->is<rive::ViewModelInstanceAssetImage>()) {
        Ref<Texture2D> texture = p_value;
        if (texture.is_valid()) {
            rive::rcp<rive::RenderImage> render_image = rive_integration::RiveTextureFactory::make_image(texture);
            if (render_image) {
                prop->as<rive::ViewModelInstanceAssetImage>()->value(render_image.get());
                return true;
            }
        }
        return true; // Handled, even if failed or null
    }
    
    return false;
}

bool RiveViewModelInstance::_get(const StringName &p_name, Variant &r_ret) const {
    if (!instance) return false;
    
    String name = p_name;
    
    if (property_cache.has(name)) {
        r_ret = property_cache[name];
        return true;
    }

    if (child_vm_cache.has(name)) {
        r_ret = child_vm_cache[name];
        return true;
    }
    
    rive::ViewModelInstanceValue* prop = instance->propertyValue(name.utf8().get_data());
    
    if (!prop) return false;
    
    if (prop->is<rive::ViewModelInstanceNumber>()) {
        Ref<RiveViewModelNumber> ret;
        ret.instantiate();
        ret->_init(instance, prop->as<rive::ViewModelInstanceNumber>());
        property_cache[name] = ret;
        r_ret = ret;
        return true;
    } else if (prop->is<rive::ViewModelInstanceString>()) {
        Ref<RiveViewModelString> ret;
        ret.instantiate();
        ret->_init(instance, prop->as<rive::ViewModelInstanceString>());
        property_cache[name] = ret;
        r_ret = ret;
        return true;
    } else if (prop->is<rive::ViewModelInstanceBoolean>()) {
        Ref<RiveViewModelBoolean> ret;
        ret.instantiate();
        ret->_init(instance, prop->as<rive::ViewModelInstanceBoolean>());
        property_cache[name] = ret;
        r_ret = ret;
        return true;
    } else if (prop->is<rive::ViewModelInstanceColor>()) {
        Ref<RiveViewModelColor> ret;
        ret.instantiate();
        ret->_init(instance, prop->as<rive::ViewModelInstanceColor>());
        property_cache[name] = ret;
        r_ret = ret;
        return true;
    } else if (prop->is<rive::ViewModelInstanceEnum>()) {
        Ref<RiveViewModelEnum> ret;
        ret.instantiate();
        ret->_init(instance, prop->as<rive::ViewModelInstanceEnum>());
        property_cache[name] = ret;
        r_ret = ret;
        return true;
    } else if (prop->is<rive::ViewModelInstanceTrigger>()) {
        Ref<RiveViewModelTrigger> ret;
        ret.instantiate();
        ret->_init(instance, prop->as<rive::ViewModelInstanceTrigger>());
        property_cache[name] = ret;
        r_ret = ret;
        return true;
    } else if (prop->is<rive::ViewModelInstanceAssetImage>()) {
        Ref<RiveViewModelImage> ret;
        ret.instantiate();
        ret->_init(instance, prop->as<rive::ViewModelInstanceAssetImage>());
        property_cache[name] = ret;
        r_ret = ret;
        return true;
    } else if (prop->is<rive::ViewModelInstanceViewModel>()) {
        rive::ViewModelInstanceViewModel* vm_prop = prop->as<rive::ViewModelInstanceViewModel>();
        if (vm_prop) {
            rive::rcp<rive::ViewModelInstance> child_vm = vm_prop->referenceViewModelInstance();
            if (child_vm) {
                Ref<RiveViewModelInstance> wrapper;
                wrapper.instantiate();
                wrapper->_init(child_vm);
                child_vm_cache[name] = wrapper;
                r_ret = wrapper;
                return true;
            }
        }
    }
    
    return false;
}

void RiveViewModelInstance::_get_property_list(List<PropertyInfo> *p_list) const {
    if (!instance) return;
    
    std::vector<rive::ViewModelInstanceValue*> props = instance->propertyValues();
    for (auto prop : props) {
        if (!prop) continue;
        
        String name = prop->name().c_str();
        
        if (prop->is<rive::ViewModelInstanceNumber>()) {
            p_list->push_back(PropertyInfo(Variant::OBJECT, name, PROPERTY_HINT_RESOURCE_TYPE, "RiveViewModelNumber"));
        } else if (prop->is<rive::ViewModelInstanceString>()) {
            p_list->push_back(PropertyInfo(Variant::OBJECT, name, PROPERTY_HINT_RESOURCE_TYPE, "RiveViewModelString"));
        } else if (prop->is<rive::ViewModelInstanceBoolean>()) {
            p_list->push_back(PropertyInfo(Variant::OBJECT, name, PROPERTY_HINT_RESOURCE_TYPE, "RiveViewModelBoolean"));
        } else if (prop->is<rive::ViewModelInstanceColor>()) {
            p_list->push_back(PropertyInfo(Variant::OBJECT, name, PROPERTY_HINT_RESOURCE_TYPE, "RiveViewModelColor"));
        } else if (prop->is<rive::ViewModelInstanceEnum>()) {
            p_list->push_back(PropertyInfo(Variant::OBJECT, name, PROPERTY_HINT_RESOURCE_TYPE, "RiveViewModelEnum"));
        } else if (prop->is<rive::ViewModelInstanceTrigger>()) {
            p_list->push_back(PropertyInfo(Variant::OBJECT, name, PROPERTY_HINT_RESOURCE_TYPE, "RiveViewModelTrigger"));
        } else if (prop->is<rive::ViewModelInstanceAssetImage>()) {
            p_list->push_back(PropertyInfo(Variant::OBJECT, name, PROPERTY_HINT_RESOURCE_TYPE, "RiveViewModelImage"));
        } else if (prop->is<rive::ViewModelInstanceViewModel>()) {
             p_list->push_back(PropertyInfo(Variant::OBJECT, name, PROPERTY_HINT_RESOURCE_TYPE, "RiveViewModelInstance"));
        }
    }
}

