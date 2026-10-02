#include "rive_player.h"
#include "../renderer/rive_render_registry.h"
#include "../renderer/rive_renderer.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/core/class_db.hpp>

void RivePlayer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_rive_view_model_instance"), &RivePlayer::get_rive_view_model_instance);
}

RivePlayer::RivePlayer() {
}

RivePlayer::~RivePlayer() {
    set_artboard(nullptr);
}

bool RivePlayer::load_from_bytes(const PackedByteArray &data) {
    if (data.is_empty()) {
        ERR_PRINT("Cannot load an empty Rive file.");
        return false;
    }

    bool factory_ready = false;
    rive::ImportResult result{};
    rive::rcp<rive::File> file;
    const bool executed = rive_integration::run_on_rive_render_thread([&]() {
        rive::Factory *factory = RiveRenderRegistry::get_singleton()->get_factory();
        if (!factory) return;
        factory_ready = true;
        rive::Span<const uint8_t> bytes(data.ptr(), data.size());
        file = rive::File::import(bytes, factory, &result);
    });

    if (!executed || !factory_ready) {
        ERR_PRINT("Rive factory not available (context not created?)");
        return false;
    }

    if (!file) {
        ERR_PRINT(vformat("Failed to import Rive file (ImportResult=%d).", (int)result));
        return false;
    }

    return load(std::move(file));
}

bool RivePlayer::load(rive::rcp<rive::File> file) {
    if (file) {
        std::unique_ptr<rive::ArtboardInstance> ab = file->artboardDefault();
        if (ab) {
            set_artboard(std::move(ab), std::move(file));
            return true;
        }
    }

    // load_from_bytes() transfers its sole imported File reference here. On a
    // malformed file with no artboard, do not drop that final reference on the
    // scene thread.
    if (file) {
        auto release_file = [&]() { file.reset(); };
        if (!rive_integration::run_on_rive_render_thread(release_file)) {
            release_file();
        }
    }
    return false;
}

void RivePlayer::set_artboard(std::unique_ptr<rive::ArtboardInstance> p_artboard, rive::rcp<rive::File> p_file) {
    std::unique_ptr<rive::StateMachineInstance> old_state_machine = std::move(state_machine);
    std::unique_ptr<rive::LinearAnimationInstance> old_animation = std::move(animation);
    std::unique_ptr<rive::ArtboardInstance> old_artboard = std::move(artboard);
    rive::rcp<rive::ViewModelInstance> old_view_model = std::move(view_model_instance);
    rive::rcp<rive::File> old_file = std::move(rive_file);

    wrapper_view_model_instance.unref();
    artboard = std::move(p_artboard);
    rive_file = std::move(p_file);

    // A File owns decoded backend images. Release the complete old object
    // graph on the render thread, in dependency order.
    if (old_state_machine || old_animation || old_artboard || old_view_model || old_file) {
        auto release_old = [&]() {
            old_state_machine.reset();
            old_animation.reset();
            old_view_model.reset();
            old_artboard.reset();
            old_file.reset();
        };
        if (!rive_integration::run_on_rive_render_thread(release_old)) {
            release_old();
        }
    }

    if (artboard) {
        UtilityFunctions::print_verbose("RivePlayer: Advancing artboard 0.0f");
        artboard->advance(0.0f);
        UtilityFunctions::print_verbose("RivePlayer: Artboard advanced");

        if (rive_file) {
            int viewModelId = artboard->viewModelId();
            if (viewModelId != -1) {
                view_model_instance = rive_file->createViewModelInstance(viewModelId, 0);
            }

            if (!view_model_instance) {
                view_model_instance = rive_file->createViewModelInstance(artboard.get());
            }
        }

        // Try to load specified state machine or animation
        if (!current_state_machine.is_empty()) {
            state_machine = artboard->stateMachineNamed(current_state_machine.utf8().get_data());
        }

        if (!state_machine && !current_animation.is_empty()) {
            animation = artboard->animationNamed(current_animation.utf8().get_data());
        }

        // Fallback to defaults if nothing specified or found
        if (!state_machine && !animation) {
            if (artboard->stateMachineCount() > 0) {
                state_machine = artboard->stateMachineAt(0);
                current_state_machine = String::utf8(state_machine->name().c_str());
                current_animation = "";
            } else if (artboard->animationCount() > 0) {
                animation = artboard->animationAt(0);
                current_animation = String::utf8(animation->name().c_str());
                current_state_machine = "";
            }
        }

        if (state_machine) {
            if (view_model_instance) {
                state_machine->bindViewModelInstance(view_model_instance);
            }
        } else if (animation) {
            if (view_model_instance) {
                artboard->bindViewModelInstance(view_model_instance);
            }
        }

        if (view_model_instance) {
            if (state_machine) state_machine->advanceAndApply(0.0f);
            else if (animation) animation->advanceAndApply(0.0f);
            else if (artboard) artboard->advance(0.0f);
        }
    }
}

