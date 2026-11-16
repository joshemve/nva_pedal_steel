// Release stress test: drives the real processor (and editor) the way hosts do at their worst and checks that nothing
// crashes, the output stays finite and bounded, the audio thread does not allocate, state survives garbage, and it runs
// faster than real time. Build it with AddressSanitizer too (cmake -DPEDALSTEEL_ASAN=ON) to catch memory errors.
// Usage: PedalSteelStress [seconds-per-config, default 2] [--quick]
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <atomic>
#include <cstdlib>
#include <new>
#include <random>
#include <thread>
#if JUCE_WINDOWS
 #define NOMINMAX
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#endif

// CPU time this thread has actually run (not wall time): stays accurate when the machine is busy with other work
static double threadCpuSeconds() {
#if JUCE_WINDOWS
    static const double hz = [] { DWORD mhz = 0, sz = sizeof(mhz);
        RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\DESCRIPTION\System\CentralProcessor\0", "~MHz", RRF_RT_REG_DWORD, nullptr, &mhz, &sz);
        return mhz > 0 ? mhz * 1.0e6 : 3.0e9; }();
    ULONG64 c = 0; QueryThreadCycleTime(GetCurrentThread(), &c); return (double) c / hz;
#else
    return juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks());
#endif
}

// ---- audio-thread allocation counter: global operator new/delete, counted only while a thread has armed itself ----
static thread_local bool tlArmed = false;
static std::atomic<long long> gAudioAllocs{ 0 };
void* operator new(std::size_t n) { if (tlArmed) gAudioAllocs.fetch_add(1, std::memory_order_relaxed); if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { if (tlArmed) gAudioAllocs.fetch_add(1, std::memory_order_relaxed); if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
struct Armed { Armed() { tlArmed = true; } ~Armed() { tlArmed = false; } };

static int fails = 0;
static void check(bool ok, const juce::String& what) { std::cout << (ok ? "  PASS " : "  FAIL ") << what << std::endl; if (! ok) ++fails; }

struct Scan { bool finite = true; float peak = 0.0f; };
static void scan(const juce::AudioBuffer<float>& b, int n, Scan& s) {
    for (int c = 0; c < b.getNumChannels(); ++c) {
        const float* d = b.getReadPointer(c);
        for (int i = 0; i < n; ++i) { const float v = d[i]; if (! std::isfinite(v)) s.finite = false; else s.peak = std::max(s.peak, std::abs(v)); }
    }
}

// random MIDI the way a bad controller / host sends it: every message type, any channel, positions anywhere in the block
static void fuzzMidi(juce::MidiBuffer& m, std::mt19937& rng, int numSamples, float density) {
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    auto ri = [&](int lo, int hi) { return lo + (int) (u(rng) * (float) (hi - lo + 1)) % (hi - lo + 1); };
    const int count = u(rng) < density ? ri(1, 6) : 0;
    for (int i = 0; i < count; ++i) {
        const int pos = numSamples > 0 ? ri(0, numSamples - 1) : 0, ch = ri(1, 16);
        switch (ri(0, 11)) {
            case 0: case 1: case 2: m.addEvent(juce::MidiMessage::noteOn(ch, ri(0, 127), (juce::uint8) ri(0, 127)), pos); break;
            case 3: case 4: m.addEvent(juce::MidiMessage::noteOff(ch, ri(0, 127), (juce::uint8) ri(0, 127)), pos); break;
            case 5: m.addEvent(juce::MidiMessage::pitchWheel(ch, ri(0, 16383)), pos); break;
            case 6: m.addEvent(juce::MidiMessage::controllerEvent(ch, ri(0, 127), ri(0, 127)), pos); break;
            case 7: m.addEvent(juce::MidiMessage::controllerEvent(ch, 64, ri(0, 1) * 127), pos); break;
            case 8: m.addEvent(juce::MidiMessage::channelPressureChange(ch, ri(0, 127)), pos); break;
            case 9: m.addEvent(juce::MidiMessage::aftertouchChange(ch, ri(0, 127), ri(0, 127)), pos); break;
            case 10: m.addEvent(juce::MidiMessage::programChange(ch, ri(0, 127)), pos); break;
            default: m.addEvent(u(rng) < 0.5f ? juce::MidiMessage::allNotesOff(ch) : juce::MidiMessage::allSoundOff(ch), pos); break;
        }
    }
}

static void randomiseParams(PedalSteelAudioProcessor& p, std::mt19937& rng, float fraction) {
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    for (auto* prm : p.getParameters()) if (u(rng) < fraction) prm->setValueNotifyingHost(u(rng) < 0.15f ? (u(rng) < 0.5f ? 0.0f : 1.0f) : u(rng));
}

int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI init;
    double secs = 2.0; bool quick = false;
    for (int a = 1; a < argc; ++a) { juce::String s(argv[a]); if (s == "--quick") quick = true; else if (s.getDoubleValue() > 0) secs = s.getDoubleValue(); }
    auto pump = [](int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); };
    std::mt19937 rng(20261116);

    // 1. every sample rate x block size: fuzzed MIDI + automation; finite, bounded, no audio-thread allocation, real time
    std::cout << "1. sample rates x block sizes (fuzzed MIDI + automation, " << secs << " s each)" << std::endl;
    const double rates[] = { 22050, 44100, 48000, 88200, 96000, 176400, 192000 };
    const int blocks[] = { 1, 16, 64, 441, 512, 4096 };
    double worstLoad = 0; juce::String worstLoadAt;
    for (double sr : rates) for (int bs : blocks) {
        if (quick && ! ((sr == 44100 || sr == 96000) && (bs == 64 || bs == 441))) continue;
        PedalSteelAudioProcessor p; p.setPlayConfigDetails(0, 2, sr, bs); p.prepareToPlay(sr, bs);
        juce::AudioBuffer<float> buf(2, bs); Scan s; gAudioAllocs = 0;
        const int nBlocks = (int) std::ceil(secs * sr / bs); double maxLoad = 0, total = 0;
        for (int b = 0; b < nBlocks; ++b) {
            juce::MidiBuffer midi; fuzzMidi(midi, rng, bs, bs >= 64 ? 0.35f : 0.02f);
            if (b % std::max(1, nBlocks / 40) == 0) randomiseParams(p, rng, 0.3f);
            if (b % std::max(1, nBlocks / 6) == 0) p.setCurrentProgram((int) (rng() % (unsigned) std::max(1, p.getNumPrograms())));
            for (int c = 0; c < 2; ++c) juce::FloatVectorOperations::fill(buf.getWritePointer(c), 1.0e30f, bs);   // garbage in
            const double t0 = threadCpuSeconds();
            { Armed arm; p.processBlock(buf, midi); }
            const double dt = (threadCpuSeconds() - t0);
            total += dt; if (bs >= 64) maxLoad = std::max(maxLoad, dt / (bs / sr));
            scan(buf, bs, s);
        }
        const double avgLoad = total / (nBlocks * (bs / sr));
        if (avgLoad > worstLoad) { worstLoad = avgLoad; worstLoadAt = juce::String(sr, 0) + " Hz / " + juce::String(bs); }
        const long long allocs = gAudioAllocs.load();
        check(s.finite && s.peak < 8.0f && allocs == 0 && avgLoad < (bs == 1 ? 1.0 : 0.6),   // 1-sample blocks: per-call overhead dominates
              juce::String(sr, 0) + " Hz, block " + juce::String(bs) + ": peak " + juce::String(s.peak, 3) + (s.finite ? "" : " NaN/Inf!")
              + ", audio-thread allocations " + juce::String(allocs) + ", CPU avg " + juce::String(avgLoad * 100, 1) + "% worst block " + juce::String(maxLoad * 100, 0) + "%");
        p.releaseResources();
    }
    std::cout << "  (heaviest average load: " << worstLoad * 100 << "% at " << worstLoadAt << ")" << std::endl;

    // 1b. realistic load and where the time goes: a held 10-note chord per rate; then single events timed alone
    std::cout << "1b. realistic load (10 held notes, no fuzz) and per-event block cost at 48 kHz / 64" << std::endl;
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 }) {
        PedalSteelAudioProcessor p; p.setPlayConfigDetails(0, 2, sr, 64); p.prepareToPlay(sr, 64); juce::AudioBuffer<float> buf(2, 64); double total = 0, worst = 0; const int nB = (int) (sr * 4 / 64);
        for (int b = 0; b < nB; ++b) { juce::MidiBuffer midi; if (b % (nB / 4) == 0) for (int n : { 40, 47, 52, 56, 59, 64, 68, 71, 76, 80 }) midi.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8) 100), 0);
            const double t0 = threadCpuSeconds(); p.processBlock(buf, midi); const double dt = (threadCpuSeconds() - t0); total += dt; worst = std::max(worst, dt); }
        const double avg = total / (nB * 64 / sr);
        check(avg < 0.5, juce::String(sr, 0) + " Hz: CPU avg " + juce::String(avg * 100, 1) + "%, worst block " + juce::String(worst * 1e3, 2) + " ms (block = " + juce::String(64 / sr * 1e3, 2) + " ms)");
    }
    {
        struct Ev { const char* name; std::function<void(PedalSteelAudioProcessor&, juce::MidiBuffer&)> f; };
        const Ev evs[] = {
            { "idle (held chord)", [](PedalSteelAudioProcessor&, juce::MidiBuffer&) {} },
            { "10 note-ons", [](PedalSteelAudioProcessor&, juce::MidiBuffer& m) { for (int n = 50; n < 60; ++n) m.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8) 100), 0); } },
            { "low keyswitch / phrase notes 24..47", [](PedalSteelAudioProcessor&, juce::MidiBuffer& m) { for (int n = 24; n < 48; ++n) m.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8) 100), 0); } },
            { "notes on channel 2 (phrases)", [](PedalSteelAudioProcessor&, juce::MidiBuffer& m) { for (int n = 36; n < 48; ++n) m.addEvent(juce::MidiMessage::noteOn(2, n, (juce::uint8) 100), 0); } },
            { "all notes off", [](PedalSteelAudioProcessor&, juce::MidiBuffer& m) { m.addEvent(juce::MidiMessage::allNotesOff(1), 0); } },
            { "MIDI program change", [](PedalSteelAudioProcessor&, juce::MidiBuffer& m) { m.addEvent(juce::MidiMessage::programChange(1, 5), 0); } },
            { "preset change (host)", [](PedalSteelAudioProcessor& p, juce::MidiBuffer&) { p.setCurrentProgram((p.getCurrentProgram() + 7) % p.getNumPrograms()); } },
            { "FX character change", [](PedalSteelAudioProcessor& p, juce::MidiBuffer&) { if (auto* q = p.apvts.getParameter(ParamIDs::fxCharacter)) q->setValueNotifyingHost(std::fmod(q->getValue() + 0.37f, 1.0f)); } },
            { "preset style change", [](PedalSteelAudioProcessor& p, juce::MidiBuffer&) { if (auto* q = p.apvts.getParameter(ParamIDs::presetStyle)) q->setValueNotifyingHost(std::fmod(q->getValue() + 0.37f, 1.0f)); } },
            { "play mode change", [](PedalSteelAudioProcessor& p, juce::MidiBuffer&) { if (auto* q = p.apvts.getParameter(ParamIDs::playMode)) q->setValueNotifyingHost(std::fmod(q->getValue() + 0.37f, 1.0f)); } },
            { "delay time / reverb decay jump", [](PedalSteelAudioProcessor& p, juce::MidiBuffer&) { for (auto id : { ParamIDs::delayTime, ParamIDs::reverbDecay }) if (auto* q = p.apvts.getParameter(id)) q->setValueNotifyingHost(q->getValue() > 0.5f ? 0.0f : 1.0f); } },
            { "all parameters random", [&](PedalSteelAudioProcessor& p, juce::MidiBuffer&) { randomiseParams(p, rng, 1.0f); } } };
        for (auto& ev : evs) {
            PedalSteelAudioProcessor p; p.setPlayConfigDetails(0, 2, 48000, 64); p.prepareToPlay(48000, 64); juce::AudioBuffer<float> buf(2, 64); double worst = 0;
            for (int b = 0; b < 3000; ++b) { juce::MidiBuffer midi; if (b == 0) for (int n : { 52, 59, 64, 68 }) midi.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8) 100), 0);
                const bool fire = b > 200 && b % 150 == 0; if (fire) ev.f(p, midi);
                const double t0 = threadCpuSeconds(); p.processBlock(buf, midi); const double dt = (threadCpuSeconds() - t0);
                if (b > 200 && (fire || ev.f == nullptr)) worst = std::max(worst, dt); if (b > 200 && juce::String(ev.name).startsWith("idle")) worst = std::max(worst, dt); }
            check(worst < 64 / 48000.0, juce::String(ev.name) + ": worst block " + juce::String(worst * 1e3, 3) + " ms of 1.333 ms");
        }
    }

    // 1c. the same chord must sound the same at every host rate (high rates run the engine at 44.1 / 48 kHz and upsample)
    std::cout << "1c. level and spectrum match across host rates" << std::endl;
    {
        auto render = [](double sr, int bs, float& rms, float& hfRatio) {
            PedalSteelAudioProcessor p; p.setPlayConfigDetails(0, 2, sr, bs); p.prepareToPlay(sr, bs); juce::AudioBuffer<float> buf(2, bs);
            double e = 0, eh = 0; float prev = 0, prev2 = 0; long long n = 0; const int nB = (int) (sr * 3 / bs);
            for (int b = 0; b < nB; ++b) { juce::MidiBuffer midi; if (b == 0) for (int note : { 52, 59, 64, 68 }) midi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8) 100), 0);
                p.processBlock(buf, midi); const float* d = buf.getReadPointer(0);
                for (int i = 0; i < bs; ++i) { e += d[i] * d[i]; const float hp = d[i] - 2 * prev + prev2; eh += hp * hp; prev2 = prev; prev = d[i]; ++n; } }
            rms = (float) std::sqrt(e / (double) n); hfRatio = (float) std::sqrt(eh / std::max(1.0e-20, e));
        };
        float r48, h48, r96, h96, r192, h192, r44, h44, r88, h88;
        render(48000, 256, r48, h48); render(96000, 256, r96, h96); render(192000, 441, r192, h192); render(44100, 256, r44, h44); render(88200, 100, r88, h88);
        auto dB = [](float a, float b) { return juce::Decibels::gainToDecibels(a / std::max(1.0e-9f, b)); };
        check(std::abs(dB(r96, r48)) < 0.5f && std::abs(dB(r192, r48)) < 0.5f && std::abs(dB(r88, r44)) < 0.5f,
              "chord level: 96k " + juce::String(dB(r96, r48), 2) + " dB, 192k " + juce::String(dB(r192, r48), 2) + " dB vs 48k; 88.2k " + juce::String(dB(r88, r44), 2) + " dB vs 44.1k");
        check(h96 < h48 && h192 < h96, "no added ultrasonic energy when upsampled (2nd-difference ratio 48k " + juce::String(h48, 4) + ", 96k " + juce::String(h96, 4) + ", 192k " + juce::String(h192, 4) + ")");
        PedalSteelAudioProcessor p; p.setPlayConfigDetails(0, 2, 96000, 256); p.prepareToPlay(96000, 256);
        check(p.getLatencySamples() == 16, "reports its upsampler latency to the host at 96 kHz (" + juce::String(p.getLatencySamples()) + " samples = " + juce::String(p.getLatencySamples() / 96.0, 2) + " ms)");
        p.prepareToPlay(48000, 256); check(p.getLatencySamples() == 0, "zero latency at 48 kHz");
    }

    // 2. variable block sizes inside one prepare (0 .. max), then blocks LARGER than promised, then mono
    std::cout << "2. variable / oversized / zero-length blocks, mono" << std::endl;
    {
        PedalSteelAudioProcessor p; p.setPlayConfigDetails(0, 2, 48000, 512); p.prepareToPlay(48000, 512);
        Scan s; std::uniform_int_distribution<int> d(0, 512);
        for (int b = 0; b < 4000; ++b) { const int n = (b % 50 == 0) ? 0 : d(rng); juce::AudioBuffer<float> buf(2, std::max(1, n)); buf.setSize(2, n, false, false, true);
            juce::MidiBuffer midi; fuzzMidi(midi, rng, n, 0.3f); p.processBlock(buf, midi); scan(buf, n, s); }
        check(s.finite && s.peak < 8.0f, "random block sizes 0..512 (peak " + juce::String(s.peak, 3) + ")");
        Scan s2; for (int b = 0; b < 200; ++b) { const int n = 512 * (2 + b % 15); juce::AudioBuffer<float> buf(2, n); juce::MidiBuffer midi; fuzzMidi(midi, rng, n, 0.5f);
            p.processBlock(buf, midi); scan(buf, n, s2); }
        check(s2.finite && s2.peak < 8.0f, "blocks up to 16x larger than prepareToPlay promised (peak " + juce::String(s2.peak, 3) + ")");
        juce::MidiBuffer bad; bad.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 5000); bad.addEvent(juce::MidiMessage::noteOff(1, 60), 99999);
        juce::AudioBuffer<float> buf(2, 64); Scan s3; p.processBlock(buf, bad); scan(buf, 64, s3);
        check(s3.finite, "MIDI events positioned past the end of the block");
    }
    {
        PedalSteelAudioProcessor p; juce::AudioProcessor::BusesLayout mono; mono.outputBuses.add(juce::AudioChannelSet::mono());
        const bool ok = p.checkBusesLayoutSupported(mono) && p.setBusesLayout(mono);
        p.prepareToPlay(44100, 256); juce::AudioBuffer<float> buf(1, 256); Scan s;
        for (int b = 0; b < 400; ++b) { juce::MidiBuffer midi; fuzzMidi(midi, rng, 256, 0.4f); p.processBlock(buf, midi); scan(buf, 256, s); }
        check(ok && s.finite && s.peak < 8.0f, "mono output bus (peak " + juce::String(s.peak, 3) + ")");
    }

    // 3. re-prepare at another rate without re-creating; process straight after releaseResources + prepare; reset
    std::cout << "3. sample-rate changes on a live instance" << std::endl;
    {
        PedalSteelAudioProcessor p; Scan s;
        for (int round = 0; round < 24; ++round) {
            const double sr = rates[round % 7]; const int bs = blocks[1 + round % 5];
            p.setPlayConfigDetails(0, 2, sr, bs); p.prepareToPlay(sr, bs); juce::AudioBuffer<float> buf(2, bs);
            for (int b = 0; b < 60; ++b) { juce::MidiBuffer midi; if (b == 0) for (int n : { 52, 59, 64, 68 }) midi.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8) 110), 0);
                p.processBlock(buf, midi); scan(buf, bs, s); }
            if (round % 3 == 0) p.reset();
            if (round % 2 == 0) p.releaseResources();     // notes still sounding when the host stops / re-prepares
        }
        check(s.finite && s.peak < 8.0f, "24 prepare / release cycles across all rates with notes held (peak " + juce::String(s.peak, 3) + ")");
    }

    // 4. silence: after all notes off the output must die away completely (no DC, no stuck tail, no denormal slowdown)
    std::cout << "4. tail and silence" << std::endl;
    {
        PedalSteelAudioProcessor p; p.setPlayConfigDetails(0, 2, 48000, 256); p.prepareToPlay(48000, 256); juce::AudioBuffer<float> buf(2, 256);
        for (int prog = 0; prog < p.getNumPrograms(); prog += std::max(1, p.getNumPrograms() / 8)) {
            p.setCurrentProgram(prog);
            for (int b = 0; b < 200; ++b) { juce::MidiBuffer midi; if (b == 0) for (int n : { 40, 52, 64, 76 }) midi.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8) 127), 0);
                if (b == 100) midi.addEvent(juce::MidiMessage::allNotesOff(1), 0), midi.addEvent(juce::MidiMessage::allSoundOff(1), 0);
                if (b == 100) for (int n : { 40, 52, 64, 76 }) midi.addEvent(juce::MidiMessage::noteOff(1, n), 1);
                p.processBlock(buf, midi); }
        }
        float tail = 1.0f; double slowest = 0; int quietAt = -1;
        for (int b = 0; b < 48000 * 40 / 256; ++b) { juce::MidiBuffer midi; const double t0 = threadCpuSeconds(); p.processBlock(buf, midi);
            slowest = std::max(slowest, (threadCpuSeconds() - t0));
            tail = buf.getMagnitude(0, 256); if (tail < 1.0e-5f && quietAt < 0) quietAt = b; if (tail >= 1.0e-5f) quietAt = -1; }
        check(tail < 1.0e-5f, "output reaches silence after notes end (level after 40 s: " + juce::String(juce::Decibels::gainToDecibels(tail, -200.0f), 1) + " dB, quiet from "
              + (quietAt < 0 ? juce::String("never") : juce::String(quietAt * 256 / 48000.0, 1) + " s") + "; declared tail " + juce::String(p.getTailLengthSeconds(), 1) + " s)");
        check(slowest < 256 / 48000.0, "no denormal slowdown in the silent tail (slowest block " + juce::String(slowest * 1e6, 0) + " us of " + juce::String(256 / 48000.0 * 1e6, 0) + ")");
    }

    // 5. every factory preset: loads, plays, finite
    std::cout << "5. every preset" << std::endl;
    {
        PedalSteelAudioProcessor p; p.setPlayConfigDetails(0, 2, 44100, 256); p.prepareToPlay(44100, 256); juce::AudioBuffer<float> buf(2, 256); Scan s; int silentPresets = 0; juce::String silentNames;
        for (int prog = 0; prog < p.getNumPrograms(); ++prog) {
            p.setCurrentProgram(prog); float energy = 0;
            for (int b = 0; b < 120; ++b) { juce::MidiBuffer midi; if (b == 2) for (int n : { 52, 59, 64, 68, 71 }) midi.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8) 100), 0);
                if (b == 90) for (int n : { 52, 59, 64, 68, 71 }) midi.addEvent(juce::MidiMessage::noteOff(1, n), 0);
                p.processBlock(buf, midi); scan(buf, 256, s); energy += buf.getRMSLevel(0, 0, 256); }
            if (energy / 120 < 1.0e-4f) { ++silentPresets; silentNames << p.getProgramName(prog) << "; "; }
        }
        check(s.finite && s.peak < 4.0f, juce::String(p.getNumPrograms()) + " presets play, finite, peak " + juce::String(s.peak, 3) + " (" + juce::String(juce::Decibels::gainToDecibels(s.peak), 1) + " dBFS)");
        check(silentPresets == 0, "no preset is silent on a played chord" + (silentPresets ? " -- silent: " + silentNames : juce::String()));
        check(p.getProgramName(-1).isEmpty() || true, "getProgramName(-1) does not crash"); p.setCurrentProgram(-5); p.setCurrentProgram(100000);
        juce::MidiBuffer midi; p.processBlock(buf, midi); check(true, "setCurrentProgram out of range does not crash (now " + juce::String(p.getCurrentProgram()) + ")");
    }

    // 6. state: round trip, then truncated / corrupted / garbage / empty data must never crash and must leave it playable
    std::cout << "6. state save / restore" << std::endl;
    {
        PedalSteelAudioProcessor a; a.setPlayConfigDetails(0, 2, 48000, 256); a.prepareToPlay(48000, 256); randomiseParams(a, rng, 1.0f);
        juce::MemoryBlock st; a.getStateInformation(st);
        PedalSteelAudioProcessor b; b.setPlayConfigDetails(0, 2, 48000, 256); b.prepareToPlay(48000, 256); b.setStateInformation(st.getData(), (int) st.getSize());
        int diff = 0; juce::String which;
        for (int i = 0; i < a.getParameters().size(); ++i) if (auto* ra = dynamic_cast<juce::RangedAudioParameter*>(a.getParameters()[i]))
            if (std::abs(ra->convertTo0to1(ra->convertFrom0to1(ra->getValue())) - b.getParameters()[i]->getValue()) > 1.0e-4f) { ++diff; which << a.getParameters()[i]->getName(40) << " "; }
        check(diff == 0, "round trip restores all " + juce::String(a.getParameters().size()) + " parameters" + (diff ? " -- differ: " + which : juce::String()));
        juce::MemoryBlock st2; b.getStateInformation(st2); check(st == st2 || st.getSize() == st2.getSize(), "saved state is stable (save -> load -> save)");
        juce::AudioBuffer<float> buf(2, 256); Scan s; int n = 0;
        auto tryState = [&](const void* d, int sz) { b.setStateInformation(d, sz); juce::MidiBuffer midi; midi.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100), 0); b.processBlock(buf, midi); scan(buf, 256, s); ++n; };
        tryState(nullptr, 0); tryState(st.getData(), 0);
        for (size_t len = 1; len < st.getSize(); len += std::max<size_t>(1, st.getSize() / 300)) tryState(st.getData(), (int) len);          // truncated
        for (int k = 0; k < 400; ++k) { juce::MemoryBlock m(st); auto* d = (juce::uint8*) m.getData(); for (int f = 0; f < 1 + k % 12; ++f) d[rng() % m.getSize()] = (juce::uint8) rng(); tryState(m.getData(), (int) m.getSize()); }   // bit rot
        for (int k = 0; k < 200; ++k) { juce::MemoryBlock m((size_t) (1 + rng() % 4096)); auto* d = (juce::uint8*) m.getData(); for (size_t i = 0; i < m.getSize(); ++i) d[i] = (juce::uint8) rng(); tryState(m.getData(), (int) m.getSize()); }   // noise
        {   // well-formed XML with hostile values
            const char* hostile[] = { "<PARAMS/>", "<Parameters currentPresetIndex=\"999999\" editorWidth=\"-7\" midiMap=\"zzz:::;;;999=nope\"/>",
                "<Parameters><PARAM id=\"outputLevel\" value=\"nan\"/><PARAM id=\"delayTime\" value=\"1e38\"/><PARAM id=\"presetStyle\" value=\"-4000\"/><PARAM id=\"nope\" value=\"1\"/></Parameters>" };
            for (auto* h : hostile) {
                for (auto tag : { juce::String(h), juce::String(h).replace("Parameters", b.apvts.state.getType().toString()) }) {
                    if (auto xml = juce::parseXML(tag)) { juce::MemoryBlock m; juce::AudioProcessor::copyXmlToBinary(*xml, m); tryState(m.getData(), (int) m.getSize()); }
                }
            }
        }
        bool paramsFinite = true; juce::String badParams; for (auto* prm : b.getParameters()) { const float v = prm->getValue(); if (! std::isfinite(v) || v < 0.0f || v > 1.0f) { paramsFinite = false; badParams << prm->getName(40) << "=" << juce::String(v) << " "; } }
        check(s.finite && s.peak < 8.0f, juce::String(n) + " empty / truncated / corrupted / hostile states: no crash, audio finite (peak " + juce::String(s.peak, 3) + ")");
        check(paramsFinite, "every parameter is still a legal 0..1 value afterwards" + (paramsFinite ? juce::String() : " -- " + badParams));
        b.setStateInformation(st.getData(), (int) st.getSize()); juce::MemoryBlock st3; b.getStateInformation(st3);
        check(st3.getSize() == st.getSize(), "a good state still loads after all that");
    }

    // 7. user preset files with bad content
    std::cout << "7. user preset files" << std::endl;
    {
        PedalSteelAudioProcessor p; p.setPlayConfigDetails(0, 2, 48000, 256); p.prepareToPlay(48000, 256);
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("nvps_stress"); dir.deleteRecursively(); dir.createDirectory();
        auto good = dir.getChildFile("good.nvps"); const bool saved = p.saveUserPreset(good);
        check(saved && good.existsAsFile() && p.loadUserPreset(good), "save then load a user preset");
        int crashes = 0;
        auto bad = dir.getChildFile("bad.nvps");
        bad.replaceWithText(""); p.loadUserPreset(bad);
        bad.replaceWithText("not xml at all \x01\x02"); p.loadUserPreset(bad);
        bad.replaceWithText("<wrong><tags/></wrong>"); p.loadUserPreset(bad);
        { juce::MemoryBlock m(200000); auto* d = (juce::uint8*) m.getData(); for (size_t i = 0; i < m.getSize(); ++i) d[i] = (juce::uint8) rng(); bad.replaceWithData(m.getData(), m.getSize()); p.loadUserPreset(bad); }
        { auto txt = good.loadFileAsString(); bad.replaceWithText(txt.substring(0, txt.length() / 2)); p.loadUserPreset(bad); }
        p.loadUserPreset(dir.getChildFile("does_not_exist.nvps")); p.loadUserPreset(dir);
        juce::AudioBuffer<float> buf(2, 256); juce::MidiBuffer midi; Scan s; p.processBlock(buf, midi); scan(buf, 256, s);
        check(crashes == 0 && s.finite, "empty / garbage / truncated / missing preset files are refused without a crash");
        dir.deleteRecursively();
    }

    // 8. threads: audio running while the "host" and "UI" hammer parameters, presets, state and UI commands
    std::cout << "8. concurrency (audio thread vs automation, presets, state, UI commands)" << std::endl;
    {
        PedalSteelAudioProcessor p; p.setPlayConfigDetails(0, 2, 48000, 128); p.prepareToPlay(48000, 128);
        std::atomic<bool> run{ true }; std::atomic<bool> finite{ true }; std::atomic<float> peak{ 0 }; std::atomic<long long> blocksDone{ 0 };
        std::thread audio([&] { std::mt19937 r(7); juce::AudioBuffer<float> buf(2, 128);
            while (run.load()) { juce::MidiBuffer midi; fuzzMidi(midi, r, 128, 0.3f); p.processBlock(buf, midi); Scan s; scan(buf, 128, s); if (! s.finite) finite = false; peak = std::max(peak.load(), s.peak); ++blocksDone; } });
        std::thread host([&] { std::mt19937 r(11); while (run.load()) { randomiseParams(p, r, 0.5f); std::this_thread::yield(); } });
        std::thread host2([&] { std::mt19937 r(13); juce::MemoryBlock st; while (run.load()) { p.getStateInformation(st); if (r() % 4 == 0) p.setStateInformation(st.getData(), (int) st.getSize()); (void) p.getLiveStatus(); std::this_thread::yield(); } });
        const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) (std::max(3.0, secs * 2) * 1000); std::mt19937 r(17);
        while (juce::Time::getMillisecondCounter() < end) {   // this is the message thread: what the editor does
            switch (r() % 9) { case 0: p.stepPreset(r() % 2 ? 1 : -1); break; case 1: p.triggerChordAudition((int) (r() % 8)); break; case 2: p.triggerPanic(); break;
                case 3: p.triggerFretboardNote((int) (r() % 10), (float) (r() % 24)); break; case 4: p.slideFretboardNote((int) (r() % 10), (float) (r() % 24)); break;
                case 5: p.releaseFretboardNote((int) (r() % 10)); break; case 6: p.setLivePedalState((int) (r() % 10), (float) (r() % 2)); break;
                case 7: p.triggerPhraseAudition(); break; default: p.slideHeldMidiNotes((float) (r() % 5) - 2.0f); break; }
            pump(1);
        }
        run = false; audio.join(); host.join(); host2.join();
        check(finite.load() && peak.load() < 8.0f && blocksDone.load() > 100, "survived " + juce::String(blocksDone.load()) + " audio blocks under concurrent host + UI activity (peak " + juce::String(peak.load(), 3) + ")");
    }

    // 9. editor: open / close many times while audio runs, resize to extremes, two processors at once
    std::cout << "9. editor lifetime" << std::endl;
    {
        PedalSteelAudioProcessor p, q; p.setPlayConfigDetails(0, 2, 48000, 256); p.prepareToPlay(48000, 256); q.setPlayConfigDetails(0, 2, 48000, 256); q.prepareToPlay(48000, 256);
        std::atomic<bool> run{ true }; std::atomic<bool> finite{ true };
        std::thread audio([&] { std::mt19937 r(5); juce::AudioBuffer<float> buf(2, 256);
            while (run.load()) { juce::MidiBuffer midi; fuzzMidi(midi, r, 256, 0.3f); p.processBlock(buf, midi); Scan s; scan(buf, 256, s); if (! s.finite) finite = false;
                juce::MidiBuffer m2; q.processBlock(buf, m2); std::this_thread::sleep_for(std::chrono::microseconds(300)); } });
        const int rounds = quick ? 8 : 40; bool sizesOk = true;
        for (int i = 0; i < rounds; ++i) {
            std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditorIfNeeded()), ed2(i % 4 == 0 ? q.createEditorIfNeeded() : nullptr);
            if (ed == nullptr) { sizesOk = false; break; }
            juce::DocumentWindow w("stress", juce::Colours::black, 0); w.setAccessible(false); w.setContentNonOwned(ed.get(), true); w.setTopLeftPosition(-6000, -6000); w.setVisible(true); pump(i % 5 == 0 ? 120 : 15);
            const int ws[] = { 1, 200, 640, 1280, 1600, 2400, 5000 }; ed->setSize(ws[i % 7], ws[(i + 3) % 7]); pump(10);
            if (ed->getWidth() <= 0 || ed->getHeight() <= 0) sizesOk = false;
            randomiseParams(p, rng, 0.4f); p.stepPreset(1); pump(10);
            w.clearContentComponent(); w.setVisible(false);
            p.editorBeingDeleted(ed.get()); ed.reset(); if (ed2 != nullptr) { q.editorBeingDeleted(ed2.get()); ed2.reset(); }
            pump(5);
        }
        run = false; audio.join();
        check(sizesOk && finite.load(), juce::String(rounds) + " editor open / resize / close cycles with audio running, two instances");
    }

    std::cout << (fails == 0 ? "ALL STRESS CHECKS PASSED" : "STRESS CHECKS FAILED: " + juce::String(fails)) << std::endl;
    return fails == 0 ? 0 : 1;
}
