#include "FlowBoxLayout.h"

namespace Upp {


FlowBoxLayout& FlowBoxLayout::ClearItems() {
    for(Ctrl *q = GetFirstChild(); q; ) {
        Ctrl* next = q->GetNext();
        q->Remove();
        q = next;
    }
    items.Clear();
    used_w = used_h = 0;
    Layout();
    return *this;
}

void FlowBoxLayout::Layout() {
    if(layout_pause > 0) return;       // ← short-circuit when paused
    Rect rc = GetSize();
    if(rc.IsEmpty()) { used_w = used_h = 0; return; }

    Rect irc = rc;
    irc.left   += inset.left;
    irc.top    += inset.top;
    irc.right  -= inset.right;
    irc.bottom -= inset.bottom;
    if(irc.IsEmpty()) { used_w = used_h = 0; return; }

    if(plan_inner != irc.GetSize() || plan_gen != cur_gen)
        PreLayoutCalc(irc);

    PostLayoutCommit();

    if(debug) Refresh();
}

void FlowBoxLayout::PostLayoutCommit() {
    for(Item& it : items) {
        if(!it.cl.visible) continue;
        if(it.c)
            it.c->SetRect(it.cl.content);
    }
}

void FlowBoxLayout::PreLayoutCalc(const Rect& irc) {
    plan_inner = irc.GetSize();

    // reset transient cache
    for(Item& it : items)
        it.cl = Item::TransientLayoutCache{};

    used_w = used_h = 0;

    const int inner_w = max(0, irc.GetWidth());
    const int inner_h = max(0, irc.GetHeight());

    // mark visible
    int visible_semantic = 0;
    for(Item& it : items) {
        const bool semantic = it.is_break || (!it.c && !it.is_break); // break or spacer
        const bool vis_ctrl = it.c && it.c->IsShown();
        if(!(vis_ctrl || semantic)) continue;
        it.cl.visible = true;
        it.cl.spacer  = (!it.c && !it.is_break);
        ++visible_semantic;
    }

    if(visible_semantic == 0) {
        used_w = used_h = 0;
        plan_gen = cur_gen;
        return;
    }

    if(dir == H)
        LayoutHorizontal(irc, inner_w, inner_h, visible_semantic);
    else
        LayoutVertical  (irc, inner_w, inner_h, visible_semantic);

    plan_gen = cur_gen;
}


void FlowBoxLayout::LayoutHorizontal(const Rect& irc, int inner_w, int inner_h, int /*visible_semantic*/) {
    // Local structs stay in .cpp (no header bloat)
    struct RowCell {
        int   idx   = -1;
        bool  is_ctrl = false;
        int   w     = 0;
        int   base_h= 0;
        int   hmin  = -1, hmax = INT_MAX;
        Align self_align = Align::Auto;
    };
    struct GapExp { int idx; int weight; int minw; };

    auto eff_align = [&](const Item& it)->Align {
        return (it.align_self != Align::Auto) ? it.align_self : align_items;
    };

    Vector<Vector<RowCell>> rows;
    Vector<Vector<GapExp>>  row_gap_exp;
    rows.Reserve(max(1, items.GetCount() / 8));
    row_gap_exp.Reserve(max(1, items.GetCount() / 8));
    rows.Add(); row_gap_exp.Add();

    int x_row  = irc.left;
    int placed = 0;

    auto new_row = [&](){
        rows.Add();
        row_gap_exp.Add();
        x_row  = irc.left;
        placed = 0;
    };

    // PASS 1: build rows (width base)
    for(int i = 0; i < items.GetCount(); ++i) {
        Item& it = items[i];
        if(!it.cl.visible) continue;

        // wrap + break = newline marker
        if(wrap && it.is_break) {
            it.cl.breakMark = true;
            it.cl.rowOrCol  = rows.GetCount() - 1;
            if(rows.Top().GetCount() > 0 || row_gap_exp.Top().GetCount() > 0)
                new_row();
            continue;
        }

        // fixed-column mode
        if(fixed_column >= 0) {
            const int cell_w = fixed_column;

            if(!wrap && it.is_break) {
                RowCell rc; rc.idx=i; rc.is_ctrl=false; rc.w=cell_w; rc.hmin=it.minh; rc.hmax=it.maxh; rc.base_h=0;
                if(placed > 0) x_row += gap;
                it.cl.rowOrCol = rows.GetCount() - 1;
                rows.Top().Add(rc);
                x_row += cell_w; ++placed;
                continue;
            }
            if(it.cl.spacer) {
                if(wrap) {
                    int need = (placed == 0 ? cell_w : (x_row - irc.left) + gap + cell_w);
                    if(need > inner_w && (rows.Top().GetCount() > 0 || row_gap_exp.Top().GetCount() > 0))
                        new_row();
                }
                RowCell rc; rc.idx=i; rc.is_ctrl=false; rc.w=cell_w; rc.hmin=it.minh; rc.hmax=it.maxh; rc.base_h=0;
                if(placed > 0) x_row += gap;
                it.cl.rowOrCol = rows.GetCount() - 1;
                rows.Top().Add(rc);
                x_row += cell_w; ++placed;
                continue;
            }

            const Size ms = GetCtrlMinSize(it);
            if(wrap) {
                int need = (placed==0 ? cell_w : (x_row - irc.left) + gap + cell_w);
                if(need > inner_w && (rows.Top().GetCount() > 0 || row_gap_exp.Top().GetCount() > 0))
                    new_row();
            }
            RowCell rc; rc.idx=i; rc.is_ctrl=true; rc.w=cell_w; rc.base_h=ms.cy; rc.hmin=it.minh; rc.hmax=it.maxh; rc.self_align=it.align_self;
            if(placed > 0) x_row += gap;
            it.cl.rowOrCol = rows.GetCount() - 1;
            rows.Top().Add(rc);
            x_row += cell_w; ++placed;
            continue;
        }

        // fluid mode
        if(!wrap && it.is_break) {
            row_gap_exp.Top().Add(GapExp{ i, 1, max(0, gap) });
            it.cl.rowOrCol = rows.GetCount() - 1;
            continue;
        }
        if(it.cl.spacer) {
            row_gap_exp.Top().Add(GapExp{ i, max(1, it.expandingWeight), 0 });
            it.cl.rowOrCol = rows.GetCount() - 1;
            continue;
        }

        const Size ms = GetCtrlMinSize(it);
        int base_w;
        if(it.fixed >= 0)             base_w = it.fixed;
        else if(it.fit) {
            base_w = ms.cx;

            // width-for-height for V child that wraps & auto-resizes
            if(it.c) {
                if(FlowBoxLayout* fb = dynamic_cast<FlowBoxLayout*>(it.c)) {
                    if(fb->dir == V && fb->wrap && fb->wrap_auto_resize) {
                        const int child_inner_h = max(0, inner_h - fb->inset.top - fb->inset.bottom);
                        fb->PreLayoutCalc(RectC(0, 0, INT_MAX, child_inner_h));
                        const int wantw = fb->used_w + fb->inset.left + fb->inset.right;
                        base_w = max(base_w, wantw);
                    }
                }
            }
        }
        else if(it.expandingWeight>0) base_w = 0;
        else                          base_w = ms.cx;

        base_w = ClampWith(it.minw, it.maxw, base_w);

        int candidate = (placed == 0 ? base_w : (x_row - irc.left) + gap + base_w);
        if(wrap && base_w > 0) {
            if(candidate > inner_w && (rows.Top().GetCount() > 0 || row_gap_exp.Top().GetCount() > 0)) {
                new_row();
                candidate = base_w;
            }
        }

        RowCell rc; rc.idx=i; rc.is_ctrl=true; rc.w=base_w; rc.base_h=ms.cy; rc.hmin=it.minh; rc.hmax=it.maxh; rc.self_align=it.align_self;
        if(placed > 0) x_row += gap;
        it.cl.rowOrCol = rows.GetCount() - 1;
        rows.Top().Add(rc);
        x_row += base_w; ++placed;
    }

    // PASS 2A: base row heights
    Vector<int> row_h_base;
    row_h_base.SetCount(rows.GetCount());
    for(int r = 0; r < rows.GetCount(); ++r) {
        const auto& R = rows[r];
        int row_h = 0;
        for(const RowCell& rc : R) {
            int ch = ClampWith(rc.hmin, rc.hmax, rc.base_h);
            row_h = max(row_h, ch);
        }
        if(fixed_row >= 0) row_h = fixed_row;
        if(!wrap && (align_items == Align::Stretch || align_items == Align::Auto))
            row_h = inner_h;
        row_h_base[r] = row_h;
    }

    // PASS 2B: optionally distribute extra height across wrapped rows
    Vector<int> row_h_final;
    row_h_final <<= row_h_base; // deep copy (U++ idiom)
    const bool measuring = inner_h > 100000000; // treat huge heights as probes
    
    // reuse the existing switch: auto-resize implies “wrap rows expand”
    if(wrap && wrap_rows_expand && !measuring && rows.GetCount() > 0) {

        int base_total = 0;
        for(int r = 0; r < rows.GetCount(); ++r) base_total += row_h_base[r];
        base_total += max(0, rows.GetCount() - 1) * gap;

        int extra = max(0, inner_h - base_total);
        if(extra > 0) {
            int each = extra / rows.GetCount();
            int rem  = extra % rows.GetCount();
            for(int r = 0; r < rows.GetCount(); ++r)
                row_h_final[r] += each + (r < rem ? 1 : 0);
        }
    }

    // PASS 2C: expand widths within row + place cells
    used_w = used_h = 0;
    int y = irc.top;

    for(int r = 0; r < rows.GetCount(); ++r) {
        auto& R  = rows[r];
        auto& GE = row_gap_exp[r];
        const int row_h = row_h_final[r];

        // provisional width
        int sum_w = 0, cell_count = 0;
        for(const RowCell& rc : R) { if(cell_count++>0) sum_w += gap; sum_w += rc.w; }
        for(const auto& g : GE)    { sum_w += gap; sum_w += g.minw; }

        int remainder = max(0, inner_w - sum_w);

        // distribute width
        if(fixed_column < 0 && remainder > 0) {
            int total_w = 0;
            Vector<int> exp_idx; exp_idx.Reserve(R.GetCount());
            for(int i = 0; i < R.GetCount(); ++i) {
                const RowCell& rc = R[i];
                const Item& it = items[rc.idx];
                if(it.expandingWeight > 0) { total_w += max(1, it.expandingWeight); exp_idx.Add(i); }
            }
            for(const auto& g : GE) total_w += g.weight;

            if(total_w > 0) {
                int rem = remainder;
                // controls first
                for(int k = 0; k < exp_idx.GetCount() && rem > 0; ++k) {
                    RowCell& rc = R[exp_idx[k]];
                    Item& it = items[rc.idx];

                    int share = (int)((int64)remainder * max(1, it.expandingWeight) / total_w);
                    if(share == 0 && rem > 0) share = 1;
                    share = min(share, rem);
                    rem -= share;

                    int neww = ClampWith(it.minw, it.maxw, rc.w + share);
                    int consumed = neww - rc.w;
                    if(consumed < share) rem += (share - consumed);
                    rc.w = neww;
                }
                // then gaps/spacers
                for(int k = 0; k < GE.GetCount() && rem > 0; ++k) {
                    GapExp& g = GE[k];
                    int share = (int)((int64)remainder * g.weight / total_w);
                    if(share == 0 && rem > 0) share = 1;
                    share = min(share, rem);
                    rem -= share;
                    g.minw += share;
                }
            }
        }

        // place left→right
        int x = irc.left;
        int placed_in_row = 0;

        for(int i = 0; i < R.GetCount(); ++i) {
            RowCell& rc = R[i];
            if(placed_in_row > 0) x += gap;
            Item& it = items[rc.idx];

            // vertical (cross-axis)
            int ch = ClampWith(rc.hmin, rc.hmax, rc.base_h);
            Align va = eff_align(it);
            int topy = y;
            if(va == Align::Center)      topy = y + (row_h - ch) / 2;
            else if(va == Align::End)    topy = y + (row_h - ch);
            else if(va == Align::Stretch || va == Align::Auto) { ch = ClampWith(rc.hmin, rc.hmax, row_h); topy = y; }

            // write cell rect
            it.cl.cell = Rect(x, y, x + rc.w, y + row_h);
            it.cl.rowOrCol = r;

            // content rect
            if(it.c) {
                int cx = x, avail_w = rc.w;
                // if you’re in fixed_column path we already used cell width; content padding handled below
                int natural_w;
                const Size ms = GetCtrlMinSize(it);
                if(it.fixed >= 0)               natural_w = it.fixed;
                else if(it.fit)                 natural_w = ms.cx;
                else if(it.expandingWeight > 0) natural_w = avail_w;
                else                             natural_w = ms.cx;
                natural_w = ClampWith(it.minw, it.maxw, natural_w);

                Align ha = eff_align(it);
                int cw;
                if(ha == Align::Stretch || ha == Align::Auto || it.expandingWeight > 0) {
                    cw = avail_w;
                } else {
                    cw = min(natural_w, avail_w);
                    if(ha == Align::Center) cx += (avail_w - cw)/2;
                    else if(ha == Align::End) cx += (avail_w - cw);
                }

                it.cl.content = Rect(cx, topy, cx + cw, topy + ch);
            } else {
                it.cl.content = Rect(0,0,0,0);
            }

            x += rc.w;
            ++placed_in_row;
        }

        used_w = max(used_w, x - irc.left);
        used_h = max(used_h, (y - irc.top) + row_h);

        y += row_h;
        if(r + 1 < rows.GetCount()) y += gap;
    }
}

void FlowBoxLayout::LayoutVertical(const Rect& irc, int inner_w, int inner_h, int /*visible_semantic*/) {
    struct VCell { int idx; int h; int wshare; };

    auto eff_align = [&](const Item& it)->Align {
        return (it.align_self != Align::Auto) ? it.align_self : align_items;
    };

    Vector<VCell> cells;
    cells.Reserve(items.GetCount());

    int base_sum_h = 0;
    int exp_weight_sum = 0;

    // build cells
    for(int i = 0; i < items.GetCount(); ++i) {
        Item& it = items[i];
        if(!it.cl.visible) continue;

        VCell c; c.idx = i; c.h = 0; c.wshare = 0;

        if(fixed_row >= 0) {
            c.h = fixed_row;
        } else {
            if(it.is_break) {
                c.h = max(gap, 0);
                if(it.expandingWeight <= 0) c.wshare = 1; // gap-expander
            }
            else if(it.cl.spacer) {
                c.h = 0;
                c.wshare = max(1, it.expandingWeight);
            }
            else {
                const Size ms = GetCtrlMinSize(it);
                if(it.fixed >= 0)             c.h = it.fixed;
                else if(it.fit) {
                    c.h = ms.cy;

                    // height-for-width for H child that wraps & auto-resizes
                    if(it.c) {
                        if(FlowBoxLayout* fb = dynamic_cast<FlowBoxLayout*>(it.c)) {
                            if(fb->dir == H && fb->wrap && fb->wrap_auto_resize) {
                                const int child_inner_w = max(0, inner_w - fb->inset.left - fb->inset.right);
                                fb->PreLayoutCalc(RectC(0, 0, child_inner_w, INT_MAX));
                                const int wanth = fb->used_h + fb->inset.top + fb->inset.bottom;
                                c.h = max(c.h, wanth);
                            }
                        }
                    }
                }
                else if(it.expandingWeight>0) c.h = 0;
                else                          c.h = ms.cy;
                if(it.expandingWeight>0)      c.wshare = max(1, it.expandingWeight);
            }
            c.h = ClampWith(it.minh, it.maxh, c.h);
        }

        base_sum_h += c.h;
        exp_weight_sum += c.wshare;
        cells.Add(c);
    }

    // distribute remainder
    const int gaps_total = max(0, cells.GetCount() - 1) * gap;
    int remainder = (fixed_row >= 0 ? 0 : max(0, inner_h - (base_sum_h + gaps_total)));

    if(fixed_row < 0 && exp_weight_sum > 0 && remainder > 0) {
        int rem = remainder;
        for(int k = 0; k < cells.GetCount(); ++k) {
            if(cells[k].wshare <= 0) continue;
            int share = (int)((int64)remainder * cells[k].wshare / exp_weight_sum);
            if(share == 0 && rem > 0) share = 1;
            share = min(share, rem);
            rem -= share;

            Item& it = items[cells[k].idx];
            int nh = ClampWith(it.minh, it.maxh, cells[k].h + share);
            int consumed = nh - cells[k].h;
            if(consumed < share) rem += (share - consumed);
            cells[k].h = nh;
            if(rem == 0) break;
        }
        if(rem > 0) {
            for(int k = 0; k < cells.GetCount() && rem > 0; ++k) {
                if(cells[k].wshare <= 0) continue;
                Item& it = items[cells[k].idx];
                int room = (it.maxh >= 0 ? it.maxh : INT_MAX) - cells[k].h;
                if(room <= 0) continue;
                int take = min(room, rem);
                cells[k].h += take;
                rem -= take;
            }
        }
    }

    // place top→bottom
    int y = irc.top;
    int max_w = 0;

    for(int k = 0; k < cells.GetCount(); ++k) {
        Item& it = items[cells[k].idx];
        it.cl.cell = Rect(irc.left, y, irc.right, y + cells[k].h);
        it.cl.rowOrCol = k;

        if(it.c) {
            const Size ms = GetCtrlMinSize(it);
            Align ha = eff_align(it);
            int natural_w = (it.fixed >= 0 ? it.fixed : ms.cx);
            natural_w = ClampWith(it.minw, it.maxw, natural_w);

            int cw = (ha == Align::Stretch || ha == Align::Auto)
                       ? ClampWith(it.minw, it.maxw, inner_w)
                       : min(natural_w, inner_w);
            int cx = irc.left;
            if(ha == Align::Center && cw < inner_w) cx = irc.left + (inner_w - cw) / 2;
            else if(ha == Align::End && cw < inner_w) cx = irc.right - cw;

            it.cl.content = Rect(cx, y, cx + cw, y + cells[k].h);
            max_w = max(max_w, cw);
        } else {
            it.cl.content = Rect(0,0,0,0);
        }

        y += cells[k].h;
        if(k + 1 < cells.GetCount()) y += gap;
    }

    used_w = max_w;
    used_h = min(inner_h, y - irc.top);
}

void FlowBoxLayout::InvalidateMinSize(Ctrl& c) {
    for(Item& it : items) {
        if(it.c == &c) {
            it.ms_valid = false;
            // do not Layout() here; let caller decide
            return;
        }
    }
}

void FlowBoxLayout::InvalidateAllMinSizes() {
    // Bump epoch so all items become stale lazily
    ++minsize_epoch;
    if(minsize_epoch == INT_MAX) {
        // Extremely unlikely; hard reset to keep logic simple
        minsize_epoch = 1;
        for(Item& it : items)
            it.ms_valid = false;
    }
}

Size FlowBoxLayout::GetCtrlMinSize(Item& it) {
    if(!it.c) return Size(0,0);
    if(!it.ms_valid || it.ms_epoch != minsize_epoch) {
        it.cachedMinSize = it.c->GetMinSize();
        it.ms_epoch      = minsize_epoch;
        it.ms_valid      = true;
    }
    return it.cachedMinSize;
}

int FlowBoxLayout::MeasureHeightForWidth(int width) {
    // Use the *inner* width that content actually gets
    Rect irc = RectC(0, 0, max(0, width - inset.left - inset.right), INT_MAX);
    PreLayoutCalc(irc);                // plan rows/cells for that width
    return used_h + inset.top + inset.bottom;  // height required (includes padding)
}

Size FlowBoxLayout::GetMinSize() const {
    // If horizontal + wrapping + auto-resize: report height-for-width based on current width.
    // This makes width-sensitive flows cooperate with generic scroll parents (e.g., StageCard).
    if(dir == H && wrap && wrap_auto_resize) {
        int eff_inner_w = GetSize().cx - inset.left - inset.right;
        if(eff_inner_w <= 0) eff_inner_w = plan_inner.cx;
        if(eff_inner_w <= 0) eff_inner_w = DPI(240); // conservative fallback

        int h = const_cast<FlowBoxLayout*>(this)->MeasureHeightForWidth(eff_inner_w);
        if(h < 0) h = 0;

        // Width: keep current width as a conservative baseline (parent will set it anyway)
        int baseline_w = max(1, GetSize().cx);
        return Size(baseline_w, h + inset.top + inset.bottom);
    }

    // Default conservative computation (no height-for-width)
    int cross = 0, main = 0, visible = 0;

    if(dir == V) {
        for(const Item& it : items) {
            if(!(it.c && it.c->IsShown()))
                continue;
            ++visible;

            const Size ms = it.c->GetMinSize();

            // Main-axis (height) with per-item caps and container fixed_row
            int add = ClampWith(it.minh, it.maxh, basePrimary(it, ms, /*vertical*/true));
            if(fixed_row >= 0)
                add = min(add, fixed_row);
            main += add;

            // Cross-axis (width) envelope
            cross = max(cross, ClampWith(it.minw, it.maxw, ms.cx));
        }
        if(visible > 1)
            main += (visible - 1) * gap;

        return Size(cross + inset.left + inset.right,
                    main  + inset.top  + inset.bottom);
    } else {
        for(const Item& it : items) {
            if(!(it.c && it.c->IsShown()))
                continue;
            ++visible;

            const Size ms = it.c->GetMinSize();

            // Main-axis (width)
            const int snapped = (fixed_column >= 0 ? fixed_column
                                                   : basePrimary(it, ms, /*vertical*/false));
            int add = ClampWith(it.minw, it.maxw, snapped);
            main += add;

            // Cross-axis (height) envelope
            cross = max(cross, ClampWith(it.minh, it.maxh, ms.cy));
        }
        if(visible > 1)
            main += (visible - 1) * gap;

        return Size(main  + inset.left + inset.right,
                    cross + inset.top  + inset.bottom);
    }
}




void FlowBoxLayout::Paint(Draw& w) {
    if(!debug) return;
    Rect rc = GetSize();
    Rect inner = rc;
    inner.left   += inset.left;
    inner.top    += inset.top;
    inner.right  -= inset.right;
    inner.bottom -= inset.bottom;
    DebugPaint(w, inner);
}

void FlowBoxLayout::DebugPaint(Draw& w, const Rect& inner_rc) const {
    if(!debug) return;

    // --- tuning knobs
    const int   t           = max(1, DPI(2));          // stroke thickness
    const int   grid_step   = max(4, DPI(12));         // grid spacing
    const int   tint_k      = 7000;                    // 0..65535: bg tint toward stroke
    const int   depth_k     = 2800;                    // 0..65535 per nesting level toward black
    const int   grid_k      = 8500;                    // 0..65535: grid color mix toward stroke
    const Color stroke(240,0,0);
    Font f = StdFont().Bold();
    
    // --- compute nesting depth for subtle darkening
    int depth = 0;
    for(const Ctrl* p = this; p; p = p->GetParent())
        if(dynamic_cast<const FlowBoxLayout*>(p)) ++depth;

    // --- background tint & per-depth darken (all via Blend)
    Color base_bg   = SColorFace();                    // or White()
    Color tinted_bg = Blend(base_bg, stroke, tint_k);  // nudge toward stroke
    int dk          = min(depth * depth_k, 22000);     // clamp darkening
    Color bg        = Blend(tinted_bg, Black(), dk);

    // --- paint an offscreen background (grid lines with Painter)
    const Size isz = inner_rc.GetSize();
    ImageBuffer ib(isz);
    BufferPainter p(ib, MODE_ANTIALIASED);

    // helpers
    auto FillRectPainter = [](Painter& pp, const Rect& r, Color c) {
        pp.Begin();
            pp.Move(Pointf(r.left,  r.top));
            pp.Line(Pointf(r.right, r.top));
            pp.Line(Pointf(r.right, r.bottom));
            pp.Line(Pointf(r.left,  r.bottom));
            pp.Close();
            pp.Fill(c);
        pp.End();
    };
    auto DrawGridPainter = [](Painter& pp, const Rect& r, int step, Color gc) {
        if(step <= 0) return;
        for(int x = r.left + step; x < r.right; x += step)
            pp.DrawRect(RectC(x, r.top, 1, r.Height()), gc);
        for(int y = r.top + step; y < r.bottom; y += step)
            pp.DrawRect(RectC(r.left, y, r.Width(), 1), gc);
    };

    // background + subtle grid (grid color is bg nudged toward stroke)
    FillRectPainter(p, RectC(0, 0, isz.cx, isz.cy), bg);
    Color gridc = Blend(bg, stroke, grid_k);
    DrawGridPainter(p, RectC(0, 0, isz.cx, isz.cy), grid_step, gridc);

    // blit the composed background
    w.DrawImage(inner_rc.left, inner_rc.top, Image(ib));

    // --- outline helper (uses Draw for speed)
    auto frame = [&](const Rect& r) {
        if(r.IsEmpty()) return;
        w.DrawRect(r.left,  r.top,           r.GetWidth(), t, stroke);         // top
        w.DrawRect(r.left,  r.bottom - t,    r.GetWidth(), t, stroke);         // bottom
        w.DrawRect(r.left,  r.top,           t,            r.GetHeight(), stroke); // left
        w.DrawRect(r.right - t, r.top,       t,            r.GetHeight(), stroke); // right
    };

    // outer frame
    frame(inner_rc);

    // --- items
    for(const Item& it : items) {
        if(!it.cl.visible) continue;

        // cell box
        frame(it.cl.cell);

      // Spacer → "←-→"
        if(it.cl.spacer) {
            const char* k = "\xE2\x86\x90-\xE2\x86\x92";
            Size ts = GetTextSize(k, f);
            w.DrawText(it.cl.cell.left + (it.cl.cell.GetWidth() - ts.cx)/2,
                       it.cl.cell.top  + (it.cl.cell.GetHeight()- ts.cy)/2,
                       k, f, stroke);
        }

        // Hard break mark (if you choose to keep it)
        if(it.cl.breakMark) {
            const char* br = "\xE2\x86\xB2";
            Size ts = GetTextSize(br, f);
            w.DrawText(it.cl.cell.right - DPI(10), it.cl.cell.top + DPI(2), br, f, stroke);
        }

        // content outline
        if(it.c)
            frame(it.cl.content);
    }
}



String FlowBoxLayout::ToString() const {
    String s;
    s << "FlowBoxLayout{dir=" << (dir == H ? "H" : "V")
      << ", wrap=" << (wrap ? "true" : "false")
      << ", gap=" << gap
      << ", inset=(" << inset.left << "," << inset.top << "," << inset.right << "," << inset.bottom << ")"
      << ", fixed_column=" << fixed_column
      << ", fixed_row="    << fixed_row
      << ", items=" << items.GetCount()
      << ", used=(" << used_w << "x" << used_h << ")"
      << ", debug=" << (debug ? "on" : "off") << "}";
    return s;
}

} // namespace Upp
