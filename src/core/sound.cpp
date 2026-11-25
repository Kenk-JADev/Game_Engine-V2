#include "core/sound.h"
#include "core/log.h"
#include "core/wav.h"   // Etappe 21: portabler WAV-Leser

#include <atomic>
#include <cstring>
#include <map>
#include <vector>

namespace rune::core::sound {
namespace {

constexpr int kRate = 44100;
constexpr int kBuffers = 3;
constexpr int kSamplesPerBuffer = 1024;   // ~23 ms pro Puffer
// WICHTIG: Die Startverzögerung eines neuen Sounds entspricht der Menge
// vorgequeueter Stille (kBuffers * Pufferdauer). 3 x 23 ms = max ~70 ms,
// im Schnitt ~35 ms — hört sich sofort an. Grössere Puffer fühlen
// sich träg an (4 x 8192 waren ~740 ms Vorlauf!).
constexpr int kMaxVoices = 12;

struct Voice {
    const std::vector<int16_t>* pcm = nullptr;
    size_t pos = 0;
    float  vol = 1.0f;
    bool   loop = false;
    std::string name;
};

HWAVEOUT            g_wave = nullptr;
WAVEHDR             g_hdr[kBuffers];
int16_t             g_buf[kBuffers][kSamplesPerBuffer];
std::atomic<bool>   g_done[kBuffers];
bool                g_started[kBuffers];

std::map<std::string, std::vector<int16_t>> g_sounds;
Voice g_voices[kMaxVoices];
int   g_voice_count = 0;

void CALLBACK wave_out_proc(HWAVEOUT, UINT msg, DWORD_PTR, DWORD_PTR dw_param1, DWORD_PTR) {
    if (msg == WOM_DONE) {
        // dwParam1 zeigt auf den fertigen Header; dwUser = Slot-Index.
        const WAVEHDR* h = reinterpret_cast<const WAVEHDR*>(dw_param1);
        g_done[static_cast<int>(h->dwUser)] = true;
    }
}

} // namespace

bool init() {
    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = kRate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = kRate * 2;

    MMRESULT r = waveOutOpen(&g_wave, WAVE_MAPPER, &fmt,
                             reinterpret_cast<DWORD_PTR>(&wave_out_proc),
                             0, CALLBACK_FUNCTION);
    if (r != MMSYSERR_NOERROR) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "waveOutOpen fehlgeschlagen (0x%X)", static_cast<unsigned>(r));
        log_error(buf);
        return false;
    }

    for (int i = 0; i < kBuffers; ++i) {
        std::memset(&g_hdr[i], 0, sizeof(WAVEHDR));
        g_hdr[i].lpData = reinterpret_cast<LPSTR>(g_buf[i]);
        g_hdr[i].dwBufferLength = kSamplesPerBuffer * 2;
        g_hdr[i].dwUser = static_cast<DWORD_PTR>(i);
        g_done[i] = true;
        g_started[i] = false;
    }

    log_info("Sound bereit (waveOut, 44100 Hz mono)");
    return true;
}

void register_sound(const std::string& name, int rate, const int16_t* samples, size_t count) {
    if (!samples || count == 0) return;
    if (rate == kRate) {
        g_sounds[name].assign(samples, samples + count);
        return;
    }
    // Linear hoch/runter sampeln auf die Geräterate.
    const double step = static_cast<double>(rate) / kRate;
    const size_t out_count = static_cast<size_t>(static_cast<double>(count) / step);
    std::vector<int16_t>& out = g_sounds[name];
    out.resize(out_count);
    for (size_t i = 0; i < out_count; ++i) {
        const double src = static_cast<double>(i) * step;
        const size_t i0 = static_cast<size_t>(src);
        const size_t i1 = (i0 + 1 < count) ? i0 + 1 : i0;
        const float f = static_cast<float>(src - static_cast<double>(i0));
        out[i] = static_cast<int16_t>(samples[i0] * (1.0f - f) + samples[i1] * f + 0.5f);
    }
}

// Etappe 21: WAV-Bytes kommen aus Ruby (sichere Assets-Brücke), werden hier
// geparst, auf mono heruntergemischt und mit Pitch auf die Geräterate
// gebracht. register_sound übernimmt den Rest (Resampling auf 44100 Hz).
bool load_wav(const std::string& name, const unsigned char* bytes, size_t size, int pitch) {
    if (!bytes || size == 0) {
        log_warn("Sound: leere WAV-Daten für '" + name + "'");
        return false;
    }
    wav::Clip clip;
    std::string fehler;
    if (!wav::parse(bytes, size, clip, &fehler)) {
        log_warn("Sound: '" + name + "': " + fehler);
        return false;
    }
    if (pitch != 100) clip = wav::resample(clip, wav::rate_fuer_pitch(clip.sample_rate, pitch));
    register_sound(name, clip.sample_rate, clip.samples.data(), clip.samples.size());
    char info[160];
    std::snprintf(info, sizeof(info), "Sound: '%s' geladen (%d Hz, %.2f s, Pitch %d%%)",
                  name.c_str(), clip.sample_rate,
                  clip.sample_rate > 0
                      ? static_cast<double>(clip.samples.size()) / clip.sample_rate
                      : 0.0,
                  pitch);
    log_info(info);
    return true;
}

