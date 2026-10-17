#include "rive_audio_stream.h"
#include <godot_cpp/core/class_db.hpp>
#include <algorithm>

using namespace godot;
std::atomic<RiveAudioStreamPlayback *> RiveAudioStreamPlayback::active_playback{nullptr};

void RiveAudioStream::_bind_methods() {
    ClassDB::bind_static_method("RiveAudioStream", D_METHOD("is_available"), &RiveAudioStream::is_available);
}
bool RiveAudioStream::is_available() {
#ifdef EXTERNAL_RIVE_AUDIO_ENGINE
    return true;
#else
    return false;
#endif
}
Ref<AudioStreamPlayback> RiveAudioStream::_instantiate_playback() const {
    ERR_FAIL_COND_V_MSG(!is_available(), Ref<AudioStreamPlayback>(), "Build with rive_audio_backend=godot to use RiveAudioStream. The default build uses Rive's native audio device.");
    auto engine = rive::AudioEngine::RuntimeEngine();
    ERR_FAIL_COND_V_MSG(!engine || engine->channels() != 2, Ref<AudioStreamPlayback>(), "Cannot create a stereo Rive audio mixer.");
    Ref<RiveAudioStreamPlayback> playback;
    playback.instantiate();
    playback->initialize(std::move(engine));
    return playback;
}
RiveAudioStreamPlayback::~RiveAudioStreamPlayback() { _stop(); }
void RiveAudioStreamPlayback::_start(double) {
    if (!engine) return;
    RiveAudioStreamPlayback *expected = nullptr;
    if (!active_playback.compare_exchange_strong(expected, this) && expected != this) return;
    frames.store(0);
    playing.store(true);
    begin_resample();
}
void RiveAudioStreamPlayback::_stop() {
    playing.store(false);
    RiveAudioStreamPlayback *expected = this;
    active_playback.compare_exchange_strong(expected, nullptr);
}
double RiveAudioStreamPlayback::_get_playback_position() const {
    return engine ? static_cast<double>(frames.load()) / engine->sampleRate() : 0;
}
float RiveAudioStreamPlayback::_get_stream_sampling_rate() const {
    return engine ? engine->sampleRate() : 48000;
}
int32_t RiveAudioStreamPlayback::_mix_resampled(AudioFrame *p_buffer, int32_t p_frames) {
    if (!p_buffer || p_frames <= 0) return 0;
    std::fill_n(p_buffer, p_frames, AudioFrame{0, 0});
#ifdef EXTERNAL_RIVE_AUDIO_ENGINE
    if (!playing.load() || !engine) return 0;
    static_assert(sizeof(AudioFrame) == 2 * sizeof(float), "Stereo frame ABI changed");
    uint64_t read = 0;
    if (!engine->readAudioFrames(reinterpret_cast<float *>(p_buffer), p_frames, &read)) return 0;
    frames.fetch_add(read);
    return static_cast<int32_t>(read);
#else
    return 0;
#endif
}
