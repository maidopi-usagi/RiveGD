#include "rive_view_model.h"
#include "../renderer/rive_renderer.h"
#include "../resources/rive_types.h"
#include "../resources/rive_artboard_reference.h"

#include <godot_cpp/core/class_db.hpp>
#include <rive/file.hpp>
#include <rive/text/font_hb.hpp>
#include <rive/viewmodel/viewmodel_instance_artboard.hpp>
#include <rive/viewmodel/viewmodel_instance_asset_font.hpp>
#include <rive/viewmodel/viewmodel_instance_list.hpp>
#include <rive/viewmodel/viewmodel_instance_list_item.hpp>
#include <rive/viewmodel/viewmodel_instance_viewmodel.hpp>
#include <unordered_set>
#include <vector>

namespace {

bool is_acyclic_view_model_graph(rive::ViewModelInstance *p_instance,
        rive::ViewModelInstance *p_root, rive::ViewModelInstance *p_parent,
        std::unordered_set<rive::ViewModelInstance *> &p_active,
        std::unordered_set<rive::ViewModelInstance *> &p_complete, int p_depth) {
    if (!p_instance) return true;
    if (p_instance == p_root || p_instance == p_parent || p_depth > 32) return false;
    if (p_active.count(p_instance)) return false;
    if (p_complete.count(p_instance)) return true;
    p_active.insert(p_instance);
    for (const auto &entry : p_instance->propertyValues()) {
        rive::ViewModelInstanceValue *value = entry.get();
        if (!value) continue;
        if (value->is<rive::ViewModelInstanceViewModel>()) {
            auto child = value->as<rive::ViewModelInstanceViewModel>()->referenceViewModelInstance();
            if (!is_acyclic_view_model_graph(child.get(), p_root, p_parent, p_active, p_complete, p_depth + 1)) return false;
        } else if (value->is<rive::ViewModelInstanceList>()) {
            for (const auto &item : value->as<rive::ViewModelInstanceList>()->listItems()) {
                if (item && !is_acyclic_view_model_graph(item->viewModelInstance().get(), p_root, p_parent,
                        p_active, p_complete, p_depth + 1)) return false;
            }
        } else if (value->is<rive::ViewModelInstanceArtboard>()) {
            auto child = value->as<rive::ViewModelInstanceArtboard>()->boundViewModelInstance();
            if (!is_acyclic_view_model_graph(child.get(), p_root, p_parent, p_active, p_complete, p_depth + 1)) return false;
        }
    }
    p_active.erase(p_instance);
    p_complete.insert(p_instance);
    return true;
}

bool is_compatible_list_item(const Ref<RiveViewModelInstance> &p_item,
        rive::ViewModelInstance *p_root, rive::ViewModelInstanceList *p_list,
        const rive::rcp<rive::File> &p_file) {
    if (p_item.is_null() || !p_item->_get_native_instance()) return false;
    if (!p_file || p_item->_get_native_file().get() != p_file.get()) return false;
    std::unordered_set<rive::ViewModelInstance *> active;
    std::unordered_set<rive::ViewModelInstance *> complete;
    return is_acyclic_view_model_graph(p_item->_get_native_instance().get(), p_root,
            p_list->parentViewModelInstance(), active, complete, 0);
}

}

bool rive_is_safe_view_model_reference(rive::ViewModelInstance *p_candidate,
        rive::ViewModelInstance *p_root, rive::ViewModelInstance *p_parent) {
    std::unordered_set<rive::ViewModelInstance *> active;
    std::unordered_set<rive::ViewModelInstance *> complete;
    return is_acyclic_view_model_graph(p_candidate, p_root, p_parent, active, complete, 0);
}

void RiveViewModelFont::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_value", "font"), &RiveViewModelFont::set_value);
    ClassDB::bind_method(D_METHOD("get_value"), &RiveViewModelFont::get_value);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "value", PROPERTY_HINT_RESOURCE_TYPE, "FontFile,RiveFont"), "set_value", "get_value");
    ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::OBJECT, "font")));
}

