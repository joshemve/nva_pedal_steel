#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
    inline juce::Font makeFont(float height, int styleFlags = juce::Font::plain) {
        return juce::Font(juce::FontOptions(height, styleFlags));
    }

    inline bool isOpenE9StringMidi(int midiNote) {
        static constexpr int kOpen[10] = { 47, 50, 52, 54, 56, 59, 63, 64, 66, 68 };
        for (int m : kOpen) if (m == midiNote) return true;
        return false;
    }

    inline const char* getKeyswitchShortLabel(int midiNote, bool phraseKsActive) {
        switch (midiNote) {
            case 24: return "A";
            case 25: return "C";
            case 26: return "B";
            case 28: return "AB";
            case 29: return "LK";
            case 31: return "LR";
            case 36: return phraseKsActive ? "L1" : nullptr;
            case 38: return phraseKsActive ? "L2" : nullptr;
            case 40: return phraseKsActive ? "L3" : nullptr;
            case 41: return phraseKsActive ? "L4" : nullptr;
            case 43: return phraseKsActive ? "LP" : nullptr;
            default: return nullptr;
        }
    }

    inline const char* getPedalTargetIdForButton(int btnIdx) {
        static constexpr const char* kIds[6] = {
            ParamIDs::pedalA, "pedalAB", ParamIDs::pedalB, ParamIDs::pedalC, ParamIDs::leverLKL, ParamIDs::leverLKR
        };
        return (btnIdx >= 0 && btnIdx < 6) ? kIds[btnIdx] : "";
    }

    static constexpr const char* kCharModeNames[37] = {
        "Studio Standard",
        "4-Track Cassette",
        "Ribbon Opto Velvet",
        "Analog Phaser",
        "Spring + Bias Trem",
        "Underwater R&B",
        "Vintage Warble",
        "Apollo Shimmer",
        "Leslie 147 Rotary",
        "Gated Diode Bloom",
        "Standel 807 Chamber",
        "Bosstone Violin Fuzz",
        "Talk-Box Vowel",
        "1937 Bakelite Lap",
        "Abbey Road Tape ADT",
        "Dimension D Chorus",
        "Poly-Octave Organ",
        "Harmonic-Trem Noir",
        "Revolver Reverse Tape",
        "Mohan Veena Jawari",
        "Neo-Soul CE-1 Syrup",
        "Binson Echorec Drum",
        "CS-80 Synth-Steel",
        "SPX90 Reverse-Glide",
        "Kingston Dub-Tank",
        "Microcosm Crystal",
        "Dumble H910 Lead",
        "Cinecitta Twang",
        "Khruangbin Cry-Spring",
        "VHS Generation-Drift",
        "Mercury7 Sub-Cathedra",
        "Golden Hour 224XL Silk",
        "Mellotron M400 Flute-Str",
        "Maggot Brain Octavia Fuzz",
        "'72 Suitcase Rhodes & Dyno",
        "Blonde White-Ferrari Prism",
        "'77 Rumours Pianet Flanger"
    };
}

//==============================================================================
PedalSteelLookAndFeel::PedalSteelLookAndFeel() {
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1d1814));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xfff3e8d2));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff6b533b));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1d1814));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xfff3e8d2));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffb87b31));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a211b));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffc9822b));
    setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0dec2));
    setColour(juce::TextButton::textColourOnId, juce::Colour(0xff140f0a));
}

void PedalSteelLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float rotaryStartAngle,
                                             float rotaryEndAngle, juce::Slider& slider) {
    const float radius = static_cast<float>(juce::jmin(width / 2, height / 2)) - 6.0f;
    const float centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f - 4.0f;
    const float rx = centreX - radius;
    const float ry = centreY - radius;
    const float rw = radius * 2.0f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const bool isModified = static_cast<bool>(slider.getProperties().getWithDefault("modified", false));
    const bool isLearning = static_cast<bool>(slider.getProperties().getWithDefault("learning", false));
    const juce::String ccTag = slider.getProperties().getWithDefault("ccTag", "").toString();

    // Outer value arc track
    juce::Path bgArc;
    bgArc.addCentredArc(centreX, centreY, radius + 2.5f, radius + 2.5f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(isLearning ? juce::Colour(0xff205d66) : juce::Colour(0xff2c231d));
    g.strokePath(bgArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Active warm amber (or cyan MIDI-Learn) value arc
    juce::Path valArc;
    valArc.addCentredArc(centreX, centreY, radius + 2.5f, radius + 2.5f, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(isLearning ? juce::Colour(0xff4cd4dc) : juce::Colour(0xffe59b3c));
    g.strokePath(valArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Subtle glowing amber dot when knob is tweaked away from preset baseline
    if (isModified) {
        g.setColour(juce::Colour(0xffffb84d));
        g.fillEllipse(static_cast<float>(x + width - 9), static_cast<float>(y + 2), 5.0f, 5.0f);
    }

    // Brushed chrome & knurled vintage Peavey/Fender skirted knob body
    juce::ColourGradient bodyGrad(juce::Colour(0xff4c4640), centreX - radius * 0.5f, centreY - radius * 0.5f,
                                  juce::Colour(0xff1b1815), centreX + radius * 0.6f, centreY + radius * 0.6f, true);
    g.setGradientFill(bodyGrad);
    g.fillEllipse(rx, ry, rw, rw);

    // Chrome inner cap
    const float capR = radius * 0.65f;
    juce::ColourGradient capGrad(juce::Colour(0xffd8cfc0), centreX - capR * 0.4f, centreY - capR * 0.4f,
                                 juce::Colour(0xff6e655c), centreX + capR * 0.5f, centreY + capR * 0.5f, true);
    g.setGradientFill(capGrad);
    g.fillEllipse(centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f);

    g.setColour(isLearning ? juce::Colour(0xff4cd4dc) : juce::Colour(0xff14110e));
    g.drawEllipse(rx, ry, rw, rw, 1.4f);

    // Pointer indicator line
    juce::Path p;
    const float pointerLength = radius * 0.85f;
    const float pointerThickness = 2.5f;
    p.addRoundedRectangle(-pointerThickness * 0.5f, -radius + 2.0f, pointerThickness, pointerLength * 0.58f, 1.0f);
    p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    g.setColour(juce::Colour(0xffffdca0));
    g.fillPath(p);

    // Value readout (or MIDI Learn / CC badge) at bottom of knob
    if (isLearning) {
        g.setColour(juce::Colour(0xff4cd4dc));
        g.setFont(makeFont(9.5f, juce::Font::bold));
        g.drawText("LEARN CC...", x, y + height - 14, width, 14, juce::Justification::centred, false);
    } else {
        juce::String valText = slider.getTextFromValue(slider.getValue());
        if (ccTag.isNotEmpty()) valText += " [" + ccTag + "]";
        g.setColour(ccTag.isNotEmpty() ? juce::Colour(0xff9ee8ef) : juce::Colour(0xffd8c7ab));
        g.setFont(makeFont(ccTag.isNotEmpty() ? 9.5f : 10.5f, juce::Font::bold));
        g.drawText(valText, x, y + height - 14, width, 14, juce::Justification::centred, false);
    }
}

void PedalSteelLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                           bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/) {
    const juce::String txt = button.getButtonText();
    const float fontSize = (txt.length() <= 2) ? 12.0f : 10.2f;
    g.setFont(makeFont(fontSize, juce::Font::bold));
    g.setColour(button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                          : juce::TextButton::textColourOffId)
                      .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.55f));
    g.drawFittedText(txt, button.getLocalBounds().reduced(2, 1), juce::Justification::centred, 1, 0.85f);
}

//==============================================================================
void PedalSteelKeyboardComponent::drawWhiteNote(int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                                                bool isDown, bool isOver, juce::Colour /*lineColour*/, juce::Colour /*textColour*/) {
    const bool isPedalKs  = (midiNoteNumber >= 24 && midiNoteNumber <= 31);
    const bool isPhraseKs = phraseKsActive && (midiNoteNumber >= 36 && midiNoteNumber <= 43);

    juce::Colour baseCol = juce::Colour(0xfff5efe4);
    if (isPedalKs)       baseCol = juce::Colour(0xff6b4722);
    else if (isPhraseKs) baseCol = juce::Colour(0xff234c54);
    else if (midiNoteNumber < 36) baseCol = juce::Colour(0xffcfc5b4);

    if (isDown)      baseCol = isPhraseKs ? juce::Colour(0xff4cd4dc) : juce::Colour(0xffffa834);
    else if (isOver) baseCol = baseCol.brighter(0.14f);

    juce::ColourGradient grad(baseCol.darker(0.08f), area.getX(), area.getY(),
                              baseCol, area.getX(), area.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRect(area);

    g.setColour(juce::Colour(0xff261f1a));
    g.drawRect(area, 0.8f);

    if (!isPedalKs && !isPhraseKs && isOpenE9StringMidi(midiNoteNumber)) {
        g.setColour(juce::Colour(0xffd48728).withAlpha(0.75f));
        g.fillEllipse(area.getCentreX() - 2.5f, area.getBottom() - 23.0f, 5.0f, 5.0f);
    }

    if (const char* ksLbl = getKeyswitchShortLabel(midiNoteNumber, phraseKsActive)) {
        g.setColour(isDown ? juce::Colour(0xff14100c) : juce::Colour(0xffffeed2));
        g.setFont(makeFont(10.0f, juce::Font::bold));
        g.drawText(ksLbl, area.withTrimmedTop(area.getHeight() - 28.0f).withHeight(14.0f).toNearestInt(), juce::Justification::centred, false);
    }

    if (midiNoteNumber % 12 == 0) {
        const int oct = (midiNoteNumber / 12) - 2;
        g.setColour((isPedalKs || isPhraseKs) ? juce::Colour(0xffdfd2bc) : juce::Colour(0xff524538));
        g.setFont(makeFont(9.5f, juce::Font::bold));
        g.drawText("C" + juce::String(oct), area.withTrimmedTop(area.getHeight() - 14.0f).toNearestInt(), juce::Justification::centred, false);
    }
}

void PedalSteelKeyboardComponent::drawBlackNote(int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                                                bool isDown, bool isOver, juce::Colour /*noteFillColour*/) {
    const bool isPedalKs  = (midiNoteNumber >= 24 && midiNoteNumber <= 31);
    const bool isPhraseKs = phraseKsActive && (midiNoteNumber >= 36 && midiNoteNumber <= 43);

    juce::Colour col = juce::Colour(0xff1e1915);
    if (isPedalKs)       col = juce::Colour(0xff3f2812);
    else if (isPhraseKs) col = juce::Colour(0xff152e34);

    if (isDown)      col = isPhraseKs ? juce::Colour(0xff3dc2ca) : juce::Colour(0xffe59b3c);
    else if (isOver) col = col.brighter(0.22f);

    g.setColour(col);
    g.fillRoundedRectangle(area, 2.0f);
    g.setColour(juce::Colour(0xff0b0907));
    g.drawRoundedRectangle(area, 2.0f, 1.0f);

    if (!isPedalKs && !isPhraseKs && isOpenE9StringMidi(midiNoteNumber)) {
        g.setColour(juce::Colour(0xffe59b3c).withAlpha(0.85f));
        g.fillEllipse(area.getCentreX() - 2.2f, area.getBottom() - 10.0f, 4.4f, 4.4f);
    }

    if (const char* ksLbl = getKeyswitchShortLabel(midiNoteNumber, phraseKsActive)) {
        g.setColour(isDown ? juce::Colour(0xff120e0a) : juce::Colour(0xffffdca0));
        g.setFont(makeFont(9.0f, juce::Font::bold));
        g.drawText(ksLbl, area.withTrimmedTop(area.getHeight() - 15.0f).toNearestInt(), juce::Justification::centred, false);
    }
}

//==============================================================================
FretboardVisualizer::FretboardVisualizer(PedalSteelAudioProcessor& proc)
    : processor(proc) {}

void FretboardVisualizer::updateStatus(const CopedentStatus& st) {
    status = st;
    repaint();
}

void FretboardVisualizer::setPatchInfoText(const juce::String& info) {
    if (patchInfoText != info) {
        patchInfoText = info;
        repaint();
    }
}

void FretboardVisualizer::clearLatchedPedals() {
    bool anyWasLatched = false;
    for (bool& b : latchedUiPedals) {
        if (b) { b = false; anyWasLatched = true; }
    }
    if (anyWasLatched) {
        syncPedalsFromUiState(false, false);
        repaint();
    }
}

void FretboardVisualizer::syncPedalsFromUiState(bool beginGesture, bool endGesture) {
    float p[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    auto applyBtn = [&](int btnIdx) {
        if (btnIdx == 0) p[0] = 1.0f;
        else if (btnIdx == 1) { p[0] = 1.0f; p[1] = 1.0f; }
        else if (btnIdx == 2) p[1] = 1.0f;
        else if (btnIdx == 3) p[2] = 1.0f;
        else if (btnIdx == 4) p[3] = 1.0f;
        else if (btnIdx == 5) p[4] = 1.0f;
    };
    for (int i = 0; i < 6; ++i) {
        if (latchedUiPedals[static_cast<size_t>(i)]) applyBtn(i);
    }
    if (heldUiPedal >= 0 && heldUiPedal < 6) {
        applyBtn(heldUiPedal);
    }
    for (int k = 0; k < 5; ++k) {
        processor.setLivePedalState(k, p[k], beginGesture, endGesture);
    }
}

void FretboardVisualizer::setOpticalSwellFromMouseX(float mouseX) {
    if (cachedVolRect.getWidth() <= 4.0f) return;
    const float norm = juce::jlimit(0.0f, 1.0f, (mouseX - cachedVolRect.getX()) / cachedVolRect.getWidth());
    if (auto* param = processor.apvts.getParameter(ParamIDs::volumePedal)) {
        param->setValueNotifyingHost(norm);
    }
}

void FretboardVisualizer::showPedalContextMenu(int buttonIndex) {
    const juce::String targetId = getPedalTargetIdForButton(buttonIndex);
    const bool isLatched  = latchedUiPedals[static_cast<size_t>(buttonIndex)];
    const bool isLearning = processor.isMidiLearningTarget(targetId);
    const int assignedCc  = processor.getAssignedMidiCc(targetId);

    juce::PopupMenu m;
    m.addItem(1, isLatched ? "Unlatch Pedal (Shift+Click)" : "Latch Pedal ON (Shift+Click)", true, isLatched);
    m.addSeparator();
    m.addItem(2, isLearning ? "Cancel MIDI CC Learn" : "Learn MIDI CC...", true, isLearning);
    if (assignedCc >= 0) {
        m.addItem(3, "Clear MIDI CC (CC " + juce::String(assignedCc) + ")");
    }

    juce::Component::SafePointer<FretboardVisualizer> safeThis(this);
    m.showMenuAsync(juce::PopupMenu::Options(), [safeThis, buttonIndex, targetId, isLearning](int res) {
        if (safeThis == nullptr || res == 0) return;
        if (res == 1) {
            auto& latch = safeThis->latchedUiPedals[static_cast<size_t>(buttonIndex)];
            latch = !latch;
            safeThis->syncPedalsFromUiState(true, true);
            safeThis->repaint();
        } else if (res == 2) {
            if (isLearning) safeThis->processor.cancelMidiLearn();
            else            safeThis->processor.startMidiLearn(targetId);
            safeThis->repaint();
        } else if (res == 3) {
            safeThis->processor.clearMidiCcForTarget(targetId);
            safeThis->repaint();
        }
    });
}

void FretboardVisualizer::showOpticalSwellContextMenu() {
    const juce::String targetId = ParamIDs::volumePedal;
    const bool isLearning = processor.isMidiLearningTarget(targetId);
    const int assignedCc  = processor.getAssignedMidiCc(targetId);

    juce::PopupMenu m;
    m.addItem(1, "Reset Optical Swell to Preset Default");
    m.addSeparator();
    m.addItem(2, isLearning ? "Cancel MIDI CC Learn" : "Learn MIDI CC...", true, isLearning);
    if (assignedCc >= 0) {
        m.addItem(3, "Clear MIDI CC (CC " + juce::String(assignedCc) + ")");
    }

    juce::Component::SafePointer<FretboardVisualizer> safeThis(this);
    m.showMenuAsync(juce::PopupMenu::Options(), [safeThis, targetId, isLearning](int res) {
        if (safeThis == nullptr || res == 0) return;
        if (res == 1) {
            if (auto* p = safeThis->processor.apvts.getParameter(ParamIDs::volumePedal)) {
                const float base = safeThis->processor.getBaselineParamValue(ParamIDs::volumePedal);
                p->setValueNotifyingHost(p->convertTo0to1(base));
            }
        } else if (res == 2) {
            if (isLearning) safeThis->processor.cancelMidiLearn();
            else            safeThis->processor.startMidiLearn(targetId);
        } else if (res == 3) {
            safeThis->processor.clearMidiCcForTarget(targetId);
        }
    });
}

void FretboardVisualizer::mouseDown(const juce::MouseEvent& e) {
    const auto pt = e.position;

    for (int i = 0; i < 6; ++i) {
        if (pedalRects[static_cast<size_t>(i)].contains(pt)) {
            if (e.mods.isPopupMenu()) {
                showPedalContextMenu(i);
                return;
            }
            if (e.mods.isShiftDown()) {
                latchedUiPedals[static_cast<size_t>(i)] = !latchedUiPedals[static_cast<size_t>(i)];
                syncPedalsFromUiState(true, true);
                repaint();
                return;
            }
            if (latchedUiPedals[static_cast<size_t>(i)]) {
                latchedUiPedals[static_cast<size_t>(i)] = false;
                syncPedalsFromUiState(true, true);
                repaint();
                return;
            }
            heldUiPedal = i;
            syncPedalsFromUiState(true, false);
            repaint();
            return;
        }
    }

    if (cachedVolRect.contains(pt)) {
        if (e.mods.isPopupMenu()) {
            showOpticalSwellContextMenu();
            return;
        }
        draggingVolPedal = true;
        if (auto* param = processor.apvts.getParameter(ParamIDs::volumePedal)) {
            param->beginChangeGesture();
        }
        setOpticalSwellFromMouseX(pt.x);
        repaint();
        return;
    }

    // Interactive 10-String Fretboard Pluck & Continuous Tonebar Slide!
    if (cachedNeckArea.contains(pt) && cachedScaleWidth > 10.0f) {
        const int sIdx = juce::jlimit(0, 9, static_cast<int>(std::round((pt.y - cachedStrTop) / std::max(1.0f, cachedStrStep))));
        const float fret = juce::jlimit(0.0f, 24.0f, ((pt.x - cachedNutX) / cachedScaleWidth) * 24.0f);
        draggedString = sIdx;
        processor.triggerFretboardNote(sIdx, fret);
    }
}

void FretboardVisualizer::mouseDrag(const juce::MouseEvent& e) {
    if (draggingVolPedal) {
        setOpticalSwellFromMouseX(e.position.x);
        repaint();
        return;
    }
    if (draggedString >= 0 && cachedScaleWidth > 10.0f) {
        const float fret = juce::jlimit(0.0f, 24.0f, ((e.position.x - cachedNutX) / cachedScaleWidth) * 24.0f);
        processor.slideFretboardNote(draggedString, fret);
    }
}

void FretboardVisualizer::mouseUp(const juce::MouseEvent&) {
    if (draggingVolPedal) {
        draggingVolPedal = false;
        if (auto* param = processor.apvts.getParameter(ParamIDs::volumePedal)) {
            param->endChangeGesture();
        }
        repaint();
    }
    if (heldUiPedal >= 0) {
        heldUiPedal = -1;
        syncPedalsFromUiState(false, true);
        repaint();
    }
    if (draggedString >= 0) {
        processor.releaseFretboardNote(draggedString);
        draggedString = -1;
    }
}

void FretboardVisualizer::mouseDoubleClick(const juce::MouseEvent& e) {
    if (cachedVolRect.contains(e.position)) {
        if (auto* param = processor.apvts.getParameter(ParamIDs::volumePedal)) {
            const float base = processor.getBaselineParamValue(ParamIDs::volumePedal);
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1(base));
            param->endChangeGesture();
        }
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

    auto neckArea = bounds.reduced(14.0f, 10.0f);
    auto pedalStrip = neckArea.removeFromBottom(42.0f);
    neckArea.removeFromBottom(6.0f);
    cachedNeckArea = neckArea;

    // Dark rosewood/ebony fretboard with pearloid position markers
    g.setColour(juce::Colour(0xff120f0d));
    g.fillRoundedRectangle(neckArea, 5.0f);
    g.setColour(juce::Colour(0xff524538));
    g.drawRoundedRectangle(neckArea, 5.0f, 1.2f);

    const float nutX = neckArea.getX() + 62.0f;
    const float pickupX = neckArea.getRight() - 62.0f;
    const float scaleWidth = std::max(40.0f, pickupX - nutX - 24.0f);
    cachedNutX = nutX;
    cachedScaleWidth = scaleWidth;

    static constexpr int kMarkerFrets[] = { 3, 5, 7, 9, 12, 15, 17, 19, 24 };
    g.setFont(makeFont(10.0f, juce::Font::bold));
    for (int f = 0; f <= 24; ++f) {
        const float normX = static_cast<float>(f) / 24.0f;
        const float fx = nutX + normX * scaleWidth;
        bool isMarker = false;
        for (int mf : kMarkerFrets) if (mf == f) isMarker = true;

        g.setColour(f == 0 ? juce::Colour(0xffd9ceb8) : (isMarker ? juce::Colour(0xff7a6a58) : juce::Colour(0xff362f29)));
        g.drawLine(fx, neckArea.getY() + 15.0f, fx, neckArea.getBottom() - 15.0f, f == 0 ? 3.0f : 1.2f);

        if (isMarker || f == 0) {
            g.setColour(juce::Colour(0xffa89882));
            g.drawText(juce::String(f), static_cast<int>(fx - 10), static_cast<int>(neckArea.getBottom() - 14), 20, 13, juce::Justification::centred, false);
        }
    }

    // Active preset signal-chain summary on top-left of neck margin + interactive hint on top-right
    if (patchInfoText.isNotEmpty()) {
        const int maxInfoW = std::max(120, static_cast<int>(pickupX - nutX - 305.0f));
        g.setColour(juce::Colour(0xffcdb898));
        g.setFont(makeFont(9.5f, juce::Font::bold));
        g.drawText(patchInfoText, static_cast<int>(nutX + 4.0f), static_cast<int>(neckArea.getY() + 2.0f),
                   maxInfoW, 12, juce::Justification::centredLeft, true);
    }

    g.setColour(juce::Colour(0xff9c8972));
    g.setFont(makeFont(9.0f, juce::Font::bold));
    g.drawText("SHIFT/RIGHT-CLICK PEDALS: LATCH & CC LEARN | DRAG STRINGS",
               static_cast<int>(pickupX - 300.0f), static_cast<int>(neckArea.getY() + 2.0f), 286, 12, juce::Justification::centredRight, false);

    // High-Inductance Single-Coil Pickup & All-Pull Changer Bridge
    juce::Rectangle<float> pickupRect(pickupX - 8.0f, neckArea.getY() + 14.0f, 18.0f, neckArea.getHeight() - 28.0f);
    g.setColour(juce::Colour(0xff252220));
    g.fillRoundedRectangle(pickupRect, 4.0f);
    g.setColour(juce::Colour(0xff9c8e7b));
    g.drawRoundedRectangle(pickupRect, 4.0f, 1.2f);

    juce::Rectangle<float> changerRect(neckArea.getRight() - 34.0f, neckArea.getY() + 12.0f, 26.0f, neckArea.getHeight() - 26.0f);
    juce::ColourGradient chromeGrad(juce::Colour(0xffd0cbc2), changerRect.getX(), changerRect.getY(),
                                    juce::Colour(0xff5e5952), changerRect.getRight(), changerRect.getBottom(), false);
    g.setGradientFill(chromeGrad);
    g.fillRoundedRectangle(changerRect, 3.0f);

    static const char* kStringNamesE9[10] = {
        "1 F#4", "2 D#4", "3 G#4 (B)", "4 E4 (C/LK)", "5 B3 (A)",
        "6 G#3 (B)", "7 F#3", "8 E3 (LK)", "9 D3", "10 B2 (A)"
    };
    static const char* kStringNamesC6[10] = {
        "1 G4", "2 E4 (P6)", "3 C4 (P5)", "4 A3 (P7)", "5 G3 (P6)",
        "6 E3 (P6)", "7 C3 (P5)", "8 A2 (P7)", "9 F2", "10 C2"
    };
    static const char* kStringNamesB6[10] = {
        "1 C#4", "2 A#3", "3 D#4 (B)", "4 B3 (C/LK)", "5 F#3 (A)",
        "6 D#3 (B)", "7 C#3", "8 B2 (LK)", "9 A2", "10 F#2 (A)"
    };
    const char* const* kStringNames = (status.presetStyle == 4) ? kStringNamesC6
                                    : ((status.presetStyle == 5) ? kStringNamesB6 : kStringNamesE9);
    const float strTop = neckArea.getY() + 18.0f;
    const float strBot = neckArea.getBottom() - 20.0f;
    const float strStep = (strBot - strTop) / 9.0f;
    cachedStrTop = strTop;
    cachedStrStep = strStep;

    g.setFont(makeFont(10.0f, juce::Font::bold));
    for (int s = 0; s < 10; ++s) {
        const float sy = strTop + static_cast<float>(s) * strStep;
        const float energy = status.stringEnergy[static_cast<size_t>(s)];
        const float bend = status.stringBendSemis[static_cast<size_t>(s)];

        g.setColour(energy > 0.05f ? juce::Colour(0xffffb84d) : juce::Colour(0xff9e8f7c));
        g.drawText(kStringNames[s], static_cast<int>(neckArea.getX() + 4), static_cast<int>(sy - 6), 56, 12, juce::Justification::centredLeft, false);

        const float thickness = 1.0f + static_cast<float>(s) * 0.16f;
        if (energy > 0.02f) {
            g.setColour(juce::Colour(0xffe59b3c).withAlpha(juce::jlimit(0.0f, 0.85f, energy * 0.9f)));
            g.drawLine(nutX, sy, changerRect.getX(), sy, thickness + 3.5f * energy);
        }
        g.setColour(energy > 0.02f ? juce::Colour(0xfffff0d4) : juce::Colour(0xff8c8275));
        g.drawLine(nutX, sy, changerRect.getX(), sy, thickness);

        g.setColour(juce::Colour(0xffd0c8b8));
        g.fillEllipse(pickupRect.getCentreX() - 2.5f, sy - 2.5f, 5.0f, 5.0f);

        if (std::abs(bend) > 0.08f) {
            juce::String bendTxt = (bend > 0.0f ? "+" : "") + juce::String(bend, 1);
            g.setColour(bend > 0.0f ? juce::Colour(0xffffa834) : juce::Colour(0xff5ec4ff));
            g.drawText(bendTxt, static_cast<int>(changerRect.getX() + 1), static_cast<int>(sy - 6), 24, 12, juce::Justification::centred, false);
        }
    }

    // Animated 3/4" Solid Chrome Tonebar
    if (status.activeVoiceCount > 0) {
        const float clampedFret = juce::jlimit(0.0f, 24.0f, status.barPositionFret);
        const float barX = nutX + (clampedFret / 24.0f) * scaleWidth;
        juce::Rectangle<float> barRect(barX - 7.0f, strTop - 5.0f, 14.0f, (strBot - strTop) + 10.0f);
        juce::ColourGradient barGrad(juce::Colour(0xffffffff), barRect.getX(), barRect.getY(),
                                     juce::Colour(0xff778088), barRect.getRight(), barRect.getY(), false);
        g.setGradientFill(barGrad);
        g.fillRoundedRectangle(barRect, 6.0f);
        g.setColour(status.phrasePlaying ? juce::Colour(0xff4cd4dc) : juce::Colour(0xffffaa33));
        g.drawRoundedRectangle(barRect, 6.0f, 1.5f);
    }

    // Interactive Floor Pedals & Knee Levers Strip (Shift+Click / Right-Click Latch + MIDI CC Learn!)
    static const char* kPedalTitlesE9[6] = {
        "PEDAL A (+2 B->C#)",
        "A + B SLANT (I->IV)",
        "PEDAL B (+1 G#->A)",
        "PEDAL C (+2 E/B)",
        "LKL (+1 E->F)",
        "LKR (-1 E->D#)"
    };
    static const char* kPedalTitlesC6[6] = {
        "PEDAL 5 (+2 C->D)",
        "P5 + P7 (C6->C9)",
        "PEDAL 7 (+1 A->Bb)",
        "PEDAL 6 (-1 E->Eb)",
        "LKL (+1 C->C#)",
        "LKR (-1 C->B)"
    };
    const char* const* kPedalTitles = (status.presetStyle == 4) ? kPedalTitlesC6 : kPedalTitlesE9;
    const float pedalValues[6] = {
        status.pedalA,
        std::min(status.pedalA, status.pedalB),
        status.pedalB,
        status.pedalC,
        status.leverLKL,
        status.leverLKR
    };

    const float pWidth = (pedalStrip.getWidth() - 158.0f) / 6.0f;
    for (int i = 0; i < 6; ++i) {
        auto r = juce::Rectangle<float>(pedalStrip.getX() + i * pWidth + 2.5f, pedalStrip.getY() + 2.0f, pWidth - 5.0f, pedalStrip.getHeight() - 4.0f);
        pedalRects[static_cast<size_t>(i)] = r;
        const float val = juce::jlimit(0.0f, 1.0f, pedalValues[i]);
        const bool isLatched  = latchedUiPedals[static_cast<size_t>(i)];
        const juce::String targetId = getPedalTargetIdForButton(i);
        const bool isLearning = processor.isMidiLearningTarget(targetId);
        const int assignedCc  = processor.getAssignedMidiCc(targetId);

        g.setColour(juce::Colour(0xff1a1512));
        g.fillRoundedRectangle(r, 5.0f);
        if (val > 0.01f) {
            g.setColour(juce::Colour(0xffe59b3c).withAlpha(0.25f + 0.60f * val));
            g.fillRoundedRectangle(r.withHeight(r.getHeight() * val).withBottomY(r.getBottom()), 5.0f);
        }
        g.setColour(isLearning ? juce::Colour(0xff4cd4dc)
                               : ((isLatched || val > 0.1f) ? juce::Colour(0xffffb84d) : juce::Colour(0xff6b5844)));
        g.drawRoundedRectangle(r, 5.0f, isLearning ? 1.8f : 1.3f);

        if (isLatched) {
            g.setColour(juce::Colour(0xffffd166));
            g.fillEllipse(r.getRight() - 9.5f, r.getY() + 4.0f, 5.5f, 5.5f);
        }
        if (isLearning || assignedCc >= 0) {
            g.setColour(juce::Colour(0xff4cd4dc));
            g.setFont(makeFont(8.5f, juce::Font::bold));
            g.drawText(isLearning ? "LEARN" : ("CC" + juce::String(assignedCc)),
                       static_cast<int>(r.getX() + 4), static_cast<int>(r.getY() + 2), 42, 10, juce::Justification::centredLeft, false);
        }

        g.setColour(juce::Colour(0xfff5ead6));
        g.setFont(makeFont(10.5f, juce::Font::bold));
        g.drawText(kPedalTitles[i], r.toNearestInt(), juce::Justification::centred, false);
    }

    // Draggable Horizontal Goodrich Optical Volume Swell Bar!
    auto volRect = juce::Rectangle<float>(pedalStrip.getRight() - 152.0f, pedalStrip.getY() + 2.0f, 150.0f, pedalStrip.getHeight() - 4.0f);
    cachedVolRect = volRect;
    const bool volLearning = processor.isMidiLearningTarget(ParamIDs::volumePedal);
    const int volCc        = processor.getAssignedMidiCc(ParamIDs::volumePedal);

    g.setColour(juce::Colour(0xff1a1512));
    g.fillRoundedRectangle(volRect, 5.0f);
    const float vFill = juce::jlimit(0.0f, 1.0f, status.volumePedal);
    g.setColour(juce::Colour(0xff52b788).withAlpha(0.78f));
    g.fillRoundedRectangle(volRect.withWidth(volRect.getWidth() * vFill), 5.0f);
    g.setColour(volLearning ? juce::Colour(0xff4cd4dc) : (draggingVolPedal ? juce::Colour(0xff7ee8b2) : juce::Colour(0xff6b5844)));
    g.drawRoundedRectangle(volRect, 5.0f, volLearning ? 1.8f : 1.3f);

    juce::String swellLabel = volLearning
        ? "LEARN SWELL CC..."
        : ("OPTICAL SWELL: " + juce::String(static_cast<int>(std::round(vFill * 100.0f))) + "%"
           + (volCc >= 0 ? (" [CC" + juce::String(volCc) + "]") : ""));
    g.setColour(juce::Colour(0xfff5ead6));
    g.setFont(makeFont(volCc >= 0 ? 9.5f : 10.5f, juce::Font::bold));
    g.drawText(swellLabel, volRect.toNearestInt(), juce::Justification::centred, false);
}

//==============================================================================
PedalSteelAudioProcessorEditor::PedalSteelAudioProcessorEditor(PedalSteelAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      fretboard(p),
      midiKeyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    activeQwertyMidi.fill(-1);
    qwertyEnabled = (audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone);

    setLookAndFeel(&lookAndFeel);
    setWantsKeyboardFocus(true);
    setResizable(true, true);
    setResizeLimits(1060, 660, 1600, 980);
    setSize(1220, 720);

    // Preset Stepper, Revert & Categorized Factory/User Preset Menu
    prevPresetBtn.setTooltip("Previous Factory Preset");
    prevPresetBtn.onClick = [this] {
        fretboard.clearLatchedPedals();
        audioProcessor.stepPreset(-1);
        syncPresetComboSelection();
        updateKnobDoubleClickTargets();
        updatePresetInfoAndTooltips();
    };
    addAndMakeVisible(prevPresetBtn);

    nextPresetBtn.setTooltip("Next Factory Preset");
    nextPresetBtn.onClick = [this] {
        fretboard.clearLatchedPedals();
        audioProcessor.stepPreset(1);
        syncPresetComboSelection();
        updateKnobDoubleClickTargets();
        updatePresetInfoAndTooltips();
    };
    addAndMakeVisible(nextPresetBtn);

    revertBtn.setTooltip("Revert all tweaked knobs & modes back to the active preset's saved settings");
    revertBtn.onClick = [this] {
        fretboard.clearLatchedPedals();
        audioProcessor.revertToBaseline();
        syncPresetComboSelection();
        updateKnobDoubleClickTargets();
        updatePresetInfoAndTooltips();
    };
    addAndMakeVisible(revertBtn);

    rebuildPresetMenu();
    presetBox.onChange = [this] {
        const int id = presetBox.getSelectedId();
        const int numFactory = static_cast<int>(getPedalSteelPresets().size());
        if (id >= 1 && id <= numFactory) {
            fretboard.clearLatchedPedals();
            audioProcessor.loadPreset(id - 1);
            syncPresetComboSelection();
            updateKnobDoubleClickTargets();
            updatePresetInfoAndTooltips();
        } else if (id >= 501 && id < 501 + numFactory) {
            fretboard.clearLatchedPedals();
            audioProcessor.loadPreset(id - 501);
            syncPresetComboSelection();
            updateKnobDoubleClickTargets();
            updatePresetInfoAndTooltips();
        } else if (id >= 1000 && id < 1000 + static_cast<int>(cachedUserPresets.size())) {
            fretboard.clearLatchedPedals();
            const auto file = cachedUserPresets[static_cast<size_t>(id - 1000)];
            audioProcessor.loadUserPreset(file);
            rebuildPresetMenu();
            updateKnobDoubleClickTargets();
            updatePresetInfoAndTooltips();
        } else if (id == 9000) {
            fretboard.clearLatchedPedals();
            audioProcessor.revertToBaseline();
            syncPresetComboSelection();
            updateKnobDoubleClickTargets();
        } else if (id == 9001) {
            syncPresetComboSelection();
            promptSaveUserPreset();
        } else if (id == 9002) {
            syncPresetComboSelection();
            promptLoadUserPreset();
        } else if (id == 9003) {
            syncPresetComboSelection();
            PedalSteelAudioProcessor::getUserPresetsFolder().revealToUser();
        }
    };
    addAndMakeVisible(presetBox);

    // Voice Style, Performance Mode & Categorized Analog Character ComboBoxes
    styleBox.addItemList({
        "Sweet Georgia E9",
        "Hedonist Bootgaze",
        "Mikey's Grit Tweed",
        "Montana Lush Swell",
        "Western Swing C6th",
        "Baritone Canyon B6"
    }, 1);
    styleBox.setTooltip("Selects 10-String Neck Voicing & Studio Pedal-Steel DNA (E9th Front-Neck, Back-Neck C6th Swing, or Baritone Canyon B6)");
    addAndMakeVisible(styleBox);
    styleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, ParamIDs::presetStyle, styleBox);

    modeBox.addItemList({
        "Smart Auto-Pedal E9",
        "Crying Lead Gliss",
        "Lush Swell Pad",
        "Manual Copedent",
        "Studio Phrase Licks",
        "Palm Chime Harmonics"
    }, 1);
    modeBox.setTooltip("Performance Mode (including Fingered Auto-Oblique Chords, Overlapped Crying Lead Gliss, 12th-Fret Palm Chimes & Studio Phrase Licks)");
    addAndMakeVisible(modeBox);
    modeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, ParamIDs::playMode, modeBox);

    buildCategorizedCharacterMenu();
    charBox.setTooltip("Categorized Analog Character Circuits (37 Modes): 36 first-principles studio & vintage hardware circuits alongside Studio Standard — click-free 17 ms live crossfade!");
    charBox.onChange = [this] {
        if (ignoreCharComboCallback) return;
        const int id = charBox.getSelectedId();
        if (id >= 1 && id <= 37 && charParamAttach != nullptr) {
            charParamAttach->setValueAsCompleteGesture(static_cast<float>(id - 1));
        }
    };
    addAndMakeVisible(charBox);
    if (auto* fxParam = audioProcessor.apvts.getParameter(ParamIDs::fxCharacter)) {
        charParamAttach = std::make_unique<juce::ParameterAttachment>(
            *fxParam,
            [this](float plainVal) {
                const juce::ScopedValueSetter<bool> svs(ignoreCharComboCallback, true);
                const int targetId = juce::jlimit(1, 37, static_cast<int>(std::round(plainVal)) + 1);
                charBox.setSelectedId(targetId, juce::dontSendNotification);
            },
            nullptr);
        charParamAttach->sendInitialUpdate();
    }

    // Host Tempo Sync Button
    syncBtn.setClickingTogglesState(true);
    syncBtn.setTooltip("Locks Tape Delay Time (1/16, 1/8, 1/8D, 1/4) & Swell Pocket to DAW Host BPM");
    addAndMakeVisible(syncBtn);
    syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, ParamIDs::tempoSync, syncBtn);

    // Laptop QWERTY Musical Typing Toggle Button
    qwertyBtn.onClick = [this] {
        qwertyEnabled = !qwertyEnabled;
        if (!qwertyEnabled) releaseAllQwertyKeys();
        else grabKeyboardFocus();
        updateQwertyButtonAppearance();
    };
    updateQwertyButtonAppearance();
    addAndMakeVisible(qwertyBtn);

    // Play Studio Lick Audition Button
    lickBtn.setTooltip("Audition & cycle the 4 recorded 44.1 kHz stereo studio licks for the active style (click again to stop)");
    lickBtn.onClick = [this] {
        audioProcessor.triggerPhraseAudition();
    };
    addAndMakeVisible(lickBtn);

    // 4 Instant Triad Audition Buttons
    static const char* kChordLabels[4] = { "E", "G", "D", "C" };
    static const char* kChordTips[4]   = { "Audition E Major Triad", "Audition G Major Triad", "Audition D Major Triad", "Audition C Major Triad" };
    for (int i = 0; i < 4; ++i) {
        chordButtons[static_cast<size_t>(i)].setButtonText(kChordLabels[i]);
        chordButtons[static_cast<size_t>(i)].setTooltip(kChordTips[i]);
        chordButtons[static_cast<size_t>(i)].onClick = [this, i] {
            audioProcessor.triggerChordAudition(i);
        };
        addAndMakeVisible(chordButtons[static_cast<size_t>(i)]);
    }

    addAndMakeVisible(fretboard);

    // Setup 24 Symmetrical Rotary Knobs (8 + 8 + 8 across the 3 studio hardware panels)
    struct KnobSpec { const char* id; const char* title; const char* tip; };
    static constexpr KnobSpec kSpecs[24] = {
        // Panel I: Copedent & Tonebar Articulation (8 knobs)
        { ParamIDs::scoopDepth,    "AUTO SCOOP",   "Automatic A/B pedal grace-note scoop depth on attacks and triads (Right-click for MIDI CC Learn)" },
        { ParamIDs::pedalSpeed,    "PEDAL SPEED",  "Mechanical bellcrank pull speed (ms) for Pedals A, B, C, Knee Levers & Fingered Oblique Bends" },
        { ParamIDs::barGlide,      "BAR GLIDE",    "Solid 3/4-inch chrome tonebar portamento glide time (ms) on overlapped lead slides" },
        { ParamIDs::barSlideNoise, "BAR SLIDE FX", "Physical stainless-steel winding slide sheen & harmonic whistle during multi-fret glides" },
        { ParamIDs::vibratoDepth,  "VIB DEPTH",    "Delayed wrist-roll tonebar vibrato depth (also responds to MIDI Aftertouch)" },
        { ParamIDs::vibratoRate,   "VIB RATE",     "Tonebar vibrato speed (Hz)" },
        { ParamIDs::autoSwell,     "AUTO SWELL",   "Hands-free Goodrich optical volume-pedal swell bloom on attacks" },
        { ParamIDs::swellTime,     "SWELL TIME",   "Optical volume-pedal swell rise time" },

        // Panel II: E9th Pickup, String & Cabinet DNA (8 knobs)
        { ParamIDs::sampleBlend,   "STUDIO DNA",   "Blend between 44.1 kHz Stereo Studio Multi-Samples and Physical 12-Harmonic E9th String Engine" },
        { ParamIDs::pickSnap,      "PICK SNAP",    "Dunlop metal fingerpick & thumbpick attack transient bite" },
        { ParamIDs::pickupPos,     "PICKUP POS",   "Single-coil pickup distance from bridge (inches)" },
        { ParamIDs::coilPeakFreq,  "COIL PEAK HZ", "High-inductance single-coil pickup LC resonant vocal frequency (Hz)" },
        { ParamIDs::coilResonance, "COIL SING Q",  "Pickup resonant vocal sing (delta-calibrated around studio baseline)" },
        { ParamIDs::sustainTime,   "SUSTAIN",      "Plucked pedal-steel string ring-out decay time (seconds)" },
        { ParamIDs::cabinetDrop,   "CABINET SYMP", "Mechanical cabinet drop & sympathetic open-string resonance" },
        { ParamIDs::stereoWidth,   "STEREO WIDTH", "Stereo panorama width (0.0 = Mono Combo, 1.0 = Studio Stereo, 1.5 = Wide Panorama)" },

        // Panel III: Swell, Amp, Character & Space (8 knobs)
        { ParamIDs::volumePedal,   "VOL PEDAL",    "Goodrich optical volume pedal position (also draggable on the green OPTICAL SWELL bar above!)" },
        { ParamIDs::ampDrive,      "TWEED DRIVE",  "57 Fender Tweed Deluxe 12AX7/6V6 harmonic breakup & cathode sag" },
        { ParamIDs::toneWarmth,    "15\" WARMTH",  "15-inch JBL D130F / Peavey Black Widow cabinet low-mid body" },
        { ParamIDs::delayMix,      "TAPE DELAY",   "Quadrature-modulated Echoplex / BBD stereo tape delay mix with dynamic phrase ducking" },
        { ParamIDs::delayTime,     "DELAY TIME",   "Tape echo time in ms (or rhythmic beat subdivision when SYNC is active)" },
        { ParamIDs::reverbMix,     "PLATE REVERB", "Dattorro figure-8 stereo studio plate reverb mix" },
        { ParamIDs::reverbDecay,   "REV DECAY",    "Plate reverb decay tail length & stereo diffusion" },
        { ParamIDs::outputLevel,   "OUTPUT dB",    "Master plugin output trim (-24 dB to +12 dB)" }
    };

    for (int i = 0; i < 24; ++i) {
        auto& k = knobs[static_cast<size_t>(i)];
        k.paramId = kSpecs[i].id;
        k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        k.slider.setPopupDisplayEnabled(false, false, this);
        k.slider.setVelocityModeParameters(1.0, 1, 0.08, true, juce::ModifierKeys::ctrlModifier);
        k.slider.setTooltip(juce::String(kSpecs[i].tip) + " (Double-click: reset | Right-click: MIDI CC Learn)");
        k.slider.addMouseListener(this, false);
        addAndMakeVisible(k.slider);

        k.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.apvts, kSpecs[i].id, k.slider);

        k.label.setText(kSpecs[i].title, juce::dontSendNotification);
        k.label.setJustificationType(juce::Justification::centred);
        k.label.setFont(makeFont(10.0f, juce::Font::bold));
        k.label.setColour(juce::Label::textColourId, juce::Colour(0xffe4d4bc));
        k.label.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(k.label);
    }

    knobs[20].slider.textFromValueFunction = [this](double val) {
        if (syncBtn.getToggleState()) {
            return PedalSteelEngine::getSyncedSubdivisionLabel(static_cast<float>(val));
        }
        return juce::String(static_cast<int>(std::round(val))) + " ms";
    };

    updateKnobDoubleClickTargets();
    updatePresetInfoAndTooltips();

    midiKeyboard.setAvailableRange(24, 96);
    midiKeyboard.setLowestVisibleKey(24);
    midiKeyboard.setOctaveForMiddleC(4);
    midiKeyboard.setScrollButtonsVisible(false);
    addAndMakeVisible(midiKeyboard);

    resized();
    refreshVisualizer();
    startTimerHz(30);
}

