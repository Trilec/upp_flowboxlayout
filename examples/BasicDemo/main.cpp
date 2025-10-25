#include <CtrlLib/CtrlLib.h>
#include <FlowBoxLayout/FlowBoxLayout.h>
#include <limits.h>

using namespace Upp;

// --------------------------------------------------------------
// ColorTile: simple colored, labeled tile
// --------------------------------------------------------------
class ColorTile : public Ctrl {
public:
    typedef ColorTile CLASSNAME;
    ColorTile() { NoWantFocus(); Transparent(false); }

    ColorTile& SetLabel(const String& s) { label = s; Refresh(); return *this; }
    ColorTile& SetColor(Color c)         { bg = c;   Refresh(); return *this; }
    ColorTile& SetMin(Size sz)           { minsz = sz; Refresh(); return *this; }

    virtual void Paint(Draw& w) override {
        const Rect r = GetSize();
        w.DrawRect(r, bg);
        const Color br = IsDark(bg) ? SColorHighlight() : SColorText();
        w.DrawRect(r.left, r.top, r.GetWidth(), 1, br);
        w.DrawRect(r.left, r.bottom-1, r.GetWidth(), 1, br);
        w.DrawRect(r.left, r.top, 1, r.GetHeight(), br);
        w.DrawRect(r.right-1, r.top, 1, r.GetHeight(), br);

        Font f = StdFont().Bold();
        Size ts = GetTextSize(label, f);
        Point p((r.left + r.right - ts.cx) / 2, (r.top + r.bottom - ts.cy) / 2);
        w.DrawText(p.x, p.y, label, f, IsDark(bg) ? White() : SColorText());
    }

    virtual Size GetMinSize() const override {
        return (minsz.cx > 0 || minsz.cy > 0) ? minsz : Size(100, 36);
    }

private:
    String label = "Tile";
    Color  bg    = LtGray();
    Size   minsz = Size(0,0);

    static bool IsDark(Color c) {
        int lum = (30 * c.GetR() + 59 * c.GetG() + 11 * c.GetB()) / 100;
        return lum < 128;
    }
};

static inline Color LightHue(Color c) { return Blend(c, White(), 20000); }
static inline Color BoldHue (Color c) { return Color( (c.GetR()*5)/6, (c.GetG()*5)/6, (c.GetB()*5)/6 ); }

// --------------------------------------------------------------
// Quadrant: title + a FlowBoxLayout body; builds demo layouts
// --------------------------------------------------------------
struct Quadrant : public ParentCtrl {
    typedef Quadrant CLASSNAME;

    FlowBoxLayout root { FlowBoxLayout::V };
    ColorTile     title;

    Array<ColorTile>     tiles;
    Array<FlowBoxLayout> layouts;

    Quadrant() {
        Add(root.SizePos());
        root.SetGap(DPI(6)).SetInset(DPI(8), DPI(8));
        root.AddFit(title).MinMaxHeight(DPI(28), DPI(34));
    }

    void SetTitle(const String& s, Color base) {
        title.SetLabel(s).SetColor(BoldHue(base));
    }

    // ---- Demo 1: Holy Grail ---------------------------------
   void BuildHolyGrail(Color base) {
    SetTitle("Holy Grail Layout", base);
    Color LightBase = Blend(base, White(), 50);
    // Header
    ColorTile& header = tiles.Add();
    header.SetLabel("Header").SetColor( LightBase );
    root.AddFixed(header, DPI(40));

    // Middle row container (left | main | right)
    FlowBoxLayout& mid = layouts.Add();
    mid.SetDirection(FlowBoxLayout::H);
    mid.SetGap(DPI(6)).SetInset(DPI(2), DPI(2));
    mid.SetAlignItems(FlowBoxLayout::Stretch);

    // Left sidebar
    ColorTile& left = tiles.Add();
    left.SetLabel("Left Sidebar").SetColor(LightBase);

    // Main content is a FlowBox grid that wraps tiny tiles
    FlowBoxLayout& main = layouts.Add();
    main.SetDirection(FlowBoxLayout::H)
        .SetWrap(true)
        .SetGap(DPI(2))
        .SetAlignItems(FlowBoxLayout::Start);

    // 60 tiny 20x20 tiles (no labels to reduce overdraw)
    for (int i = 0; i < 60; ++i) {
        ColorTile& tiny = tiles.Add();
        tiny.SetLabel("")
            .SetColor((i & 1) ? base : LightBase)
            .SetMin(Size(DPI(20), DPI(20)));
        main.AddFit(tiny);
    }

    // Right sidebar
    ColorTile& right = tiles.Add();
    right.SetLabel("Right Sidebar").SetColor(LightBase);

    // Compose middle row
    mid.AddFixed(left, DPI(140)).MinMaxHeight(DPI(80), INT_MAX);
    mid.Add(main).Expand().MinMaxHeight(DPI(80), INT_MAX);
    mid.AddFixed(right, DPI(140)).MinMaxHeight(DPI(80), INT_MAX);

    root.Add(mid).Expand(1);

    // Footer
    ColorTile& footer = tiles.Add();
    footer.SetLabel("Footer").SetColor( Blend(base, White(), 80) );
    root.AddFixed(footer, DPI(36));
}


