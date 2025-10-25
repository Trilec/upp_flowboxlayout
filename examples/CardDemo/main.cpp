#include <CtrlLib/CtrlLib.h>
#include <Painter/Painter.h>
#include <StageCard/StageCard.h>
#include <FlowBoxLayout/FlowBoxLayout.h>

using namespace Upp;

// --------------------------------------------------------------
// ColorTile: simple colored, labeled tile (theme-aware)
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
        return (minsz.cx > 0 || minsz.cy > 0) ? minsz : Size(DPI(100), DPI(36));
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

// --------------------------------------------------------------
// QuadrantCard: StageCard-derived card that builds its own body
// --------------------------------------------------------------
class QuadrantCard : public StageCard {
public:
    typedef QuadrantCard CLASSNAME;

    QuadrantCard() {
        AddContent(body);
        body.Add(root.SizePos());

        SetHeaderAlign(StageCard::LEFT)
            .SetContentInset(DPI(8), DPI(8), DPI(8), DPI(8))
            .SetCardCornerRadius(DPI(12))
            .SetContentCornerRadius(DPI(8))
            .EnableContentScroll(false)
            .EnableContentAutoFill(true);

        root.SetGap(DPI(6)).SetInset(DPI(8), DPI(8));
    }

    // ---- Demo 1: Holy Grail ---------------------------------
    void BuildHolyGrail(Color base) {
        SetTitle("Holy Grail").SetSubTitle("Header + sidebars + footer").SetBadge("▥");
        Color LightBase = Blend(base, White(), 50);

        ColorTile& header = tiles.Add();
        header.SetLabel("Header").SetColor(LightBase);
        root.AddFixed(header, DPI(30));

        FlowBoxLayout& mid = layouts.Add();
        mid.SetDirection(FlowBoxLayout::H);
        mid.SetGap(DPI(6)).SetInset(DPI(2), DPI(2));
        mid.SetAlignItems(FlowBoxLayout::Stretch);

        ColorTile& left = tiles.Add();
        left.SetLabel("Left Sidebar").SetColor(LightBase);

        FlowBoxLayout& main = layouts.Add();
        main.SetDirection(FlowBoxLayout::H)
            .SetWrap(true)
            .SetGap(DPI(2))
            .SetAlignItems(FlowBoxLayout::Start);

        for (int i = 0; i < 48; ++i) {
            ColorTile& tiny = tiles.Add();
            tiny.SetLabel("")
                .SetColor((i & 1) ? base : LightBase)
                .SetMin(Size(DPI(20), DPI(20)));
            main.AddFit(tiny);
        }

        ColorTile& right = tiles.Add();
        right.SetLabel("Right Sidebar").SetColor(LightBase);

        mid.AddFixed(left, DPI(140)).MinMaxHeight(DPI(80), INT_MAX);
        mid.Add(main).Expand().MinMaxHeight(DPI(80), INT_MAX);
        mid.AddFixed(right, DPI(140)).MinMaxHeight(DPI(80), INT_MAX);

        root.Add(mid).Expand(1);

        ColorTile& footer = tiles.Add();
        footer.SetLabel("Footer").SetColor( Blend(base, White(), 80) );
        root.AddFixed(footer, DPI(30));
    }

    // ---- Demo 2: Magazine -----------------------------------
    void BuildMagazine(Color base) {
        SetTitle("Magazine").SetSubTitle("Hero + mixed grid").SetBadge("▥");
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
            rightcol.AddFixed(s, DPI(30));
        }

        grid.Add(leftcol).Expand();
        grid.Add(rightcol).Expand();

        root.Add(grid).Expand(1);

