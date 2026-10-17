#ifndef RIVE_AUDIO_STREAM_H
#define RIVE_AUDIO_STREAM_H
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_playback_resampled.hpp>
#include <rive/audio/audio_engine.hpp>
#include <atomic>

class RiveAudioStreamPlayback : public godot::AudioStreamPlaybackResampled {
    GDCLASS(RiveAudioStreamPlayback, godot::AudioStreamPlaybackResampled);
    rive::rcp<rive::AudioEngine> engine;
    std::atomic<bool> playing{false};
    std::atomic<uint64_t> frames{0};
    static std::atomic<RiveAudioStreamPlayback *> active_playback;
protected:
    static void _bind_methods() {}
public:
    ~RiveAudioStreamPlayback();
    void initialize(rive::rcp<rive::AudioEngine> p_engine) { engine = std::move(p_engine); }
    void _start(double p_from_pos) override;
    void _stop() override;
    bool _is_playing() const override { return playing.load(); }
    int32_t _get_loop_count() const override { return 0; }
    double _get_playback_position() const override;
    void _seek(double) override {}
    int32_t _mix_resampled(godot::AudioFrame *p_buffer, int32_t p_frames) override;
    float _get_stream_sampling_rate() const override;
};

class RiveAudioStream : public godot::AudioStream {
    GDCLASS(RiveAudioStream, godot::AudioStream);
protected:
    static void _bind_methods();
public:
    godot::Ref<godot::AudioStreamPlayback> _instantiate_playback() const override;
    godot::String _get_stream_name() const override { return "Rive Runtime Audio"; }
    double _get_length() const override { return 0; }
    bool _is_monophonic() const override { return true; }
    static bool is_available();
};
#endif
