#include "PluginEditor.h"
#include "UiLayout.h"
#include "DotFont.h"
#include "Params.h"
#include "BinaryData.h"

using namespace UiLayout;

namespace {
constexpr float kDesignW = 1600.0f, kDesignH = 1000.0f;
constexpr int kFps = 60;
const juce::Colour kAmber(0xffffa83a), kCream(0xffeadcc2), kInk(0xf2181410);

juce::Image loadImage(const void* data, int size) { return juce::ImageCache::getFromMemory(data, size); }

juce::Rectangle<float> R(const float* b) { return { b[0], b[1], b[2], b[3] }; }

// The design's printed values and its pointer angles don't follow one straight line (e.g. GLIDE reads 145 ms on a
// 10-500 ms range but points to +35 deg), so each control maps value -> angle/position piecewise-linearly through the
// design's own (value, angle) pair: at the design state every pixel is the reference's, and motion stays monotonic.
struct Anchor { float norm, angle; };
float anchored(float n, Anchor a, float lo = -135.0f, float hi = 135.0f) {
    n = juce::jlimit(0.0f, 1.0f, n);
    return n <= a.norm ? lo + (a.angle - lo) * n / juce::jmax(a.norm, 1e-4f)
                       : a.angle + (hi - a.angle) * (n - a.norm) / juce::jmax(1.0f - a.norm, 1e-4f);
}
float unanchored(float v, Anchor a, float lo = -135.0f, float hi = 135.0f) {   // inverse, for dragging the slider
    return v <= a.angle ? a.norm * (v - lo) / juce::jmax(a.angle - lo, 1e-4f)
                        : a.norm + (1.0f - a.norm) * (v - a.angle) / juce::jmax(hi - a.angle, 1e-4f);
}
// dome strips are labelled so the design's pointer is frame "35 deg" etc.; FEEL/SPACE frames are true pointer angles
// (measured on the design by matcap_strips.py)
constexpr Anchor kTone{ 0.65f, 35.0f }, kPick{ 0.40f, -50.0f }, kGlide{ (145.0f - 10.0f) / 490.0f, 34.8f },
                 kSwell{ 0.35f, 33.8f }, kEcho{ 0.18f, -101.5f }, kReverb{ 0.28f, -68.9f };
constexpr Anchor kVolume{ 0.72f, sliderThumbCx };

// knob angle (deg clockwise from 12 o'clock, -135..135) -> filmstrip frame
int frameFor(float angleDeg, int frames) {
    return juce::jlimit(0, frames - 1, juce::roundToInt((angleDeg + 135.0f) / 270.0f * (float) (frames - 1)));
}
// the names printed on the panel (parameters keep their engine names for hosts)
const char* panelLabel(int c) {
    static const char* names[] = { "TONE", "PICK", "GLIDE", "SWELL", "ECHO", "REVERB", "TRIM", "VOLUME PEDAL",
                                   "PEDAL A", "PEDAL B", "PEDAL C", "KNEE LEVER LKL", "KNEE LEVER LKR", "MODE" };
    return c >= 0 && c < 14 ? names[c] : "";
}
juce::Point<float> ptrCentre(const float* o, int px) { return { o[0] + px * 0.25f, o[1] + px * 0.25f }; }

const juce::Rectangle<float> kSteelArea{ 470.0f, 60.0f, 900.0f, 620.0f };   // bar, strings, floor pedals, knee levers
} // namespace

PedalSteelAudioProcessorEditor::PedalSteelAudioProcessorEditor(PedalSteelAudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p) {
    bg = loadImage(BinaryData::ui_bg_jpg, BinaryData::ui_bg_jpgSize);
    spPedal[0] = loadImage(BinaryData::sp_pedal_A_on_png, BinaryData::sp_pedal_A_on_pngSize);
    spPedal[1] = loadImage(BinaryData::sp_pedal_B_on_png, BinaryData::sp_pedal_B_on_pngSize);
    spPedal[2] = loadImage(BinaryData::sp_pedal_C_on_png, BinaryData::sp_pedal_C_on_pngSize);
    spMeter = loadImage(BinaryData::sp_meter_on_png, BinaryData::sp_meter_on_pngSize);
    spThumb = loadImage(BinaryData::sp_slider_thumb_png, BinaryData::sp_slider_thumb_pngSize);
    spFill = loadImage(BinaryData::sp_slider_fill_png, BinaryData::sp_slider_fill_pngSize);
    spLever[0] = loadImage(BinaryData::sp_lever_L_on_png, BinaryData::sp_lever_L_on_pngSize);
    spLever[1] = loadImage(BinaryData::sp_lever_R_on_png, BinaryData::sp_lever_R_on_pngSize);
    spMidi = loadImage(BinaryData::sp_midi_on_png, BinaryData::sp_midi_on_pngSize);
    spKnee[0] = loadImage(BinaryData::sp_kl_left_pushed_png, BinaryData::sp_kl_left_pushed_pngSize);
    spKnee[1] = loadImage(BinaryData::sp_kl_right_pushed_png, BinaryData::sp_kl_right_pushed_pngSize);
    spFloor[0] = loadImage(BinaryData::sp_floor_A_png, BinaryData::sp_floor_A_pngSize);
    spFloor[1] = loadImage(BinaryData::sp_floor_B_png, BinaryData::sp_floor_B_pngSize);
    spFloor[2] = loadImage(BinaryData::sp_floor_C_png, BinaryData::sp_floor_C_pngSize);
    spBar = loadImage(BinaryData::sp_tone_bar_png, BinaryData::sp_tone_bar_pngSize);
    spBarHome = loadImage(BinaryData::sp_tone_bar_home_png, BinaryData::sp_tone_bar_home_pngSize);
    spKneeBrass[0] = loadImage(BinaryData::sp_kl_left_pushed_brass_png, BinaryData::sp_kl_left_pushed_brass_pngSize);
    spKneeBrass[1] = loadImage(BinaryData::sp_kl_right_pushed_brass_png, BinaryData::sp_kl_right_pushed_brass_pngSize);
    spFloorBrass[0] = loadImage(BinaryData::sp_floor_A_brass_png, BinaryData::sp_floor_A_brass_pngSize);
    spFloorBrass[1] = loadImage(BinaryData::sp_floor_B_brass_png, BinaryData::sp_floor_B_brass_pngSize);
    spFloorBrass[2] = loadImage(BinaryData::sp_floor_C_brass_png, BinaryData::sp_floor_C_brass_pngSize);
    scallop = { loadImage(BinaryData::knob_scallop_png, BinaryData::knob_scallop_pngSize), scallopFrames, scallopFramePx };
    space = { loadImage(BinaryData::knob_space_png, BinaryData::knob_space_pngSize), spaceFrames, spaceFramePx };
    ptrTone = { loadImage(BinaryData::ptr_tone_png, BinaryData::ptr_tone_pngSize), ptrTONEFrames, ptrTONEFramePx };
    ptrPick = { loadImage(BinaryData::ptr_pick_png, BinaryData::ptr_pick_pngSize), ptrPICKFrames, ptrPICKFramePx };
    ptrTrim = { loadImage(BinaryData::ptr_trim_png, BinaryData::ptr_trim_pngSize), ptrTRIMFrames, ptrTRIMFramePx };

    // display dot kernel: display_dot.png value 255 == dispKernelMax added per channel
    {
        auto k = loadImage(BinaryData::display_dot_png, BinaryData::display_dot_pngSize);
        dotK = k.getWidth(); dotKernel.assign((size_t) dotK * dotK * 3, 0.0f);
        for (int y = 0; y < dotK; ++y)
            for (int x = 0; x < dotK; ++x) {
                auto c = k.getPixelAt(x, y);
                float* d = &dotKernel[((size_t) y * dotK + x) * 3];
                d[0] = c.getRed() / 255.0f * dispKernelMax; d[1] = c.getGreen() / 255.0f * dispKernelMax; d[2] = c.getBlue() / 255.0f * dispKernelMax;
            }
    }

    poppinsMedium = juce::Typeface::createSystemTypefaceFor(BinaryData::Poppins_Medium_ttf, BinaryData::Poppins_Medium_ttfSize);
    poppinsSemi = juce::Typeface::createSystemTypefaceFor(BinaryData::Poppins_SemiBold_ttf, BinaryData::Poppins_SemiBold_ttfSize);
    interMedium = juce::Typeface::createSystemTypefaceFor(BinaryData::Inter_w500_o14_ttf, BinaryData::Inter_w500_o14_ttfSize);

    auto setTxt = [&](int i, juce::Typeface::Ptr f, const char* ref, const float* t, unsigned int col, bool left = false) {
        txt[i].face = f; txt[i].ref = ref; txt[i].cx = t[0]; txt[i].cy = t[1]; txt[i].refInkW = t[2]; txt[i].left = t[3];
        txt[i].colour = juce::Colour(col); txt[i].leftAlign = left; prepareText(txt[i]);
    };
    setTxt(0, poppinsMedium, txtRef_val_TONE, txt_val_TONE, txtColour_val_TONE);
    setTxt(1, poppinsMedium, txtRef_val_PICK, txt_val_PICK, txtColour_val_PICK);
    setTxt(2, poppinsMedium, txtRef_val_GLIDE, txt_val_GLIDE, txtColour_val_GLIDE);
    setTxt(3, poppinsMedium, txtRef_val_SWELL, txt_val_SWELL, txtColour_val_SWELL);
    setTxt(4, poppinsMedium, txtRef_val_ECHO, txt_val_ECHO, txtColour_val_ECHO);
    setTxt(5, poppinsMedium, txtRef_val_REVERB, txt_val_REVERB, txtColour_val_REVERB);
    setTxt(6, poppinsMedium, txtRef_vol, txt_vol, txtColour_vol);
    setTxt(7, interMedium, txtRef_mode, txt_mode, txtColour_mode, true);
    setTxt(8, poppinsSemi, txtRef_db, txt_db, txtColour_db);
    auto setTxtS = [&](int i, juce::Typeface::Ptr f, const char* ref, const float* t, unsigned int col, bool left = false) {
        txtS[i].face = f; txtS[i].ref = ref; txtS[i].cx = t[0]; txtS[i].cy = t[1]; txtS[i].refInkW = t[2]; txtS[i].left = t[3];
        txtS[i].colour = juce::Colour(col); txtS[i].leftAlign = left; prepareText(txtS[i]);
    };
    setTxtS(0, poppinsSemi, txtRef_val_TONE, txtS_val_TONE, txtSColour_val_TONE);
    setTxtS(1, poppinsSemi, txtRef_val_PICK, txtS_val_PICK, txtSColour_val_PICK);
    setTxtS(2, poppinsSemi, txtRef_val_GLIDE, txtS_val_GLIDE, txtSColour_val_GLIDE);
    setTxtS(3, poppinsSemi, txtRef_val_SWELL, txtS_val_SWELL, txtSColour_val_SWELL);
    setTxtS(4, poppinsSemi, txtRef_val_ECHO, txtS_val_ECHO, txtSColour_val_ECHO);
    setTxtS(5, poppinsSemi, txtRef_val_REVERB, txtS_val_REVERB, txtSColour_val_REVERB);
    setTxtS(6, poppinsSemi, txtRef_vol, txtS_vol, txtSColour_vol);
    setTxtS(7, poppinsMedium, txtRef_mode, txtS_mode, txtSColour_mode, true);
    setTxtS(8, poppinsSemi, txtRef_db, txtS_db, txtSColour_db);
    spSmallDeck = loadImage(BinaryData::sp_small_deck_png, BinaryData::sp_small_deck_pngSize);
    skinImg[1] = loadImage(BinaryData::skin_maple_jpg, BinaryData::skin_maple_jpgSize);
    skinImg[2] = loadImage(BinaryData::skin_redsparkle_jpg, BinaryData::skin_redsparkle_jpgSize);
    skinImg[3] = loadImage(BinaryData::skin_pearloid_jpg, BinaryData::skin_pearloid_jpgSize);
    skinImg[4] = loadImage(BinaryData::skin_koa_jpg, BinaryData::skin_koa_jpgSize);
    skin = skinForCurrentPreset();
    spSmallTop = loadImage(BinaryData::sp_small_top_png, BinaryData::sp_small_top_pngSize);

    menuLnf.setFaces(poppinsMedium, poppinsSemi);
    kit.medium = poppinsMedium; kit.semi = poppinsSemi; kit.menuLnf = &menuLnf;
    for (int i = 0; i < 5; ++i) pedalAnim[(size_t) i] = norm((Ctl) (PedA + i));
    buildAccessibility();
    {   // the play-the-steel tip shows until the user has played the neck once (or closed it)
        auto xml = juce::XmlDocument::parse(settingsFile());
        hintDismissed = xml != nullptr && xml->getBoolAttribute("playHintDone", false);
    }
    setLookAndFeel(&menuLnf);   // resize corner (and any stock widgets) in the plugin's style
    setWantsKeyboardFocus(true);
    const int startWidth = proc.editorWidth;   // setResizeLimits below resizes (and so calls resized()) before setSize
    setResizable(true, true);
    setResizeLimits(800, 500, 2400, 1500);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio(kDesignW / kDesignH);
    setSize(startWidth, juce::roundToInt(startWidth * kDesignH / kDesignW));
    lastNorm.fill(-1.0f);
    rebuildDisplay();
    startTimerHz(kFps);
}

