#pragma once
#include <JuceHeader.h>
#include "../Params.h"
#include <array>
#include <vector>

struct CopedentStatus {
    float pedalA = 0.0f;
    float pedalB = 0.0f;
    float pedalC = 0.0f;
    float leverLKL = 0.0f;
    float leverLKR = 0.0f;
    float volumePedal = 0.92f;
    float barPositionFret = 0.0f;
    int activeVoiceCount = 0;
    int presetStyle = 0;
    bool phrasePlaying = false;
    int activeLickSlot = 0;
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
    void triggerPhraseAudition();
    void allNotesOff();

    // Interactive 10-String Fretboard mouse pluck & continuous chrome-bar slide
    void triggerFretboardNote(int stringIndex, float fret);
    void slideFretboardNote(int stringIndex, float fret);
    void releaseFretboardNote(int stringIndex);

    CopedentStatus getStatus() const;

    static double computeSyncedDelayMs(float delayParamMs, double bpm);
    static juce::String getSyncedSubdivisionLabel(float delayParamMs);

private:
    static constexpr int kNumStrings = 10;
    static constexpr int kMaxHarmonics = 16;

    // Standard Nashville 10-String E9th, Back-Neck C6th, and Baritone B6 open MIDI notes (Strings 1..10):
    static constexpr std::array<int, kNumStrings> kOpenStringMidi = {
        66, 63, 68, 64, 59, 56, 54, 52, 50, 47
    };
    static constexpr std::array<int, kNumStrings> kOpenStringMidiC6 = {
        67, 64, 60, 57, 55, 52, 48, 45, 41, 36
    };
    static constexpr std::array<int, kNumStrings> kOpenStringMidiB6 = {
        61, 58, 63, 59, 54, 51, 49, 47, 45, 42
    };
    static int getOpenStringMidiForStyle(int style, int stringIdx);

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
        bool uiTriggered = false;
        int stringIndex = 0;       // 0..9 (Strings 1..10)
        int midiNote = 60;         // Currently assigned target MIDI note
        int channel = 1;
        StringRole role = StringRole::Root;

        float velocity = 0.8f;
        double noteAgeSec = 0.0;
        double releaseAgeSec = 0.0;
        uint64_t triggerOrder = 0;

        // Continuous Tonebar, MPE & Mechanical Changer states
        float barTargetMidi = 60.0f;
        float barCurrentMidi = 60.0f;
        float barVelocity = 0.0f;

        float scoopOffsetSemis = 0.0f;
        float scoopVel = 0.0f;
        float pedalCurrentSemis = 0.0f;
        float pedalVel = 0.0f;
        float mpePitchSemis = 0.0f;
        float mpePitchVel = 0.0f;
        float activePitchWheel = 0.0f;

        // Swell & Amp Envelope
        float ampEnv = 0.0f;
        float phraseTargetAmp = 1.0f;
        float swellEnv = 1.0f;
        float chokeFade = 0.0f;
        float vibratoEnv = 0.0f;
        double vibratoPhase = 0.0;

        // Multi-Sample & Studio Phrase Articulation playback state
        bool isPhraseSlice = false;
        bool isPalmHarmonic = false;
        int phraseSliceIdx = 0;
        float phraseTransposeSemis = 0.0f;
        int multiSampleIdx = 0;
        double samplePos = 0.0;

        // 16-Harmonic phase accumulators + Baritone sub-resonator phase
        std::array<double, kMaxHarmonics> harmPhases{};
        double subHarmPhase = 0.0;
        float chimeHpStateL = 0.0f;
        float chimeHpStateR = 0.0f;
        float chimeLpStateL = 0.0f;
        float chimeLpStateR = 0.0f;

        // Transient exciter state
        int transientIdx = 0;
        double transientPos = 0.0;
        float transientGain = 0.0f;

