// Tobo Imager - the panel. Copyright (C) 2026 Tobokegao. Licensed under the GNU AGPL v3 or later.
// A port of the drawing code (@gfx) of the JSFX effect it started as (not part of this repository), changed since.
#include "PluginEditor.h"

using namespace tobo;

namespace
{
constexpr double LW = 464, LHT = 378, CR = 4, CJOG = 12, BY0 = 142, BYS = 230, LGH = 12;
constexpr int INK = 0x0d2233, WHITE = 0xffffff, DESK = 0xe4e9ef, PALE = 0xcfe0ea, BLUE = 0x006599, YEL = 0xffcc00, ONC = 0x3ddc97,
              AZURE = 0x58a6ff, ORANGE = 0xff7a14, TINT = 0xe3f0f8,
              DIS = 0x95a7b3;      // a control that does nothing now: pale mixed 30 % towards ink
constexpr int bcol[4] = { BLUE, AZURE, YEL, ORANGE };
// The least distance the panel keeps between two crossovers, and between a crossover and the ends of the range (20 Hz,
// 20 kHz): half an octave, as a ratio of frequencies. So no band is ever without width and no two handles stand on one
// spot. A handle that is dragged against its neighbour pushes it along (as the Ozone Imager does, which keeps a fixed
// distance on the screen, 0.55 to 0.8 octaves). A host can still set the parameters closer together or out of order;
// the engine takes them as they come.
constexpr double STLINE = 0.35;      // how strong the line at the STEREOIZE amount of a band is drawn (ink over the band colour)
constexpr double XGAP = 1.4142135623730951;
// what the bands are called and which colours they carry, for one, two, three and four bands
constexpr int bcolN[4][4] = { { AZURE, AZURE, AZURE, AZURE }, { BLUE, ORANGE, ORANGE, ORANGE }, { BLUE, YEL, ORANGE, ORANGE }, { BLUE, AZURE, YEL, ORANGE } };
const char* const bnameN[4][4] = { { "FULL", "", "", "" }, { "LOW", "HIGH", "", "" }, { "LOW", "MID", "HIGH", "" }, { "LOW", "LO MID", "HI MID", "HIGH" } };
constexpr double zooms[4] = { 1.0, 1.25, 1.5, 2.0 };
constexpr double SIDEA = 0.36;       // how strong the tint of the side curve is
double sqr (double v) { return v * v; }

// The name on the logo plate: its own letters, each a few strokes (lines and round bows) drawn with one heavy round pen,
// leaning a little to the right. Round ends and round bends give the soft look; the same shapes serve every size.
// Units: the middle of the pen runs from y = 0 (top) to y = 10 (foot); PEN is how wide the pen is.
struct Pt { double x, y; };
using Stroke = std::vector<Pt>;
constexpr double PEN = 2.3, LEAN = 0.18, LGAP = 1.7, LSPACE = 4.2;
void bow (Stroke& p, double cx, double cy, double r, double a0, double a1)      // degrees, 0 = right, 90 = down
{
    for (int i = 0; i <= 12; ++i) { const double a = (a0 + (a1 - a0) * i / 12) * tobo::kPi / 180; p.push_back ({ cx + r * std::cos (a), cy + r * std::sin (a) }); }
}
void oval (Stroke& p, double a0, double a1)       // the soft box of O and G (a superellipse 8 wide, 10 high)
{
    for (int i = 0; i <= 40; ++i)
    {
        const double a = (a0 + (a1 - a0) * i / 40) * tobo::kPi / 180, c = std::cos (a), q = std::sin (a);
        p.push_back ({ 4 + 4 * (c < 0 ? -1 : 1) * std::pow (std::abs (c), 0.72), 5 + 5 * (q < 0 ? -1 : 1) * std::pow (std::abs (q), 0.72) });
    }
}
double letter (char ch, std::vector<Stroke>& out)      // returns the width of the letter
{
    Stroke a, b, c;
    switch (ch)
    {
        case 'T': out.push_back ({ { 0, 0 }, { 8, 0 } }); out.push_back ({ { 4, 0 }, { 4, 10 } }); return 8;
        case 'O': oval (a, 0, 360); out.push_back (a); return 8;
        case 'B': out.push_back ({ { 0, 0 }, { 0, 10 } });
                  a = { { 0, 0 }, { 4.6, 0 } }; bow (a, 4.6, 2.5, 2.5, -90, 90); a.push_back ({ 0, 5 }); out.push_back (a);
                  b = { { 0, 5 }, { 5.0, 5 } }; bow (b, 5.0, 7.5, 2.5, -90, 90); b.push_back ({ 0, 10 }); out.push_back (b); return 7.5;
        case 'I': out.push_back ({ { 0, 0 }, { 0, 10 } }); return 0;
        case 'M': out.push_back ({ { 0, 10 }, { 0, 0 }, { 4.5, 7.2 }, { 9, 0 }, { 9, 10 } }); return 9;
        case 'A': out.push_back ({ { 0, 10 }, { 4, 0 }, { 8, 10 } }); out.push_back ({ { 1.6, 6.8 }, { 6.4, 6.8 } }); return 8;
        case 'G': oval (a, -38, -360); a.push_back ({ 4.4, 5 }); out.push_back (a); return 8;
        case 'E': out.push_back ({ { 7.4, 0 }, { 0, 0 }, { 0, 10 }, { 7.4, 10 } }); out.push_back ({ { 0, 5 }, { 6.2, 5 } }); return 7.4;
        case 'R': out.push_back ({ { 0, 0 }, { 0, 10 } });
                  a = { { 0, 0 }, { 4.6, 0 } }; bow (a, 4.6, 2.75, 2.75, -90, 90); a.push_back ({ 0, 5.5 }); out.push_back (a);
                  out.push_back ({ { 3.4, 5.5 }, { 7.6, 10 } }); return 7.6;
        default: return 0;
    }
}

double now() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }
}

//==============================================================================
void Canvas::resize (int nw, int nh)
{
    w = juce::jmax (1, nw); h = juce::jmax (1, nh);
    img = juce::Image (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::readWrite);
    px = bd.data; stride = bd.lineStride;      // a software image keeps its memory, so the pointer stays good
    unclip();
}

inline void Canvas::put (int x, int y, double alpha)
{
    auto* p = px + y * stride + x * 4;         // blue, green, red, alpha
    if (alpha >= 0.999) { p[0] = (juce::uint8) cb; p[1] = (juce::uint8) cg; p[2] = (juce::uint8) cr; p[3] = 255; return; }
    p[0] = (juce::uint8) (p[0] + (cb - p[0]) * alpha + 0.5); p[1] = (juce::uint8) (p[1] + (cg - p[1]) * alpha + 0.5);
    p[2] = (juce::uint8) (p[2] + (cr - p[2]) * alpha + 0.5); p[3] = 255;
}

void Canvas::rect (double x, double y, double rw, double rh)
{
    int x0 = (int) x, y0 = (int) y, x1 = x0 + (int) rw, y1 = y0 + (int) rh;
    x0 = juce::jmax (kx0, x0); y0 = juce::jmax (ky0, y0); x1 = juce::jmin (kx1, x1); y1 = juce::jmin (ky1, y1);
    for (int yy = y0; yy < y1; ++yy) for (int xx = x0; xx < x1; ++xx) put (xx, yy, a);
}

void Canvas::circle (double cx, double cy, double r)
{
    const int x0 = juce::jmax (kx0, (int) std::floor (cx - r - 1)), x1 = juce::jmin (kx1 - 1, (int) std::ceil (cx + r + 1));
    const int y0 = juce::jmax (ky0, (int) std::floor (cy - r - 1)), y1 = juce::jmin (ky1 - 1, (int) std::ceil (cy + r + 1));
    for (int yy = y0; yy <= y1; ++yy)
        for (int xx = x0; xx <= x1; ++xx)
        {
            const double c = (r < 0.01 ? 0.5 : r < 1.0 ? r + 0.5 : r) + 0.5 - std::sqrt (sqr (xx - cx) + sqr (yy - cy));   // the smallest dot is one pixel
            if (c > 0.02) put (xx, yy, a * juce::jmin (1.0, c));
        }
}

void Canvas::blit (const Canvas& src, int sx, int sy, int sw, int sh, int dx, int dy)
{
    for (int yy = 0; yy < sh; ++yy)
    {
        const int ys = sy + yy, yd = dy + yy;
        if (ys < 0 || ys >= src.h || yd < 0 || yd >= h) continue;
        for (int xx = 0; xx < sw; ++xx)
        {
            const int xs = sx + xx, xd = dx + xx;
            if (xs < 0 || xs >= src.w || xd < 0 || xd >= w) continue;
            std::memcpy (px + yd * stride + xd * 4, src.px + ys * src.stride + xs * 4, 4);
        }
    }
}

int Canvas::red (int x, int y) const { return (x < 0 || y < 0 || x >= w || y >= h) ? 0 : px[y * stride + x * 4 + 2]; }

void Canvas::measure (const juce::String& s, double& tw, double& th) const
{
    tw = std::ceil (juce::GlyphArrangement::getStringWidth (fnt, s)); th = std::ceil (fnt.getHeight());
}

void Canvas::text (const juce::String& s, int x, int y)
{
    juce::Graphics gr (img);
    gr.setColour (juce::Colour ((juce::uint8) cr, (juce::uint8) cg, (juce::uint8) cb));
    gr.setFont (fnt);
    gr.drawSingleLineText (s, x, y + juce::roundToInt (fnt.getAscent()));
}

void Canvas::wash (int hex, double alpha)
{
    const double rr = (hex >> 16) & 255, gg = (hex >> 8) & 255, bb = hex & 255;
    for (int y = 0; y < h; ++y)
    {
        auto* p = px + y * stride;
        for (int x = 0; x < w; ++x, p += 4)
        {
            p[0] = (juce::uint8) (p[0] + (bb - p[0]) * alpha + 0.5); p[1] = (juce::uint8) (p[1] + (gg - p[1]) * alpha + 0.5); p[2] = (juce::uint8) (p[2] + (rr - p[2]) * alpha + 0.5);
        }
    }
}

