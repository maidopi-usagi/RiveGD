#ifndef RIVE_VIEW_MODEL_H
#define RIVE_VIEW_MODEL_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/font_file.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <rive/viewmodel/viewmodel_instance.hpp>
#include <rive/file.hpp>
#include <memory>

namespace rive {
    class ViewModelInstanceValue;
    class ViewModelInstanceNumber;
    class ViewModelInstanceString;
    class ViewModelInstanceBoolean;
    class ViewModelInstanceColor;
    class ViewModelInstanceEnum;
    class ViewModelInstanceTrigger;
    class ViewModelInstanceAssetImage;
    class ViewModelInstanceAssetFont;
    class ViewModelInstanceArtboard;
    class ViewModelInstanceList;
    class ViewModelInstanceAssetBlob;
    class ViewModelInstanceSymbolListIndex;
    class File;
}

class RiveFont;
class RiveArtboardReference;
class RivePropertyObserver;

using namespace godot;

class RiveViewModelInstance;
bool rive_is_safe_view_model_reference(rive::ViewModelInstance *p_candidate,
        rive::ViewModelInstance *p_root, rive::ViewModelInstance *p_parent);

class RiveViewModelProperty : public RefCounted {
    GDCLASS(RiveViewModelProperty, RefCounted);

protected:
    rive::rcp<rive::File> file_ref;
    rive::rcp<rive::ViewModelInstance> instance_ref;
    rive::ViewModelInstanceValue *native_value = nullptr;
    std::unique_ptr<RivePropertyObserver> observer;
    uint64_t change_revision = 0;
    uint64_t trigger_revision = 0;
    void _observe();
    static void _bind_methods();

public:
    RiveViewModelProperty();
    virtual ~RiveViewModelProperty();
    virtual String get_property_name() const = 0;
    void _retain_file(rive::rcp<rive::File> p_file) { file_ref = std::move(p_file); }
    rive::ViewModelInstanceValue *_get_native_value() const { return native_value; }
    virtual bool flush_changes();
    uint64_t _get_change_revision() const { return change_revision; }
    uint64_t _get_trigger_revision() const { return trigger_revision; }
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

class RiveViewModelImage : public RiveViewModelProperty {
    GDCLASS(RiveViewModelImage, RiveViewModelProperty);

private:
    rive::ViewModelInstanceAssetImage* instance_image = nullptr;
    Ref<Texture2D> texture_value;
    rive::RenderImage *cached_image = nullptr;

protected:
    static void _bind_methods();

public:
    void _init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceAssetImage* p_image);
    bool set_value(const Ref<Texture2D>& p_texture);
    Ref<Texture2D> get_value() const;
    String get_property_name() const override;
};

class RiveViewModelFont : public RiveViewModelProperty {
    GDCLASS(RiveViewModelFont, RiveViewModelProperty);
    rive::ViewModelInstanceAssetFont *instance_font = nullptr;
    Ref<RefCounted> font_value;
    rive::Font *cached_font = nullptr;
protected:
    static void _bind_methods();
public:
    void _init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceAssetFont *p_font);
    bool set_value(const Ref<RefCounted> &p_font);
    Ref<RefCounted> get_value() const;
    String get_property_name() const override;
};

class RiveViewModelArtboard : public RiveViewModelProperty {
    GDCLASS(RiveViewModelArtboard, RiveViewModelProperty);
    rive::ViewModelInstanceArtboard *instance_artboard = nullptr;
    Ref<RiveArtboardReference> artboard_reference;
    rive::BindableArtboard *reference_asset = nullptr;
    rive::rcp<rive::File> reference_file_ref;
    Ref<Resource> reference_file_resource;
    String reference_artboard_name;
    Ref<RiveViewModelInstance> bound_instance;
protected:
    static void _bind_methods();
public:
    ~RiveViewModelArtboard() override;
    void _init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceArtboard *p_artboard);
    bool set_value(const String &p_name);
    bool set_reference(const Ref<RiveArtboardReference> &p_reference);
    Ref<RiveArtboardReference> get_reference() const;
    bool bind_view_model(const Ref<RiveViewModelInstance> &p_instance);
    Ref<RiveViewModelInstance> get_view_model() const;
    String get_value() const;
    PackedStringArray get_artboard_list() const;
    String get_property_name() const override;
};

