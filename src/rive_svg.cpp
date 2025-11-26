#include "rive_svg.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>

#define NANOSVG_IMPLEMENTATION
#include "../third-party/nanosvg.h"

#include "rive/artboard.hpp"
#include "rive/drawable.hpp"
#include "rive/renderer.hpp"
#include "rive/factory.hpp"
#include "rive/importers/artboard_importer.hpp"

// Custom Drawable to render SVG shapes
class RiveSVGDrawable : public rive::Drawable {
public:
    // Use raw pointer to avoid Ref counting issues. 
    // Data is owned by RiveSVG which owns the Artboard containing this Drawable.
    RiveSVG::Data* data = nullptr;

    RiveSVGDrawable() {}

    ~RiveSVGDrawable() {}
    
    uint16_t coreType() const override { return rive::Drawable::typeKey; }
    bool isTypeOf(uint16_t typeKey) const override {
        return typeKey == rive::Drawable::typeKey || rive::Drawable::isTypeOf(typeKey);
    }

    rive::Core* clone() const override {
        auto newObj = new RiveSVGDrawable();
        newObj->copy(*this);
        return newObj;
    }
    
    void copy(const rive::Core& source) {
        rive::Drawable::copy(static_cast<const rive::DrawableBase&>(source));
        // Note: is<RiveSVGDrawable> checks Drawable::typeKey, so it matches any Drawable.
        // But we know we are cloning RiveSVGDrawable.
        if (source.is<RiveSVGDrawable>()) {
            auto src = static_cast<const RiveSVGDrawable&>(source);
            data = src.data;
        }
    }

    void draw(rive::Renderer* renderer) override {
        if (!data) return;
        
        for (int i = 0; i < data->shapes.size(); i++) {
            const RiveSVG::Shape& s = data->shapes[i];
            renderer->save();
            
            rive::Mat2D m = s.transform;
            renderer->transform(m);
            
            RiveRenderRegistry* registry = RiveRenderRegistry::get_singleton();
            if (registry) {
                rive::Factory* factory = registry->get_factory();
                if (factory) {
                    if (s.path.is_valid() && s.paint.is_valid()) {
                         renderer->drawPath(s.path->get_render_path(factory), s.paint->get_render_paint(factory));
                    }
                }
            }
            
            renderer->restore();
        }
    }
    
    rive::Core* hitTest(rive::HitInfo*, const rive::Mat2D&) override { return nullptr; }

    rive::StatusCode onAddedDirty(rive::CoreContext* context) override {
        // godot::UtilityFunctions::print("RiveSVGDrawable::onAddedDirty");
        return rive::Drawable::onAddedDirty(context);
    }

    rive::StatusCode onAddedClean(rive::CoreContext* context) override {
        // godot::UtilityFunctions::print("RiveSVGDrawable::onAddedClean");
        return rive::Drawable::onAddedClean(context);
    }
};

RiveSVG::RiveSVG() {
    data.instantiate();
}

RiveSVG::~RiveSVG() {
    if (artboard_instance) {
        artboard_instance.reset();
    }
    if (source_artboard) {
        source_artboard.reset();
    }
    base_svg.unref();
}

void RiveSVG::_bind_methods() {
    ClassDB::bind_method(D_METHOD("parse", "xml_content"), &RiveSVG::parse);
    ClassDB::bind_method(D_METHOD("load_file", "path"), &RiveSVG::load_file);
    ClassDB::bind_method(D_METHOD("draw", "renderer"), &RiveSVG::draw);
    ClassDB::bind_method(D_METHOD("instance"), &RiveSVG::instance);
}

void RiveSVG::load_file(String path) {
    Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
    if (f.is_valid()) {
        parse(f->get_as_text());
    } else {
        UtilityFunctions::printerr("Failed to open SVG file: " + path);
    }
}

