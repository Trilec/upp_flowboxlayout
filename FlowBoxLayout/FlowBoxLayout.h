#ifndef _FlowBoxLayout_h_
#define _FlowBoxLayout_h_

// -----------------------------------------------------------------------------
// FlowBoxLayout
//
// A small, dependency-free layout control for U++ that arranges children in
// a **flow**: either left→right (H) or top→bottom (V). It is designed to be:
//   • Simple to use (tiny API, value semantics for item settings)
//   • Predictable (explicit sizing modes per item: Fixed / Fit / Expand)
//   • Flexible (optional wrapping in Horizontal mode; min/max caps; per-item
//     cross-axis alignment; spacers and hard breaks)
//   • Fast (keeps a tiny min-size cache; does not allocate per-layout pass)
//
// Key concepts
// ============
// • Direction
//     H: items are laid out from left to right; optional wrapping creates rows.
//     V: items are stacked top to bottom; wrapping is ignored; think "column".
//
// • Item sizing modes (main axis):
//     Fixed(px)   – use exactly px on the main axis (never grows/shrinks)
//     Fit()       – use the child’s *minimum* size on the main axis
//     Expand(w)   – share the remaining space in proportion to weight w
//
// • Cross-axis alignment (secondary axis):
//     Container default via SetAlignItems(Align), overridable per item with
//     ItemRef::AlignSelf(Align). Stretch means the item fills the cross-axis.
//
// • Global caps / grids
//     SetFixedColumn(px) – in H mode, cap each item’s width to px (wrapping
//                          respects this, yielding a “fixed column” look)
//     SetFixedRow(px)    – in V mode, cap each item’s height to px
//
// • Spacing
//     SetInset(...) – inner padding of the container
//     SetGap(px)    – space between neighboring items (applies both axes)
//
// • Wrapping helpers (Horizontal mode)
//     SetWrap(true)           – enable row wrapping
//     SetWrapAutoResize(true) – report natural height as a function of width
//                               (parents can query via GetMinSize/Measure…)
//     SetWrapRowsExpand(true) – when the container has extra height, *rows*
//                               grow to consume it (useful in card grids)
//
// • Debug
//     SetDebug(true) – draws an overlay for inset, rows/columns, and item rects
//
// Typical usage
// =============
//     FlowBoxLayout fb(FlowBoxLayout::H);
//     fb.SetWrap(true).SetFixedColumn(DPI(180)).SetGap(DPI(8)).SetInset(DPI(8));
//     fb.Add(myTileA).Fit();
//     fb.Add(myTileB).Expand(2).MinMaxHeight(DPI(80), INT_MAX);
//     fb.AddBreak(); // new wrapped row
//     fb.Add(myTileC).Fixed(DPI(120)).AlignSelf(FlowBoxLayout::Center);
//
// -----------------------------------------------------------------------------

#include <CtrlLib/CtrlLib.h>
#include <limits.h>

namespace Upp {

class FlowBoxLayout : public ParentCtrl {
public:
    typedef FlowBoxLayout CLASSNAME;

    // Primary direction of the flow. H enables optional wrapping; V stacks.
    enum Direction { H, V };

    // Cross-axis alignment (secondary axis), both as a container default
    // and per-item override via ItemRef::AlignSelf(...).
    enum Align     { Auto,        // use container default (do not override)
                     Stretch,     // fill cross-axis
                     Start,       // align to start (top for H, left for V)
                     Center,      // center on the cross-axis
                     End };       // align to end (bottom for H, right for V)

    // -------------------------------------------------------------------------
    // Item
    //
    // Internal storage of one child’s layout state. Public facing API mutates
    // these fields via ItemRef methods (Fixed / Fit / Expand / MinMax…).
    // -------------------------------------------------------------------------
    struct Item : Moveable<Item> {
        // --- Persistent API-facing state (sticks across passes) ---------------
        Ctrl*  c               = nullptr;     // the child (nullptr => spacer/break)
        int    fixed           = -1;          // >=0 => Fixed(px) on main axis
        int    expandingWeight = 0;           // >0  => Expand(weight)
        bool   fit             = false;       // true => Fit() on main axis
        bool   is_break        = false;       // true => AddBreak semantics
        int    minw            = -1;          // main-axis MIN cap  (if set >=0)
        int    maxw            = 2048;        // main-axis MAX cap  (if set >=0)
        int    minh            = -1;          // cross-axis MIN cap (if set >=0)
        int    maxh            = INT_MAX;     // cross-axis MAX cap (if set >=0)
        Align  align_self      = Align::Auto; // per-item cross-axis alignment

        // --- Persistent min-size cache (survives resizes/layouts) -------------
        Size   cachedMinSize   = Size(0,0);   // child’s cached GetMinSize
        int    ms_epoch        = 0;           // last epoch when cache updated
        bool   ms_valid        = false;       // quick guard for cache use