    // ---- Demo 2: Magazine -----------------------------------
    void BuildMagazine(Color base) {
        SetTitle("Magazine Layout", base);
        Color LightBase = Blend(base, White(), 50);
        ColorTile& hero = tiles.Add();
        hero.SetLabel("Featured Article (Hero)").SetColor(LightBase);
        root.AddFixed(hero, DPI(60));

        FlowBoxLayout& grid = layouts.Add();
        grid.SetDirection(FlowBoxLayout::H);
        grid.SetGap(DPI(6)).SetInset(DPI(2), DPI(2));
        grid.SetAlignItems(FlowBoxLayout::Stretch);

        FlowBoxLayout& leftcol = layouts.Add();
        leftcol.SetDirection(FlowBoxLayout::V);
        leftcol.SetGap(DPI(6)).SetInset(DPI(2), DPI(2)).SetAlignItems(FlowBoxLayout::Stretch);

        ColorTile& tall = tiles.Add();
        tall.SetLabel("Tall Card").SetColor(base);
        leftcol.Add(tall).Expand(1);

        FlowBoxLayout& rightcol = layouts.Add();
        rightcol.SetDirection(FlowBoxLayout::V);
        rightcol.SetGap(DPI(6)).SetInset(DPI(2), DPI(2)).SetAlignItems(FlowBoxLayout::Stretch);

        for(int i=1;i<=3;i++){
            ColorTile& s = tiles.Add();
            s.SetLabel(Format("Short Card %d", i)).SetColor((i & 1) ? base : LightBase);
            rightcol.AddFixed(s, DPI(60));
        }

        grid.Add(leftcol).Expand();
        grid.Add(rightcol).Expand();

        root.Add(grid).Expand(1);

        ColorTile& foot = tiles.Add();
        foot.SetLabel("Footer").SetColor(LightBase);
        root.AddFixed(foot, DPI(36));
    }

    // ---- Demo 3: SPA (original) ------------------------------
    void BuildSPA(Color base) {
        SetTitle("Single-Page App Layout", base);
        Color LightBase = Blend(base, White(), 40);
        
        ColorTile& hdr = tiles.Add();
        hdr.SetLabel("Sticky Header").SetColor(LightBase);
        root.AddFixed(hdr, DPI(44));

        FlowBoxLayout& sections = layouts.Add();
        sections.SetDirection(FlowBoxLayout::V);
        sections.SetGap(DPI(6)).SetInset(DPI(2), DPI(2)).SetAlignItems(FlowBoxLayout::Stretch);

        for(const char* s : { "Home", "Profile", "Settings", "CTA" }) {
            int i = 0;
            ColorTile& t = tiles.Add();
            t.SetLabel(Format("%s Section", s)).SetColor((i++ & 1) ? base : LightBase);
            sections.AddFixed(t, DPI(70));
        }
        root.Add(sections).Expand(1);
    }

