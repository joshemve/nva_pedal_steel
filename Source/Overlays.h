#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UiLookAndFeel.h"
#include "BinaryData.h"

// The approved secondary screens (design/ui_v2/round18): the mode menu (mode_2), the preset browser (pb2_2) and the
// preset editor / save dialog (pe1_2). All three are laid out in the editor's 1600x1000 design space; the editor gives
// them a scale transform, so every number here is a design pixel.

struct UiKit {
    juce::Typeface::Ptr medium, semi;
    NvaMenuLookAndFeel* menuLnf = nullptr;
    juce::Font font(float h, bool bold = false) const { return juce::Font(juce::FontOptions(bold ? semi : medium).withHeight(h)); }
    const juce::Colour panel{ 0xf8161210 }, cream{ 0xffeadcc2 }, amber{ 0xffffa83a }, walnut{ 0xff4b2a17 },
                       paper{ 0xfff3ead9 }, paperInk{ 0xff2b221b }, paperLine{ 0xff9c8a74 };
};

inline void drawSpacedText(juce::Graphics& g, const juce::String& s, juce::Font f, float kern, float x, float y, float h,
                           juce::Justification j = juce::Justification::centredLeft) {
    g.setFont(f.withExtraKerningFactor(kern));
    g.drawText(s, juce::Rectangle<float>(x, y, 2000.0f, h), j, false);
}
inline float spacedWidth(const juce::String& s, juce::Font f, float kern) {
    return juce::GlyphArrangement::getStringWidth(f.withExtraKerningFactor(kern), s);
}

// ---------------------------------------------------------------------------------------------------------------------
// preset catalogue: factory presets with their categories and card photos, plus the user's saved presets

struct PresetEntry {
    juce::String title; int factoryIndex = -1; juce::File file; juce::StringArray cats; int card = 1;
};

inline juce::StringArray factoryCategories(int index) {
    static const char* table[] = {
        "Clean", "Ambient", "Grit", "Swell", "Clean", "Vintage", "Ambient", "Swell Ambient", "Clean Vintage", "Grit",
        "Clean", "Clean", "Vintage Grit", "Clean Swell", "Ambient Grit", "Ambient", "Ambient", "Clean Vintage",
        "Swell Ambient", "Ambient", "Vintage Swell", "Grit Vintage", "Vintage Clean", "Grit Ambient" };
    return index >= 0 && index < 24 ? juce::StringArray::fromTokens(table[index], " ", "") : juce::StringArray { "Clean" };
}

inline juce::String presetTitle(const juce::String& programName) {   // "01 - Sweet Georgia E9 (Flyover...)" -> "01 Sweet Georgia E9"
    return programName.upToFirstOccurrenceOf("(", false, false).trim().replace(" - ", " ");
}

inline std::vector<PresetEntry> buildCatalogue(PedalSteelAudioProcessor& p) {
    std::vector<PresetEntry> out;
    for (int i = 0; i < p.getNumPrograms(); ++i)
        out.push_back({ presetTitle(p.getProgramName(i)), i, {}, factoryCategories(i), i + 1 });
    for (auto& f : p.getUserPresetFiles()) {
        PresetEntry e; e.title = f.getFileNameWithoutExtension(); e.file = f; e.cats = { "User" };
        if (auto xml = juce::XmlDocument::parse(f)) {
            for (auto& c : juce::StringArray::fromTokens(xml->getStringAttribute("category"), " /", "")) if (c.isNotEmpty()) e.cats.add(c);
            e.card = juce::jlimit(1, 24, xml->getIntAttribute("card", 1));
        }
        out.push_back(e);
    }
    return out;
}

// favourite presets (by title), kept in the per-user UI settings file
struct Favourites {
    static juce::File file() {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Northern Valley Audio").getChildFile("NVA Pedal Steel").getChildFile("favourites.txt");
    }
    static juce::StringArray load() { juce::StringArray a; a.addLines(file().loadFileAsString()); a.removeEmptyStrings(); return a; }
    static void save(const juce::StringArray& a) { file().getParentDirectory().createDirectory(); file().replaceWithText(a.joinIntoString("\n")); }
};

inline juce::Image cardImage(int card) {
    int size = 0;
    const juce::String name = "card_" + juce::String(card).paddedLeft('0', 2) + "_jpg";
    if (auto* data = BinaryData::getNamedResource(name.toRawUTF8(), size)) return juce::ImageCache::getFromMemory(data, size);
    if (auto* data = BinaryData::getNamedResource("card_01_jpg", size)) return juce::ImageCache::getFromMemory(data, size);
    return {};
}

// ---------------------------------------------------------------------------------------------------------------------
// mode menu (mode_2): each mode with a one-line description, a check on the current one; opens above the MODE box

class ModePanel : public juce::Component {
public:
    ModePanel(UiKit& k, juce::AudioParameterChoice& p) : kit(k), param(p) {
        setSize(300, 12 + rowH * param.choices.size());
    }
    std::function<void()> onClose;