void Canvas::touch() { juce::Image::BitmapData bd (img, juce::Image::BitmapData::readWrite); px = bd.data; stride = bd.lineStride; }

//==============================================================================
ToboImagerEditor::ToboImagerEditor (ToboImagerProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setOpaque (true);
    setResizable (false, false);
    fbm.assign ((size_t) Engine::SPN * 2, 0.0); fbs.assign ((size_t) Engine::SPN * 2, 0.0);
    applySize();
    startTimerHz (30);
}

ToboImagerEditor::~ToboImagerEditor() { stopTimer(); endGesture(); }

void ToboImagerEditor::applySize()
{
    const double z = zooms[proc.getGuiSize()];
    setSize ((int) std::ceil (LW * z), (int) std::ceil (LHT * z));
}

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "(test build)"      // PanelShot is not a plugin target
#endif
void ToboImagerEditor::showMenu()
{
    juce::PopupMenu m;
    const char* names[4] = { "1x", "1.25x", "1.5x", "2x" };
    m.addSectionHeader ("Size");
    for (int k = 0; k < 4; ++k) m.addItem (k + 1, names[k], true, proc.getGuiSize() == k);
    m.addSeparator(); m.addItem (99, "Tobo Imager " JucePlugin_VersionString, false, false);
    juce::Component::SafePointer<ToboImagerEditor> self (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [self] (int r)
    {
        if (self == nullptr || r <= 0 || r > 4) return;
        self->proc.setGuiSize (r - 1); self->applySize();
    });
}

void ToboImagerEditor::track (const juce::MouseEvent& e)
{
    mouse_x = e.position.x * scale; mouse_y = e.position.y * scale;
    ctrl = e.mods.isCtrlDown(); shift = e.mods.isShiftDown();
}
void ToboImagerEditor::mouseDown (const juce::MouseEvent& e)
{
    track (e);
    if (e.mods.isPopupMenu()) { showMenu(); return; }
    held = true; pressed = true; pressX = mouse_x; pressY = mouse_y;
}
void ToboImagerEditor::mouseDrag (const juce::MouseEvent& e) { track (e); }
void ToboImagerEditor::mouseMove (const juce::MouseEvent& e) { track (e); }
void ToboImagerEditor::mouseUp (const juce::MouseEvent& e) { track (e); held = false; }

void ToboImagerEditor::timerCallback() { frame(); repaint(); }

void ToboImagerEditor::paint (juce::Graphics& gr)
{
    // the image is made in the pixels of the screen; on a scaled display it is drawn back at one image pixel per screen pixel
    const double s = (double) gr.getInternalContext().getPhysicalPixelScaleFactor();
    if (std::abs (s - scale) > 0.001 && s > 0.1) scale = s;
    if (scr.width() < 2) { gr.fillAll (juce::Colour (0xffe4e9ef)); return; }
    gr.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    gr.drawImageTransformed (scr.image(), juce::AffineTransform::scale ((float) (1.0 / scale)));
}

//==============================================================================
int ToboImagerEditor::mixc (int h1, int h2, double t)
{
    auto ch = [&] (int sh) { return (int) std::floor (((h1 >> sh) & 255) * (1 - t) + ((h2 >> sh) & 255) * t + 0.5); };
    return (ch (16) << 16) + (ch (8) << 8) + ch (0);
}

void ToboImagerEditor::frect (double x, double y, double w, double h) { g->rect (ux (x), uy (y), ux (x + w) - ux (x), uy (y + h) - uy (y)); }

void ToboImagerEditor::rrect (double x, double y, double w, double h, double rad)
{
    const int x0 = ux (x), y0 = uy (y), x1 = ux (x + w), y1 = uy (y + h);
    const int rp = juce::jmin ((int) std::floor (rad * ZM + 0.5), (int) std::floor ((juce::jmin (x1 - x0, y1 - y0) - 1) / 2.0));
    if (rp < 1) { g->rect (x0, y0, x1 - x0, y1 - y0); return; }
    g->rect (x0 + rp, y0, x1 - x0 - 2 * rp, y1 - y0); g->rect (x0, y0 + rp, x1 - x0, y1 - y0 - 2 * rp);
    g->circle (x0 + rp, y0 + rp, rp); g->circle (x1 - 1 - rp, y0 + rp, rp);
    g->circle (x0 + rp, y1 - 1 - rp, rp); g->circle (x1 - 1 - rp, y1 - 1 - rp, rp);
}

void ToboImagerEditor::box (double x, double y, double w, double h, double rad, int fill)
{
    setc (INK); rrect (x, y, w, h, rad);
    setc (fill); rrect (x + LN, y + LN, w - 2 * LN, h - 2 * LN, rad - LN);
}

void ToboImagerEditor::sline (double xa, double ya, double xb, double yb, double grow)
{
    const double hw = juce::jmax (0.0, (LN * ZM - 1.5) * 0.5) + grow;
    const int n = juce::jmax (1, (int) std::ceil (std::sqrt (sqr (xb - xa) + sqr (yb - ya)) / 0.5));
    for (int j = 0; j <= n; ++j) g->circle (xa + (xb - xa) * j / n, ya + (yb - ya) * j / n, hw);
}

// arc of the one line weight around the pixel (pcx, pcy), from a0 to a1 degrees (0 = up, clockwise; within -180..180)
void ToboImagerEditor::aarc (double pcx, double pcy, double rad, double a0, double a1)
{
    const double hw = LN * ZM * 0.5;
    int yy = (int) std::floor (pcy - rad - hw - 1);
    const int rows = 2 * (int) std::ceil (rad + hw + 1) + 1;
    for (int r = 0; r < rows; ++r, ++yy)
    {
        const double dy = yy - pcy;
        if (std::abs (dy) > rad + hw + 0.5) continue;
        const double xo = std::sqrt (sqr (rad + hw + 0.5) - dy * dy);
        const double xi = std::abs (dy) < rad - hw - 0.5 ? std::sqrt (sqr (rad - hw - 0.5) - dy * dy) : 0;
        for (int sp = 0; sp < (xi > 0 ? 2 : 1); ++sp)
        {
            const int x0 = (int) (xi > 0 ? (sp == 0 ? std::floor (pcx - xo) : std::floor (pcx + xi)) : std::floor (pcx - xo));
            const int x1 = (int) (xi > 0 ? (sp == 0 ? std::ceil (pcx - xi) : std::ceil (pcx + xo)) : std::ceil (pcx + xo));
            for (int xx = x0; xx <= x1; ++xx)
            {
                const double dx = xx - pcx, d = std::sqrt (dx * dx + dy * dy);
                const double c = juce::jlimit (0.0, 1.0, hw + 0.5 - std::abs (d - rad));
                if (c > 0)
                {
                    double ca = std::atan2 (dx, -dy) * 180 / kPi;
                    ca = juce::jlimit (0.0, 1.0, juce::jmin (ca - a0, a1 - ca) * kPi / 180 * rad + 0.5);
                    if (c * ca > 0.02) { g->a = c * ca; g->rect (xx, yy, 1, 1); }
                }
            }
        }
    }
    g->a = 1;
}

// straight piece of the one line weight with round ends, between two points in pixels
void ToboImagerEditor::aaseg (double xa, double ya, double xb, double yb)
{
    const double hw = LN * ZM * 0.5, l2 = sqr (xb - xa) + sqr (yb - ya);
    const int x0 = (int) std::floor (juce::jmin (xa, xb) - hw - 1), x1 = (int) std::ceil (juce::jmax (xa, xb) + hw + 1);
    const int y0 = (int) std::floor (juce::jmin (ya, yb) - hw - 1), y1 = (int) std::ceil (juce::jmax (ya, yb) + hw + 1);
    for (int yy = y0; yy <= y1; ++yy)
        for (int xx = x0; xx <= x1; ++xx)
        {
            const double t = l2 > 0 ? juce::jlimit (0.0, 1.0, ((xx - xa) * (xb - xa) + (yy - ya) * (yb - ya)) / l2) : 0;
            const double d = hw + 0.5 - std::sqrt (sqr (xx - xa - t * (xb - xa)) + sqr (yy - ya - t * (yb - ya)));
            if (d > 0.02) { g->a = juce::jmin (1.0, d); g->rect (xx, yy, 1, 1); }
        }
    g->a = 1;
}

// 45 degree line going down to the left, as heavy as an upright bar whose left pixel is c
void ToboImagerEditor::slant45 (int c, int y0, int n)
{
    const int lw = (int) std::floor (LN * ZM + 0.5);
    for (int j = 0; j < n; ++j)
    {
        g->a = 1; g->rect (c - j, y0 + j, lw, 1);
        g->a = 0.41;
        if (j < n - 1) g->rect (c - j - 1, y0 + j, 1, 1);
        if (j > 0) g->rect (c - j + lw, y0 + j, 1, 1);
    }
    g->a = 1;
}

// A line of the band plate at its jog, as one stroke: upright down to the row y0, n rows at 45 degrees to the left, upright
// again. xu = left pixel of the upper upright bar. Each pixel gets the share of it that a stroke of the one line weight
// (measured at right angles, so the slanted part is as heavy as the upright parts) covers; the joints are part of the stroke.
void ToboImagerEditor::jog (int xu, int y0, int n)
{
    const double hw = std::floor (LN * ZM + 0.5) * 0.5, cx = xu + hw, ext = 3 * hw + 2;
    // the bends lie half a row up, so that a line of an odd number of pixels runs through the middles of pixels on the slant
    const double px[4] = { cx, cx, cx - n, cx - n }, py[4] = { y0 - ext, y0 - 0.5, y0 + n - 0.5, y0 + n + ext };
    for (int yy = (int) std::floor (y0 - ext); yy <= (int) std::ceil (y0 + n + ext); ++yy)
        for (int xx = (int) std::floor (cx - n - hw - 1); xx <= (int) std::ceil (cx + hw + 1); ++xx)
        {
            double d2 = 1e18;
            for (int k = 0; k < 3; ++k)
            {
                const double ex = px[k + 1] - px[k], ey = py[k + 1] - py[k], wx = xx + 0.5 - px[k], wy = yy + 0.5 - py[k];
                const double t = juce::jlimit (0.0, 1.0, (wx * ex + wy * ey) / (ex * ex + ey * ey));
                d2 = juce::jmin (d2, sqr (wx - ex * t) + sqr (wy - ey * t));
            }
            const double c = hw + 0.5 - std::sqrt (d2);
            if (c > 0.02) { g->a = juce::jmin (1.0, c); g->rect (xx, yy, 1, 1); }
        }
    g->a = 1;
}

