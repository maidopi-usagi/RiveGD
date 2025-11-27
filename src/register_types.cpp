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
#include "renderer/rive_texture_target.h"
#include "editor/rive_editor_plugin.h"
#include <godot_cpp/classes/editor_plugin_registration.hpp>

using namespace godot;

void initialize_rive_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        ClassDB::register_class<RiveControl>();
        ClassDB::register_class<RivePath>();
        ClassDB::register_class<RivePaint>();
        ClassDB::register_class<RiveRendererWrapper>();
        ClassDB::register_class<RiveSVG>();
        
        ClassDB::register_class<RiveFile>();
        ClassDB::register_class<RiveNode>();
        ClassDB::register_class<RiveFileInstance>();
        ClassDB::register_class<RiveMultiInstance>();
        ClassDB::register_class<RiveRaw>();
        ClassDB::register_class<RiveCanvas2D>();
        
        ClassDB::register_class<RivePlayer>();
        ClassDB::register_class<RiveTextureTarget>();
        
        // Initialize renderer
    }
    
    if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
        ClassDB::register_class<RiveImportPlugin>();
        ClassDB::register_class<RiveEditorPlugin>();
        EditorPlugins::add_by_type<RiveEditorPlugin>();
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
}