        // --- Transient per-pass layout cache (reset each PreLayoutCalc) -------
        struct TransientLayoutCache : Moveable<TransientLayoutCache> {
            bool visible   = false;  // participates in this pass
            bool spacer    = false;  // explicit spacer (AddSpacer)
            bool breakMark = false;  // explicit hard wrap (AddBreak)
            int  rowOrCol  = -1;     // row index (H) or position (V)
            Rect cell;               // cell rect (before cross-axis align)
            Rect content;            // final rect assigned to Ctrl
        } cl;

        Item() {}
        Item(Ctrl& ctrl) : c(&ctrl) {}
    };

    // -------------------------------------------------------------------------
    // ItemRef
    //
    // A tiny fluent handle returned by Add/AddFixed/... that lets you tune the
    // last inserted item (sizing mode, caps, alignment). Each call marks the
    // layout “dirty” and triggers a Layout unless you paused it.
    // -------------------------------------------------------------------------
    class ItemRef {
    public:
        ItemRef(FlowBoxLayout* owner, int idx) : owner(owner), index(idx) {}

        // Use the remaining space on the main axis. 'w' is a relative weight.
        // Example: A.Expand(1), B.Expand(2) -> B gets ~2× A’s share.
        ItemRef& Expand(int w=1) {
            if(ok()) owner->items[index].expandingWeight = max(1, w);
            if(owner) { owner->cur_gen++; if(owner->layout_pause == 0) owner->Layout(); }
            return *this;
        }

        // Take exactly 'px' on the main axis (never expands/shrinks).
        ItemRef& Fixed(int px) {
            if(ok()) {
                auto& it = owner->items[index];
                it.fixed = max(0, px);
                it.expandingWeight = 0;
                it.fit = false;
            }
            if(owner) { owner->cur_gen++; if(owner->layout_pause == 0) owner->Layout(); }
            return *this;
        }

        // Use the child’s logical minimum size on the main axis.
        // Good for labels/buttons/tiles that should not stretch.
        ItemRef& Fit() {
            if(ok()) {
                auto& it = owner->items[index];
                it.fit = true;
                it.fixed = -1;
                it.expandingWeight = 0;
            }
            if(owner) { owner->cur_gen++; if(owner->layout_pause == 0) owner->Layout(); }
            return *this;
        }

        // Hard caps (apply after the base main/cross size is chosen).
        // Use this to keep tiles/cards inside a fixed grid.
        ItemRef& MinMaxWidth(int minw = -1, int maxw = 2048) {
            if(ok()) { auto& it = owner->items[index]; it.minw = minw; it.maxw = maxw; }
            if(owner) { owner->cur_gen++; if(owner->layout_pause == 0) owner->Layout(); }
            return *this;
        }
        ItemRef& MinMaxHeight(int minh = -1, int maxh = INT_MAX) {
            if(ok()) { auto& it = owner->items[index]; it.minh = minh; it.maxh = maxh; }
            if(owner) { owner->cur_gen++; if(owner->layout_pause == 0) owner->Layout(); }
            return *this;
        }

        // Override container cross-axis alignment for this item only.
        ItemRef& AlignSelf(Align a) {
            if(ok()) owner->items[index].align_self = a;
            if(owner) { owner->cur_gen++; if(owner->layout_pause == 0) owner->Layout(); }
            return *this;
        }

    private:
        bool ok() const { return owner && index >= 0 && index < owner->items.GetCount(); }
        FlowBoxLayout* owner = nullptr;
        int            index = -1;
    };

    // Create a layout in a given direction. Starts transparent by default.
    FlowBoxLayout(Direction d = V) : dir(d) { Transparent(); }
    virtual ~FlowBoxLayout() {}

    // -------------------------------------------------------------------------
    // Container configuration (why/when to use each)
    // -------------------------------------------------------------------------

    // Change primary flow direction at runtime.
    // Use H for galleries/toolbars; V for stacked forms/sidebars.
    FlowBoxLayout& SetDirection(Direction d) {
        dir = d; ++cur_gen; if(layout_pause==0) Layout(); return *this;
    }

    // Set space between neighboring items (both axes). Great for card gutters.
    FlowBoxLayout& SetGap(int px) {
        gap = max(0, px); ++cur_gen; if(layout_pause==0) Layout(); return *this;
    }

    // Set inner padding (inset) of the container – single value (all sides).
    FlowBoxLayout& SetInset(int wh) {
        inset = Rect(wh, wh, wh, wh); ++cur_gen; if(layout_pause==0) Layout(); return *this;
    }
    // Set symmetric horizontal/vertical padding.
    FlowBoxLayout& SetInset(int w, int h) {
        inset = Rect(w, h, w, h); ++cur_gen; if(layout_pause==0) Layout(); return *this;
    }
    // Set per-edge padding (l, t, r, b). Use for asymmetric layouts.
    FlowBoxLayout& SetInset(int l, int t, int r, int b) {
        inset = Rect(l, t, r, b); ++cur_gen; if(layout_pause==0) Layout(); return *this;
    }

