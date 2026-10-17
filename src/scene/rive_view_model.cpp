#include "rive_view_model.h"
#include "../renderer/rive_render_registry.h"
#include "../renderer/rive_renderer.h"
#include "../renderer/rive_texture_factory.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/os.hpp>
#include <rive/viewmodel/viewmodel_value_dependent.hpp>
#include <rive/viewmodel/runtime/viewmodel_runtime.hpp>
#include <rive/viewmodel/viewmodel.hpp>
#include <rive/viewmodel/viewmodel_property_viewmodel.hpp>
#include "../resources/rive_artboard_reference.h"

#include <rive/viewmodel/viewmodel_instance.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include <rive/viewmodel/viewmodel_instance_string.hpp>
#include <rive/viewmodel/viewmodel_instance_boolean.hpp>
#include <rive/viewmodel/viewmodel_instance_color.hpp>
#include <rive/viewmodel/viewmodel_instance_enum.hpp>
#include <rive/viewmodel/viewmodel_instance_trigger.hpp>
#include <rive/viewmodel/viewmodel_instance_viewmodel.hpp>
#include <rive/viewmodel/viewmodel_instance_asset_image.hpp>
#include <rive/viewmodel/viewmodel_instance_asset_font.hpp>
#include <rive/viewmodel/viewmodel_instance_artboard.hpp>
#include <rive/viewmodel/viewmodel_instance_list.hpp>
#include <rive/viewmodel/viewmodel_instance_asset_blob.hpp>
#include <rive/viewmodel/viewmodel_instance_symbol_list_index.hpp>
#include <rive/bindable_artboard.hpp>
#include <rive/viewmodel/viewmodel_property_enum.hpp>
#include <rive/viewmodel/data_enum.hpp>
#include <rive/viewmodel/data_enum_value.hpp>
#include <rive/factory.hpp>
#include <cmath>
#include <atomic>
#include <vector>
#include <algorithm>

class RivePropertyObserver : public rive::ViewModelValueDependent {
    rive::ViewModelInstanceValue *value;
    uint32_t previous_trigger_count = 0;
public:
    std::atomic<bool> pending_value_change{false};
    std::atomic<uint64_t> pending_trigger_events{0};
    explicit RivePropertyObserver(rive::ViewModelInstanceValue *p_value) : value(p_value) {
        if (value->is<rive::ViewModelInstanceTrigger>()) previous_trigger_count = value->as<rive::ViewModelInstanceTrigger>()->propertyValue();
        value->addDependent(this);
    }
    ~RivePropertyObserver() { value->removeDependent(this); }
    void addDirt(rive::ComponentDirt, bool) override {
        if (value->is<rive::ViewModelInstanceTrigger>()) {
            const uint32_t trigger_count = value->as<rive::ViewModelInstanceTrigger>()->propertyValue();
            if (trigger_count != 0) pending_trigger_events.fetch_add(static_cast<uint32_t>(trigger_count - previous_trigger_count));
            previous_trigger_count = trigger_count;
        } else pending_value_change.store(true);
    }
    void relinkDataBind() override { pending_value_change.store(true); }
};

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

RiveViewModelProperty::RiveViewModelProperty() = default;

void RiveViewModelProperty::_observe() {
    if (native_value && !observer) {
        rive_integration::run_on_rive_render_thread([&]() { observer = std::make_unique<RivePropertyObserver>(native_value); });
    }
}

bool RiveViewModelProperty::flush_changes() {
    OS *os = OS::get_singleton();
    if (!observer || (os && os->get_thread_caller_id() != os->get_main_thread_id())) return false;
    Ref<RiveViewModelProperty> keep_alive(this);
    const uint64_t events = observer->pending_trigger_events.exchange(0);
    const bool dirty = observer->pending_value_change.exchange(false);
    if (!events && !dirty) return false;
    ++change_revision;
    if (events) {
        constexpr uint64_t MAX_TRIGGER_EVENTS_PER_FLUSH = 1024;
        const uint64_t delivered = std::min(events, MAX_TRIGGER_EVENTS_PER_FLUSH);
        trigger_revision += delivered;
        for (uint64_t i = 0; i < delivered; ++i) emit_signal("triggered");
        if (events > delivered) observer->pending_trigger_events.fetch_add(events - delivered);
        return true;
    }
    if (Object::cast_to<RiveViewModelList>(this)) emit_signal("list_changed");
    else {
        Variant current;
        rive_integration::run_on_rive_render_thread([&]() {
            if (auto *p = Object::cast_to<RiveViewModelNumber>(this)) current = p->get_value();
            else if (auto *p = Object::cast_to<RiveViewModelString>(this)) current = p->get_value();
            else if (auto *p = Object::cast_to<RiveViewModelBoolean>(this)) current = p->get_value();
            else if (auto *p = Object::cast_to<RiveViewModelColor>(this)) current = p->get_value();
            else if (auto *p = Object::cast_to<RiveViewModelEnum>(this)) current = p->get_value();
            else if (auto *p = Object::cast_to<RiveViewModelImage>(this)) current = p->get_value();
            else if (auto *p = Object::cast_to<RiveViewModelFont>(this)) current = p->get_value();
            else if (auto *p = Object::cast_to<RiveViewModelArtboard>(this)) current = p->get_value();
            else if (auto *p = Object::cast_to<RiveViewModelBlob>(this)) current = p->get_value();
            else if (auto *p = Object::cast_to<RiveViewModelListIndex>(this)) current = p->get_value();
        });
        emit_signal("value_changed", current);
    }
    return true;
}

