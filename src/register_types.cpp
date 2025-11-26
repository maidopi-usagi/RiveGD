#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/engine.hpp>

#include "rive_viewer.h"
#include "rive_renderer.h"
#include "rive_canvas.h"
#include "rive_svg.h"

using namespace godot;

void initialize_rive_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        ClassDB::register_class<RiveViewer>();
        ClassDB::register_class<RivePath>();
        ClassDB::register_class<RivePaint>();
        ClassDB::register_class<RiveRendererWrapper>();
        ClassDB::register_class<RiveCanvas>();
        ClassDB::register_class<RiveSVG>();
        
        // Initialize renderer
    }
}

void initialize_rive() {
	rive_integration::initialize_rive_renderer();
}

void uninitialize_rive_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        // Cleanup if needed
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
