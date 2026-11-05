#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PedalSteelLookAndFeel::PedalSteelLookAndFeel() {
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1d1814));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xfff3e8d2));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff6b533b));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1d1814));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xfff3e8d2));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffb87b31));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a211b));
    setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0dec2));
}

void PedalSteelLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float rotaryStartAngle,
                                             float rotaryEndAngle, juce::Slider& slider) {
    const float radius = static_cast<float>(juce::jmin(width / 2, height / 2)) - 5.0f;
    const float centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f - 2.0f;
    const float rx = centreX - radius;
    const float ry = centreY - radius;
    const float rw = radius * 2.0f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Outer value arc track
    juce::Path bgArc;
    bgArc.addCentredArc(centreX, centreY, radius + 2.5f, radius + 2.5f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff2c231d));
    g.strokePath(bgArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Active warm amber value arc
    juce::Path valArc;
    valArc.addCentredArc(centreX, centreY, radius + 2.5f, radius + 2.5f, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(juce::Colour(0xffe59b3c));
    g.strokePath(valArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Brushed chrome & knurled vintage Peavey/Fender skirted knob body
    juce::ColourGradient bodyGrad(juce::Colour(0xff4c4640), centreX - radius * 0.5f, centreY - radius * 0.5f,
                                  juce::Colour(0xff1b1815), centreX + radius * 0.6f, centreY + radius * 0.6f, true);
    g.setGradientFill(bodyGrad);
    g.fillEllipse(rx, ry, rw, rw);

    // Chrome inner cap
    const float capR = radius * 0.66f;
    juce::ColourGradient capGrad(juce::Colour(0xffd8cfc0), centreX - capR * 0.4f, centreY - capR * 0.4f,
                                 juce::Colour(0xff6e655c), centreX + capR * 0.5f, centreY + capR * 0.5f, true);
    g.setGradientFill(capGrad);
    g.fillEllipse(centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f);

    g.setColour(juce::Colour(0xff14110e));
    g.drawEllipse(rx, ry, rw, rw, 1.5f);

    // Pointer indicator line
    juce::Path p;
    const float pointerLength = radius * 0.85f;
    const float pointerThickness = 2.6f;
    p.addRoundedRectangle(-pointerThickness * 0.5f, -radius + 2.0f, pointerThickness, pointerLength * 0.58f, 1.0f);
    p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    g.setColour(juce::Colour(0xffffdca0));
    g.fillPath(p);

    // Draw current value readout at bottom of knob
    g.setColour(juce::Colour(0xffcbbba0));
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText(slider.getTextFromValue(slider.getValue()), x, y + height - 14, width, 14, juce::Justification::centred, false);
}

//==============================================================================
FretboardVisualizer::FretboardVisualizer(PedalSteelAudioProcessor& proc)
    : processor(proc) {}

void FretboardVisualizer::updateStatus(const CopedentStatus& st) {
    status = st;
    repaint();
}

void FretboardVisualizer::mouseDown(const juce::MouseEvent& e) {
    const auto pt = e.position;
    for (int i = 0; i < 6; ++i) {
        if (pedalRects[static_cast<size_t>(i)].contains(pt)) {
            heldUiPedal = i;
            if (i == 0) processor.setLivePedalState(0, 1.0f);
            else if (i == 1) { processor.setLivePedalState(0, 1.0f); processor.setLivePedalState(1, 1.0f); }
            else if (i == 2) processor.setLivePedalState(1, 1.0f);
            else if (i == 3) processor.setLivePedalState(2, 1.0f);
            else if (i == 4) processor.setLivePedalState(3, 1.0f);
            else if (i == 5) processor.setLivePedalState(4, 1.0f);
            repaint();
            return;
        }
    }
}

void FretboardVisualizer::mouseUp(const juce::MouseEvent&) {
    if (heldUiPedal >= 0) {
        if (heldUiPedal == 0) processor.setLivePedalState(0, 0.0f);
        else if (heldUiPedal == 1) { processor.setLivePedalState(0, 0.0f); processor.setLivePedalState(1, 0.0f); }
        else if (heldUiPedal == 2) processor.setLivePedalState(1, 0.0f);
        else if (heldUiPedal == 3) processor.setLivePedalState(2, 0.0f);
        else if (heldUiPedal == 4) processor.setLivePedalState(3, 0.0f);
        else if (heldUiPedal == 5) processor.setLivePedalState(4, 0.0f);
        heldUiPedal = -1;
        repaint();
    }
}

void FretboardVisualizer::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();

    // Birdseye Maple / Lacquered Walnut Pedal Steel Cabinet Frame
    juce::ColourGradient woodGrad(juce::Colour(0xff3b2314), bounds.getX(), bounds.getY(),
                                  juce::Colour(0xff1e120a), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(woodGrad);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colour(0xff8c6d46));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 8.0f, 1.5f);

    // Fretboard area (top 72%) and Interactive Pedal/Knee Lever Bar (bottom 28%)
    auto neckArea = bounds.reduced(14.0f, 10.0f);
    auto pedalStrip = neckArea.removeFromBottom(44.0f);
    neckArea.removeFromBottom(6.0f);

    // Dark rosewood/ebony fret board with pearloid position markers
    g.setColour(juce::Colour(0xff120f0d));
    g.fillRoundedRectangle(neckArea, 5.0f);
    g.setColour(juce::Colour(0xff524538));
    g.drawRoundedRectangle(neckArea, 5.0f, 1.2f);

    const float nutX = neckArea.getX() + 58.0f;
    const float pickupX = neckArea.getRight() - 62.0f;
    const float scaleWidth = pickupX - nutX - 24.0f;

    // Draw 24 frets along neck
    static constexpr int kMarkerFrets[] = { 3, 5, 7, 9, 12, 15, 17, 19, 24 };
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    for (int f = 0; f <= 24; ++f) {
        float normX = static_cast<float>(f) / 24.0f;
        float fx = nutX + normX * scaleWidth;
        bool isMarker = false;
        for (int mf : kMarkerFrets) if (mf == f) isMarker = true;

        g.setColour(f == 0 ? juce::Colour(0xffd9ceb8) : (isMarker ? juce::Colour(0xff7a6a58) : juce::Colour(0xff362f29)));
        g.drawLine(fx, neckArea.getY() + 4.0f, fx, neckArea.getBottom() - 14.0f, f == 0 ? 3.0f : 1.2f);

        if (isMarker || f == 0) {
            g.setColour(juce::Colour(0xffa89882));
            g.drawText(juce::String(f), static_cast<int>(fx - 10), static_cast<int>(neckArea.getBottom() - 14), 20, 13, juce::Justification::centred, false);
        }
    }

    // Draw High-Inductance Single-Coil Pickup & All-Pull Changer Bridge on right
    juce::Rectangle<float> pickupRect(pickupX - 8.0f, neckArea.getY() + 6.0f, 18.0f, neckArea.getHeight() - 20.0f);
    g.setColour(juce::Colour(0xff252220));
    g.fillRoundedRectangle(pickupRect, 4.0f);
    g.setColour(juce::Colour(0xff9c8e7b));
    g.drawRoundedRectangle(pickupRect, 4.0f, 1.2f);

    juce::Rectangle<float> changerRect(neckArea.getRight() - 34.0f, neckArea.getY() + 4.0f, 26.0f, neckArea.getHeight() - 18.0f);
    juce::ColourGradient chromeGrad(juce::Colour(0xffd0cbc2), changerRect.getX(), changerRect.getY(),
                                    juce::Colour(0xff5e5952), changerRect.getRight(), changerRect.getBottom(), false);
    g.setGradientFill(chromeGrad);
    g.fillRoundedRectangle(changerRect, 3.0f);

    // Draw the 10 Nashville E9th Strings + Live Vibration Glow + Individual String Pull Indicators
    static const char* kStringNames[10] = {
        "1 F#4", "2 D#4", "3 G#4 (B)", "4 E4 (C/LK)", "5 B3 (A)",
        "6 G#3 (B)", "7 F#3", "8 E3 (LK)", "9 D3", "10 B2 (A)"
    };
    const float strTop = neckArea.getY() + 10.0f;
    const float strBot = neckArea.getBottom() - 20.0f;
    const float strStep = (strBot - strTop) / 9.0f;

    g.setFont(juce::Font(10.0f, juce::Font::bold));
    for (int s = 0; s < 10; ++s) {
        const float sy = strTop + static_cast<float>(s) * strStep;
        const float energy = status.stringEnergy[static_cast<size_t>(s)];
        const float bend = status.stringBendSemis[static_cast<size_t>(s)];

        // Left string label
        g.setColour(energy > 0.05f ? juce::Colour(0xffffb84d) : juce::Colour(0xff9e8f7c));
        g.drawText(kStringNames[s], static_cast<int>(neckArea.getX() + 4), static_cast<int>(sy - 6), 52, 12, juce::Justification::centredLeft, false);

        // String gauge thickness (plain 1..5, wound 6..10)
        const float thickness = 1.0f + static_cast<float>(s) * 0.16f;
        if (energy > 0.02f) {
            g.setColour(juce::Colour(0xffe59b3c).withAlpha(juce::jlimit(0.0f, 0.85f, energy * 0.9f)));
            g.drawLine(nutX, sy, changerRect.getX(), sy, thickness + 3.5f * energy);
        }
        g.setColour(energy > 0.02f ? juce::Colour(0xfffff0d4) : juce::Colour(0xff8c8275));
        g.drawLine(nutX, sy, changerRect.getX(), sy, thickness);

        // Pole piece on pickup
        g.setColour(juce::Colour(0xffd0c8b8));
        g.fillEllipse(pickupRect.getCentreX() - 2.5f, sy - 2.5f, 5.0f, 5.0f);

        // Individual changer finger bend readout on right when string is pulled by pedal/lever!
        if (std::abs(bend) > 0.08f) {
            juce::String bendTxt = (bend > 0.0f ? "+" : "") + juce::String(bend, 1);
            g.setColour(bend > 0.0f ? juce::Colour(0xffffa834) : juce::Colour(0xff5ec4ff));
            g.drawText(bendTxt, static_cast<int>(changerRect.getX() + 1), static_cast<int>(sy - 6), 24, 12, juce::Justification::centred, false);
        }
    }

    // Draw Animated 3/4" Solid Chrome Tonebar at current fret position
    if (status.activeVoiceCount > 0) {
        const float clampedFret = juce::jlimit(0.0f, 24.0f, status.barPositionFret);
        const float barX = nutX + (clampedFret / 24.0f) * scaleWidth;
        juce::Rectangle<float> barRect(barX - 7.0f, strTop - 5.0f, 14.0f, (strBot - strTop) + 10.0f);
        juce::ColourGradient barGrad(juce::Colour(0xffffffff), barRect.getX(), barRect.getY(),
                                     juce::Colour(0xff778088), barRect.getRight(), barRect.getY(), false);
        g.setGradientFill(barGrad);
        g.fillRoundedRectangle(barRect, 6.0f);
        g.setColour(juce::Colour(0xffffaa33));
        g.drawRoundedRectangle(barRect, 6.0f, 1.5f);
    }

    // Draw Interactive Floor Pedals & Knee Levers Strip at bottom of visualizer
    static const char* kPedalTitles[6] = {
        "PEDAL A (+2 B->C#)",
        "A + B PEDALS (I->IV)",
        "PEDAL B (+1 G#->A)",
        "PEDAL C (+2 E/B)",
        "LKL (+1 E->F)",
        "LKR (-1 E->D#)"
    };
    const float pedalValues[6] = {
        status.pedalA,
        std::min(status.pedalA, status.pedalB),
        status.pedalB,
        status.pedalC,
        status.leverLKL,
        status.leverLKR
    };

    const float pWidth = (pedalStrip.getWidth() - 160.0f) / 6.0f;
    for (int i = 0; i < 6; ++i) {
        auto r = juce::Rectangle<float>(pedalStrip.getX() + i * pWidth + 3.0f, pedalStrip.getY() + 2.0f, pWidth - 6.0f, pedalStrip.getHeight() - 4.0f);
        pedalRects[static_cast<size_t>(i)] = r;
        const float val = juce::jlimit(0.0f, 1.0f, pedalValues[i]);

        g.setColour(juce::Colour(0xff1a1512));
        g.fillRoundedRectangle(r, 5.0f);
        if (val > 0.01f) {
            g.setColour(juce::Colour(0xffe59b3c).withAlpha(0.25f + 0.60f * val));
            g.fillRoundedRectangle(r.withHeight(r.getHeight() * val).withBottomY(r.getBottom()), 5.0f);
        }
        g.setColour(val > 0.1f ? juce::Colour(0xffffb84d) : juce::Colour(0xff6b5844));
        g.drawRoundedRectangle(r, 5.0f, 1.4f);

        g.setColour(juce::Colour(0xfff5ead6));
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText(kPedalTitles[i], r.toNearestInt(), juce::Justification::centred, false);
    }

    // Volume Swell Meter on right of pedal strip
    auto volRect = juce::Rectangle<float>(pedalStrip.getRight() - 150.0f, pedalStrip.getY() + 2.0f, 147.0f, pedalStrip.getHeight() - 4.0f);
    g.setColour(juce::Colour(0xff1a1512));
    g.fillRoundedRectangle(volRect, 5.0f);
    const float vFill = juce::jlimit(0.0f, 1.0f, status.volumePedal);
    g.setColour(juce::Colour(0xff52b788).withAlpha(0.75f));
    g.fillRoundedRectangle(volRect.withWidth(volRect.getWidth() * vFill), 5.0f);
    g.setColour(juce::Colour(0xff6b5844));
    g.drawRoundedRectangle(volRect, 5.0f, 1.4f);
    g.setColour(juce::Colour(0xfff5ead6));
    g.drawText("OPTICAL SWELL: " + juce::String(static_cast<int>(vFill * 100.0f)) + "%", volRect.toNearestInt(), juce::Justification::centred, false);
}