        ColorTile& foot = tiles.Add();
        foot.SetLabel("Footer").SetColor(LightBase);
        root.AddFixed(foot, DPI(30));
    }

    // ---- Demo 3: SPA ----------------------------------------
    void BuildSplitScreen(Color base) {
        SetTitle("Split Screen Layout").SetSubTitle("Sections in column").SetBadge("▥");
        Color LightBase = Blend(base, White(), 50);

        ColorTile& hdr = tiles.Add();
        hdr.SetLabel("Shared Header").SetColor(base);
        root.AddFixed(hdr, DPI(30));

        FlowBoxLayout& middle = layouts.Add().SetDirection(FlowBoxLayout::H);

        ColorTile& leftPanel = tiles.Add().SetLabel("Left Panel").SetColor(LightBase);
        ColorTile& rightPanel = tiles.Add().SetLabel("Right Panel").SetColor(LightBase);
        
        middle.Add(leftPanel).Expand();
        middle.Add(rightPanel).Expand();

        root.Add(middle);

        ColorTile& foot = tiles.Add().SetLabel("Footer").SetColor(base);
        root.AddFixed(foot, DPI(30));
    }

    // ---- Demo 4: Card Grid ----------------------------------
    void BuildCardGrid(Color base) {
        SetTitle("Card Grid Layout").SetSubTitle("Wrapping cards, with column alignment").SetBadge("▥");
        Color LightBase = Blend(base, White(), 70);

        ColorTile& header = tiles.Add();
        header.SetLabel("Header").SetColor(base);
        root.AddFixed(header, DPI(30));

        FlowBoxLayout& mid = layouts.Add();
        mid.SetDirection(FlowBoxLayout::H)
           .SetWrap(true)
           .SetGap(DPI(6))
           .SetInset(DPI(6), DPI(6))
           .SetAlignItems(FlowBoxLayout::Stretch)
           .SetFixedColumn(DPI(160));

        auto bucket_h = [&](int i)->Size {
            switch(i % 4){
                case 0: return Size(DPI(10),  DPI(30));
                case 1: return Size(DPI(20),  DPI(50));
                case 2: return Size(DPI(30),  DPI(80));
                default:return Size(DPI(50),  DPI(190));
            }
        };

        for(int i = 1; i <= 10; i++) {
            if(i == 5 || i == 7) mid.AddSpacer(1);
            if(i == 3)           mid.AddBreak();

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
        root.AddFixed(footer, DPI(30));
    }

    // ---- Demo 5: SPA (cards column variant) -----------------
    void BuildSPALayout(Color base) {
        SetTitle("SPA Layout").SetSubTitle("Cards column").SetBadge("▥");
        Color LightBase = Blend(base, White(), 60);

        ColorTile& nav = tiles.Add();
        nav.SetLabel("Navigation").SetColor(LightBase);
        root.AddFixed(nav, DPI(30));

        FlowBoxLayout& cards = layouts.Add();
        cards.SetDirection(FlowBoxLayout::V)
             .SetGap(DPI(12))
             .SetInset(DPI(10), DPI(10))
             .SetAlignItems(FlowBoxLayout::Stretch);

        auto card = [&](const char* t){
            ColorTile& c = tiles.Add();
            c.SetLabel(t).SetColor(LightBase);
            cards.Add(c).Expand(1);
        };
        card("Home");
        card("Profile");
        card("Settings");

        root.Add(cards).Expand();
    }

    // ---- Demo 6: F-Pattern ----------------------------------
    void BuildFPattern(Color base) {
        SetTitle("F Pattern Layout").SetSubTitle("Metrics + two columns").SetBadge("▥");
        Color LightBase = Blend(base, White(), 60);

        ColorTile& header = tiles.Add();
        header.SetLabel("Header").SetColor(LightBase);
        root.AddFixed(header, DPI(30));
        
        FlowBoxLayout& metrics = layouts.Add();
        metrics.SetDirection(FlowBoxLayout::H)
               .SetWrap(true)
               .SetGap(DPI(4))
               .SetInset(0,0)
               .SetAlignItems(FlowBoxLayout::Stretch)
               .SetFixedRow(DPI(24))
               .SetWrapAutoResize();

        auto metric = [&](const char* t){
            ColorTile& m = tiles.Add();
            m.SetLabel(t).SetColor(LightBase);
            metrics.Add(m).MinMaxWidth(90,300).MinMaxHeight(DPI(20), DPI(25));
        };
        metric("Key Metric 1");
        metric("Key Metric 2");
        metric("CTA Button");

        // Fit to natural height (one line), but allow multiple lines if constrained
        root.Add(metrics).Fit().MinMaxHeight(DPI(24), INT_MAX);

        FlowBoxLayout& mid = layouts.Add();
        mid.SetDirection(FlowBoxLayout::H);
        mid.SetGap(DPI(6)).SetInset(DPI(2), DPI(2));
        mid.SetAlignItems(FlowBoxLayout::Stretch);

        ColorTile& left = tiles.Add();
        left.SetLabel("Primary List").SetColor(LightBase);
        ColorTile& right = tiles.Add();
        right.SetLabel("Secondary Content").SetColor(LightBase);

        mid.Add(left);
        mid.Add(right).Expand(2);

        root.Add(mid).Expand();

        ColorTile& footer = tiles.Add();
        footer.SetLabel("Footer").SetColor( Blend(base, White(), 80) );
        root.AddFixed(footer, DPI(30));
    }

private:
    ParentCtrl     body;
    FlowBoxLayout  root { FlowBoxLayout::V };

    Array<ColorTile>     tiles;
    Array<FlowBoxLayout> layouts;
};


