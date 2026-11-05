#pragma once
#include <JuceHeader.h>
#include "../Params.h"
#include <array>

struct CopedentStatus {
    float pedalA = 0.0f;
    float pedalB = 0.0f;
    float pedalC = 0.0f;
    float leverLKL = 0.0f;
    float leverLKR = 0.0f;
    float volumePedal = 0.92f;
    float barPositionFret = 0.0f;
    int activeVoiceCount = 0;
    std::array<float, 10> stringEnergy{};
    std::array<float, 10> stringPitchSemis{};
    std::array<float, 10> stringBendSemis{};
};

class PedalSteelEngine {
public:
    PedalSteelEngine();
    ~PedalSteelEngine() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setParams(const PedalSteelParams& newParams);
    const PedalSteelParams& getParams() const { return params; }

    void handleMidiMessage(const juce::MidiMessage& msg);
    void renderBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    // Direct performance triggers from UI or automation
    void setManualPedal(int pedalIndex, float value); // 0=A, 1=B, 2=C, 3=LKL, 4=LKR
    void triggerAuditionChord(int chordIndex);
    CopedentStatus getStatus() const;

private:
    static constexpr int kNumStrings = 10;
    static constexpr int kMaxHarmonics = 16;

    // Standard Nashville 10-String E9th open MIDI notes (Strings 1..10):
    // 1:F#4(66), 2:D#4(63), 3:G#4(68), 4:E4(64), 5:B3(59), 6:G#3(56), 7:F#3(54), 8:E3(52), 9:D3(50), 10:B2(47)
    static constexpr std::array<int, kNumStrings> kOpenStringMidi = {
        66, 63, 68, 64, 59, 56, 54, 52, 50, 47
    };

    enum class StringRole {
        Root,       // Responds to LKL (+1), LKR (-1), Pedal C (+2)
        Third,      // Responds to Pedal B (+1)
        Fifth,      // Responds to Pedal A (+2)
        SixthOr9th  // RKL/RKR chromatic coloring
    };

    struct Biquad {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float z1L = 0.0f, z2L = 0.0f, z1R = 0.0f, z2R = 0.0f;

        void reset() { z1L = z2L = z1R = z2R = 0.0f; }
        void setLowpass(double sr, float freq, float q);
        void setHighpass(double sr, float freq, float q);
        void setPeak(double sr, float freq, float q, float gainDb);
        void setBandpass(double sr, float freq, float q);

        inline float processL(float x) {
            float y = b0 * x + z1L;
            z1L = b1 * x - a1 * y + z2L;
            z2L = b2 * x - a2 * y;
            return y;
        }
        inline float processR(float x) {
            float y = b0 * x + z1R;
            z1R = b1 * x - a1 * y + z2R;
            z2R = b2 * x - a2 * y;
            return y;
        }
    };

    struct StringVoice {
        bool active = false;
        bool held = false;
        bool sustainedByPedal = false;
        int stringIndex = 0;       // 0..9 (Strings 1..10)
        int midiNote = 60;         // Currently assigned target MIDI note
        int channel = 1;
        StringRole role = StringRole::Root;

        float velocity = 0.8f;
        double noteAgeSec = 0.0;
        double releaseAgeSec = 0.0;
        uint64_t triggerOrder = 0;

        // Continuous Tonebar & Mechanical Changer states
        float barTargetMidi = 60.0f;
        float barCurrentMidi = 60.0f;
        float barVelocity = 0.0f;   // For 2nd-order smooth tonebar inertia

        float scoopOffsetSemis = 0.0f;   // Decaying grace scoop (-2..0 or -1..0)
        float scoopVel = 0.0f;
        float pedalCurrentSemis = 0.0f;  // Smoothed mechanical changer pull offset
        float pedalVel = 0.0f;

        // Swell & Amp Envelope
        float ampEnv = 0.0f;
        float swellEnv = 1.0f;
        float vibratoEnv = 0.0f;
        double vibratoPhase = 0.0;

        // Multi-Sample playback state
        int multiSampleIdx = 0;
        double samplePos = 0.0;