        // Per-string pick & chrome-bar winding slide resonator state
        float pickLpState = 0.0f;
        float slideSvfBand = 0.0f;
        float slideSvfLow  = 0.0f;
        float lastOutput = 0.0f;
    };

    // Warm Stereo Echoplex / BBD Tape Delay with Capstan Modulation & Dynamic Ducking
    struct StudioTapeDelay {
        std::vector<float> bufL, bufR;
        int writePos = 0;
        int maxLen = 1;
        float smoothDelaySamples = 15000.0f;
        double wowPhase = 0.0;
        float fbLpL = 0.0f, fbLpR = 0.0f;
        float fbHpL = 0.0f, fbHpR = 0.0f;
        float duckEnv = 0.0f;
        float slideDuckSmooth = 1.0f;

        void prepare(double sr);
        void reset();
        void processSample(double sr, float inL, float inR, float delayMs, float mix, float slideSpeed, float& outWetL, float& outWetR);
    };

    // Dattorro Figure-8 Cross-Coupled Stereo Plate Reverb
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
        float dampCoeff = 0.24f;
        Biquad reverbToneFilter;

        void prepare(double sr);
        void reset();
        void processSample(float inL, float inR, float decay, float width, float& outL, float& outR);
    };

    // Sympathetic E9th string & wood cabinet resonator bank
    struct SympatheticBank {
        std::array<std::vector<float>, 4> combBufs;
        std::array<int, 4> combPos{};
        std::array<float, 4> combLp{};
        float lpCoeff = 0.14f;
        void prepare(double sr);
        void reset();
        float process(float input, float amount);
    };

    // First-Principles Modern Producer & Vintage Hardware Character Circuits (Modes 1..29; Mode 0 = bit-transparent bypass)
    struct ModernCharacterProcessor {
        std::vector<float> modBufL, modBufR;
        int modWritePos = 0;
        int modMaxLen = 1;
        double lfoPhase1 = 0.0;
        double lfoPhase2 = 0.27;
        double lfoPhase3 = 0.61;
        double tremPhase = 0.0;
        float cassetteSagEnv = 0.0f;

        // Sample-rate-adaptive dual-grain C1 Hann-windowed +12st/-12st Shimmer/Organ, Leslie 147, Microcosm & Gated Diode Room Tank
        std::vector<float> shimBufL, shimBufR;
        int shimWritePos = 0;
        int shimMaxLen = 1;
        double shimPhasor = 0.0;
        double subPhasor = 0.0;
        double auxPhasor = 0.0;
        float shimLpL = 0.0f, shimLpR = 0.0f;
        float gateEnv = 0.0f;

        // 1965 Leslie 147 Dual-Motor Inertia, LR2 800 Hz Crossover & 3D Horn/Drum Beaming state
        double hornRateHz = 0.78;
        double drumRateHz = 0.66;
        float xoverLp1L = 0.0f, xoverLp1R = 0.0f;
        float xoverLp2L = 0.0f, xoverLp2R = 0.0f;
        float hornBeamLpL = 0.0f, hornBeamLpR = 0.0f;
        float drumBeamLpL = 0.0f, drumBeamLpR = 0.0f;
        float leslieDcL = 0.0f, leslieDcR = 0.0f;

        // Trapezoidal ZDF Dual-Formant Vocal Tract SVF state (Mode 12: Talk-Box Vowel)
        float svf1Ic1L = 0.0f, svf1Ic2L = 0.0f, svf1Ic1R = 0.0f, svf1Ic2R = 0.0f;
        float svf2Ic1L = 0.0f, svf2Ic2L = 0.0f, svf2Ic1R = 0.0f, svf2Ic2R = 0.0f;

        // 4-stage Analog JFET Optical All-Pass Phaser state (Mode 3: Dark Side Phase Steel)
        std::array<float, 4> phaserStateL{}, phaserStateR{};
        float phaserFbL = 0.0f, phaserFbR = 0.0f;

        // 4-stage Dispersive Accutronics 3-Spring Tank Allpass Chirp Network
        std::array<std::vector<float>, 4> springApL, springApR;
        std::array<int, 4> springApPosL{}, springApPosR{};

        int currentMode = 0;
        int targetMode = 0;
        int lastCoeffMode = -1;
        float wetGain = 0.0f;
        bool hasRendered = false;

        Biquad charFilter1;
        Biquad charFilter2;
        Biquad charFilter3;
        Biquad charFilter4;
        Biquad dimHpFilter;
        Biquad subLpFilter;

        void prepare(double sr);
        void reset();
        void setTargetMode(double sr, int mode);
        void applyModeCoeffs(double sr, int mode);
        int getCurrentMode() const { return currentMode; }
        float getWetMix() const { return (currentMode > 0) ? wetGain : 0.0f; }
        void processSample(double sr, int desiredMode, double hostBpm, bool tempoSync, float pedalActivity,
                           float& ioL, float& ioR, float& outExtraRevL, float& outExtraRevR);
    };

    static constexpr int kNumChokeTails = 16;

    double sampleRate = 44100.0;
    PedalSteelParams params;

    std::array<StringVoice, kNumStrings> voices{};
    std::array<StringVoice, kNumChokeTails> chokeTails{};
    int nextChokeSlot = 0;
    uint64_t noteCounter = 0;
    uint32_t rngState = 0x9E3779B9u;

    // Live MIDI / Keyswitch / MPE / UI pedal states
    bool sustainPedalDown = false;
    float pitchWheelNorm = 0.0f;   // Master Channel 1 pitch wheel (-1..+1)
    std::array<float, 17> mpeChannelPitchNorm{}; // Per-channel MPE pitch bend (channels 1..16)
    std::array<float, 17> mpeChannelPressure{};  // Per-channel aftertouch pressure (0..1)
    float modWheelNorm = 0.0f;     // 0..1
    float expressionNorm = 1.0f;   // 0..1
    int selectedLickSlot = 0;      // 0..3 selected studio lick slot
    int uiAuditionLickCycle = 0;   // Cycles licks 0..3 when clicking UI "PLAY LICK"
    std::array<float, 5> keyswitchPedals{}; // A, B, C, LKL, LKR
    std::array<float, 5> ccPedals{};        // Continuous CC 65..69: A, B, C, LKL, LKR
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

    // Physical String Bus Filter Chain (Pickup Mid-Scoop + LC Tank + 15" Speaker Cabinet)
    Biquad midScoopFilter;
    Biquad pickupPeakFilter;
    Biquad speakerBodyFilter;
    Biquad speakerPresenceFilter;
    Biquad dcBlockerPhys;

    // Delta-Calibrated Studio Sample & Phrase Bus Filters
    Biquad studioMidClarifierFilter;
    Biquad studioDeltaCoilFilter;
    Biquad studioDeltaWarmthFilter;
    Biquad studioDeltaTrebleFilter;
    Biquad studioTweedBarkFilter;
    Biquad studioTweedCabFilter;
    Biquad dcBlockerStudio;
    float tweedSagEnv = 0.0f;
    float rackBellSmooth = 0.0f;
    float organRankSmooth = 0.0f;
    float jawariSmooth = 0.0f;
    float cs80BrassSmooth = 0.0f;

    SympatheticBank sympathetic;
    ModernCharacterProcessor modernChar;
    StudioTapeDelay tapeDelay;
    StudioReverb reverb;

    // Helper methods
    float nextRandomSigned();
    void updateFilterCoeffs();
    void reassignChordRoles();
    void pushVoiceToChokeTail(const StringVoice& v, int strIdx);
    int chooseBestStringForNote(int midiNote, int channel, float velocity, bool& outIsLegatoTransfer, float& outPrevBarMidi);
    int selectBestMultiSample(int midiNote, int style, float velocity) const;
    int selectPhraseSliceBySlot(int slotIndex, int style) const;
    void startPhraseVoice(int phraseIdx, int midiNote, int channel, float vel, float transposeSemis, bool fromUi);
    void readStereoMultiSampleHermite(int sampleIdx, double relPos, float& outL, float& outR) const;
    void readStereoPhraseSliceHermite(int phraseIdx, double relPos, float& outL, float& outR) const;
};