// four-sided shape with rounded corners (for plates with slanted sides): signed distance to the shape qx/qy
double ToboImagerEditor::sdquad (double px, double py) const
{
    auto seg = [&] (int i, int j)
    {
        const double ex = qx[j] - qx[i], ey = qy[j] - qy[i], wx = px - qx[i], wy = py - qy[i];
        const double t = juce::jlimit (0.0, 1.0, (wx * ex + wy * ey) / (ex * ex + ey * ey));
        return sqr (wx - ex * t) + sqr (wy - ey * t);
    };
    auto side = [&] (int i, int j) { return (qx[j] - qx[i]) * (py - qy[i]) - (qy[j] - qy[i]) * (px - qx[i]) >= 0; };
    const double d = juce::jmin (juce::jmin (seg (0, 1), seg (1, 2)), juce::jmin (seg (2, 3), seg (3, 0)));
    const bool ins = side (0, 1) && side (1, 2) && side (2, 3) && side (3, 0);
    return ins ? -std::sqrt (d) : std::sqrt (d);
}

void ToboImagerEditor::fillrq (int x0, int y0, int x1, int y1, double rad, double shr)
{
    for (int yy = y0; yy <= y1; ++yy)
        for (int xx = x0; xx <= x1; ++xx)
        {
            const double c = 0.5 - (sdquad (xx, yy) - rad + shr);
            if (c > 0.02) { g->a = juce::jmin (1.0, c); g->rect (xx, yy, 1, 1); }
        }
    g->a = 1;
}

void ToboImagerEditor::setrq (double xl, double xr, double yt, double yb, double rad, bool slr)
{
    const double k2 = std::sqrt (2.0), tt = yt - 0.5 + rad, bb = yb + 0.5 - rad, cl = xl - 0.5 + yt - 0.5 + rad * k2;
    qx[0] = cl - tt; qy[0] = tt; qx[3] = cl - bb; qy[3] = bb; qy[1] = tt; qy[2] = bb;
    if (slr) { const double c = xr + 0.5 + yt - 0.5 - rad * k2; qx[1] = c - tt; qx[2] = c - bb; }
    else { qx[1] = xr + 0.5 - rad; qx[2] = qx[1]; }
}

void ToboImagerEditor::tcen (double cx, double cy, const juce::String& s)
{
    double w, h; g->measure (s, w, h);
    g->text (s, (int) std::floor (ox + cx * ZM - w / 2 + 0.5), (int) std::floor (oy + cy * ZM - h / 2 + 0.5));
}
// The name on a button (capitals and digits only): the rows of pixels its letters really cover are put in the middle of
// the rows y0 .. y1 - 1 (the inside of the frame). If the space cannot be shared evenly, the odd row goes below.
void ToboImagerEditor::tcap (double cxPix, int y0, int y1, const juce::String& s)
{
    // left to right too, by the columns the letters really cover: a "1" stands off the middle of the room the font gives it
    // (and top to bottom by the rows this very name covers: the digits of the font are a little taller than its capitals)
    inkSpan (s); const auto rows = inkRows[s];
    const int sp = (y1 - y0) - (rows.second - rows.first + 1);
    g->text (s, (int) std::floor (cxPix - inkMid[s] + 0.5), y0 + (int) std::floor (sp / 2.0) - rows.first);
}
std::pair<int, int> ToboImagerEditor::inkSpan (const juce::String& s)
{
    const auto it = inkSpans.find (s);
    if (it != inkSpans.end()) return it->second;
    double w, h; g->measure (s, w, h);
    Canvas c; c.resize ((int) w + 24, 64); c.set (0xffffff); c.rect (0, 0, (int) w + 24, 64); c.font (f3); c.set (0x000000); c.text (s, 8, 8); c.touch();
    int lo = 9999, hi = -9999;
    for (int x = 0; x < (int) w + 24; ++x)
    {
        int dark = 255; for (int y = 0; y < 64; ++y) dark = juce::jmin (dark, c.red (x, y));
        if (dark < 128) { lo = juce::jmin (lo, x - 8); hi = juce::jmax (hi, x - 8); }
    }
    if (lo > hi) { lo = 0; hi = (int) w - 1; }
    // The middle of the name, left to right, to a part of a pixel: its first and last column are mostly only partly
    // covered (the grey of the smoothed edge), and the edge of the letter lies as far into that column as it is dark.
    {
        double le = lo, re = hi + 1; bool first = true;
        for (int x = 0; x < (int) w + 24; ++x)
        {
            int dark = 255; for (int y = 0; y < 64; ++y) dark = juce::jmin (dark, c.red (x, y));
            const double cov = (255 - dark) / 255.0;
            if (cov > 0.08) { if (first) { le = (x - 8) + (1 - cov); first = false; } re = (x - 8) + cov; }
        }
        inkMid[s] = (le + re) / 2;
    }
    int top = 99, bot = -99;
    for (int y = 0; y < 64; ++y)
    {
        int dark = 255; for (int x = 0; x < (int) w + 24; ++x) dark = juce::jmin (dark, c.red (x, y));
        if (dark < 128) { top = juce::jmin (top, y - 8); bot = juce::jmax (bot, y - 8); }
    }
    if (top > bot) { top = capTop; bot = capBot; }
    inkRows[s] = { top, bot };
    return inkSpans[s] = { lo, hi };
}
// finds the rows that a capital of the button font covers (counted from the y given to Canvas::text)
void ToboImagerEditor::capRows()
{
    Canvas c; c.resize (64, 64); c.set (0xffffff); c.rect (0, 0, 64, 64); c.font (f3); c.set (0x000000); c.text ("HEM", 4, 8); c.touch();
    inkSpans.clear(); inkRows.clear(); inkMid.clear();
    capTop = 99; capBot = -99;
    for (int y = 0; y < 64; ++y)
    {
        int dark = 255; for (int x = 0; x < 64; ++x) dark = juce::jmin (dark, c.red (x, y));
        if (dark < 128) { capTop = juce::jmin (capTop, y - 8); capBot = juce::jmax (capBot, y - 8); }
    }
}
void ToboImagerEditor::tleft (double x, double cy, const juce::String& s)
{
    double w, h; g->measure (s, w, h);
    g->text (s, ux (x), (int) std::floor (oy + cy * ZM - h / 2 + 0.5));
}
void ToboImagerEditor::tright (double x, double cy, const juce::String& s)
{
    double w, h; g->measure (s, w, h);
    g->text (s, ux (x) - (int) w, (int) std::floor (oy + cy * ZM - h / 2 + 0.5));
}

int ToboImagerEditor::bc (int k) const { return bcolN[NB - 1][k]; }

ToboImagerEditor::BandPos ToboImagerEditor::bandPos (int k) const
{
    const double cm = (colX (k) + colX (k + 1)) / 2;
    // in a column: WIDTH in the upper row (which stands 2 * CJOG to the right), SAFE in the jog, STEREOIZE and M / S below
    BandPos q { cm + 2 * CJOG, BY0 + 50, cm, BY0 + 129.5, cm, BY0 + 81, cm, BY0 + 160, 11 };
    return q;
}

double ToboImagerEditor::sget (int k) const
{
    auto* p = proc.params[k - 1];
    return (double) p->convertFrom0to1 (p->getValue());
}

void ToboImagerEditor::sset (int k, double v, bool drag)
{
    auto* p = proc.params[k - 1];
    if (drag)
    {
        if (gestureParam != k) { endGesture(); p->beginChangeGesture(); gestureParam = k; }
        p->setValueNotifyingHost (p->convertTo0to1 ((float) v));
    }
    else
    {
        if (gestureParam == k) endGesture();
        p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 ((float) v)); p->endChangeGesture();
    }
}

void ToboImagerEditor::endGesture()
{
    if (gestureParam > 0) proc.params[gestureParam - 1]->endChangeGesture();
    gestureParam = -1;
}

// knob: drag up/down, shift = fine, double-click or ctrl-click = default
void ToboImagerEditor::knob (int id, int sl, double cx, double cy, double r, double lo, double hi, double def, double stp, int face, bool enabled)
{
    double v = sget (sl);
    const bool hit = enabled && sqr (mxl - cx) + sqr (myl - cy) <= sqr (r + 5);
    if (mpress && hit)
    {
        if ((lastid == id && now() - lastt < 0.35) || ctrl) { sset (sl, def); v = def; act = 0; lastid = 0; }
        else { act = id; av = v; ay = mouse_y; dragShift = shift; lastid = id; lastt = now(); }
    }
    // a knob that stops being usable in the middle of a drag (the host changed the type) is let go
    if (act == id && ! enabled) { endGesture(); act = 0; }
    if (act == id)
    {
        // shift pressed or let go in the middle of a drag: the drag goes on from where the knob is now, at the new speed
        // (without this the whole way dragged so far would be counted again at the new speed, and the knob would jump)
        if (shift != dragShift) { av = v; ay = mouse_y; dragShift = shift; }
        double nv = av + (ay - mouse_y) / ZM * (hi - lo) / (shift ? 900 : 150);
        nv = std::floor (nv / stp + 0.5) * stp; nv = juce::jlimit (lo, hi, nv);
        if (std::abs (nv - v) > stp * 0.01) { sset (sl, nv, true); v = nv; }
    }
    const double nrm = (v - lo) / (hi - lo);
    const int pcx = ux (cx), pcy = uy (cy), rp = (int) std::floor (r * ZM + 0.5);
    // value arc: a pale track, the part up to the value in ink
    setc (PALE); aarc (pcx, pcy, rp + 3.5 * ZM, -135, 135);
    if (nrm > 0 && enabled) { setc (INK); aarc (pcx, pcy, rp + 3.5 * ZM, -135, -135 + 270 * nrm); }
    // flat face with the ink rim
    setc (enabled ? INK : DIS); g->circle (pcx, pcy, rp);
    setc (act == id ? ONC : face); g->circle (pcx, pcy, rp - std::floor (LN * ZM + 0.5));
    const double a = (-135 + 270 * nrm) * kPi / 180;
    setc (! enabled ? DIS : act != id && face == BLUE ? WHITE : INK);
    aaseg (pcx + std::sin (a) * rp * 0.30, pcy - std::cos (a) * rp * 0.30, pcx + std::sin (a) * (rp - 3.5 * ZM), pcy - std::cos (a) * (rp - 3.5 * ZM));
}

