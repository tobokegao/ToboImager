// Tobo Imager - stress test. Copyright (C) 2026 Tobokegao. Licensed under the GNU AGPL v3 or later.
// Things a user or a host is not supposed to do, done on purpose, without a host or a screen: odd sample rates and
// block sizes, every number of bands, crossovers out of order, parameters changed at random every block, bad numbers
// in the input, broken saved states, and a mouse that presses, drags and lets go anywhere, with most of the weight on
// the crossover handles. It also measures that each band does what its knobs say, for one to four bands.
// Every check prints a line; the last line says how many failed.   usage: StressTest
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <thread>

using namespace tobo;

static int failed = 0, checks = 0;
static void check (bool ok, const juce::String& what)
{
    ++checks; if (! ok) ++failed;
    std::cout << (ok ? "ok    " : "FAIL  ") << what << std::endl;
}

static void setp (ToboImagerProcessor& p, int k, double v) { p.params[k]->setValueNotifyingHost (p.params[k]->convertTo0to1 ((float) v)); }
static double getp (ToboImagerProcessor& p, int k) { return p.params[k]->convertFrom0to1 (p.params[k]->getValue()); }
static void defaults (ToboImagerProcessor& p) { for (int k = 0; k < kNumParams; ++k) p.params[k]->setValueNotifyingHost (p.params[k]->getDefaultValue()); }
static void start (ToboImagerProcessor& p, double sr, int size) { p.setPlayConfigDetails (2, 2, sr, size); p.prepareToPlay (sr, size); defaults (p); }

struct Stat { bool finite = true; double peak = 0, em = 0, es = 0; };      // em, es: energy of mid and of side

// noise through the plugin: independent left and right, or the same on both (mono)
template <typename Float>
static Stat run (ToboImagerProcessor& p, juce::Random& rng, int blocks, int size, double level = 0.25, bool mono = false, Stat* in = nullptr)
{
    juce::AudioBuffer<Float> b (2, juce::jmax (1, size)); juce::MidiBuffer m; Stat s;
    for (int n = 0; n < blocks; ++n)
    {
        b.setSize (2, size, false, false, true);
        for (int i = 0; i < size; ++i)
        {
            const double l = level * (rng.nextDouble() * 2 - 1), r = mono ? l : level * (rng.nextDouble() * 2 - 1);
            b.setSample (0, i, (Float) l); b.setSample (1, i, (Float) r);
            if (in != nullptr) { in->em += (l + r) * (l + r) / 4; in->es += (l - r) * (l - r) / 4; }
        }
        p.processBlock (b, m);
        for (int i = 0; i < size; ++i)
        {
            const double l = b.getSample (0, i), r = b.getSample (1, i);
            if (! std::isfinite (l) || ! std::isfinite (r)) s.finite = false;
            s.peak = juce::jmax (s.peak, std::abs (l), std::abs (r)); s.em += (l + r) * (l + r) / 4; s.es += (l - r) * (l - r) / 4;
        }
    }
    return s;
}
// a sine that is only in the side (left = -right), 0.25 high: returns the level of the side that comes out against
// what went in, in dB, measured after the first half second
static double sideTone (ToboImagerProcessor& p, double freq, double sr)
{
    juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; double ein = 0, eout = 0, ph = 0;
    for (int n = 0; n < (int) (sr * 1.5 / 512); ++n)
    {
        for (int i = 0; i < 512; ++i) { ph += 2 * kPi * freq / sr; const float v = (float) (0.25 * std::sin (ph)); b.setSample (0, i, v); b.setSample (1, i, -v); }
        const bool count = n * 512 > sr * 0.5;
        if (count) for (int i = 0; i < 512; ++i) ein += b.getSample (0, i) * b.getSample (0, i);
        p.processBlock (b, m);
        if (count) for (int i = 0; i < 512; ++i) { const double sd = (b.getSample (0, i) - b.getSample (1, i)) * 0.5; eout += sd * sd; }
    }
    return 10 * std::log10 ((eout + 1e-30) / (ein + 1e-30));
}
static double dB (double a, double b) { return 10 * std::log10 ((a + 1e-30) / (b + 1e-30)); }

