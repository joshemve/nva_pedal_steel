#include "../../Source/UiLayout.h"
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Params.h"

// draws a PopupMenu with a LookAndFeel the way the menu window would (header/items/separators, one highlighted row)
static juce::Image renderMenu(const juce::PopupMenu& menu, juce::LookAndFeel& lnf, int highlightRow, int minWidth = 0) {
    struct Row { bool sep, header, active, ticked, sub; juce::String text; int h; };
    std::vector<Row> rows; int w = minWidth;
    for (juce::PopupMenu::MenuItemIterator it(menu); it.next();) {
        const auto& item = it.getItem(); Row r{ item.isSeparator, item.isSectionHeader, item.isEnabled, item.isTicked, item.subMenu != nullptr, item.text, 30 };
        int iw = 0, ih = 0; lnf.getIdealPopupMenuItemSize(item.text, item.isSeparator, 30, iw, ih); r.h = ih;
        if (item.isSectionHeader) r.h = 26;
        w = juce::jmax(w, iw); rows.push_back(r);
    }
    const int border = lnf.getPopupMenuBorderSize(); int h = 2 * border; for (auto& r : rows) h += r.h;
    juce::Image img(juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
    { juce::Graphics g(img);
    lnf.drawPopupMenuBackground(g, w, h);
    int y = border, row = 0;
    for (auto& r : rows) {
        juce::Rectangle<int> a(0, y, w, r.h);
        if (r.header) lnf.drawPopupMenuSectionHeader(g, a, r.text);
        else lnf.drawPopupMenuItem(g, a, r.sep, r.active, row == highlightRow, r.ticked, r.sub, r.text, {}, nullptr, nullptr);
        y += r.h; ++row;
    }
    }
    return img;
}

static bool hintWasShown = false;

int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File outFile = (argc > 1)
        ? juce::File(juce::String(argv[1]))
        : juce::File::getCurrentWorkingDirectory().getChildFile("renders/ui_screenshot.png");
    outFile.getParentDirectory().createDirectory();

    PedalSteelAudioProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    // Load Preset 15 (Dark Side Phase Steel) to showcase SYNC + Analog Phaser Character + E9th Pedals A+B
    processor.loadPreset(14);
    processor.triggerChordAudition(0);
    processor.setLivePedalState(0, 0.85f);
    processor.setLivePedalState(1, 1.0f);

    juce::AudioBuffer<float> audioBuf(2, 512);
    juce::MidiBuffer midiBuf;
    for (int i = 0; i < 16; ++i) {
        processor.processBlock(audioBuf, midiBuf);
    }

    // Verify full state & user-preset XML round-trip integrity
    {
        juce::MemoryBlock stateBlob;
        processor.getStateInformation(stateBlob);
        if (stateBlob.getSize() == 0) {
            std::cerr << "State serialization failed!" << std::endl;
            return 2;
        }
        processor.setStateInformation(stateBlob.getData(), static_cast<int>(stateBlob.getSize()));

        auto tempPreset = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("_nvps_audit_roundtrip.nvps");
        if (!processor.saveUserPreset(tempPreset) || !processor.loadUserPreset(tempPreset)) {
            tempPreset.deleteFile();
            std::cerr << "User preset .nvps round-trip failed!" << std::endl;
            return 3;
        }
        tempPreset.deleteFile();
        processor.loadPreset(14);
        processor.setLivePedalState(0, 0.85f);
        processor.setLivePedalState(1, 1.0f);
        for (int i = 0; i < 4; ++i) processor.processBlock(audioBuf, midiBuf);
    }

    // the approved design's state: preset 01, TONE 65 %, PICK 40 %, GLIDE 145 ms, SWELL 35 %, ECHO 18 %, REVERB 28 %,
    // pedals A + B down, volume pedal 72 %, Smart Auto-Pedal E9, trim 0.0 dB
    processor.loadPreset(0);
    auto setPlain = [&](const char* id, float v) {
        if (auto* p = processor.apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(v));
    };
    setPlain(ParamIDs::toneWarmth, 0.65f); setPlain(ParamIDs::pickSnap, 0.40f); setPlain(ParamIDs::barGlide, 145.0f);
    setPlain(ParamIDs::autoSwell, 0.35f); setPlain(ParamIDs::delayMix, 0.18f); setPlain(ParamIDs::reverbMix, 0.28f);
    setPlain(ParamIDs::pedalA, 1.0f); setPlain(ParamIDs::pedalB, 1.0f); setPlain(ParamIDs::pedalC, 0.0f);
    setPlain(ParamIDs::volumePedal, 0.72f); setPlain(ParamIDs::playMode, 0.0f); setPlain(ParamIDs::outputLevel, 0.0f);
    processor.captureCurrentAsBaseline();   // the design's values are "the preset" here (no * in the display)

    // 1) live: four strings picked at fret 7 with the bar down -> strings vibrate, bar moves there, floor pedals pressed
    {
        for (int s = 2; s < 6; ++s) processor.triggerFretboardNote(s, 7.0f);
        for (int i = 0; i < 6; ++i) processor.processBlock(audioBuf, midiBuf);
        { auto st = processor.getLiveStatus(); std::cout << "live: voices " << st.activeVoiceCount << " bar " << st.barPositionFret << " energy";
          for (auto e : st.stringEnergy) std::cout << " " << e; std::cout << std::endl; }
        std::unique_ptr<juce::AudioProcessorEditor> live(processor.createEditor());
        live->setSize(1600, 1000);
        if (auto* e = dynamic_cast<PedalSteelAudioProcessorEditor*>(live.get())) e->refreshVisualizer();
        juce::Image img = live->createComponentSnapshot(live->getLocalBounds());
        auto liveFile = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_live.png");
        liveFile.deleteFile(); juce::FileOutputStream ls(liveFile);
        if (ls.openedOk()) juce::PNGImageFormat().writeImageToStream(img, ls);
        for (int s = 2; s < 6; ++s) processor.releaseFretboardNote(s);
    }
    // 2) the design state, silent (strings at rest, bar where the design has it)
    processor.triggerPanic();
    for (int i = 0; i < 400; ++i) processor.processBlock(audioBuf, midiBuf);

    // first-launch tip: start from "never shown" so the check below is deterministic, and keep the design shot clean
    auto uiSettings = PedalSteelAudioProcessorEditor::settingsFile(); auto uiBackup = uiSettings.loadFileAsString();
    uiSettings.deleteFile();
    {
        std::unique_ptr<juce::AudioProcessorEditor> first(processor.createEditor()); first->setSize(1600, 1000);
        auto* fe = dynamic_cast<PedalSteelAudioProcessorEditor*>(first.get());
        for (int i = 0; i < 90; ++i) fe->refreshVisualizer();   // ~1.5 s of ticks (refresh snaps, so it is fully in)
        hintWasShown = fe->isHintShowing();
        auto img = first->createComponentSnapshot({ 470, 260, 660, 130 });
        auto f = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_hint.png");
        f.deleteFile(); juce::FileOutputStream os(f); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream(img, os);
        fe->dismissHint();
    }
    {   // editor open cost: construct + first full paint at 1280x800 (what a DAW does when you open the plugin window)
        juce::Thread::sleep(2500);   // the processor's background preload has finished by the time a user opens the window
        for (int rep = 0; rep < 3; ++rep) {
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            std::unique_ptr<juce::AudioProcessorEditor> probe(processor.createEditor());
            const double t1 = juce::Time::getMillisecondCounterHiRes();
            juce::Image fb(juce::Image::ARGB, probe->getWidth(), probe->getHeight(), true);
            { juce::Graphics g(fb); probe->paintEntireComponent(g, false); }
            const double t2 = juce::Time::getMillisecondCounterHiRes();
            std::cout << "  editor open: construct " << (t1 - t0) << " ms, first paint " << (t2 - t1) << " ms" << std::endl;
        }
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    editor->setSize(1600, 1000);

    if (auto* psEditor = dynamic_cast<PedalSteelAudioProcessorEditor*>(editor.get())) {
        psEditor->refreshVisualizer();
    }

    juce::Image snapshot = editor->createComponentSnapshot(editor->getLocalBounds());
    {   // small window (small_1 labels), design state
        editor->setSize(1000, 625);
        if (auto* e = dynamic_cast<PedalSteelAudioProcessorEditor*>(editor.get())) e->refreshVisualizer();
        auto img = editor->createComponentSnapshot(editor->getLocalBounds());
        auto f = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_small.png");
        f.deleteFile(); juce::FileOutputStream os(f); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream(img, os);
        editor->setSize(1600, 1000);
        if (auto* e = dynamic_cast<PedalSteelAudioProcessorEditor*>(editor.get())) e->refreshVisualizer();
    }

    // interaction tests: synthetic mouse events on the real editor (1600x1000 = design px), checked on the processor
    {
        int fails = 0;
        auto check = [&](bool ok, const std::string& what) { std::cout << (ok ? "  PASS " : "  FAIL ") << what << std::endl; fails += ok ? 0 : 1; };
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        auto ev = [&](juce::Point<float> p, juce::ModifierKeys m = {}) {
            return juce::MouseEvent(src, p, m, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, editor.get(), editor.get(), juce::Time::getCurrentTime(),
                                    p, juce::Time::getCurrentTime(), 1, false);
        };
        auto run = [&](int blocks) { for (int i = 0; i < blocks; ++i) processor.processBlock(audioBuf, midiBuf); };
        auto pv = [&](const char* id) { return processor.apvts.getParameter(id)->getValue(); };
        std::cout << "interaction tests:" << std::endl;
        // fretboard: click string 4 a little right of fret 12, drag to fret 15, release (positions from the generated layout)
        const float nx12 = UiLayout::fretAnchor[12][1] + 4.0f, nx15 = UiLayout::fretAnchor[15][1];
        const float ny4 = UiLayout::stringY[3], ny6 = UiLayout::stringY[5];
        editor->mouseDown(ev({ nx12, ny4 })); run(40);
        auto st = processor.getLiveStatus();
        check(st.activeVoiceCount >= 1 && st.stringEnergy[3] > 0.05f, "click on the neck plays the string under the mouse");
        check(std::abs(st.barPositionFret - 12.0f) < 0.05f, "...in tune: snapped to fret 12 (" + std::to_string(st.barPositionFret) + ")");
        editor->mouseDrag(ev({ nx15, ny4 })); run(40);
        st = processor.getLiveStatus();
        check(std::abs(st.barPositionFret - 15.0f) < 0.8f, "dragging along the neck slides the bar");
        editor->mouseDrag(ev({ nx15, ny6 })); run(4);
        st = processor.getLiveStatus();
        check(st.stringEnergy[5] > 0.05f, "dragging across strings strums them");
        editor->mouseUp(ev({ nx15, ny6 })); run(4);
        // pedal C toggles on and off; knee lever LKR
        const float c0 = pv(ParamIDs::pedalC);
        editor->mouseDown(ev({ 1202.0f, 800.0f })); editor->mouseUp(ev({ 1202.0f, 800.0f }));
        check(pv(ParamIDs::pedalC) != c0, "click on pedal C toggles it");
        editor->mouseDown(ev({ 1202.0f, 800.0f })); editor->mouseUp(ev({ 1202.0f, 800.0f }));
        check(pv(ParamIDs::pedalC) == c0, "...and back");
        // knob drag: TONE up by 50 px -> +0.2
        const float t0 = pv(ParamIDs::toneWarmth);
        editor->mouseDown(ev({ 133.0f, 778.0f })); editor->mouseDrag(ev({ 133.0f, 728.0f })); editor->mouseUp(ev({ 133.0f, 728.0f }));
        check(std::abs(pv(ParamIDs::toneWarmth) - juce::jmin(1.0f, t0 + 0.2f)) < 0.01f, "dragging TONE up raises it");
        // volume slider: click at the right end -> 100 %
        editor->mouseDown(ev({ UiLayout::sliderX1 + 4.0f, 935.0f })); editor->mouseUp(ev({ UiLayout::sliderX1 + 4.0f, 935.0f }));
        check(pv(ParamIDs::volumePedal) > 0.98f, "clicking the slider's far end sets 100 %");
        // MIDI LEARN: button on, click REVERB, send CC 21 -> mapped; then CC 21 = 127 moves REVERB to 100 %
        const juce::Point<float> midiBtn { UiLayout::hitMidiLearn[0] + UiLayout::hitMidiLearn[2] * 0.5f, UiLayout::hitMidiLearn[1] + UiLayout::hitMidiLearn[3] * 0.5f };
        editor->mouseDown(ev(midiBtn)); editor->mouseUp(ev(midiBtn));
        check(processor.midiLearn.learnMode, "MIDI LEARN button switches learn mode on");
        editor->mouseDown(ev({ 878.0f, 779.0f })); editor->mouseUp(ev({ 878.0f, 779.0f }));
        check(processor.midiLearn.armedParam() == ParamIDs::reverbMix, "clicking REVERB in learn mode arms it");
        juce::MidiBuffer cc; cc.addEvent(juce::MidiMessage::controllerEvent(1, 21, 10), 0);
        processor.processBlock(audioBuf, cc); processor.midiLearn.process();
        check(processor.midiLearn.ccFor(ParamIDs::reverbMix) == 21, "moving CC 21 assigns it to REVERB");
        juce::MidiBuffer cc2; cc2.addEvent(juce::MidiMessage::controllerEvent(1, 21, 127), 0);
        processor.processBlock(audioBuf, cc2); processor.midiLearn.process();
        check(pv(ParamIDs::reverbMix) > 0.99f, "CC 21 then drives REVERB");
        juce::MemoryBlock blob; processor.getStateInformation(blob); processor.midiLearn.clearAll();
        processor.setStateInformation(blob.getData(), (int) blob.getSize());
        check(processor.midiLearn.ccFor(ParamIDs::reverbMix) == 21, "the assignment is saved with the session");
        editor->mouseDown(ev(midiBtn)); editor->mouseUp(ev(midiBtn));
        check(! processor.midiLearn.learnMode, "MIDI LEARN button switches learn mode off");
        // preset arrows
        const int pr0 = processor.getCurrentProgram();
        editor->mouseDown(ev({ 1030.0f, 61.0f })); editor->mouseUp(ev({ 1030.0f, 61.0f }));
        check(processor.getCurrentProgram() == (pr0 + 1) % processor.getNumPrograms(), "next-preset arrow steps the preset");
        editor->mouseDown(ev({ 568.0f, 61.0f })); editor->mouseUp(ev({ 568.0f, 61.0f }));
        check(processor.getCurrentProgram() == pr0, "previous-preset arrow steps back");
 {   // paint cost: full frame and the steel region while strings sound, at 1280x800
            editor->setSize(1280, 800);
            juce::Image fb(juce::Image::ARGB, 1280, 800, true);
            auto t0 = juce::Time::getMillisecondCounterHiRes();
            for (int i = 0; i < 30; ++i) { juce::Graphics g(fb); editor->paintEntireComponent(g, false); }
            auto t1 = juce::Time::getMillisecondCounterHiRes();
            for (int i = 0; i < 30; ++i) { juce::Graphics g(fb); g.reduceClipRegion(376, 48, 720, 496); editor->paintEntireComponent(g, false); }
            auto t2 = juce::Time::getMillisecondCounterHiRes();
            std::cout << "  paint: full frame " << (t1 - t0) / 30.0 << " ms, steel region " << (t2 - t1) / 30.0 << " ms" << std::endl;
            editor->setSize(1600, 1000);
        }
        // menus: captured through the editor's hook, checked, acted on, and drawn with the plugin's menu look
        if (auto* pe = dynamic_cast<PedalSteelAudioProcessorEditor*>(editor.get())) {
            juce::PopupMenu lastMenu; std::function<void(int)> lastCb; int menus = 0;
            pe->menuHook = [&](const juce::PopupMenu& m, std::function<void(int)> cb) { lastMenu = m; lastCb = std::move(cb); ++menus; };
            auto items = [&](const juce::PopupMenu& m) { juce::StringArray a; for (juce::PopupMenu::MenuItemIterator it(m); it.next();) a.add(it.getItem().text); return a; };
            std::vector<juce::Image> shots;
            // right-click TONE
            processor.apvts.getParameter(ParamIDs::toneWarmth)->setValueNotifyingHost(0.9f);
            editor->mouseDown(ev({ 133.0f, 778.0f }, juce::ModifierKeys::rightButtonModifier)); editor->mouseUp(ev({ 133.0f, 778.0f }));
            auto it1 = items(lastMenu);
            check(menus == 1 && it1.contains("MIDI Learn") && it1.contains("Reset to default"), "right-click on TONE opens its menu");
            shots.push_back(renderMenu(lastMenu, pe->getMenuLookAndFeel(), 3));
            lastCb(3);
            auto* tp = processor.apvts.getParameter(ParamIDs::toneWarmth);
            check(std::abs(tp->getValue() - tp->getDefaultValue()) < 1e-4f, "...Reset to default resets it");
            editor->mouseDown(ev({ 133.0f, 778.0f }, juce::ModifierKeys::rightButtonModifier)); editor->mouseUp(ev({ 133.0f, 778.0f }));
            lastCb(1);
            check(processor.midiLearn.learnMode && processor.midiLearn.armedParam() == ParamIDs::toneWarmth, "...MIDI Learn arms it");
            processor.midiLearn.cancelLearn(); processor.midiLearn.learnMode = false;
            // preset menu from the display
            editor->mouseDown(ev({ 800.0f, 61.0f }, juce::ModifierKeys::rightButtonModifier)); editor->mouseUp(ev({ 800.0f, 61.0f }));
            auto it2 = items(lastMenu);
            check(menus == 3 && it2.contains(processor.getProgramName(2)) && it2.contains("Save preset..."), "right-clicking the display opens the quick preset list");
            shots.push_back(renderMenu(lastMenu, pe->getMenuLookAndFeel(), 4, 380));
            lastCb(3);
            check(processor.getCurrentProgram() == 2, "...choosing a preset loads it");
            // mode menu (mode_2 panel)
            auto pump = [](int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); };
            editor->mouseDown(ev({ 320.0f, 931.0f })); editor->mouseUp(ev({ 320.0f, 931.0f }));
            check(pe->getModePanel() != nullptr && pe->getModePanel()->isVisible(), "clicking MODE opens the mode panel");
            {
                pe->refreshVisualizer();
                auto img = editor->createComponentSnapshot({ 0, 520, 760, 480 });
                auto f = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_mode.png");
                f.deleteFile(); juce::FileOutputStream os(f); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream(img, os);
            }
            if (auto* mpn = pe->getModePanel()) mpn->choose(1);
            pump(50);
            check(dynamic_cast<juce::AudioParameterChoice*>(processor.apvts.getParameter(ParamIDs::playMode))->getIndex() == 1, "...choosing a mode sets it");
            check(pe->getModePanel() == nullptr, "...and closes the panel");

            // preset browser (pb2_2) and the save dialog (pe1_2)
            editor->mouseDown(ev({ 800.0f, 61.0f })); editor->mouseUp(ev({ 800.0f, 61.0f }));
            auto* br = pe->getBrowser();
            check(br != nullptr && br->isVisible(), "clicking the display opens the preset browser");
            if (br != nullptr) {
                const int nAll = br->numShown();
                check(nAll >= processor.getNumPrograms(), "browser lists every preset (" + std::to_string(nAll) + ")");
                {
                    pe->refreshVisualizer();
                    auto img = editor->createComponentSnapshot(editor->getLocalBounds());
                    auto f = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_browser.png");
                    f.deleteFile(); juce::FileOutputStream os(f); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream(img, os);
                }
                {   // keyboard: arrows move the selection, Enter loads it
                    using KP = juce::KeyPress;
                    br->keyPressed(KP(KP::rightKey)); br->keyPressed(KP(KP::rightKey)); br->keyPressed(KP(KP::downKey));
                    check(br->getSelected() == 1 + br->columns(), "browser: arrow keys move through the grid (" + std::to_string(br->getSelected()) + ")");
                    const int want = br->getSelected(); const auto title = br->shownTitle(want);
                    br->keyPressed(KP(KP::spaceKey));
                    check(processor.getCurrentProgram() == want, "browser: Space loads the selected preset (" + title.toStdString() + ")");
                    // favourites (keep the user's own list)
                    auto favFile = Favourites::file(); auto favBackup = favFile.loadFileAsString();
                    favFile.deleteFile(); br->refresh();
                    br->toggleFavourite(want); br->toggleFavourite(0);
                    br->setFilter("Favourites");
                    check(br->numShown() == 2 && Favourites::load().size() == 2, "favourites: starred presets show under Favourites and are saved");
                    {
                        pe->refreshVisualizer();
                        auto img = editor->createComponentSnapshot(editor->getLocalBounds());
                        auto f = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_favourites.png");
                        f.deleteFile(); juce::FileOutputStream os(f); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream(img, os);
                    }
                    br->toggleFavourite(0); br->toggleFavourite(0);
                    check(br->numShown() == 0, "favourites: un-starring removes them");
                    if (favBackup.isNotEmpty()) favFile.replaceWithText(favBackup); else favFile.deleteFile();
                    br->setFilter("All");
                }
                br->setFilter("Grit"); const int nGrit = br->numShown();
                check(nGrit > 0 && nGrit < nAll, "Grit filter narrows it (" + std::to_string(nGrit) + ")");
                br->setFilter("All"); br->setSearch("swell");
                bool allMatch = br->numShown() > 0; for (int i = 0; i < br->numShown(); ++i) allMatch &= br->shownTitle(i).containsIgnoreCase("swell");
                check(allMatch, "search 'swell' finds only matching presets (" + std::to_string(br->numShown()) + ")");
                br->setSearch(""); br->activate(4, false);
                check(processor.getCurrentProgram() == 4, "clicking a card loads that preset");
                // save dialog
                pe->openSaveDialog();
                auto* dlg = pe->getSaveDialog();
                check(dlg != nullptr && dlg->isVisible(), "Save As opens the preset editor");
                if (dlg != nullptr) {
                    {
                        pe->refreshVisualizer();
                        auto img = editor->createComponentSnapshot(editor->getLocalBounds());
                        auto f = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_save.png");
                        f.deleteFile(); juce::FileOutputStream os(f); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream(img, os);
                    }
                    dlg->setName("ZZ Test Preset"); dlg->setFinish(2); dlg->setCategory("Swell"); dlg->setNotes("made by the UI test");
                    dlg->replaceWithoutAsking = true; dlg->save(); pump(50);
                    auto saved = PedalSteelAudioProcessor::getUserPresetsFolder().getChildFile("ZZ Test Preset.nvps");
                    auto sx = juce::XmlDocument::parse(saved);
                    check(saved.existsAsFile() && sx != nullptr && sx->getStringAttribute("finish") == "redsparkle" && sx->getStringAttribute("category") == "Swell",
                          "Save writes the preset with its finish and category");
                    check(processor.getActiveUserPresetName() == "ZZ Test Preset" && pe->getSaveDialog() == nullptr, "...loads it and closes the dialog");
                    br->setFilter("User"); bool found = false, notesOk = false;
                    for (int i = 0; i < br->numShown(); ++i) if (br->shownTitle(i) == "ZZ Test Preset") { found = true; notesOk = br->shownNotes(i) == "made by the UI test"; }
                    check(found, "...and the browser lists it under User");
                    check(notesOk, "...with the notes written when it was saved");
                    saved.deleteFile();
                }
                {   // small window: browser with bigger text and 3 columns, save dialog at 2/3 size
                    br->setFilter("All"); editor->setSize(800, 500); pump(20);
                    check(br->getTextScale() > 1.2f && br->columns() == 3, "800x500: browser text scales up, 3 columns");
                    auto img = editor->createComponentSnapshot(editor->getLocalBounds());
                    pe->openSaveDialog(); pump(20);
                    auto img2 = editor->createComponentSnapshot(editor->getLocalBounds());
                    juce::Image both(juce::Image::RGB, 1610, 500, true, juce::SoftwareImageType());
                    { juce::Graphics g2(both); g2.fillAll(juce::Colours::black); g2.drawImageAt(img, 0, 0); g2.drawImageAt(img2, 810, 0); }
                    auto f = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_small_overlays.png");
                    f.deleteFile(); juce::FileOutputStream os(f); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream(both, os);
                    if (auto* d2 = pe->getSaveDialog()) {
                        auto tb = d2->getBoundsInParent().toFloat();   // already includes the transform
                        check(tb.getWidth() >= 0.66f * 1090 - 2 && editor->getLocalBounds().toFloat().contains(tb), "800x500: save dialog stays readable and on screen");
                        d2->cancel(); pump(20);
                    }
                    editor->setSize(1600, 1000); pump(20);
                }
                br->close(); pump(50);
                check(pe->getBrowser() == nullptr, "closing the browser removes it");
            }
            processor.loadPreset(0);
            pe->menuHook = nullptr;
            // sheet: the three menus side by side on the deck colour
            int W = 40, H = 0; for (auto& im : shots) { W += im.getWidth() + 30; H = juce::jmax(H, im.getHeight()); }
            juce::Image sheet(juce::Image::RGB, W, H + 40, true, juce::SoftwareImageType());
            { juce::Graphics sg(sheet); sg.fillAll(juce::Colour(0xff201b16));
              int x = 20; for (auto& im : shots) { sg.drawImageAt(im, x, 20); x += im.getWidth() + 30; } }
            auto mf = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_menus.png");
            mf.deleteFile(); juce::FileOutputStream ms(mf); if (ms.openedOk()) juce::PNGImageFormat().writeImageToStream(sheet, ms);
        }
        check(hintWasShown, "first launch shows the play-the-steel tip");
        {
            std::unique_ptr<juce::AudioProcessorEditor> again(processor.createEditor()); again->setSize(1600, 1000);
            auto* ae = dynamic_cast<PedalSteelAudioProcessorEditor*>(again.get());
            for (int i = 0; i < 90; ++i) ae->refreshVisualizer();
            check(! ae->isHintShowing(), "...and never again once it was dismissed");
        }
        // MORE: the button opens the second row (row 1 lifts, the steel's floor pedals go under the deck); its knobs,
        // CHARACTER's engraved names and SYNC work; LESS folds it away; the choice is kept with the session
        {
            auto* pm = dynamic_cast<PedalSteelAudioProcessorEditor*>(editor.get());
            processor.loadPreset(0); pm->refreshVisualizer();
            const juce::Point<float> moreBtn { UiLayout::hitMore[0] + UiLayout::hitMore[2] * 0.5f, UiLayout::hitMore[1] + UiLayout::hitMore[3] * 0.5f };
            editor->mouseDown(ev(moreBtn)); editor->mouseUp(ev(moreBtn)); pm->refreshVisualizer();
            check(pm->isMoreOpen(), "MORE opens the second row");
            {
                auto img = editor->createComponentSnapshot(editor->getLocalBounds());
                auto f = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_more.png");
                f.deleteFile(); juce::FileOutputStream os(f); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream(img, os);
            }
            const float vx = UiLayout::row2X[0], vy = UiLayout::scallopCentre0[1];
            const float v0 = pv(ParamIDs::vibratoDepth);
            editor->mouseDown(ev({ vx, vy })); editor->mouseDrag(ev({ vx, vy - 50.0f })); editor->mouseUp(ev({ vx, vy - 50.0f }));
            check(pv(ParamIDs::vibratoDepth) > v0 + 0.1f, "dragging VIBRATO up raises the bar vibrato");
            const float gy = UiLayout::scallopCentre0[1] - UiLayout::moreD;   // GLIDE has moved up with row 1
            const float g0 = pv(ParamIDs::barGlide);
            editor->mouseDown(ev({ UiLayout::scallopCentre0[0], gy })); editor->mouseDrag(ev({ UiLayout::scallopCentre0[0], gy - 40.0f }));
            editor->mouseUp(ev({ UiLayout::scallopCentre0[0], gy - 40.0f }));
            check(pv(ParamIDs::barGlide) > g0 + 0.05f, "GLIDE still turns where row 1 now sits");
            auto* fx = dynamic_cast<juce::AudioParameterChoice*>(processor.apvts.getParameter(ParamIDs::fxCharacter));
            const float a8 = juce::degreesToRadians(UiLayout::charAngle[8]);
            const juce::Point<float> rotary { UiLayout::charCentre[0] + (UiLayout::charDotR + 34.0f) * std::sin(a8),
                                              UiLayout::charCentre[1] - (UiLayout::charDotR + 34.0f) * std::cos(a8) };
            editor->mouseDown(ev(rotary)); editor->mouseUp(ev(rotary));
            check(fx != nullptr && fx->getIndex() == 8, "clicking ROTARY on the CHARACTER dial picks the Leslie rotary");
            const bool s0 = pv(ParamIDs::tempoSync) > 0.5f;
            editor->mouseDown(ev({ UiLayout::syncCentre[0], UiLayout::syncCentre[1] })); editor->mouseUp(ev({ UiLayout::syncCentre[0], UiLayout::syncCentre[1] }));
            check((pv(ParamIDs::tempoSync) > 0.5f) != s0, "SYNC switches host tempo sync");
            {
                pm->refreshVisualizer();
                auto img = editor->createComponentSnapshot(editor->getLocalBounds());
                auto f = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_more_set.png");
                f.deleteFile(); juce::FileOutputStream os(f); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream(img, os);
            }
            juce::MemoryBlock blob; processor.getStateInformation(blob); processor.showMore = false;
            processor.setStateInformation(blob.getData(), (int) blob.getSize());
            check(processor.showMore, "the MORE row stays open with the session");
            editor->mouseDown(ev(moreBtn)); editor->mouseUp(ev(moreBtn)); pm->refreshVisualizer();
            check(! pm->isMoreOpen(), "LESS folds the second row away");
            processor.loadPreset(0); pm->refreshVisualizer();
        }
        std::cout << (fails == 0 ? "all interaction tests passed" : "SOME INTERACTION TESTS FAILED") << std::endl;
        // per-preset instruments: 04 maple, 06 red sparkle, 13 pearloid, 18 koa, the rest walnut; user presets keep their finish
        {
            auto* pe5 = dynamic_cast<PedalSteelAudioProcessorEditor*>(editor.get());
            editor->mouseExit(ev({ 0.0f, 0.0f }));
            const int want[] = { 0, 1, 2, 3, 4 }; const int prog[] = { 0, 3, 5, 12, 17 };
            std::vector<juce::Image> shotsK; bool ok = true;
            for (int i = 0; i < 5; ++i) {
                processor.loadPreset(prog[i]); pe5->refreshVisualizer();
                ok &= pe5->getSkin() == want[i];
                shotsK.push_back(editor->createComponentSnapshot(editor->getLocalBounds()).rescaled(800, 500, juce::Graphics::highResamplingQuality));
            }
            check(ok, "each preset shows its own instrument (01 walnut, 04 maple, 06 red sparkle, 13 pearloid, 18 koa)");
            juce::Image sheet(juce::Image::RGB, 1600, 1500, true, juce::SoftwareImageType());
            { juce::Graphics g3(sheet); g3.fillAll(juce::Colours::black);
              for (int i = 0; i < 5; ++i) g3.drawImageAt(shotsK[(size_t) i], (i % 2) * 800, (i / 2) * 500); }
            auto f = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_instruments.png");
            f.deleteFile(); juce::FileOutputStream os(f); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream(sheet, os);
            // a user preset saved with the koa finish brings the koa steel back
            auto up = PedalSteelAudioProcessor::getUserPresetsFolder(); up.createDirectory();
            auto uf = up.getChildFile("ZZ Koa Test.nvps"); processor.saveUserPreset(uf);
            if (auto x = juce::XmlDocument::parse(uf)) { x->setAttribute("finish", "koa"); x->writeTo(uf); }
            processor.loadPreset(0); pe5->refreshVisualizer(); processor.loadUserPreset(uf); pe5->refreshVisualizer();
            check(pe5->getSkin() == 4, "a user preset saved with the koa finish shows the koa steel");
            {   // koa with all pedals and levers engaged: brass rods
                for (int i = 0; i < 5; ++i) processor.setLivePedalState(i, 1.0f);
                pe5->refreshVisualizer();
                auto img = editor->createComponentSnapshot({ 470, 380, 520, 290 });
                auto f = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_koa_pedals.png");
                f.deleteFile(); juce::FileOutputStream os(f); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream(img, os);
                for (int i = 0; i < 5; ++i) processor.setLivePedalState(i, 0.0f);
            }
            uf.deleteFile(); processor.loadPreset(0); pe5->refreshVisualizer();
        }
        // accessibility: every control has a screen-reader stand-in that reads and sets its value
        {
            auto* pe6 = dynamic_cast<PedalSteelAudioProcessorEditor*>(editor.get());
            bool named = pe6->numAccessibleControls() >= 18;
            for (int i = 0; i < pe6->numAccessibleControls(); ++i) named &= pe6->getAccessibleControl(i)->getTitle().isNotEmpty();
            check(named, "every control has a named screen-reader element (" + std::to_string(pe6->numAccessibleControls()) + ")");
            // (JUCE only hands out handlers for on-screen components; build them directly here)
            auto* glide = pe6->getAccessibleControl(2); auto hh = glide ? glide->createAccessibilityHandler() : nullptr; auto* h = hh.get();
            bool okv = false;
            if (h != nullptr && h->getValueInterface() != nullptr) {
                h->getValueInterface()->setValue(0.5);
                okv = std::abs(processor.apvts.getParameter(ParamIDs::barGlide)->getValue() - 0.5f) < 1e-3f
                      && h->getValueInterface()->getCurrentValueAsString().endsWith("ms");
            }
            check(okv, "a screen reader can set GLIDE and hears its value in ms");
            auto* pa = pe6->getAccessibleControl(8); auto phh = pa ? pa->createAccessibilityHandler() : nullptr; auto* ph = phh.get();
            const float a0 = processor.apvts.getParameter(ParamIDs::pedalA)->getValue();
            if (ph != nullptr) ph->getActions().invoke(juce::AccessibilityActionType::toggle);
            check(processor.apvts.getParameter(ParamIDs::pedalA)->getValue() != a0, "a screen reader can toggle pedal A");
            if (ph != nullptr) ph->getActions().invoke(juce::AccessibilityActionType::toggle);
        }
        // the LED display shows a knob's value while it moves, then the preset again
        {
            auto* pe7 = dynamic_cast<PedalSteelAudioProcessorEditor*>(editor.get());
            processor.loadPreset(0); pe7->refreshVisualizer();
            editor->mouseDown(ev({ 445.0f, 779.0f })); editor->mouseDrag(ev({ 445.0f, 769.0f })); editor->mouseUp(ev({ 445.0f, 769.0f }));
            pe7->refreshVisualizer();
            check(pe7->getDisplayText().startsWith("GLIDE "), "turning GLIDE shows it on the LED display: " + pe7->getDisplayText().toStdString());
            {
                auto img = editor->createComponentSnapshot({ 590, 34, 420, 54 });
                auto f = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_display_value.png");
                f.deleteFile(); juce::FileOutputStream os(f); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream(img, os);
            }
            juce::Thread::sleep(1500); pe7->refreshVisualizer();
            check(pe7->getDisplayText().startsWith("01 SWEET GEORGIA"), "...and returns to the preset name");
            processor.loadPreset(0);
        }
        // copedent glow: pedal A on, a moment of processing, strings 5 and 10 (B -> C#) glow
        {
            auto* pe8 = dynamic_cast<PedalSteelAudioProcessorEditor*>(editor.get());
            processor.loadPreset(0); processor.setLivePedalState(0, 1.0f); processor.setLivePedalState(1, 1.0f);
            for (int s2 = 0; s2 < 10; ++s2) processor.triggerFretboardNote(s2, 3.0f);
            for (int i = 0; i < 60; ++i) processor.processBlock(audioBuf, midiBuf);
            pe8->refreshVisualizer();
            auto img = editor->createComponentSnapshot({ 470, 170, 900, 120 });
            auto f = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_copedent.png");
            {
                auto ch = editor->createComponentSnapshot({ 1300, 175, 80, 100 });
                juce::Image big = ch.rescaled(320, 400, juce::Graphics::highResamplingQuality);
                auto cf = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_changer.png");
                cf.deleteFile(); juce::FileOutputStream co(cf); if (co.openedOk()) juce::PNGImageFormat().writeImageToStream(big, co);
            }
            f.deleteFile(); juce::FileOutputStream os(f); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream(img, os);
            for (int s2 = 0; s2 < 10; ++s2) processor.releaseFretboardNote(s2);
            processor.setLivePedalState(0, 0.0f); processor.setLivePedalState(1, 0.0f);
            { auto st2 = processor.getLiveStatus(); juce::ignoreUnused(st2); }
        }
        // changer motion, frame by frame: pedal A engaging (strings 5 and 10), zoomed 4x
        {
            auto* pe9 = dynamic_cast<PedalSteelAudioProcessorEditor*>(editor.get());
            processor.loadPreset(0); for (int i = 0; i < 5; ++i) processor.setLivePedalState(i, 0.0f);
            for (int i = 0; i < 80; ++i) processor.processBlock(audioBuf, midiBuf);
            juce::Image strip(juce::Image::RGB, 6 * 164, 404, true, juce::SoftwareImageType());
            const int at[] = { 0, 3, 6, 10, 20, 60 }; int done = 0;
            processor.setLivePedalState(0, 1.0f);
            for (int k = 0; k < 6; ++k) {
                for (; done < at[k]; ++done) processor.processBlock(audioBuf, midiBuf);
                pe9->refreshVisualizer();
                auto fr = editor->createComponentSnapshot({ 1318, 186, 40, 100 }).rescaled(160, 400, juce::Graphics::highResamplingQuality);
                juce::Graphics gs(strip); gs.drawImageAt(fr, k * 164, 2);
            }
            auto f = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_changer_frames.png");
            f.deleteFile(); juce::FileOutputStream os(f); if (os.openedOk()) juce::PNGImageFormat().writeImageToStream(strip, os);
            processor.setLivePedalState(0, 0.0f);
        }
        // undo / redo of changes made in the window (not of automation)
        {
            auto* pu = dynamic_cast<PedalSteelAudioProcessorEditor*>(editor.get());
            processor.loadPreset(0); pu->refreshVisualizer(); const int depth0 = pu->undoDepth();
            auto* tone = processor.apvts.getParameter(ParamIDs::toneWarmth); auto* pa2 = processor.apvts.getParameter(ParamIDs::pedalC);
            const float t0 = tone->getValue(), c0 = pa2->getValue();
            editor->mouseDown(ev({ 133.0f, 778.0f })); editor->mouseDrag(ev({ 133.0f, 748.0f })); editor->mouseUp(ev({ 133.0f, 748.0f }));
            editor->mouseDown(ev({ 1202.0f, 800.0f })); editor->mouseUp(ev({ 1202.0f, 800.0f }));   // pedal C
            tone->setValueNotifyingHost(0.05f); tone->setValueNotifyingHost(t0 + 0.12f);           // "automation": not recorded
            using KP = juce::KeyPress;
            pu->keyPressed(KP('z', juce::ModifierKeys::commandModifier, 0));
            check(std::abs(pa2->getValue() - c0) < 1e-4f, "Ctrl+Z undoes the pedal C click");
            pu->keyPressed(KP('z', juce::ModifierKeys::commandModifier, 0));
            check(std::abs(tone->getValue() - t0) < 1e-3f, "...then the TONE drag");
            pu->keyPressed(KP('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0));
            check(tone->getValue() > t0 + 0.05f, "Ctrl+Shift+Z redoes it");
            check(pu->undoDepth() == depth0 + 1, "automation moves are not in the undo history");
            pu->clearValueFlash(); processor.loadPreset(0);
        }
        // the display marks an edited preset
        {
            auto* pe2 = dynamic_cast<PedalSteelAudioProcessorEditor*>(editor.get());
            processor.loadPreset(0); pe2->refreshVisualizer();
            check(! pe2->getDisplayText().endsWith("*"), "freshly loaded preset: no * (" + pe2->getDisplayText().toStdString() + ")");
            auto* rv = processor.apvts.getParameter(ParamIDs::reverbMix); rv->setValueNotifyingHost(rv->getValue() > 0.5f ? 0.1f : 0.9f);
            pe2->refreshVisualizer();
            check(pe2->getDisplayText().endsWith("*"), "after turning REVERB: " + pe2->getDisplayText().toStdString());
            processor.apvts.getParameter(ParamIDs::pedalB)->setValueNotifyingHost(1.0f);
            processor.loadPreset(0); processor.apvts.getParameter(ParamIDs::pedalB)->setValueNotifyingHost(1.0f); pe2->refreshVisualizer();
            check(! pe2->getDisplayText().endsWith("*"), "pressing a pedal is playing, not editing: no *");
        }
                // typed values: double-click a value box, type, Enter
        {
            auto* pe4 = dynamic_cast<PedalSteelAudioProcessorEditor*>(editor.get());
            auto plain = [&](const char* id) { auto* p = processor.apvts.getParameter(id); return p->convertFrom0to1(p->getValue()); };
            editor->mouseDoubleClick(ev({ 445.0f, 861.0f }));   // GLIDE's value box
            auto* te = pe4->getValueEditor();
            check(te != nullptr && te->isVisible(), "double-clicking GLIDE's value box opens a text field");
            if (te != nullptr) {
                te->setText("200"); te->selectAll();
                auto ti = editor->createComponentSnapshot({ 360, 760, 300, 130 });
                auto tf2 = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_typein.png");
                tf2.deleteFile(); juce::FileOutputStream t2(tf2); if (t2.openedOk()) juce::PNGImageFormat().writeImageToStream(ti, t2);
                te->setText("200 ms"); te->onReturnKey();
            }
            check(std::abs(plain(ParamIDs::barGlide) - 200.0f) < 0.6f && pe4->getValueEditor() == nullptr, "typing 200 ms + Enter sets GLIDE to 200 ms");
            pe4->applyTypedValue(PedalSteelAudioProcessorEditor::kCtlTone, "42 %");
            check(std::abs(plain(ParamIDs::toneWarmth) - 0.42f) < 0.002f, "TONE accepts '42 %'");
            pe4->applyTypedValue(PedalSteelAudioProcessorEditor::kCtlTrim, "-6.5 dB");
            check(std::abs(plain(ParamIDs::outputLevel) + 6.5f) < 0.06f, "TRIM accepts '-6.5 dB'");
            pe4->applyTypedValue(PedalSteelAudioProcessorEditor::kCtlVolume, "150");
            check(plain(ParamIDs::volumePedal) > 0.999f, "out-of-range input is clamped (150 % -> 100 %)");
            check(! pe4->applyTypedValue(PedalSteelAudioProcessorEditor::kCtlEcho, "abc"), "nonsense input is ignored");
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        }
                // a long user preset name scrolls through the display
        {
            auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Midnight Train To Memphis Slow Ballad.nvps");
            processor.saveUserPreset(tmp); processor.loadUserPreset(tmp);
            auto* pe3 = dynamic_cast<PedalSteelAudioProcessorEditor*>(editor.get());
            pe3->clearValueFlash(); pe3->refreshVisualizer();
            auto d0 = editor->createComponentSnapshot({ 590, 34, 420, 54 });
            for (int i = 0; i < 160; ++i) pe3->refreshVisualizer();   // ~2.7 s of timer ticks: past the hold, scrolling
            auto d1 = editor->createComponentSnapshot({ 590, 34, 420, 54 });
            juce::Image both(juce::Image::RGB, 420, 116, true, juce::SoftwareImageType());
            { juce::Graphics g(both); g.fillAll(juce::Colours::black); g.drawImageAt(d0, 0, 0); g.drawImageAt(d1, 0, 60); }
            auto df = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_marquee.png");
            df.deleteFile(); juce::FileOutputStream ds(df); if (ds.openedOk()) juce::PNGImageFormat().writeImageToStream(both, ds);
            check(pe3->getDisplayText().length() > 22, "long names are kept whole: " + pe3->getDisplayText().toStdString());
            tmp.deleteFile(); processor.loadPreset(0);
        }
                // keyboard: Tab to TONE, arrows nudge it, Tab on to pedal A, Space toggles it, Esc drops the focus ring
        {
            using KP = juce::KeyPress;
            editor->keyPressed(KP(KP::escapeKey));
            editor->keyPressed(KP(KP::tabKey)); editor->keyPressed(KP(KP::tabKey));   // Display, then TONE
            auto* tp2 = processor.apvts.getParameter(ParamIDs::toneWarmth); tp2->setValueNotifyingHost(0.5f);
            for (int i = 0; i < 5; ++i) editor->keyPressed(KP(KP::upKey));
            check(std::abs(tp2->getValue() - 0.55f) < 0.002f, "Tab to TONE, then Up x5 raises it 5 %");
            editor->keyPressed(KP(KP::pageDownKey));
            check(std::abs(tp2->getValue() - 0.45f) < 0.002f, "...Page Down lowers it 10 %");
            for (int i = 0; i < 6; ++i) editor->keyPressed(KP(KP::tabKey));             // PICK GLIDE SWELL ECHO REVERB -> A
            const float a0 = processor.apvts.getParameter(ParamIDs::pedalA)->getValue();
            editor->keyPressed(KP(KP::spaceKey));
            check(processor.apvts.getParameter(ParamIDs::pedalA)->getValue() != a0, "Tab to pedal A, Space toggles it");
            editor->keyPressed(KP(KP::spaceKey));
            // focus ring screenshot on GLIDE
            for (int i = 0; i < 12; ++i) editor->keyPressed(KP(KP::tabKey));            // round to GLIDE (16 stops)
            if (auto* e2 = dynamic_cast<PedalSteelAudioProcessorEditor*>(editor.get())) e2->refreshVisualizer();
            auto fimg = editor->createComponentSnapshot({ 330, 690, 330, 240 });
            auto ff = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_focus.png");
            ff.deleteFile(); juce::FileOutputStream fs(ff); if (fs.openedOk()) juce::PNGImageFormat().writeImageToStream(fimg, fs);
            editor->keyPressed(KP(KP::escapeKey));
            std::cout << (fails == 0 ? "keyboard tests passed" : "SOME TESTS FAILED") << std::endl;
        }
        // tooltip: hover the neck for a moment
        editor->mouseMove(ev({ 881.0f, 208.5f })); juce::Thread::sleep(400);
        if (auto* e = dynamic_cast<PedalSteelAudioProcessorEditor*>(editor.get())) e->refreshVisualizer();
        auto tip = editor->createComponentSnapshot({ 440, 150, 760, 200 });
        auto tf = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_tooltip.png");
        tf.deleteFile(); juce::FileOutputStream ts(tf); if (ts.openedOk()) juce::PNGImageFormat().writeImageToStream(tip, ts);
        processor.midiLearn.clearAll();
        processor.loadPreset(0);
    }

    // 3) MIDI LEARN on: two controls already mapped, REVERB armed, knee levers engaged
    {
        processor.midiLearn.fromString("74:toneWarmth;1:volumePedal;64:pedalA");
        processor.midiLearn.learnMode = true; processor.midiLearn.armLearn(ParamIDs::reverbMix);
        processor.setLivePedalState(3, 1.0f); processor.setLivePedalState(4, 1.0f);
        std::unique_ptr<juce::AudioProcessorEditor> le(processor.createEditor());
        le->setSize(1600, 1000);
        if (auto* e = dynamic_cast<PedalSteelAudioProcessorEditor*>(le.get())) e->refreshVisualizer();
        juce::Image img = le->createComponentSnapshot(le->getLocalBounds());
        auto f = outFile.getSiblingFile(outFile.getFileNameWithoutExtension() + "_learn.png");
        f.deleteFile(); juce::FileOutputStream ls(f);
        if (ls.openedOk()) juce::PNGImageFormat().writeImageToStream(img, ls);
        processor.midiLearn.clearAll(); processor.midiLearn.learnMode = false;
        processor.setLivePedalState(3, 0.0f); processor.setLivePedalState(4, 0.0f);
    }
    // put the user's own UI settings back exactly as they were (no file = tip not seen yet)
    if (uiBackup.isNotEmpty()) uiSettings.replaceWithText(uiBackup); else uiSettings.deleteFile();
    juce::PNGImageFormat pngFormat;
    juce::FileOutputStream stream(outFile);
    if (stream.openedOk()) {
        stream.setPosition(0);
        stream.truncate();
        pngFormat.writeImageToStream(snapshot, stream);
        std::cout << "Saved UI screenshot to: " << outFile.getFullPathName() << std::endl;
        return 0;
    }

    std::cerr << "Failed to write screenshot: " << outFile.getFullPathName() << std::endl;
    return 1;
}
