#ifndef RIVE_TEXTURE_TARGET_H
#define RIVE_TEXTURE_TARGET_H

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/texture2drd.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
#include <godot_cpp/classes/image.hpp>

using namespace godot;

class RiveTextureTarget : public RefCounted {
    GDCLASS(RiveTextureTarget, RefCounted);

private:
    RID texture_rid;
    Ref<Texture2DRD> texture_rd_ref;
    Size2i texture_size;

protected:
    static void _bind_methods() {}

public:
    RiveTextureTarget();
    ~RiveTextureTarget();

    // Returns true if texture was recreated
    bool resize(Size2i new_size);
    
    RID get_texture_rid() const { return texture_rid; }
    Ref<Texture2DRD> get_texture_rd() const { return texture_rd_ref; }
    Size2i get_size() const { return texture_size; }
    bool is_valid() const { return texture_rid.is_valid(); }

    void clear();
};

#endif // RIVE_TEXTURE_TARGET_H
