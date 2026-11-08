#pragma once
#include <JuceHeader.h>
#include "BinaryData.h"

// Decodes the editor's bitmaps on a low-priority background thread as soon as a plugin instance exists, and keeps them
// alive while any instance does (shared by all instances). The editor's ImageCache lookups then find them decoded,
// so opening the plugin window doesn't stall on ~0.7 s of PNG/JPEG decoding.
class UiPreload {
public:
    static void acquire() {
        const juce::ScopedLock sl(lock());
        if (count()++ == 0) { thread() = std::make_unique<Loader>(); thread()->startThread(juce::Thread::Priority::low); }
    }
    static void release() {
        std::unique_ptr<Loader> t;
        {
            const juce::ScopedLock sl(lock());
            if (--count() > 0) return;
            t = std::move(thread());
        }
        if (t) t->stopThread(4000);
        const juce::ScopedLock sl(lock());
        if (count() == 0) held().clear();
    }

private:
    struct Loader : juce::Thread {
        Loader() : juce::Thread("NVA UI preload") {}
        void run() override {
            // the big ones first (background, knob strips, instrument layers), then everything else
            juce::StringArray first { "ui_bg_jpg", "knob_scallop_png", "knob_space_png", "ptr_tone_png", "ptr_pick_png", "ptr_trim_png" };
            juce::StringArray names(first);
            for (int i = 0; i < BinaryData::namedResourceListSize; ++i) {
                const juce::String n(BinaryData::namedResourceList[i]);
                if ((n.endsWith("_png") || n.endsWith("_jpg")) && ! names.contains(n)) names.add(n);
            }
            for (auto& n : names) {
                if (threadShouldExit()) return;
                int size = 0;
                if (auto* data = BinaryData::getNamedResource(n.toRawUTF8(), size)) {
                    auto img = juce::ImageCache::getFromMemory(data, size);
                    const juce::ScopedLock sl(lock()); held().push_back(img);
                }
            }
        }
    };
    static juce::CriticalSection& lock() { static juce::CriticalSection c; return c; }
    static int& count() { static int n = 0; return n; }
    static std::unique_ptr<Loader>& thread() { static std::unique_ptr<Loader> t; return t; }
    static std::vector<juce::Image>& held() { static std::vector<juce::Image> v; return v; }
};
