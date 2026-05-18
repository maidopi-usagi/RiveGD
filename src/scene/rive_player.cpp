#include "rive_player.h"
#include "../renderer/rive_render_registry.h"
#include "../renderer/godot_file_asset_loader.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <rive/event_report.hpp>
#include <rive/custom_property.hpp>
#include <rive/custom_property_string.hpp>
#include <rive/custom_property_number.hpp>
#include <rive/custom_property_boolean.hpp>
#include <rive/custom_property_color.hpp>
#include <rive/container_component.hpp>
#include <rive/audio_event.hpp>
#include <rive/assets/audio_asset.hpp>
#include <rive/audio/audio_source.hpp>

void RivePlayer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_rive_view_model_instance"), &RivePlayer::get_rive_view_model_instance);

    ADD_SIGNAL(MethodInfo("rive_event",
        PropertyInfo(Variant::STRING, "name"),
        PropertyInfo(Variant::DICTIONARY, "properties"),
        PropertyInfo(Variant::FLOAT, "delay")));

    ADD_SIGNAL(MethodInfo("rive_audio_event",
        PropertyInfo(Variant::STRING, "name"),
        PropertyInfo(Variant::PACKED_BYTE_ARRAY, "audio_data"),
        PropertyInfo(Variant::STRING, "format"),
        PropertyInfo(Variant::FLOAT, "volume")));
}

RivePlayer::RivePlayer() {
}

RivePlayer::~RivePlayer() {
}

bool RivePlayer::load_from_bytes(const PackedByteArray &data, const String &asset_path) {
    rive::Factory *factory = RiveRenderRegistry::get_singleton()->get_factory();
    if (!factory) {
        ERR_PRINT("Rive factory not available (context not created?)");
        return false;
    }

    rive::Span<const uint8_t> bytes(data.ptr(), data.size());
    rive::ImportResult result;

    // Create a FileAssetLoader for out-of-band assets (fonts, images)
    rive::rcp<rive_integration::GodotFileAssetLoader> asset_loader;
    if (!asset_path.is_empty()) {
        asset_loader = rive::rcp<rive_integration::GodotFileAssetLoader>(
            new rive_integration::GodotFileAssetLoader(asset_path));
    }

    rive::rcp<rive::File> file = rive::File::import(bytes, factory, &result, asset_loader);

    if (!file) {
        ERR_PRINT(vformat("Failed to import Rive file. Result: %d", (int)result));
        return false;
    }

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
    wrapper_view_model_instance.unref();
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

            // Collect and emit reported events
            size_t event_count = state_machine->reportedEventCount();
            for (size_t i = 0; i < event_count; i++) {
                const rive::EventReport report = state_machine->reportedEventAt(i);
                rive::Event* event = report.event();
                if (!event) continue;

                String event_name = String(event->name().c_str());
                Dictionary properties;

                // Extract custom properties from the event
                // Event inherits from ContainerComponent, CustomProperties are children
                for (auto* child : event->children()) {
                    auto* prop = child->as<rive::CustomProperty>();
                    if (!prop) continue;
                    String prop_name = String(prop->name().c_str());

                    if (prop->is<rive::CustomPropertyString>()) {
                        properties[prop_name] = String(prop->as<rive::CustomPropertyString>()->propertyValue().c_str());
                    } else if (prop->is<rive::CustomPropertyNumber>()) {
                        properties[prop_name] = prop->as<rive::CustomPropertyNumber>()->propertyValue();
                    } else if (prop->is<rive::CustomPropertyBoolean>()) {
                        properties[prop_name] = prop->as<rive::CustomPropertyBoolean>()->propertyValue();
                    } else if (prop->is<rive::CustomPropertyColor>()) {
                        properties[prop_name] = (int)prop->as<rive::CustomPropertyColor>()->propertyValue();
                    }
                }

                emit_signal("rive_event", event_name, properties, report.secondsDelay());

                // Handle AudioEvent: extract raw audio bytes and emit to GDScript
                if (event->is<rive::AudioEvent>()) {
                    auto* audioEvent = event->as<rive::AudioEvent>();
                    auto fileAsset = audioEvent->asset();
                    if (fileAsset && fileAsset->is<rive::AudioAsset>()) {
                        auto* audioAsset = fileAsset->as<rive::AudioAsset>();
                        auto audioSource = audioAsset->audioSource();
                        if (audioSource) {
                            // Get raw audio bytes from AudioSource
                            auto audioBytes = audioSource->bytes();
                            if (audioBytes.size() > 0) {
                                PackedByteArray audio_data;
                                audio_data.resize(audioBytes.size());
                                memcpy(audio_data.ptrw(), audioBytes.data(), audioBytes.size());

                                // Determine format string for Godot
                                String format_str;
                                switch (audioSource->format()) {
                                    case rive::AudioFormat::wav:   format_str = "wav"; break;
                                    case rive::AudioFormat::mp3:   format_str = "mp3"; break;
                                    case rive::AudioFormat::flac:  format_str = "flac"; break;
                                    case rive::AudioFormat::vorbis: format_str = "ogg"; break;
                                    default: format_str = "wav"; break;
                                }

                                // Volume = asset volume * artboard volume
                                float vol = audioAsset->volume();
                                if (artboard) {
                                    vol *= artboard->volume();
                                }

                                emit_signal("rive_audio_event", event_name, audio_data, format_str, vol);
                            }
                        }
                    }
                }
            }
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

Ref<RiveViewModelInstance> RivePlayer::get_rive_view_model_instance() {
    if (wrapper_view_model_instance.is_null() && view_model_instance) {
        wrapper_view_model_instance.instantiate();
        wrapper_view_model_instance->_init(view_model_instance);
    }
    return wrapper_view_model_instance;
}