PedalSteelAudioProcessorEditor::~PedalSteelAudioProcessorEditor() {
    stopTimer();
    releaseAllQwertyKeys();
    for (auto& k : knobs) {
        k.slider.removeMouseListener(this);
    }
    setLookAndFeel(nullptr);
}

void PedalSteelAudioProcessorEditor::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) {
        for (int i = 0; i < 24; ++i) {
            if (e.eventComponent == &knobs[static_cast<size_t>(i)].slider) {
                showKnobContextMenu(i);
                return;
            }
        }
    }
    juce::AudioProcessorEditor::mouseDown(e);
}

void PedalSteelAudioProcessorEditor::showKnobContextMenu(int knobIdx) {
    if (knobIdx < 0 || knobIdx >= 24) return;
    const juce::String paramId = knobs[static_cast<size_t>(knobIdx)].paramId;
    const bool isLearning = audioProcessor.isMidiLearningTarget(paramId);
    const int assignedCc  = audioProcessor.getAssignedMidiCc(paramId);

    juce::PopupMenu m;
    m.addItem(1, "Reset Knob to Preset Default");
    if (audioProcessor.isCurrentPresetModified()) {
        m.addItem(4, "Revert Entire Preset to Default");
    }
    m.addSeparator();
    m.addItem(2, isLearning ? "Cancel MIDI CC Learn" : "Learn MIDI CC...", true, isLearning);
    if (assignedCc >= 0) {
        m.addItem(3, "Clear MIDI CC (CC " + juce::String(assignedCc) + ")");
    }

    juce::Component::SafePointer<PedalSteelAudioProcessorEditor> safeThis(this);
    m.showMenuAsync(juce::PopupMenu::Options(), [safeThis, paramId, isLearning](int res) {
        if (safeThis == nullptr || res == 0) return;
        if (res == 1) {
            if (auto* rp = safeThis->audioProcessor.apvts.getParameter(paramId)) {
                const float base = safeThis->audioProcessor.getBaselineParamValue(paramId);
                rp->beginChangeGesture();
                rp->setValueNotifyingHost(rp->convertTo0to1(base));
                rp->endChangeGesture();
            }
        } else if (res == 2) {
            if (isLearning) safeThis->audioProcessor.cancelMidiLearn();
            else            safeThis->audioProcessor.startMidiLearn(paramId);
        } else if (res == 3) {
            safeThis->audioProcessor.clearMidiCcForTarget(paramId);
        } else if (res == 4) {
            safeThis->fretboard.clearLatchedPedals();
            safeThis->audioProcessor.revertToBaseline();
        }
    });
}