    // Enable line wrapping (H mode). This turns the flow into multiple rows,
    // respecting fixed/fitted widths and fixed-column caps. Ignored in V mode.
    FlowBoxLayout& SetWrap(bool on = true) {
        wrap = on; ++cur_gen; if(layout_pause==0) Layout(); return *this;
    }

    // Report a meaningful natural height for a given width (H+wrap):
    // when on, parents that query min-size/MeasureHeightForWidth get a height
    // that accounts for how many rows are needed at that width. Helpful when
    // the parent wants to decide whether to add a scrollbar.
    FlowBoxLayout& SetWrapAutoResize(bool on = true) {
        wrap_auto_resize = on; return *this;
    }

    // When the container gets more vertical room than needed (H+wrap), grow the
    // **rows** to consume it (nice for dashboards where rows should “breathe”).
    // Turn this OFF if you prefer the parent to scroll instead.
    FlowBoxLayout& SetWrapRowsExpand(bool on = true) {
        wrap_rows_expand = on; ++cur_gen; if(layout_pause==0) Layout(); return *this;
    }

    // Cross-axis alignment default for all items that do not override it.
    // Stretch is good for tile/card grids; Start/Center/End for compact toolbars.
    FlowBoxLayout& SetAlignItems(Align a) {
        align_items = a; ++cur_gen; if(layout_pause==0) Layout(); return *this;
    }

    // HARD width cap for *every* non-break item (H mode). Great for building a
    // masonry/“card column” style where each cell is a fixed column width and
    // rows wrap naturally. Set to -1 to disable.
    FlowBoxLayout& SetFixedColumn(int px) {
        fixed_column = (px >= 0 ? px : -1); ++cur_gen; if(layout_pause==0) Layout(); return *this;
    }

    // HARD height cap for *every* item (V mode). Useful for list rows of a
    // uniform height. Set to -1 to disable.
    FlowBoxLayout& SetFixedRow(int px) {
        fixed_row    = (px >= 0 ? px : -1); ++cur_gen; if(layout_pause==0) Layout(); return *this;
    }

    // Toggle the debug overlay (draws inset, gaps, rows/cells). Handy during
    // integration to see the effective boxes without instrumenting code.
    FlowBoxLayout& SetDebug(bool on = true) {
        debug = on; Refresh(); return *this;
    }

    // -------------------------------------------------------------------------
    // Children (insertion helpers)
    // -------------------------------------------------------------------------

    // Add a child with default Expand(1) behavior on the main axis.
    ItemRef Add(Ctrl& c) {
        ParentCtrl::Add(c); Item it(c); it.expandingWeight = 1; items.Add(it);
        ++cur_gen; if(layout_pause==0) Layout(); return ItemRef(this, items.GetCount()-1);
    }

    // Add a child with explicit Expand(weight).
    ItemRef AddExpand(Ctrl& c, int w=1) {
        ParentCtrl::Add(c); Item it(c); it.expandingWeight = max(1,w); items.Add(it);
        ++cur_gen; if(layout_pause==0) Layout(); return ItemRef(this, items.GetCount()-1);
    }

    // Add a child with Fixed(px).
    ItemRef AddFixed(Ctrl& c, int px) {
        ParentCtrl::Add(c); Item it(c); it.fixed = max(0,px); items.Add(it);
        ++cur_gen; if(layout_pause==0) Layout(); return ItemRef(this, items.GetCount()-1);
    }

    // Add a child with Fit() on the main axis.
    ItemRef AddFit(Ctrl& c) {
        ParentCtrl::Add(c); Item it(c); it.fit = true; items.Add(it);
        ++cur_gen; if(layout_pause==0) Layout(); return ItemRef(this, items.GetCount()-1);
    }

    // Add an *expanding spacer* (no child). In H without wrap, acts like a
    // fluid expander; with fixed-column/wrap, it occupies one cell.
    ItemRef AddSpacer(int weight = 1) {
        Item it; it.c = nullptr; it.is_break = false;
        it.expandingWeight = max(1, weight);
        items.Add(it);
        ++cur_gen; if(layout_pause==0) Layout();
        return ItemRef(this, items.GetCount() - 1);
    }