PedalSteelAudioProcessorEditor::~PedalSteelAudioProcessorEditor() {
    juce::PopupMenu::dismissAllActiveMenus();
    setLookAndFeel(nullptr);
    closeOverlays();
    stopTimer();
    releaseAllStrings();
    proc.midiLearn.cancelLearn(); proc.midiLearn.learnMode = false;
}

juce::File PedalSteelAudioProcessorEditor::settingsFile() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("Northern Valley Audio").getChildFile("NVA Pedal Steel").getChildFile("ui_settings.xml");
}

void PedalSteelAudioProcessorEditor::dismissHint() {
    if (hintDismissed) return;
    hintDismissed = true;
    auto f = settingsFile(); f.getParentDirectory().createDirectory();
    auto xml = juce::XmlDocument::parse(f); if (xml == nullptr) xml = std::make_unique<juce::XmlElement>("NvaPedalSteelUi");
    xml->setAttribute("playHintDone", true); xml->writeTo(f);
    repaint();
}

juce::Rectangle<float> PedalSteelAudioProcessorEditor::hintBounds() const {
    return { 800.0f - 250.0f, 312.0f, 500.0f, 44.0f };   // on the wood just below the neck
}

void PedalSteelAudioProcessorEditor::drawHint(juce::Graphics& g) {
    if (hintDismissed && hintAlpha <= 0.0f) return;
    auto r = hintBounds(); const float a = hintAlpha;
    if (a <= 0.0f) return;
    g.setColour(juce::Colours::black.withAlpha(0.35f * a)); g.fillRoundedRectangle(r.translated(0, 3), 22.0f);
    g.setColour(kInk.withAlpha(0.95f * a)); g.fillRoundedRectangle(r, 22.0f);
    g.setColour(kAmber.withAlpha(0.7f * a)); g.drawRoundedRectangle(r.reduced(0.5f), 22.0f, 1.2f);
    // a small hand-drag glyph: dot + arrow
    const float cx = r.getX() + 30.0f, cy = r.getCentreY();
    g.setColour(kAmber.withAlpha(a)); g.fillEllipse(cx - 4, cy - 4, 8, 8);
    g.drawArrow(juce::Line<float>(cx + 7, cy, cx + 22, cy), 1.6f, 6.0f, 6.0f);
    g.setColour(kCream.withAlpha(a)); g.setFont(juce::Font(juce::FontOptions(poppinsMedium).withHeight(15.0f)));
    g.drawText("Click a string on the neck to play it, drag along to slide the bar", r.withTrimmedLeft(62).withTrimmedRight(40), juce::Justification::centredLeft);
    auto x = juce::Rectangle<float>(r.getRight() - 32.0f, cy - 8.0f, 16.0f, 16.0f);
    g.setColour(kCream.withAlpha(0.7f * a));
    g.drawLine(x.getX() + 3, x.getY() + 3, x.getRight() - 3, x.getBottom() - 3, 1.5f);
    g.drawLine(x.getRight() - 3, x.getY() + 3, x.getX() + 3, x.getBottom() - 3, 1.5f);
}

void PedalSteelAudioProcessorEditor::buildAccessibility() {
    setTitle("NVA Pedal Steel");
    setDescription("Pedal steel guitar instrument. Tab moves between controls, arrow keys adjust, Space presses.");
    auto add = [this](Ctl c, AccessibleControl::Spec spec) {
        a11y.push_back(std::make_unique<AccessibleControl>(std::move(spec))); a11yCtl.push_back(c);
        addAndMakeVisible(*a11y.back());
    };
    for (Ctl c : { Tone, Pick, Glide, Swell, Echo, Reverb, Trim, Volume }) {
        AccessibleControl::Spec sp; sp.kind = AccessibleControl::Kind::Slider; sp.name = panelLabel(c);
        if (auto* p = param(c)) sp.help = p->getName(64);
        sp.param = param(c); sp.valueText = [this, c] { return valueText(c); };
        add(c, sp);
    }
    for (Ctl c : { PedA, PedB, PedC, LKL, LKR }) {
        AccessibleControl::Spec sp; sp.kind = AccessibleControl::Kind::Toggle; sp.name = panelLabel(c);
        if (auto* p = param(c)) sp.help = p->getName(64);
        sp.param = param(c); sp.valueText = [this, c] { return valueText(c); };
        sp.press = [this, c] { toggle(c); }; sp.isOn = [this, c] { return norm(c) > 0.5f; };
        add(c, sp);
    }
    auto button = [&](Ctl c, const juce::String& name, const juce::String& help, std::function<juce::String()> value, std::function<void()> press) {
        AccessibleControl::Spec sp; sp.kind = AccessibleControl::Kind::Button; sp.name = name; sp.help = help;
        sp.valueText = std::move(value); sp.press = std::move(press); add(c, sp);
    };
    button(Display, "Preset", "Opens the preset browser", [this] { return displayText(); }, [this] { openBrowser(); });
    button(Prev, "Previous preset", "", [this] { return displayText(); }, [this] { proc.stepPreset(-1); });
    button(Next, "Next preset", "", [this] { return displayText(); }, [this] { proc.stepPreset(+1); });
    button(Mode, "Mode", "Opens the performance mode menu",
           [this] { auto* mp = dynamic_cast<juce::AudioParameterChoice*>(param(Mode)); return mp ? mp->getCurrentChoiceName() : juce::String(); },
           [this] { showModeMenu(); });
    {
        AccessibleControl::Spec sp; sp.kind = AccessibleControl::Kind::Toggle; sp.name = "MIDI Learn";
        sp.help = "When on, choose a control and move a MIDI controller to assign it";
        sp.press = [this] { auto& ml = proc.midiLearn; ml.learnMode = ! ml.learnMode; if (! ml.learnMode) ml.cancelLearn(); repaint(); };
        sp.isOn = [this] { return proc.midiLearn.learnMode; };
        sp.valueText = [this] { return juce::String(proc.midiLearn.learnMode ? "On" : "Off"); };
        add(MidiLearnBtn, sp);
    }
}

void PedalSteelAudioProcessorEditor::resized() {
    scale = (float) getWidth() / kDesignW;
    proc.editorWidth = getWidth();
    for (size_t i = 0; i < a11y.size(); ++i) a11y[i]->setBounds((ctlBounds(a11yCtl[i]) * scale).getSmallestIntegerContainer());
    if (valueEditor != nullptr) finishValueEdit(true);
    placeOverlays();
    rebuildScaledBackground();
}

void PedalSteelAudioProcessorEditor::rebuildScaledBackground() {
    // the 2x background resampled once per size (at the screen's pixel density), so frames only blit it
    const float ps = juce::jmax(1.0f, (float) juce::Component::getApproximateScaleFactorForComponent(this));
    const int w = juce::roundToInt(getWidth() * ps), h = juce::roundToInt(getHeight() * ps);
    if (w <= 0 || h <= 0 || ! bg.isValid()) return;
    bgScaled = juce::Image(juce::Image::RGB, w, h, false);
    juce::Graphics g(bgScaled);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(bg, juce::Rectangle<float>(0, 0, (float) w, (float) h));
    static const float* boxes[5] = { nullptr, skin_maple, skin_redsparkle, skin_pearloid, skin_koa };
    if (skin > 0 && skinImg[skin].isValid()) {   // this preset's instrument, baked in
        const float k = (float) w / kDesignW; const float* b = boxes[skin];
        g.drawImage(skinImg[skin], juce::Rectangle<float>(b[0] * k, b[1] * k, b[2] * k, b[3] * k));
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// parameters

juce::RangedAudioParameter* PedalSteelAudioProcessorEditor::param(Ctl c) const {
    static const char* ids[] = { ParamIDs::toneWarmth, ParamIDs::pickSnap, ParamIDs::barGlide, ParamIDs::autoSwell,
                                 ParamIDs::delayMix, ParamIDs::reverbMix, ParamIDs::outputLevel, ParamIDs::volumePedal,
                                 ParamIDs::pedalA, ParamIDs::pedalB, ParamIDs::pedalC, ParamIDs::leverLKL, ParamIDs::leverLKR,
                                 ParamIDs::playMode };
    return (c >= Tone && c <= Mode) ? proc.apvts.getParameter(ids[c]) : nullptr;
}

float PedalSteelAudioProcessorEditor::norm(Ctl c) const {
    auto* p = param(c); return p ? p->getValue() : 0.0f;
}

void PedalSteelAudioProcessorEditor::setNorm(Ctl c, float v) {
    if (auto* p = param(c)) p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, v));
    flashValue(c);   // drags, wheel, keys: the LED display shows the value for a moment
}

