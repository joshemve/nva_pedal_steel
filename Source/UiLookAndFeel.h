#pragma once
#include <JuceHeader.h>

// Pop-up menus in the deck's own materials: dark ink panel, cream Poppins text, amber highlight and section headers,
// thin cream rim. Used for the preset, mode and right-click menus.
class NvaMenuLookAndFeel : public juce::LookAndFeel_V4 {
public:
    NvaMenuLookAndFeel() {
        setColour(juce::PopupMenu::backgroundColourId, ink);
        setColour(juce::PopupMenu::textColourId, cream);
        setColour(juce::PopupMenu::headerTextColourId, amber);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, amber.withAlpha(0.22f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xfffff1d6));
    }
    void setFaces(juce::Typeface::Ptr medium, juce::Typeface::Ptr semi) { faceMedium = medium; faceSemi = semi; }

    juce::Font getPopupMenuFont() override {
        return faceMedium != nullptr ? juce::Font(juce::FontOptions(faceMedium).withHeight(15.0f)) : juce::LookAndFeel_V4::getPopupMenuFont();
    }
    void drawPopupMenuBackground(juce::Graphics& g, int w, int h) override {
        auto r = juce::Rectangle<float>(0, 0, (float) w, (float) h);
        g.setColour(ink); g.fillRect(r);
        g.setColour(cream.withAlpha(0.22f)); g.drawRect(r.reduced(0.5f), 1.0f);
    }
    void drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name) override {
        auto f = faceSemi != nullptr ? juce::Font(juce::FontOptions(faceSemi).withHeight(11.5f)) : juce::Font(juce::FontOptions(11.5f));
        g.setFont(f.withExtraKerningFactor(0.18f)); g.setColour(amber);
        g.drawFittedText(name.toUpperCase(), area.reduced(14, 0).withTrimmedTop(4), juce::Justification::bottomLeft, 1);
    }
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                           bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                           const juce::String& shortcutKeyText, const juce::Drawable* icon, const juce::Colour* textColour) override {
        if (isSeparator) {
            g.setColour(cream.withAlpha(0.14f));
            g.fillRect(area.reduced(12, 0).withHeight(1).withY(area.getCentreY()));
            return;
        }
        auto r = area.reduced(4, 1);
        if (isHighlighted && isActive) {
            g.setColour(amber.withAlpha(0.20f)); g.fillRoundedRectangle(r.toFloat(), 4.0f);
            g.setColour(amber.withAlpha(0.55f)); g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 4.0f, 1.0f);
        }
        if (isTicked) {   // a small lit LED dot instead of a tick
            auto c = juce::Rectangle<float>(7.0f, 7.0f).withCentre({ (float) r.getX() + 11.0f, (float) r.getCentreY() });
            g.setColour(amber.withAlpha(0.35f)); g.fillEllipse(c.expanded(3.0f));
            g.setColour(amber); g.fillEllipse(c);
        }
        g.setFont(getPopupMenuFont());
        g.setColour(! isActive ? cream.withAlpha(0.35f) : isHighlighted ? juce::Colour(0xfffff1d6) : textColour != nullptr ? *textColour : cream);
        auto tr = r.withTrimmedLeft(24).withTrimmedRight(hasSubMenu ? 20 : 10);
        g.drawFittedText(text, tr, juce::Justification::centredLeft, 1);
        if (shortcutKeyText.isNotEmpty()) { g.setColour(cream.withAlpha(0.45f)); g.drawText(shortcutKeyText, tr, juce::Justification::centredRight, true); }
        if (hasSubMenu) {
            juce::Path p; const float x = (float) r.getRight() - 12.0f, y = (float) r.getCentreY();
            p.addTriangle(x - 3.0f, y - 4.5f, x - 3.0f, y + 4.5f, x + 3.0f, y);
            g.setColour(cream.withAlpha(0.7f)); g.fillPath(p);
        }
        juce::ignoreUnused(icon);
    }
    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator, int standardMenuItemHeight, int& w, int& h) override {
        if (isSeparator) { w = 50; h = 9; return; }
        juce::GlyphArrangement ga; ga.addLineOfText(getPopupMenuFont(), text, 0, 0);
        w = juce::roundToInt(ga.getBoundingBox(0, -1, true).getWidth()) + 60; h = juce::jmax(standardMenuItemHeight, 30);
    }
    int getPopupMenuBorderSize() override { return 5; }
    void drawCornerResizer(juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging) override {
        const float a = isMouseDragging ? 0.8f : isMouseOver ? 0.6f : 0.3f;
        g.setColour(cream.withAlpha(a));
        for (int i = 0; i < 3; ++i) {
            const float o = 4.0f + i * 4.0f;
            g.drawLine((float) w - o, (float) h - 2.0f, (float) w - 2.0f, (float) h - o, 1.2f);
        }
    }

    const juce::Colour ink{ 0xfa16120e }, cream{ 0xffeadcc2 }, amber{ 0xffffa83a };

private:
    juce::Typeface::Ptr faceMedium, faceSemi;
};