// button, with the same corner as every plate; inv = ink face with white text when on
bool ToboImagerEditor::button (double x, double y, double w, double h, const juce::String& s, bool on, bool inv)
{
    const bool hit = mxl >= x && mxl < x + w && myl >= y && myl < y + h;
    box (x, y, w, h, CR, on ? (inv ? INK : ONC) : WHITE);
    { const int lp = (int) std::floor (LN * ZM + 0.5); g->font (f3); setc (on && inv ? WHITE : INK); tcap ((ux (x) + ux (x + w)) / 2.0, uy (y) + lp, uy (y + h) - lp, s); }      // the middle of the button as it is on the screen, not of its place in units
    return mpress && hit && ! act;
}

// Several buttons joined into one plate, for a choice of one among several: cols x rows cells, the names row by row.
// The chosen cell is filled (ink with white text if its bit in invMask is set, else the "on" colour). Returns the cell
// that was clicked, or -1.
int ToboImagerEditor::segs (double x, double y, double w, double h, int cols, int rows, const char* const* names, int sel, int invMask, const double* colW, const double* textDx)
{
    const int lnp = (int) std::floor (LN * ZM + 0.5);
    box (x, y, w, h, CR, WHITE);
    const double ch = h / rows; int hit = -1;
    // left edge of column c in units from x: equal columns, or the widths given (they add up to w)
    auto cx = [&] (int c) { if (colW == nullptr) return w * c / cols; double v = 0; for (int i = 0; i < c; ++i) v += colW[i]; return v; };
    // edges of the cells in pixels: the outer frame, and a line of the one weight between two cells
    auto ex = [&] (int c) { return c == 0 ? ux (x) + lnp : c == cols ? ux (x + w) - lnp : ux (x + cx (c)) - lnp / 2; };
    auto ey = [&] (int r) { return r == 0 ? uy (y) + lnp : r == rows ? uy (y + h) - lnp : uy (y + r * ch) - lnp / 2; };
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
        {
            const int k = r * cols + c;
            const int x0 = ex (c) + (c == 0 ? 0 : lnp), x1 = ex (c + 1), y0 = ey (r) + (r == 0 ? 0 : lnp), y1 = ey (r + 1);
            const bool on = k == sel, inv = ((invMask >> k) & 1) != 0;
            if (on)
            {
                // the inside of the whole plate in the fill colour, but only within this cell: keeps the round outer corners
                g->clip (x0, y0, x1, y1); setc (inv ? INK : ONC); rrect (x + LN, y + LN, w - 2 * LN, h - 2 * LN, CR - LN); g->unclip();
            }
            g->font (f3); setc (on && inv ? WHITE : INK); tcap ((x0 + x1) / 2.0 + (textDx != nullptr ? std::floor (textDx[c] * ZM + 0.5) : 0.0), y0, y1, names[k]);      // in the middle of the cell as it is on the screen
            if (mpress && ! act && mxl >= x + cx (c) && mxl < x + cx (c + 1) && myl >= y + r * ch && myl < y + (r + 1) * ch) hit = k;
        }
    setc (INK);
    for (int c = 1; c < cols; ++c) g->rect (ex (c), uy (y), lnp, uy (y + h) - uy (y));
    for (int r = 1; r < rows; ++r) g->rect (ux (x), ey (r), ux (x + w) - ux (x), lnp);
    return hit;
}

