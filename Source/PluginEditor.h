// Tobo Imager - the panel. Copyright (C) 2026 Tobokegao. Licensed under the GNU AGPL v3 or later.
//
// The panel is drawn the way the JSFX drew it: everything is painted pixel by pixel into an image, once per timer
// tick, by code that also handles the mouse (immediate mode). That keeps the look of the JSFX to the pixel rules it
// was built on (one line weight in whole pixels, every edge a mix of the two colours beside it).
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include "PluginProcessor.h"

// an opaque image with the few drawing calls the panel needs (after the gfx_* calls of JSFX)
class Canvas
{
public:
    void resize (int w, int h);
    int width() const { return w; }
    int height() const { return h; }
    const juce::Image& image() const { return img; }

    void set (int hex, double alpha = 1.0) { cr = (hex >> 16) & 255; cg = (hex >> 8) & 255; cb = hex & 255; a = alpha; }
    double a = 1.0;                                             // alpha of the next calls (gfx_a)
    void rect (double x, double y, double rw, double rh);       // whole pixels (the numbers are cut off like gfx_rect)
    void circle (double cx, double cy, double r);               // filled, soft edge
    void blit (const Canvas& src, int sx, int sy, int sw, int sh, int dx, int dy);
    int red (int x, int y) const;                               // 0..255

    void font (const juce::Font& f) { fnt = f; }
    void measure (const juce::String& s, double& tw, double& th) const;
    void text (const juce::String& s, int x, int y);            // top left corner at x, y
    void touch();
    void wash (int hex, double alpha);
    void clip (int x0, int y0, int x1, int y1) { kx0 = x0; ky0 = y0; kx1 = x1; ky1 = y1; }   // drawing only inside these pixels
    void unclip() { kx0 = ky0 = 0; kx1 = w; ky1 = h; }
    void getClip (int& x0, int& y0, int& x1, int& y1) const { x0 = kx0; y0 = ky0; x1 = kx1; y1 = ky1; }                          // the whole picture mixed towards one colour                                               // tell JUCE that the pixels have changed

private:
    juce::Image img;
    juce::uint8* px = nullptr;
    int w = 0, h = 0, stride = 0, kx0 = 0, ky0 = 0, kx1 = 0, ky1 = 0;
    int cr = 0, cg = 0, cb = 0;
    juce::Font fnt { juce::FontOptions (12.0f) };
    void put (int x, int y, double alpha);
};

class ToboImagerEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ToboImagerEditor (ToboImagerProcessor&);
    ~ToboImagerEditor() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    // for test/panel_shot.cpp: draw one frame now; put the mouse somewhere (pixels of the image)
    const juce::Image& renderNow() { frame(); return scr.image(); }
    double testLabelX (int k) const { return labelX[k] + labelW[k] / 2; } double testLabelY (int k) const { return labelY[k] + 6; }      // middle of the label of handle k, in units
    double testBandsCellX (int c) const { static const double m[4] = { 7, 20, 32, 45 }; return bandsCellX + m[c]; }      // middle of cell c of the BANDS choice, in units
    void poke (double x, double y, bool down, bool shiftKey = false, bool ctrlKey = false) { mouse_x = x; mouse_y = y; shift = shiftKey; ctrl = ctrlKey; if (down && ! held) { pressed = true; pressX = x; pressY = y; } held = down; }