void PedalSteelAudioProcessorEditor::toggle(Ctl c) {
    const int idx = c - PedA;
    proc.setLivePedalState(idx, norm(c) > 0.5f ? 0.0f : 1.0f, true, true);
}

juce::String PedalSteelAudioProcessorEditor::valueText(Ctl c) const {
    auto* p = param(c); if (p == nullptr) return {};
    const float plain = p->convertFrom0to1(p->getValue());
    if (c == Glide) return juce::String(juce::roundToInt(plain)) + " ms";
    if (c == Trim) return juce::String(plain, 1) + " dB";
    if (c >= PedA && c <= LKR) return plain > 0.5f ? "On" : "Off";
    return juce::String(juce::roundToInt(plain * 100.0f)) + " %";
}

int PedalSteelAudioProcessorEditor::trimFrame() const {
    // 0.0 dB points straight up (as in the design): -24 dB -> -135 deg, +12 dB -> +135 deg
    auto* p = param(Trim); const float db = p ? p->convertFrom0to1(p->getValue()) : 0.0f;
    const float ang = db < 0 ? db / 24.0f * 135.0f : db / 12.0f * 135.0f;
    return frameFor(ang, ptrTRIMFrames);
}

int PedalSteelAudioProcessorEditor::skinForCurrentPreset() {
    static const juce::StringArray ids { "walnut", "maple", "redsparkle", "pearloid", "koa" };
    const auto user = proc.getActiveUserPresetName();
    if (user.isNotEmpty()) {
        if (user != cachedUserName) {
            cachedUserName = user; cachedUserSkin = 0;
            if (auto xml = juce::XmlDocument::parse(PedalSteelAudioProcessor::getUserPresetsFolder().getChildFile(user + ".nvps")))
                cachedUserSkin = juce::jmax(0, ids.indexOf(xml->getStringAttribute("finish", "walnut")));
        }
        return cachedUserSkin;
    }
    cachedUserName.clear();
    switch (proc.getCurrentProgram()) { case 3: return 1; case 5: return 2; case 12: return 3; case 17: return 4; default: return 0; }
}

bool PedalSteelAudioProcessorEditor::presetModified() const {
    // performance controls (pedals, levers, volume pedal) and the output trim don't count as editing the preset
    static const juce::StringArray perf { ParamIDs::pedalA, ParamIDs::pedalB, ParamIDs::pedalC, ParamIDs::leverLKL,
                                          ParamIDs::leverLKR, ParamIDs::volumePedal, ParamIDs::outputLevel };
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p)) {
            if (perf.contains(rp->getParameterID())) continue;
            const float base = proc.getBaselineParamValue(rp->getParameterID());
            const float now = rp->convertFrom0to1(rp->getValue());
            const float span = juce::jmax(1e-6f, rp->getNormalisableRange().end - rp->getNormalisableRange().start);
            if (std::abs(now - base) / span > 0.002f) return true;
        }
    return false;
}

void PedalSteelAudioProcessorEditor::flashValue(Ctl c) {
    if (c < Tone || c > Volume) return;
    juce::String label = panelLabel(c); if (c == Volume) label = "VOLUME";
    displayOverride = (label + " " + valueText(c)).toUpperCase();
    displayOverrideUntil = juce::Time::getMillisecondCounterHiRes() * 0.001 + 1.4;
}

juce::String PedalSteelAudioProcessorEditor::displayText() const {
    if (displayOverride.isNotEmpty() && juce::Time::getMillisecondCounterHiRes() * 0.001 < displayOverrideUntil) return displayOverride;
    // "01 - Sweet Georgia E9 (Flyover Country)" -> "01 SWEET GEORGIA E9"
    auto name = proc.getProgramName(proc.getCurrentProgram());
    auto user = proc.getActiveUserPresetName();
    if (user.isNotEmpty()) name = user;
    name = name.upToFirstOccurrenceOf("(", false, false).trim();
    name = name.replace(" - ", " ").toUpperCase();
    return name.substring(0, 40) + (modifiedShown ? "*" : "");
}

// ---------------------------------------------------------------------------------------------------------------------
// the steel: fret <-> x along the neck, string rows

float PedalSteelAudioProcessorEditor::fretToX(float fret) {
    fret = juce::jlimit(fretAnchor[0][0], fretAnchor[numFretAnchors - 1][0], fret);
    for (int i = 1; i < numFretAnchors; ++i)
        if (fret <= fretAnchor[i][0]) {
            const float t = (fret - fretAnchor[i - 1][0]) / (fretAnchor[i][0] - fretAnchor[i - 1][0]);
            return fretAnchor[i - 1][1] + t * (fretAnchor[i][1] - fretAnchor[i - 1][1]);
        }
    return fretAnchor[numFretAnchors - 1][1];
}

float PedalSteelAudioProcessorEditor::xToFret(float x) {
    x = juce::jlimit(fretAnchor[0][1], fretAnchor[numFretAnchors - 1][1], x);
    for (int i = 1; i < numFretAnchors; ++i)
        if (x <= fretAnchor[i][1]) {
            const float t = (x - fretAnchor[i - 1][1]) / (fretAnchor[i][1] - fretAnchor[i - 1][1]);
            return fretAnchor[i - 1][0] + t * (fretAnchor[i][0] - fretAnchor[i - 1][0]);
        }
    return fretAnchor[numFretAnchors - 1][0];
}

float PedalSteelAudioProcessorEditor::snapFret(float fret, bool hard) {
    const float r = std::round(fret);
    return (hard || std::abs(fret - r) < 0.18f) ? r : fret;   // a magnet around each fret, so slides settle in tune
}

int PedalSteelAudioProcessorEditor::stringAt(float y) {
    int best = 0;
    for (int s = 1; s < 10; ++s) if (std::abs(stringY[s] - y) < std::abs(stringY[best] - y)) best = s;
    return best;
}

void PedalSteelAudioProcessorEditor::playString(int s, float fret) {
    if (s < 0 || s >= 10) return;
    proc.triggerFretboardNote(s, fret);
    heldStrings[(size_t) s] = true;
    stringEnergy[(size_t) s] = juce::jmax(stringEnergy[(size_t) s], 0.9f);   // visual pluck right away
}

void PedalSteelAudioProcessorEditor::releaseAllStrings() {
    for (int s = 0; s < 10; ++s)
        if (heldStrings[(size_t) s]) { proc.releaseFretboardNote(s); heldStrings[(size_t) s] = false; }
    lastString = -1;
}

// ---------------------------------------------------------------------------------------------------------------------
// drawing

void PedalSteelAudioProcessorEditor::drawStripFrame(juce::Graphics& g, const Strip& s, int frame, juce::Rectangle<float> dest) const {
    if (! s.img.isValid()) return;
    frame = juce::jlimit(0, s.frames - 1, frame);
    g.drawImage(s.img, dest.getX(), dest.getY(), dest.getWidth(), dest.getHeight(), 0, frame * s.px, s.px, s.px);
}

void PedalSteelAudioProcessorEditor::drawSprite(juce::Graphics& g, const juce::Image& img, const float* box, float dx, float clipRight, float alpha) const {
    if (! img.isValid() || alpha <= 0.002f) return;
    juce::Graphics::ScopedSaveState ss(g);
    if (clipRight < 1e8f) g.reduceClipRegion(juce::Rectangle<float>(box[0] + dx - 1, box[1], clipRight - box[0] - dx + 1, box[3]).toNearestInt());
    g.setOpacity(juce::jmin(1.0f, alpha));
    g.drawImage(img, juce::Rectangle<float>(box[0] + dx, box[1], box[2], box[3]));
}

void PedalSteelAudioProcessorEditor::prepareText(TextItem& t) {
    // size the face so the reference string's ink is as wide as in the design, and pin its baseline
    juce::Font f(juce::FontOptions(t.face).withHeight(100.0f));
    juce::GlyphArrangement ga; ga.addLineOfText(f, t.ref, 0.0f, 0.0f);
    juce::Path path; ga.createPath(path); auto b = path.getBounds();
    const float k = b.getWidth() > 0 ? t.refInkW / b.getWidth() : 0.1f;
    t.height = 100.0f * k;
    t.baseline = t.cy - b.getCentreY() * k;
}

void PedalSteelAudioProcessorEditor::drawText(juce::Graphics& g, const TextItem& t, const juce::String& s) const {
    juce::Font f(juce::FontOptions(t.face).withHeight(t.height));
    juce::GlyphArrangement ga; ga.addLineOfText(f, s, 0.0f, t.baseline);
    juce::Path path; ga.createPath(path); auto b = path.getBounds();
    const float dx = t.leftAlign ? t.left - b.getX() : t.cx - b.getCentreX();
    g.setColour(t.colour);
    g.fillPath(path, juce::AffineTransform::translation(dx, 0.0f));
}

