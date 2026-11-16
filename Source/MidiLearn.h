#pragma once
#include <JuceHeader.h>

// MIDI CC -> parameter mapping with a learn mode.
//  - audio thread: handleMidi() reads controller messages (lock-free: atomics only)
//  - message thread: a timer applies the latest value of each mapped CC to its parameter (with gestures, so hosts record
//    it as automation) and finishes a pending "learn" once a CC has arrived.
//  - the sound never waits for that timer: from the block a CC arrives in, the audio thread reads the CC's value in place
//    of the parameter (liveValue) until the timer has written it to the parameter. So an offline bounce (where the
//    message thread may not run at all) sounds the same as live playback.
// Mappings are saved in the plugin state as one attribute: "cc:paramId;cc:paramId".
class MidiLearn : private juce::Timer {
public:
    MidiLearn(juce::AudioProcessorValueTreeState& s, juce::StringArray learnableIds)
        : apvts(s), ids(std::move(learnableIds)) {
        for (auto& m : ccToIdx) m.store(-1);
        for (auto& v : pending) v.store(-1);
        for (auto& v : live) v.store(-1.0f);
        jassert(ids.size() <= kMaxIds);
        for (int i = 0; i < juce::jmin(kMaxIds, ids.size()); ++i) {
            raw[(size_t) i] = apvts.getRawParameterValue(ids[i]);
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ids[i]))) param[(size_t) i] = rp;
        }
        startTimerHz(60);
    }
    ~MidiLearn() override { stopTimer(); }

    // ---- audio thread ------------------------------------------------------------------------------------------
    void handleMidi(const juce::MidiBuffer& midi) noexcept {
        for (const auto meta : midi) {
            if (meta.numBytes > 3) continue;                          // SysEx: getMessage() would allocate
            const auto m = meta.getMessage();
            if (! m.isController()) continue;
            const int cc = m.getControllerNumber();
            if (cc < 0 || cc >= 120) continue;                       // 120+ are channel-mode messages
            if (learnIdx.load(std::memory_order_relaxed) >= 0) learnedCC.store(cc, std::memory_order_relaxed);
            if (const int idx = ccToIdx[(size_t) cc].load(std::memory_order_relaxed); idx >= 0 && idx < kMaxIds) {
                pending[(size_t) cc].store(m.getControllerValue(), std::memory_order_relaxed);
                live[(size_t) idx].store((float) m.getControllerValue() / 127.0f, std::memory_order_relaxed);
            }
            lastActivity.store(cc, std::memory_order_relaxed);
        }
        anyLive = false;
        for (auto& v : live) if (v.load(std::memory_order_relaxed) >= 0.0f) { anyLive = true; break; }
    }

    // the value the engine should use for this parameter's raw atomic this block (audio thread, after handleMidi)
    float liveValue(const std::atomic<float>* rawParam, float current) const noexcept {
        if (! anyLive) return current;
        for (size_t i = 0; i < (size_t) kMaxIds; ++i)
            if (raw[i] == rawParam && param[i] != nullptr) {
                const float n = live[i].load(std::memory_order_relaxed);
                return n >= 0.0f ? param[i]->convertFrom0to1(n) : current;
            }
        return current;
    }

    // ---- message thread ----------------------------------------------------------------------------------------
    bool learnMode = false;   // the MIDI LEARN button (UI state)

    void armLearn(const juce::String& paramId) { learnedCC.store(-1); learnIdx.store(ids.indexOf(paramId)); ++revision; }
    void cancelLearn() { learnIdx.store(-1); learnedCC.store(-1); ++revision; }
    juce::String armedParam() const { const int i = learnIdx.load(); return i >= 0 ? ids[i] : juce::String(); }

    int ccFor(const juce::String& paramId) const {
        const int idx = ids.indexOf(paramId); if (idx < 0) return -1;
        for (int cc = 0; cc < 128; ++cc) if (ccToIdx[(size_t) cc].load() == idx) return cc;
        return -1;
    }
    void clear(const juce::String& paramId) {
        const int idx = ids.indexOf(paramId);
        for (auto& m : ccToIdx) if (m.load() == idx) m.store(-1);
        if (idx >= 0 && idx < kMaxIds) live[(size_t) idx].store(-1.0f);
        ++revision;
    }
    void clearAll() { for (auto& m : ccToIdx) m.store(-1); for (auto& v : live) v.store(-1.0f); ++revision; }
    int numMappings() const { int n = 0; for (auto& m : ccToIdx) n += m.load() >= 0; return n; }
    juce::uint32 getRevision() const { return revision.load(); }

    juce::String toString() const {
        juce::StringArray parts;
        for (int cc = 0; cc < 128; ++cc)
            if (const int i = ccToIdx[(size_t) cc].load(); i >= 0) parts.add(juce::String(cc) + ":" + ids[i]);
        return parts.joinIntoString(";");
    }
    void fromString(const juce::String& s) {
        for (auto& m : ccToIdx) m.store(-1);
        for (auto& v : live) v.store(-1.0f);
        for (auto& part : juce::StringArray::fromTokens(s, ";", "")) {
            const int cc = part.upToFirstOccurrenceOf(":", false, false).getIntValue();
            const int i = ids.indexOf(part.fromFirstOccurrenceOf(":", false, false));
            if (cc >= 0 && cc < 120 && i >= 0) ccToIdx[(size_t) cc].store(i);
        }
        ++revision;
    }

    // applies pending CC values / finishes a learn (the timer calls this; tests may call it directly)
    void process() {
        // finish a learn: the first CC that arrived while armed takes the parameter (one CC per parameter)
        if (const int idx = learnIdx.load(); idx >= 0)
            if (const int cc = learnedCC.exchange(-1); cc >= 0) {
                for (auto& m : ccToIdx) if (m.load() == idx) m.store(-1);
                ccToIdx[(size_t) cc].store(idx);
                learnIdx.store(-1); ++revision;
            }
        for (int cc = 0; cc < 128; ++cc) {
            const int v = pending[(size_t) cc].exchange(-1);
            if (v < 0) continue;
            const int idx = ccToIdx[(size_t) cc].load(); if (idx < 0) continue;
            if (auto* p = apvts.getParameter(ids[idx])) {
                const float n = (float) v / 127.0f;
                if (std::abs(p->getValue() - n) >= 1e-4f) { p->beginChangeGesture(); p->setValueNotifyingHost(n); p->endChangeGesture(); }
                // the parameter now holds it: hand the sound back to the parameter, unless a newer CC came in meanwhile
                if (idx < kMaxIds) { float expected = n; live[(size_t) idx].compare_exchange_strong(expected, -1.0f); }
            }
        }
    }

private:
    void timerCallback() override { process(); }

    juce::AudioProcessorValueTreeState& apvts;
    juce::StringArray ids;
    std::array<std::atomic<int>, 128> ccToIdx;
    std::array<std::atomic<int>, 128> pending;
    static constexpr int kMaxIds = 32;
    std::array<std::atomic<float>, kMaxIds> live;                  // normalised CC value not yet in the parameter, or -1
    std::array<const std::atomic<float>*, kMaxIds> raw{};          // each learnable parameter's raw value
    std::array<juce::RangedAudioParameter*, kMaxIds> param{};
    bool anyLive = false;                                          // audio thread only
    std::atomic<int> learnIdx{ -1 }, learnedCC{ -1 }, lastActivity{ -1 };
    std::atomic<juce::uint32> revision{ 1 };
};
