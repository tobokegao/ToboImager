// Tobo Imager - test of the panel. Copyright (C) 2026 Tobokegao. Licensed under the GNU AGPL v3 or later.
// Draws the panel without a host or a screen: every size, with the kinds of stereo meter and side curve, to PNG files,
// and works the mouse on a few controls to see that they answer.   usage: PanelShot <output folder>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>

using namespace tobo;

static void feed (ToboImagerProcessor& p, juce::Random& rng, double& ph)
{
    juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m;
    for (int blk = 0; blk < 12; ++blk)
    {
        for (int i = 0; i < 512; ++i)
        {
            ph += 0.01;
            const float mid = (float) (0.25 * std::sin (ph * 3.1) + 0.2 * std::sin (ph * 41.0) + 0.1 * (rng.nextFloat() - 0.5f));
            const float side = (float) (0.12 * std::sin (ph * 17.0) + 0.08 * (rng.nextFloat() - 0.5f));
            b.setSample (0, i, mid + side); b.setSample (1, i, mid - side);
        }
        p.processBlock (b, m);
    }
}

static void setp (ToboImagerProcessor& p, int k, float v) { p.params[k]->setValueNotifyingHost (p.params[k]->convertTo0to1 (v)); }
static float getp (ToboImagerProcessor& p, int k) { return p.params[k]->convertFrom0to1 (p.params[k]->getValue()); }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out (argc > 1 ? juce::String (argv[1]) : juce::String ("panel_shots"));
    out.createDirectory();
    ToboImagerProcessor p; p.prepareToPlay (48000.0, 512);
    juce::Random rng (3); double ph = 0;
    const float w[4] = { 60, 130, 170, 200 }, a[4] = { 0, 40, 60, 80 };
    for (int k = 0; k < 4; ++k) { setp (p, pW1 + k, w[k]); setp (p, pA1 + k, a[k]); }
    setp (p, pSafe1 + 1, 1); setp (p, pMute1 + 2, 1); setp (p, pOn1 + 3, 1);
    static const double zoom[4] = { 1.0, 1.25, 1.5, 2.0 };
    for (int size = 0; size < 4; ++size)
        for (int v = 0; v < 3; ++v)
        {
            p.setGuiSize (size); p.setOpt ("scopeMode", v); p.setOpt ("sideStyle", v);
            setp (p, pType, v == 0 ? 2.0f : v == 1 ? 0.0f : 3.0f); setp (p, pMonitor, (float) v);
            std::unique_ptr<ToboImagerEditor> ed (dynamic_cast<ToboImagerEditor*> (p.createEditor()));
            juce::Image img;
            for (int f = 0; f < 6; ++f) { feed (p, rng, ph); img = ed->renderNow(); }
            juce::FileOutputStream os (out.getChildFile ("size" + juce::String (size) + "_v" + juce::String (v) + ".png"));
            os.setPosition (0); os.truncate();
            juce::PNGImageFormat().writeImageToStream (img, os);
        }

    setp (p, pMonitor, 0.0f);

    // one, two and three bands, at the smallest and the largest size
    for (int size : { 0, 3 })
        for (int n = 1; n <= 3; ++n)
        {
            p.setGuiSize (size); p.setOpt ("scopeMode", 0); setp (p, pBands, (float) (n - 1)); setp (p, pType, 2.0f);
            std::unique_ptr<ToboImagerEditor> ed2 (dynamic_cast<ToboImagerEditor*> (p.createEditor()));
            juce::Image img; for (int f = 0; f < 6; ++f) { feed (p, rng, ph); img = ed2->renderNow(); }
            juce::FileOutputStream os (out.getChildFile ("bands" + juce::String (n) + "_s" + juce::String (size) + ".png")); os.setPosition (0); os.truncate();
            juce::PNGImageFormat().writeImageToStream (img, os);
        }
    setp (p, pBands, 3.0f);

    // crossover handles close together and at the ends: their labels must not lie on top of each other
    {
        const float sets[5][3] = { { 300, 300, 5000 }, { 120, 2000, 2000 }, { 500, 500, 1000 }, { 29, 42, 60 }, { 7070, 9999, 14141 } };
        for (int size : { 0, 3 })
            for (int n = 0; n < 5; ++n)
            {
                p.setGuiSize (size); p.setOpt ("scopeMode", 0); for (int k = 0; k < 3; ++k) setp (p, pXo1 + k, sets[n][k]);
                std::unique_ptr<ToboImagerEditor> ed2 (dynamic_cast<ToboImagerEditor*> (p.createEditor()));
                juce::Image img; for (int f = 0; f < 6; ++f) { feed (p, rng, ph); img = ed2->renderNow(); }
                juce::FileOutputStream os (out.getChildFile ("handles" + juce::String (size) + "_" + juce::String (n) + ".png")); os.setPosition (0); os.truncate();
                juce::PNGImageFormat().writeImageToStream (img, os);
            }
        setp (p, pXo1, 120); setp (p, pXo2, 1000); setp (p, pXo3, 5000);
    }

    // ---- the mouse, at 1.5x ----
    p.setGuiSize (2); setp (p, pType, 0.0f);
    std::unique_ptr<ToboImagerEditor> ed (dynamic_cast<ToboImagerEditor*> (p.createEditor()));
    const double z = 1.5;
    auto at = [&] (double ux, double uy, bool down, bool shift = false) { ed->poke (ux * z, uy * z, down, shift); feed (p, rng, ph); ed->renderNow(); };
    // WIDTH of band 2: drag 30 pixels up = +40 %, then with shift (fine)
    const double kx = 8 + 67 + 33.5 + 24, ky = 142 + 50;
    float v0 = getp (p, pW2);
    at (kx, ky, true); at (kx, ky - 20, true); at (kx, ky - 20, false);
    std::cout << "width 2 drag 30 px up: " << v0 << " -> " << getp (p, pW2) << "\n";
    juce::Thread::sleep (500);
    v0 = getp (p, pW2); at (kx, ky, true, true); at (kx, ky + 20, true, true); at (kx, ky + 20, false);
    std::cout << "width 2 shift drag 30 px down: " << v0 << " -> " << getp (p, pW2) << "\n";
    juce::Thread::sleep (500);
    at (kx, ky, true); at (kx, ky, false); at (kx, ky, true); at (kx, ky, false);
    std::cout << "width 2 double click: " << getp (p, pW2) << " (default 100)\n";
    // crossover 2: drag the handle to the right
    const double hx = 33 + std::log (1000.0 / 20) / std::log (1000.0) * 266;
    juce::Thread::sleep (500);
    at (hx, 60, true); at (hx + 30, 60, true); at (hx + 30, 60, false);
    std::cout << "crossover 2 dragged 30 units right: 1000 -> " << getp (p, pXo2) << "\n";
    // band header, SAFE, M, S, type buttons, a pale knob
    auto click = [&] (double ux, double uy) { juce::Thread::sleep (450); at (ux, uy, true); at (ux, uy, false); };
    click (8 + 24 + 30, 142 + 8); std::cout << "band 1 header click: on = " << getp (p, pOn1) << " (was 1)\n";
    click (8 + 33.5 + 24 - 12, 142 + 88); std::cout << "band 1 SAFE click: " << getp (p, pSafe1) << " (was 0)\n";
    click (8 + 33.5 - 14, 142 + 167); std::cout << "band 1 M click: " << getp (p, pMute1) << " (was 0)\n";
    click (8 + 33.5 + 14, 142 + 167); std::cout << "band 1 S click: solo = " << getp (p, pSolo) << " (was 0)\n";
    // the joined plates: every part of each. Type (four parts of 39 from x 292, y 269..283):
    int bad = 0;
    for (int k : { 2, 1, 3, 0, 3 })
    {
        click (292 + 39 * (k + 0.5), 276); const int got = (int) getp (p, pType);
        std::cout << "TYPE part " << k << " click: type = " << got << (got == k ? "" : "   WRONG") << "\n"; bad += got != k;
    }
    // near the two ends of a part and just outside the plate
    click (292 + 39 + 1.5, 276); std::cout << "TYPE left end of part 1: type = " << getp (p, pType) << " (want 1)\n"; bad += (int) getp (p, pType) != 1;
    click (292 + 39 * 3 - 1.5, 276); std::cout << "TYPE right end of part 2: type = " << getp (p, pType) << " (want 2)\n"; bad += (int) getp (p, pType) != 2;
    click (370, 290); std::cout << "under the TYPE plate: type = " << getp (p, pType) << " (want 2, no change)\n"; bad += (int) getp (p, pType) != 2;
    setp (p, pType, 2.0f);
    v0 = getp (p, pDelay); juce::Thread::sleep (450); at (318, 328, true); at (318, 308, true); at (318, 308, false);
    std::cout << "pale DELAY knob dragged: " << v0 << " -> " << getp (p, pDelay) << " (must not move)\n"; bad += getp (p, pDelay) != v0;
    v0 = getp (p, pScatter); juce::Thread::sleep (450); at (370, 328, true); at (370, 308, true); at (370, 308, false);
    std::cout << "pale SCATTER knob dragged: " << v0 << " -> " << getp (p, pScatter) << " (must not move)\n"; bad += getp (p, pScatter) != v0;
    click (292 + 19.5, 276);
    v0 = getp (p, pDelay); juce::Thread::sleep (450); at (318, 328, true); at (318, 308, true); at (318, 308, false);
    std::cout << "DELAY knob (type DELAY) dragged 30 px up: " << v0 << " -> " << getp (p, pDelay) << "\n"; bad += ! (getp (p, pDelay) > v0);
    click (292 + 39 * 3.5, 276);
    v0 = getp (p, pScatter); juce::Thread::sleep (450); at (370, 328, true); at (370, 308, true); at (370, 308, false);
    std::cout << "SCATTER knob (type FVN) dragged 30 px up: " << v0 << " -> " << getp (p, pScatter) << "\n"; bad += ! (getp (p, pScatter) > v0);
    v0 = getp (p, pGuard); juce::Thread::sleep (450); at (422, 328, true); at (422, 308, true); at (422, 308, false);
    std::cout << "GUARD knob dragged 30 px up: " << v0 << " -> " << getp (p, pGuard) << "\n"; bad += ! (getp (p, pGuard) > v0);
    // meter kinds (three parts of 44 from x 316, y 16..30)
    for (int k : { 1, 2, 0, 2, 2 })
    {
        click (316 + 44 * (k + 0.5), 23); const int got = p.getOpt ("scopeMode", 0);
        std::cout << "METER part " << k << " click: kind = " << got << (got == k ? "" : "   WRONG") << "\n"; bad += got != k;
    }
    // monitor (three parts of 46 from x 130, y 344..358); a second click on MID or SIDE goes back to STEREO
    static const int mk[6] = { 1, 2, 2, 1, 0, 0 }, mw[6] = { 1, 2, 0, 1, 0, 0 };
    for (int n = 0; n < 6; ++n)
    {
        click (98 + 42.5 * (mk[n] + 0.5), 351); const int got = (int) getp (p, pMonitor);
        std::cout << "MONITOR part " << mk[n] << " click: monitor = " << got << " (want " << mw[n] << ")" << (got == mw[n] ? "" : "   WRONG") << "\n"; bad += got != mw[n];
    }
    click (98 + 42.5 * 3.5, 351); { const int got = (int) getp (p, pMonitor); std::cout << "MONITOR DELTA click: monitor = " << got << " (want 3)\n"; bad += got != 3; }
    click (98 + 42.5 * 3.5, 351); { const int got = (int) getp (p, pMonitor); std::cout << "MONITOR DELTA click again: monitor = " << got << " (want 0)\n"; bad += got != 0; }
    std::cout << (bad ? "JOINED PLATES: " + std::to_string (bad) + " WRONG" : std::string ("joined plates: all right")) << "\n";
    return 0;
}