RiveViewModelProperty::~RiveViewModelProperty() {
    rive::rcp<rive::ViewModelInstance> old_instance = std::move(instance_ref);
    rive::rcp<rive::File> old_file = std::move(file_ref);
    auto release_old = [&]() { observer.reset(); old_instance.reset(); old_file.reset(); };
    if (!rive_integration::run_on_rive_render_thread(release_old)) {
        release_old();
    }
}

void RiveViewModelProperty::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_property_name"), &RiveViewModelProperty::get_property_name);
    ClassDB::bind_method(D_METHOD("flush_changes"), &RiveViewModelProperty::flush_changes);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "property_name"), "", "get_property_name");
}

void RiveViewModelNumber::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_value", "value"), &RiveViewModelNumber::set_value);
    ClassDB::bind_method(D_METHOD("get_value"), &RiveViewModelNumber::get_value);
    
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "value"), "set_value", "get_value");
    ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::FLOAT, "new_value")));
}

void RiveViewModelNumber::_init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceNumber* p_number) {
    instance_ref = p_owner;
    instance_number = p_number;
    native_value = p_number;
    _observe();
}

void RiveViewModelNumber::set_value(float p_value) {
    if (!instance_number || !std::isfinite(p_value)) return;
    rive_integration::run_on_rive_render_thread([&]() { instance_number->propertyValue(p_value); });
    flush_changes();
}

float RiveViewModelNumber::get_value() const {
    if (instance_number) {
        return instance_number->propertyValue();
    }
    return 0.0f;
}

String RiveViewModelNumber::get_property_name() const {
    if (instance_number) {
        return String::utf8(instance_number->name().c_str());
    }
    return "";
}

void RiveViewModelString::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_value", "value"), &RiveViewModelString::set_value);
    ClassDB::bind_method(D_METHOD("get_value"), &RiveViewModelString::get_value);
    
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "value"), "set_value", "get_value");
    ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::STRING, "new_value")));
}

void RiveViewModelString::_init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceString* p_string) {
    instance_ref = p_owner;
    instance_string = p_string;
    native_value = p_string;
    _observe();
}

void RiveViewModelString::set_value(const String& p_value) {
    if (!instance_string) return;
    const std::string value = p_value.utf8().get_data();
    rive_integration::run_on_rive_render_thread([&]() { instance_string->propertyValue(value); });
    flush_changes();
}

String RiveViewModelString::get_value() const {
    if (instance_string) {
        return String::utf8(instance_string->propertyValue().c_str());
    }
    return "";
}

String RiveViewModelString::get_property_name() const {
    if (instance_string) {
        return String::utf8(instance_string->name().c_str());
    }
    return "";
}

void RiveViewModelBoolean::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_value", "value"), &RiveViewModelBoolean::set_value);
    ClassDB::bind_method(D_METHOD("get_value"), &RiveViewModelBoolean::get_value);
    
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "value"), "set_value", "get_value");
    ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::BOOL, "new_value")));
}

void RiveViewModelBoolean::_init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceBoolean* p_boolean) {
    instance_ref = p_owner;
    instance_boolean = p_boolean;
    native_value = p_boolean;
    _observe();
}

void RiveViewModelBoolean::set_value(bool p_value) {
    if (!instance_boolean) return;
    rive_integration::run_on_rive_render_thread([&]() { instance_boolean->propertyValue(p_value); });
    flush_changes();
}

bool RiveViewModelBoolean::get_value() const {
    if (instance_boolean) {
        return instance_boolean->propertyValue();
    }
    return false;
}

String RiveViewModelBoolean::get_property_name() const {
    if (instance_boolean) {
        return String::utf8(instance_boolean->name().c_str());
    }
    return "";
}

void RiveViewModelColor::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_value", "value"), &RiveViewModelColor::set_value);
    ClassDB::bind_method(D_METHOD("get_value"), &RiveViewModelColor::get_value);
    
    ADD_PROPERTY(PropertyInfo(Variant::COLOR, "value"), "set_value", "get_value");
    ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::COLOR, "new_value")));
}

void RiveViewModelColor::_init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceColor* p_color) {
    instance_ref = p_owner;
    instance_color = p_color;
    native_value = p_color;
    _observe();
}

void RiveViewModelColor::set_value(Color p_value) {
    if (!std::isfinite(p_value.r) || !std::isfinite(p_value.g) || !std::isfinite(p_value.b) || !std::isfinite(p_value.a)) return;
    p_value = p_value.clamp();
    if (instance_color) {
        uint32_t a = (uint32_t)(p_value.a * 255.0f);
        uint32_t r = (uint32_t)(p_value.r * 255.0f);
        uint32_t g = (uint32_t)(p_value.g * 255.0f);
        uint32_t b = (uint32_t)(p_value.b * 255.0f);
        uint32_t argb = (a << 24) | (r << 16) | (g << 8) | b;
        
        if (instance_color->propertyValue() != argb) {
            rive_integration::run_on_rive_render_thread([&]() { instance_color->propertyValue(argb); });
            flush_changes();
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
        return String::utf8(instance_color->name().c_str());
    }
    return "";
}

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
    native_value = p_enum;
    _observe();
}