void PedalSteelAudioProcessorEditor::updateQwertyButtonAppearance() {
    const int octNum = (qwertyBaseMidi / 12) - 2; // MIDI 60 -> C3
    qwertyBtn.setButtonText(qwertyEnabled ? ("KEYS C" + juce::String(octNum)) : "KEYS");
    qwertyBtn.setColour(juce::TextButton::buttonColourId, qwertyEnabled ? juce::Colour(0xff96601e) : juce::Colour(0xff2a211b));
    qwertyBtn.setTooltip("Laptop QWERTY Musical Typing (" + juce::String(qwertyEnabled ? "ON" : "OFF")
                         + ", Base C" + juce::String(octNum)
                         + "): A-L white keys, W/E/T/Y/U/O/P sharps, Z/X octave down/up, 1-5 Pedals (A, B, A+B, LKL, LKR)");
}

bool PedalSteelAudioProcessorEditor::keyPressed(const juce::KeyPress& key) {
    if (!qwertyEnabled) return false;
    const juce::juce_wchar ch = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
    if (ch == 'z') {
        releaseAllQwertyKeys();
        qwertyBaseMidi = juce::jlimit(36, 84, qwertyBaseMidi - 12);
        updateQwertyButtonAppearance();
        return true;
    }
    if (ch == 'x') {
        releaseAllQwertyKeys();
        qwertyBaseMidi = juce::jlimit(36, 84, qwertyBaseMidi + 12);
        updateQwertyButtonAppearance();
        return true;
    }
    // Consume musical typing keys so they don't trigger unintended menu shortcuts while typing chords
    static const char* kUsedChars = "awsedftgyhujkolp;'12345";
    for (int i = 0; kUsedChars[i] != '\0'; ++i) {
        if (ch == static_cast<juce::juce_wchar>(kUsedChars[i])) {
            pollQwertyKeys();
            return true;
        }
    }
    return false;
}