    // ---- Demo 4: Card Grid ----------------------------------
    void BuildCardGrid(Color base) {
        SetTitle("Card Grid Layout", base);
        Color LightBase = Blend(base, White(), 70);
        
        ColorTile& header = tiles.Add();
        header.SetLabel("Header").SetColor(base);
        root.AddFixed(header, DPI(40));

        FlowBoxLayout& mid = layouts.Add();
        mid.SetDirection(FlowBoxLayout::H)
           .SetWrap(true)
           .SetGap(DPI(6))
           .SetInset(DPI(6), DPI(6))
           .SetAlignItems(FlowBoxLayout::Stretch)
           .SetFixedColumn(DPI(160)); // one cell == one column

        srand(1337); // deterministic demo

        auto bucket_h = [&](int i)->Size {
            switch(i % 4){
                case 0: return Size(DPI(10),  DPI(30));   // XS
                case 1: return Size(DPI(20),  DPI(50));   // SM
                case 2: return Size(DPI(30),  DPI(80));   // L
                default:return Size(DPI(50),  DPI(190));  // XL
            }
        };

        for(int i = 1; i <= 10; i++) {
            if(i == 5 || i == 7) mid.AddSpacer(1);   // one empty column
            if(i == 3)           mid.AddBreak();     // hard newline (no width)

            ColorTile& c = tiles.Add();
            c.SetColor((i % 2) ? base : LightBase );

            auto ref = mid.Add(c);

            bool exp = (i & 1);
            if(exp) ref.Expand(1); else ref.Fit();

            FlowBoxLayout::Align va;
            switch(i % 4) {
                case 0: va = FlowBoxLayout::Start;  break;
                case 1: va = FlowBoxLayout::Center; break;
                case 2: va = FlowBoxLayout::End;    break;
                default: va = FlowBoxLayout::Stretch; break;
            }
            Size h = bucket_h(i + 17);
            ref.MinMaxHeight(h.cx, h.cy)
               .AlignSelf(va);

            c.SetLabel(Format("Card %d (%s, %s)", i,
                              exp ? "Expand" : "Fit",
                              va==FlowBoxLayout::Stretch ? "Stretch" :
                              va==FlowBoxLayout::Start   ? "Start"   :
                              va==FlowBoxLayout::Center  ? "Center"  : "End"));
        }

        root.Add(mid).Expand();

        ColorTile& footer = tiles.Add();
        footer.SetLabel("Footer").SetColor(LightBase);
        root.AddFixed(footer, DPI(36));
    }

    // ---- Demo 5: SPA mock (nav + cards) ---------------------
    // ---- Demo 5: SPA mock (nav + cards) ---------------------
void BuildSPA_Mock(Color base) {
    SetTitle("SPA Layout", base);
    Color LightBase = Blend(base, White(), 60);

    // Top nav: fixed height
    ColorTile& nav = tiles.Add();
    nav.SetLabel("Navigation").SetColor(LightBase);
    root.AddFixed(nav, DPI(40));

    // Cards column: will fill all remaining space
    FlowBoxLayout& cards = layouts.Add();
    cards.SetDirection(FlowBoxLayout::V)
         .SetGap(DPI(12))
         .SetInset(DPI(10), DPI(10))
         .SetAlignItems(FlowBoxLayout::Stretch);

    auto card = [&](const char* t){
        ColorTile& c = tiles.Add();
        c.SetLabel(t).SetColor(LightBase);
        cards.Add(c).Expand(1);                // <-- equal share of remaining height
        // Optionally cap how small they can get:
        // .MinMaxHeight(DPI(64), INT_MAX);
    };

    card("Home");
    card("Profile");
    card("Settings");

    // Let the cards column consume all space not used by the nav
    root.Add(cards).Expand();
}

    // ---- Demo 6: F-Pattern ----------------------------------
    void BuildFPattern(Color base) {
	    SetTitle("F Pattern Layout", base);
	    Color LightBase = Blend(base, White(), 60);
	    
		FlowBoxLayout& metrics = layouts.Add();
		metrics.SetDirection(FlowBoxLayout::H)
		       .SetWrap(true)
		       .SetGap(DPI(4))
		       .SetInset(0,0)
		       .SetAlignItems(FlowBoxLayout::Stretch)
		       .SetFixedRow(DPI(24))          // per-line height
		       .SetWrapAutoResize();          // enable height-for-width measurement
		
		auto metric = [&](const char* t){
		    ColorTile& m = tiles.Add();
		    m.SetLabel(t).SetColor(LightBase);
		    metrics.Add(m).MinMaxWidth(90,300).MinMaxHeight(DPI(20), DPI(25));
		};
		metric("Key Metric 1");
		metric("Key Metric 2");
		metric("CTA Button");
		
		// *** critical change: Fit, not Fixed ***
		root.Add(metrics).Fit().MinMaxHeight(DPI(24), INT_MAX);
	
	    // Middle row container (left | main | right)
	    FlowBoxLayout& mid = layouts.Add();
	    mid.SetDirection(FlowBoxLayout::H);
	    mid.SetGap(DPI(6)).SetInset(DPI(2), DPI(2));
	    mid.SetAlignItems(FlowBoxLayout::Stretch);
	
	    // Left sidebar
	    ColorTile& left = tiles.Add();
	    left.SetLabel("Primary List").SetColor(LightBase);
	
	    // Right sidebar
	    ColorTile& right = tiles.Add();
	    right.SetLabel("Secondary Content").SetColor(LightBase);
	
	    // Compose middle row
	    mid.Add(left);
	    mid.Add(right).Expand(2);
	
	    root.Add(mid).Expand();
	
	    // Footer
	    ColorTile& footer = tiles.Add();
	    footer.SetLabel("Footer").SetColor( Blend(base, White(), 80) );
	    root.AddFixed(footer, DPI(36));
    }