void RiveViewModelFont::_init(rive::rcp<rive::ViewModelInstance> p_owner,
        rive::ViewModelInstanceAssetFont *p_font) {
    instance_ref = std::move(p_owner);
    instance_font = p_font;
    native_value = p_font;
    _observe();
}

bool RiveViewModelFont::set_value(const Ref<RefCounted> &p_font) {
    if (!instance_font) return false;
    rive::rcp<rive::Font> decoded;
    if (p_font.is_valid()) {
        if (RiveFont *font = Object::cast_to<RiveFont>(p_font.ptr())) {
            decoded = font->get_font();
        } else if (FontFile *font = Object::cast_to<FontFile>(p_font.ptr())) {
            const PackedByteArray bytes = font->get_data();
            if (!bytes.is_empty()) decoded = HBFont::Decode(rive::Span<const uint8_t>(bytes.ptr(), bytes.size()));
        } else {
            return false;
        }
        if (!decoded) return false;
    }
    const bool applied = rive_integration::run_on_rive_render_thread([&]() {
        instance_font->value(decoded.get());
        cached_font = decoded.get();
        font_value = p_font;
    });
    if (applied) flush_changes();
    return applied;
}

Ref<RefCounted> RiveViewModelFont::get_value() const {
    bool matches = false;
    if (instance_font) rive_integration::run_on_rive_render_thread([&]() {
        matches = instance_font->propertyValue() == UINT32_MAX && instance_font->asset()->font().get() == cached_font;
    });
    return matches ? font_value : Ref<RefCounted>();
}

String RiveViewModelFont::get_property_name() const {
    return instance_font ? String::utf8(instance_font->name().c_str()) : String();
}

void RiveViewModelArtboard::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_value", "artboard_name"), &RiveViewModelArtboard::set_value);
    ClassDB::bind_method(D_METHOD("get_value"), &RiveViewModelArtboard::get_value);
    ClassDB::bind_method(D_METHOD("get_artboard_list"), &RiveViewModelArtboard::get_artboard_list);
    ClassDB::bind_method(D_METHOD("set_reference", "reference"), &RiveViewModelArtboard::set_reference);
    ClassDB::bind_method(D_METHOD("get_reference"), &RiveViewModelArtboard::get_reference);
    ClassDB::bind_method(D_METHOD("bind_view_model", "instance"), &RiveViewModelArtboard::bind_view_model);
    ClassDB::bind_method(D_METHOD("get_view_model"), &RiveViewModelArtboard::get_view_model);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "value"), "set_value", "get_value");
    ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::STRING, "artboard_name")));
}

void RiveViewModelArtboard::_init(rive::rcp<rive::ViewModelInstance> p_owner,
        rive::ViewModelInstanceArtboard *p_artboard) {
    instance_ref = std::move(p_owner);
    instance_artboard = p_artboard;
    native_value = p_artboard;
    _observe();
}

RiveViewModelArtboard::~RiveViewModelArtboard() {
    auto release_source = [&]() { reference_file_ref.reset(); };
    if (!rive_integration::run_on_rive_render_thread(release_source)) release_source();
}

bool RiveViewModelArtboard::set_value(const String &p_name) {
    if (!instance_artboard || !file_ref) return false;
    bool valid = false;
    const bool executed = rive_integration::run_on_rive_render_thread([&]() {
        rive::rcp<rive::BindableArtboard> artboard;
        if (!p_name.is_empty()) {
            artboard = file_ref->bindableArtboardNamed(p_name.utf8().get_data());
            if (!artboard) return;
        }
        instance_artboard->boundViewModelInstance(nullptr);
        instance_artboard->asset(std::move(artboard));
        reference_file_ref.reset();
        valid = true;
    });
    if (executed && valid) {
        artboard_reference.unref(); reference_file_resource.unref(); reference_artboard_name = String();
        reference_asset = nullptr; bound_instance.unref(); flush_changes();
    }
    return executed && valid;
}