bool PedalSteelAudioProcessorEditor::keyStateChanged(bool /*isKeyDown*/) {
    if (!qwertyEnabled) return false;
    pollQwertyKeys();
    return false;
}

void PedalSteelAudioProcessorEditor::focusLost(FocusChangeType) {
    releaseAllQwertyKeys();
}

void PedalSteelAudioProcessorEditor::pollQwertyKeys() {
    if (!qwertyEnabled || !hasKeyboardFocus(true)) {
        releaseAllQwertyKeys();
        return;
    }
    struct KeyMapEntry { int keyCode; int semiOffset; bool isKeyswitch; int ksMidi; };
    static constexpr KeyMapEntry kMap[23] = {
        { 'A', 0,  false, 0 },  { 'W', 1,  false, 0 },  { 'S', 2,  false, 0 },  { 'E', 3,  false, 0 },
        { 'D', 4,  false, 0 },  { 'F', 5,  false, 0 },  { 'T', 6,  false, 0 },  { 'G', 7,  false, 0 },
        { 'Y', 8,  false, 0 },  { 'H', 9,  false, 0 },  { 'U', 10, false, 0 },  { 'J', 11, false, 0 },
        { 'K', 12, false, 0 },  { 'O', 13, false, 0 },  { 'L', 14, false, 0 },  { 'P', 15, false, 0 },
        { ';', 16, false, 0 },  { '\'',17, false, 0 },
        { '1', 0,  true, 24 },  { '2', 0,  true, 26 },  { '3', 0,  true, 28 },  { '4', 0,  true, 29 },
        { '5', 0,  true, 31 }
    };

    for (size_t i = 0; i < 23; ++i) {
        const bool down = juce::KeyPress::isKeyCurrentlyDown(kMap[i].keyCode);
        const int prevMidi = activeQwertyMidi[i];
        if (down && prevMidi < 0) {
            const int midi = kMap[i].isKeyswitch ? kMap[i].ksMidi : juce::jlimit(36, 96, qwertyBaseMidi + kMap[i].semiOffset);
            activeQwertyMidi[i] = midi;
            audioProcessor.keyboardState.noteOn(1, midi, kMap[i].isKeyswitch ? 1.0f : 0.85f);
        } else if (!down && prevMidi >= 0) {
            activeQwertyMidi[i] = -1;
            audioProcessor.keyboardState.noteOff(1, prevMidi, 0.0f);
        }
    }
}