//==============================================================================
PedalSteelAudioProcessorEditor::PedalSteelAudioProcessorEditor(PedalSteelAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      fretboard(p),
      midiKeyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&lookAndFeel);
    setSize(1060, 700);

    // Preset ComboBox
    const auto& presets = getPedalSteelPresets();
    for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
        presetBox.addItem(presets[static_cast<size_t>(i)].name, i + 1);
    }
    presetBox.setSelectedId(audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] {
        int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0) audioProcessor.loadPreset(idx);
    };
    addAndMakeVisible(presetBox);

    // Voice Style & Play Mode ComboBoxes
    styleBox.addItemList({ "Sweet Georgia E9", "Hedonist Bootgaze", "Mikey's Grit Tweed", "Montana Lush Swell" }, 1);
    addAndMakeVisible(styleBox);
    styleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, ParamIDs::presetStyle, styleBox);

    modeBox.addItemList({ "Smart Auto-Pedal E9", "Crying Lead Gliss", "Lush Swell Pad", "Manual Copedent" }, 1);
    addAndMakeVisible(modeBox);
    modeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, ParamIDs::playMode, modeBox);

    // 4 Instant Audition Chord Buttons
    static const char* kChordLabels[4] = { "Audition E Maj", "Audition G Maj", "Audition D Maj", "Audition C Maj" };
    for (int i = 0; i < 4; ++i) {
        chordButtons[static_cast<size_t>(i)].setButtonText(kChordLabels[i]);
        chordButtons[static_cast<size_t>(i)].onClick = [this, i] {
            audioProcessor.triggerChordAudition(i);
        };
        addAndMakeVisible(chordButtons[static_cast<size_t>(i)]);
    }

    addAndMakeVisible(fretboard);

    // Setup 20 Rotary Knobs
    struct KnobSpec { const char* id; const char* title; };
    static constexpr KnobSpec kSpecs[20] = {
        // Panel 1: Copedent & Tonebar Articulation (7 knobs)
        { ParamIDs::scoopDepth,    "AUTO SCOOP" },
        { ParamIDs::pedalSpeed,    "PEDAL SPEED" },
        { ParamIDs::barGlide,      "BAR GLIDE" },
        { ParamIDs::vibratoDepth,  "VIB DEPTH" },
        { ParamIDs::vibratoRate,   "VIB RATE" },
        { ParamIDs::autoSwell,     "AUTO SWELL" },
        { ParamIDs::swellTime,     "SWELL TIME" },

        // Panel 2: E9th Pickup, String & Cabinet Physics (7 knobs)
        { ParamIDs::sampleBlend,   "REF DNA BLEND" },
        { ParamIDs::pickSnap,      "PICK & BAR" },
        { ParamIDs::pickupPos,     "PICKUP POS" },
        { ParamIDs::coilPeakFreq,  "COIL PEAK HZ" },
        { ParamIDs::coilResonance, "COIL SING Q" },
        { ParamIDs::sustainTime,   "SUSTAIN" },
        { ParamIDs::cabinetDrop,   "CABINET SYMP" },

        // Panel 3: Amp, 15" Speaker & Studio Reverb (6 knobs)
        { ParamIDs::volumePedal,   "VOL PEDAL" },
        { ParamIDs::ampDrive,      "TWEED DRIVE" },
        { ParamIDs::toneWarmth,    "15\" WARMTH" },
        { ParamIDs::reverbMix,     "SPRING+PLATE" },
        { ParamIDs::reverbDecay,   "REV DECAY" },
        { ParamIDs::outputLevel,   "OUTPUT dB" }
    };

    for (int i = 0; i < 20; ++i) {
        auto& k = knobs[static_cast<size_t>(i)];
        k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(k.slider);

        k.label.setText(kSpecs[i].title, juce::dontSendNotification);
        k.label.setJustificationType(juce::Justification::centred);
        k.label.setColour(juce::Label::textColourId, juce::Colour(0xfff0e2c8));
        k.label.setFont(juce::Font(11.0f, juce::Font::bold));
        addAndMakeVisible(k.label);

        k.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.apvts, kSpecs[i].id, k.slider);
    }

    // Playable bottom MIDI keyboard
    midiKeyboard.setAvailableRange(24, 96);
    midiKeyboard.setLowestVisibleKey(36);
    midiKeyboard.setKeyWidth(22.0f);
    addAndMakeVisible(midiKeyboard);

    startTimerHz(30);
}