bool RiveViewModelArtboard::set_reference(const Ref<RiveArtboardReference> &p_reference) {
    if (p_reference.is_null()) return set_value(String());
    const Ref<RiveFile> source = p_reference->get_file();
    if (!instance_artboard || source.is_null() || source->load_rive_file() != OK) return false;
    auto *file = source->get_rive_file();
    if (!file) return false;
    bool valid = false;
    rive_integration::run_on_rive_render_thread([&]() {
        const String name = p_reference->get_artboard_name();
        auto asset = name.is_empty() ? file->bindableArtboardDefault() : file->bindableArtboardNamed(name.utf8().get_data());
        if (!asset) return;
        reference_asset = asset.get();
        instance_artboard->boundViewModelInstance(nullptr);
        instance_artboard->asset(std::move(asset));
        reference_file_ref = rive::ref_rcp(file);
        valid = true;
    });
    if (valid) {
        artboard_reference = p_reference; reference_file_resource = source;
        reference_artboard_name = p_reference->get_artboard_name();
        bound_instance.unref(); flush_changes();
    }
    return valid;
}

Ref<RiveArtboardReference> RiveViewModelArtboard::get_reference() const {
    if (artboard_reference.is_null() || artboard_reference->get_file() != reference_file_resource ||
            artboard_reference->get_artboard_name() != reference_artboard_name) return nullptr;
    const auto *source_resource = Object::cast_to<RiveFile>(reference_file_resource.ptr());
    if (!source_resource || source_resource->_get_cached_rive_file() != reference_file_ref.get()) return nullptr;
    bool matches = false;
    if (instance_artboard) rive_integration::run_on_rive_render_thread([&]() { matches = instance_artboard->asset().get() == reference_asset; });
    return matches ? artboard_reference : Ref<RiveArtboardReference>();
}

bool RiveViewModelArtboard::bind_view_model(const Ref<RiveViewModelInstance> &p_instance) {
    if (!instance_artboard) return false;
    if (p_instance.is_null()) {
        rive_integration::run_on_rive_render_thread([&]() {
            instance_artboard->boundViewModelInstance(nullptr);
            auto asset = instance_artboard->asset();
            if (asset) instance_artboard->asset(std::move(asset));
        });
        bound_instance.unref();
        flush_changes();
        return true;
    }
    auto native = p_instance->_get_native_instance();
    if (!native) return false;
    auto source_file = file_ref;
    rive_integration::run_on_rive_render_thread([&]() {
        if (instance_artboard->asset().get() == reference_asset && reference_file_ref) source_file = reference_file_ref;
    });
    if (!source_file || source_file != p_instance->_get_native_file()) return false;
    bool valid = false;
    rive_integration::run_on_rive_render_thread([&]() {
        auto asset = instance_artboard->asset();
        auto *artboard = asset ? asset->artboard() : source_file->artboard(static_cast<size_t>(instance_artboard->propertyValue()));
        if (!artboard || static_cast<uint32_t>(artboard->viewModelId()) != native->viewModelId() ||
                !rive_is_safe_view_model_reference(native.get(), instance_ref.get(), instance_ref.get())) return;
        if (!asset) asset = source_file->bindableArtboardNamed(artboard->name());
        if (!asset) return;
        instance_artboard->asset(std::move(asset));
        instance_artboard->boundViewModelInstance(native);
        valid = true;
    });
    if (valid) { bound_instance = p_instance; flush_changes(); }
    return valid;
}

Ref<RiveViewModelInstance> RiveViewModelArtboard::get_view_model() const {
    rive::rcp<rive::ViewModelInstance> native;
    if (instance_artboard) rive_integration::run_on_rive_render_thread([&]() { native = instance_artboard->boundViewModelInstance(); });
    if (!native) return nullptr;
    if (bound_instance.is_valid() && bound_instance->_get_native_instance() == native) return bound_instance;
    auto source_file = file_ref;
    rive_integration::run_on_rive_render_thread([&]() {
        if (instance_artboard->asset().get() == reference_asset && reference_file_ref) source_file = reference_file_ref;
    });
    Ref<RiveViewModelInstance> wrapper;
    wrapper.instantiate();
    wrapper->_init(native, source_file);
    return wrapper;
}

