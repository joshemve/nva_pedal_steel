// Loads the BUILT VST3 (as a DAW would) and checks it: instantiation, parameters, audio from MIDI, the editor opening at
// the design's aspect, state round trip (including MIDI LEARN assignments), and a learned CC driving its parameter.
// Usage: PedalSteelHostTest [path/to/NVA Pedal Steel.vst3]
#include <JuceHeader.h>

int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File vst3 = argc > 1 ? juce::File(juce::String(argv[1]))
        : juce::File::getCurrentWorkingDirectory().getChildFile("build/PedalSteel_artefacts/Release/VST3/NVA Pedal Steel.vst3");
    int fails = 0;
    auto check = [&](bool ok, const juce::String& what) { std::cout << (ok ? "  PASS " : "  FAIL ") << what << std::endl; fails += ok ? 0 : 1; };
    auto pump = [](int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); };

    juce::VST3PluginFormat fmt;
    juce::OwnedArray<juce::PluginDescription> types;
    fmt.findAllTypesForFile(types, vst3.getFullPathName());
    check(types.size() == 1, "VST3 scanned: " + (types.isEmpty() ? juce::String("none") : types[0]->manufacturerName + " / " + types[0]->name));
    if (types.isEmpty()) return 1;
    check(types[0]->isInstrument, "registered as an instrument");

    juce::String err;
    auto inst = fmt.createInstanceFromDescription(*types[0], 48000.0, 256, err);
    check(inst != nullptr, "instantiated" + (err.isNotEmpty() ? " (" + err + ")" : juce::String()));
    if (inst == nullptr) return 1;
    inst->enableAllBuses();
    inst->prepareToPlay(48000.0, 256);

    auto& params = inst->getParameters();
    auto findParam = [&](const juce::String& namePart) -> juce::AudioProcessorParameter* {
        for (auto* p : params) if (p->getName(128).containsIgnoreCase(namePart)) return p;
        return nullptr;
    };
    check(params.size() >= 30, "exposes " + juce::String(params.size()) + " parameters to the host");

    // audio: an E major triad for 0.5 s
    juce::AudioBuffer<float> buf(2, 256); double energy = 0;
    for (int b = 0; b < 94; ++b) {
        juce::MidiBuffer midi;
        if (b == 0) for (int n : { 64, 68, 71 }) midi.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8) 100), 0);
        buf.clear(); inst->processBlock(buf, midi);
        energy += buf.getRMSLevel(0, 0, 256);
    }
    check(energy / 94 > 1e-3, "plays audio from MIDI (mean RMS " + juce::String(energy / 94, 4) + ")");

    // editor
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed(inst->createEditorIfNeeded());
        check(ed != nullptr, "editor opens");
        if (ed != nullptr) {
            const float aspect = (float) ed->getWidth() / (float) juce::jmax(1, ed->getHeight());
            check(std::abs(aspect - 1.6f) < 0.02f, "editor is " + juce::String(ed->getWidth()) + " x " + juce::String(ed->getHeight()) + " (1.6:1)");
            juce::DocumentWindow w("host test", juce::Colours::black, 0);
            w.setUsingNativeTitleBar(true); w.setContentNonOwned(ed.get(), true); w.setTopLeftPosition(-4000, -4000);
            w.setVisible(true); pump(600);
            ed->setSize(1600, 1000); pump(200);
            check(ed->getWidth() == 1600 && ed->getHeight() == 1000, "editor resizes to 1600 x 1000");
            w.clearContentComponent(); w.setVisible(false);
            inst->editorBeingDeleted(ed.get());
        }
    }

    // state: change a parameter + add a MIDI LEARN assignment, save, scramble, restore
    auto* reverb = findParam("Reverb Mix"); if (reverb == nullptr) reverb = findParam("Reverb");
    check(reverb != nullptr, "found the REVERB parameter: " + (reverb ? reverb->getName(64) : juce::String("-")));
    if (reverb != nullptr) {
        reverb->setValueNotifyingHost(0.33f); pump(50);
        juce::MemoryBlock st; inst->getStateInformation(st);
        // the VST3 host state wraps the plugin's own state: <VST3PluginState><IComponent>base64(plugin state)</IComponent>
        auto outer = juce::AudioProcessor::getXmlFromBinary(st.getData(), (int) st.getSize());
        auto* comp = outer != nullptr ? outer->getChildByName("IComponent") : nullptr;
        juce::MemoryBlock inner; if (comp != nullptr) inner.fromBase64Encoding(comp->getAllSubText());
        auto xml = juce::AudioProcessor::getXmlFromBinary(inner.getData(), (int) inner.getSize());
        check(xml != nullptr && xml->hasAttribute("midiMap") && xml->hasAttribute("editorWidth"), "state carries the MIDI LEARN map and window size");
        if (xml != nullptr && comp != nullptr) {
            xml->setAttribute("midiMap", "21:reverbMix");   // as if REVERB had been learned on CC 21 in an earlier session
            juce::MemoryBlock inner2; juce::AudioProcessor::copyXmlToBinary(*xml, inner2);
            comp->deleteAllTextElements(); comp->addTextElement(inner2.toBase64Encoding());
            juce::MemoryBlock st2; juce::AudioProcessor::copyXmlToBinary(*outer, st2);
            reverb->setValueNotifyingHost(0.9f); pump(50);
            inst->setStateInformation(st2.getData(), (int) st2.getSize()); pump(100);
            check(std::abs(reverb->getValue() - 0.33f) < 0.01f, "state restores REVERB (" + juce::String(reverb->getValue(), 3) + ")");
            juce::MidiBuffer cc; cc.addEvent(juce::MidiMessage::controllerEvent(1, 21, 127), 0);
            buf.clear(); inst->processBlock(buf, cc); pump(150);
            buf.clear(); juce::MidiBuffer none; inst->processBlock(buf, none); pump(100);
            check(reverb->getValue() > 0.99f, "restored MIDI LEARN map: CC 21 drives REVERB (" + juce::String(reverb->getValue(), 3) + ")");
        }
    }
    // --show-editor: the hosted editor in a real on-screen window for ~7 s while notes play (captured from outside),
    // resized by the host half way through
    for (int a = 1; a < argc; ++a)
        if (juce::String(argv[a]) == "--show-editor") {
            std::unique_ptr<juce::AudioProcessorEditor> ed(inst->createEditorIfNeeded());
            if (ed == nullptr) { check(false, "editor for on-screen test"); break; }
            juce::DocumentWindow w("NVA Pedal Steel host window", juce::Colours::black, juce::DocumentWindow::closeButton);
            w.setUsingNativeTitleBar(true); w.setContentNonOwned(ed.get(), true); w.setTopLeftPosition(40, 40);
            w.setVisible(true); w.toFront(false);
            std::cout << "WINDOW_READY" << std::endl;
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            int block = 0; bool resized = false;
            while (juce::Time::getMillisecondCounterHiRes() - t0 < 7000.0) {
                juce::MidiBuffer m;
                const int phase = (block / 120) % 4;   // a new chord every ~0.6 s, with pedal changes via CC 65/66
                if (block % 120 == 0) {
                    static const int chords[4][3] = { { 64, 68, 71 }, { 66, 69, 73 }, { 64, 68, 71 }, { 69, 73, 76 } };
                    for (int n = 40; n < 90; ++n) m.addEvent(juce::MidiMessage::noteOff(1, n), 0);
                    for (int n : chords[phase]) m.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8) 100), 1);
                    m.addEvent(juce::MidiMessage::controllerEvent(1, 65, phase == 1 ? 127 : 0), 2);
                    m.addEvent(juce::MidiMessage::controllerEvent(1, 66, phase == 3 ? 127 : 0), 2);
                }
                buf.clear(); inst->processBlock(buf, m); ++block;
                pump(5);
                if (! resized && juce::Time::getMillisecondCounterHiRes() - t0 > 3500.0) {
                    ed->setSize(1600, 1000); resized = true; std::cout << "RESIZED " << ed->getWidth() << "x" << ed->getHeight() << std::endl;
                }
            }
            check(resized && ed->getWidth() == 1600, "host-driven resize of the on-screen editor");
            w.clearContentComponent(); w.setVisible(false); inst->editorBeingDeleted(ed.get());
        }

    inst->releaseResources();
    inst.reset();
    std::cout << (fails == 0 ? "VST3 host test passed" : "VST3 HOST TEST FAILED") << std::endl;
    return fails == 0 ? 0 : 1;
}
