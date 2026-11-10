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

    inline const char* getKeyswitchShortLabel(int midiNote) {
        switch (midiNote) {
            case 24: return "A";
            case 25: return "C";
            case 26: return "B";
            case 28: return "AB";
            case 29: return "LK";
            case 31: return "LR";
            case 36: return "L1";
            case 38: return "L2";
            case 40: return "L3";
            case 41: return "L4";
            case 43: return "LP";
            default: return nullptr;
        }
    }
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
    const float capR = radius * 0.65f;
    juce::ColourGradient capGrad(juce::Colour(0xffd8cfc0), centreX - capR * 0.4f, centreY - capR * 0.4f,
                                 juce::Colour(0xff6e655c), centreX + capR * 0.5f, centreY + capR * 0.5f, true);
    g.setGradientFill(capGrad);
    g.fillEllipse(centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f);

    g.setColour(juce::Colour(0xff14110e));
    g.drawEllipse(rx, ry, rw, rw, 1.4f);

    // Pointer indicator line
    juce::Path p;
    const float pointerLength = radius * 0.85f;
    const float pointerThickness = 2.5f;
    p.addRoundedRectangle(-pointerThickness * 0.5f, -radius + 2.0f, pointerThickness, pointerLength * 0.58f, 1.0f);
    p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    g.setColour(juce::Colour(0xffffdca0));
    g.fillPath(p);

    // Value readout at bottom of knob
    g.setColour(juce::Colour(0xffd8c7ab));
    g.setFont(makeFont(10.5f, juce::Font::bold));
    g.drawText(slider.getTextFromValue(slider.getValue()), x, y + height - 14, width, 14, juce::Justification::centred, false);
}