class RiveViewModelList : public RiveViewModelProperty {
    GDCLASS(RiveViewModelList, RiveViewModelProperty);
    rive::ViewModelInstanceList *instance_list = nullptr;
    mutable HashMap<int, Ref<RiveViewModelInstance>> item_cache;
protected:
    static void _bind_methods();
public:
    void _init(rive::rcp<rive::ViewModelInstance> p_owner, rive::ViewModelInstanceList *p_list);
    int get_size() const;
    Ref<RiveViewModelInstance> get_item(int p_index) const;
    Array get_items() const;
    bool set_items(const Array &p_items);
    bool append(const Ref<RiveViewModelInstance> &p_item);
    bool insert(int p_index, const Ref<RiveViewModelInstance> &p_item);
    bool remove_at(int p_index);
    bool swap(int p_a, int p_b);
    void clear();
    bool flush_changes() override;
    String get_property_name() const override;
};

class RiveViewModelBlob : public RiveViewModelProperty {
    GDCLASS(RiveViewModelBlob, RiveViewModelProperty);
    rive::ViewModelInstanceAssetBlob *instance_blob = nullptr;
protected:
    static void _bind_methods();
public:
    void _init(rive::rcp<rive::ViewModelInstance> owner, rive::ViewModelInstanceAssetBlob *value);
    bool set_value(const PackedByteArray &bytes);
    PackedByteArray get_value() const;
    void clear();
    String get_property_name() const override;
};

class RiveViewModelListIndex : public RiveViewModelProperty {
    GDCLASS(RiveViewModelListIndex, RiveViewModelProperty);
    rive::ViewModelInstanceSymbolListIndex *instance_index = nullptr;
protected:
    static void _bind_methods();
public:
    void _init(rive::rcp<rive::ViewModelInstance> owner, rive::ViewModelInstanceSymbolListIndex *value);
    int64_t get_value() const;
    String get_property_name() const override;
};

class RiveViewModelInstance : public RefCounted {
    GDCLASS(RiveViewModelInstance, RefCounted);

private:
    rive::rcp<rive::File> file_ref;
    rive::rcp<rive::ViewModelInstance> instance;
    mutable HashMap<String, Ref<RiveViewModelProperty>> property_cache;
    mutable HashMap<String, Ref<RiveViewModelInstance>> child_vm_cache;
    HashMap<String, uint64_t> flushed_revisions;
    HashMap<String, uint64_t> flushed_triggers;

protected:
    static void _bind_methods();

public:
    ~RiveViewModelInstance();
    void _init(rive::rcp<rive::ViewModelInstance> p_instance, rive::rcp<rive::File> p_file = nullptr);
    rive::rcp<rive::ViewModelInstance> _get_native_instance() const { return instance; }
    rive::rcp<rive::File> _get_native_file() const { return file_ref; }

    bool has_property(const String &p_path) const;
    bool set_property_value(const String &p_path, const Variant &p_value);
    Variant get_property_value(const String &p_path) const;
    bool flush_changes();
    String get_name() const;
    String get_view_model_name() const;
    Array get_properties() const;
    Dictionary get_property_info(const String &p_path) const;
    Ref<RiveViewModelInstance> get_view_model_property(const String &p_path);
    bool replace_view_model(const String &p_path, const Ref<RiveViewModelInstance> &p_value);
    static String canonicalize_property_path(const String &p_path);
    
    Ref<RiveViewModelNumber> get_number_property(const String& p_path);
    Ref<RiveViewModelString> get_string_property(const String& p_path);
    Ref<RiveViewModelBoolean> get_boolean_property(const String& p_path);
    Ref<RiveViewModelColor> get_color_property(const String& p_path);
    Ref<RiveViewModelEnum> get_enum_property(const String& p_path);
    Ref<RiveViewModelTrigger> get_trigger_property(const String& p_path);
    Ref<RiveViewModelImage> get_image_property(const String& p_path);
    Ref<RiveViewModelFont> get_font_property(const String& p_path);
    Ref<RiveViewModelArtboard> get_artboard_property(const String& p_path);
    Ref<RiveViewModelList> get_list_property(const String& p_path);
    Ref<RiveViewModelBlob> get_blob_property(const String &p_path);
    Ref<RiveViewModelListIndex> get_list_index_property(const String &p_path);

    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(List<PropertyInfo> *p_list) const;
};

#endif