void PedalSteelAudioProcessorEditor::rebuildDisplay() {
    // the empty display from the background, plus one measured dot glow per lit dot (bilinear sub-pixel placement)
    const int w = juce::roundToInt(displayBox[2] * 2), h = juce::roundToInt(displayBox[3] * 2);
    const int bx = juce::roundToInt(displayBox[0] * 2), by = juce::roundToInt(displayBox[1] * 2);
    lastDisplay = displayText();
    std::vector<std::pair<int, int>> dots;
    for (int i = 0; i < lastDisplay.length(); ++i)
        if (auto* gl = DotFont::find((char) lastDisplay[i]))
            for (int r = 0; r < 7; ++r)
                for (int c = 0; c < 5; ++c)
                    if (gl->rows[r][c] == '#') dots.push_back({ i * 6 + c, r });
    int cols = 1; for (auto& d : dots) cols = juce::jmax(cols, d.first + 1);
    constexpr float halfWindow = 352.0f;                       // the glass: centre +- this (2x px) holds 22 characters
    const int capacity = (int) (2.0f * halfWindow / dispPitchX) + 1;
    float ox = dispCentreX - (cols - 1) * dispPitchX * 0.5f;
    if (cols > capacity) {
        dispScroll = juce::jlimit(0, cols - capacity, dispScroll);
        ox = dispCentreX - halfWindow - dispScroll * dispPitchX;
        dots.erase(std::remove_if(dots.begin(), dots.end(), [&](const std::pair<int, int>& d) {
            const float x = ox + d.first * dispPitchX; return x < dispCentreX - halfWindow - 1.0f || x > dispCentreX + halfWindow + 1.0f; }),
            dots.end());
    }
    std::vector<float> add((size_t) w * h * 3, 0.0f);
    const int half = dotK / 2;
    for (auto& d : dots) {
        const float x = ox + d.first * dispPitchX, y = dispOriginY + d.second * dispPitchY;
        const int xi = (int) std::floor(x), yi = (int) std::floor(y); const float fx = x - xi, fy = y - yi;
        const float wts[4] = { (1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy };
        for (int q = 0; q < 4; ++q) {
            const int cx = xi + (q & 1), cy = yi + (q >> 1);
            for (int ky = 0; ky < dotK; ++ky) {
                const int py = cy + ky - half; if (py < 0 || py >= h) continue;
                for (int kx = 0; kx < dotK; ++kx) {
                    const int px = cx + kx - half; if (px < 0 || px >= w) continue;
                    const float* k = &dotKernel[((size_t) ky * dotK + kx) * 3]; float* a = &add[((size_t) py * w + px) * 3];
                    a[0] += k[0] * wts[q]; a[1] += k[1] * wts[q]; a[2] += k[2] * wts[q];
                }
            }
        }
    }
    dispImg = juce::Image(juce::Image::RGB, w, h, false);
    juce::Image::BitmapData out(dispImg, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            auto base = bg.getPixelAt(bx + x, by + y); const float* a = &add[((size_t) y * w + x) * 3];
            out.setPixelColour(x, y, juce::Colour((juce::uint8) juce::jlimit(0, 255, juce::roundToInt(base.getRed() + a[0])),
                                                  (juce::uint8) juce::jlimit(0, 255, juce::roundToInt(base.getGreen() + a[1])),
                                                  (juce::uint8) juce::jlimit(0, 255, juce::roundToInt(base.getBlue() + a[2]))));
        }
}

void PedalSteelAudioProcessorEditor::drawSteel(juce::Graphics& g) {
    // under the neck: knee levers swing, floor pedals go down (eased, so they move rather than blink)
    const bool brass = skin == 4;   // the koa steel has brass rods and cross bar
    drawSprite(g, brass ? spKneeBrass[0] : spKnee[0], sp_kl_left_pushed, 0, 1e9f, pedalShown(3));
    drawSprite(g, brass ? spKneeBrass[1] : spKnee[1], sp_kl_right_pushed, 0, 1e9f, pedalShown(4));
    {   // pressed floor pedals slide down into place as they engage (a half-pressed pedal sits half way, no double image)
        const float* boxes[3] = { sp_floor_A, sp_floor_B, sp_floor_C };
        for (int i = 0; i < 3; ++i) {
            const float a = pedalShown(i); if (a <= 0.002f) continue;
            const float* b = boxes[i]; const float lift = (1.0f - a) * 7.0f;
            const float moved[4] = { b[0], b[1] - lift, b[2], b[3] };
            drawSprite(g, brass ? spFloorBrass[i] : spFloor[i], moved, 0, 1e9f, juce::jmin(1.0f, a * 1.6f));
        }
    }

    // at its home fret the strip around the bar is the reference's own pixels; it fades out as the bar moves away
    if (skin == 0) drawSprite(g, spBarHome, sp_tone_bar_home, 0, 1e9f, 1.0f - std::abs(barFretShown - barRefFret) * 4.0f);

    // copedent: strings a pedal or lever is retuning glow softly along their whole length (brighter = bigger bend)
    {
        juce::Graphics::ScopedSaveState cs(g);
        for (int s = 0; s < 10; ++s) {
            const float b = juce::jlimit(0.0f, 1.0f, std::abs(stringBend[(size_t) s]) / 2.0f);
            if (b < 0.03f) continue;
            const float y = stringY[s], x0g = fretAnchor[0][1] - 40.0f, x1g = stringEndX + 20.0f;
            juce::ColourGradient cg(kAmber.withAlpha(0.0f), x0g, y, kAmber.withAlpha(0.0f), x1g, y, false);
            cg.addColour(0.12, kAmber.withAlpha(0.30f * b)); cg.addColour(0.88, kAmber.withAlpha(0.30f * b));
            g.setGradientFill(cg); g.fillRect(juce::Rectangle<float>(x0g, y - 2.2f, x1g - x0g, 4.4f));
            cg = juce::ColourGradient(juce::Colour(0xffffe2b0).withAlpha(0.0f), x0g, y, juce::Colour(0xffffe2b0).withAlpha(0.0f), x1g, y, false);
            cg.addColour(0.12, juce::Colour(0xffffe2b0).withAlpha(0.55f * b)); cg.addColour(0.88, juce::Colour(0xffffe2b0).withAlpha(0.55f * b));
            g.setGradientFill(cg); g.fillRect(juce::Rectangle<float>(x0g, y - 0.6f, x1g - x0g, 1.2f));
        }
    }

    // changer: each retuned string's finger rocks a little at the end of the steel (raise = toward the end plate),
    // drawn from the current instrument's own pixels, with a small glint on the hook
    {
        juce::Graphics::ScopedSaveState cs(g);
        static const float* skinBoxes[5] = { nullptr, skin_maple, skin_redsparkle, skin_pearloid, skin_koa };
        for (int s = 0; s < 10; ++s) {
            const float b = juce::jlimit(-2.0f, 2.0f, stringBend[(size_t) s]);
            if (std::abs(b) < 0.05f) continue;
            const float ysC = stringY[s] * 2.0f, xsC = 2668.0f + (ysC - 385.0f) * 0.212f;   // finger at the string's end (2x px)
            const float dx = b * 1.0f;   // UI px at the top of the hook
            const juce::Image& src = (skin > 0 && skinImg[skin].isValid()) ? skinImg[skin] : bg;
            const float ox = (skin > 0 && skinImg[skin].isValid()) ? skinBoxes[skin][0] * 2.0f : 0.0f;
            const float oy = (skin > 0 && skinImg[skin].isValid()) ? skinBoxes[skin][1] * 2.0f : 0.0f;
            g.setOpacity(1.0f);
            // the finger rocks about its base: rows of the hook shift more toward the top (a skew, no hard seam)
            const float hx0 = xsC + 2.0f, hw = 44.0f, top = ysC - 9.0f;
            for (int r = 0; r < 9; ++r) {
                const float rowY = top + r * 2.0f, k = 1.0f - (float) r / 8.0f;
                g.drawImage(src, hx0 * 0.5f + dx * k, rowY * 0.5f, hw * 0.5f, 1.0f,
                            juce::roundToInt(hx0 - ox), juce::roundToInt(rowY - oy), (int) hw, 2);
            }
            const float gx = xsC * 0.5f + 16.0f + dx * 0.6f, gy = ysC * 0.5f - 2.0f;
            g.setColour(juce::Colour(0xfffff3dc).withAlpha(0.8f * std::abs(b) / 2.0f));
            g.fillEllipse(gx - 2.0f, gy - 1.2f, 4.0f, 2.4f);
        }
    }

    // strings: the length between the bar and the changer shimmers while it sounds (standing wave + motion blur)
    const float barX = fretToX(barFretShown);
    const float x0 = barX + 16.0f, x1 = stringEndX;
    {
    juce::Graphics::ScopedSaveState colourState(g);   // the low-alpha colours below must not fade the images drawn later
    for (int s = 0; s < 10; ++s) {
        const float e = stringEnergy[(size_t) s];
        if (e < 0.01f || x1 <= x0) continue;
        const float y = stringY[s], amp = 0.6f + 2.6f * e;
        const float ph = (float) (animTime * juce::MathConstants<double>::twoPi * (11.0 + 1.7 * s));
        juce::Path lens, line;
        const int N = 48;
        for (int i = 0; i <= N; ++i) {   // upper edge of the blur envelope, and the string at this instant
            const float t = (float) i / N, x = x0 + t * (x1 - x0), env = std::sin(juce::MathConstants<float>::pi * t);
            if (i == 0) lens.startNewSubPath(x, y - amp * env); else lens.lineTo(x, y - amp * env);
            const float dy = amp * env * std::sin(ph);
            if (i == 0) line.startNewSubPath(x, y + dy); else line.lineTo(x, y + dy);
        }
        for (int i = N; i >= 0; --i) {   // lower edge
            const float t = (float) i / N, x = x0 + t * (x1 - x0), env = std::sin(juce::MathConstants<float>::pi * t);
            lens.lineTo(x, y + amp * env);
        }
        lens.closeSubPath();
        g.setGradientFill(juce::ColourGradient(kAmber.withAlpha(0.0f), x0, y, kAmber.withAlpha(0.0f), x1, y, false));
        { juce::ColourGradient glow(kAmber.withAlpha(0.0f), x0, y, kAmber.withAlpha(0.0f), x1, y, false);
          glow.addColour(0.5, kAmber.withAlpha(0.10f * e)); g.setGradientFill(glow);
          g.fillRect(juce::Rectangle<float>(x0, y - 4.0f, x1 - x0, 8.0f)); }          // warm light along the moving string
        g.setColour(kCream.withAlpha(0.16f + 0.30f * e)); g.fillPath(lens);          // motion blur envelope
        g.setColour(juce::Colours::white.withAlpha(0.45f + 0.5f * e)); g.strokePath(line, juce::PathStrokeType(1.0f));
    }
    }

    // hover on the neck: the string under the mouse lights, and a guide shows where the bar would go
    if ((hover == Fretboard || hover == Pluck) && dragging == None && ! proc.midiLearn.learnMode) {
        juce::Graphics::ScopedSaveState hs(g);
        const int s = stringAt(hoverPos.y); const float y = stringY[s];
        const float sx0 = hover == Pluck ? barX + 16.0f : fretAnchor[0][1], sx1 = stringEndX;
        juce::ColourGradient sg(kCream.withAlpha(0.0f), sx0, y, kCream.withAlpha(0.0f), sx1, y, false);
        sg.addColour(0.15, kCream.withAlpha(0.55f)); sg.addColour(0.85, kCream.withAlpha(0.55f));
        g.setGradientFill(sg); g.fillRect(juce::Rectangle<float>(sx0, y - 0.9f, sx1 - sx0, 1.8f));
        if (hover == Fretboard) {
            const float fx = fretToX(std::round(xToFret(hoverPos.x)));
            const float y0 = stringY[0] - 7.0f, y1 = stringY[9] + 7.0f;
            juce::ColourGradient fg(kAmber.withAlpha(0.0f), fx, y0, kAmber.withAlpha(0.0f), fx, y1, false);
            fg.addColour(0.5, kAmber.withAlpha(0.65f));
            g.setGradientFill(fg); g.fillRect(juce::Rectangle<float>(fx - 1.0f, y0, 2.0f, y1 - y0));
            g.setColour(kAmber.withAlpha(0.9f)); g.fillEllipse(juce::Rectangle<float>(5.0f, 5.0f).withCentre({ fx, y }));
        }
    }

    // the tone bar, wherever it is (drawn for the reference position, shifted along the neck)
    if (skin == 0) drawSprite(g, spBar, sp_tone_bar, barX - barRefCx);
    else {   // other bodies: the bar without its reflection on walnut
        juce::Graphics::ScopedSaveState bs(g);
        g.reduceClipRegion(juce::Rectangle<float>(0, 0, kDesignW, 289.0f).toNearestInt());
        drawSprite(g, spBar, sp_tone_bar, barX - barRefCx);
    }
}

void PedalSteelAudioProcessorEditor::paint(juce::Graphics& g) {
    if (skinFade < 1.0f && bgScaledPrev.isValid()) {   // crossfade from the previous instrument
        g.drawImage(bgScaledPrev, getLocalBounds().toFloat());
        g.setOpacity(skinFade);
    }
    if (bgScaled.isValid()) g.drawImage(bgScaled, getLocalBounds().toFloat());
    g.setOpacity(1.0f);
    g.addTransform(juce::AffineTransform::scale(scale));
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    if (! bgScaled.isValid()) g.drawImage(bg, juce::Rectangle<float>(0, 0, kDesignW, kDesignH));

    if (compact()) { drawSprite(g, spSmallDeck, sp_small_deck); drawSprite(g, spSmallTop, sp_small_top); }   // small_1 labels
    drawSteel(g);

    // preset display, meter, trim
    g.drawImage(dispImg, R(displayBox));
    if (meterLit > 0) {
        const float srcW = (float) meterSegEnd[juce::jlimit(0, meterSegments - 1, meterLit - 1)] + 1.0f;
        g.drawImage(spMeter, sp_meter_on[0], sp_meter_on[1], srcW * 0.5f, sp_meter_on[3], 0, 0, (int) srcW, spMeter.getHeight());
    }
    drawStripFrame(g, ptrTrim, trimFrame(), { ptrTRIMOrigin[0], ptrTRIMOrigin[1], ptrTRIMFramePx * 0.5f, ptrTRIMFramePx * 0.5f });

    // deck knobs
    auto knobAngle = [&](Ctl c) {
        const Anchor a = c == Tone ? kTone : c == Pick ? kPick : c == Glide ? kGlide : c == Swell ? kSwell : c == Echo ? kEcho : kReverb;
        return anchored(norm(c), a);
    };
    drawStripFrame(g, ptrTone, frameFor(knobAngle(Tone), ptrTONEFrames), { ptrTONEOrigin[0], ptrTONEOrigin[1], ptrTONEFramePx * 0.5f, ptrTONEFramePx * 0.5f });
    drawStripFrame(g, ptrPick, frameFor(knobAngle(Pick), ptrPICKFrames), { ptrPICKOrigin[0], ptrPICKOrigin[1], ptrPICKFramePx * 0.5f, ptrPICKFramePx * 0.5f });
    auto centred = [](const float* c, int px) { const float s = px * 0.5f; return juce::Rectangle<float>(c[0] - s * 0.5f, c[1] - s * 0.5f, s, s); };
    drawStripFrame(g, scallop, frameFor(knobAngle(Glide), scallopFrames), centred(scallopCentre0, scallopFramePx));
    drawStripFrame(g, scallop, frameFor(knobAngle(Swell), scallopFrames), centred(scallopCentre1, scallopFramePx));
    drawStripFrame(g, space, frameFor(knobAngle(Echo), spaceFrames), centred(spaceCentre0, spaceFramePx));
    drawStripFrame(g, space, frameFor(knobAngle(Reverb), spaceFrames), centred(spaceCentre1, spaceFramePx));

    // pedals A B C (lit + pressed), knee levers (lit + pressed) -- eased in and out
    drawSprite(g, spPedal[0], sp_pedal_A_on, 0, 1e9f, pedalShown(0));
    drawSprite(g, spPedal[1], sp_pedal_B_on, 0, 1e9f, pedalShown(1));
    drawSprite(g, spPedal[2], sp_pedal_C_on, 0, 1e9f, pedalShown(2));
    drawSprite(g, spLever[0], sp_lever_L_on, 0, 1e9f, pedalShown(3));
    drawSprite(g, spLever[1], sp_lever_R_on, 0, 1e9f, pedalShown(4));

    // volume pedal slider: fill up to the handle, then the handle
    const float hx = anchored(norm(Volume), kVolume, sliderX0, sliderX1);
    drawSprite(g, spFill, sp_slider_fill, 0.0f, hx);
    drawSprite(g, spThumb, sp_slider_thumb, hx - sliderThumbCx);

    // live text
    const Ctl vals[6] = { Tone, Pick, Glide, Swell, Echo, Reverb };
    const TextItem* T = compact() ? txtS : txt;
    for (int i = 0; i < 6; ++i) if (editingCtl != vals[i]) drawText(g, T[i], valueText(vals[i]));
    if (editingCtl != Volume) drawText(g, T[6], valueText(Volume));
    if (auto* mp = dynamic_cast<juce::AudioParameterChoice*>(param(Mode))) drawText(g, T[7], mp->getCurrentChoiceName());
    if (editingCtl != Trim) drawText(g, T[8], valueText(Trim));

    if (proc.midiLearn.learnMode) drawSprite(g, spMidi, sp_midi_on);
    { juce::Graphics::ScopedSaveState sh(g); drawHint(g); }
    { juce::Graphics::ScopedSaveState s0(g); drawFocus(g); }
    { juce::Graphics::ScopedSaveState s1(g); drawLearnOverlay(g); }
    { juce::Graphics::ScopedSaveState s2(g); drawTooltip(g); }
}

juce::Rectangle<float> PedalSteelAudioProcessorEditor::ctlBounds(Ctl c) const {
    auto circle = [](juce::Point<float> p, float r) { return juce::Rectangle<float>(p.x - r, p.y - r, 2 * r, 2 * r); };
    switch (c) {
        case Tone:   return circle(ptrCentre(ptrTONEOrigin, ptrTONEFramePx), 36);
        case Pick:   return circle(ptrCentre(ptrPICKOrigin, ptrPICKFramePx), 36);
        case Glide:  return circle({ scallopCentre0[0], scallopCentre0[1] }, 46);
        case Swell:  return circle({ scallopCentre1[0], scallopCentre1[1] }, 46);
        case Echo:   return circle({ spaceCentre0[0], spaceCentre0[1] }, 40);
        case Reverb: return circle({ spaceCentre1[0], spaceCentre1[1] }, 40);
        case Trim:   return circle(ptrCentre(ptrTRIMOrigin, ptrTRIMFramePx), 22);
        case Volume: return R(hitSlider);
        case PedA:   return R(hitPedalA);
        case PedB:   return R(hitPedalB);
        case PedC:   return R(hitPedalC);
        case LKL:    return R(hitLKL);
        case LKR:    return R(hitLKR);
        case Mode:   return R(hitMode);
        case Prev:   return R(hitPrev);
        case Next:   return R(hitNext);
        case Display: return R(displayBox);
        case MidiLearnBtn: return R(hitMidiLearn);
        case Fretboard: return R(hitFretboard);
        case Pluck:  return R(hitPluck);
        case None: case NumCtl: default: return {};
    }
}

void PedalSteelAudioProcessorEditor::drawLearnOverlay(juce::Graphics& g) {
    auto& ml = proc.midiLearn;
    const bool on = ml.learnMode;
    const auto armed = ml.armedParam();
    juce::Font badge(juce::FontOptions(poppinsSemi).withHeight(10.5f));
    for (int i = Tone; i <= Mode; ++i) {
        const Ctl c = (Ctl) i; auto* p = param(c); if (p == nullptr) continue;
        const int cc = ml.ccFor(p->paramID);
        if (! on && cc < 0) continue;
        auto b = ctlBounds(c);
        const bool isCircle = c <= Trim, isArmed = on && armed == p->paramID;
        if (on) {
            const float pulse = isArmed ? 0.5f + 0.5f * std::sin((float) animTime * 7.0f) : 0.0f;
            g.setColour(kAmber.withAlpha(isArmed ? 0.18f + 0.22f * pulse : 0.07f));
            if (isCircle) g.fillEllipse(b); else g.fillRoundedRectangle(b, 6.0f);
            g.setColour(kAmber.withAlpha(isArmed ? 0.95f : 0.55f));
            if (isCircle) g.drawEllipse(b, isArmed ? 2.0f : 1.2f); else g.drawRoundedRectangle(b, 6.0f, isArmed ? 2.0f : 1.2f);
        }
        if (cc >= 0 && on) {   // which CC drives it (only while learning: the design stays clean otherwise)
            const juce::String s = "CC " + juce::String(cc);
            const float w = juce::GlyphArrangement::getStringWidth(badge, s) + 10.0f;
            juce::Rectangle<float> r(b.getCentreX() - w * 0.5f, b.getY() - 9.0f, w, 15.0f);
            g.setColour(kInk); g.fillRoundedRectangle(r, 4.0f);
            g.setColour(kAmber); g.drawRoundedRectangle(r, 4.0f, 1.0f);
            g.setFont(badge); g.drawText(s, r, juce::Justification::centred, false);
        }
    }
    if (on) {   // one line of help, on the cream top bar's shadow side of the deck
        const juce::String help = armed.isNotEmpty() ? "Move a knob, fader or pedal on your MIDI controller..."
                                                     : "MIDI LEARN: click a control, then move a controller. Click MIDI LEARN again to finish.";
        juce::Font f(juce::FontOptions(poppinsMedium).withHeight(13.0f));
        const float w = juce::GlyphArrangement::getStringWidth(f, help) + 28.0f;
        juce::Rectangle<float> r(800.0f - w * 0.5f, 652.0f, w, 26.0f);
        g.setColour(kInk); g.fillRoundedRectangle(r, 13.0f);
        g.setColour(kAmber.withAlpha(0.8f)); g.drawRoundedRectangle(r, 13.0f, 1.0f);
        g.setFont(f); g.setColour(kCream); g.drawText(help, r, juce::Justification::centred, false);
    }
}

juce::String PedalSteelAudioProcessorEditor::neckNoteText(juce::Point<float> d) const {
    // "String 3 (G#4) at fret 12 = G#5": open tuning of the current steel voice (pedals/levers bend on top of this)
    const int s = stringAt(d.y);
    const float fret = hover == Pluck ? barFret : std::round(xToFret(d.x));
    const int style = (int) proc.apvts.getRawParameterValue(ParamIDs::presetStyle)->load();
    const int open = PedalSteelEngine::openStringMidi(style, s), fr = juce::roundToInt(fret);
    auto name = [](int n) { return juce::MidiMessage::getMidiNoteName(n, true, true, 4); };
    return "String " + juce::String(s + 1) + " (" + name(open) + ")  at fret " + juce::String(fr) + "  =  " + name(open + fr)
         + (hover == Pluck ? juce::String("   pluck at the bar") : juce::String("   click to play, drag to slide or strum"));
}

juce::String PedalSteelAudioProcessorEditor::tooltipFor(Ctl c) const {
    if (auto* p = param(c)) {
        juce::String s = panelLabel(c);
        if (c <= Volume) s << "  " << valueText(c);
        if (c <= Volume && c != Trim) s << "  (double-click its value box to type)";
        s << "  -  " << p->getName(64);
        if (const int cc = proc.midiLearn.ccFor(p->paramID); cc >= 0) s << "  (MIDI CC " << cc << ")";
        return s;
    }
    switch (c) {
        case Prev: return "Previous preset";
        case Next: return "Next preset";
        case Display: return "Presets: click to browse, right-click for the quick list, scroll to step";
        case MidiLearnBtn: return "MIDI LEARN: assign hardware controllers. Right-click a control for its MIDI options";
        case Fretboard: case Pluck: return neckNoteText(hoverPos);
        default: return {};
    }
}

void PedalSteelAudioProcessorEditor::drawTooltip(juce::Graphics& g) {
    if (browser || saveDlg || modePanel) return;   // an open overlay has the user's attention
    const bool keyboardTip = hover == None && showFocus && focusCtl != None;
    if ((hover == None && ! keyboardTip) || dragging != None || proc.midiLearn.learnMode) return;
    const bool neck = hover == Fretboard || hover == Pluck;
    if (! keyboardTip && juce::Time::getMillisecondCounterHiRes() * 0.001 - hoverSince < (neck ? 0.25 : 0.6)) return;
    const auto s = tooltipFor(keyboardTip ? focusCtl : hover); if (s.isEmpty()) return;
    const auto anchor = keyboardTip ? juce::Point<float>(ctlBounds(focusCtl).getCentreX(), ctlBounds(focusCtl).getBottom() - 18.0f) : hoverPos;
    const float fh = juce::jmax(13.0f, 10.0f / juce::jmax(scale, 0.1f));   // at least ~10 screen px in small windows
    juce::Font f(juce::FontOptions(poppinsMedium).withHeight(fh));
    const float w = juce::GlyphArrangement::getStringWidth(f, s) + fh * 1.7f, h = fh * 2.0f;
    float x = juce::jlimit(6.0f, kDesignW - w - 6.0f, anchor.x - w * 0.5f);
    float y = anchor.y + 24.0f; if (y + h > kDesignH - 6.0f) y = anchor.y - h - 14.0f;
    juce::Rectangle<float> r(x, y, w, h);
    g.setColour(juce::Colours::black.withAlpha(0.35f)); g.fillRoundedRectangle(r.translated(0, 2), 7.0f);
    g.setColour(kInk); g.fillRoundedRectangle(r, 7.0f);
    g.setColour(kCream.withAlpha(0.35f)); g.drawRoundedRectangle(r, 7.0f, 1.0f);
    g.setFont(f); g.setColour(kCream); g.drawText(s, r, juce::Justification::centred, false);
}

void PedalSteelAudioProcessorEditor::timerCallback() {
    const bool snap = std::exchange(snapAnimations, false);
    bool dirty = false, steelDirty = false;
    animTime += 1.0 / kFps;

    for (int c = Tone; c <= Mode; ++c) {
        const float v = norm((Ctl) c);
        if (v != lastNorm[(size_t) c]) {
            lastNorm[(size_t) c] = v; dirty = true;
            for (size_t i = 0; i < a11y.size(); ++i) if (a11yCtl[i] == (Ctl) c) a11y[i]->valueChanged();   // tell screen readers
        }
    }
    // pedals and levers ease toward their state (~80 ms)
    for (int i = 0; i < 5; ++i) {
        const float target = norm((Ctl) (PedA + i)) > 0.5f ? 1.0f : 0.0f; float& a = pedalAnim[(size_t) i];
        if (a != target) { a = snap ? target : (std::abs(target - a) < 0.02f ? target : a + (target - a) * 0.3f); dirty = true; }
    }
    // the steel: what's sounding, and where the bar is
    const auto st = proc.getLiveStatus();
    for (int s = 0; s < 10; ++s) {
        const float nb = st.stringBendSemis[(size_t) s] - st.stringPitchSemis[(size_t) s] * 0.0f;
        if (std::abs(nb - stringBend[(size_t) s]) > 0.02f) { stringBend[(size_t) s] = nb; steelDirty = true; }
    }
    // the engine's own pedal state counts only while audio is running (otherwise it is a frozen snapshot)
    const auto blocks = proc.getBlocksProcessed();
    staleTicks = blocks != lastBlocks ? 0 : staleTicks + 1; lastBlocks = blocks;
    const bool live = staleTicks < kFps / 4;
    const float lp[5] = { live ? st.pedalA : 0.0f, live ? st.pedalB : 0.0f, live ? st.pedalC : 0.0f, live ? st.leverLKL : 0.0f, live ? st.leverLKR : 0.0f };
    for (int i = 0; i < 5; ++i)
        if (std::abs(lp[i] - livePedal[(size_t) i]) > 0.01f || (lp[i] == 0.0f && livePedal[(size_t) i] != 0.0f)) { livePedal[(size_t) i] = lp[i]; dirty = true; }
    bool anySounding = false;
    for (int s = 0; s < 10; ++s) {
        float& e = stringEnergy[(size_t) s];
        const float ne = juce::jmax(st.stringEnergy[(size_t) s], e * 0.86f);
        if (std::abs(ne - e) > 1e-3f || ne > 0.01f) steelDirty = true;
        e = ne < 0.005f ? 0.0f : ne; anySounding |= e > 0.0f;
    }
    if (dragging != Fretboard && st.activeVoiceCount > 0 && st.barPositionFret > 0.25f) barFret = st.barPositionFret;
    if (barFretShown != barFret) {
        barFretShown = snap || std::abs(barFret - barFretShown) < 0.01f ? barFret : barFretShown + (barFret - barFretShown) * 0.35f;
        steelDirty = true;
    }
    juce::ignoreUnused(anySounding);

    const float peak = juce::jmax(proc.getOutputPeakL(), proc.getOutputPeakR());
    const float db = juce::Decibels::gainToDecibels(peak, -100.0f);
    const int lit = juce::jlimit(0, meterSegments, juce::roundToInt((db + 48.0f) / 48.0f * meterSegments));
    if (lit != meterLit) { meterLit = lit; repaint((R(sp_meter_on) * scale).getSmallestIntegerContainer().expanded(2)); }
    if (((int) (animTime * kFps)) % 8 == 0 || snap) modifiedShown = presetModified();
    if (const int sk = skinForCurrentPreset(); sk != skin) {
        bgScaledPrev = bgScaled.createCopy(); skin = sk; rebuildScaledBackground();
        skinFade = snap ? 1.0f : 0.0f; dirty = true;
    }
    if (! hintDismissed || hintAlpha > 0.0f) {
        hintClock += 1.0 / kFps;
        const float target = (! hintDismissed && hintClock > 1.0 && ! browser && ! saveDlg && ! proc.midiLearn.learnMode) ? 1.0f : 0.0f;
        const float na = snap ? target : juce::jlimit(0.0f, 1.0f, hintAlpha + (target > hintAlpha ? 1.0f : -1.0f) / (0.4f * kFps));
        if (na != hintAlpha) { hintAlpha = na; repaint((hintBounds().expanded(8) * scale).getSmallestIntegerContainer()); }
    }
    if (skinFade < 1.0f) { skinFade = juce::jmin(1.0f, skinFade + 1.0f / (0.35f * kFps)); dirty = true; if (skinFade >= 1.0f) bgScaledPrev = {}; }
    {   // marquee for names longer than the glass: hold 1.5 s, one dot column per 50 ms, hold, back to the start
        const auto text = displayText();
        int cols = 0; for (int i = 0; i < text.length(); ++i) cols += 6;
        const int over = juce::jmax(0, cols - 1 - (int) (2.0f * 352.0f / dispPitchX));
        if (text != lastDisplay) { dispScroll = 0; dispScrollClock = 0.0; rebuildDisplay(); dirty = true; }
        else if (over > 0) {
            dispScrollClock += 1.0 / kFps;
            const double hold = 1.5, step = 0.05, run = over * step, cycle = hold + run + hold;
            const double t = std::fmod(dispScrollClock, cycle);
            const int want = t < hold ? 0 : t < hold + run ? (int) ((t - hold) / step) : over;
            if (want != dispScroll) { dispScroll = want; rebuildDisplay(); repaint((R(displayBox) * scale).getSmallestIntegerContainer()); }
        }
    }
    if (proc.midiLearn.getRevision() != lastLearnRevision) { lastLearnRevision = proc.midiLearn.getRevision(); dirty = true; }
    if (proc.midiLearn.learnMode && proc.midiLearn.armedParam().isNotEmpty()) dirty = true;   // pulse
    // tooltip appears after a short hover
    const bool tip = hover != None && dragging == None
                     && juce::Time::getMillisecondCounterHiRes() * 0.001 - hoverSince >= ((hover == Fretboard || hover == Pluck) ? 0.25 : 0.6);
    if (tip != tipShown) { tipShown = tip; dirty = true; }

    if (dirty) repaint();
    else if (steelDirty) repaint((kSteelArea * scale).getSmallestIntegerContainer());
}

// ---------------------------------------------------------------------------------------------------------------------
// interaction

PedalSteelAudioProcessorEditor::Ctl PedalSteelAudioProcessorEditor::hitTest(juce::Point<float> d) const {
    auto inCircle = [&](juce::Rectangle<float> b) { return d.getDistanceFrom(b.getCentre()) <= b.getWidth() * 0.5f + 2.0f; };
    for (int i = Tone; i <= Trim; ++i) if (inCircle(ctlBounds((Ctl) i))) return (Ctl) i;
    for (Ctl c : { Volume, PedA, PedB, PedC, LKL, LKR, Mode, Prev, Next, Display, MidiLearnBtn, Fretboard, Pluck })
        if (ctlBounds(c).contains(d)) return c;
    return None;
}

void PedalSteelAudioProcessorEditor::mouseMove(const juce::MouseEvent& e) {
    hoverPos = toDesign(e.position);
    const auto c = hitTest(hoverPos);
    if (c == None && pillAt(hoverPos) != None) { setMouseCursor(juce::MouseCursor::IBeamCursor); if (hover != None) { hover = None; repaint(); } return; }
    if (c != hover) { hover = c; hoverSince = juce::Time::getMillisecondCounterHiRes() * 0.001; repaint(); }
    else if (c == Fretboard || c == Pluck) repaint();   // guide line + note tooltip follow the mouse
    const bool learnClick = proc.midiLearn.learnMode && isLearnable(c);
    setMouseCursor(c == None ? juce::MouseCursor::NormalCursor
                   : (learnClick || c >= PedA) ? juce::MouseCursor::PointingHandCursor
                   : c == Volume ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor);
}

void PedalSteelAudioProcessorEditor::mouseExit(const juce::MouseEvent&) {
    if (hover != None) { hover = None; repaint(); }
}

void PedalSteelAudioProcessorEditor::runMenu(juce::PopupMenu& menu, juce::Rectangle<float> target, std::function<void(int)> cb) {
    if (menuHook) { menuHook(menu, cb); return; }
    menu.setLookAndFeel(&menuLnf);
    auto opts = juce::PopupMenu::Options().withStandardItemHeight(30);
    if (! target.isEmpty()) {
        auto area = localAreaToGlobal((target * scale).toNearestInt());
        opts = opts.withTargetScreenArea(area).withMinimumWidth(area.getWidth());
    }
    juce::Component::SafePointer<PedalSteelAudioProcessorEditor> safe(this);
    menu.showMenuAsync(opts, [safe, cb](int r) { if (safe != nullptr && r > 0) cb(r); });
}

void PedalSteelAudioProcessorEditor::deferred(std::function<void(PedalSteelAudioProcessorEditor&)> fn) {
    juce::Component::SafePointer<PedalSteelAudioProcessorEditor> safe(this);
    juce::MessageManager::callAsync([safe, fn] { if (safe != nullptr) fn(*safe); });
}

void PedalSteelAudioProcessorEditor::placeOverlays() {
    // a component's transform scales about the editor's origin, so a design-space position p lands at p * s on screen
    const float W = (float) getWidth(), Hh = (float) getHeight();
    if (modePanel) {   // at least 3/4 size, its bottom-left on the MODE box, kept on screen
        const float s = juce::jmax(scale, 0.75f), h = modePanel->getHeight() * s;
        const float x = juce::jmin(hitMode[0] * scale, W - modePanel->getWidth() * s - 4.0f), y = juce::jmax(4.0f, hitMode[1] * scale - 8.0f * scale - h);
        modePanel->setTopLeftPosition(juce::roundToInt(x / s), juce::roundToInt(y / s)); modePanel->setTransform(juce::AffineTransform::scale(s));
    }
    if (browser) {     // same frame as the deck; its text grows in small windows
        browser->setTopLeftPosition(14, 104); browser->setTransform(juce::AffineTransform::scale(scale));
        browser->setTextScale(0.66f / scale);
    }
    if (saveDlg) {     // at least 2/3 size, centred, never larger than the window
        const float s = juce::jmin(juce::jmax(scale, 0.66f), (W - 16.0f) / saveDlg->getWidth(), (Hh - 16.0f) / saveDlg->getHeight());
        const float x = (W - saveDlg->getWidth() * s) * 0.5f, y = (Hh - saveDlg->getHeight() * s) * 0.5f;
        saveDlg->setTopLeftPosition(juce::roundToInt(x / s), juce::roundToInt(y / s)); saveDlg->setTransform(juce::AffineTransform::scale(s));
    }
}

void PedalSteelAudioProcessorEditor::closeOverlays() {
    modePanel.reset(); saveDlg.reset(); browser.reset();
    repaint();
}

void PedalSteelAudioProcessorEditor::showModeMenu() {
    auto* mp = dynamic_cast<juce::AudioParameterChoice*>(param(Mode)); if (mp == nullptr) return;
    if (modePanel != nullptr) { modePanel.reset(); repaint(); return; }
    modePanel = std::make_unique<ModePanel>(kit, *mp);
    modePanel->onClose = [this] { deferred([](PedalSteelAudioProcessorEditor& e) { e.modePanel.reset(); e.repaint(); }); };
    addAndMakeVisible(*modePanel); placeOverlays();
}

void PedalSteelAudioProcessorEditor::openBrowser() {
    modePanel.reset();
    if (browser == nullptr) {
        browser = std::make_unique<PresetBrowser>(kit, proc);
        browser->onClose = [this] { deferred([](PedalSteelAudioProcessorEditor& e) { e.saveDlg.reset(); e.browser.reset(); e.repaint(); }); };
        browser->onSaveAs = [this] { openSaveDialog(); };
        addAndMakeVisible(*browser);
    }
    placeOverlays(); browser->toFront(true);
}

void PedalSteelAudioProcessorEditor::openSaveDialog() {
    modePanel.reset();
    saveDlg = std::make_unique<PresetSaveDialog>(kit, proc);
    saveDlg->onClose = [this] { deferred([](PedalSteelAudioProcessorEditor& e) { e.saveDlg.reset(); e.repaint(); }); };
    saveDlg->onSaved = [this](const juce::File&) { if (browser) browser->refresh(); };
    addAndMakeVisible(*saveDlg); placeOverlays(); saveDlg->toFront(true);
}

void PedalSteelAudioProcessorEditor::showPresetMenu() {
    juce::PopupMenu menu, user;
    const int cur = proc.getCurrentProgram(); const bool isUser = proc.getActiveUserPresetName().isNotEmpty();
    menu.addSectionHeader("Factory");
    for (int i = 0; i < proc.getNumPrograms(); ++i) menu.addItem(1 + i, proc.getProgramName(i), true, ! isUser && i == cur);
    auto files = std::make_shared<std::vector<juce::File>>(proc.getUserPresetFiles());
    for (size_t j = 0; j < files->size(); ++j)
        user.addItem(1000 + (int) j, (*files)[j].getFileNameWithoutExtension(), true,
                     isUser && (*files)[j].getFileNameWithoutExtension() == proc.getActiveUserPresetName());
    if (! files->empty()) menu.addSubMenu("User presets", user);
    menu.addSeparator();
    if (modifiedShown) menu.addItem(5003, "Revert to saved preset");
    menu.addItem(5002, "Browse presets...");
    menu.addItem(5000, "Save preset...");
    menu.addItem(5001, "Show presets folder");
    runMenu(menu, R(displayBox), [this, files](int r) {
        if (r < 1000) { proc.setCurrentProgram(r - 1); return; }
        if (r < 5000) { proc.loadUserPreset((*files)[(size_t) (r - 1000)]); return; }
        if (r == 5002) { openBrowser(); return; }
        if (r == 5003) {   // back to the preset as loaded
            const auto user = proc.getActiveUserPresetName();
            if (user.isNotEmpty()) proc.loadUserPreset(PedalSteelAudioProcessor::getUserPresetsFolder().getChildFile(user + ".nvps"));
            else proc.loadPreset(proc.getCurrentProgram());
            return;
        }
        if (r == 5001) { auto dir = PedalSteelAudioProcessor::getUserPresetsFolder(); dir.createDirectory(); dir.revealToUser(); return; }
        openSaveDialog();
    });
}

void PedalSteelAudioProcessorEditor::showContextMenu(Ctl c) {
    auto* p = param(c); if (p == nullptr) return;
    auto& ml = proc.midiLearn; const int cc = ml.ccFor(p->paramID);
    juce::PopupMenu menu;
    menu.addSectionHeader(panelLabel(c));
    menu.addItem(1, "MIDI Learn");
    menu.addItem(2, cc >= 0 ? "Forget MIDI CC " + juce::String(cc) : juce::String("No MIDI CC assigned"), cc >= 0);
    menu.addSeparator();
    menu.addItem(3, "Reset to default");
    if (ml.numMappings() > 0) { menu.addSeparator(); menu.addItem(4, "Forget all MIDI assignments"); }
    const juce::String id = p->paramID;
    runMenu(menu, {}, [this, id, c](int r) {
        auto& m = proc.midiLearn;
        if (r == 1) { m.learnMode = true; m.armLearn(id); repaint(); }
        else if (r == 2) m.clear(id);
        else if (r == 3) { if (auto* q = param(c)) { q->beginChangeGesture(); q->setValueNotifyingHost(q->getDefaultValue()); q->endChangeGesture(); } }
        else if (r == 4) m.clearAll();
    });
}

void PedalSteelAudioProcessorEditor::mouseDown(const juce::MouseEvent& e) {
    const auto d = toDesign(e.position); const auto c = hitTest(d);
    dragging = None; hover = None; showFocus = false; repaint();
    if (! hintDismissed && hintAlpha > 0.5f && hintBounds().contains(d)) { dismissHint(); return; }   // click on the tip closes it
    if (modePanel != nullptr) { modePanel.reset(); repaint(); if (c == Mode) return; }   // a click anywhere else closes the mode menu
    if (e.mods.isPopupMenu()) { if (isLearnable(c)) showContextMenu(c); else if (c == Display) showPresetMenu(); return; }

    if ((e.mods.isCtrlDown() || e.mods.isAltDown()) && c >= Tone && c <= Volume) {   // ctrl/alt-click: back to default
        if (auto* p = param(c)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->getDefaultValue()); p->endChangeGesture(); }
        return;
    }
    auto& ml = proc.midiLearn;
    if (c == MidiLearnBtn) { ml.learnMode = ! ml.learnMode; if (! ml.learnMode) ml.cancelLearn(); repaint(); return; }
    if (ml.learnMode && isLearnable(c)) { ml.armLearn(param(c)->paramID); repaint(); return; }

    switch (c) {
        case PedA: case PedB: case PedC: case LKL: case LKR: toggle(c); return;
        case Prev: proc.stepPreset(-1); return;
        case Next: proc.stepPreset(+1); return;
        case Display: openBrowser(); return;
        case Mode: showModeMenu(); return;
        case Fretboard: {   // put the bar down here and pick the string under the mouse
            dismissHint();
            barFret = std::round(xToFret(d.x)); lastString = stringAt(d.y);   // a click plays in tune: nearest fret
            playString(lastString, barFret); dragging = Fretboard; return;
        }
        case Pluck: lastString = stringAt(d.y); playString(lastString, barFret); dragging = Pluck; return;
        case None: case NumCtl: case MidiLearnBtn: return;
        default: break;
    }
    dragging = c; dragStartNorm = norm(c); dragStartY = d.y; dragStartX = d.x;
    if (auto* p = param(c)) p->beginChangeGesture();
    if (c == Volume) setNorm(c, unanchored(d.x, kVolume, sliderX0, sliderX1));
}

void PedalSteelAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e) {
    if (dragging == None) return;
    const auto d = toDesign(e.position);
    if (dragging == Fretboard || dragging == Pluck) {
        if (dragging == Fretboard) {   // slide every held string with the bar
            barFret = snapFret(xToFret(d.x), e.mods.isShiftDown());
            for (int s = 0; s < 10; ++s) if (heldStrings[(size_t) s]) proc.slideFretboardNote(s, barFret);
        }
        const int s = stringAt(d.y);   // dragging across strings strums them
        if (s != lastString && std::abs(stringY[s] - d.y) < 4.0f) { lastString = s; playString(s, barFret); }
        return;
    }
    if (dragging == Volume) { setNorm(Volume, unanchored(d.x, kVolume, sliderX0, sliderX1)); return; }
    const float speed = e.mods.isShiftDown() ? 0.0008f : 0.004f;   // shift = fine
    setNorm(dragging, dragStartNorm + ((dragStartY - d.y) + (d.x - dragStartX)) * speed);
}

void PedalSteelAudioProcessorEditor::mouseUp(const juce::MouseEvent& e) {
    if (dragging == Fretboard || dragging == Pluck) releaseAllStrings();
    else if (dragging != None) { if (auto* p = param(dragging)) p->endChangeGesture(); }
    dragging = None;
    mouseMove(e);
}