private:
    void timerCallback() override;
    void applySize();
    void showMenu();
    void track (const juce::MouseEvent&);

    // ---- the panel (names as in the JSFX) ----
    void drawStatic();
    void frame();
    int ux (double v) const { return (int) std::floor (ox + v * ZM + 0.5 + 1e-6); }   // + 1e-6: a half that comes out as .4999999 still goes up
    int uy (double v) const { return (int) std::floor (oy + v * ZM + 0.5 + 1e-6); }
    void setc (int hex, double alpha = 1.0) { g->set (hex, alpha); }
    void frect (double x, double y, double w, double h);
    void rrect (double x, double y, double w, double h, double rad);
    void box (double x, double y, double w, double h, double rad, int fill);
    void sline (double xa, double ya, double xb, double yb, double grow = 0);
    void aarc (double pcx, double pcy, double rad, double a0, double a1);
    void aaseg (double xa, double ya, double xb, double yb);
    void slant45 (int c, int y0, int n);
    void jog (int xu, int y0, int n);
    double sdquad (double px, double py) const;
    void fillrq (int x0, int y0, int x1, int y1, double rad, double shr);
    void setrq (double xl, double xr, double yt, double yb, double rad, bool slr);
    void tcen (double cx, double cy, const juce::String& s);
    void tleft (double x, double cy, const juce::String& s);
    void tright (double x, double cy, const juce::String& s);
    double sget (int k) const;                 // k = slider number of the JSFX, 1..29
    void sset (int k, double v, bool drag = false);
    void endGesture();
    void knob (int id, int sl, double cx, double cy, double r, double lo, double hi, double def, double stp, int face, bool enabled = true);
    bool button (double x, double y, double w, double h, const juce::String& s, bool on, bool inv);
    void tcap (double cxPix, int y0, int y1, const juce::String& s); void capRows(); int capTop = 0, capBot = 8;
    std::pair<int, int> inkSpan (const juce::String& s);              // first and last column of pixels the name covers
    std::map<juce::String, std::pair<int, int>> inkSpans;             // (worked out once for each name and size)
    std::map<juce::String, double> inkWid;                             // how wide the name is, to a part of a pixel
    std::map<juce::String, double> inkMid;                             // the middle of the name left to right, from the x given to Canvas::text
    std::map<juce::String, std::pair<int, int>> inkRows;              // first and last row, from the y given to Canvas::text
    int segs (double x, double y, double w, double h, int cols, int rows, const char* const* names, int sel, int invMask, const double* colW = nullptr, const double* textDx = nullptr, int iconSet = 0);
    void icon (int set, int k, int left, int cpt, int capH, double cellMid);
    int iconW (int set, int k, int capH) const;
    static double fx (double f) { return 33 + std::log (f / 20) / std::log (1000.0) * 266; }
    static int mixc (int h1, int h2, double t);

    ToboImagerProcessor& proc;
    Canvas stat, logo, safeOff, safeOn, scr;
    Canvas* g = &scr;
    juce::Font f1 { juce::FontOptions (12.0f) }, f3 { juce::FontOptions (12.0f) };
    juce::Typeface::Ptr logoFace;
    double ZM = 1.5, LN = 1, lastzm = 0, scale = 1.0;
    double bandsCellX = 0;                                             // left edge of the first cell of the BANDS choice
    int NB = 4, lastNB = 0;                                            // how many bands there are (1..4)
    double PX0 = 8, PW = 268;                                          // the band plate: left edge of its foot and its width
    double colX (double k) const { return PX0 + PW * k / NB; }        // left edge of the column of band k (its foot)
    // where the parts of band k stand: the two knobs, the SAFE plate (the x its middle would have in a column), M / S
    struct BandPos { double wx, wy, ax, ay, sx, sy, mx, my, r; };
    BandPos bandPos (int k) const;
    int bc (int k) const;                                              // the colour band k carries
    int ox = 0, oy = 0;
    double qx[4] = {}, qy[4] = {};
    int sfh = 0, sfw = 0, sfiw = 0;
    // mouse
    double mouse_x = 0, mouse_y = 0, mxl = 0, myl = 0, ay = 0, av = 0, lastt = 0;
    bool mdown = false, mpress = false, lastdown = false, pressed = false, held = false, ctrl = false, shift = false;
    int act = 0, lastid = 0, gestureParam = -1;
    bool dragShift = false;                // whether shift was down when the drag of a knob (or its last piece) began
    double pressX = 0, pressY = 0;         // where the button went down (the mouse may have moved on before the next frame)
    double labelX[3] = { -1000, -1000, -1000 }, labelW[3] = {}, labelY[3] = { -1000, -1000, -1000 };      // the labels of the crossover handles as last drawn (units)
    int grabMask = 0;                      // the crossover handles that were within reach of the press (bit k)
    int pendLo = 0, pendHi = 0;            // crossover handles on one spot, waiting for the direction of the drag (act == 30)
    double grabOff = 0, pendX = 0;         // where on the handle the mouse took it; where the press was
    // meters
    unsigned glastc = 0; double galive = 0; bool gidle = false;
    double gsg = 0;
    std::vector<double> fbm, fbs;
    double spym[1024] = {}, spys[1024] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToboImagerEditor)
};
