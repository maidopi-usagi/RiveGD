#ifndef RIVE_VIEW_MODEL_H
#define RIVE_VIEW_MODEL_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <rive/viewmodel/viewmodel_instance.hpp>

// Forward declarations for Rive types to avoid including everything in header if possible,
namespace rive {
    class ViewModelInstanceValue;
    class ViewModelInstanceNumber;
    class ViewModelInstanceString;
    class ViewModelInstanceBoolean;
    class ViewModelInstanceColor;
    class ViewModelInstanceEnum;
    class ViewModelInstanceTrigger;
}

using namespace godot;

class RiveViewModelInstance;

class RiveViewModelProperty : public RefCounted {
    GDCLASS(RiveViewModelProperty, RefCounted);

protected:
    rive::rcp<rive::ViewModelInstance> instance_ref;
    static void _bind_methods();

public:
    virtual String get_property_name() const = 0;
};

class RiveViewModelNumber : public RiveViewModelProperty {
    GDCLASS(RiveViewModelNumber, RiveViewModelProperty);

private:
    rive::ViewModelInstanceNumber* instance_number = nullptr;

protected:
    static void _bind_methods();

public:
    void _init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceNumber* p_number);
    void set_value(float p_value);
    float get_value() const;
    String get_property_name() const override;
};

class RiveViewModelString : public RiveViewModelProperty {
    GDCLASS(RiveViewModelString, RiveViewModelProperty);

private:
    rive::ViewModelInstanceString* instance_string = nullptr;

protected:
    static void _bind_methods();

public:
    void _init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceString* p_string);
    void set_value(const String& p_value);
    String get_value() const;
    String get_property_name() const override;
};

class RiveViewModelBoolean : public RiveViewModelProperty {
    GDCLASS(RiveViewModelBoolean, RiveViewModelProperty);

private:
    rive::ViewModelInstanceBoolean* instance_boolean = nullptr;

protected:
    static void _bind_methods();

public:
    void _init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceBoolean* p_boolean);
    void set_value(bool p_value);
    bool get_value() const;
    String get_property_name() const override;
};

class RiveViewModelColor : public RiveViewModelProperty {
    GDCLASS(RiveViewModelColor, RiveViewModelProperty);

private:
    rive::ViewModelInstanceColor* instance_color = nullptr;

protected:
    static void _bind_methods();

public:
    void _init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceColor* p_color);
    void set_value(Color p_value);
    Color get_value() const;
    String get_property_name() const override;
};

class RiveViewModelEnum : public RiveViewModelProperty {
    GDCLASS(RiveViewModelEnum, RiveViewModelProperty);

private:
    rive::ViewModelInstanceEnum* instance_enum = nullptr;

protected:
    static void _bind_methods();

public:
    void _init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceEnum* p_enum);
    void set_value(int p_value);
    int get_value() const;
    
    void set_value_by_name(const String& p_name);
    String get_value_name() const;
    PackedStringArray get_values() const;
    
    String get_property_name() const override;
};

class RiveViewModelTrigger : public RiveViewModelProperty {
    GDCLASS(RiveViewModelTrigger, RiveViewModelProperty);

private:
    rive::ViewModelInstanceTrigger* instance_trigger = nullptr;

protected:
    static void _bind_methods();

public:
    void _init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceTrigger* p_trigger);
    void fire();
    String get_property_name() const override;
};

class RiveViewModelInstance : public RefCounted {
    GDCLASS(RiveViewModelInstance, RefCounted);

private:
    rive::rcp<rive::ViewModelInstance> instance;
    mutable HashMap<String, Ref<RiveViewModelProperty>> property_cache;
    mutable HashMap<String, Ref<RiveViewModelInstance>> child_vm_cache;

protected:
    static void _bind_methods();

public:
    void _init(rive::rcp<rive::ViewModelInstance> p_instance);
    
    Ref<RiveViewModelNumber> get_number_property(const String& p_path);
    Ref<RiveViewModelString> get_string_property(const String& p_path);
    Ref<RiveViewModelBoolean> get_boolean_property(const String& p_path);
    Ref<RiveViewModelColor> get_color_property(const String& p_path);
    Ref<RiveViewModelEnum> get_enum_property(const String& p_path);
    Ref<RiveViewModelTrigger> get_trigger_property(const String& p_path);

    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(List<PropertyInfo> *p_list) const;
};

#endif // RIVE_VIEW_MODEL_H