void RiveViewModelEnum::set_value(int p_value) {
    if (instance_enum && p_value >= 0 && p_value < get_values().size()) {
        if (instance_enum->propertyValue() != (uint32_t)p_value) {
            rive_integration::run_on_rive_render_thread([&]() { instance_enum->value((uint32_t)p_value); });
            flush_changes();
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
                    return String::utf8(values[idx]->key().c_str());
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
                    arr.push_back(values[i] ? String::utf8(values[i]->key().c_str()) : String());
                }
            }
        }
    }
    return arr;
}

String RiveViewModelEnum::get_property_name() const {
    if (instance_enum) {
        return String::utf8(instance_enum->name().c_str());
    }
    return "";
}

void RiveViewModelTrigger::_bind_methods() {
    ClassDB::bind_method(D_METHOD("fire"), &RiveViewModelTrigger::fire);
    ADD_SIGNAL(MethodInfo("triggered"));
}

void RiveViewModelTrigger::_init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceTrigger* p_trigger) {
    instance_ref = p_owner;
    instance_trigger = p_trigger;
    native_value = p_trigger;
    _observe();
}

void RiveViewModelTrigger::fire() {
    if (instance_trigger) {
        rive_integration::run_on_rive_render_thread([&]() { instance_trigger->trigger(); });
        flush_changes();
    }
}

String RiveViewModelTrigger::get_property_name() const {
    if (instance_trigger) {
        return String::utf8(instance_trigger->name().c_str());
    }
    return "";
}

void RiveViewModelImage::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_value", "texture"), &RiveViewModelImage::set_value);
    ClassDB::bind_method(D_METHOD("get_value"), &RiveViewModelImage::get_value);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "value", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_value", "get_value");
    ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::OBJECT, "texture")));
}

void RiveViewModelImage::_init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceAssetImage* p_image) {
    instance_ref = p_owner;
    instance_image = p_image;
    native_value = p_image;
    _observe();
}

bool RiveViewModelImage::set_value(const Ref<Texture2D>& p_texture) {
    if (!instance_image) return false;
    rive::rcp<rive::RenderImage> render_image;
    if (p_texture.is_valid()) {
        render_image = rive_integration::RiveTextureFactory::make_image(p_texture);
        if (!render_image) return false;
    }
    const bool applied = rive_integration::run_on_rive_render_thread([&]() {
        instance_image->value(render_image.get());
        cached_image = render_image.get();
        render_image.reset();
    });
    if (applied) { texture_value = p_texture; flush_changes(); }
    return applied;
}

String RiveViewModelImage::get_property_name() const {
    if (instance_image) {
        return String::utf8(instance_image->name().c_str());
    }
    return "";
}

RiveViewModelInstance::~RiveViewModelInstance() {
    property_cache.clear();
    child_vm_cache.clear();
    rive::rcp<rive::ViewModelInstance> old_instance = std::move(instance);
    rive::rcp<rive::File> old_file = std::move(file_ref);
    auto release_old = [&]() { old_instance.reset(); old_file.reset(); };
    if (!rive_integration::run_on_rive_render_thread(release_old)) {
        release_old();
    }
}