//==============================================================================
void PedalSteelKeyboardComponent::drawWhiteNote(int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                                                bool isDown, bool isOver, juce::Colour /*lineColour*/, juce::Colour /*textColour*/) {
    const bool isPedalKs  = (midiNoteNumber >= 24 && midiNoteNumber <= 31);
    const bool isPhraseKs = (midiNoteNumber >= 36 && midiNoteNumber <= 43);

    juce::Colour baseCol = juce::Colour(0xfff5efe4); // Warm vintage ivory
    if (isPedalKs)       baseCol = juce::Colour(0xff6b4722); // Amber Pedal Keyswitches (C0..G0)
    else if (isPhraseKs) baseCol = juce::Colour(0xff234c54); // Teal Studio Lick Keyswitches (C1..G1)
    else if (midiNoteNumber < 45) baseCol = juce::Colour(0xffcfc5b4);

    if (isDown)      baseCol = isPhraseKs ? juce::Colour(0xff4cd4dc) : juce::Colour(0xffffa834);
    else if (isOver) baseCol = baseCol.brighter(0.14f);

    juce::ColourGradient grad(baseCol.darker(0.08f), area.getX(), area.getY(),
                              baseCol, area.getX(), area.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRect(area);

    g.setColour(juce::Colour(0xff261f1a));
    g.drawRect(area, 0.8f);

    // Subtle open E9th string gold dot indicator
    if (!isPedalKs && !isPhraseKs && isOpenE9StringMidi(midiNoteNumber)) {
        g.setColour(juce::Colour(0xffd48728).withAlpha(0.75f));
        g.fillEllipse(area.getCentreX() - 2.5f, area.getBottom() - 23.0f, 5.0f, 5.0f);
    }

    if (const char* ksLbl = getKeyswitchShortLabel(midiNoteNumber)) {
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
    const bool isPhraseKs = (midiNoteNumber >= 36 && midiNoteNumber <= 43);

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

    if (const char* ksLbl = getKeyswitchShortLabel(midiNoteNumber)) {
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

void FretboardVisualizer::mouseDown(const juce::MouseEvent& e) {
    const auto pt = e.position;
    for (int i = 0; i < 6; ++i) {
        if (pedalRects[static_cast<size_t>(i)].contains(pt)) {
            heldUiPedal = i;
            if (i == 0) processor.setLivePedalState(0, 1.0f, true, false);
            else if (i == 1) { processor.setLivePedalState(0, 1.0f, true, false); processor.setLivePedalState(1, 1.0f, true, false); }
            else if (i == 2) processor.setLivePedalState(1, 1.0f, true, false);
            else if (i == 3) processor.setLivePedalState(2, 1.0f, true, false);
            else if (i == 4) processor.setLivePedalState(3, 1.0f, true, false);
            else if (i == 5) processor.setLivePedalState(4, 1.0f, true, false);
            repaint();
            return;
        }
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
    if (draggedString >= 0 && cachedScaleWidth > 10.0f) {
        const float fret = juce::jlimit(0.0f, 24.0f, ((e.position.x - cachedNutX) / cachedScaleWidth) * 24.0f);
        processor.slideFretboardNote(draggedString, fret);
    }
}

void FretboardVisualizer::mouseUp(const juce::MouseEvent&) {
    if (heldUiPedal >= 0) {
        if (heldUiPedal == 0) processor.setLivePedalState(0, 0.0f, false, true);
        else if (heldUiPedal == 1) { processor.setLivePedalState(0, 0.0f, false, true); processor.setLivePedalState(1, 0.0f, false, true); }
        else if (heldUiPedal == 2) processor.setLivePedalState(1, 0.0f, false, true);
        else if (heldUiPedal == 3) processor.setLivePedalState(2, 0.0f, false, true);
        else if (heldUiPedal == 4) processor.setLivePedalState(3, 0.0f, false, true);
        else if (heldUiPedal == 5) processor.setLivePedalState(4, 0.0f, false, true);
        heldUiPedal = -1;
        repaint();
    }
    if (draggedString >= 0) {
        processor.releaseFretboardNote(draggedString);
        draggedString = -1;
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

    // Subtle interactive hint cleanly in top-right margin above String 1 (zero fret-number overlap!)
    g.setColour(juce::Colour(0xff9c8972));
    g.setFont(makeFont(9.0f, juce::Font::bold));
    g.drawText("CLICK & DRAG STRINGS TO SLIDE CHROME BAR",
               static_cast<int>(pickupX - 260.0f), static_cast<int>(neckArea.getY() + 2.0f), 246, 12, juce::Justification::centredRight, false);

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

    // Interactive Floor Pedals & Knee Levers Strip (adapts labels when Back-Neck C6th is selected!)
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

        g.setColour(juce::Colour(0xff1a1512));
        g.fillRoundedRectangle(r, 5.0f);
        if (val > 0.01f) {
            g.setColour(juce::Colour(0xffe59b3c).withAlpha(0.25f + 0.60f * val));
            g.fillRoundedRectangle(r.withHeight(r.getHeight() * val).withBottomY(r.getBottom()), 5.0f);
        }
        g.setColour(val > 0.1f ? juce::Colour(0xffffb84d) : juce::Colour(0xff6b5844));
        g.drawRoundedRectangle(r, 5.0f, 1.3f);

        g.setColour(juce::Colour(0xfff5ead6));
        g.setFont(makeFont(10.5f, juce::Font::bold));
        g.drawText(kPedalTitles[i], r.toNearestInt(), juce::Justification::centred, false);
    }

    // Optical Volume Swell Meter
    auto volRect = juce::Rectangle<float>(pedalStrip.getRight() - 150.0f, pedalStrip.getY() + 2.0f, 147.0f, pedalStrip.getHeight() - 4.0f);
    g.setColour(juce::Colour(0xff1a1512));
    g.fillRoundedRectangle(volRect, 5.0f);
    const float vFill = juce::jlimit(0.0f, 1.0f, status.volumePedal);
    g.setColour(juce::Colour(0xff52b788).withAlpha(0.78f));
    g.fillRoundedRectangle(volRect.withWidth(volRect.getWidth() * vFill), 5.0f);
    g.setColour(juce::Colour(0xff6b5844));
    g.drawRoundedRectangle(volRect, 5.0f, 1.3f);
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
    setResizable(true, true);
    setResizeLimits(1060, 660, 1600, 980);
    setSize(1220, 720);

    // Preset Stepper & Factory/User Preset Menu
    prevPresetBtn.setTooltip("Previous Factory Preset");
    prevPresetBtn.onClick = [this] {
        audioProcessor.stepPreset(-1);
        syncPresetComboSelection();
        updateKnobDoubleClickTargets();
    };
    addAndMakeVisible(prevPresetBtn);

    nextPresetBtn.setTooltip("Next Factory Preset");
    nextPresetBtn.onClick = [this] {
        audioProcessor.stepPreset(1);
        syncPresetComboSelection();
        updateKnobDoubleClickTargets();
    };
    addAndMakeVisible(nextPresetBtn);

    presetBox.setTooltip("Factory & User Presets (Save or Load custom .nvps patches at the bottom of this menu)");
    rebuildPresetMenu();
    presetBox.onChange = [this] {
        const int id = presetBox.getSelectedId();
        if (id >= 1 && id <= static_cast<int>(getPedalSteelPresets().size())) {
            audioProcessor.loadPreset(id - 1);
            syncPresetComboSelection();
            updateKnobDoubleClickTargets();
        } else if (id >= 1000 && id < 1000 + static_cast<int>(cachedUserPresets.size())) {
            const auto file = cachedUserPresets[static_cast<size_t>(id - 1000)];
            audioProcessor.loadUserPreset(file);
            rebuildPresetMenu();
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

    // Voice Style, Performance Mode & Analog Character ComboBoxes
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
    modeBox.setTooltip("Performance Mode (including 12th-Fret 'Palm Chime Harmonics' & Chromatic 'Studio Phrase Licks')");
    addAndMakeVisible(modeBox);
    modeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, ParamIDs::playMode, modeBox);

    charBox.addItemList({
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
        "Two Star VG-8 Bowed",
        "Revolver Reverse Tape",
        "Mohan Veena Jawari",
        "Neo-Soul CE-1 Syrup",
        "Binson Echorec Drum",
        "CS-80 Synth-Steel"
    }, 1);
    charBox.setTooltip("Analog Character Circuit (23 Modes): Cassette, Ribbon+LA-2A, Phaser, Spring+Trem, R&B, VB-2, Shimmer, Leslie 147, Diode Bloom, Standel, Bosstone, Talk-Box, Bakelite, ADT, Tri-Chorus, Organ, Dobro Cone, Reverse Tape, Mohan Veena, CE-1 Syrup, Binson Echorec, or CS-80 Synth-Steel!");
    addAndMakeVisible(charBox);
    charAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, ParamIDs::fxCharacter, charBox);

    // Host Tempo Sync Button
    syncBtn.setClickingTogglesState(true);
    syncBtn.setTooltip("Locks Tape Delay Time (1/16, 1/8, 1/8D, 1/4) & Swell Pocket to DAW Host BPM");
    addAndMakeVisible(syncBtn);
    syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, ParamIDs::tempoSync, syncBtn);

    // Play Studio Lick Audition Button
    lickBtn.setTooltip("Audition & cycle the 4 recorded 44.1 kHz stereo studio licks for the active style (click again to stop, or trigger via C1-G1)");
    lickBtn.onClick = [this] {
        audioProcessor.triggerPhraseAudition();
    };
    addAndMakeVisible(lickBtn);

    // 4 Instant Triad Audition Buttons
    static const char* kChordLabels[4] = { "E", "G", "D", "C" };
    static const char* kChordTips[4]   = { "Audition E Major E9th Triad", "Audition G Major E9th Triad", "Audition D Major E9th Triad", "Audition C Major E9th Triad" };
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
        { ParamIDs::scoopDepth,    "AUTO SCOOP",   "Automatic A/B pedal grace-note scoop depth on attacks and triads" },
        { ParamIDs::pedalSpeed,    "PEDAL SPEED",  "Mechanical bellcrank pull speed (ms) for Pedals A, B, C & Knee Levers" },
        { ParamIDs::barGlide,      "BAR GLIDE",    "Solid 3/4-inch chrome tonebar portamento glide time (ms)" },
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

        // Panel III: Volume Pedal, Tweed Amp, Tape Echo & Studio Plate (8 knobs)
        { ParamIDs::volumePedal,   "VOL PEDAL",    "Goodrich optical floor volume pedal (also controlled via CC11 / CC7)" },
        { ParamIDs::ampDrive,      "TWEED DRIVE",  "Push-pull 12AX7 / 6V6 Fender Twin clean to Tweed Deluxe harmonic breakup" },
        { ParamIDs::toneWarmth,    "15\" WARMTH",  "15-inch JBL / Black Widow speaker body warmth vs presence tilt" },
        { ParamIDs::delayMix,      "TAPE DELAY",   "Warm Echoplex / BBD stereo analog tape delay mix with dynamic ducking" },
        { ParamIDs::delayTime,     "DELAY TIME",   "Tape echo delay time (ms, or musical beat subdivision 1/16..1/4D when SYNC is active)" },
        { ParamIDs::reverbMix,     "PLATE REVERB", "Lush Dattorro Figure-8 cross-coupled stereo plate reverb mix" },
        { ParamIDs::reverbDecay,   "REV DECAY",    "Stereo plate reverb decay tail length" },
        { ParamIDs::outputLevel,   "OUTPUT dB",    "Master plugin output trim (dB)" }
    };

    for (int i = 0; i < 24; ++i) {
        auto& k = knobs[static_cast<size_t>(i)];
        k.paramId = kSpecs[i].id;
        k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        k.slider.setVelocityModeParameters(1.0, 1, 0.08, true, juce::ModifierKeys::ctrlAltCommandModifiers);
        k.slider.setTooltip(juce::String(kSpecs[i].tip) + "  (Double-click: reset to preset | Ctrl/Cmd+Drag: fine)");
        addAndMakeVisible(k.slider);

        k.label.setText(kSpecs[i].title, juce::dontSendNotification);
        k.label.setJustificationType(juce::Justification::centred);
        k.label.setColour(juce::Label::textColourId, juce::Colour(0xfff0e2c8));
        k.label.setFont(makeFont(10.5f, juce::Font::bold));
        addAndMakeVisible(k.label);

        k.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.apvts, kSpecs[i].id, k.slider);
    }

    updateKnobDoubleClickTargets();
    lastSeenPresetRevision = audioProcessor.getPresetRevision();

    // Dynamic subdivision label readout on DELAY TIME knob (index 20) when SYNC is enabled!
    knobs[20].slider.textFromValueFunction = [this](double val) -> juce::String {
        if (syncBtn.getToggleState()) {
            return PedalSteelEngine::getSyncedSubdivisionLabel(static_cast<float>(val));
        }
        return juce::String(static_cast<int>(std::round(val))) + " ms";
    };

    // Playable Color-Coded Keyswitch & E9th MIDI Keyboard (spans 24..96 = 43 white keys cleanly across 100% width!)
    midiKeyboard.setAvailableRange(24, 96);
    midiKeyboard.setLowestVisibleKey(24);
    midiKeyboard.setScrollButtonsVisible(false);
    addAndMakeVisible(midiKeyboard);

    resized();
    refreshVisualizer();
    startTimerHz(30);
}

PedalSteelAudioProcessorEditor::~PedalSteelAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
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
    fretboard.updateStatus(st);
}

void PedalSteelAudioProcessorEditor::rebuildPresetMenu() {
    presetBox.clear(juce::dontSendNotification);
    auto* menu = presetBox.getRootMenu();
    const auto& presets = getPedalSteelPresets();
    menu->addSectionHeader("Factory Presets (" + juce::String(presets.size()) + ")");

    for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
        menu->addItem(i + 1, presets[static_cast<size_t>(i)].name);
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
                }
            }
        });
}