bool has(const std::string& name) {
    const auto it = g_sounds.find(name);
    return it != g_sounds.end() && !it->second.empty();
}

void set_volume(const std::string& name, float volume) {
    const float v = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
    for (int i = 0; i < g_voice_count; ++i) {
        if (g_voices[i].name == name) g_voices[i].vol = v;
    }
}

float volume_of(const std::string& name) {
    for (int i = 0; i < g_voice_count; ++i) {
        if (g_voices[i].name == name) return g_voices[i].vol;
    }
    return 0.0f;
}

void play(const std::string& name, float volume) {
    auto it = g_sounds.find(name);
    if (it == g_sounds.end() || it->second.empty()) return;
    if (g_voice_count >= kMaxVoices) return;   // notfalls leise Sounds verwerfen
    Voice& v = g_voices[g_voice_count++];
    v.pcm = &it->second;
    v.pos = 0;
    v.vol = volume;
    v.loop = false;
    v.name = name;
}

void loop(const std::string& name, float volume) {
    auto it = g_sounds.find(name);
    if (it == g_sounds.end() || it->second.empty()) return;
    // Bereits laufende Stimmen desselben Namens ersetzen (F5/Wechsel-Sicher).
    int w = 0;
    for (int i = 0; i < g_voice_count; ++i) {
        if (g_voices[i].name != name) g_voices[w++] = g_voices[i];
    }
    g_voice_count = w;
    if (g_voice_count >= kMaxVoices) return;
    Voice& v = g_voices[g_voice_count++];
    v.pcm = &it->second;
    v.pos = 0;
    v.vol = volume;
    v.loop = true;
    v.name = name;
}

void stop(const std::string& name) {
    int w = 0;
    for (int i = 0; i < g_voice_count; ++i) {
        if (g_voices[i].name != name) g_voices[w++] = g_voices[i];
    }
    g_voice_count = w;
}

void update() {
    if (!g_wave) return;

    // Fertige Stimmen entfernen; Loops beginnen von vorn.
    int w = 0;
    for (int i = 0; i < g_voice_count; ++i) {
        if (g_voices[i].pcm && g_voices[i].pos < g_voices[i].pcm->size()) {
            g_voices[w++] = g_voices[i];
        } else if (g_voices[i].loop) {
            g_voices[i].pos = 0;
            g_voices[w++] = g_voices[i];
        }
    }
    g_voice_count = w;

    // Freie Puffer neu füllen und an waveOut übergeben.
    for (int b = 0; b < kBuffers; ++b) {
        bool free_now = !g_started[b] || g_done[b].exchange(false);
        if (!free_now) continue;

        if (g_started[b]) waveOutUnprepareHeader(g_wave, &g_hdr[b], sizeof(WAVEHDR));

        std::memset(g_buf[b], 0, sizeof(g_buf[b]));
        for (int i = 0; i < g_voice_count; ++i) {
            Voice& v = g_voices[i];
            const std::vector<int16_t>& pcm = *v.pcm;
            const size_t n = pcm.size();
            for (int s = 0; s < kSamplesPerBuffer && v.pos < n; ++s, ++v.pos) {
                int mixed = g_buf[b][s] + static_cast<int>(pcm[v.pos] * v.vol);
                if (mixed > 32767) mixed = 32767;
                if (mixed < -32768) mixed = -32768;
                g_buf[b][s] = static_cast<int16_t>(mixed);
            }
        }

        g_hdr[b].dwBufferLength = kSamplesPerBuffer * 2;
        if (waveOutPrepareHeader(g_wave, &g_hdr[b], sizeof(WAVEHDR)) == MMSYSERR_NOERROR &&
            waveOutWrite(g_wave, &g_hdr[b], sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
            g_started[b] = true;
        } else {
            g_done[b] = true;
        }
    }
}

void shutdown() {
    if (!g_wave) return;
    waveOutReset(g_wave);
    for (int b = 0; b < kBuffers; ++b) {
        if (g_started[b]) waveOutUnprepareHeader(g_wave, &g_hdr[b], sizeof(WAVEHDR));
    }
    waveOutClose(g_wave);
    g_wave = nullptr;
    g_voice_count = 0;
    g_sounds.clear();
}

bool ready() { return g_wave != nullptr; }

} // namespace rune::core::sound