void RiveViewModelInstance::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_number_property", "path"), &RiveViewModelInstance::get_number_property);
    ClassDB::bind_method(D_METHOD("get_string_property", "path"), &RiveViewModelInstance::get_string_property);
    ClassDB::bind_method(D_METHOD("get_boolean_property", "path"), &RiveViewModelInstance::get_boolean_property);
    ClassDB::bind_method(D_METHOD("get_color_property", "path"), &RiveViewModelInstance::get_color_property);
    ClassDB::bind_method(D_METHOD("get_enum_property", "path"), &RiveViewModelInstance::get_enum_property);
    ClassDB::bind_method(D_METHOD("get_trigger_property", "path"), &RiveViewModelInstance::get_trigger_property);
    ClassDB::bind_method(D_METHOD("get_image_property", "path"), &RiveViewModelInstance::get_image_property);
    ClassDB::bind_method(D_METHOD("get_font_property", "path"), &RiveViewModelInstance::get_font_property);
    ClassDB::bind_method(D_METHOD("get_artboard_property", "path"), &RiveViewModelInstance::get_artboard_property);
    ClassDB::bind_method(D_METHOD("get_list_property", "path"), &RiveViewModelInstance::get_list_property);
    ClassDB::bind_method(D_METHOD("get_blob_property", "path"), &RiveViewModelInstance::get_blob_property);
    ClassDB::bind_method(D_METHOD("get_list_index_property", "path"), &RiveViewModelInstance::get_list_index_property);
    ClassDB::bind_method(D_METHOD("has_property", "path"), &RiveViewModelInstance::has_property);
    ClassDB::bind_method(D_METHOD("set_property_value", "path", "value"), &RiveViewModelInstance::set_property_value);
    ClassDB::bind_method(D_METHOD("get_property_value", "path"), &RiveViewModelInstance::get_property_value);
    ClassDB::bind_method(D_METHOD("flush_changes"), &RiveViewModelInstance::flush_changes);
    ClassDB::bind_method(D_METHOD("get_name"), &RiveViewModelInstance::get_name);
    ClassDB::bind_method(D_METHOD("get_view_model_name"), &RiveViewModelInstance::get_view_model_name);
    ClassDB::bind_method(D_METHOD("get_properties"), &RiveViewModelInstance::get_properties);
    ClassDB::bind_method(D_METHOD("get_property_info", "path"), &RiveViewModelInstance::get_property_info);
    ClassDB::bind_method(D_METHOD("get_view_model_property", "path"), &RiveViewModelInstance::get_view_model_property);
    ClassDB::bind_method(D_METHOD("replace_view_model", "path", "instance"), &RiveViewModelInstance::replace_view_model);
    ClassDB::bind_static_method("RiveViewModelInstance", D_METHOD("canonicalize_property_path", "path"), &RiveViewModelInstance::canonicalize_property_path);
    ADD_SIGNAL(MethodInfo("property_changed", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
    ADD_SIGNAL(MethodInfo("trigger_fired", PropertyInfo(Variant::STRING, "path")));
}

void RiveViewModelInstance::_init(rive::rcp<rive::ViewModelInstance> p_instance, rive::rcp<rive::File> p_file) {
    property_cache.clear();
    child_vm_cache.clear();
    flushed_revisions.clear();
    flushed_triggers.clear();
    rive::rcp<rive::ViewModelInstance> old_instance = std::move(instance);
    rive::rcp<rive::File> old_file = std::move(file_ref);
    instance = std::move(p_instance);
    file_ref = std::move(p_file);
    if (old_instance || old_file) {
        auto release_old = [&]() { old_instance.reset(); old_file.reset(); };
        if (!rive_integration::run_on_rive_render_thread(release_old)) {
            release_old();
        }
    }
}

String RiveViewModelInstance::canonicalize_property_path(const String &path) {
    if (path.is_empty()) return String();
    String normalized;
    for (int64_t i = 0; i < path.length(); ++i) {
        if (path[i] == '[') {
            const int64_t end = path.find("]", i + 1);
            if (end < 0) return String();
            const String index = path.substr(i + 1, end - i - 1);
            if (index.is_empty() || !index.is_valid_int() || index.to_int() < 0) return String();
            for (int64_t j = 0; j < index.length(); ++j) {
                if (index[j] < '0' || index[j] > '9') return String();
            }
            if (end + 1 < path.length() && path[end + 1] != '.' && path[end + 1] != '/') return String();
            normalized += "." + String::num_int64(index.to_int());
            i = end;
        } else if (path[i] == ']') {
            return String();
        } else {
            normalized += path[i] == '/' ? String(".") : path.substr(i, 1);
        }
    }
    PackedStringArray parts = normalized.split(".");
    for (int i = 0; i < parts.size(); ++i) {
        if (parts[i].is_empty()) return String();
    }
    return normalized;
}

Ref<Texture2D> RiveViewModelImage::get_value() const {
    bool matches = false;
    if (instance_image) rive_integration::run_on_rive_render_thread([&]() {
        matches = instance_image->propertyValue() == UINT32_MAX && instance_image->asset()->renderImage() == cached_image;
    });
    return matches ? texture_value : Ref<Texture2D>();
}

static rive::ViewModelInstance *resolve_view_model_instance(rive::ViewModelInstance *root, const String &path, String &out_property_name)
{
    const String normalized = RiveViewModelInstance::canonicalize_property_path(path);
    if (!root || normalized.is_empty()) return nullptr;
    const PackedStringArray parts = normalized.split(".");
    rive::ViewModelInstance *current = root;
    for (int i = 0; i < parts.size() - 1; ++i)
    {
        rive::ViewModelInstanceValue *prop = current->propertyValue(parts[i].utf8().get_data());
        if (!prop) return nullptr;
        if (prop->is<rive::ViewModelInstanceViewModel>()) {
            auto *vm_prop = prop->as<rive::ViewModelInstanceViewModel>();
            current = vm_prop->referenceViewModelInstance().get();
        } else if (prop->is<rive::ViewModelInstanceList>()) {
            auto *list = prop->as<rive::ViewModelInstanceList>();
            if (++i >= parts.size() - 1 || !parts[i].is_valid_int()) return nullptr;
            const int64_t index = parts[i].to_int();
            if (index < 0 || static_cast<uint64_t>(index) >= list->listItems().size()) return nullptr;
            auto item = list->item(static_cast<uint32_t>(index));
            current = item ? item->viewModelInstance().get() : nullptr;
        } else {
            return nullptr;
        }
        if (!current) return nullptr;
    }
    out_property_name = parts[parts.size() - 1];
    return current;
}

namespace {
template <typename Native, typename Wrapper>
Ref<Wrapper> property_wrapper(rive::ViewModelInstance *root, const String &path,
        const rive::rcp<rive::File> &file, HashMap<String, Ref<RiveViewModelProperty>> &cache) {
    String name;
    rive::ViewModelInstance *owner = resolve_view_model_instance(root, path, name);
    auto *value = owner ? owner->propertyValue(name.utf8().get_data()) : nullptr;
    auto *native = value && value->is<Native>() ? value->as<Native>() : nullptr;
    if (!native) return nullptr;
    const String key = RiveViewModelInstance::canonicalize_property_path(path);
    if (auto *entry = cache.getptr(key)) {
        auto *wrapper = Object::cast_to<Wrapper>(entry->ptr());
        if (wrapper && wrapper->_get_native_value() == value) return Ref<Wrapper>(wrapper);
    }
    Ref<Wrapper> result;
    result.instantiate();
    result->_init(rive::ref_rcp(owner), native);
    result->_retain_file(file);
    cache[key] = result;
    return result;
}
}

Ref<RiveViewModelNumber> RiveViewModelInstance::get_number_property(const String& p_path) {
    return property_wrapper<rive::ViewModelInstanceNumber, RiveViewModelNumber>(instance.get(), p_path, file_ref, property_cache);
}

Ref<RiveViewModelBlob> RiveViewModelInstance::get_blob_property(const String &p_path) {
    return property_wrapper<rive::ViewModelInstanceAssetBlob, RiveViewModelBlob>(instance.get(), p_path, file_ref, property_cache);
}

Ref<RiveViewModelListIndex> RiveViewModelInstance::get_list_index_property(const String &p_path) {
    return property_wrapper<rive::ViewModelInstanceSymbolListIndex, RiveViewModelListIndex>(instance.get(), p_path, file_ref, property_cache);
}

Ref<RiveViewModelString> RiveViewModelInstance::get_string_property(const String& p_path) {
    return property_wrapper<rive::ViewModelInstanceString, RiveViewModelString>(instance.get(), p_path, file_ref, property_cache);
}

Ref<RiveViewModelBoolean> RiveViewModelInstance::get_boolean_property(const String& p_path) {
    return property_wrapper<rive::ViewModelInstanceBoolean, RiveViewModelBoolean>(instance.get(), p_path, file_ref, property_cache);
}

Ref<RiveViewModelColor> RiveViewModelInstance::get_color_property(const String& p_path) {
    return property_wrapper<rive::ViewModelInstanceColor, RiveViewModelColor>(instance.get(), p_path, file_ref, property_cache);
}

Ref<RiveViewModelEnum> RiveViewModelInstance::get_enum_property(const String& p_path) {
    return property_wrapper<rive::ViewModelInstanceEnum, RiveViewModelEnum>(instance.get(), p_path, file_ref, property_cache);
}

Ref<RiveViewModelTrigger> RiveViewModelInstance::get_trigger_property(const String& p_path) {
    return property_wrapper<rive::ViewModelInstanceTrigger, RiveViewModelTrigger>(instance.get(), p_path, file_ref, property_cache);
}

Ref<RiveViewModelImage> RiveViewModelInstance::get_image_property(const String& p_path) {
    return property_wrapper<rive::ViewModelInstanceAssetImage, RiveViewModelImage>(instance.get(), p_path, file_ref, property_cache);
}

Ref<RiveViewModelFont> RiveViewModelInstance::get_font_property(const String &p_path) {
    return property_wrapper<rive::ViewModelInstanceAssetFont, RiveViewModelFont>(instance.get(), p_path, file_ref, property_cache);
}

Ref<RiveViewModelArtboard> RiveViewModelInstance::get_artboard_property(const String &p_path) {
    return property_wrapper<rive::ViewModelInstanceArtboard, RiveViewModelArtboard>(instance.get(), p_path, file_ref, property_cache);
}

Ref<RiveViewModelList> RiveViewModelInstance::get_list_property(const String &p_path) {
    return property_wrapper<rive::ViewModelInstanceList, RiveViewModelList>(instance.get(), p_path, file_ref, property_cache);
}

Ref<RiveViewModelInstance> RiveViewModelInstance::get_view_model_property(const String &p_path) {
    const String key = canonicalize_property_path(p_path);
    String name;
    auto *owner = resolve_view_model_instance(instance.get(), key, name);
    auto *value = owner ? owner->propertyValue(name.utf8().get_data()) : nullptr;
    if (!value || !value->is<rive::ViewModelInstanceViewModel>()) return nullptr;
    auto child = value->as<rive::ViewModelInstanceViewModel>()->referenceViewModelInstance();
    if (!child) return nullptr;
    if (auto *cached = child_vm_cache.getptr(key)) {
        if ((*cached)->_get_native_instance() == child) return *cached;
    }
    Ref<RiveViewModelInstance> wrapper;
    wrapper.instantiate();
    wrapper->_init(child, file_ref);
    child_vm_cache[key] = wrapper;
    return wrapper;
}

bool RiveViewModelInstance::replace_view_model(const String &p_path, const Ref<RiveViewModelInstance> &p_value) {
    if (!instance || p_value.is_null() || !p_value->instance || !file_ref || file_ref != p_value->file_ref) return false;
    bool applied = false;
    rive_integration::run_on_rive_render_thread([&]() {
        String name;
        auto *owner = resolve_view_model_instance(instance.get(), p_path, name);
        auto *value = owner ? owner->propertyValue(name.utf8().get_data()) : nullptr;
        if (!value || !value->is<rive::ViewModelInstanceViewModel>()) return;
        auto *schema = value->viewModelProperty();
        if (!schema || !schema->is<rive::ViewModelPropertyViewModel>() ||
                schema->as<rive::ViewModelPropertyViewModel>()->viewModelReferenceId() != p_value->instance->viewModelId() ||
                !rive_is_safe_view_model_reference(p_value->instance.get(), instance.get(), owner)) return;
        applied = owner->replaceViewModelByProperty(value->as<rive::ViewModelInstanceViewModel>(), p_value->instance);
    });
    if (applied) {
        child_vm_cache.erase(canonicalize_property_path(p_path));
        emit_signal("property_changed", canonicalize_property_path(p_path), p_value);
    }
    return applied;
}

String RiveViewModelInstance::get_name() const {
    return instance ? String::utf8(instance->name().c_str()) : String();
}

String RiveViewModelInstance::get_view_model_name() const {
    return instance && instance->viewModel() ? String::utf8(instance->viewModel()->name().c_str()) : String();
}

Array RiveViewModelInstance::get_properties() const {
    Array result;
    if (!instance || !instance->viewModel()) return result;
    auto definitions = instance->viewModel()->properties();
    const auto properties = rive::ViewModelRuntime::buildPropertiesData(definitions);
    static const char *types[] = {"none", "string", "number", "boolean", "color", "list", "enum", "trigger", "view_model", "integer", "list_index", "image", "artboard", "font", "blob"};
    for (const auto &property : properties) {
        Dictionary info;
        info["name"] = String::utf8(property.name.c_str());
        const size_t type = static_cast<size_t>(property.type);
        info["type"] = type < sizeof(types) / sizeof(types[0]) ? String(types[type]) : String("none");
        info["native_type"] = static_cast<int>(property.type);
        info["enum_name"] = String::utf8(property.enumName.c_str());
        info["read_only"] = property.type == rive::DataType::symbolListIndex;
        if (property.type == rive::DataType::enumType) {
            auto value = const_cast<RiveViewModelInstance *>(this)->get_enum_property(info["name"]);
            if (value.is_valid()) info["values"] = value->get_values();
        }
        result.push_back(info);
    }
    return result;
}

Dictionary RiveViewModelInstance::get_property_info(const String &p_path) const {
    String name;
    auto *owner = resolve_view_model_instance(instance.get(), p_path, name);
    if (!owner) return Dictionary();
    Ref<RiveViewModelInstance> wrapper;
    wrapper.instantiate();
    wrapper->_init(rive::ref_rcp(owner), file_ref);
    const Array properties = wrapper->get_properties();
    for (int64_t i = 0; i < properties.size(); ++i) {
        const Dictionary info = properties[i];
        if (String(info["name"]) == name) return info;
    }
    return Dictionary();
}

bool RiveViewModelInstance::flush_changes() {
    OS *os = OS::get_singleton();
    if (os && os->get_thread_caller_id() != os->get_main_thread_id()) return false;
    Ref<RiveViewModelInstance> keep_alive(this);
    const auto original_instance = instance;
    std::vector<std::pair<String, Ref<RiveViewModelProperty>>> properties;
    std::vector<std::pair<String, Ref<RiveViewModelInstance>>> children;
    for (const auto &entry : property_cache) properties.emplace_back(entry.key, entry.value);
    for (const auto &entry : child_vm_cache) children.emplace_back(entry.key, entry.value);
    bool changed = false;
    for (const auto &entry : properties) {
        bool replaced = false;
        rive_integration::run_on_rive_render_thread([&]() {
            String name;
            auto *owner = resolve_view_model_instance(instance.get(), entry.first, name);
            auto *current = owner ? owner->propertyValue(name.utf8().get_data()) : nullptr;
            replaced = current != entry.second->_get_native_value();
        });
        if (replaced) {
            changed = true;
            property_cache.erase(entry.first);
            flushed_revisions.erase(entry.first);
            flushed_triggers.erase(entry.first);
            const Variant current = get_property_value(entry.first);
            emit_signal("property_changed", entry.first, current);
            if (instance != original_instance) break;
            continue;
        }
        const bool property_dirty = entry.second->flush_changes();
        if (instance != original_instance) break;
        const uint64_t revision = entry.second->_get_change_revision();
        const uint64_t seen = flushed_revisions.has(entry.first) ? flushed_revisions[entry.first] : 0;
        if (property_dirty || revision != seen) {
            changed = true;
            flushed_revisions[entry.first] = revision;
            if (Object::cast_to<RiveViewModelTrigger>(entry.second.ptr())) {
                const uint64_t count = entry.second->_get_trigger_revision();
                const uint64_t previous = flushed_triggers.has(entry.first) ? flushed_triggers[entry.first] : 0;
                flushed_triggers[entry.first] = count;
                for (uint64_t i = previous; i < count && i - previous < 1024; ++i) emit_signal("trigger_fired", entry.first);
            } else emit_signal("property_changed", entry.first, get_property_value(entry.first));
            if (instance != original_instance) break;
        }
    }
    for (const auto &entry : children) {
        if (instance != original_instance) break;
        const auto current = get_view_model_property(entry.first);
        if (current != entry.second) {
            changed = true;
            if (current.is_null()) child_vm_cache.erase(entry.first);
            emit_signal("property_changed", entry.first, current);
            continue;
        }
        if (entry.second->flush_changes()) {
            changed = true;
            emit_signal("property_changed", entry.first, entry.second);
        }
    }
    return changed;
}

bool RiveViewModelInstance::has_property(const String &p_path) const {
    String name;
    auto *owner = resolve_view_model_instance(instance.get(), p_path, name);
    return owner && owner->propertyValue(name.utf8().get_data());
}

bool RiveViewModelInstance::set_property_value(const String &p_path, const Variant &p_value) {
    String name;
    auto *owner = resolve_view_model_instance(instance.get(), p_path, name);
    auto *prop = owner ? owner->propertyValue(name.utf8().get_data()) : nullptr;
    if (!prop) return false;
    const auto type = p_value.get_type();
    if (prop->is<rive::ViewModelInstanceNumber>()) {
        if (type != Variant::FLOAT && type != Variant::INT) return false;
        const double value = p_value;
        if (!std::isfinite(value) || !std::isfinite(static_cast<float>(value))) return false;
        get_number_property(p_path)->set_value(static_cast<float>(value));
        return true;
    }
    if (prop->is<rive::ViewModelInstanceString>()) {
        if (type != Variant::STRING && type != Variant::STRING_NAME) return false;
        get_string_property(p_path)->set_value(p_value);
        return true;
    }
    if (prop->is<rive::ViewModelInstanceBoolean>()) {
        if (type != Variant::BOOL) return false;
        get_boolean_property(p_path)->set_value(p_value);
        return true;
    }
    if (prop->is<rive::ViewModelInstanceColor>()) {
        if (type != Variant::COLOR) return false;
        Color value = p_value;
        if (!std::isfinite(value.r) || !std::isfinite(value.g) || !std::isfinite(value.b) || !std::isfinite(value.a)) return false;
        get_color_property(p_path)->set_value(value);
        return true;
    }
    if (prop->is<rive::ViewModelInstanceEnum>()) {
        auto wrapper = get_enum_property(p_path);
        auto options = wrapper->get_values();
        int64_t index = -1;
        if (type == Variant::INT) index = p_value;
        else if (type == Variant::STRING || type == Variant::STRING_NAME) index = options.find(String(p_value));
        if (index < 0 || index >= options.size() || options[index].is_empty()) return false;
        wrapper->set_value(static_cast<int>(index));
        return wrapper->get_value() == index;
    }
    if (prop->is<rive::ViewModelInstanceTrigger>()) {
        if (type != Variant::BOOL) return false;
        if (bool(p_value)) get_trigger_property(p_path)->fire();
        return true;
    }
    if (prop->is<rive::ViewModelInstanceAssetImage>()) {
        Ref<Texture2D> texture = p_value;
        if (type != Variant::NIL && (type != Variant::OBJECT || (static_cast<Object *>(p_value) && texture.is_null()))) return false;
        return get_image_property(p_path)->set_value(texture);
    }
    if (prop->is<rive::ViewModelInstanceAssetFont>()) {
        if (type != Variant::OBJECT && type != Variant::NIL) return false;
        Ref<RefCounted> font = p_value;
        if (type == Variant::OBJECT && static_cast<Object *>(p_value) && font.is_null()) return false;
        return get_font_property(p_path)->set_value(font);
    }
    if (prop->is<rive::ViewModelInstanceArtboard>()) {
        if (type == Variant::NIL) return get_artboard_property(p_path)->set_value(String());
        if (type == Variant::OBJECT) {
            Ref<RiveArtboardReference> reference = p_value;
            return reference.is_valid() && get_artboard_property(p_path)->set_reference(reference);
        }
        if (type != Variant::STRING && type != Variant::STRING_NAME) return false;
        return get_artboard_property(p_path)->set_value(p_value);
    }
    if (prop->is<rive::ViewModelInstanceList>()) {
        if (type != Variant::ARRAY) return false;
        return get_list_property(p_path)->set_items(p_value);
    }
    if (prop->is<rive::ViewModelInstanceAssetBlob>()) {
        if (type == Variant::NIL) { get_blob_property(p_path)->clear(); return true; }
        return type == Variant::PACKED_BYTE_ARRAY && get_blob_property(p_path)->set_value(p_value);
    }
    if (prop->is<rive::ViewModelInstanceViewModel>()) {
        if (type != Variant::OBJECT) return false;
        Ref<RiveViewModelInstance> child = p_value;
        return child.is_valid() && replace_view_model(p_path, child);
    }
    return false;
}

Variant RiveViewModelInstance::get_property_value(const String &p_path) const {
    Variant wrapper;
    if (!_get(StringName(p_path), wrapper)) return Variant();
    Object *object = wrapper;
    if (auto *value = Object::cast_to<RiveViewModelNumber>(object)) return value->get_value();
    if (auto *value = Object::cast_to<RiveViewModelString>(object)) return value->get_value();
    if (auto *value = Object::cast_to<RiveViewModelBoolean>(object)) return value->get_value();
    if (auto *value = Object::cast_to<RiveViewModelColor>(object)) return value->get_value();
    if (auto *value = Object::cast_to<RiveViewModelEnum>(object)) return value->get_value();
    if (Object::cast_to<RiveViewModelTrigger>(object)) return false;
    if (auto *value = Object::cast_to<RiveViewModelImage>(object)) return value->get_value();
    if (auto *value = Object::cast_to<RiveViewModelFont>(object)) return value->get_value();
    if (auto *value = Object::cast_to<RiveViewModelArtboard>(object)) {
        const auto reference = value->get_reference();
        return reference.is_valid() ? Variant(reference) : Variant(value->get_value());
    }
    if (auto *value = Object::cast_to<RiveViewModelList>(object)) return value->get_items();
    if (auto *value = Object::cast_to<RiveViewModelBlob>(object)) return value->get_value();
    if (auto *value = Object::cast_to<RiveViewModelListIndex>(object)) return value->get_value();
    return wrapper;
}

bool RiveViewModelInstance::_set(const StringName &p_name, const Variant &p_value) {
    return set_property_value(String(p_name), p_value);
}

bool RiveViewModelInstance::_get(const StringName &p_name, Variant &r_ret) const {
    if (!instance) return false;
    
    String name = p_name;
    
    String property_name;
    auto *owner = resolve_view_model_instance(instance.get(), name, property_name);
    auto *prop = owner ? owner->propertyValue(property_name.utf8().get_data()) : nullptr;
    
    if (!prop) return false;
    
    if (prop->is<rive::ViewModelInstanceNumber>()) {
        r_ret = const_cast<RiveViewModelInstance *>(this)->get_number_property(name);
        return true;
    } else if (prop->is<rive::ViewModelInstanceString>()) {
        r_ret = const_cast<RiveViewModelInstance *>(this)->get_string_property(name);
        return true;
    } else if (prop->is<rive::ViewModelInstanceBoolean>()) {
        r_ret = const_cast<RiveViewModelInstance *>(this)->get_boolean_property(name);
        return true;
    } else if (prop->is<rive::ViewModelInstanceColor>()) {
        r_ret = const_cast<RiveViewModelInstance *>(this)->get_color_property(name);
        return true;
    } else if (prop->is<rive::ViewModelInstanceEnum>()) {
        r_ret = const_cast<RiveViewModelInstance *>(this)->get_enum_property(name);
        return true;
    } else if (prop->is<rive::ViewModelInstanceTrigger>()) {
        r_ret = const_cast<RiveViewModelInstance *>(this)->get_trigger_property(name);
        return true;
    } else if (prop->is<rive::ViewModelInstanceAssetImage>()) {
        r_ret = const_cast<RiveViewModelInstance *>(this)->get_image_property(name);
        return true;
    } else if (prop->is<rive::ViewModelInstanceAssetFont>()) {
        r_ret = const_cast<RiveViewModelInstance *>(this)->get_font_property(name);
        return true;
    } else if (prop->is<rive::ViewModelInstanceArtboard>()) {
        r_ret = const_cast<RiveViewModelInstance *>(this)->get_artboard_property(name);
        return true;
    } else if (prop->is<rive::ViewModelInstanceList>()) {
        r_ret = const_cast<RiveViewModelInstance *>(this)->get_list_property(name);
        return true;
    } else if (prop->is<rive::ViewModelInstanceAssetBlob>()) {
        r_ret = const_cast<RiveViewModelInstance *>(this)->get_blob_property(name);
        return true;
    } else if (prop->is<rive::ViewModelInstanceSymbolListIndex>()) {
        r_ret = const_cast<RiveViewModelInstance *>(this)->get_list_index_property(name);
        return true;
    } else if (prop->is<rive::ViewModelInstanceViewModel>()) {
        r_ret = const_cast<RiveViewModelInstance *>(this)->get_view_model_property(name);
        return static_cast<Object *>(r_ret) != nullptr;
    }
    
    return false;
}

void RiveViewModelInstance::_get_property_list(List<PropertyInfo> *p_list) const {
    if (!instance) return;
    
    const auto& props = instance->propertyValues();
    for (const auto &entry : props) {
        rive::ViewModelInstanceValue *prop = view_model_value_ptr(entry);
        if (!prop) continue;
        
        String name = String::utf8(prop->name().c_str());
        
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
        } else if (prop->is<rive::ViewModelInstanceAssetFont>()) {
            p_list->push_back(PropertyInfo(Variant::OBJECT, name, PROPERTY_HINT_RESOURCE_TYPE, "RiveViewModelFont"));
        } else if (prop->is<rive::ViewModelInstanceArtboard>()) {
            p_list->push_back(PropertyInfo(Variant::OBJECT, name, PROPERTY_HINT_RESOURCE_TYPE, "RiveViewModelArtboard"));
        } else if (prop->is<rive::ViewModelInstanceList>()) {
            p_list->push_back(PropertyInfo(Variant::OBJECT, name, PROPERTY_HINT_RESOURCE_TYPE, "RiveViewModelList"));
        } else if (prop->is<rive::ViewModelInstanceAssetBlob>()) {
            p_list->push_back(PropertyInfo(Variant::OBJECT, name, PROPERTY_HINT_RESOURCE_TYPE, "RiveViewModelBlob"));
        } else if (prop->is<rive::ViewModelInstanceSymbolListIndex>()) {
            p_list->push_back(PropertyInfo(Variant::OBJECT, name, PROPERTY_HINT_RESOURCE_TYPE, "RiveViewModelListIndex", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY));
        } else if (prop->is<rive::ViewModelInstanceViewModel>()) {
             p_list->push_back(PropertyInfo(Variant::OBJECT, name, PROPERTY_HINT_RESOURCE_TYPE, "RiveViewModelInstance"));
        }
    }
}