juce::Rectangle<float> PedalSteelAudioProcessorEditor::pillBounds(Ctl c) const {
    // the outlined value boxes: under each deck knob, next to the volume slider, and the dB readout by TRIM
    const int i = c == Volume ? 6 : c == Trim ? 8 : (int) c;
    if (i < 0 || i > 8 || i == 7) return {};
    const auto& t = txt[i];
    const float w = c == Trim ? 58.0f : 74.0f, h = c == Trim ? 24.0f : 28.0f;
    return { t.cx - w * 0.5f, t.cy - h * 0.5f, w, h };
}

PedalSteelAudioProcessorEditor::Ctl PedalSteelAudioProcessorEditor::pillAt(juce::Point<float> d) const {
    for (Ctl c : { Tone, Pick, Glide, Swell, Echo, Reverb, Volume, Trim })
        if (pillBounds(c).contains(d)) return c;
    return None;
}

bool PedalSteelAudioProcessorEditor::applyTypedValue(int ctl, const juce::String& text) {
    // "145", "145 ms", "65 %", "-3.5 dB", "0.65" (fractions < 1 for percent controls mean 0..1)
    auto* p = param((Ctl) ctl); if (p == nullptr) return false;
    const auto t = text.trim(); if (t.isEmpty() || ! t.containsAnyOf("0123456789")) return false;
    float v = t.retainCharacters("0123456789.-+").getFloatValue();
    const bool percentCtl = ctl != Glide && ctl != Trim;
    if (percentCtl) v = (v <= 1.0f && ! t.containsChar('%') && t.containsChar('.')) ? v : v / 100.0f;
    const auto range = p->getNormalisableRange();
    v = juce::jlimit(range.start, range.end, v);
    p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(v)); p->endChangeGesture();
    flashValue((Ctl) ctl); repaint(); return true;
}

