#include "rive_player.h"
#include "../renderer/rive_render_registry.h"
#include <godot_cpp/variant/utility_functions.hpp>

RivePlayer::RivePlayer() {
}

RivePlayer::~RivePlayer() {
}

bool RivePlayer::load_from_bytes(const PackedByteArray &data) {
    rive::Factory *factory = RiveRenderRegistry::get_singleton()->get_factory();
    if (!factory) {
        ERR_PRINT("Rive factory not available (context not created?)");
        return false;
    }

    rive::Span<const uint8_t> bytes(data.ptr(), data.size());
    rive::ImportResult result;
    rive::rcp<rive::File> file = rive::File::import(bytes, factory, &result);
    
    return load(file);
}

bool RivePlayer::load(rive::rcp<rive::File> file) {
    if (file) {
        std::unique_ptr<rive::ArtboardInstance> ab = file->artboardDefault();
        if (ab) {
            set_artboard(std::move(ab), file);
            return true;
        }
    }
    return false;
}

void RivePlayer::set_artboard(std::unique_ptr<rive::ArtboardInstance> p_artboard, rive::rcp<rive::File> p_file) {
    state_machine.reset();
    animation.reset();
    view_model_instance = nullptr;
    artboard = std::move(p_artboard);
    rive_file = p_file;

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
                current_state_machine = state_machine->name().c_str();
                current_animation = "";
            } else if (artboard->animationCount() > 0) {
                animation = artboard->animationAt(0);
                current_animation = animation->name().c_str();
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
            if (artboard) artboard->advance(0.0f);
            if (state_machine) state_machine->advance(0.0f);
            else if (animation) animation->advance(0.0f);
        }
    }
}

void RivePlayer::advance(float delta) {
    if (artboard) {
        if (state_machine) {
            state_machine->advance(delta);
        } else if (animation) {
            animation->advance(delta);
            animation->apply();
        }
        artboard->advance(delta);
    }
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

    state_machine.reset();
    animation = artboard->animationNamed(p_name.utf8().get_data());
    if (animation) {
        current_animation = p_name;
        current_state_machine = "";
    }
}

void RivePlayer::play_state_machine(const String &p_name) {
    if (!artboard) return;

    animation.reset();
    state_machine = artboard->stateMachineNamed(p_name.utf8().get_data());
    if (state_machine) {
        current_state_machine = p_name;
        current_animation = "";
    }
}

PackedStringArray RivePlayer::get_animation_list() const {
    PackedStringArray list;
    if (artboard) {
        for (size_t i = 0; i < artboard->animationCount(); ++i) {
            list.push_back(String(artboard->animation(i)->name().c_str()));
        }
    }
    return list;
}

PackedStringArray RivePlayer::get_state_machine_list() const {
    PackedStringArray list;
    if (artboard) {
        for (size_t i = 0; i < artboard->stateMachineCount(); ++i) {
            list.push_back(String(artboard->stateMachine(i)->name().c_str()));
        }
    }
    return list;
}