void PedalSteelAudioProcessorEditor::timerCallback() {
    const uint32_t rev = audioProcessor.getPresetRevision();
    if (rev != lastSeenPresetRevision && !presetBox.isPopupActive()) {
        rebuildPresetMenu();
        updateKnobDoubleClickTargets();
    } else if (!presetBox.isPopupActive() && audioProcessor.getActiveUserPresetName().isEmpty()) {
        const int hostProg = audioProcessor.getCurrentProgram() + 1;
        if (presetBox.getSelectedId() != hostProg) {
            presetBox.setSelectedId(hostProg, juce::dontSendNotification);
            updateKnobDoubleClickTargets();
        }
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
    g.setFont(makeFont(17.0f, juce::Font::bold));
    g.drawText("NVA PEDAL STEEL", 18, 11, 190, 22, juce::Justification::centredLeft, false);
    g.setColour(juce::Colour(0xffbfb099));
    g.setFont(makeFont(9.0f, juce::Font::plain));
    g.drawText("C0-G0: PEDALS | C1-G1: LICKS | MPE", 18, 32, 196, 16, juce::Justification::centredLeft, false);

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

    juce::Rectangle<float> p1(12.0f,                panelY, pw, panelH);
    juce::Rectangle<float> p2(12.0f + pw + 8.0f,    panelY, pw, panelH);
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

    // Header Bar Controls (responsive across 1060..1600 px with Preset + Style + Mode + Analog Character)
    const int rightFixed = 72; // meter area
    const int chordBtnW = 26;
    const int chordBtnStep = 29;
    const int chordGroupW = 4 * chordBtnW + 3 * 3;
    const int lickW = 74;
    const int syncW = 68;
    const int rightButtonsStart = w - rightFixed - chordGroupW - lickW - syncW - 14;

    int x = 214;
    prevPresetBtn.setBounds(x, 18, 22, 28); x += 24;
    nextPresetBtn.setBounds(x, 18, 22, 28); x += 25;

    const int combosAvail = std::max(340, rightButtonsStart - x - 6);
    const int presetW = static_cast<int>(combosAvail * 0.34f);
    const int styleW  = static_cast<int>(combosAvail * 0.22f);
    const int modeW   = static_cast<int>(combosAvail * 0.22f);
    const int charW   = combosAvail - presetW - styleW - modeW - 12;

    presetBox.setBounds(x, 18, presetW, 28); x += presetW + 4;
    styleBox.setBounds(x, 18, styleW, 28);   x += styleW + 4;
    modeBox.setBounds(x, 18, modeW, 28);     x += modeW + 4;
    charBox.setBounds(x, 18, charW, 28);     x += charW + 6;

    syncBtn.setBounds(x, 18, syncW, 28);     x += syncW + 4;
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
