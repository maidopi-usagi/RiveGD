#ifndef RIVE_EXPORT_PLUGIN_H
#define RIVE_EXPORT_PLUGIN_H
#include <godot_cpp/classes/editor_export_plugin.hpp>
#include <godot_cpp/classes/editor_export_platform.hpp>
#include <godot_cpp/templates/hash_set.hpp>
using namespace godot;

class RiveExportPlugin : public EditorExportPlugin {
    GDCLASS(RiveExportPlugin, EditorExportPlugin);
    HashSet<String> included;
    HashSet<uint64_t> inspected;
    void inspect_value(const Variant &value, int depth);
protected:
    static void _bind_methods() {}
public:
    String _get_name() const override { return "Rive external asset dependencies"; }
    void _export_file(const String &path, const String &type, const PackedStringArray &features) override;
    void _export_begin(const PackedStringArray &, bool, const String &, uint32_t) override { included.clear(); inspected.clear(); }
};
#endif