//==============================================================================
// everything that stands still, drawn once per size
void ToboImagerEditor::drawStatic()
{
    const int iwid = (int) std::ceil (LW * ZM), ihgt = (int) std::ceil (LHT * ZM);
    stat.resize (iwid, ihgt); scr.resize (iwid, ihgt);
    g = &stat; ox = 0; oy = 0;
    f1 = juce::Font (juce::FontOptions ("Meiryo UI", (float) juce::jmax (11.0, std::floor (11 * ZM + 0.5)), juce::Font::plain));
    f3 = juce::Font (juce::FontOptions ("Meiryo UI", (float) juce::jmax (10.0, std::floor (9 * ZM + 0.5)), juce::Font::plain));
    capRows();
    setc (DESK); g->rect (0, 0, iwid, ihgt);
    const int lnp = (int) std::floor (LN * ZM + 0.5);
    // strip plate (its inside is drawn every frame)
    box (8 + 2 * CJOG, 8, 268, 126, CR, WHITE);
    // the four bands share one plate whose sides and section lines jog 45 degrees to the left
    setc (INK); rrect (PX0 + 2 * CJOG, BY0, PW, BYS + CJOG - BY0, CR); rrect (PX0, BYS - CJOG, PW, BY0 + 182 - BYS + CJOG, CR);
    setc (WHITE); rrect (PX0 + 2 * CJOG + LN, BY0 + LN, PW - 2 * LN, BYS + CJOG - BY0 - 2 * LN, CR - LN); rrect (PX0 + LN, BYS - CJOG + LN, PW - 2 * LN, BY0 + 182 - BYS + CJOG - 2 * LN, CR - LN);
    // the two corners that stick out are cut off along the slant, row by row
    setc (DESK);
    const int jogRows = uy (BYS + CJOG) - uy (BYS - CJOG);
    for (int k = 0; k < jogRows; ++k)
    {
        g->rect (ux (PX0) - 1, uy (BYS - CJOG) + k, ux (PX0 + 2 * CJOG) - k - ux (PX0) + 1, 1);
        g->rect (ux (PX0 + PW + 2 * CJOG) - k - 1, uy (BYS - CJOG) + k, k + 1, 1);    // from the pixel right of the slanted line (see jog())
    }
    for (int k = 0; k < NB; ++k)
    {
        g->font (f1); setc (INK);
        // heading, knob and value stand close together as one group (3.5 and 4.5 units between them, measured on the ink)
        // with about 9 units clear above and below it: upper row between the header and SAFE, lower row between SAFE and M / S
        const BandPos q = bandPos (k);
        tcen (q.wx, q.wy - q.r - 11, "WIDTH"); tcen (q.ax, q.ay - q.r - 11, "STEREOIZE");
    }
    setc (INK); frect (PX0 + 2 * CJOG, BY0 + 16, PW, LN);
    // the frame bars of the two overlapping boxes run two rows past the joints of the outer slants: take those ends out
    setc (WHITE);
    g->rect (ux (PX0 + 2 * CJOG), uy (BYS - CJOG), lnp, 2 * lnp);
    g->rect (ux (PX0 + PW) - lnp, uy (BYS + CJOG) - 2 * lnp, lnp, 2 * lnp);
    for (int k = 0; k <= NB; ++k)
    {
        const double x = colX (k);
        setc (INK);
        // upright bars in whole pixels, at the place the stroke of the jog continues them
        if (k > 0 && k < NB) { g->rect (ux (x + 2 * CJOG) - lnp / 2, uy (BY0), lnp, uy (BYS - CJOG) - uy (BY0)); g->rect (ux (x) - lnp / 2, uy (BYS + CJOG), lnp, uy (BY0 + 182) - uy (BYS + CJOG)); }
        const int w = k == 0 ? 0 : k == NB ? -lnp : -(lnp / 2);  // left pixel of the upright bar it joins, from ux()
        setc (INK); jog (ux (x + 2 * CJOG) + w, uy (BYS - CJOG), jogRows);
    }
    // monitor plate, lightly tinted, under the bands
    box (8, 332, 268, 38, CR, TINT);
    g->font (f1); setc (INK); tleft (16, 351, "MONITOR");
    // vectorscope and correlation
    box (308, 8, 148, 184, CR, WHITE);
    setc (PALE);
    if (proc.getOpt ("scopeMode", 0) == 0)
    {
        // between the buttons (down to 30) and the top of the CORR line (175.5): 145.5 units for a picture 124 high
        sline (ux (382), uy (41), ux (444), uy (103)); sline (ux (444), uy (103), ux (382), uy (165));
        sline (ux (382), uy (165), ux (320), uy (103)); sline (ux (320), uy (103), ux (382), uy (41));
        g->rect (ux (382) - lnp / 2, uy (41), lnp, uy (165) - uy (41)); g->rect (ux (320), uy (103) - lnp / 2, ux (444) - ux (320), lnp);
        g->font (f3); setc (INK); tleft (316, 47, "L"); tright (448, 47, "R");
    }
    else
    {
        // half disc: mono straight up, left and right at 45 degrees, out of phase along the floor
        const double R = 66 * ZM; const int cx = ux (382), cy = uy (136);
        aarc (cx, cy, R, -90, 90);
        g->rect (cx - (int) R, cy - lnp / 2, 2 * (int) R + 1, lnp);
        g->rect (cx - lnp / 2, cy - (int) R, lnp, (int) R);
        sline (cx, cy, cx - R * 0.7071, cy - R * 0.7071); sline (cx, cy, cx + R * 0.7071, cy - R * 0.7071);
        g->font (f3); setc (INK); tcen (382 - 53, 136 - 53, "L"); tcen (382 + 53, 136 - 53, "R");
    }
    box (308, 200, 148, 16, CR, WHITE);
    // logo, across the jog: an ink plate whose left side runs along the slant of the band plate.
    // The name is drawn on a spare image first, to find the box of its letters, then copied over as it is.
    // The name, in its own soft letters (see letter()). All strokes are laid out in pixels first; then every pixel of the
    // plate gets the share of it that the pen covers.
    {
        const int h = uy (BYS + LGH / 2) - uy (BYS - LGH / 2);                // height of the plate in pixels
        // one unit in pixels: the letters about 0.58 of the plate high, but the pen runs a whole number of rows from top
        // to foot (so the level strokes lie on rows of pixels and stay sharp), never less than 7 rows, and the pen is never
        // thinner than 1.5 pixels. On the smallest panel the letters are therefore a little larger for their plate.
        const double u = juce::jmax (7.0, std::floor (10 * 0.58 * h / (10 + PEN) + 0.5)) / 10, pen = juce::jmax (PEN * u, 1.5);
        const double H = 10 * u + pen;                                         // height of the letters, pen included
        static const char* const name = "TOBO IMAGER";
        std::vector<Stroke> strokes; double adv = 0;
        for (const char* c = name; *c; ++c)
        {
            if (*c == ' ') { adv += LSPACE; continue; }
            std::vector<Stroke> gl; const double w = letter (*c, gl);
            for (auto& st : gl) { for (auto& q : st) { q.x = (q.x + adv + (10 - q.y) * LEAN) * u; q.y *= u; } strokes.push_back (st); }
            adv += w + PEN + LGAP;
            // pairs whose shapes leave a hole between them are set closer
            if (c[0] == 'A' && c[1] == 'G') adv -= 1.5;
            if (c[0] == 'M' && c[1] == 'A') adv -= 0.6;
            if (c[0] == 'T' && c[1] == 'O') adv -= 0.5;
        }
        adv -= LGAP + PEN;                                                     // now: from the middle of the first stem to the middle of the last
        const int lw = (int) std::ceil ((adv + 10 * LEAN) * u + pen);
        const double pad = (h - H) / 2.0;                                      // space above and below the letters
        const double lx = ox + (276 + CJOG + LGH / 2 + 8 * std::sqrt (2.0)) * ZM;   // left end of the top edge
        const int tx = (int) std::floor (lx + pad * 0.414 + 0.5);              // the letters start the same space away from the slant
        const int xr = tx + lw + (int) std::floor (pad + 0.5);                 // right end of the plate
        const double rad = juce::jmin (std::floor (CR * ZM + 0.5), h / 2.0 - 1);
        setc (INK);
        setrq (std::ceil (lx), xr - 1, uy (BYS - LGH / 2), uy (BYS - LGH / 2) + h - 1, rad, false);
        fillrq ((int) std::ceil (lx) - h - 2, uy (BYS - LGH / 2), xr, uy (BYS - LGH / 2) + h - 1, rad, 0);
        // the four band colours as four small plates in the rest of the row; widths 4 : 3 : 2.5 : 2
        double x = (xr - ox) / ZM + 3; const double w = (456 - x - 9) / 11.5;
        for (int k = 0; k < 4; ++k) { const double hh = k == 0 ? 4 : k == 1 ? 3 : k == 2 ? 2.5 : 2; box (x, BYS - LGH / 2, w * hh, LGH, 3, bcol[k]); x += w * hh + 3; }
        setc (YEL);
        const int y0 = uy (BYS - LGH / 2);
        const double oxp = tx + pen * 0.5 - 10 * LEAN * u * 0.0, oyp = y0 + std::floor ((h - 10 * u - 1) / 2 + 0.5) + 0.5;     // where the unit (0, 0) lands: the middle of a row of pixels
        for (int yy = y0; yy < y0 + h; ++yy)
            for (int xx = tx - 2; xx < tx + lw + 2; ++xx)
            {
                const double fx0 = xx + 0.5 - oxp, fy0 = yy + 0.5 - oyp; double d2 = 1e18;
                for (const auto& st : strokes)
                    for (size_t k = 0; k + 1 < st.size(); ++k)
                    {
                        const double ex = st[k + 1].x - st[k].x, ey = st[k + 1].y - st[k].y, wx = fx0 - st[k].x, wy = fy0 - st[k].y;
                        const double l2 = ex * ex + ey * ey, t = l2 > 0 ? juce::jlimit (0.0, 1.0, (wx * ex + wy * ey) / l2) : 0;
                        d2 = juce::jmin (d2, sqr (wx - ex * t) + sqr (wy - ey * t));
                    }
                // a thin pen gets a harder edge, or small letters turn to mush
                const double cv = (pen * 0.5 - std::sqrt (d2)) * (pen < 2 ? 1.6 : 1.0) + 0.5;
                if (cv > 0.02) { g->a = juce::jmin (1.0, cv); g->rect (xx, yy, 1, 1); }
            }
        g->a = 1;
    }
    // SAFE button: a plate whose two sides slant like the lines of the band plate. Drawn once for "off" and "on".
    sfh = uy (BY0 + 95) - uy (BY0 + 81); sfw = (int) std::floor ((67 - 16 * std::sqrt (2.0)) * ZM + 0.5); sfiw = sfw + sfh + 2;
    for (int k = 0; k < 2; ++k)
    {
        g = k == 0 ? &safeOff : &safeOn;
        g->resize (sfiw, sfh); setc (WHITE); g->rect (0, 0, sfiw, sfh);
        setrq (sfh, sfh + sfw - 1, 0, sfh - 1, std::floor (CR * ZM + 0.5), true);
        setc (INK); fillrq (0, 0, sfiw - 1, sfh - 1, std::floor (CR * ZM + 0.5), 0);
        setc (k == 0 ? WHITE : ONC); fillrq (0, 0, sfiw - 1, sfh - 1, std::floor (CR * ZM + 0.5), std::floor (LN * ZM + 0.5));
    }
    g = &stat;
    // stereoize type plate
    box (284, 244, 172, 126, CR, WHITE);
    g->font (f1); setc (INK); tleft (292, 257, "STEREOIZE TYPE");
    // dot meter labels
    g->font (f3); setc (INK); tcen (11, 206, "I"); tcen (21, 206, "O");
    g = &scr;
}