void PedalSteelAudioProcessorEditor::startValueEdit(Ctl c) {
    auto b = pillBounds(c); if (b.isEmpty()) return;
    finishValueEdit(false);
    editingCtl = c;
    valueEditor = std::make_unique<juce::TextEditor>();
    auto& ed = *valueEditor;
    ed.setFont(juce::Font(juce::FontOptions(poppinsMedium).withHeight(18.0f * scale)));
    ed.setJustification(juce::Justification::centred);
    ed.setColour(juce::TextEditor::backgroundColourId, kInk.withAlpha(1.0f));
    ed.setColour(juce::TextEditor::textColourId, kCream);
    ed.setColour(juce::TextEditor::highlightColourId, kAmber.withAlpha(0.35f));
    ed.setColour(juce::TextEditor::highlightedTextColourId, juce::Colour(0xfffff1d6));
    ed.setColour(juce::TextEditor::outlineColourId, kAmber);
    ed.setColour(juce::TextEditor::focusedOutlineColourId, kAmber);
    ed.setColour(juce::CaretComponent::caretColourId, kAmber);
    ed.setIndents(4, 0);
    ed.setText(valueText(c).upToFirstOccurrenceOf(" ", false, false), false);
    ed.setBounds((b * scale).getSmallestIntegerContainer());
    ed.onReturnKey = [this] { finishValueEdit(true); };
    ed.onEscapeKey = [this] { finishValueEdit(false); };
    ed.onFocusLost = [this] { finishValueEdit(true); };
    addAndMakeVisible(ed);
    ed.selectAll();
    if (isShowing()) ed.grabKeyboardFocus();
}