    static juce::String describe(int i) {
        static const char* d[] = { "Automatic pedal changes for classic E9 playing.", "Expressive glissando slides for crying lead lines.",
                                   "Soft volume swells that bloom into a pad.", "Control every pedal and knee lever yourself.",
                                   "Trigger expressive steel phrases and licks.", "Bell-like harmonics, chimed over the pickup." };
        return i >= 0 && i < 6 ? d[i] : "";
    }
    void choose(int i) { param.beginChangeGesture(); param = i; param.endChangeGesture(); if (onClose) onClose(); }

    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat();
        juce::DropShadow(juce::Colours::black.withAlpha(0.6f), 18, { 0, 6 }).drawForRectangle(g, r.toNearestInt());
        g.setColour(kit.panel); g.fillRoundedRectangle(r, 8.0f);
        g.setColour(kit.cream.withAlpha(0.22f)); g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 1.0f);
        for (int i = 0; i < param.choices.size(); ++i) {
            auto row = rowBounds(i);
            if (i == hoverRow) { g.setColour(kit.amber.withAlpha(0.14f)); g.fillRoundedRectangle(row.reduced(4.0f, 1.0f), 5.0f); }
            if (i > 0) { g.setColour(kit.cream.withAlpha(0.12f)); g.fillRect(row.getX() + 14.0f, row.getY(), row.getWidth() - 28.0f, 1.0f); }
            g.setColour(kit.cream); g.setFont(kit.font(15.5f));
            g.drawText(param.choices[i], row.withTrimmedLeft(18).withTrimmedRight(40).withHeight(26).withY(row.getY() + 6), juce::Justification::centredLeft);
            g.setColour(kit.cream.withAlpha(0.6f)); g.setFont(kit.font(11.5f));
            g.drawText(describe(i), row.withTrimmedLeft(18).withTrimmedRight(40).withHeight(16).withY(row.getY() + 29), juce::Justification::centredLeft);
            if (i == param.getIndex()) {
                juce::Path tick; const float x = row.getRight() - 30.0f, y = row.getCentreY();
                tick.startNewSubPath(x, y); tick.lineTo(x + 5, y + 5); tick.lineTo(x + 14, y - 6);
                g.setColour(kit.cream); g.strokePath(tick, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }
    }
    void mouseMove(const juce::MouseEvent& e) override { const int r = rowAt(e.position.y); if (r != hoverRow) { hoverRow = r; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hoverRow = -1; repaint(); }
    void mouseUp(const juce::MouseEvent& e) override { const int r = rowAt(e.position.y); if (r >= 0) choose(r); }

private:
    juce::Rectangle<float> rowBounds(int i) const { return { 0.0f, 6.0f + (float) (i * rowH), (float) getWidth(), (float) rowH }; }
    int rowAt(float y) const { const int r = (int) std::floor((y - 6.0f) / rowH); return r >= 0 && r < param.choices.size() ? r : -1; }
    UiKit& kit; juce::AudioParameterChoice& param;
    static constexpr int rowH = 50;
    int hoverRow = -1;
};

// ---------------------------------------------------------------------------------------------------------------------
// preset browser (pb2_2): filters, search, a grid of photo cards; replaces the steel and the knob deck while open

class PresetBrowser : public juce::Component {
public:
    PresetBrowser(UiKit& k, PedalSteelAudioProcessor& p) : kit(k), proc(p) {
        setSize(1572, 786);
        setWantsKeyboardFocus(true);
        favs = Favourites::load();
        search.setFont(kit.font(15.0f));
        search.setTextToShowWhenEmpty("Search presets", kit.cream.withAlpha(0.45f));
        search.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);   // box drawn by paint()
        search.setColour(juce::TextEditor::textColourId, kit.cream);
        search.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        search.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        search.setColour(juce::TextEditor::highlightColourId, kit.amber.withAlpha(0.3f));
        search.setColour(juce::CaretComponent::caretColourId, kit.amber);
        search.setIndents(34, 7);
        search.setBounds(getWidth() - 340, 56, 300, 34);
        search.onTextChange = [this] { refresh(); };
        search.onFocusLost = [this] { repaint(); };
        search.onReturnKey = [this] { if (numShown() > 0) { selected = juce::jmax(0, selected); grabKeyboardFocus(); moveSelection(0); } };
        search.onEscapeKey = [this] { if (search.isEmpty()) close(); else search.clear(); };
        addAndMakeVisible(search);
        refresh();
    }
    std::function<void()> onClose, onSaveAs;

    // small windows: the editor shrinks everything, so the browser enlarges its text and uses fewer, bigger cards
    void setTextScale(float newK) {
        newK = juce::jlimit(1.0f, 1.7f, newK); if (std::abs(newK - k) < 0.01f) return;
        k = newK; search.setFont(kit.font(15.0f * k));
        search.setBounds(getWidth() - (int) (300 * k) - 40, 56, (int) (300 * k), (int) (34 * k)); search.setIndents((int) (34 * k), (int) (7 * k));
        refresh();
    }
    float getTextScale() const { return k; }
    int columns() const { return k >= 1.3f ? 3 : 4; }

    void refresh() {
        all = buildCatalogue(proc);
        shown.clear();
        const auto q = search.getText().trim();
        for (size_t i = 0; i < all.size(); ++i) {
            const auto& e = all[i];
            if (filter == "Favourites" ? ! favs.contains(e.title) : (filter != "All" && ! e.cats.contains(filter))) continue;
            if (q.isNotEmpty() && ! e.title.containsIgnoreCase(q)) continue;
            shown.push_back((int) i);
        }
        scroll = juce::jlimit(0.0f, maxScroll(), scroll);
        selected = juce::jlimit(-1, numShown() - 1, selected);
        repaint();
    }
    // favourites
    bool isFavourite(const juce::String& title) const { return favs.contains(title); }
    void toggleFavourite(int i) {
        if (i < 0 || i >= numShown()) return;
        const auto t = shownTitle(i);
        if (favs.contains(t)) favs.removeString(t); else favs.add(t);
        Favourites::save(favs);
        if (filter == "Favourites") refresh(); else repaint();
    }
    // keyboard: arrows move the selection through the grid, Enter loads, Esc closes, typing goes to the search box
    int getSelected() const { return selected; }
    void moveSelection(int delta) {
        if (numShown() == 0) return;
        selected = selected < 0 ? 0 : juce::jlimit(0, numShown() - 1, selected + delta);
        auto c = cardBounds(selected), ga = gridArea();   // keep it in view
        if (c.getY() < ga.getY()) scroll -= ga.getY() - c.getY();
        if (c.getBottom() > ga.getBottom()) scroll += c.getBottom() - ga.getBottom();
        scroll = juce::jlimit(0.0f, maxScroll(), scroll);
        repaint();
    }
    int numShown() const { return (int) shown.size(); }
    juce::String shownTitle(int i) const { return i >= 0 && i < numShown() ? all[(size_t) shown[(size_t) i]].title : juce::String(); }
    void setFilter(const juce::String& f) { filter = f; scroll = 0; refresh(); }
    void setSearch(const juce::String& s) { search.setText(s, false); refresh(); }
    void activate(int i, bool closeAfter) {
        if (i < 0 || i >= numShown()) return;
        const auto& e = all[(size_t) shown[(size_t) i]];
        if (e.factoryIndex >= 0) proc.setCurrentProgram(e.factoryIndex); else proc.loadUserPreset(e.file);
        repaint();
        if (closeAfter) close();
    }
    void close() { if (onClose) onClose(); }

    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat();
        juce::DropShadow(juce::Colours::black.withAlpha(0.7f), 24, { 0, 8 }).drawForRectangle(g, r.toNearestInt());
        juce::ColourGradient wood(juce::Colour(0xff6b3a1c), 0, 0, juce::Colour(0xff3d1f0e), 0, r.getHeight(), false);
        g.setGradientFill(wood); g.fillRoundedRectangle(r, 12.0f);
        auto in = r.reduced(7.0f);
        g.setColour(juce::Colour(0xff15110e)); g.fillRoundedRectangle(in, 7.0f);
        g.setColour(juce::Colours::black.withAlpha(0.5f)); g.drawRoundedRectangle(in, 7.0f, 1.5f);

        // header
        g.setColour(kit.cream);
        const auto title = juce::String("PRESET BROWSER"); const auto tf = kit.font(22.0f * k);
        drawSpacedText(g, title, tf, 0.32f, 32.0f, 16.0f, 34.0f * k);
        const float tw = spacedWidth(title, tf, 0.32f);
        g.setColour(kit.cream.withAlpha(0.35f)); g.fillRect(32.0f + tw + 18.0f, 33.0f, r.getWidth() - (32.0f + tw + 18.0f) - 72.0f, 1.0f);
        auto x = closeBounds(); g.setColour(hoverClose ? kit.amber : kit.cream.withAlpha(0.85f));
        g.drawLine(x.getX() + 6, x.getY() + 6, x.getRight() - 6, x.getBottom() - 6, 2.0f);
        g.drawLine(x.getRight() - 6, x.getY() + 6, x.getX() + 6, x.getBottom() - 6, 2.0f);

        // filters
        for (size_t i = 0; i < filters.size(); ++i) {
            auto b = filterBounds((int) i); const bool on = filters[i] == filter;
            g.setColour(on ? kit.cream : (hoverFilter == (int) i ? kit.cream.withAlpha(0.12f) : juce::Colours::transparentBlack));
            g.fillRoundedRectangle(b, b.getHeight() * 0.5f);
            g.setColour(kit.cream.withAlpha(on ? 1.0f : 0.55f)); g.drawRoundedRectangle(b.reduced(0.5f), b.getHeight() * 0.5f, 1.0f);
            g.setColour(on ? juce::Colour(0xff1a1511) : kit.cream); g.setFont(kit.font(14.0f * k));
            g.drawText(filters[i], b, juce::Justification::centred);
        }
        // search box (rounded, like the filter pills) with a magnifier
        {
            auto sb = search.getBounds().toFloat(); const float cx = sb.getX() + 17.0f, cy = sb.getCentreY();
            g.setColour(juce::Colour(0xff0d0b09)); g.fillRoundedRectangle(sb, sb.getHeight() * 0.5f);
            g.setColour(search.hasKeyboardFocus(true) ? kit.amber.withAlpha(0.8f) : kit.cream.withAlpha(0.35f));
            g.drawRoundedRectangle(sb.reduced(0.5f), sb.getHeight() * 0.5f, 1.0f);
            g.setColour(kit.cream.withAlpha(0.6f)); g.drawEllipse(cx - 6, cy - 6, 10, 10, 1.6f); g.drawLine(cx + 2.5f, cy + 2.5f, cx + 7, cy + 7, 1.8f);
        }

        // cards
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(gridArea().toNearestInt());
            const int cur = proc.getCurrentProgram(); const auto user = proc.getActiveUserPresetName();
            for (int i = 0; i < numShown(); ++i) {
                auto c = cardBounds(i); if (! c.intersects(gridArea())) continue;
                const auto& e = all[(size_t) shown[(size_t) i]];
                auto photo = c.withHeight(c.getWidth() / 2.3f);
                auto img = cardImage(e.card);
                {
                    juce::Graphics::ScopedSaveState s2(g);
                    juce::Path clip; clip.addRoundedRectangle(photo, 7.0f); g.reduceClipRegion(clip);
                    g.setOpacity(1.0f);   // images take the current colour's alpha
                    if (img.isValid()) g.drawImage(img, photo, juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
                    if (i != hoverCard) { g.setColour(juce::Colours::black.withAlpha(0.12f)); g.fillRect(photo); }
                }
                const bool current = e.factoryIndex >= 0 ? (user.isEmpty() && e.factoryIndex == cur) : (user == e.title);
                if (current) {
                    g.setColour(kit.amber.withAlpha(0.35f)); g.drawRoundedRectangle(photo.expanded(2.5f), 9.0f, 4.0f);
                    g.setColour(kit.amber); g.drawRoundedRectangle(photo.expanded(1.0f), 8.0f, 2.0f);
                } else {
                    g.setColour(kit.cream.withAlpha(i == hoverCard ? 0.6f : 0.14f)); g.drawRoundedRectangle(photo, 7.0f, 1.0f);
                }
                g.setColour(kit.cream.withAlpha(current ? 1.0f : 0.88f)); g.setFont(kit.font(15.0f * k));
                g.drawText(e.title, juce::Rectangle<float>(c.getX(), photo.getBottom() + 4.0f, c.getWidth(), 24.0f * k), juce::Justification::centred, true);
                {   // favourite star: always on favourites, on hover otherwise
                    const bool fav = favs.contains(e.title);
                    if (fav || i == hoverCard) {
                        auto sb = starBounds(photo);
                        g.setColour(juce::Colour(0xc0151210)); g.fillEllipse(sb.expanded(3.0f));
                        auto star = starPath(sb.reduced(3.0f));
                        if (fav) { g.setColour(kit.amber); g.fillPath(star); }
                        else { g.setColour(kit.cream.withAlpha(0.85f)); g.strokePath(star, juce::PathStrokeType(1.4f)); }
                    }
                }
                if (i == selected) {   // keyboard selection
                    g.setColour(kit.cream.withAlpha(0.9f)); g.drawRoundedRectangle(photo.expanded(4.0f), 10.0f, 1.6f);
                }
                if (e.factoryIndex < 0) {   // user preset badge
                    auto badge = juce::Rectangle<float>(photo.getRight() - 52.0f, photo.getY() + 8.0f, 44.0f, 18.0f);
                    g.setColour(juce::Colour(0xe0151210)); g.fillRoundedRectangle(badge, 9.0f);
                    g.setColour(kit.amber); g.setFont(kit.font(10.5f, true)); g.drawText("USER", badge, juce::Justification::centred);
                }
            }
            if (numShown() == 0) {   // say why the grid is empty
                const auto msg = filter == "User" && search.isEmpty() ? juce::String("No user presets yet. Use Save As to keep the current sound.")
                                                                       : juce::String("No presets match.");
                g.setColour(kit.cream.withAlpha(0.6f)); g.setFont(kit.font(16.0f * k));
                g.drawText(msg, gridArea().withHeight(120.0f), juce::Justification::centred, true);
            }
            if (maxScroll() > 0) {   // thin scroll bar
                auto ga = gridArea(); const float frac = ga.getHeight() / (ga.getHeight() + maxScroll());
                const float h = ga.getHeight() * frac, y = ga.getY() + (ga.getHeight() - h) * (scroll / maxScroll());
                g.setColour(kit.cream.withAlpha(0.25f)); g.fillRoundedRectangle(ga.getRight() + 6.0f, y, 4.0f, h, 2.0f);
            }
        }
        // footer
        g.setColour(juce::Colour(0x22eadcc2)); g.fillRect(24.0f, r.getHeight() - 62.0f, r.getWidth() - 48.0f, 1.0f);
        g.setColour(kit.cream.withAlpha(0.7f)); g.setFont(kit.font(14.0f * k));
        g.drawText(juce::String(numShown()) + " of " + juce::String((int) all.size()) + " presets",
                   juce::Rectangle<float>(32.0f, r.getHeight() - 52.0f, 300.0f, 34.0f), juce::Justification::centredLeft);
        auto sa = saveAsBounds();
        g.setColour(hoverSaveAs ? kit.cream.withAlpha(0.14f) : juce::Colours::transparentBlack); g.fillRoundedRectangle(sa, 5.0f);
        g.setColour(kit.cream.withAlpha(0.7f)); g.drawRoundedRectangle(sa.reduced(0.5f), 5.0f, 1.0f);
        g.setColour(kit.cream); g.setFont(kit.font(14.5f * k)); g.drawText("Save As", sa, juce::Justification::centred);
    }

    void mouseMove(const juce::MouseEvent& e) override {
        const auto p = e.position;
        const int hc = cardAt(p), hf = filterAt(p); const bool hx = closeBounds().contains(p), hs = saveAsBounds().contains(p);
        if (hc != hoverCard || hf != hoverFilter || hx != hoverClose || hs != hoverSaveAs) {
            hoverCard = hc; hoverFilter = hf; hoverClose = hx; hoverSaveAs = hs; repaint();
        }
        setMouseCursor(hc >= 0 || hf >= 0 || hx || hs ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit(const juce::MouseEvent&) override { hoverCard = hoverFilter = -1; hoverClose = hoverSaveAs = false; repaint(); }
    void mouseUp(const juce::MouseEvent& e) override {
        const auto p = e.position;
        if (closeBounds().contains(p)) { close(); return; }
        if (saveAsBounds().contains(p)) { if (onSaveAs) onSaveAs(); return; }
        if (const int f = filterAt(p); f >= 0) { setFilter(filters[(size_t) f]); return; }
        if (const int c = cardAt(p); c >= 0) {
            auto photo = cardBounds(c).withHeight(cardBounds(c).getWidth() / 2.3f);
            if (starBounds(photo).expanded(4.0f).contains(p)) { toggleFavourite(c); return; }
            selected = c; activate(c, e.getNumberOfClicks() >= 2);
        }
    }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override {
        scroll = juce::jlimit(0.0f, maxScroll(), scroll - w.deltaY * 260.0f); repaint();
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if (key == juce::KeyPress::escapeKey) { close(); return true; }
        const int code = key.getKeyCode(), n = columns();
        if (code == juce::KeyPress::leftKey) { moveSelection(-1); return true; }
        if (code == juce::KeyPress::rightKey) { moveSelection(+1); return true; }
        if (code == juce::KeyPress::upKey) { moveSelection(-n); return true; }
        if (code == juce::KeyPress::downKey) { moveSelection(+n); return true; }
        if (code == juce::KeyPress::returnKey && selected >= 0) { activate(selected, true); return true; }
        if (code == juce::KeyPress::spaceKey && selected >= 0) { activate(selected, false); return true; }
        if (key.getTextCharacter() == 'f' && selected >= 0 && ! search.hasKeyboardFocus(false)) { toggleFavourite(selected); return true; }
        if (key.getTextCharacter() > ' ') { search.grabKeyboardFocus(); search.insertTextAtCaret(juce::String::charToString(key.getTextCharacter())); return true; }
        return false;
    }

private:
    juce::Rectangle<float> closeBounds() const { return { (float) getWidth() - 28.0f - 28.0f * k, 20.0f, 28.0f * k, 28.0f * k }; }
    juce::Rectangle<float> saveAsBounds() const { return { (float) getWidth() - 36.0f - 124.0f * k, (float) getHeight() - 18.0f - 32.0f * k, 124.0f * k, 32.0f * k }; }
    juce::Rectangle<float> gridArea() const { const float top = 64.0f + 44.0f * k; return { 30.0f, top, (float) getWidth() - 60.0f, (float) getHeight() - top - 70.0f }; }
    juce::Rectangle<float> filterBounds(int i) const {
        float x = 32.0f;
        for (int j = 0; j < i; ++j) x += spacedWidth(filters[(size_t) j], kit.font(14.0f * k), 0.0f) + 36.0f * k + 10.0f;
        return { x, 58.0f, spacedWidth(filters[(size_t) i], kit.font(14.0f * k), 0.0f) + 36.0f * k, 30.0f * k };
    }
    juce::Rectangle<float> cardBounds(int i) const {
        const int n = columns();
        auto ga = gridArea(); const float gap = 20.0f, w = (ga.getWidth() - (n - 1) * gap) / (float) n, h = w / 2.3f + 10.0f + 24.0f * k;
        return { ga.getX() + (i % n) * (w + gap), ga.getY() + (i / n) * (h + 14.0f) - scroll, w, h };
    }
    float maxScroll() const {
        if (numShown() == 0) return 0.0f;
        auto last = cardBounds(numShown() - 1); return juce::jmax(0.0f, last.getBottom() + scroll - gridArea().getBottom());
    }
    int cardAt(juce::Point<float> p) const {
        if (! gridArea().contains(p)) return -1;
        for (int i = 0; i < numShown(); ++i) if (cardBounds(i).contains(p)) return i;
        return -1;
    }
    int filterAt(juce::Point<float> p) const {
        for (size_t i = 0; i < filters.size(); ++i) if (filterBounds((int) i).contains(p)) return (int) i;
        return -1;
    }

    UiKit& kit; PedalSteelAudioProcessor& proc;
    juce::TextEditor search;
    std::vector<juce::String> filters { "All", "Favourites", "Clean", "Grit", "Swell", "Ambient", "Vintage", "User" };
    juce::String filter = "All";
    std::vector<PresetEntry> all; std::vector<int> shown;
    float scroll = 0.0f, k = 1.0f;
    int hoverCard = -1, hoverFilter = -1; bool hoverClose = false, hoverSaveAs = false;
    int selected = -1; juce::StringArray favs;
    juce::Rectangle<float> starBounds(juce::Rectangle<float> photo) const { return { photo.getX() + 8.0f, photo.getY() + 8.0f, 26.0f * k, 26.0f * k }; }
    static juce::Path starPath(juce::Rectangle<float> r) {
        juce::Path p; p.addStar(r.getCentre(), 5, r.getWidth() * 0.22f, r.getWidth() * 0.5f, 0.0f); return p;
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// preset editor / save (pe1_2): name, category, steel finish, notes; Cancel / Save

class PresetSaveDialog : public juce::Component {
public:
    PresetSaveDialog(UiKit& k, PedalSteelAudioProcessor& p) : kit(k), proc(p) {
        setSize(1090, 640);
        auto style = [this](juce::TextEditor& t, float fontH) {
            t.setFont(kit.font(fontH));
            t.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xfffbf7ef));
            t.setColour(juce::TextEditor::textColourId, kit.paperInk);
            t.setColour(juce::TextEditor::outlineColourId, kit.paperLine);
            t.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff6b4a2a));
            t.setColour(juce::TextEditor::highlightColourId, kit.amber.withAlpha(0.35f));
            t.setColour(juce::TextEditor::highlightedTextColourId, kit.paperInk);
            t.setColour(juce::CaretComponent::caretColourId, kit.paperInk);
            addAndMakeVisible(t);
        };
        style(name, 18.0f); name.setIndents(12, 9); name.setBounds(40, 122, 610, 44);
        style(notes, 15.0f); notes.setMultiLine(true, true); notes.setReturnKeyStartsNewLine(true); notes.setIndents(12, 10);
        notes.setTextToShowWhenEmpty("Add a note about this preset...", kit.paperLine);
        notes.setBounds(40, 414, 1010, 104);
        name.onReturnKey = [this] { save(); };
        name.onEscapeKey = notes.onEscapeKey = [this] { cancel(); };
        name.onTextChange = [this] { nameError = false; repaint(); };

        // defaults from the current preset
        const auto user = proc.getActiveUserPresetName();
        const int cur = proc.getCurrentProgram();
        name.setText(user.isNotEmpty() ? user : "My " + presetTitle(proc.getProgramName(cur)).fromFirstOccurrenceOf(" ", false, false), false);
        category = factoryCategories(cur)[0];
        card = cur + 1;
    }
    std::function<void()> onClose;
    std::function<void(const juce::File&)> onSaved;

    void setName(const juce::String& s) { name.setText(s, true); }
    void setFinish(int i) { finish = juce::jlimit(0, 4, i); repaint(); }
    void setCategory(const juce::String& c) { category = c; repaint(); }
    void setNotes(const juce::String& s) { notes.setText(s, false); }
    bool replaceWithoutAsking = false;   // tests

    void cancel() { if (onClose) onClose(); }
    void save() {
        const auto n = juce::File::createLegalFileName(name.getText().trim());
        if (n.isEmpty()) { nameError = true; repaint(); return; }
        auto dir = PedalSteelAudioProcessor::getUserPresetsFolder(); dir.createDirectory();
        const auto file = dir.getChildFile(n + ".nvps");
        if (file.existsAsFile() && ! replaceWithoutAsking) {
            juce::Component::SafePointer<PresetSaveDialog> safe(this);
            juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon)
                                             .withTitle("Replace preset?").withMessage("A preset called \"" + n + "\" already exists.")
                                             .withButton("Replace").withButton("Cancel").withAssociatedComponent(this),
                                         [safe, file](int r) { if (safe != nullptr && r == 1) safe->write(file); });
            return;
        }
        write(file);
    }

    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat();
        juce::DropShadow(juce::Colours::black.withAlpha(0.65f), 30, { 0, 10 }).drawForRectangle(g, r.toNearestInt());
        juce::ColourGradient wood(juce::Colour(0xff6e3c1d), 0, 0, juce::Colour(0xff3b1d0c), 0, r.getHeight(), false);
        g.setGradientFill(wood); g.fillRoundedRectangle(r, 12.0f);
        auto in = r.reduced(8.0f);
        juce::ColourGradient pap(juce::Colour(0xfff6efe1), 0, in.getY(), juce::Colour(0xffeee2cc), 0, in.getBottom(), false);
        g.setGradientFill(pap); g.fillRoundedRectangle(in, 6.0f);

        g.setColour(kit.paperInk);
        const juce::String title = "PRESET EDITOR / SAVE"; const auto tf = kit.font(27.0f, true);
        drawSpacedText(g, title, tf, 0.06f, 40.0f, 30.0f, 40.0f);
        const float tw = spacedWidth(title, tf, 0.06f);
        g.setColour(kit.paperInk.withAlpha(0.5f)); g.fillRect(40.0f + tw + 20.0f, 51.0f, 1050.0f - (40.0f + tw + 20.0f), 1.0f);

        auto label = [&](const juce::String& s, float x, float y) {
            g.setColour(kit.paperInk); drawSpacedText(g, s, kit.font(12.5f, true), 0.2f, x, y, 18.0f);
        };
        label("PRESET NAME", 40.0f, 98.0f);
        if (nameError) { g.setColour(juce::Colour(0xffa03020)); g.setFont(kit.font(12.5f)); g.drawText("Give the preset a name", juce::Rectangle<float>(200.0f, 98.0f, 300.0f, 18.0f), juce::Justification::centredLeft); }
        label("CATEGORY", 690.0f, 98.0f);
        auto cb = categoryBounds();
        g.setColour(juce::Colour(0xfffbf7ef)); g.fillRect(cb); g.setColour(kit.paperLine); g.drawRect(cb, 1.0f);
        g.setColour(kit.paperInk); g.setFont(kit.font(17.0f)); g.drawText(categoryLabel(category), cb.withTrimmedLeft(14), juce::Justification::centredLeft);
        { juce::Path tri; const float x = cb.getRight() - 24.0f, y = cb.getCentreY(); tri.addTriangle(x - 6, y - 3, x + 6, y - 3, x, y + 4); g.fillPath(tri); }

        label("STEEL FINISH", 40.0f, 194.0f);
        for (int i = 0; i < 5; ++i) {
            auto sb = swatchBounds(i);
            juce::DropShadow(juce::Colours::black.withAlpha(0.35f), 6, { 0, 2 }).drawForRectangle(g, sb.toNearestInt());
            {
                juce::Graphics::ScopedSaveState ss(g); juce::Path clip; clip.addRoundedRectangle(sb, 9.0f); g.reduceClipRegion(clip);
                int sz = 0; const juce::String rn = juce::String("finish_") + finishIds[i] + "_png";
                g.setOpacity(1.0f);
                if (auto* d = BinaryData::getNamedResource(rn.toRawUTF8(), sz)) g.drawImage(juce::ImageCache::getFromMemory(d, sz), sb, juce::RectanglePlacement::fillDestination);
            }
            g.setColour(juce::Colours::black.withAlpha(0.45f)); g.drawRoundedRectangle(sb, 9.0f, 1.2f);
            if (i == finish) {
                g.setColour(kit.paperInk); g.drawRoundedRectangle(sb.expanded(5.0f), 12.0f, 2.5f);
                auto c = juce::Rectangle<float>(22.0f, 22.0f).withCentre({ sb.getRight() - 14.0f, sb.getY() + 14.0f });
                g.setColour(juce::Colour(0xf0f8f2e6)); g.fillEllipse(c);
                juce::Path tick; tick.startNewSubPath(c.getX() + 6, c.getCentreY()); tick.lineTo(c.getX() + 10, c.getCentreY() + 4); tick.lineTo(c.getRight() - 5, c.getY() + 7);
                g.setColour(kit.paperInk); g.strokePath(tick, juce::PathStrokeType(2.0f));
            }
            g.setColour(kit.paperInk); g.setFont(kit.font(15.0f));
            g.drawText(finishNames[i], juce::Rectangle<float>(sb.getX() - 10, sb.getBottom() + 8, sb.getWidth() + 20, 22), juce::Justification::centred);
        }
        g.setColour(kit.paperInk.withAlpha(0.25f)); g.fillRect(40.0f, 372.0f, 1010.0f, 1.0f);
        label("NOTES", 40.0f, 390.0f);

        auto drawButton = [&](juce::Rectangle<float> b, const juce::String& s, bool primary, bool hot) {
            if (primary) { g.setColour(hot ? juce::Colour(0xff2e251e) : kit.paperInk); g.fillRoundedRectangle(b, 6.0f); g.setColour(kit.paper); }
            else { g.setColour(hot ? juce::Colour(0xffe6d9c2) : juce::Colour(0xfff6efe1)); g.fillRoundedRectangle(b, 6.0f);
                   g.setColour(kit.paperInk.withAlpha(0.7f)); g.drawRoundedRectangle(b.reduced(0.5f), 6.0f, 1.2f); g.setColour(kit.paperInk); }
            g.setFont(kit.font(16.5f)); g.drawText(s, b, juce::Justification::centred);
        };
        drawButton(cancelBounds(), "Cancel", false, hover == 1);
        drawButton(saveBounds(), "Save", true, hover == 2);
    }
    void mouseMove(const juce::MouseEvent& e) override {
        const int h = cancelBounds().contains(e.position) ? 1 : saveBounds().contains(e.position) ? 2 : 0;
        if (h != hover) { hover = h; repaint(); }
        const bool hand = h != 0 || categoryBounds().contains(e.position) || swatchAt(e.position) >= 0;
        setMouseCursor(hand ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseUp(const juce::MouseEvent& e) override {
        const auto p = e.position;
        if (cancelBounds().contains(p)) { cancel(); return; }
        if (saveBounds().contains(p)) { save(); return; }
        if (const int s = swatchAt(p); s >= 0) { setFinish(s); return; }
        if (categoryBounds().contains(p)) {
            juce::PopupMenu m; if (kit.menuLnf != nullptr) m.setLookAndFeel(kit.menuLnf);
            for (int i = 0; i < categories.size(); ++i) m.addItem(i + 1, categoryLabel(categories[i]), true, categories[i] == category);
            juce::Component::SafePointer<PresetSaveDialog> safe(this);
            m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(localAreaToGlobal(categoryBounds().toNearestInt()))
                                .withMinimumWidth(juce::roundToInt(categoryBounds().getWidth() * getTransform().getScaleFactor())),
                            [safe](int r) { if (safe != nullptr && r > 0) safe->setCategory(safe->categories[r - 1]); });
        }
    }
    bool keyPressed(const juce::KeyPress& k) override {
        if (k == juce::KeyPress::escapeKey) { cancel(); return true; }
        return false;
    }

    const juce::StringArray categories { "Clean", "Grit", "Swell", "Ambient", "Vintage" };

private:
    static juce::String categoryLabel(const juce::String& c) {
        if (c == "Swell") return "Ambient / Swells";
        if (c == "Ambient") return "Ambient / Textures";
        if (c == "Grit") return "Grit / Drive";
        if (c == "Vintage") return "Vintage / Tape";
        return "Clean";
    }
    void write(const juce::File& file) {
        if (! proc.saveUserPreset(file)) return;
        if (auto xml = juce::XmlDocument::parse(file)) {   // what the browser and (later) the instrument finish need
            xml->setAttribute("category", category);
            xml->setAttribute("finish", finishIds[finish]);
            xml->setAttribute("card", card);
            xml->setAttribute("notes", notes.getText());
            xml->writeTo(file);
        }
        proc.loadUserPreset(file);
        if (onSaved) onSaved(file);
        if (onClose) onClose();
    }
    juce::Rectangle<float> categoryBounds() const { return { 690.0f, 122.0f, 360.0f, 44.0f }; }
    juce::Rectangle<float> swatchBounds(int i) const { return { 44.0f + i * 136.0f, 222.0f, 108.0f, 104.0f }; }
    juce::Rectangle<float> cancelBounds() const { return { 690.0f, 556.0f, 170.0f, 48.0f }; }
    juce::Rectangle<float> saveBounds() const { return { 880.0f, 556.0f, 170.0f, 48.0f }; }
    int swatchAt(juce::Point<float> p) const { for (int i = 0; i < 5; ++i) if (swatchBounds(i).expanded(4).contains(p)) return i; return -1; }

    UiKit& kit; PedalSteelAudioProcessor& proc;
    juce::TextEditor name, notes;
    juce::String category = "Clean";
    int finish = 0, card = 1, hover = 0;
    bool nameError = false;
    const char* finishIds[5] = { "walnut", "maple", "redsparkle", "pearloid", "koa" };
    const char* finishNames[5] = { "Walnut", "Maple", "Red Sparkle", "Pearloid", "Koa" };
};