PedalSteelAudioProcessorEditor::~PedalSteelAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void PedalSteelAudioProcessorEditor::timerCallback() {
    fretboard.updateStatus(audioProcessor.getLiveStatus());
}

void PedalSteelAudioProcessorEditor::paint(juce::Graphics& g) {
    // Rich dark studio chassis background
    juce::ColourGradient bg(juce::Colour(0xff1a1512), 0.0f, 0.0f,
                            juce::Colour(0xff0e0b09), 0.0f, static_cast<float>(getHeight()), false);
    g.setGradientFill(bg);
    g.fillAll();

    // Top Header Banner
    auto headerRect = juce::Rectangle<float>(12.0f, 8.0f, static_cast<float>(getWidth() - 24), 48.0f);
    g.setColour(juce::Colour(0xff261e18));
    g.fillRoundedRectangle(headerRect, 6.0f);
    g.setColour(juce::Colour(0xff7a5c3c));
    g.drawRoundedRectangle(headerRect, 6.0f, 1.2f);

    g.setColour(juce::Colour(0xffffb84d));
    g.setFont(juce::Font(18.5f, juce::Font::bold));
    g.drawText("NVA PEDAL STEEL", 22, 12, 210, 24, juce::Justification::centredLeft, false);
    g.setColour(juce::Colour(0xffbfb099));
    g.setFont(juce::Font(10.5f, juce::Font::plain));
    g.drawText("10-STRING E9th COPEDENT & REFERENCE DNA WORKSTATION", 22, 33, 290, 16, juce::Justification::centredLeft, false);

    // Draw 3 Brushed Hardware Module Panels
    const float panelY = 308.0f;
    const float panelH = 282.0f;
    const float panelW1 = 362.0f;
    const float panelW2 = 362.0f;
    const float panelW3 = static_cast<float>(getWidth()) - 24.0f - panelW1 - panelW2 - 16.0f;

    juce::Rectangle<float> p1(12.0f, panelY, panelW1, panelH);
    juce::Rectangle<float> p2(12.0f + panelW1 + 8.0f, panelY, panelW2, panelH);
    juce::Rectangle<float> p3(12.0f + panelW1 + panelW2 + 16.0f, panelY, panelW3, panelH);

    auto drawPanel = [&](juce::Rectangle<float> r, const char* title) {
        g.setColour(juce::Colour(0xff211a15));
        g.fillRoundedRectangle(r, 7.0f);
        g.setColour(juce::Colour(0xff634c36));
        g.drawRoundedRectangle(r, 7.0f, 1.3f);

        auto banner = r.removeFromTop(26.0f);
        g.setColour(juce::Colour(0xff31251c));
        g.fillRoundedRectangle(banner, 7.0f);
        g.setColour(juce::Colour(0xffe59b3c));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText(title, banner.toNearestInt(), juce::Justification::centred, false);
    };

    drawPanel(p1, "I. COPEDENT PEDALS & TONEBAR ARTICULATION");
    drawPanel(p2, "II. E9th PICKUP, STRING & CABINET DNA");
    drawPanel(p3, "III. SWELL PEDAL, TWEED AMP & STUDIO SPACE");
}

