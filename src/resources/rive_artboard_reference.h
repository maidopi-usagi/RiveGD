#ifndef RIVE_ARTBOARD_REFERENCE_H
#define RIVE_ARTBOARD_REFERENCE_H

#include "rive_file.h"

class RiveArtboardReference : public Resource {
    GDCLASS(RiveArtboardReference, Resource);
    Ref<RiveFile> file;
    String artboard_name;
protected:
    static void _bind_methods();
public:
    void set_file(const Ref<RiveFile> &p_file);
    Ref<RiveFile> get_file() const { return file; }
    void set_artboard_name(const String &p_name);
    String get_artboard_name() const { return artboard_name; }
};

#endif
