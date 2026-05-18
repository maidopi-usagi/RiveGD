#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/engine.hpp>

#include "renderer/rive_renderer.h"
#include "rive_svg.h"
#include "resources/rive_file.h"
#include "resources/rive_types.h"
#include "scene/rive_node.h"
#include "scene/rive_file_instance.h"
#include "scene/rive_multi_instance.h"
#include "scene/rive_control.h"
#include "scene/rive_canvas_2d.h"
#include "scene/rive_raw.h"
#include "scene/rive_player.h"
#include "scene/rive_view_model.h"
#include "renderer/rive_texture_target.h"
#include "editor/rive_editor_plugin.h"
#include "editor/rive_view_model_inspector.h"
#include "editor/rive_file_instance_editor_plugin.h"
#include <godot_cpp/classes/editor_plugin_registration.hpp>

using namespace godot;

void initialize_rive_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        ClassDB::register_class<RiveControl>();
        ClassDB::register_class<RivePath>();
        ClassDB::register_class<RivePaint>();
        ClassDB::register_class<RiveGradient>();
        ClassDB::register_class<RiveImage>();
        ClassDB::register_class<RiveRendererWrapper>();
        ClassDB::register_class<RiveFont>();
        ClassDB::register_class<RiveText>();
        ClassDB::register_class<RiveSVG>();
        
        ClassDB::register_class<RiveFile>();
        ClassDB::register_class<RiveNode>();
        ClassDB::register_class<RiveFileInstance>();
        ClassDB::register_class<RiveMultiInstance>();
        ClassDB::register_class<RiveRaw>();
        ClassDB::register_class<RiveCanvas2D>();
        
        ClassDB::register_class<RivePlayer>();
        ClassDB::register_class<RiveTextureTarget>();
        
        ClassDB::register_abstract_class<RiveViewModelProperty>();
        ClassDB::register_class<RiveViewModelNumber>();
        ClassDB::register_class<RiveViewModelString>();
        ClassDB::register_class<RiveViewModelBoolean>();
        ClassDB::register_class<RiveViewModelColor>();
        ClassDB::register_class<RiveViewModelEnum>();
        ClassDB::register_class<RiveViewModelTrigger>();
        ClassDB::register_class<RiveViewModelImage>();
        ClassDB::register_class<RiveViewModelInstance>();

        // Initialize renderer
    }
    
    if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
        ClassDB::register_class<RiveImportPlugin>();
        ClassDB::register_class<RiveViewModelInspector>();
        ClassDB::register_class<RiveInspectorPlugin>();
        ClassDB::register_class<RiveEditorPlugin>();
        ClassDB::register_class<RiveFileInstanceEditor>();
        ClassDB::register_class<RiveFileInstanceEditorPlugin>();
        EditorPlugins::add_by_type<RiveEditorPlugin>();
        EditorPlugins::add_by_type<RiveFileInstanceEditorPlugin>();
    }
}

void initialize_rive() {
	rive_integration::initialize_rive_renderer();
}

void uninitialize_rive_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        rive_integration::cleanup_rive_renderer();
    }
}

extern "C" {
    GDExtensionBool GDE_EXPORT gdextension_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
        godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

        init_obj.register_initializer(initialize_rive_module);
		init_obj.register_startup_callback(initialize_rive);
        init_obj.register_terminator(uninitialize_rive_module);
        init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

        return init_obj.init();
    }

    // Alias entry symbol for backward compatibility with .gdextension files
    // that use "rive_godot_library_init" as the entry_symbol
    GDExtensionBool GDE_EXPORT rive_godot_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
        return gdextension_init(p_get_proc_address, p_library, r_initialization);
    }
}