        // 16-Harmonic phase accumulators
        std::array<double, kMaxHarmonics> harmPhases{};

        // Transient exciter state
        int transientIdx = 0;
        double transientPos = 0.0;
        float transientGain = 0.0f;

        // Per-string filters
        float pickLpState = 0.0f;
        float msHpState = 0.0f;
        float slideNoiseLp = 0.0f;
        float lastOutput = 0.0f;
    };

    // Zero-Comb-Warble Velvet Multi-Tap + 8-Stage Unit-Gain Schroeder Allpass Stereo Plate Reverb
    // Specially engineered for continuous pedal-steel glissandos (0.0 dB comb-tooth ripple during bar slides!)
    struct StudioReverb {
        std::vector<float> velvetBufL, velvetBufR;
        int velvetPos = 0;
        int maxVelvetLen = 1;

        static constexpr int kNumTaps = 12;
        std::array<int, kNumTaps> tapDelaysL{}, tapDelaysR{};
        std::array<float, kNumTaps> tapSignsL{}, tapSignsR{};

        static constexpr int kNumAllpass = 8;
        std::array<std::vector<float>, kNumAllpass> allpassL, allpassR;
        std::array<int, kNumAllpass> allpassPosL{}, allpassPosR{};

        float dampStateL = 0.0f, dampStateR = 0.0f;
        Biquad reverbToneFilter;

        void prepare(double sr);
        void reset();
        void processSample(float inL, float inR, float decay, float width, float& outL, float& outR);
    };

    // Sympathetic E9th string resonator bank
    struct SympatheticBank {
        std::array<std::vector<float>, 4> combBufs;
        std::array<int, 4> combPos{};
        std::array<float, 4> combLp{};
        void prepare(double sr);
        void reset();
        float process(float input, float amount);
    };

    double sampleRate = 44100.0;
    PedalSteelParams params;

    std::array<StringVoice, kNumStrings> voices{};
    uint64_t noteCounter = 0;
    uint32_t rngState = 0x9E3779B9u;

    // Live MIDI / Keyswitch / UI pedal states
    bool sustainPedalDown = false;
    float pitchWheelNorm = 0.0f;   // -1..+1
    float modWheelNorm = 0.0f;     // 0..1
    float expressionNorm = 1.0f;   // 0..1
    std::array<float, 5> keyswitchPedals{}; // A, B, C, LKL, LKR
    std::array<float, 5> uiPedals{};        // A, B, C, LKL, LKR

    // Smoothed master pedal states
    float smoothPedalA = 0.0f;
    float smoothPedalB = 0.0f;
    float smoothPedalC = 0.0f;
    float smoothLKL = 0.0f;
    float smoothLKR = 0.0f;
    float smoothVolPedal = 0.92f;
    float smoothPitchWheel = 0.0f;
    float smoothPitchWheelVel = 0.0f;
    double globalVibratoPhase = 0.0;
    float globalSwellEnv = 1.0f;

    // Master filter chain (Pickup Mid-Scoop + LC Tank + 15" Speaker Cabinet + Post-Amp Tone)
    Biquad midScoopFilter;
    Biquad pickupPeakFilter;
    Biquad speakerBodyFilter;
    Biquad speakerPresenceFilter;
    Biquad speakerLowpass1;
    Biquad speakerLowpass2;
    Biquad dcBlocker;

    // Close-miked stereo speaker cabinet dual-mic (SM57 dustcap L + R121 cone R with 0.55ms baffle offset)
    std::array<float, 64> cabHaasBuf{};
    int cabHaasPos = 0;
    float cabConeLpR = 0.0f;

    SympatheticBank sympathetic;
    StudioReverb reverb;

    // Helper methods
    float nextRandomSigned();
    void updateFilterCoeffs();
    void reassignChordRoles();
    int chooseBestStringForNote(int midiNote, float velocity, bool& outIsLegatoTransfer, float& outPrevBarMidi);
    int selectBestMultiSample(int midiNote, int style) const;
    float readMultiSampleHermite(int sampleIdx, double pos) const;
};