String RiveViewModelArtboard::get_value() const {
    String result;
    if (!instance_artboard || !file_ref) return result;
    rive_integration::run_on_rive_render_thread([&]() {
        auto asset = instance_artboard->asset();
        if (asset && asset->artboard()) {
            result = String::utf8(asset->artboard()->name().c_str());
        } else if (instance_artboard->propertyValue() < file_ref->artboardCount()) {
            result = String::utf8(file_ref->artboardNameAt(instance_artboard->propertyValue()).c_str());
        }
    });
    return result;
}

PackedStringArray RiveViewModelArtboard::get_artboard_list() const {
    PackedStringArray names;
    if (!file_ref) return names;
    for (size_t i = 0; i < file_ref->artboardCount(); ++i) {
        names.push_back(String::utf8(file_ref->artboardNameAt(i).c_str()));
    }
    return names;
}

String RiveViewModelArtboard::get_property_name() const {
    return instance_artboard ? String::utf8(instance_artboard->name().c_str()) : String();
}

void RiveViewModelList::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_size"), &RiveViewModelList::get_size);
    ClassDB::bind_method(D_METHOD("get_item", "index"), &RiveViewModelList::get_item);
    ClassDB::bind_method(D_METHOD("get_items"), &RiveViewModelList::get_items);
    ClassDB::bind_method(D_METHOD("set_items", "items"), &RiveViewModelList::set_items);
    ClassDB::bind_method(D_METHOD("append", "item"), &RiveViewModelList::append);
    ClassDB::bind_method(D_METHOD("insert", "index", "item"), &RiveViewModelList::insert);
    ClassDB::bind_method(D_METHOD("remove_at", "index"), &RiveViewModelList::remove_at);
    ClassDB::bind_method(D_METHOD("swap", "a", "b"), &RiveViewModelList::swap);
    ClassDB::bind_method(D_METHOD("clear"), &RiveViewModelList::clear);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "items"), "set_items", "get_items");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "size"), "", "get_size");
    ADD_SIGNAL(MethodInfo("list_changed"));
}

void RiveViewModelList::_init(rive::rcp<rive::ViewModelInstance> p_owner,
        rive::ViewModelInstanceList *p_list) {
    instance_ref = std::move(p_owner);
    instance_list = p_list;
    native_value = p_list;
    _observe();
}

int RiveViewModelList::get_size() const {
    int size = 0;
    if (instance_list) {
        rive_integration::run_on_rive_render_thread([&]() {
            size = static_cast<int>(instance_list->listItems().size());
        });
    }
    return size;
}

Ref<RiveViewModelInstance> RiveViewModelList::get_item(int p_index) const {
    if (!instance_list || p_index < 0) return nullptr;
    rive::rcp<rive::ViewModelInstance> instance;
    const bool executed = rive_integration::run_on_rive_render_thread([&]() {
        if (static_cast<size_t>(p_index) >= instance_list->listItems().size()) return;
        auto item = instance_list->item(static_cast<uint32_t>(p_index));
        if (item) instance = item->viewModelInstance();
    });
    if (!executed || !instance) return nullptr;
    if (auto *cached = item_cache.getptr(p_index)) {
        if ((*cached)->_get_native_instance() == instance) return *cached;
    }
    Ref<RiveViewModelInstance> wrapper;
    wrapper.instantiate();
    wrapper->_init(std::move(instance), file_ref);
    item_cache[p_index] = wrapper;
    return wrapper;
}

Array RiveViewModelList::get_items() const {
    Array result;
    if (!instance_list) return result;
    std::vector<rive::rcp<rive::ViewModelInstance>> instances;
    const bool executed = rive_integration::run_on_rive_render_thread([&]() {
        for (const auto &item : instance_list->listItems()) {
            instances.push_back(item ? item->viewModelInstance() : nullptr);
        }
    });
    if (!executed) return result;
    int index = 0;
    for (auto &instance : instances) {
        if (!instance) {
            result.push_back(Variant());
            ++index;
            continue;
        }
        Ref<RiveViewModelInstance> wrapper = get_item(index++);
        result.push_back(wrapper);
    }
    return result;
}