void PedalSteelAudioProcessorEditor::finishValueEdit(bool apply) {
    if (valueEditor == nullptr) return;
    auto ed = std::move(valueEditor);   // clear first: focus changes below must not re-enter
    if (apply) applyTypedValue(editingCtl, ed->getText());
    editingCtl = None;
    removeChildComponent(ed.get());
    juce::MessageManager::callAsync([e = ed.release()] { delete e; });   // don't delete it inside its own callback
    repaint();
}

void PedalSteelAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent& e) {
    if (const auto pc = pillAt(toDesign(e.position)); pc != None && ! proc.midiLearn.learnMode) { startValueEdit(pc); return; }
    const auto c = hitTest(toDesign(e.position));
    if (c >= Tone && c <= Volume && ! proc.midiLearn.learnMode)
        if (auto* p = param(c)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->getDefaultValue()); p->endChangeGesture(); }
}

void PedalSteelAudioProcessorEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    const auto c = hitTest(toDesign(e.position));
    if (c >= Tone && c <= Volume) {
        if (auto* p = param(c)) { p->beginChangeGesture(); setNorm(c, norm(c) + w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f)); p->endChangeGesture(); }
    } else if (c == Display || c == Prev || c == Next) {
        if (w.deltaY != 0) proc.stepPreset(w.deltaY > 0 ? -1 : 1);
    } else if (c == Mode) {
        if (auto* mp = dynamic_cast<juce::AudioParameterChoice*>(param(Mode)); mp != nullptr && w.deltaY != 0) {
            const int n = juce::jlimit(0, mp->choices.size() - 1, mp->getIndex() + (w.deltaY > 0 ? -1 : 1));
            mp->beginChangeGesture(); *mp = n; mp->endChangeGesture();
        }
    }
}

void PedalSteelAudioProcessorEditor::moveFocus(int dir) {
    static const Ctl order[] = { Display, Tone, Pick, Glide, Swell, Echo, Reverb, PedA, PedB, PedC, LKL, LKR, Mode, Volume, MidiLearnBtn, Trim };
    constexpr int n = (int) (sizeof(order) / sizeof(order[0]));
    int i = -1; for (int j = 0; j < n; ++j) if (order[j] == focusCtl) i = j;
    i = i < 0 ? (dir > 0 ? 0 : n - 1) : (i + dir + n) % n;
    focusCtl = order[i]; showFocus = true; repaint();
}

void PedalSteelAudioProcessorEditor::drawFocus(juce::Graphics& g) {
    if (! showFocus || focusCtl == None) return;
    auto b = ctlBounds(focusCtl).expanded(3.0f);
    g.setColour(kAmber.withAlpha(0.25f));
    if (focusCtl <= Trim) g.drawEllipse(b.expanded(2.0f), 4.0f); else g.drawRoundedRectangle(b.expanded(2.0f), 8.0f, 4.0f);
    g.setColour(kAmber.withAlpha(0.95f));
    if (focusCtl <= Trim) g.drawEllipse(b, 1.6f); else g.drawRoundedRectangle(b, 7.0f, 1.6f);
}

bool PedalSteelAudioProcessorEditor::keyPressed(const juce::KeyPress& k) {
    auto& ml = proc.midiLearn;
    if (k == juce::KeyPress::escapeKey && (saveDlg || browser || modePanel)) {
        if (saveDlg) saveDlg.reset(); else if (browser) browser.reset(); else modePanel.reset();
        repaint(); return true;
    }
    if (k == juce::KeyPress::escapeKey) {
        if (ml.learnMode) { ml.learnMode = false; ml.cancelLearn(); repaint(); return true; }
        if (showFocus) { showFocus = false; focusCtl = None; repaint(); return true; }
        return false;
    }
    const int code = k.getKeyCode();
    if (code == juce::KeyPress::tabKey) { moveFocus(k.getModifiers().isShiftDown() ? -1 : 1); return true; }
    const bool continuous = showFocus && focusCtl >= Tone && focusCtl <= Volume;
    if (continuous) {   // arrows nudge (shift = fine), page up/down = big steps, home/end = the ends
        const float fine = k.getModifiers().isShiftDown() ? 0.002f : 0.01f;
        float delta = 0.0f, to = 0.0f; bool set = false;
        if (code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey) delta = fine;
        else if (code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey) delta = -fine;
        else if (code == juce::KeyPress::pageUpKey) delta = 0.1f;
        else if (code == juce::KeyPress::pageDownKey) delta = -0.1f;
        else if (code == juce::KeyPress::homeKey) { set = true; to = 0.0f; }
        else if (code == juce::KeyPress::endKey) { set = true; to = 1.0f; }
        if (delta != 0.0f || set) {
            if (auto* p = param(focusCtl)) { p->beginChangeGesture(); setNorm(focusCtl, set ? to : norm(focusCtl) + delta); p->endChangeGesture(); }
            repaint(); return true;
        }
    }
    if (showFocus && (code == juce::KeyPress::spaceKey || code == juce::KeyPress::returnKey)) {
        switch (focusCtl) {
            case PedA: case PedB: case PedC: case LKL: case LKR: toggle(focusCtl); return true;
            case Mode: showModeMenu(); return true;
            case Display: openBrowser(); return true;
            case MidiLearnBtn: ml.learnMode = ! ml.learnMode; if (! ml.learnMode) ml.cancelLearn(); repaint(); return true;
            default:
                if (ml.learnMode && isLearnable(focusCtl)) { ml.armLearn(param(focusCtl)->paramID); repaint(); return true; }
                break;
        }
    }
    if (code == juce::KeyPress::leftKey) { proc.stepPreset(-1); return true; }
    if (code == juce::KeyPress::rightKey) { proc.stepPreset(+1); return true; }
    return false;
}