    // Add a *hard break*.
    //  • wrap ON  (H): forces a new row; spacer weight is ignored.
    //  • wrap OFF (H): inserts a flexible gap (like an expander with given weight)
    //  • V mode:   treated as a vertical spacer in the stack.
    ItemRef AddBreak(int spacer_expandingWeight = 1) {
        Item it;
        it.is_break = true;
        it.expandingWeight = max(1, spacer_expandingWeight); // used only when wrap==false
        items.Add(it);
        ++cur_gen; if(layout_pause==0) Layout();
        return ItemRef(this, items.GetCount() - 1);
    }

    // -------------------------------------------------------------------------
    // Batch edits / throttling
    // -------------------------------------------------------------------------

    // Temporarily suspend auto-Layout (nestable). Useful when inserting many
    // items in a loop; call ResumeLayout(true) once at the end.
    FlowBoxLayout& PauseLayout() { ++layout_pause; return *this; }

    // Resume auto-Layout; optionally trigger a relayout immediately.
    FlowBoxLayout& ResumeLayout(bool relayout = true) {
        if(layout_pause > 0) --layout_pause;
        if(layout_pause == 0 && relayout) Layout();
        return *this;
    }

    // RAII helper for Pause/Resume.
    struct PauseScope {
        FlowBoxLayout& L;
        bool relayout;
        PauseScope(FlowBoxLayout& l, bool r=true) : L(l), relayout(r) { L.PauseLayout(); }
        ~PauseScope() { L.ResumeLayout(relayout); }
    };

    // Remove all items/children and reset internal book-keeping.
    FlowBoxLayout& ClearItems();

    // -------------------------------------------------------------------------
    // Introspection / diagnostics
    // -------------------------------------------------------------------------

    // Total used size after the last Layout (excludes inset). Handy to compute
    // whether to enable a scrollbar in a parent.
    int  GetUsedWidth()  const { return used_w; }
    int  GetUsedHeight() const { return used_h; }

    // Human-readable summary (direction, wrap, counts, etc.).
    String ToString() const;

    // -------------------------------------------------------------------------
    // ParentCtrl overrides
    // -------------------------------------------------------------------------
    virtual void Layout() override;                 // perform layout pass
    virtual Size GetMinSize() const override;       // conservative natural size
    virtual void Paint(Draw& w) override;           // draws debug overlay when enabled

    // Min-size cache invalidation (call when a child’s intrinsic min size changes)
    void InvalidateMinSize(Ctrl& c);
    void InvalidateAllMinSizes();

private:
    // Predicate: true if the item participates this pass (shown OR a break).
    static inline bool IsItemVisible(const Item& it) {
        return (it.c && it.c->IsShown()) || it.is_break;
    }

    // Clamp helper that respects “unset” (-1) semantics on min/max.
    static int ClampWith(int minv, int maxv, int v) {
        if(minv >= 0) v = max(v, minv);
        if(maxv >= 0) v = min(v, maxv);
        return v;
    }

    // Compute main-axis base size from the chosen mode.
    static int basePrimary(const Item& it, const Size& ms, bool vertical) {
        if(it.fixed >= 0) return it.fixed;
        if(it.fit)       return vertical ? ms.cy : ms.cx;
        return vertical ? ms.cy : ms.cx; // default to min-size on main axis
    }

    // -------------------------------------------------------------------------
    // Implementation pipeline
    // -------------------------------------------------------------------------
    void PreLayoutCalc(const Rect& inner_rc);
    void LayoutHorizontal(const Rect& irc, int inner_w, int inner_h, int visible_semantic);
    void LayoutVertical  (const Rect& irc, int inner_w, int inner_h, int visible_semantic);

    void PostLayoutCommit();
    void DebugPaint(Draw& w, const Rect& inner_rc) const;

    // Helper for parents: compute natural height for a given width (respects
    // wrapping and fixed columns). Used when SetWrapAutoResize(true).
    int MeasureHeightForWidth(int width);

    // Central helper to fetch (and cache) a child’s min size.
    inline Size GetCtrlMinSize(Item& it);

private:
    // Items in visual order.
    Vector<Item> items;

    // Container configuration
    Direction    dir   = V;
    int          gap   = 0;
    Rect         inset = Rect(0,0,0,0);
    bool         wrap  = false;
    bool         wrap_auto_resize = false;
    bool         wrap_rows_expand = false;

    // Layout results
    int          used_w = 0, used_h = 0;

    // Throttling
    int          layout_pause = 0;

    // Min-size cache epoching
    int          minsize_epoch = 1;

    // Planning guards (avoid redundant work)
    Size         plan_inner = Size(0,0);
    int          plan_gen   = 0;
    int          cur_gen    = 0;

    // Cross-axis default
    Align        align_items = Align::Stretch;

    // Global caps (container-wide)
    int          fixed_column = -1; // H: cap width of all non-break items
    int          fixed_row    = -1; // V: cap height of all non-break items

    // Debug overlay flag
    bool  debug = false;
};

} // namespace Upp

#endif // _FlowBoxLayout_h_