bool RiveViewModelList::set_items(const Array &p_items) {
    if (!instance_list) return false;
    std::vector<Ref<RiveViewModelInstance>> wrappers;
    wrappers.reserve(p_items.size());
    for (int i = 0; i < p_items.size(); ++i) {
        if (p_items[i].get_type() != Variant::OBJECT) return false;
        Ref<RiveViewModelInstance> item = p_items[i];
        if (item.is_null()) return false;
        wrappers.push_back(item);
    }
    bool valid = false;
    const bool executed = rive_integration::run_on_rive_render_thread([&]() {
        std::vector<rive::rcp<rive::ViewModelInstanceListItem>> items;
        items.reserve(wrappers.size());
        for (const auto &wrapper : wrappers) {
            if (!is_compatible_list_item(wrapper, instance_ref.get(), instance_list, file_ref)) return;
            auto item = rive::make_rcp<rive::ViewModelInstanceListItem>();
            item->viewModelInstance(wrapper->_get_native_instance());
            items.push_back(std::move(item));
        }
        instance_list->updateList(&items);
        valid = true;
    });
    if (executed && valid) flush_changes();
    return executed && valid;
}

bool RiveViewModelList::append(const Ref<RiveViewModelInstance> &p_item) {
    return insert(get_size(), p_item);
}

bool RiveViewModelList::insert(int p_index, const Ref<RiveViewModelInstance> &p_item) {
    if (!instance_list || p_index < 0) return false;
    bool inserted = false;
    const bool executed = rive_integration::run_on_rive_render_thread([&]() {
        if (static_cast<size_t>(p_index) > instance_list->listItems().size() ||
                !is_compatible_list_item(p_item, instance_ref.get(), instance_list, file_ref)) return;
        auto item = rive::make_rcp<rive::ViewModelInstanceListItem>();
        item->viewModelInstance(p_item->_get_native_instance());
        inserted = instance_list->addItemAt(std::move(item), p_index);
    });
    if (executed && inserted) flush_changes();
    return executed && inserted;
}

bool RiveViewModelList::remove_at(int p_index) {
    if (!instance_list || p_index < 0) return false;
    bool removed = false;
    const bool executed = rive_integration::run_on_rive_render_thread([&]() {
        if (static_cast<size_t>(p_index) >= instance_list->listItems().size()) return;
        auto item = instance_list->item(static_cast<uint32_t>(p_index));
        auto child = item ? item->viewModelInstance() : nullptr;
        instance_list->removeItem(p_index);
        if (child) {
            for (const auto &remaining : instance_list->listItems()) {
                if (remaining && remaining->viewModelInstance().get() == child.get()) {
                    child->addParent(instance_list->parentViewModelInstance());
                    break;
                }
            }
        }
        removed = true;
    });
    if (executed && removed) flush_changes();
    return executed && removed;
}

bool RiveViewModelList::swap(int p_a, int p_b) {
    if (!instance_list || p_a < 0 || p_b < 0) return false;
    bool swapped = false;
    const bool executed = rive_integration::run_on_rive_render_thread([&]() {
        const auto size = instance_list->listItems().size();
        if (static_cast<size_t>(p_a) >= size || static_cast<size_t>(p_b) >= size) return;
        instance_list->swap(static_cast<uint32_t>(p_a), static_cast<uint32_t>(p_b));
        swapped = true;
    });
    if (executed && swapped) flush_changes();
    return executed && swapped;
}

void RiveViewModelList::clear() {
    if (!instance_list) return;
    bool changed = false;
    const bool executed = rive_integration::run_on_rive_render_thread([&]() {
        changed = !instance_list->listItems().empty();
        instance_list->removeAllItems();
    });
    if (executed && changed) flush_changes();
}

bool RiveViewModelList::flush_changes() {
    bool changed = RiveViewModelProperty::flush_changes();
    std::vector<Ref<RiveViewModelInstance>> children;
    for (const auto &entry : item_cache) children.push_back(entry.value);
    for (const auto &child : children) if (child->flush_changes()) changed = true;
    return changed;
}

String RiveViewModelList::get_property_name() const {
    return instance_list ? String::utf8(instance_list->name().c_str()) : String();
}