void PedalSteelAudioProcessorEditor::releaseAllQwertyKeys() {
    for (size_t i = 0; i < activeQwertyMidi.size(); ++i) {
        if (activeQwertyMidi[i] >= 0) {
            audioProcessor.keyboardState.noteOff(1, activeQwertyMidi[i], 0.0f);
            activeQwertyMidi[i] = -1;
        }
    }
}

void PedalSteelAudioProcessorEditor::buildCategorizedCharacterMenu() {
    charBox.clear(juce::dontSendNotification);
    auto* root = charBox.getRootMenu();

    root->addItem(1, kCharModeNames[0]); // 0: Studio Standard
    root->addSeparator();

    auto addGroup = [&](const char* header, std::initializer_list<int> modeIndices) {
        juce::PopupMenu sub;
        for (int mIdx : modeIndices) {
            sub.addItem(mIdx + 1, kCharModeNames[mIdx]);
        }
        root->addSubMenu(header, sub);
    };

    addGroup("Console, Rack & Clean Chorus", { 2, 10, 15, 20, 26, 31 });
    addGroup("Tape, Lo-Fi & Dub",            { 1, 5, 6, 14, 18, 24, 29 });
    addGroup("Amp, Tremolo, Fuzz & Filter",  { 3, 4, 11, 12, 13, 17, 27, 28, 33 });
    addGroup("Shimmer, Space & World",       { 7, 8, 9, 19, 21, 23, 25, 30 });
    addGroup("Vintage Keys, Organ & Synth",  { 16, 22, 32, 34, 35, 36 });
}

void PedalSteelAudioProcessorEditor::updatePresetInfoAndTooltips() {
    const auto& presets = getPedalSteelPresets();
    const juce::String userPreset = audioProcessor.getActiveUserPresetName();
    if (userPreset.isNotEmpty()) {
        presetBox.setTooltip("[User Preset] " + userPreset + " — Saved in Documents/NVA Pedal Steel/Presets");
        fretboard.setPatchInfoText("[User Preset] " + userPreset);
        return;
    }
    const int idx = juce::jlimit(0, static_cast<int>(presets.size()) - 1, audioProcessor.getCurrentProgram());
    const auto& pr = presets[static_cast<size_t>(idx)];
    presetBox.setTooltip("[" + juce::String(pr.category) + "] " + juce::String(pr.description));
    fretboard.setPatchInfoText(juce::String(pr.category) + "  •  " + juce::String(pr.description));
}

