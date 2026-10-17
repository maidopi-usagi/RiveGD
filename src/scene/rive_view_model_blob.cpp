#include "rive_view_model.h"
#include "../renderer/rive_renderer.h"
#include <godot_cpp/core/class_db.hpp>
#include <rive/viewmodel/viewmodel_instance_asset_blob.hpp>
#include <rive/viewmodel/viewmodel_instance_symbol_list_index.hpp>
#include <rive/assets/blob_asset.hpp>
#include <cstring>

void RiveViewModelBlob::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_value", "bytes"), &RiveViewModelBlob::set_value);
    ClassDB::bind_method(D_METHOD("get_value"), &RiveViewModelBlob::get_value);
    ClassDB::bind_method(D_METHOD("clear"), &RiveViewModelBlob::clear);
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "value"), "set_value", "get_value");
    ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "bytes")));
}
void RiveViewModelBlob::_init(rive::rcp<rive::ViewModelInstance> owner, rive::ViewModelInstanceAssetBlob *value) {
    instance_ref = std::move(owner); instance_blob = value; native_value = value; _observe();
}
bool RiveViewModelBlob::set_value(const PackedByteArray &bytes) {
    if (!instance_blob || bytes.size() > 64 * 1024 * 1024) return false;
    const bool applied = rive_integration::run_on_rive_render_thread([&]() {
        auto asset = rive::make_rcp<rive::BlobAsset>();
        rive::SimpleArray<uint8_t> contents(bytes.ptr(), bytes.size());
        asset->decode(contents, nullptr);
        instance_blob->value(asset.get());
    });
    if (applied) flush_changes();
    return applied;
}
PackedByteArray RiveViewModelBlob::get_value() const {
    PackedByteArray result;
    rive_integration::run_on_rive_render_thread([&]() {
        auto asset = instance_blob ? instance_blob->asset() : nullptr;
        if (!asset) return;
        auto bytes = asset->bytes();
        result.resize(bytes.size());
        if (!bytes.empty()) memcpy(result.ptrw(), bytes.data(), bytes.size());
    });
    return result;
}
void RiveViewModelBlob::clear() {
    rive_integration::run_on_rive_render_thread([&]() { if (instance_blob) instance_blob->value(nullptr); });
    flush_changes();
}
String RiveViewModelBlob::get_property_name() const { return instance_blob ? String::utf8(instance_blob->name().c_str()) : String(); }

void RiveViewModelListIndex::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_value"), &RiveViewModelListIndex::get_value);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "get_value");
    ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::INT, "index")));
}
void RiveViewModelListIndex::_init(rive::rcp<rive::ViewModelInstance> owner, rive::ViewModelInstanceSymbolListIndex *value) {
    instance_ref = std::move(owner); instance_index = value; native_value = value; _observe();
}
int64_t RiveViewModelListIndex::get_value() const { return instance_index ? instance_index->propertyValue() : 0; }
String RiveViewModelListIndex::get_property_name() const { return instance_index ? String::utf8(instance_index->name().c_str()) : String(); }