// --------------------------------------------------------------
// Card-wrapped showcase using StageCard and FlowBoxLayout
// --------------------------------------------------------------
class CardDemoApp : public TopWindow {
public:
    typedef CardDemoApp CLASSNAME;

    StageCard     outer;        // overall card with header + scroll
    FlowBoxLayout showcase { FlowBoxLayout::H };

    Array<QuadrantCard> cards;  // keep cards in an Array per style guide

    CardDemoApp() {
        Title("CardDemo — FlowBoxLayout showcase");
        Sizeable().Zoomable().CenterOwner();
	    Rect wa = GetWorkArea();
	    Size sz = Size(DPI(1200), DPI(850));
	    SetRect(wa.CenterRect(sz));
        SetMinSize(Size(DPI(400), DPI(400)));
        
        Add(outer.SizePos());
        BuildOuter();
        BuildQuadrants();
    }

    void BuildOuter() {
        outer
            .SetTitle("FlowBoxLayout Showcase")
            .SetSubTitle("Card-wrapped quadrants with responsive grid")
            .SetBadge("💳")
            .SetBadgeFont( StdFont().Height(DPI(22)) )
            .SetTitleFont( StdFont().Bold().Height(DPI(22)) )
            .SetHeaderAlign(StageCard::LEFT)
            .EnableCardFrame(false)
            .SetContentCornerRadius(DPI(10))
            .SetContentInset(DPI(12), DPI(12), DPI(12), DPI(12))
            .EnableContentScroll(true)
            .EnableContentAutoFill(true);

        showcase
            .SetWrap(true)
            .SetWrapAutoResize(true)
            .SetWrapRowsExpand()
            .SetGap(DPI(12))
            .SetInset(DPI(2), DPI(2))
            .SetAlignItems(FlowBoxLayout::Stretch);


        outer.AddContent(showcase);
    }

    void BuildQuadrants() {
        const Color A = Color( 22, 86,160);
        const Color B = Color( 20,120, 84);
        const Color C = Color(144, 88,162);
        const Color D = Color(206,120, 40);
        const Color E = Color( 62,128,224);
        const Color F = Color(249, 96, 48);

        cards.SetCount(6);
        cards[0].BuildHolyGrail(A);
        cards[1].BuildMagazine (B);
        cards[2].BuildSplitScreen(C);
        cards[3].BuildCardGrid (D);
        cards[4].BuildSPALayout (E);
        cards[5].BuildFPattern (F);

        for(int i = 0; i < cards.GetCount(); ++i)
            showcase.Add(cards[i]).Expand().MinMaxHeight(DPI(300), INT_MAX).MinMaxWidth(300,900);
    }
};

GUI_APP_MAIN
{
    CardDemoApp().Run();
}