void PedalSteelAudioProcessorEditor::refreshVisualizer() {
    const int charId = charBox.getSelectedId();
    if (charId != lastCharSelectedId) {
        lastCharSelectedId = charId;
        const bool charActive = (charId > 1);
        charBox.setColour(juce::ComboBox::outlineColourId, charActive ? juce::Colour(0xffe59b3c) : juce::Colour(0xff6b533b));
        charBox.setColour(juce::ComboBox::textColourId,    charActive ? juce::Colour(0xffffdca0) : juce::Colour(0xfff3e8d2));
        charBox.lookAndFeelChanged();
        charBox.repaint();
    }

    const bool presetDirty = audioProcessor.isCurrentPresetModified();
    if (presetDirty != lastPresetModifiedState) {
        lastPresetModifiedState = presetDirty;
        presetBox.setColour(juce::ComboBox::outlineColourId, presetDirty ? juce::Colour(0xffe59b3c) : juce::Colour(0xff6b533b));
        revertBtn.setColour(juce::TextButton::buttonColourId, presetDirty ? juce::Colour(0xff8c5619) : juce::Colour(0xff221b16));
        revertBtn.setColour(juce::TextButton::textColourOffId, presetDirty ? juce::Colour(0xfffff0d4) : juce::Colour(0xff857563));
        revertBtn.setEnabled(presetDirty);
        presetBox.lookAndFeelChanged();
        presetBox.repaint();
        revertBtn.repaint();
        repaint(200, 8, 420, 48);
    }

    for (auto& k : knobs) {
        const bool mod   = audioProcessor.isParamModified(k.paramId);
        const bool learn = audioProcessor.isMidiLearningTarget(k.paramId);
        const int cc     = audioProcessor.getAssignedMidiCc(k.paramId);
        const juce::String ccStr = (cc >= 0) ? ("CC" + juce::String(cc)) : "";

        auto& props = k.slider.getProperties();
        if (static_cast<bool>(props.getWithDefault("modified", false)) != mod
            || static_cast<bool>(props.getWithDefault("learning", false)) != learn
            || props.getWithDefault("ccTag", "").toString() != ccStr)
        {
            props.set("modified", mod);
            props.set("learning", learn);
            props.set("ccTag", ccStr);
            k.slider.repaint();
        }
    }

    const int bpmInt = static_cast<int>(std::round(audioProcessor.getHostBpm()));
    syncBtn.setButtonText(syncBtn.getToggleState() ? ("SYNC " + juce::String(bpmInt)) : "SYNC");

    const auto st = audioProcessor.getLiveStatus();
    if (st.phrasePlaying != phraseActiveUi) {
        phraseActiveUi = st.phrasePlaying;
        lickBtn.setButtonText(phraseActiveUi ? ("STOP L" + juce::String(st.activeLickSlot + 1)) : "PLAY LICK");
        lickBtn.setColour(juce::TextButton::buttonColourId, phraseActiveUi ? juce::Colour(0xff236a73) : juce::Colour(0xff2a211b));
    }

    meterL = audioProcessor.getOutputPeakL();
    meterR = audioProcessor.getOutputPeakR();
    midiKeyboard.setPhraseKeyswitchActive(st.phraseKsActive);
    fretboard.updateStatus(st);
}

void PedalSteelAudioProcessorEditor::rebuildPresetMenu() {
    presetBox.clear(juce::dontSendNotification);
    auto* menu = presetBox.getRootMenu();
    const auto& presets = getPedalSteelPresets();

    menu->addSectionHeader("Factory Categories (" + juce::String(presets.size()) + " Presets)");

    auto addCategorySubMenu = [&](const char* catTitle, std::initializer_list<int> oneBasedSlots) {
        juce::PopupMenu sub;
        for (int slot : oneBasedSlots) {
            if (slot >= 1 && slot <= static_cast<int>(presets.size())) {
                sub.addItem(slot, presets[static_cast<size_t>(slot - 1)].name);
            }
        }
        menu->addSubMenu(catTitle, sub);
    };

    addCategorySubMenu("Country, Swing & Americana", { 1, 2, 3, 5, 6, 9, 10, 11, 12, 14, 23, 27, 45 });
    addCategorySubMenu("Indie, Bedroom & Neo-Soul",  { 7, 13, 17, 18, 22, 29, 34, 40, 42, 43 });
    addCategorySubMenu("Cinema, Tape & Psych-Rock",  { 15, 16, 24, 25, 26, 28, 31, 32, 33, 35, 38, 41, 47 });
    addCategorySubMenu("Ambient, Shimmer & Chimes",  { 4, 8, 19, 20, 37, 39, 44 });
    addCategorySubMenu("Vintage Keys, Organ & Synth",{ 21, 30, 36, 46, 48, 49, 50 });

    {
        juce::PopupMenu allSub;
        for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
            allSub.addItem(501 + i, presets[static_cast<size_t>(i)].name);
        }
        menu->addSubMenu("All 50 Factory Presets (01-50)", allSub);
    }

    cachedUserPresets = audioProcessor.getUserPresetFiles();
    const juce::String activeUser = audioProcessor.getActiveUserPresetName();
    bool foundActiveInFolder = false;

    if (!cachedUserPresets.empty()) {
        menu->addSeparator();
        menu->addSectionHeader("User Presets");
        for (int i = 0; i < static_cast<int>(cachedUserPresets.size()); ++i) {
            const juce::String name = cachedUserPresets[static_cast<size_t>(i)].getFileNameWithoutExtension();
            if (activeUser.isNotEmpty() && name.equalsIgnoreCase(activeUser)) {
                foundActiveInFolder = true;
            }
            menu->addItem(1000 + i, "[User] " + name);
        }
    }

    if (activeUser.isNotEmpty() && !foundActiveInFolder) {
        menu->addSeparator();
        menu->addItem(999, "[User] " + activeUser);
    }

    menu->addSeparator();
    menu->addItem(9000, "Revert Tweaks to Preset Default");
    menu->addItem(9001, "Save User Preset...");
    menu->addItem(9002, "Load Preset File...");
    menu->addItem(9003, "Open User Presets Folder");

    syncPresetComboSelection();
    lastSeenPresetRevision = audioProcessor.getPresetRevision();
}

void PedalSteelAudioProcessorEditor::syncPresetComboSelection() {
    const juce::String activeUser = audioProcessor.getActiveUserPresetName();
    if (activeUser.isNotEmpty()) {
        for (int i = 0; i < static_cast<int>(cachedUserPresets.size()); ++i) {
            if (cachedUserPresets[static_cast<size_t>(i)].getFileNameWithoutExtension().equalsIgnoreCase(activeUser)) {
                presetBox.setSelectedId(1000 + i, juce::dontSendNotification);
                return;
            }
        }
        if (presetBox.indexOfItemId(999) >= 0) {
            presetBox.setSelectedId(999, juce::dontSendNotification);
            return;
        }
    }
    presetBox.setSelectedId(audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
}

void PedalSteelAudioProcessorEditor::updateKnobDoubleClickTargets() {
    for (auto& k : knobs) {
        if (k.paramId != nullptr && k.paramId[0] != '\0') {
            k.slider.setDoubleClickReturnValue(true, audioProcessor.getBaselineParamValue(k.paramId));
        }
    }
}

void PedalSteelAudioProcessorEditor::promptSaveUserPreset() {
    const auto dir = PedalSteelAudioProcessor::getUserPresetsFolder();
    const juce::String curUser = audioProcessor.getActiveUserPresetName();
    const juce::String suggested = curUser.isNotEmpty() ? curUser : "My Pedal Steel";
    activeFileChooser = std::make_shared<juce::FileChooser>(
        "Save NVA Pedal Steel User Preset",
        dir.getChildFile(suggested + ".nvps"),
        "*.nvps");

    juce::Component::SafePointer<PedalSteelAudioProcessorEditor> safeThis(this);
    activeFileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [safeThis](const juce::FileChooser& fc) {
            if (safeThis == nullptr) return;
            const auto result = fc.getResult();
            if (result != juce::File{}) {
                if (safeThis->audioProcessor.saveUserPreset(result)) {
                    safeThis->rebuildPresetMenu();
                    safeThis->updateKnobDoubleClickTargets();
                    safeThis->updatePresetInfoAndTooltips();
                }
            }
        });
}

void PedalSteelAudioProcessorEditor::promptLoadUserPreset() {
    const auto dir = PedalSteelAudioProcessor::getUserPresetsFolder();
    activeFileChooser = std::make_shared<juce::FileChooser>(
        "Load NVA Pedal Steel Preset",
        dir,
        "*.nvps;*.xml");

    juce::Component::SafePointer<PedalSteelAudioProcessorEditor> safeThis(this);
    activeFileChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis](const juce::FileChooser& fc) {
            if (safeThis == nullptr) return;
            const auto result = fc.getResult();
            if (result.existsAsFile()) {
                if (safeThis->audioProcessor.loadUserPreset(result)) {
                    safeThis->rebuildPresetMenu();
                    safeThis->updateKnobDoubleClickTargets();
                    safeThis->updatePresetInfoAndTooltips();
                }
            }
        });
}

void PedalSteelAudioProcessorEditor::timerCallback() {
    const uint32_t rev = audioProcessor.getPresetRevision();
    if (rev != lastSeenPresetRevision && !presetBox.isPopupActive()) {
        rebuildPresetMenu();
        updateKnobDoubleClickTargets();
        updatePresetInfoAndTooltips();
    } else if (!presetBox.isPopupActive() && audioProcessor.getActiveUserPresetName().isEmpty()) {
        const int hostProg = audioProcessor.getCurrentProgram() + 1;
        if (presetBox.getSelectedId() != hostProg) {
            presetBox.setSelectedId(hostProg, juce::dontSendNotification);
            updateKnobDoubleClickTargets();
            updatePresetInfoAndTooltips();
        }
    }

    if (qwertyEnabled) {
        pollQwertyKeys();
    }

    refreshVisualizer();
    knobs[20].slider.repaint();
    repaint(getWidth() - 78, 8, 68, 48);
}