//==============================================================================
// one frame: the moving parts, and the mouse
void ToboImagerEditor::frame()
{
    auto& e = proc.engine;
    ZM = zooms[proc.getGuiSize()] * scale;
    LN = juce::jmax (1.0, std::floor (ZM + 0.49)) / ZM;                // a half goes down: at 1.5x one pixel (two looked heavy on small parts)                 // the one line weight: 1 unit, a whole number of pixels
    NB = juce::jlimit (1, 4, (int) std::floor (sget (30) + 0.5) + 1);
    // one band alone would stand in a plate four times too wide for it: its plate is half as wide, in the middle
    PX0 = NB == 1 ? 8 + 67 : 8; PW = NB == 1 ? 134 : 268;
    if (ZM != lastzm || NB != lastNB) { drawStatic(); lastzm = ZM; lastNB = NB; }      // (lastzm is set to 0 to ask for a redraw)
    const int lnp = (int) std::floor (LN * ZM + 0.5);
    // when no audio has been processed for a quarter of a second, the meters, the scope and the spectrum go back to empty
    const unsigned c = e.smpc.load (std::memory_order_relaxed);
    // (a host that runs the plugin bypassed says so at once; one that just stops calling it is noticed after 0.15 s)
    if (c != glastc) { glastc = c; galive = now(); gidle = false; }
    else if (! gidle && (proc.bypassed.load() || now() - galive > 0.15))
    {
        gidle = true;
        std::fill (std::begin (e.scl), std::end (e.scl), 0.0f); std::fill (std::begin (e.scr), std::end (e.scr), 0.0f);
        std::fill (std::begin (e.sgl), std::end (e.sgl), 0.0f); std::fill (std::begin (e.sgr), std::end (e.sgr), 0.0f);
        e.imm = 0; e.iss = 0; e.omm = 0; e.oss = 0; e.sll = 0; e.srr = 0; e.slr = 0; e.clearMeters = true;
    }
    ox = 0; oy = 0; g = &scr;
    g->a = 1; g->blit (stat, 0, 0, stat.width(), stat.height(), 0, 0);
    // In the frame that sees a press, the mouse counts as standing where the button went down. The frames come 30 times
    // a second and the mouse may have moved on in between: without this a quick drag would miss the control it started
    // on, and a drag would not know where it began.
    const double realX = mouse_x, realY = mouse_y;
    if (pressed) { mouse_x = pressX; mouse_y = pressY; }
    mxl = (mouse_x - ox) / ZM; myl = (mouse_y - oy) / ZM;
    mdown = held || pressed; mpress = pressed; pressed = false;
    if (! mdown && act) { endGesture(); act = 0; }

    // ---- dot meter: side against mid, going in (left) and coming out (right). Top dot +6 dB, 3 dB a dot ----
    const double imm = e.imm.load(), iss = e.iss.load(), omm = e.omm.load(), oss = e.oss.load();
    const double gdi = imm > 1e-10 ? 10 * std::log10 ((iss + 1e-12) / imm) : -200;
    const double gdo = omm > 1e-10 ? 10 * std::log10 ((oss + 1e-12) / omm) : -200;
    for (int k = 0; k < 19; ++k)
    {
        const int gcy = uy (13 + 10 * k); const double grd = std::floor (3 * ZM + 0.5);
        setc (INK); g->circle (ux (11), gcy, grd); g->circle (ux (21), gcy, grd);
        if (gdi < 6 - 3 * k) { setc (WHITE); g->circle (ux (11), gcy, grd - lnp); }
        if (gdo < 6 - 3 * k) { setc (WHITE); g->circle (ux (21), gcy, grd - lnp); }
    }

    // ---- strip: for each band a block as high as its WIDTH, a line at its STEREOIZE, and the spectrum over it ----
    double gf[3] = { sget (1), sget (2), sget (3) };
    const int gsolo = (int) std::floor (sget (16) + 0.5);
    // The handles are worked first, before anything that depends on the crossovers is drawn: the coloured blocks of the
    // bands, the lines and the labels then all show the same frequencies in the same frame.
    // crossover handles
    // Which handle a press takes: the nearest one within 6 units. The mouse keeps the spot of the handle it pressed on
    // (grabOff), so a press alone moves nothing. Handles that stand on one spot (less than a unit apart) cannot be told
    // apart by the press: the choice waits for the first move (act == 30). To the right it is the upper one, to the
    // left the lower one, so two handles that were pushed together can always be pulled apart again.
    // the plate with the number of bands lies over the display: a press on it is not a press on a handle behind it
    g->font (f3); double bpw, bph; g->measure ("BANDS", bpw, bph); bpw = std::floor (bpw / ZM + 0.5);
    const double bpx = 8 + 2 * CJOG + 6, bpcw = 12, bpend = 2, bplw = bpw + 5, bpx1 = bpx + bplw + 4 * bpcw + 2 * bpend;      // the name, then the plate of four cells
    // (the two end cells are bpend wider: their round ends take room away from the number, which then looks off the middle)
    const bool onBandsPlate = mxl >= bpx && mxl < bpx1 && myl >= 12 && myl < 24;
    bandsCellX = bpx + bplw;
    if (mpress && ! act && myl >= 8 && myl < 134 && ! onBandsPlate)
    {
        int best = -1; double bd = 6.0001; grabMask = 0;
        // A press on the label of a handle takes that handle, wherever the label stands: at the ends of the display and
        // beside another label it is not under its line. (The labels are where the last frame drew them.)
        for (int k = 0; k < NB - 1; ++k)
            if (best < 0 && myl >= labelY[k] && myl < labelY[k] + 12 && mxl >= labelX[k] && mxl < labelX[k] + labelW[k])
            {
                best = k;
                for (int j = 0; j < NB - 1; ++j) if (std::abs (fx (gf[j]) - fx (gf[k])) < 1.0) grabMask |= 1 << j;
            }
        if (best < 0)
            for (int k = 0; k < NB - 1; ++k) { const double d = std::abs (mxl - fx (gf[k])); if (d <= 6) grabMask |= 1 << k; if (d < bd) { bd = d; best = k; } }
        // crossovers that a host has put out of order: the first press on a handle puts them in order (the engine uses
        // them in rising order anyway), and takes nothing; the next press finds every handle where its band is
        bool sorted = true; for (int k = 1; k < NB - 1; ++k) sorted = sorted && gf[k - 1] <= gf[k];
        if (best >= 0 && ! sorted)
        {
            double t[3] = { gf[0], gf[1], gf[2] }; std::sort (t, t + (NB - 1));
            for (int k = 0; k < NB - 1; ++k) if (t[k] != gf[k]) { gf[k] = t[k]; sset (k + 1, gf[k]); }
            best = -1; grabMask = 0;
        }
        if (best >= 0)
        {
            int lo = best, hi = best;
            for (int k = 0; k < NB - 1; ++k) if (std::abs (fx (gf[k]) - fx (gf[best])) < 1.0) { lo = juce::jmin (lo, k); hi = juce::jmax (hi, k); }
            grabOff = mxl - fx (gf[best]);
            if (lo == hi) act = 31 + best; else { act = 30; pendLo = lo; pendHi = hi; pendX = mxl; }
        }
    }
    if (act == 30 && std::abs (mxl - pendX) >= 0.5) act = 31 + (mxl > pendX ? pendHi : pendLo);
    // the handle in hand follows the mouse, and the others make room
    for (int k = 0; k < NB - 1; ++k)
        if (act == 31 + k)
        {
            const int nh1 = NB - 1;
            double gnf = std::floor (20 * std::pow (1000.0, (mxl - grabOff - 33) / 266) + 0.5);      // whole Hz, so that "no move" is exactly that
            if (gnf == gf[k]) break;
            // how low and how high each handle can stand when all of them keep half an octave from each other and from
            // the two ends of the range
            double mn[3], mx[3];
            for (int q = 0; q < nh1; ++q) mn[q] = std::ceil ((q > 0 ? mn[q - 1] : 20.0) * XGAP);
            for (int q = nh1 - 1; q >= 0; --q) mx[q] = std::floor ((q < nh1 - 1 ? mx[q + 1] : 20000.0) / XGAP);
            double nf[3] = { gf[0], gf[1], gf[2] };
            nf[k] = juce::jlimit (mn[k], mx[k], gnf);
            // Going up, the ones above are pushed up as far as needed; going down, the ones below are pushed down. The
            // handles on the other side are left alone, also when they stand closer than half an octave (a host or a
            // saved state can put them so): a handle nobody touched does not jump away.
            if (nf[k] > gf[k]) for (int q = k + 1; q < nh1; ++q) nf[q] = juce::jlimit (mn[q], mx[q], juce::jmax (nf[q], std::ceil (nf[q - 1] * XGAP)));
            if (nf[k] < gf[k]) for (int q = k - 1; q >= 0; --q) nf[q] = juce::jlimit (mn[q], mx[q], juce::jmin (nf[q], std::floor (nf[q + 1] / XGAP)));
            for (int q = 0; q < nh1; ++q)
                if (nf[q] != gf[q]) { gf[q] = nf[q]; if (q != k) sset (q + 1, nf[q]); }
            if (proc.params[k]->convertFrom0to1 (proc.params[k]->getValue()) != (float) nf[k]) sset (k + 1, nf[k], true);
            break;
        }
    for (int k = 0; k < NB; ++k)
    {
        const double gx0 = k == 0 ? 8 + 2 * CJOG + LN : fx (gf[k - 1]);
        const double gx1 = k == NB - 1 ? 300 - LN : fx (gf[k]);
        const bool gon = sget (18 + k) > 0.5;
        if (gx1 > gx0)
        {
            // a band that is switched off, muted, or not the soloed one, is shown pale
            setc (! gon || (gsolo > 0 ? gsolo != k + 1 : sget (22 + k) > 0.5) ? mixc (bc (k), WHITE, 0.75) : bc (k));
            const double gbh = juce::jmax (CR, sget (4 + k) / 200 * 116);
            // the two end blocks follow the round bottom corners of the plate
            if (NB == 1) { rrect (gx0, (134 - LN) - gbh, gx1 - gx0, gbh, CR - LN); frect (gx0, (134 - LN) - gbh, gx1 - gx0, gbh - CR); }
            else if (k == 0) { rrect (gx0, (134 - LN) - gbh, juce::jmin (2 * CR, gx1 - gx0), gbh, CR - LN); frect (gx0, (134 - LN) - gbh, juce::jmin (2 * CR, gx1 - gx0), gbh - CR); if (gx1 - gx0 > CR) frect (gx0 + CR, (134 - LN) - gbh, gx1 - gx0 - CR, gbh); }
            else if (k == NB - 1) { rrect (juce::jmax (gx0, gx1 - 2 * CR), (134 - LN) - gbh, juce::jmin (2 * CR, gx1 - gx0), gbh, CR - LN); frect (juce::jmax (gx0, gx1 - 2 * CR), (134 - LN) - gbh, juce::jmin (2 * CR, gx1 - gx0), gbh - CR); if (gx1 - gx0 > CR) frect (gx0, (134 - LN) - gbh, gx1 - gx0 - CR, gbh); }
            else frect (gx0, (134 - LN) - gbh, gx1 - gx0, gbh);
            if (gon && sget (8 + k) > 0) { setc (INK, STLINE); frect (gx0, (134 - LN) - juce::jmax (CR, sget (8 + k) / 100 * 116) - LN / 2, gx1 - gx0, LN); g->a = 1; }      // a faint line: it must not look like a part of the frame
        }
    }
    // spectrum of the output: mid as a line, side as dots. 3 dB per octave tilt, top of the strip = -12 dB, bottom = -96 dB
    {
        const int SPN = Engine::SPN; const int gsp = e.sgw.load (std::memory_order_relaxed);
        for (int k = 0; k < SPN; ++k)
        {
            const double gwn = 0.5 - 0.5 * std::cos (2 * kPi * k / SPN);
            const double gl = e.sgl[(gsp + k) % SPN], gr = e.sgr[(gsp + k) % SPN];
            fbm[(size_t) (2 * k)] = (gl + gr) * 0.5 * gwn; fbm[(size_t) (2 * k + 1)] = 0; fbs[(size_t) (2 * k)] = (gl - gr) * 0.5 * gwn; fbs[(size_t) (2 * k + 1)] = 0;
        }
        tobo::fft (fbm.data(), SPN, false); tobo::fft (fbs.data(), SPN, false);
        const double srate = e.srate;
        const int gnp = juce::jmin (1022, (int) std::floor ((266 * ZM) / 3));
        for (int k = 0; k <= gnp; ++k)
        {
            const double f = 20 * std::pow (1000.0, (double) k / gnp);
            const int gb0 = juce::jmax (1, (int) std::floor (f * SPN / srate + 0.5));
            const int gb1 = juce::jmin (SPN / 2, juce::jmax (gb0 + 1, (int) std::floor (20 * std::pow (1000.0, (double) (k + 1) / gnp) * SPN / srate + 0.5)));
            double gpm = 0, gps = 0;
            for (int b = gb0; b < gb1; ++b) { gpm = juce::jmax (gpm, sqr (fbm[(size_t) (2 * b)]) + sqr (fbm[(size_t) (2 * b + 1)])); gps = juce::jmax (gps, sqr (fbs[(size_t) (2 * b)]) + sqr (fbs[(size_t) (2 * b + 1)])); }
            const double gtl = 3 * std::log (f / 1000) / std::log (2.0) - 20 * std::log10 (SPN / 4.0);
            double gdm = 10 * std::log10 (gpm + 1e-16) + gtl, gds = 10 * std::log10 (gps + 1e-16) + gtl;
            gdm = juce::jlimit (0.0, 1.0, (gdm + 96) / 84); gds = juce::jlimit (0.0, 1.0, (gds + 96) / 84);
            spym[k] += (gdm - spym[k]) * (gdm > spym[k] ? 0.6 : 0.15); spys[k] += (gds - spys[k]) * (gds > spys[k] ? 0.6 : 0.15);
        }
        auto px = [&] (int k) { return ux (33) + k * 266 * ZM / gnp; };
        auto pm = [&] (int k) { return uy (132) - spym[k] * 118 * ZM; };
        // side first, as a tint of ink over the bands: every pixel column from the curve down to the floor of the strip
        {
            setc (INK, SIDEA);
            const int xa = ux (33), xb = ux (33 + 266), yb = uy (134 - LN);
            for (int xx = xa; xx < xb; ++xx)
            {
                const double t = (xx - xa) * (double) gnp / (266 * ZM); const int k = juce::jlimit (0, gnp - 1, (int) t);
                const double v = spys[k] + (spys[k + 1] - spys[k]) * (t - k);
                const int yt = (int) std::floor (uy (132) - v * 118 * ZM + 0.5);
                if (v > 0.005 && yb > yt) g->rect (xx, yt, 1, yb - yt);
            }
            g->a = 1;
        }
        setc (INK);
        // (not where it lies on the floor: in silence it would run along the bottom of the display and over its round corner)
        for (int k = 0; k < gnp; ++k) if (spym[k] > 0.004 || spym[k + 1] > 0.004) sline (px (k), pm (k), px (k + 1), pm (k + 1));
    }
    juce::String glb[3]; double glw[3], glx[3];      // the labels of the handles: name, width, left edge
    for (int k = 0; k < NB - 1; ++k)
    {
        double f = gf[k], ghx = fx (f);
        setc (INK); g->rect (ux (ghx) - lnp / 2, uy (8), lnp, uy (134) - uy (8));
        glb[k] = f >= 1000 ? juce::String (f / 1000, 1) + "k" : juce::String ((int) f);
        g->font (f3); double gth; g->measure (glb[k], glw[k], gth); glw[k] = std::floor (glw[k] / ZM + 0.5) + 8;
        glx[k] = juce::jmin (300 - LN - 1 - glw[k], juce::jmax (8 + 2 * CJOG + LN + 1, ghx - glw[k] / 2));
    }
    // Every label stands under its own line (only the edges of the display move it a little). Handles half an octave
    // apart are closer together than a label is wide: where a label would lie on one before it, it goes a row up.
    const int nh = NB - 1; double gly[3] = { 116, 116, 116 };
    for (int k = 1; k < nh; ++k)
        for (int row = 0; row < 3; ++row)
        {
            gly[k] = 116 - 13 * row; bool clear = true;
            for (int q = 0; q < k; ++q) clear = clear && ! (gly[q] == gly[k] && glx[q] < glx[k] + glw[k] + 1 && glx[k] < glx[q] + glw[q] + 1);
            if (clear) break;
        }
    for (int k = 0; k < 3; ++k) { labelX[k] = k < nh ? glx[k] : -1000; labelW[k] = k < nh ? glw[k] : 0; labelY[k] = k < nh ? gly[k] : -1000; }
    for (int k = 0; k < nh; ++k)
    {
        box (glx[k], gly[k], glw[k], 12, CR, act == 31 + k || (act == 30 && k >= pendLo && k <= pendHi) ? ONC : WHITE);
        g->font (f3); setc (INK); tcen (glx[k] + glw[k] / 2, gly[k] + 6, glb[k]);
    }
    // how many bands: a small plate in the top left corner of the display, the number chosen like the other choices.
    // With fewer bands the upper ones go: band 1 .. n keep their settings. A crossover that comes back into use is put
    // above the one below it if it was not there already, and a solo on a band that goes is taken off.
    {
        // the name stands on the display itself, the four numbers on a plate. The name is white where the dark blue
        // block of the first band has come up behind it (its WIDTH near 200 %), as on the header of that band.
        static const char* const nums[4] = { "1", "2", "3", "4" };
        const bool dark = bc (0) == BLUE && sget (18) > 0.5 && (gsolo > 0 ? gsolo == 1 : sget (22) < 0.5) && (134 - LN) - sget (4) / 200 * 116 < 21 && fx (NB > 1 ? gf[0] : 20000.0) > bpx + bplw * 0.5;
        g->font (f3); setc (dark ? WHITE : INK); tleft (bpx, 18, "BANDS");
        const double cw4[4] = { bpcw + bpend, bpcw, bpcw, bpcw + bpend };
        // the numbers of the two end cells stand one unit towards the inside, away from the round ends (chosen by eye:
        // a number in the middle of an end cell looks pushed towards its round end). That is half of what the cells were
        // widened by, so these numbers are as far from the line between the cells as the numbers in the middle.
        const double dx4[4] = { bpend / 2, 0, 0, -bpend / 2 };
        const int c = segs (bpx + bplw, 12, 4 * bpcw + 2 * bpend, 12, 4, 1, nums, NB - 1, 0, cw4, dx4);
        if (c >= 0 && c != NB - 1)
        {
            const int n = c + 1;
            sset (30, n - 1);
            if (gsolo > n) sset (16, 0);
            // the crossovers that come into use: each at least a third of an octave above the one below it (half way
            // up to 20 kHz, in octaves, if it was not); then, from the top down, each far enough below the one above
            // it and below 20 kHz, which moves the lower ones down where there is no room
            double nf[3] = { gf[0], gf[1], gf[2] };
            for (int k = juce::jmax (1, NB - 1); k < n - 1; ++k)
                if (nf[k] < nf[k - 1] * XGAP) nf[k] = std::sqrt (nf[k - 1] * 20000.0);
            if (n > NB)
                for (int k = n - 2; k >= 0; --k)
                    nf[k] = juce::jmax (std::ceil (20.0 * XGAP), juce::jmin (nf[k], std::floor ((k < n - 2 ? nf[k + 1] : 20000.0) / XGAP)));
            for (int k = 0; k < n - 1; ++k) { nf[k] = std::floor (nf[k] + 0.5); if (nf[k] != gf[k]) { gf[k] = nf[k]; sset (k + 1, gf[k]); } }
        }
    }
    // what the two curves are: a short piece of each, drawn the way the curve is drawn, with its name
    {
        g->font (f3); double wm, ws, th; g->measure ("MID", wm, th); g->measure ("SIDE", ws, th);
        wm = std::floor (wm / ZM + 0.5); ws = std::floor (ws / ZM + 0.5);
        const double sl = 12, gtw = 6 + sl + 3 + wm + 8 + sl + 3 + ws + 6, x0 = 300 - 4 - gtw;
        box (x0, 12, gtw, 12, CR, WHITE);
        double x = x0 + 6;
        setc (INK); sline (ux (x), uy (18), ux (x + sl), uy (18)); x += sl + 3;
        setc (INK); tleft (x, 18, "MID"); x += wm + 8;
        setc (INK, SIDEA); g->rect (ux (x), uy (15), ux (x + sl) - ux (x), uy (21) - uy (15)); g->a = 1;
        x += sl + 3;
        setc (INK); tleft (x, 18, "SIDE");
    }
    // dashed lines across the gap, from each crossover of the strip to the line between the same two bands below
    setc (INK);
    for (int k = 0; k < NB - 1; ++k)
    {
        // from the middle of the handle line to the middle of the bar below (both lnp pixels wide from ux() - lnp / 2)
        const double mid = (lnp - 1) * 0.5 - lnp / 2;
        const double gxa = ux (fx (gf[k])) + mid, gxb = ux (colX (k + 1) + 2 * CJOG) + mid;
        const double gya = uy (134) + 1, gyb = uy (BY0) - 2, gln = std::sqrt (sqr (gxb - gxa) + sqr (gyb - gya));
        const int gnd = juce::jmax (1, (int) std::floor ((gln + 3) / 6)); const double gdl = juce::jmax (1.0, (gln - (gnd - 1) * 3) / gnd);
        for (int b = 0; b < gnd; ++b)
        {
            const double t0 = b * (gdl + 3) / gln, t1 = (b * (gdl + 3) + gdl) / gln;
            aaseg (gxa + (gxb - gxa) * t0, gya + (gyb - gya) * t0, gxa + (gxb - gxa) * t1, gya + (gyb - gya) * t1);
        }
    }

    // ---- bands ----
    for (int k = 0; k < NB; ++k)
    {
        const double gx = (colX (k) + colX (k + 1)) / 2;
        const bool gon = sget (18 + k) > 0.5;
        // header: the band colour when the band is on, white when it is off. A click anywhere on it switches the band
        // from the pixel after the bar on its left to the pixel before the bar on its right (the bars stand at ux() - lnp / 2)
        const BandPos q = bandPos (k);
        const double gx0 = (k == 0 ? ux (PX0 + 2 * CJOG) + lnp : ux (colX (k) + 2 * CJOG) - lnp / 2 + lnp) / ZM, gx1 = (k == NB - 1 ? ux (PX0 + PW + 2 * CJOG) - lnp : ux (colX (k + 1) + 2 * CJOG) - lnp / 2) / ZM;
        setc (gon ? bc (k) : WHITE);
        if (NB == 1) { rrect (gx0, BY0 + LN, gx1 - gx0, 10, CR - LN); frect (gx0, BY0 + 6, gx1 - gx0, 10); }
        else if (k == 0) { rrect (gx0, BY0 + LN, 12, 10, CR - LN); frect (gx0 + 4, BY0 + LN, gx1 - gx0 - 4, 16 - LN); frect (gx0, BY0 + 6, 8, 10); }
        else if (k == NB - 1) { rrect (gx1 - 12, BY0 + LN, 12, 10, CR - LN); frect (gx0, BY0 + LN, gx1 - gx0 - 4, 16 - LN); frect (gx1 - 8, BY0 + 6, 8, 10); }
        else frect (gx0, BY0 + LN, gx1 - gx0, 16 - LN);
        setc (! gon ? PALE : bc (k) == BLUE ? WHITE : INK);
        aarc (ux (gx0 + 8), uy (BY0 + 9), 3.5 * ZM, -145, -35); aarc (ux (gx0 + 8), uy (BY0 + 9), 3.5 * ZM, -180, -145); aarc (ux (gx0 + 8), uy (BY0 + 9), 3.5 * ZM, 35, 180);
        g->rect (ux (gx0 + 8) - lnp / 2, uy (BY0 + 3.5), lnp, uy (BY0 + 8.5) - uy (BY0 + 3.5));   // stem: a whole number of pixels at every size
        g->font (f1); setc (gon && bc (k) == BLUE ? WHITE : INK);
        tcen (gx + 2 * CJOG + 5, BY0 + 8.5, bnameN[NB - 1][k]);
        if (mpress && ! act && mxl >= gx0 && mxl < gx1 && myl >= BY0 && myl < BY0 + 16) sset (18 + k, gon ? 0 : 1);
        // the WIDTH knob carries the colour of the band, like the block of the strip whose height it sets;
        // the STEREOIZE knob is white with an ink pointer, like the line it sets on the strip
        knob (4 + k, 4 + k, q.wx, q.wy, q.r, 0, 200, 100, 1, gon ? bc (k) : WHITE);
        g->font (f3); setc (INK); tcen (q.wx, q.wy + q.r + 7.5, juce::String ((int) std::floor (sget (4 + k) + 0.5)) + "%");
        // safe: in the jog between the two rows; the plate was drawn once, here it is copied row by row and labelled
        const int gsx = ux (q.sx - 33.5 + 2 * CJOG - (95 - 76) + 8 * std::sqrt (2.0)) - 3;
        const int gsy = uy (q.sy);
        const bool safeIsOn = sget (26 + k) > 0.5;
        for (int r = 0; r < sfh; ++r)
            g->blit (safeIsOn ? safeOn : safeOff, juce::jmax (0, sfh - r - 3), r, juce::jmin (sfiw, sfw + 5), 1, gsx + juce::jmax (0, sfh - r - 3), gsy + r);
        g->font (f3); setc (INK);
        { const int lp = (int) std::floor (LN * ZM + 0.5); tcap (gsx + sfiw / 2.0, gsy + lp, gsy + sfh - lp, "SAFE"); }
        // a click counts between the two slanted sides
        const double gsr = mouse_y - gsy, gsc = mouse_x - gsx + gsr;
        if (mpress && ! act && gsr >= 0 && gsr < sfh && gsc >= sfh && gsc < sfh + sfw) sset (26 + k, safeIsOn ? 0 : 1);
        knob (8 + k, 8 + k, q.ax, q.ay, q.r, 0, 100, 0, 1, WHITE);
        g->font (f3); setc (INK); tcen (q.ax, q.ay + q.r + 7.5, juce::String ((int) std::floor (sget (8 + k) + 0.5)) + "%");
        // mute (ink when on) and solo; a solo is heard even if the band is muted
        if (button (q.mx - 24, q.my, 20, 14, "M", sget (22 + k) > 0.5, true)) sset (22 + k, sget (22 + k) > 0.5 ? 0 : 1);
        if (button (q.mx + 4, q.my, 20, 14, "S", gsolo == k + 1, false)) sset (16, gsolo == k + 1 ? 0 : k + 1);
    }

    // ---- monitor ----
    const int gmon = (int) std::floor (sget (17) + 0.5);
    {
        // the three choices as one plate; a click on MID or SIDE while it is chosen goes back to STEREO
        static const char* const mons[4] = { "STEREO", "MID", "SIDE", "DELTA" };
        const int c = segs (98, 344, 170, 14, 4, 1, mons, gmon, 14);
        if (c >= 0) sset (17, c == gmon ? 0 : c);
    }

    // ---- stereoize type: one row of four as one plate, its own knob and the guard under it ----
    const int gtyp = (int) std::floor (sget (13) + 0.5);
    g->font (f3); setc (INK);
    tright (448, 257, gtyp == 3 ? "LATENCY " + juce::String ((int) std::floor (e.fvnLatency() * 1000.0 / e.srate + 0.5)) + " ms" : juce::String ("LATENCY 0 ms"));
    {
        // four equal parts in the order of the types (the two velvet names are shortened to fit), so DELAY stands at the left
        // above its knob and FVN at the right
        static const char* const types[4] = { "DELAY", "VEL A", "VEL B", "FVN" };
        const int c = segs (292, 269, 156, 14, 4, 1, types, gtyp, 0);
        if (c >= 0) sset (13, c);
    }
    const double kdy = -12;      // heading, knob and value in the middle between the row and the bottom of the plate (20 units either side)
    // three knobs side by side; the one that the chosen type does not use is pale and cannot be turned
    {
        const bool de = gtyp == 0, se = gtyp == 3;
        g->font (f1); setc (de ? INK : DIS); tcen (318, 318 + kdy, "DELAY");
        knob (12, 12, 318, 340 + kdy, 11, 2, 30, 12, 0.1, WHITE, de);
        g->font (f3); setc (de ? INK : DIS); tcen (318, 358.5 + kdy, juce::String (sget (12), 1) + " ms");
        g->font (f1); setc (se ? INK : DIS); tcen (370, 318 + kdy, "SCATTER");
        knob (15, 15, 370, 340 + kdy, 11, 0, 100, 25, 1, WHITE, se);
        g->font (f3); setc (se ? INK : DIS); tcen (370, 358.5 + kdy, juce::String ((int) std::floor (sget (15) + 0.5)) + "%");
        g->font (f1); setc (INK); tcen (422, 318 + kdy, "GUARD");
        knob (14, 14, 422, 340 + kdy, 11, 0, 100, 0, 1, WHITE);
        g->font (f3); setc (INK); tcen (422, 358.5 + kdy, juce::String ((int) std::floor (sget (14) + 0.5)) + "%");
    }

    // ---- stereo meter: three kinds, chosen with the buttons along the top of its plate ----
    {
        static const char* const kinds[3] = { "GONIO", "POLAR", "LEVEL" };
        const int mode = juce::jlimit (0, 2, proc.getOpt ("scopeMode", 0));
        const int c = segs (316, 16, 132, 14, 3, 1, kinds, mode, 0);
        if (c >= 0 && c != mode) { proc.setOpt ("scopeMode", c); lastzm = 0; }
    }
    double gpk = 0.0001, gcorr = 1;
    for (int k = 0; k < Engine::SCN; ++k) gpk = juce::jmax (gpk, (double) juce::jmax (std::abs (e.scl[k]), std::abs (e.scr[k])));
    if (gpk > 0.0002)
    {
        // the picture is scaled to the loudest point: down at once, up slowly, and left alone in silence
        const double gtg = 1 / gpk; gsg = (gtg < gsg || gsg <= 0) ? gtg : gsg + (gtg - gsg) * 0.1;
        gcorr = e.slr.load() / std::sqrt ((double) e.sll.load() * e.srr.load() + 1e-13);
        setc (INK);
        const int gps = juce::jmax (1, (int) std::floor (ZM * 0.5 + 0.5));
        const int mode = proc.getOpt ("scopeMode", 0);
        if (mode == 0)
        {
            for (int k = 0; k < Engine::SCN; ++k)
            {
                const double gl = juce::jlimit (-1.0, 1.0, e.scl[k] * gsg), gr = juce::jlimit (-1.0, 1.0, e.scr[k] * gsg);
                g->rect (ux (382 + (gr - gl) * 0.5 * 60), uy (103 - (gl + gr) * 0.5 * 60), gps, gps);
            }
        }
        else if (mode == 1)
        {
            // polar sample: the same dots, those below the floor turned round the centre into the upper half
            for (int k = 0; k < Engine::SCN; ++k)
            {
                double x = (e.scr[k] - e.scl[k]) * 0.5 * gsg, y = (e.scl[k] + e.scr[k]) * 0.5 * gsg;
                if (y < 0) { x = -x; y = -y; }
                const double m = std::sqrt (x * x + y * y); if (m > 1) { x /= m; y /= m; }
                g->rect (ux (382 + x * 64), uy (136 - y * 64), gps, gps);
            }
        }
        else
        {
            // polar level: one ray for each short piece of the sound; its angle is where the piece sits, its length how loud it is
            const int N = 32, per = Engine::SCN / N; const int cx = ux (382), cy = uy (136);
            for (int c = 0; c < N; ++c)
            {
                double sx = 0, sy = 0, en = 0;
                for (int k = c * per; k < (c + 1) * per; ++k)
                {
                    double x = (e.scr[k] - e.scl[k]) * 0.5 * gsg, y = (e.scl[k] + e.scr[k]) * 0.5 * gsg;
                    if (y < 0) { x = -x; y = -y; }
                    const double m = std::sqrt (x * x + y * y);
                    sx += x * m; sy += y * m; en += m * m;      // louder samples count more for the angle
                }
                const double len = juce::jmin (1.0, std::sqrt (en / per) * 1.6), d = std::sqrt (sx * sx + sy * sy);
                if (d > 1e-9 && len > 0.02) aaseg (cx, cy, cx + sx / d * len * 64 * ZM, cy - sy / d * len * 64 * ZM);
            }
        }
    }
    gcorr = juce::jlimit (-1.0, 1.0, gcorr);
        g->font (f3); setc (INK); tleft (316, 180, "CORR " + juce::String (gcorr >= 0 ? "+" : "-") + juce::String (std::abs (gcorr), 2));
    // correlation bar: ink from the middle towards the value
    const double gcx = gcorr * (74 - 2 * LN - 2);
    setc (INK);
    if (gcx >= 0) rrect (382 - 3, 200 + 2 * LN, juce::jmax (6.0, gcx + 3), 16 - 4 * LN, CR - LN);
    else rrect (382 + gcx, 200 + 2 * LN, juce::jmax (6.0, 3 - gcx), 16 - 4 * LN, CR - LN);
    lastdown = mdown;
    // not processing (bypassed, offline, or the host has stopped calling): the whole panel is shown washed out.
    // The controls still work.
    if (gidle) scr.wash (DESK, 0.6);
    mouse_x = realX; mouse_y = realY;
    scr.touch();
}