void PedalSteelAudioProcessorEditor::resized() {
    // Top bar layout
    presetBox.setBounds(316, 18, 222, 28);
    styleBox.setBounds(544, 18, 142, 28);
    modeBox.setBounds(692, 18, 148, 28);

    int btnX = 848;
    for (int i = 0; i < 4; ++i) {
        chordButtons[static_cast<size_t>(i)].setBounds(btnX + i * 50, 18, 48, 28);
        chordButtons[static_cast<size_t>(i)].setButtonText(i == 0 ? "E Maj" : i == 1 ? "G Maj" : i == 2 ? "D Maj" : "C Maj");
    }

    // Center Fretboard & Copedent Visualizer
    fretboard.setBounds(12, 62, getWidth() - 24, 238);

    // Layout knobs in the 3 panels
    const int panelY = 308;
    const int panelW1 = 362;
    const int panelW2 = 362;
    const int panelW3 = getWidth() - 24 - panelW1 - panelW2 - 16;

    auto layoutPanelKnobs = [&](int startIdx, int count, int px, int pw) {
        const int cols = (count <= 6) ? 3 : 4;
        const int cellW = pw / cols;
        const int rowH = 118;
        for (int i = 0; i < count; ++i) {
            int r = i / cols;
            int c = i % cols;
            int cx = px + c * cellW;
            int cy = panelY + 34 + r * rowH;
            auto& k = knobs[static_cast<size_t>(startIdx + i)];
            k.label.setBounds(cx + 2, cy, cellW - 4, 16);
            k.slider.setBounds(cx + 6, cy + 16, cellW - 12, 92);
        }
    };

    layoutPanelKnobs(0, 7, 12, panelW1);
    layoutPanelKnobs(7, 7, 12 + panelW1 + 8, panelW2);
    layoutPanelKnobs(14, 6, 12 + panelW1 + panelW2 + 16, panelW3);

    // Bottom Playable MIDI Keyboard
    midiKeyboard.setBounds(12, 598, getWidth() - 24, 92);
}