void PedalSteelAudioProcessorEditor::paint(juce::Graphics& g) {
    const float w = static_cast<float>(getWidth());
    const float h = static_cast<float>(getHeight());

    // Rich dark studio chassis background
    juce::ColourGradient bg(juce::Colour(0xff1a1512), 0.0f, 0.0f,
                            juce::Colour(0xff0e0b09), 0.0f, h, false);
    g.setGradientFill(bg);
    g.fillAll();

    // Top Header Banner
    auto headerRect = juce::Rectangle<float>(12.0f, 8.0f, w - 24.0f, 48.0f);
    g.setColour(juce::Colour(0xff261e18));
    g.fillRoundedRectangle(headerRect, 6.0f);
    g.setColour(juce::Colour(0xff7a5c3c));
    g.drawRoundedRectangle(headerRect, 6.0f, 1.2f);

    g.setColour(juce::Colour(0xffffb84d));
    g.setFont(makeFont(16.5f, juce::Font::bold));
    g.drawText("NVA PEDAL STEEL", 18, 11, 186, 22, juce::Justification::centredLeft, false);
    g.setColour(juce::Colour(0xffbfb099));
    g.setFont(makeFont(8.8f, juce::Font::plain));
    g.drawText("AUTO-OBLIQUE KEYS | LEGATO GLISS | MPE", 18, 32, 188, 16, juce::Justification::centredLeft, false);

    // Modified '*' indicator above top-right corner of presetBox when tweaked from preset baseline
    if (lastPresetModifiedState) {
        g.setColour(juce::Colour(0xffffb84d));
        g.setFont(makeFont(13.0f, juce::Font::bold));
        g.drawText("*", presetBox.getRight() - 10, presetBox.getY() - 6, 12, 14, juce::Justification::centred, false);
    }

    // Live Stereo Output Peak Meter on right of header
    auto meterBox = juce::Rectangle<float>(w - 68.0f, 15.0f, 48.0f, 34.0f);
    g.setColour(juce::Colour(0xff14100d));
    g.fillRoundedRectangle(meterBox, 4.0f);
    g.setColour(juce::Colour(0xff5b4733));
    g.drawRoundedRectangle(meterBox, 4.0f, 1.0f);

    auto drawBar = [&](juce::Rectangle<float> bar, float linPeak) {
        const float db = juce::Decibels::gainToDecibels(std::max(1e-4f, linPeak), -48.0f);
        const float norm = juce::jlimit(0.0f, 1.0f, (db + 48.0f) / 48.0f);
        if (norm > 0.01f) {
            juce::Colour col = (linPeak > 0.94f) ? juce::Colour(0xffff4d4d)
                             : (linPeak > 0.65f) ? juce::Colour(0xffffb84d)
                                                 : juce::Colour(0xff52b788);
            g.setColour(col);
            g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * norm), 2.0f);
        }
    };
    drawBar(juce::Rectangle<float>(meterBox.getX() + 14.0f, meterBox.getY() + 6.0f,  30.0f, 8.0f), meterL);
    drawBar(juce::Rectangle<float>(meterBox.getX() + 14.0f, meterBox.getY() + 19.0f, 30.0f, 8.0f), meterR);
    g.setColour(juce::Colour(0xffbfb099));
    g.setFont(makeFont(8.5f, juce::Font::bold));
    g.drawText("L", static_cast<int>(meterBox.getX() + 3), static_cast<int>(meterBox.getY() + 4),  10, 11, juce::Justification::centred, false);
    g.drawText("R", static_cast<int>(meterBox.getX() + 3), static_cast<int>(meterBox.getY() + 17), 10, 11, juce::Justification::centred, false);

    // 3 Symmetrical Brushed Hardware Module Panels
    const float fretH  = std::round((h - 64.0f) * 0.365f);
    const float kbH    = std::round(juce::jlimit(82.0f, 108.0f, h * 0.132f));
    const float panelY = 62.0f + fretH + 8.0f;
    const float panelH = h - panelY - kbH - 16.0f;
    const float availW = w - 24.0f - 16.0f;
    const float pw     = availW / 3.0f;

    juce::Rectangle<float> p1(12.0f,                    panelY, pw, panelH);
    juce::Rectangle<float> p2(12.0f + pw + 8.0f,        panelY, pw, panelH);
    juce::Rectangle<float> p3(12.0f + 2.0f * (pw + 8.0f), panelY, pw, panelH);

    auto drawPanel = [&](juce::Rectangle<float> r, const char* title) {
        g.setColour(juce::Colour(0xff211a15));
        g.fillRoundedRectangle(r, 7.0f);
        g.setColour(juce::Colour(0xff634c36));
        g.drawRoundedRectangle(r, 7.0f, 1.3f);

        auto banner = r.removeFromTop(25.0f);
        g.setColour(juce::Colour(0xff31251c));
        g.fillRoundedRectangle(banner, 7.0f);
        g.setColour(juce::Colour(0xffe59b3c));
        g.setFont(makeFont(11.5f, juce::Font::bold));
        g.drawText(title, banner.toNearestInt(), juce::Justification::centred, false);
    };

    drawPanel(p1, "I. COPEDENT PEDALS & TONEBAR ARTICULATION");
    drawPanel(p2, "II. E9th PICKUP, STRING & CABINET DNA");
    drawPanel(p3, "III. SWELL, AMP, CHARACTER & SPACE");
}

void PedalSteelAudioProcessorEditor::resized() {
    const int w = getWidth();
    const int h = getHeight();

    // Header Bar Controls (responsive across 1060..1600 px)
    const int rightFixed = 68; // meter area
    const int chordBtnW = 24;
    const int chordBtnStep = 27;
    const int chordGroupW = 4 * chordBtnW + 3 * 3;
    const int lickW = 66;
    const int qwertyW = 48;
    const int syncW = 60;
    const int rightButtonsStart = w - rightFixed - chordGroupW - lickW - qwertyW - syncW - 16;

    int x = 198;
    prevPresetBtn.setBounds(x, 18, 22, 28); x += 24;
    nextPresetBtn.setBounds(x, 18, 22, 28); x += 24;
    revertBtn.setBounds(x, 18, 34, 28);     x += 37;

    const int combosAvail = std::max(320, rightButtonsStart - x - 5);
    const int presetW = static_cast<int>(combosAvail * 0.33f);
    const int styleW  = static_cast<int>(combosAvail * 0.24f);
    const int modeW   = static_cast<int>(combosAvail * 0.22f);
    const int charW   = combosAvail - presetW - styleW - modeW - 12;

    presetBox.setBounds(x, 18, presetW, 28); x += presetW + 4;
    styleBox.setBounds(x, 18, styleW, 28);   x += styleW + 4;
    modeBox.setBounds(x, 18, modeW, 28);     x += modeW + 4;
    charBox.setBounds(x, 18, charW, 28);     x += charW + 5;

    syncBtn.setBounds(x, 18, syncW, 28);     x += syncW + 4;
    qwertyBtn.setBounds(x, 18, qwertyW, 28); x += qwertyW + 4;
    lickBtn.setBounds(x, 18, lickW, 28);     x += lickW + 4;

    for (int i = 0; i < 4; ++i) {
        chordButtons[static_cast<size_t>(i)].setBounds(x + i * chordBtnStep, 18, chordBtnW, 28);
    }

    // Center Fretboard & Copedent Visualizer
    const int fretH  = static_cast<int>(std::round((h - 64.0f) * 0.365f));
    const int kbH    = static_cast<int>(std::round(juce::jlimit(82.0f, 108.0f, h * 0.132f)));
    const int panelY = 62 + fretH + 8;
    const int panelH = h - panelY - kbH - 16;

    fretboard.setBounds(12, 62, w - 24, fretH);

    // Symmetrical 4x2 Rotary Knobs inside each of the 3 Hardware Panels
    const float availW = static_cast<float>(w - 24 - 16);
    const float pw = availW / 3.0f;

    auto layoutPanelKnobs = [&](int startIdx, float px) {
        constexpr int cols = 4;
        const float cellW = pw / static_cast<float>(cols);
        const float bodyH = static_cast<float>(panelH - 30);
        const float rowH  = bodyH * 0.5f;

        for (int i = 0; i < 8; ++i) {
            const int r = i / cols;
            const int c = i % cols;
            const int cx = static_cast<int>(std::round(px + c * cellW));
            const int cy = static_cast<int>(std::round(static_cast<float>(panelY + 29) + r * rowH));
            const int cw = static_cast<int>(std::round(cellW));
            const int ch = static_cast<int>(std::round(rowH));

            auto& k = knobs[static_cast<size_t>(startIdx + i)];
            k.label.setBounds(cx + 2, cy, cw - 4, 15);
            k.slider.setBounds(cx + 4, cy + 15, cw - 8, std::max(56, ch - 17));
        }
    };

    layoutPanelKnobs(0,  12.0f);
    layoutPanelKnobs(8,  12.0f + pw + 8.0f);
    layoutPanelKnobs(16, 12.0f + 2.0f * (pw + 8.0f));

    // Bottom Playable Color-Coded Keyboard (24..96 = 43 white keys -> spans 100.0% of width with zero gap!)
    const int kbWidth = w - 24;
    midiKeyboard.setBounds(12, h - kbH - 8, kbWidth, kbH);
    midiKeyboard.setLowestVisibleKey(24);
    midiKeyboard.setKeyWidth(static_cast<float>(kbWidth) / 43.0f);
}