void RiveSVG::parse(String xml_content) {
    data->shapes.clear();
    
    // nanosvg expects a char* string and modifies it.
    CharString cs = xml_content.utf8();
    std::vector<char> input_data(cs.length() + 1);
    memcpy(input_data.data(), cs.get_data(), cs.length());
    input_data[cs.length()] = '\0';
    
    // Parse with 96 DPI (standard)
    NSVGimage* image = nsvgParse(input_data.data(), "px", 96.0f);
    if (!image) {
        UtilityFunctions::printerr("Failed to parse SVG content");
        return;
    }
    
    for (NSVGshape* shape = image->shapes; shape != NULL; shape = shape->next) {
        if (!(shape->flags & NSVG_FLAGS_VISIBLE)) continue;
        
        Ref<RivePath> path;
        path.instantiate();
        path->set_fill_rule(shape->fillRule);
        
        for (NSVGpath* svg_path = shape->paths; svg_path != NULL; svg_path = svg_path->next) {
            if (svg_path->npts < 1) continue;
            
            path->move_to(svg_path->pts[0], svg_path->pts[1]);
            
            for (int i = 0; i < svg_path->npts - 1; i += 3) {
                float* p = &svg_path->pts[i * 2];
                path->cubic_to(p[2], p[3], p[4], p[5], p[6], p[7]);
            }
            
            if (svg_path->closed) {
                path->close();
            }
        }
        
        // Fill
        bool has_fill = false;
        unsigned int fill_color = 0;

        if (shape->fill.type == NSVG_PAINT_COLOR) {
            has_fill = true;
            fill_color = shape->fill.color;
        } else if ((shape->fill.type == NSVG_PAINT_LINEAR_GRADIENT || shape->fill.type == NSVG_PAINT_RADIAL_GRADIENT) && shape->fill.gradient && shape->fill.gradient->nstops > 0) {
            has_fill = true;
            fill_color = shape->fill.gradient->stops[0].color;
            UtilityFunctions::print("RiveSVG: Gradient fill detected, falling back to first stop color.");
        }

        if (has_fill) {
            Ref<RivePaint> paint;
            paint.instantiate();
            paint->set_style(1); // Fill (Rive: 1=Fill, 0=Stroke)
            
            unsigned int c = fill_color;
            float r = (float)(c & 0xFF) / 255.0f;
            float g = (float)((c >> 8) & 0xFF) / 255.0f;
            float b = (float)((c >> 16) & 0xFF) / 255.0f;
            float a = ((float)((c >> 24) & 0xFF) / 255.0f) * shape->opacity;
            
            paint->set_color(Color(r, g, b, a));
            
            float cx = (shape->bounds[0] + shape->bounds[2]) * 0.5f;
            float cy = (shape->bounds[1] + shape->bounds[3]) * 0.5f;
            
            data->shapes.push_back({path, paint, rive::Mat2D(), cx, cy});
        }
        
        // Stroke
        bool has_stroke = false;
        unsigned int stroke_color = 0;

        if (shape->stroke.type == NSVG_PAINT_COLOR) {
            has_stroke = true;
            stroke_color = shape->stroke.color;
        } else if ((shape->stroke.type == NSVG_PAINT_LINEAR_GRADIENT || shape->stroke.type == NSVG_PAINT_RADIAL_GRADIENT) && shape->stroke.gradient && shape->stroke.gradient->nstops > 0) {
            has_stroke = true;
            stroke_color = shape->stroke.gradient->stops[0].color;
            UtilityFunctions::print("RiveSVG: Gradient stroke detected, falling back to first stop color.");
        }

        if (has_stroke) {
            Ref<RivePaint> paint;
            paint.instantiate();
            paint->set_style(0); // Stroke (Rive: 1=Fill, 0=Stroke)
            
            unsigned int c = stroke_color;
            float r = (float)(c & 0xFF) / 255.0f;
            float g = (float)((c >> 8) & 0xFF) / 255.0f;
            float b = (float)((c >> 16) & 0xFF) / 255.0f;
            float a = ((float)((c >> 24) & 0xFF) / 255.0f) * shape->opacity;
            
            paint->set_color(Color(r, g, b, a));
            paint->set_thickness(shape->strokeWidth);
            
            int cap = 0; // Butt
            if (shape->strokeLineCap == NSVG_CAP_ROUND) cap = 1;
            else if (shape->strokeLineCap == NSVG_CAP_SQUARE) cap = 2;
            paint->set_cap(cap);
            
            int join = 0; // Miter
            if (shape->strokeLineJoin == NSVG_JOIN_ROUND) join = 1;
            else if (shape->strokeLineJoin == NSVG_JOIN_BEVEL) join = 2;
            paint->set_join(join);
            
            float cx = (shape->bounds[0] + shape->bounds[2]) * 0.5f;
            float cy = (shape->bounds[1] + shape->bounds[3]) * 0.5f;
            
            data->shapes.push_back({path, paint, rive::Mat2D(), cx, cy});
        }
    }
    
    // Create Artboard
    source_artboard = std::make_unique<rive::Artboard>();
    // Artboard is the root, so it shouldn't have a parent. 
    // Default parentId is 0, which resolves to itself (index 0), causing infinite loops in hierarchy traversal.
    source_artboard->parentId(source_artboard->emptyId);
    
    auto drawable = new RiveSVGDrawable();
    drawable->data = data.ptr();
    
    // Use ArtboardImporter to add object to private list
    rive::ArtboardImporter importer(source_artboard.get());
    // IMPORTANT: Artboard must be the first object in m_Objects (index 0)
    importer.addComponent(source_artboard.get());
    importer.addComponent(drawable);
    
    source_artboard->initialize();
    // Disable clip to avoid using m_Factory which is null
    source_artboard->clip(false);

    artboard_instance = source_artboard->instance();

    if (artboard_instance) {
        artboard_instance->advance(0.0f);
    }
    
    if (image) {
        nsvgDelete(image);
    }
}

void RiveSVG::draw(Ref<RiveRendererWrapper> renderer) {
    if (renderer.is_null()) return;
    if (!artboard_instance) return;
    
    if (renderer->renderer) {
        renderer->save();
        artboard_instance->draw(renderer->renderer);
        renderer->restore();
    }
}

Ref<RiveSVG> RiveSVG::instance() {
    // UtilityFunctions::print("RiveSVG::instance called");
    Ref<RiveSVG> new_svg;
    new_svg.instantiate();
    
    if (source_artboard) {
        new_svg->base_svg = Ref<RiveSVG>(this);
        new_svg->artboard_instance = source_artboard->instance();
        if (new_svg->artboard_instance) {
            new_svg->artboard_instance->advance(0.0f);
        }
    } else if (base_svg.is_valid() && base_svg->source_artboard) {
        new_svg->base_svg = base_svg;
        new_svg->artboard_instance = base_svg->source_artboard->instance();
        if (new_svg->artboard_instance) {
            new_svg->artboard_instance->advance(0.0f);
        }
    }
    
    return new_svg;
}