    // Toggle debug overlay on nested FlowBoxLayouts
    void SetDebugAll(bool on) {
        root.SetDebug(on);
        for(Ctrl* c = root.GetFirstChild(); c; c = c->GetNext()) {
            if(FlowBoxLayout* f = dynamic_cast<FlowBoxLayout*>(c)) {
                f->SetDebug(on);
                for(Ctrl* k = f->GetFirstChild(); k; k = k->GetNext()) {
                    if(FlowBoxLayout* f2 = dynamic_cast<FlowBoxLayout*>(k)) f2->SetDebug(on);
                }
            }
        }
    }
};

// --------------------------------------------------------------
// Main application window: wrap-based 3×2 showcase 
// --------------------------------------------------------------
class MainWin : public TopWindow {
public:
    typedef MainWin CLASSNAME;
    MainWin() {
        Title("FlowBoxLayout — Showcase (Flow + Wrap)");
        Sizeable().Zoomable();
	    Rect wa = GetWorkArea();
	    Size sz = Size(DPI(1480), DPI(860));
	    SetRect(wa.CenterRect(sz));
        SetMinSize(Size(DPI(400), DPI(600)));
    
    // 1) Configure the main horizontal flow with wrapping
        showcase_flow.SetDirection(FlowBoxLayout::H)
                     .SetWrap(true)                   // ← rows form automatically
                     .SetGap(DPI(12))
                     .SetInset(DPI(12))               // padding around the edge
                     .SetWrapRowsExpand()
                     .SetAlignItems(FlowBoxLayout::Stretch);
        Add(showcase_flow.SizePos());

        // 2) Build the demo panels (same Quadrant builders you already have)
        panel_holy_grail.BuildHolyGrail(Color(0x4C,0xAF,0x50)); // green
        panel_spa       .BuildSPA       (Color(0xF9,0x60,0x30)); // orange
        panel_fpattern  .BuildFPattern  (Color(0x3E,0x80,0xE0)); // indigo/blue
        panel_magazine  .BuildMagazine  (Color(0x21,0x96,0xF3)); // blue
        panel_cardgrid  .BuildCardGrid  (Color(0x79,0x55,0x48)); // brown
        panel_spa_mock  .BuildSPA_Mock  (Color(0x6A,0x5A,0xCD)); // purple

        // 3) Add panels to the flow in a 3×2 “reading order”
        // Row 1: Holy Grail | SPA | F-Pattern
        // Row 2: Magazine   | Card Grid | SPA Mock
        auto add_section = [&](Quadrant& q){
            showcase_flow.Add(q)
                         .Fit()                       // base width for wrap decisions
                         .Expand(1)                   // share leftover width evenly per row
                         .MinMaxWidth (DPI(400), DPI(900))   // wrap threshold
                         .MinMaxHeight(DPI(400), DPI(900));  // keep a decent presence
        };
        add_section(panel_holy_grail);
        add_section(panel_spa);
        add_section(panel_fpattern);
        add_section(panel_magazine);
        add_section(panel_cardgrid);
        add_section(panel_spa_mock);
    }

    // F2 toggles debug overlays everywhere
    virtual bool Key(dword key, int) override {
        if(key == K_F2) {
            debug_on = !debug_on;
            for (Quadrant* q : { &panel_holy_grail, &panel_spa, &panel_fpattern,
                                  &panel_magazine, &panel_cardgrid, &panel_spa_mock })
                q->SetDebugAll(debug_on);
            return true;
        }
        return TopWindow::Key(key, 0);
    }

private:
    bool          debug_on = false;

    // New: single wrap container for the whole showcase
    FlowBoxLayout showcase_flow { FlowBoxLayout::H };

    // Panels (clear, verbose names)
    Quadrant panel_holy_grail;
    Quadrant panel_spa;
    Quadrant panel_fpattern;
    Quadrant panel_magazine;
    Quadrant panel_cardgrid;
    Quadrant panel_spa_mock;
};

GUI_APP_MAIN
{
    MainWin().Run();
}