bool RivePlayer::advance(float delta) {
    if (!artboard) return false;

    if (state_machine) {
        return state_machine->advanceAndApply(delta);
    }
    if (animation) {
        return animation->advanceAndApply(delta);
    }
    return artboard->advance(delta);
}

void RivePlayer::draw(rive::Renderer *renderer, const rive::Mat2D &transform) {
    if (artboard) {
        renderer->save();
        renderer->transform(transform);
        artboard->draw(renderer);
        renderer->restore();
    }
}

bool RivePlayer::hit_test(Vector2 position, const rive::Mat2D &transform) {
    if (!state_machine) return false;
    
    rive::Mat2D inverse;
    if (!transform.invert(&inverse)) return false;

    rive::Vec2D rive_pos = inverse * rive::Vec2D(position.x, position.y);
    return state_machine->hitTest(rive_pos);
}

bool RivePlayer::pointer_down(Vector2 position, const rive::Mat2D &transform) {
    if (!state_machine) return false;
    
    rive::Mat2D inverse;
    if (!transform.invert(&inverse)) return false;

    rive::Vec2D rive_pos = inverse * rive::Vec2D(position.x, position.y);
    return state_machine->pointerDown(rive_pos) != rive::HitResult::none;
}

bool RivePlayer::pointer_up(Vector2 position, const rive::Mat2D &transform) {
    if (!state_machine) return false;
    
    rive::Mat2D inverse;
    if (!transform.invert(&inverse)) return false;

    rive::Vec2D rive_pos = inverse * rive::Vec2D(position.x, position.y);
    return state_machine->pointerUp(rive_pos) != rive::HitResult::none;
}

bool RivePlayer::pointer_move(Vector2 position, const rive::Mat2D &transform) {
    if (!state_machine) return false;
    
    rive::Mat2D inverse;
    if (!transform.invert(&inverse)) return false;

    rive::Vec2D rive_pos = inverse * rive::Vec2D(position.x, position.y);
    return state_machine->pointerMove(rive_pos) != rive::HitResult::none;
}

void RivePlayer::play_animation(const String &p_name) {
    if (!artboard) return;

    std::unique_ptr<rive::LinearAnimationInstance> next_animation = artboard->animationNamed(p_name.utf8().get_data());
    if (!next_animation) {
        ERR_PRINT("Rive animation not found: " + p_name);
        return;
    }

    std::unique_ptr<rive::StateMachineInstance> old_state_machine = std::move(state_machine);
    std::unique_ptr<rive::LinearAnimationInstance> old_animation = std::move(animation);
    animation = std::move(next_animation);
    if (old_state_machine || old_animation) {
        auto release_old = [&]() {
            old_state_machine.reset();
            old_animation.reset();
        };
        if (!rive_integration::run_on_rive_render_thread(release_old)) {
            release_old();
        }
    }
    current_animation = p_name;
    current_state_machine = "";
    if (view_model_instance) {
        artboard->bindViewModelInstance(view_model_instance);
    }
}

void RivePlayer::play_state_machine(const String &p_name) {
    if (!artboard) return;

    std::unique_ptr<rive::StateMachineInstance> next_state_machine = artboard->stateMachineNamed(p_name.utf8().get_data());
    if (!next_state_machine) {
        ERR_PRINT("Rive state machine not found: " + p_name);
        return;
    }

    std::unique_ptr<rive::LinearAnimationInstance> old_animation = std::move(animation);
    std::unique_ptr<rive::StateMachineInstance> old_state_machine = std::move(state_machine);
    state_machine = std::move(next_state_machine);
    if (old_animation || old_state_machine) {
        auto release_old = [&]() {
            old_animation.reset();
            old_state_machine.reset();
        };
        if (!rive_integration::run_on_rive_render_thread(release_old)) {
            release_old();
        }
    }
    current_state_machine = p_name;
    current_animation = "";
    if (view_model_instance) {
        state_machine->bindViewModelInstance(view_model_instance);
    }
}

PackedStringArray RivePlayer::get_animation_list() const {
    PackedStringArray list;
    if (artboard) {
        for (size_t i = 0; i < artboard->animationCount(); ++i) {
            list.push_back(String::utf8(artboard->animation(i)->name().c_str()));
        }
    }
    return list;
}

PackedStringArray RivePlayer::get_state_machine_list() const {
    PackedStringArray list;
    if (artboard) {
        for (size_t i = 0; i < artboard->stateMachineCount(); ++i) {
            list.push_back(String::utf8(artboard->stateMachine(i)->name().c_str()));
        }
    }
    return list;
}

Ref<RiveViewModelInstance> RivePlayer::get_rive_view_model_instance() {
    if (wrapper_view_model_instance.is_null() && view_model_instance) {
        wrapper_view_model_instance.instantiate();
        wrapper_view_model_instance->_init(view_model_instance);
    }
    return wrapper_view_model_instance;
}