static bool paramsSane (ToboImagerProcessor& p, juce::String& why)
{
    static const double lo[kNumParams] = { 20, 20, 20, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    static const double hi[kNumParams] = { 20000, 20000, 20000, 200, 200, 200, 200, 100, 100, 100, 100, 30, 3, 100, 100, 4, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3 };
    for (int k = 0; k < kNumParams; ++k)
    {
        const double v = getp (p, k);
        if (! std::isfinite (v) || v < lo[k] - 1e-3 || v > hi[k] + 1e-3) { why = "parameter " + juce::String (k) + " = " + juce::String (v); return false; }
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
static void engineTests()
{
    juce::Random rng (11);
    std::cout << "\n== sample rates, block sizes, every type, every number of bands, crossovers at their ends ==\n";
    for (double sr : { 8000.0, 11025.0, 22050.0, 32000.0, 44100.0, 48000.0, 96000.0, 192000.0, 384000.0 })
    {
        bool fin = true; double peak = 0, worstMid = 0; juce::String worst;
        for (int bands = 1; bands <= 4; ++bands)
            for (int typ = 0; typ < 4; ++typ)
                for (int ends = 0; ends < 3; ++ends)          // crossovers: default, all at 20 Hz, all at 20 kHz
                    for (int size : { 1, 64, 4096, 20000 })
                    {
                        if (size == 1 && (typ + ends + bands) % 3 != 0) continue;      // one sample at a time is slow: a third of the cases
                        ToboImagerProcessor p; start (p, sr, size); setp (p, pType, typ); setp (p, pBands, bands - 1);
                        if (ends > 0) for (int k = 0; k < 3; ++k) setp (p, pXo1 + k, ends == 1 ? 20 : 20000);
                        for (int k = 0; k < 4; ++k) { setp (p, pW1 + k, 200); setp (p, pA1 + k, 100); }
                        const int blocks = juce::jmax (3, (int) (sr * 0.45 / size)); Stat in;
                        const Stat s = run<float> (p, rng, blocks, size, 0.25, false, &in);
                        fin = fin && s.finite; peak = juce::jmax (peak, s.peak);
                        // the mid must come out with the energy it went in with (the FVN type holds it back at the start: not counted)
                        const double d = std::abs (dB (s.em, in.em));
                        if (typ != 3 && size * blocks > sr * 0.3 && d > worstMid) { worstMid = d; worst = juce::String (bands) + " bands, type " + juce::String (typ) + ", ends " + juce::String (ends) + ", block " + juce::String (size); }
                    }
        check (fin && peak < 8, juce::String (sr, 0) + " Hz: output finite, peak " + juce::String (peak, 2));
        check (worstMid < 0.2, juce::String (sr, 0) + " Hz: mid keeps its energy, worst change " + juce::String (worstMid, 3) + " dB (" + worst + ")");
    }

    std::cout << "\n== what each band does, for one to four bands (48 kHz) ==\n";
    for (int bands = 1; bands <= 4; ++bands)
    {
        const double xo[3] = { 120, 1000, 5000 };
        // nothing changed (every width 100 %, no stereoize): mid and side keep their energy
        {
            ToboImagerProcessor p; start (p, 48000, 512); setp (p, pBands, bands - 1);
            run<float> (p, rng, 40, 512); Stat in; const Stat s = run<float> (p, rng, 300, 512, 0.25, false, &in);
            check (std::abs (dB (s.em, in.em)) < 0.05 && std::abs (dB (s.es, in.es)) < 0.05,
                   juce::String (bands) + " band(s), nothing turned: mid " + juce::String (dB (s.em, in.em), 3) + " dB, side " + juce::String (dB (s.es, in.es), 3) + " dB (want 0, 0)");
        }
        // a tone in the middle of band k: WIDTH of that band at 0 % takes it away, at 200 % doubles it; the other bands' knobs do not touch it
        bool ok = true; juce::String what;
        for (int k = 0; k < bands; ++k)
        {
            const double f0 = k == 0 ? 20 : xo[k - 1], f1 = k == bands - 1 ? 20000 : xo[k], f = std::sqrt (f0 * f1);
            double r[4];
            for (int c = 0; c < 4; ++c)
            {
                ToboImagerProcessor p; start (p, 48000, 512); setp (p, pBands, bands - 1);
                if (c == 0) setp (p, pW1 + k, 0);
                if (c == 1) setp (p, pW1 + k, 200);
                if (c == 2) for (int j = 0; j < 4; ++j) if (j != k) setp (p, pW1 + j, 0);       // every other band at 0 %
                if (c == 3) { setp (p, pMute1 + k, 1); }
                r[c] = sideTone (p, f, 48000);
            }
            const bool good = r[0] < -20 && r[1] > 5.3 && r[1] < 6.1 && r[2] > -1.2 && r[2] < 0.1 && r[3] < -20;
            what += " | band " + juce::String (k + 1) + " at " + juce::String (f, 0) + " Hz: " + juce::String (r[0], 1) + ", " + juce::String (r[1], 2) + ", " + juce::String (r[2], 2) + ", " + juce::String (r[3], 1);
            ok = ok && good;
        }
        check (ok, juce::String (bands) + " band(s), side tone in each band: own WIDTH 0 %, own WIDTH 200 %, the others at 0 %, own band muted (want below -20, about +6, about 0, below -20 dB)" + what);
        // settings of bands that are not there must not be heard
        {
            ToboImagerProcessor p; start (p, 48000, 512); setp (p, pBands, bands - 1);
            for (int k = bands; k < 4; ++k) { setp (p, pW1 + k, 0); setp (p, pA1 + k, 100); setp (p, pMute1 + k, 1); setp (p, pSafe1 + k, 1); setp (p, pOn1 + k, 0); }
            if (bands < 4) setp (p, pSolo, 4);            // a solo on a band that is not there
            run<float> (p, rng, 40, 512); Stat in; const Stat s = run<float> (p, rng, 300, 512, 0.25, false, &in);
            check (std::abs (dB (s.em, in.em)) < 0.05 && std::abs (dB (s.es, in.es)) < 0.05,
                   juce::String (bands) + " band(s), the knobs of the bands that are not there turned, solo on band 4: nothing changes (mid " + juce::String (dB (s.em, in.em), 3) + ", side " + juce::String (dB (s.es, in.es), 3) + " dB)");
        }
        // stereoize: a mono sound gets a side, its mid stays; with safe the side stays at or below the mid
        for (int typ = 0; typ < 4; ++typ)
        {
            ToboImagerProcessor p; start (p, 48000, 512); setp (p, pBands, bands - 1); setp (p, pType, typ);
            for (int k = 0; k < 4; ++k) setp (p, pA1 + k, 100);
            run<float> (p, rng, 60, 512, 0.25, true); Stat in; const Stat s = run<float> (p, rng, 300, 512, 0.25, true, &in);
            for (int k = 0; k < 4; ++k) { setp (p, pSafe1 + k, 1); setp (p, pW1 + k, 200); }
            run<float> (p, rng, 200, 512, 0.25, true); const Stat t = run<float> (p, rng, 300, 512, 0.25, true);
            check (std::abs (dB (s.em, in.em)) < 0.05 && dB (s.es, s.em) > -12 && dB (t.es, t.em) < 0.6,
                   juce::String (bands) + " band(s), type " + juce::String (typ) + ", mono noise, stereoize 100 %: mid " + juce::String (dB (s.em, in.em), 3) + " dB, side against mid " + juce::String (dB (s.es, s.em), 1)
                   + " dB; with safe " + juce::String (dB (t.es, t.em), 2) + " dB (want 0; above -12; at most about 0)");
        }
    }

    std::cout << "\n== the delta monitor: the output less what comes out when nothing is turned ==\n";
    for (int bands = 1; bands <= 4; ++bands)
        for (int typ = 0; typ < 4; ++typ)
        {
            // nothing turned: silence
            ToboImagerProcessor p; start (p, 48000, 512); setp (p, pBands, bands - 1); setp (p, pType, typ); setp (p, pMonitor, 3);
            run<float> (p, rng, 60, 512); Stat in; const Stat z = run<float> (p, rng, 200, 512, 0.25, false, &in);
            // every width at 200 %: the side once more (the mid is not in it)
            for (int k = 0; k < 4; ++k) setp (p, pW1 + k, 200);
            run<float> (p, rng, 60, 512); Stat in2; const Stat w = run<float> (p, rng, 200, 512, 0.25, false, &in2);
            // widths back, stereoize on mono noise: only side, as much as the stereo output has
            for (int k = 0; k < 4; ++k) { setp (p, pW1 + k, 100); setp (p, pA1 + k, 100); }
            run<float> (p, rng, 60, 512, 0.25, true); const Stat d = run<float> (p, rng, 200, 512, 0.25, true);
            setp (p, pMonitor, 0); run<float> (p, rng, 60, 512, 0.25, true); const Stat o = run<float> (p, rng, 200, 512, 0.25, true);
            // one band muted: the delta is that band, turned over (energy of what is missing)
            check (dB (z.em + z.es, in.em + in.es) < -120 && std::abs (dB (w.es, in2.es)) < 0.05 && dB (w.em, in2.em) < -120 && dB (d.em, d.es) < -100 && std::abs (dB (d.es, o.es)) < 0.3,
                   juce::String (bands) + " band(s), type " + juce::String (typ) + ": nothing turned " + juce::String (dB (z.em + z.es, in.em + in.es), 0) + " dB; widths 200 %: side " + juce::String (dB (w.es, in2.es), 3) + " dB, mid " + juce::String (dB (w.em, in2.em), 0)
                   + " dB; stereoize on mono: mid against side " + juce::String (dB (d.em, d.es), 0) + " dB, side against the stereo output's " + juce::String (dB (d.es, o.es), 2) + " dB (want silence; 0 and silence; silence and 0)");
        }

    std::cout << "\n== crossovers out of order (a host can automate them so) ==\n";
    for (int c = 0; c < 6; ++c)
    {
        ToboImagerProcessor p; start (p, 48000, 512);
        const double f[6][3] = { { 5000, 200, 1000 }, { 120, 9000, 1000 }, { 20000, 20, 20 }, { 500, 500, 500 }, { 12000, 300, 40 }, { 20, 20000, 20 } };
        for (int k = 0; k < 3; ++k) setp (p, pXo1 + k, f[c][k]);
        setp (p, pBands, c % 2 == 0 ? 3 : 2);
        for (int k = 0; k < 4; ++k) { setp (p, pW1 + k, 200); setp (p, pA1 + k, 100); setp (p, pSafe1 + k, c & 1); }
        Stat in; const Stat s = run<float> (p, rng, 200, 512, 0.25, false, &in);
        check (s.finite && s.peak < 8 && std::abs (dB (s.em, in.em)) < 0.2, juce::String (c % 2 == 0 ? 4 : 3) + " bands, crossovers " + juce::String (f[c][0], 0) + " / " + juce::String (f[c][1], 0) + " / " + juce::String (f[c][2], 0)
               + ": finite, peak " + juce::String (s.peak, 2) + ", mid " + juce::String (dB (s.em, in.em), 3) + " dB");
    }

    std::cout << "\n== every parameter changed at random, every block ==\n";
    for (double sr : { 44100.0, 96000.0 })
    {
        ToboImagerProcessor p; start (p, sr, 512);
        bool fin = true; double peak = 0;
        for (int n = 0; n < 3000; ++n)
        {
            for (int k = 0; k < kNumParams; ++k) if (rng.nextInt (4) == 0) p.params[k]->setValueNotifyingHost (rng.nextFloat());
            const Stat s = run<float> (p, rng, 1, 1 + rng.nextInt (700)); fin = fin && s.finite; peak = juce::jmax (peak, s.peak);
        }
        check (fin && peak < 4, juce::String (sr, 0) + " Hz, 3000 blocks of random settings: finite, peak " + juce::String (peak, 2));
    }
    {
        // the type and the number of bands changed every block (the latency changes with the type), double precision
        ToboImagerProcessor p; p.setProcessingPrecision (juce::AudioProcessor::doublePrecision); start (p, 48000, 256);
        for (int k = 0; k < 4; ++k) setp (p, pA1 + k, 100);
        bool fin = true; double peak = 0;
        for (int n = 0; n < 3000; ++n) { setp (p, pType, n % 4); setp (p, pBands, (n / 3) % 4); const Stat s = run<double> (p, rng, 1, 256); fin = fin && s.finite; peak = juce::jmax (peak, s.peak); }
        check (fin && peak < 4, "type and number of bands changed every block, double precision: finite, peak " + juce::String (peak, 2));
    }

    std::cout << "\n== bad numbers in the input ==\n";
    for (int typ = 0; typ < 4; ++typ)
        for (int bad = 0; bad < 4; ++bad)
        {
            ToboImagerProcessor p; start (p, 48000, 512); setp (p, pType, typ); setp (p, pBands, (typ + bad) % 4);
            for (int k = 0; k < 4; ++k) { setp (p, pA1 + k, 60); setp (p, pSafe1 + k, 1); } setp (p, pGuard, 50);
            run<float> (p, rng, 20, 512);
            juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; b.clear();
            const float v = bad == 0 ? std::numeric_limits<float>::quiet_NaN() : bad == 1 ? std::numeric_limits<float>::infinity() : bad == 2 ? 1e30f : 1e-40f;
            b.setSample (0, 100, v); b.setSample (1, 300, -v);
            p.processBlock (b, m);
            run<float> (p, rng, 400, 512);                      // 4 s of ordinary sound afterwards
            const Stat s = run<float> (p, rng, 40, 512);
            static const char* const names[4] = { "NaN", "infinity", "1e30", "1e-40" };
            check (s.finite && s.peak < 8 && s.peak > 0.05, "type " + juce::String (typ) + ", " + names[bad] + " in the input: sound is back to normal afterwards (peak " + juce::String (s.peak, 2) + ")");
        }
    {
        ToboImagerProcessor p; start (p, 48000, 512);
        for (int k = 0; k < 4; ++k) { setp (p, pW1 + k, 200); setp (p, pA1 + k, 100); }
        run<float> (p, rng, 50, 512);
        const Stat s = run<float> (p, rng, 400, 512, 0.0);      // silence
        const Stat t = run<float> (p, rng, 10, 512, 0.0);
        check (s.finite && t.peak < 1e-9, "silence in: the output dies away (peak after 4 s " + juce::String (t.peak, 12) + ")");
        const Stat u = run<float> (p, rng, 100, 512, 1.0, true);  // full scale mono
        check (u.finite && u.peak < 8, "full scale mono noise, everything at the top: peak " + juce::String (u.peak, 2));
    }
    {
        // other shapes of buffer
        ToboImagerProcessor p; start (p, 48000, 512); juce::MidiBuffer m;
        juce::AudioBuffer<float> b0 (2, 0); p.processBlock (b0, m);
        juce::AudioBuffer<float> b1 (1, 64); b1.clear(); p.processBlock (b1, m);
        juce::AudioBuffer<float> b4 (4, 64); for (int c = 0; c < 4; ++c) for (int i = 0; i < 64; ++i) b4.setSample (c, i, 0.1f); p.processBlock (b4, m);
        p.processBlockBypassed (b4, m);
        p.prepareToPlay (96000, 64); p.prepareToPlay (44100, 2048); p.releaseResources(); p.prepareToPlay (48000, 512); p.reset();
        const Stat s = run<float> (p, rng, 20, 512);
        check (s.finite, "empty block, one channel, four channels, bypass, the sample rate changed several times: still running");
        juce::AudioProcessor::BusesLayout mono; mono.inputBuses.add (juce::AudioChannelSet::mono()); mono.outputBuses.add (juce::AudioChannelSet::mono());
        check (! p.checkBusesLayoutSupported (mono), "a mono layout is refused");
    }

    std::cout << "\n== saved states ==\n";
    {
        ToboImagerProcessor a; for (int k = 0; k < kNumParams; ++k) a.params[k]->setValueNotifyingHost (rng.nextFloat());
        a.setGuiSize (3); a.setOpt ("scopeMode", 2);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        ToboImagerProcessor b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        bool same = b.getGuiSize() == 3 && b.getOpt ("scopeMode", 0) == 2;
        for (int k = 0; k < kNumParams; ++k) same = same && std::abs (getp (a, k) - getp (b, k)) < 1e-2;
        check (same, "a state saved and loaded again gives the same settings");

        // broken data
        ToboImagerProcessor c; juce::String why;
        c.setStateInformation (nullptr, 0);
        const char junk[] = "this is not a state"; c.setStateInformation (junk, (int) sizeof (junk));
        juce::MemoryBlock cut (mb.getData(), mb.getSize() / 2); c.setStateInformation (cut.getData(), (int) cut.getSize());
        juce::MemoryBlock noise (4096); for (size_t i = 0; i < 4096; ++i) noise[i] = (char) rng.nextInt (256); c.setStateInformation (noise.getData(), 4096);
        check (paramsSane (c, why), "empty, text, cut-off and random data as a state: settings stay valid " + why);

        // a state that is well formed but holds values out of range, and a state with nothing in it
        for (int kind = 0; kind < 3; ++kind)
        {
            juce::ValueTree t (c.apvts.state.getType());
            if (kind < 2)
            {
                t.setProperty ("guiSize", kind == 0 ? 99 : -7, nullptr); t.setProperty ("scopeMode", kind == 0 ? 55 : -3, nullptr);
                static const char* const ids[10] = { "xo1", "xo2", "xo3", "width1", "stereoize1", "type", "delay", "monitor", "bands", "solo" };
                for (auto* id : ids) { juce::ValueTree q ("PARAM"); q.setProperty ("id", id, nullptr); q.setProperty ("value", kind == 0 ? 1e9 : -1e9, nullptr); t.addChild (q, -1, nullptr); }
            }
            juce::MemoryBlock m2; if (auto xml = t.createXml()) juce::AudioProcessor::copyXmlToBinary (*xml, m2);
            ToboImagerProcessor d; d.setStateInformation (m2.getData(), (int) m2.getSize());
            d.setPlayConfigDetails (2, 2, 48000, 512); d.prepareToPlay (48000, 512);
            const Stat s = run<float> (d, rng, 50, 512);
            std::unique_ptr<ToboImagerEditor> ed (dynamic_cast<ToboImagerEditor*> (d.createEditor()));
            for (int f = 0; f < 3; ++f) ed->renderNow();
            const int sz = d.getGuiSize();
            check (paramsSane (d, why) && s.finite && sz >= 0 && sz <= 3 && ed->getWidth() > 100,
                   juce::String (kind == 0 ? "state with values far too high" : kind == 1 ? "state with values far too low" : "state with nothing in it")
                   + ": settings valid, sound finite, panel drawn (size " + juce::String (sz) + ", meter kind " + juce::String (d.getOpt ("scopeMode", 0)) + ", bands " + juce::String (getp (d, pBands) + 1) + ") " + why);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
struct Gestures : juce::AudioProcessorParameter::Listener
{
    int open[kNumParams] = {}; int bad = 0;
    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int index, bool starting) override
    {
        if (index < 0 || index >= kNumParams) return;
        if (starting) { if (open[index] != 0) ++bad; ++open[index]; } else { if (open[index] != 1) ++bad; --open[index]; }
    }
    int stillOpen() const { int n = 0; for (int k : open) n += k != 0; return n; }
};

static void panelTests()
{
    juce::Random rng (5);
    static const double zoom[4] = { 1.0, 1.25, 1.5, 2.0 };
    auto hx = [] (double f) { return 33 + std::log (f / 20) / std::log (1000.0) * 266; };      // where a crossover stands, in units
    auto hf = [] (double x) { return std::floor (20 * std::pow (1000.0, (x - 33) / 266) + 0.5); };

    std::cout << "\n== crossover handles ==\n";
    for (int size = 0; size < 4; ++size)
    {
        ToboImagerProcessor p; start (p, 48000, 512); p.setGuiSize (size);
        Gestures gl; for (auto* q : p.params) q->addListener (&gl);
        std::unique_ptr<ToboImagerEditor> ed (dynamic_cast<ToboImagerEditor*> (p.createEditor()));
        const double z = zoom[size]; const juce::String zs = juce::String (z) + "x: ";
        auto at = [&] (double ux, double uy, bool down) { ed->poke (ux * z, uy * z, down); run<float> (p, rng, 1, 256); ed->renderNow(); };
        auto drag = [&] (double x0, double x1, double y = 60, int steps = 8) { at (x0, y, true); for (int i = 1; i <= steps; ++i) at (x0 + (x1 - x0) * i / steps, y, true); at (x1, y, false); };
        auto xo = [&] (int k) { return getp (p, pXo1 + k); };
        auto ordered = [&] { return xo (0) <= xo (1) && xo (1) <= xo (2); };
        auto reset = [&] { defaults (p); p.setGuiSize (size); at (5, 370, false); };
        auto set3 = [&] (double a, double b, double c) { setp (p, pXo1, a); setp (p, pXo2, b); setp (p, pXo3, c); at (5, 370, false); };
        juce::String why;

        // a press on a handle without moving must not change it, in the middle of it and at the edges of where it can be grabbed
        bool still = true;
        for (int k = 0; k < 3; ++k)
            for (double off : { 0.0, -5.0, 5.0, -2.3, 3.7 })
            {
                reset(); const double f0 = xo (k); at (hx (f0) + off, 60, true); at (hx (f0) + off, 60, false);
                if (xo (k) != f0) { still = false; why += " handle " + juce::String (k + 1) + " pressed " + juce::String (off) + " units beside it: " + juce::String (f0) + " -> " + juce::String (xo (k)) + ";"; }
            }
        check (still, zs + "a press on a handle without moving leaves it where it is" + why); why = {};

        // just outside of where a handle can be grabbed: nothing moves
        reset(); bool none = true;
        for (int k = 0; k < 3; ++k) { const double f0 = xo (k); drag (hx (f0) + 9, hx (f0) + 40); drag (hx (f0) - 9, hx (f0) - 40); none = none && xo (0) == 120 && xo (1) == 1000 && xo (2) == 5000; }
        check (none, zs + "a drag that starts 9 units beside a handle moves nothing");
        reset(); drag (hx (1000), hx (1000) + 30, 4); drag (hx (1000), hx (1000) + 30, 137);
        check (xo (1) == 1000, zs + "a drag that starts above or below the display moves nothing");

        // far out of the window in both directions: a handle keeps half an octave (G) from the ends of the range
        // (20 Hz, 20 kHz) and from its neighbours, and pushes the neighbours along when it comes against them
        const double G = 1.4142135623730951;
        auto lo = [&] (double f) { return std::ceil (f * G); }; auto hi = [&] (double f) { return std::floor (f / G); };
        auto gaps = [&] { return xo (0) >= lo (20) && xo (1) >= lo (xo (0)) && xo (2) >= lo (xo (1)) && xo (2) <= hi (20000); };
        auto all3 = [&] { return juce::String (xo (0)) + " / " + juce::String (xo (1)) + " / " + juce::String (xo (2)); };
        const double l1 = lo (20), l2 = lo (l1), l3 = lo (l2), h3 = hi (20000), h2 = hi (h3), h1 = hi (h2);
        reset(); drag (hx (120), -3000);
        check (xo (0) == l1 && xo (1) == 1000 && xo (2) == 5000, zs + "handle 1 dragged far out left: " + all3() + " (want " + juce::String (l1) + " / 1000 / 5000)");
        drag (hx (l1), 9000);
        check (xo (0) == h1 && xo (1) == h2 && gaps(), zs + "handle 1 dragged far out right, pushing the other two: " + all3() + " (want " + juce::String (h1) + " / " + juce::String (h2) + " / half an octave above that, at most " + juce::String (h3) + ")");
        reset(); drag (hx (5000), -3000);
        check (xo (0) == l1 && xo (1) == l2 && xo (2) == l3, zs + "handle 3 dragged far out left, pushing the other two: " + all3() + " (want " + juce::String (l1) + " / " + juce::String (l2) + " / " + juce::String (l3) + ")");
        // pulled out again one after the other
        drag (hx (l3), hx (8000)); const double u3 = xo (2); drag (hx (l2), hx (900)); const double u2 = xo (1); drag (hx (l1), hx (100)); const double u1 = xo (0);
        check (std::abs (u3 - 8000) < 200 && std::abs (u2 - 900) < 25 && std::abs (u1 - 100) < 4 && gaps(), zs + "then pulled out one by one to the right: " + all3() + " (want about 100 / 900 / 8000)");
        reset(); drag (hx (1000), -3000); const juce::String m0 = all3(); const bool mlo = xo (0) == l1 && xo (1) == l2 && xo (2) == 5000;
        drag (hx (l2), 9000);
        check (mlo && xo (0) == l1 && xo (1) == h2 && gaps(), zs + "handle 2 dragged far out left (" + m0 + "), then right: " + all3() + " (want " + juce::String (l1) + " / " + juce::String (h2) + " / " + juce::String (h3) + ")");
        // pushed, and then drawn back: the one that was pushed stays where it was pushed to
        reset(); at (hx (1000), 60, true); at (hx (6000), 60, true); const double p3 = xo (2); at (hx (1000), 60, true); at (hx (1000), 60, false);
        check (p3 > 8000 && xo (2) == p3 && std::abs (xo (1) - 1000) < 30, zs + "handle 2 dragged to 6 kHz and back: handle 3 was pushed to " + juce::String (p3) + " and stays there, handle 2 is back at " + juce::String (xo (1)));
        // up and down out of the window while dragging: the drag goes on
        reset(); at (hx (1000), 60, true); at (hx (1000) + 10, -500, true); at (hx (2000), 4000, true); at (hx (2000), 4000, false);
        check (std::abs (xo (1) - 2000) < 40, zs + "handle 2 dragged with the mouse far above and below the window: " + juce::String (xo (1)) + " (want about 2000)");

        // two handles on the same spot: the one that can go where the mouse goes must come along
        reset(); set3 (300, 300, 5000); drag (hx (300), hx (900));
        check (xo (0) == 300 && std::abs (xo (1) - 900) < 20, zs + "handles 1 and 2 both at 300, dragged right: 1 = " + juce::String (xo (0)) + ", 2 = " + juce::String (xo (1)) + " (want 300, about 900)");
        reset(); set3 (300, 300, 5000); drag (hx (300), hx (100));
        check (std::abs (xo (0) - 100) < 4 && xo (1) == 300, zs + "handles 1 and 2 both at 300, dragged left: 1 = " + juce::String (xo (0)) + ", 2 = " + juce::String (xo (1)) + " (want about 100, 300)");
        reset(); set3 (120, 2000, 2000); drag (hx (2000), hx (8000));
        check (xo (1) == 2000 && std::abs (xo (2) - 8000) < 150, zs + "handles 2 and 3 both at 2000, dragged right: 2 = " + juce::String (xo (1)) + ", 3 = " + juce::String (xo (2)) + " (want 2000, about 8000)");
        reset(); set3 (120, 2000, 2000); drag (hx (2000), hx (500));
        check (std::abs (xo (1) - 500) < 12 && xo (2) == 2000, zs + "handles 2 and 3 both at 2000, dragged left: 2 = " + juce::String (xo (1)) + ", 3 = " + juce::String (xo (2)) + " (want about 500, 2000)");
        reset(); set3 (700, 700, 700); drag (hx (700), hx (3000)); const double t3 = xo (2); const bool r3 = xo (0) == 700 && xo (1) == 700;
        set3 (700, 700, 700); drag (hx (700), hx (90));
        check (r3 && std::abs (t3 - 3000) < 70 && std::abs (xo (0) - 90) < 4 && xo (1) == 700 && xo (2) == 700,
               zs + "all three at 700: dragged right, handle 3 goes (" + juce::String (t3) + "); dragged left, handle 1 goes (" + juce::String (xo (0)) + "); the ones not touched stay");
        // two handles that look like one but are not on the same value (1 to 5 units apart): pressed on the one that is
        // blocked in the direction of the drag, the other one must come along
        for (double gap : { 1.2, 2.0, 3.5, 5.0 })
        {
            reset(); set3 (300, hf (hx (300) + gap), 5000); const double fu = xo (1);
            drag (hx (300) - 0.5, hx (300) + 60); const bool r = xo (1) > fu * 1.4;
            reset(); set3 (hf (hx (400) - gap), 400, 5000); const double fl = xo (0);
            drag (hx (400) + 0.5, hx (400) - 60); const bool l = xo (0) < fl / 1.4;
            check (r && l, zs + "two handles " + juce::String (gap) + " units apart, pressed on the one that is blocked: the other one comes along (right " + juce::String ((int) r) + ", left " + juce::String ((int) l) + ")");
        }
        // two handles 3 units apart, grabbed on the right one and dragged right: the left one stays
        reset(); set3 (300, hf (hx (300) + 3), 5000); const double f2 = xo (1); drag (hx (f2) + 2, hx (f2) + 42);
        check (xo (0) == 300 && xo (1) > f2 + 50, zs + "handles 3 units apart, grabbed on the right one: 1 = " + juce::String (xo (0)) + ", 2 = " + juce::String (xo (1)));

        // the labels can be dragged, also where they do not stand under their lines: all three at 20 kHz (the labels are
        // pushed to the left of the edge, side by side), each label dragged left in turn; and all at 20 Hz, dragged right
        reset(); set3 (20000, 20000, 20000); at (5, 370, false);
        drag (ed->testLabelX (0), ed->testLabelX (0) - 150, ed->testLabelY (0)); const double la = xo (0);
        drag (ed->testLabelX (1), ed->testLabelX (1) - 80, ed->testLabelY (1)); const double lb = xo (1);
        drag (ed->testLabelX (2), ed->testLabelX (2) - 30, ed->testLabelY (2)); const double lc = xo (2);
        check (la < 1000 && lb < 6000 && lb > la && lc < 15000 && lc > lb, zs + "all three at 20 kHz, their labels dragged left one after the other: " + juce::String (la) + " / " + juce::String (lb) + " / " + juce::String (lc) + " (want three rising values below 20000)");
        reset(); set3 (20, 20, 20); at (5, 370, false);
        drag (ed->testLabelX (2), ed->testLabelX (2) + 150, ed->testLabelY (2)); const double ra = xo (2);
        drag (ed->testLabelX (1), ed->testLabelX (1) + 80, ed->testLabelY (1)); const double rb = xo (1);
        drag (ed->testLabelX (0), ed->testLabelX (0) + 30, ed->testLabelY (0)); const double rc = xo (0);
        check (ra > 400 && rb > 60 && rb < ra && rc > 20 && rc < rb, zs + "all three at 20 Hz, their labels dragged right one after the other: " + juce::String (rc) + " / " + juce::String (rb) + " / " + juce::String (ra) + " (want three rising values above 20)");
        // a press on a label without moving changes nothing
        reset(); set3 (20, 9000, 20000); at (5, 370, false); at (ed->testLabelX (0), ed->testLabelY (0), true); at (ed->testLabelX (0), ed->testLabelY (0), false); at (ed->testLabelX (2), ed->testLabelY (2), true); at (ed->testLabelX (2), ed->testLabelY (2), false);
        check (xo (0) == 20 && xo (1) == 9000 && xo (2) == 20000, zs + "a press on the label of a handle at the edge, without moving, changes nothing");

        // every label stands under its line, also with the three handles half an octave apart at the top of the range,
        // and no two labels lie on each other
        reset(); drag (hx (120), 9000); at (5, 370, false);
        { bool under = true, apart = true;
          for (int k = 0; k < 3; ++k) under = under && std::abs (ed->testLabelX (k) - hx (xo (k))) < 3.5;
          for (int k = 0; k < 3; ++k) for (int q = 0; q < k; ++q) apart = apart && (ed->testLabelY (k) != ed->testLabelY (q) || std::abs (ed->testLabelX (k) - ed->testLabelX (q)) > 20);
          check (under && apart, zs + "three handles at the top, half an octave apart: every label is under its line (" + juce::String (ed->testLabelX (0) - hx (xo (0)), 1) + ", " + juce::String (ed->testLabelX (1) - hx (xo (1)), 1) + ", "
                 + juce::String (ed->testLabelX (2) - hx (xo (2)), 1) + " units off), none lies on another"); }

        // the mouse has already moved on when the next frame comes (a real mouse does that)
        reset(); ed->poke (hx (1000) * z, 60 * z, true); ed->poke ((hx (1000) + 40) * z, 60 * z, true); ed->renderNow(); ed->renderNow(); ed->poke ((hx (1000) + 40) * z, 60 * z, false); ed->renderNow();
        const double q1 = xo (1);
        reset(); set3 (300, 300, 5000);
        ed->poke (hx (300) * z, 60 * z, true); ed->poke ((hx (300) + 50) * z, 60 * z, true); ed->renderNow(); ed->renderNow(); ed->poke ((hx (300) + 50) * z, 60 * z, false); ed->renderNow();
        check (q1 > 1300 && xo (0) == 300 && xo (1) > 400, zs + "the mouse moved 40 units before the next frame: handle 2 follows (" + juce::String (q1) + "); two on one spot: 1 = " + juce::String (xo (0)) + ", 2 = " + juce::String (xo (1)));
        reset(); ed->poke ((8 + 33.5 - 14) * z, (142 + 167) * z, true); ed->poke ((8 + 33.5 - 14) * z, (142 + 167) * z, false); ed->renderNow();
        check (getp (p, pMute1) == 1, zs + "a click that is over before the next frame still switches the button");

        // crossovers put out of order from outside: the panel still draws, and a drag puts the handle between its neighbours
        reset(); set3 (5000, 200, 9000); for (int f = 0; f < 3; ++f) at (5, 370, false);
        at (hx (200), 60, true); at (hx (200), 60, false);
        const bool put = xo (0) == 200 && xo (1) == 5000 && xo (2) == 9000;
        drag (hx (200), hx (900));
        check (put && std::abs (xo (0) - 900) < 25 && xo (1) == 5000, zs + "crossovers 5000 / 200 / 9000 set from outside: the first press puts them in order, the next drag moves the handle that is there (" + juce::String (xo (0)) + " / " + juce::String (xo (1)) + " / " + juce::String (xo (2)) + ")");

        // ---- fewer bands: the handles that are not there cannot be taken, the last one goes up to 20 kHz ----
        for (int bands = 1; bands <= 3; ++bands)
        {
            reset(); setp (p, pBands, bands - 1); at (5, 370, false);
            bool hidden = true;
            for (int k = bands - 1; k < 3; ++k) { const double f0 = xo (k); drag (hx (f0), hx (f0) + 30); drag (hx (f0), hx (f0) - 30); hidden = hidden && xo (k) == f0; }
            double top = -1;
            if (bands > 1) { const int k = bands - 2; drag (hx (xo (k)), 9000); top = xo (k); }
            check (hidden && (bands == 1 || top == hi (20000)), zs + juce::String (bands) + " band(s): the handles that are not shown cannot be dragged"
                   + (bands > 1 ? ", handle " + juce::String (bands - 1) + " goes up to " + juce::String (top) + " (want " + juce::String (hi (20000)) + ")" : juce::String()));
        }

        // ---- the BANDS choice ----
        reset(); bool counts = true;
        for (int n : { 1, 3, 2, 4, 4, 1, 4 }) { at (ed->testBandsCellX (n - 1), 18, true); at (ed->testBandsCellX (n - 1), 18, false); counts = counts && (int) getp (p, pBands) + 1 == n; }
        check (counts && xo (0) == 120 && xo (1) == 1000 && xo (2) == 5000, zs + "BANDS clicked 1, 3, 2, 4, 4, 1, 4: the number follows, the crossovers are as they were");
        // a solo on a band that goes away is taken off; one on a band that stays, stays
        reset(); setp (p, pSolo, 4); at (5, 370, false); at (ed->testBandsCellX (2), 18, true); at (ed->testBandsCellX (2), 18, false); const double s1 = getp (p, pSolo);
        reset(); setp (p, pSolo, 2); at (5, 370, false); at (ed->testBandsCellX (2), 18, true); at (ed->testBandsCellX (2), 18, false);
        check (s1 == 0 && getp (p, pSolo) == 2, zs + "4 -> 3 bands: solo on band 4 is taken off (" + juce::String (s1) + "), solo on band 2 stays (" + juce::String (getp (p, pSolo)) + ")");
        // a crossover that comes back into use below the one under it is put above it
        reset(); setp (p, pBands, 1); set3 (3000, 50, 40); at (ed->testBandsCellX (3), 18, true); at (ed->testBandsCellX (3), 18, false);
        check ((int) getp (p, pBands) == 3 && xo (0) == 3000 && xo (1) > 3000 && xo (2) > xo (1) && xo (2) <= 20000,
               zs + "2 -> 4 bands with the unused crossovers below the first: " + juce::String (xo (0)) + " / " + juce::String (xo (1)) + " / " + juce::String (xo (2)) + " (want rising)");
        // bands added when the crossover in use stands at the top: the ones below make room
        reset(); setp (p, pBands, 1); set3 (14142, 14142, 14142); at (ed->testBandsCellX (3), 18, true); at (ed->testBandsCellX (3), 18, false);
        check ((int) getp (p, pBands) == 3 && xo (2) == hi (20000) && xo (1) <= hi (xo (2)) && xo (0) <= hi (xo (1)) && xo (0) >= lo (20),
               zs + "2 -> 4 bands with the crossover at the top of its range: " + juce::String (xo (0)) + " / " + juce::String (xo (1)) + " / " + juce::String (xo (2)) + " (want half an octave between them)");
        // a handle that stands behind the BANDS plate: a click on the plate changes the number, not the handle
        reset(); set3 (hf (ed->testBandsCellX (1)), 1000, 5000); const double fb = xo (0);
        at (ed->testBandsCellX (1), 18, true); at (ed->testBandsCellX (1) + 9, 18, true); at (ed->testBandsCellX (1) + 9, 18, false);
        check ((int) getp (p, pBands) == 1 && xo (0) == fb, zs + "a handle behind the BANDS plate: a click on the plate sets the number (" + juce::String (getp (p, pBands) + 1) + ") and leaves the handle (" + juce::String (xo (0)) + ")");
        // every number of bands: header, SAFE, M, S of every column answer where they are drawn
        bool cols = true; juce::String cw;
        for (int bands = 1; bands <= 4; ++bands)
        {
            reset(); setp (p, pBands, bands - 1); at (5, 370, false);
            for (int k = 0; k < bands; ++k)
            {
                const double cx = 8 + 268.0 * (k + 0.5) / bands;
                at (cx + 24 + 5, 142 + 8, true); at (cx + 24 + 5, 142 + 8, false);
                at (cx + 12, 142 + 88, true); at (cx + 12, 142 + 88, false);
                at (cx - 14, 142 + 167, true); at (cx - 14, 142 + 167, false);
                at (cx + 14, 142 + 167, true); at (cx + 14, 142 + 167, false);
                const bool ok = getp (p, pOn1 + k) == 0 && getp (p, pSafe1 + k) == 1 && getp (p, pMute1 + k) == 1 && (int) getp (p, pSolo) == k + 1;
                if (! ok) { cols = false; cw += " " + juce::String (bands) + " bands, column " + juce::String (k + 1) + ": on " + juce::String (getp (p, pOn1 + k)) + " safe " + juce::String (getp (p, pSafe1 + k)) + " mute " + juce::String (getp (p, pMute1 + k)) + " solo " + juce::String (getp (p, pSolo)) + ";"; }
                const double w0 = getp (p, pW1 + k), a0 = getp (p, pA1 + k);
                juce::Thread::sleep (400); at (cx + 24, 142 + 50, true); at (cx + 24, 142 + 30, true); at (cx + 24, 142 + 30, false);
                juce::Thread::sleep (400); at (cx, 142 + 129.5, true); at (cx, 142 + 109.5, true); at (cx, 142 + 109.5, false);
                if (! (getp (p, pW1 + k) > w0 && getp (p, pA1 + k) > a0)) { cols = false; cw += " " + juce::String (bands) + " bands, column " + juce::String (k + 1) + ": knobs do not turn;"; }
            }
            for (int k = bands; k < 4; ++k) if (getp (p, pOn1 + k) != 1 || getp (p, pSafe1 + k) != 0 || getp (p, pMute1 + k) != 0) { cols = false; cw += " band " + juce::String (k + 1) + " (not shown) was changed;"; }
        }
        check (cols, zs + "one to four bands: header, SAFE, M, S and both knobs of every column answer, bands that are not shown stay untouched" + cw);

        // the size changed in the middle of a drag, and the panel closed in the middle of a drag
        reset(); at (hx (1000), 60, true); at (hx (1500), 60, true);
        p.setGuiSize ((size + 1) % 4); for (int f = 0; f < 3; ++f) { run<float> (p, rng, 1, 256); ed->renderNow(); }
        ed->poke (hx (1500) * zoom[(size + 1) % 4], 60, false); ed->renderNow();
        check (paramsSane (p, why) && ordered(), zs + "size changed during a drag: settings valid " + why);
        // the number of bands changed (by the host) in the middle of a drag of the handle that goes away
        p.setGuiSize (size); reset(); at (hx (5000), 60, true); at (hx (6000), 60, true); setp (p, pBands, 1); at (hx (7000), 60, true); at (hx (9000), 60, true); at (hx (9000), 60, false);
        check (paramsSane (p, why) && gl.stillOpen() == 0 && gl.bad == 0, zs + "number of bands changed by the host while the last handle is dragged: settings valid, the drag is ended " + why);
        reset(); at (hx (xo (1)), 60, true); at (hx (xo (1)) + 20, 60, true);
        ed.reset();
        check (gl.stillOpen() == 0 && gl.bad == 0, zs + "panel closed during a drag: the host is told the drag has ended (open " + juce::String (gl.stillOpen()) + ", out of step " + juce::String (gl.bad) + ")");
        for (auto* q : p.params) q->removeListener (&gl);
    }

    std::cout << "\n== knobs ==\n";
    {
        ToboImagerProcessor p; start (p, 48000, 512); p.setGuiSize (2);
        Gestures gl; for (auto* q : p.params) q->addListener (&gl);
        std::unique_ptr<ToboImagerEditor> ed (dynamic_cast<ToboImagerEditor*> (p.createEditor()));
        const double z = 1.5, kx = 8 + 67 + 33.5 + 24, ky = 142 + 50;      // WIDTH of band 2
        auto at = [&] (double ux, double uy, bool down, bool sh = false, bool ct = false) { ed->poke (ux * z, uy * z, down, sh, ct); run<float> (p, rng, 1, 256); ed->renderNow(); };
        // shift pressed in the middle of a drag: no jump at that moment
        at (kx, ky, true); at (kx, ky - 40, true); const double v1 = getp (p, pW2); at (kx, ky - 40, true, true); const double v2 = getp (p, pW2);
        at (kx, ky - 46, true, true); const double v3 = getp (p, pW2); at (kx, ky - 46, true, false); const double v4 = getp (p, pW2); at (kx, ky - 46, false);
        check (v1 > 150 && v2 == v1 && v3 > v2 && v3 - v2 <= 3 && v4 == v3, "shift pressed and let go in the middle of a drag: " + juce::String (v1) + " -> " + juce::String (v2) + " -> " + juce::String (v3) + " -> " + juce::String (v4) + " (no jump)");
        // dragged far out of the window: stops at the ends
        juce::Thread::sleep (500); at (kx, ky, true); at (kx, ky - 90000, true); const double top = getp (p, pW2); at (kx + 5000, ky + 90000, true); const double bot = getp (p, pW2); at (kx, ky, false);
        check (top == 200 && bot == 0, "knob dragged far out of the window up and down: " + juce::String (top) + ", " + juce::String (bot) + " (want 200, 0)");
        // ctrl + click = default; three quick clicks; press on one knob, release on another
        juce::Thread::sleep (500); at (kx, ky, true, false, true); at (kx, ky, false); const double d1 = getp (p, pW2);
        setp (p, pW2, 37); juce::Thread::sleep (500); for (int n = 0; n < 3; ++n) { at (kx, ky, true); at (kx, ky, false); } const double d2 = getp (p, pW2);
        check (d1 == 100 && d2 == 100, "ctrl + click, and three quick clicks: " + juce::String (d1) + ", " + juce::String (d2) + " (want 100, 100)");
        juce::Thread::sleep (500); const double w3 = getp (p, pW3); at (kx, ky, true); at (kx + 67, ky, true); at (kx + 67, ky, false);
        check (getp (p, pW3) == w3, "press on one knob, release on its neighbour: the neighbour does not move");
        juce::Thread::sleep (500); setp (p, pW3, 150); at (kx, ky, true); at (kx, ky, false); at (kx + 67, ky, true); at (kx + 67, ky, false);
        check (getp (p, pW3) == 150, "a click on one knob and at once a click on the next: the second is not taken as a double click");
        // the DELAY knob is being dragged and the host changes the type: the knob lets go
        juce::Thread::sleep (500); setp (p, pType, 0); at (5, 370, false); at (318, 328, true); at (318, 318, true); const double dl = getp (p, pDelay);
        setp (p, pType, 1); at (318, 300, true); at (318, 280, true); const double dl2 = getp (p, pDelay); at (318, 280, false);
        check (dl > 12 && dl2 == dl && gl.stillOpen() == 0 && gl.bad == 0, "DELAY knob dragged while the host changes the type: the knob lets go (" + juce::String (dl) + " -> " + juce::String (dl2) + ")");
        // a knob of band 4 is being dragged and the host takes the band away: the drag ends, band 4 keeps what it had
        juce::Thread::sleep (500); defaults (p); p.setGuiSize (2); at (5, 370, false);
        const double k4 = 8 + 67 * 3 + 33.5 + 24; at (k4, ky, true); at (k4, ky - 20, true); const double w4 = getp (p, pW4);
        setp (p, pBands, 1); at (k4, ky - 60, true); at (k4, ky - 90, true); at (k4, ky - 90, false);
        check (w4 > 100 && getp (p, pW4) == w4 && gl.stillOpen() == 0 && gl.bad == 0, "WIDTH of band 4 dragged while the host goes to 2 bands: the knob lets go (" + juce::String (w4) + " -> " + juce::String (getp (p, pW4)) + ")");
        ed.reset(); for (auto* q : p.params) q->removeListener (&gl);
    }

    std::cout << "\n== a mouse that does anything, anywhere ==\n";
    for (int size = 0; size < 4; ++size)
    {
        ToboImagerProcessor p; start (p, 48000, 512); p.setGuiSize (size);
        Gestures gl; for (auto* q : p.params) q->addListener (&gl);
        std::unique_ptr<ToboImagerEditor> ed (dynamic_cast<ToboImagerEditor*> (p.createEditor()));
        const double z = zoom[size]; bool down = false, ok = true, fin = true; juce::String why; double x = 100, y = 100;
        for (int n = 0; n < 40000 && ok; ++n)
        {
            const int r = rng.nextInt (10);
            // half of the moves go to the display with the crossover handles; a quarter of the presses land right on a handle
            if (r < 5) { x = 20 + rng.nextDouble() * 290; y = rng.nextDouble() * 140; }
            else if (r < 8) { x = rng.nextDouble() * 464; y = rng.nextDouble() * 378; }
            else { x = rng.nextDouble() * 3000 - 1200; y = rng.nextDouble() * 3000 - 1200; }
            if (rng.nextInt (4) == 0) { x = hx (getp (p, pXo1 + rng.nextInt (3))) + rng.nextDouble() * 10 - 5; y = 10 + rng.nextDouble() * 120; }
            if (rng.nextInt (3) == 0) down = ! down;
            ed->poke (x * z, y * z, down, rng.nextInt (5) == 0, rng.nextInt (9) == 0);
            if (rng.nextInt (50) == 0) for (int k = 0; k < kNumParams; ++k) if (rng.nextInt (6) == 0) p.params[k]->setValueNotifyingHost (rng.nextFloat());     // the host moves things too
            if (rng.nextInt (400) == 0) p.setGuiSize (rng.nextInt (4)), p.setOpt ("scopeMode", rng.nextInt (3));
            if (n % 3 == 0) fin = fin && run<float> (p, rng, 1, 128).finite;
            ed->renderNow();
            ok = paramsSane (p, why);
        }
        ed->poke (x * z, y * z, false); ed->renderNow();
        check (ok && fin, juce::String (z) + "x: 40000 random mouse events with host changes in between: settings valid, sound finite " + why);
        check (gl.stillOpen() == 0 && gl.bad == 0, juce::String (z) + "x: every drag that was begun was ended (open " + juce::String (gl.stillOpen()) + ", out of step " + juce::String (gl.bad) + ")");
        ed.reset(); for (auto* q : p.params) q->removeListener (&gl);
    }
    {
        // the mouse alone (no changes from the host), the BANDS choice included: the crossovers in use must stay in order all the time
        ToboImagerProcessor p; start (p, 48000, 512); p.setGuiSize (2);
        std::unique_ptr<ToboImagerEditor> ed (dynamic_cast<ToboImagerEditor*> (p.createEditor()));
        bool down = false, order = true; juce::String why; int moved = 0, bandChanges = 0; double last[3] = { 120, 1000, 5000 }; int lastB = 4;
        for (int n = 0; n < 80000; ++n)
        {
            double x = 20 + rng.nextDouble() * 290, y = rng.nextDouble() * 140;
            if (rng.nextInt (3) == 0) { x = hx (getp (p, pXo1 + rng.nextInt (3))) + rng.nextDouble() * 10 - 5; }
            if (rng.nextInt (20) == 0) x = rng.nextDouble() * 4000 - 1500;
            if (rng.nextInt (40) == 0) { x = ed->testBandsCellX (rng.nextInt (4)); y = 18; }
            if (rng.nextInt (3) == 0) down = ! down;
            ed->poke (x * 1.5, y * 1.5, down); ed->renderNow();
            const double f[3] = { getp (p, pXo1), getp (p, pXo2), getp (p, pXo3) }; const int b = (int) getp (p, pBands) + 1;
            // the crossovers in use: a third of an octave apart (a hair less is rounding to whole Hz), and from 20 Hz and 20 kHz
            bool good = b < 2 || (f[0] >= 28 && f[b - 2] <= 14143);
            for (int k = 1; k < b - 1; ++k) good = good && f[k] >= f[k - 1] * 1.4142 - 1.5;
            if (! good && order) { order = false; why = juce::String (b) + " bands: " + juce::String (f[0]) + " / " + juce::String (f[1]) + " / " + juce::String (f[2]) + " at event " + juce::String (n); }
            for (int k = 0; k < 3; ++k) { moved += f[k] != last[k]; last[k] = f[k]; }
            bandChanges += b != lastB; lastB = b;
        }
        check (order && moved > 1000 && bandChanges > 50, "80000 random mouse events on the display: the crossovers in use always keep half an octave between them and from the ends " + why + " (" + juce::String (moved) + " moves, " + juce::String (bandChanges) + " changes of the number of bands)");
    }

    std::cout << "\n== panel opened and closed while the sound runs on another thread ==\n";
    {
        ToboImagerProcessor p; start (p, 48000, 256);
        for (int k = 0; k < 4; ++k) setp (p, pA1 + k, 50);
        std::atomic<bool> stop { false }, fin { true };
        std::thread audio ([&] { juce::Random r2 (3); while (! stop) if (! run<float> (p, r2, 1, 256).finite) fin = false; });
        for (int n = 0; n < 150; ++n)
        {
            std::unique_ptr<ToboImagerEditor> ed (dynamic_cast<ToboImagerEditor*> (p.createEditor()));
            for (int f = 0; f < 4; ++f) { ed->poke (rng.nextDouble() * 600, rng.nextDouble() * 500, f % 2 == 0); ed->renderNow(); }
            if (n % 10 == 0) setp (p, pType, (n / 10) % 4);
            if (n % 7 == 0) setp (p, pBands, (n / 7) % 4);
            p.setGuiSize (n % 4);
        }
        stop = true; audio.join();
        check (fin.load(), "150 times opened, used and closed with the sound running: no crash, sound finite");
    }
}

// which parameters make the loud moments when they jump: each group alone, changed at random every block
static void diag()
{
    juce::Random rng (11);
    struct G { const char* name; int a, b; };
    const G groups[] = { { "crossovers", pXo1, pXo3 }, { "widths", pW1, pW4 }, { "stereoize", pA1, pA4 }, { "delay", pDelay, pDelay }, { "type", pType, pType }, { "guard+scatter", pGuard, pScatter },
                         { "solo+monitor", pSolo, pMonitor }, { "on", pOn1, pOn4 }, { "mute", pMute1, pMute4 }, { "safe", pSafe1, pSafe4 }, { "bands", pBands, pBands }, { "all", 0, kNumParams - 1 } };
    for (int base = 0; base < 2; ++base)
        for (auto& g : groups)
        {
            ToboImagerProcessor p; start (p, 44100, 512);
            if (base == 1) for (int k = 0; k < 4; ++k) { setp (p, pW1 + k, 200); setp (p, pA1 + k, 100); }
            double peak = 0;
            for (int n = 0; n < 3000; ++n)
            {
                for (int k = g.a; k <= g.b; ++k) if (rng.nextInt (4) == 0) p.params[k]->setValueNotifyingHost (rng.nextFloat());
                peak = juce::jmax (peak, run<float> (p, rng, 1, 1 + rng.nextInt (700)).peak);
            }
            std::cout << (base ? "widths 200 + stereoize 100, " : "defaults, ") << g.name << " jumping: peak " << peak << std::endl;
        }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 1 && juce::String (argv[1]) == "diag") { diag(); return 0; }
    engineTests();
    panelTests();
    std::cout << "\n" << (failed == 0 ? "ALL " + std::to_string (checks) + " CHECKS PASSED" : std::to_string (failed) + " OF " + std::to_string (checks) + " CHECKS FAILED") << "\n";
    return failed == 0 ? 0 : 1;
}
