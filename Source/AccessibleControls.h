#pragma once
#include <JuceHeader.h>

// Screen-reader stand-ins for the painted controls: one invisible, click-through component per control, placed over
// it, with a name, a role (slider / toggle / button) and a live value. Narrator / VoiceOver can read, step and press
// them; the mouse still goes to the editor underneath.
class AccessibleControl : public juce::Component {
public:
    enum class Kind { Slider, Toggle, Button };
    struct Spec {
        Kind kind = Kind::Slider;
        juce::String name, help;
        juce::RangedAudioParameter* param = nullptr;       // slider / toggle
        std::function<juce::String()> valueText;            // what the panel shows ("65 %", "145 ms", "On")
        std::function<void()> press;                        // toggle / button
        std::function<bool()> isOn;                         // toggle state
    };
    explicit AccessibleControl(Spec s) : spec(std::move(s)) {
        setInterceptsMouseClicks(false, false);
        setTitle(spec.name); setDescription(spec.help); setHelpText(spec.help);
        setAccessible(true); setWantsKeyboardFocus(false);
    }
    void paint(juce::Graphics&) override {}
    void valueChanged() {
        if (auto* h = getAccessibilityHandler()) h->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
    }
    const Spec& getSpec() const { return spec; }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override {
        struct Ranged : juce::AccessibilityValueInterface {   // the base interface, so the panel's own text is read out
            explicit Ranged(AccessibleControl& c) : owner(c) {}
            bool isReadOnly() const override { return false; }
            double getCurrentValue() const override { return owner.spec.param ? owner.spec.param->getValue() : 0.0; }
            void setValue(double v) override {
                if (auto* p = owner.spec.param) { p->beginChangeGesture(); p->setValueNotifyingHost((float) juce::jlimit(0.0, 1.0, v)); p->endChangeGesture(); }
            }
            juce::String getCurrentValueAsString() const override { return owner.spec.valueText ? owner.spec.valueText() : juce::String(getCurrentValue()); }
            void setValueAsString(const juce::String& s) override { setValue(s.getDoubleValue()); }
            AccessibleValueRange getRange() const override { return { { 0.0, 1.0 }, 0.01 }; }
            AccessibleControl& owner;
        };
        struct Text : juce::AccessibilityTextValueInterface {
            explicit Text(AccessibleControl& c) : owner(c) {}
            bool isReadOnly() const override { return true; }
            juce::String getCurrentValueAsString() const override { return owner.spec.valueText ? owner.spec.valueText() : juce::String(); }
            void setValueAsString(const juce::String&) override {}
            AccessibleControl& owner;
        };
        struct ToggleHandler : juce::AccessibilityHandler {
            ToggleHandler(AccessibleControl& c, juce::AccessibilityActions a, Interfaces i)
                : juce::AccessibilityHandler(c, juce::AccessibilityRole::toggleButton, std::move(a), std::move(i)), owner(c) {}
            juce::AccessibleState getCurrentState() const override {
                auto st = juce::AccessibilityHandler::getCurrentState().withCheckable();
                return owner.spec.isOn && owner.spec.isOn() ? st.withChecked() : st;
            }
            AccessibleControl& owner;
        };
        switch (spec.kind) {
            case Kind::Slider:
                return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::slider, juce::AccessibilityActions(),
                                                                    juce::AccessibilityHandler::Interfaces { std::make_unique<Ranged>(*this) });
            case Kind::Toggle:
                return std::make_unique<ToggleHandler>(*this,
                    juce::AccessibilityActions().addAction(juce::AccessibilityActionType::toggle, [this] { if (spec.press) spec.press(); })
                                                .addAction(juce::AccessibilityActionType::press, [this] { if (spec.press) spec.press(); }),
                    juce::AccessibilityHandler::Interfaces {});
            case Kind::Button:
            default:
                return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::button,
                    juce::AccessibilityActions().addAction(juce::AccessibilityActionType::press, [this] { if (spec.press) spec.press(); }),
                    juce::AccessibilityHandler::Interfaces { std::make_unique<Text>(*this) });
        }
    }

private:
    Spec spec;
};
