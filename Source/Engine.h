// Tobo Imager - the signal processing. Copyright (C) 2026 Tobokegao. Licensed under the GNU AGPL v3 or later.
// A line by line port of the JSFX effect it started as (tobo_imager.jsfx, v22; not part of this repository).
//
// Mid/side imager in the manner of a multiband imager:
//  - 3 crossovers -> 4 bands, each with its own width (side gain) and "stereoize" amount
//  - stereoize makes a side signal out of the mid, opposite polarity left/right, so a mono source gets width.
//    type 1 (default): the mid is convolved with a 30 ms velvet-noise sequence (30 sparse impulses, decaying,
//    positions and gains optimised for a flat response per ear; after the velvet-noise decorrelator papers,
//    Alary/Politis/Valimaki DAFx-17 and Schlecht/Alary/Valimaki/Habets DAFx-18). No single echo, no regular comb.
//    type 2: the same with the 60 dB total decay of the paper: shorter tail, sharper transients, a little less width in the lows
//    type 0: one delayed copy (Lauridsen-Schodder / Haas style)
//    type 3: an all-pass side filter. Its phase is 90 degrees (so both ears keep the level of the mid at every
//    frequency) plus a random part built like the FVN of Kawahara et al. (frequency domain variant of velvet noise:
//    six-term cosine bumps of +/- phase placed on the frequency axis by the velvet-noise rule).
//    "FVN scatter" sets the size of the random part. Done by FFT block convolution, so this type adds latency (reported to the host).
//  - transient guard (after the transient handling of Das, DAFx-24): the stereoize input is turned down for the
//    first tens of ms of a new attack, so attacks stay in the centre. 1 ms look-ahead, no latency.
//  - safe (per band; the idea of the antiphase prevention of other imagers): holds the side of the band at or below
//    the level of its mid, so the band cannot turn out of phase (correlation below 0) however far it is widened.
//    It turns down what the effect adds to that band (width above 100 % and stereoize) by the needed amount, from the
//    measured levels (30 ms). It does not narrow a band below what came in. With the FVN type only the width is turned down.
//  - the level of the mid is never touched: the mono sum of the output has the spectrum of the mono sum of the input
// The output is the sum of the four bands of mid and of side, and the band split is made to add up to an all-pass
// (each half of the tree also goes through the all-pass of the other half's crossover), so a width is exactly a gain
// on that band. The price: mid and side both carry the phase of the crossovers (the same on both, levels unchanged).
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

namespace tobo
{
constexpr double kPi = 3.14159265358979323846;

// the parameters, in the order of the JSFX sliders 1..29
enum Param
{
    pXo1, pXo2, pXo3,
    pW1, pW2, pW3, pW4,
    pA1, pA2, pA3, pA4,
    pDelay, pType, pGuard, pScatter, pSolo, pMonitor,
    pOn1, pOn2, pOn3, pOn4,
    pMute1, pMute2, pMute3, pMute4,
    pSafe1, pSafe2, pSafe3, pSafe4,
    pBands,                   // 0..3 = one to four bands (not in the JSFX, which always has four)
    kNumParams
};

struct Biquad
{
    double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    void lp (double f, double sr)
    {
        double w = 2 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * 0.70710678), n = 1 / (1 + al);
        b0 = (1 - c) / 2 * n; b1 = (1 - c) * n; b2 = b0; a1 = -2 * c * n; a2 = (1 - al) * n;
    }
    void hp (double f, double sr)
    {
        double w = 2 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * 0.70710678), n = 1 / (1 + al);
        b0 = (1 + c) / 2 * n; b1 = -(1 + c) * n; b2 = b0; a1 = -2 * c * n; a2 = (1 - al) * n;
    }
    // 2nd order all-pass with the phase of an LR4 low + high pair at f
    void ap (double f, double sr)
    {
        double w = 2 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * 0.70710678), n = 1 / (1 + al);
        b0 = (1 - al) * n; b1 = -2 * c * n; b2 = 1; a1 = b1; a2 = b0;
    }
    double run (double x)
    {
        double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
    void clear() { x1 = x2 = y1 = y2 = 0; }
    double mag2 (double w) const
    {
        double c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2 * w), s2 = std::sin (2 * w);
        auto sq = [] (double v) { return v * v; };
        return (sq (b0 + b1 * c1 + b2 * c2) + sq (b1 * s1 + b2 * s2)) / (sq (1 + a1 * c1 + a2 * c2) + sq (a1 * s1 + a2 * s2));
    }
};

// Linkwitz-Riley 4th order = two Butterworth 2nd order in series
struct LR4
{
    Biquad a, b;
    void lp (double f, double sr) { a.lp (f, sr); b.lp (f, sr); }
    void hp (double f, double sr) { a.hp (f, sr); b.hp (f, sr); }
    double run (double x) { return b.run (a.run (x)); }
    void clear() { a.clear(); b.clear(); }
};

// The band split, for one to four bands. Four bands: split at f2, then the low half at f1 and the high half at f3; each
// half also goes through the all-pass of the other half's crossover, so that the four bands add up to an all-pass.
// Three bands (crossovers f1 < f2): split at f2, the low half split at f1, the high half through the all-pass of f1.
// Two bands: one split at f1. One band: the signal as it is. The bands that are not there give 0.
struct Tree
{
    LR4 lo, hi, l1, l2, h3, h4;
    Biquad c12, c34;
    double o1 = 0, o2 = 0, o3 = 0, o4 = 0;
    int nb = 4;
    void set (int bands, double f1, double f2, double f3, double sr)
    {
        nb = bands;
        if (nb == 4)
        {
            lo.lp (f2, sr); hi.hp (f2, sr);
            l1.lp (f1, sr); l2.hp (f1, sr);
            h3.lp (f3, sr); h4.hp (f3, sr);
            c12.ap (f3, sr); c34.ap (f1, sr);
        }
        else if (nb == 3) { lo.lp (f2, sr); hi.hp (f2, sr); l1.lp (f1, sr); l2.hp (f1, sr); c34.ap (f1, sr); }
        else if (nb == 2) { lo.lp (f1, sr); hi.hp (f1, sr); }
    }
    void run (double x)
    {
        if (nb == 4)
        {
            double l = c12.run (lo.run (x)), h = c34.run (hi.run (x));
            o1 = l1.run (l); o2 = l2.run (l);
            o3 = h3.run (h); o4 = h4.run (h);
        }
        else if (nb == 3) { const double l = lo.run (x); o1 = l1.run (l); o2 = l2.run (l); o3 = c34.run (hi.run (x)); o4 = 0; }
        else if (nb == 2) { o1 = lo.run (x); o2 = hi.run (x); o3 = o4 = 0; }
        else { o1 = x; o2 = o3 = o4 = 0; }
    }
    // how strong band k (0..3) is at the angular frequency w: the magnitude of its path through the split
    double bandMag (int k, double w) const
    {
        if (k >= nb) return 0;
        if (nb == 1) return 1;
        if (nb == 2) return k == 0 ? lo.a.mag2 (w) : hi.a.mag2 (w);
        if (k < 2) return lo.a.mag2 (w) * (k == 0 ? l1.a.mag2 (w) : l2.a.mag2 (w));
        if (nb == 3) return hi.a.mag2 (w);
        return hi.a.mag2 (w) * (k == 2 ? h3.a.mag2 (w) : h4.a.mag2 (w));
    }
    void clear() { lo.clear(); hi.clear(); l1.clear(); l2.clear(); h3.clear(); h4.clear(); c12.clear(); c34.clear(); o1 = o2 = o3 = o4 = 0; }
};

// in-place complex FFT, interleaved re/im, n a power of two; inverse is not scaled
inline void fft (double* d, int n, bool inverse)
{
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap (d[2 * i], d[2 * j]); std::swap (d[2 * i + 1], d[2 * j + 1]); }
    }
    for (int len = 2; len <= n; len <<= 1)
    {
        double ang = 2 * kPi / len * (inverse ? 1 : -1), wr = std::cos (ang), wi = std::sin (ang);
        for (int i = 0; i < n; i += len)
        {
            double cr = 1, ci = 0;
            for (int k = 0; k < len / 2; ++k)
            {
                double* u = d + 2 * (i + k); double* v = d + 2 * (i + k + len / 2);
                double tr = v[0] * cr - v[1] * ci, ti = v[0] * ci + v[1] * cr;
                v[0] = u[0] - tr; v[1] = u[1] - ti; u[0] += tr; u[1] += ti;
                double nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
            }
        }
    }
}

struct SafeBand        // gain worked out from the input
{
    double em = 0, es = 0, g = 1;
};
struct TrimBand        // slow trim from the measured output
{
    double em = 0, es = 0, fb = 1;
};

class Engine
{
public:
    static constexpr int BUF = 16384, NV = 30, SCN = 1024, SPN = 4096;

    // what the panel reads (written by the audio thread; a torn read only moves a dot for one frame)
    float scl[SCN] = {}, scr[SCN] = {};
    float sgl[SPN] = {}, sgr[SPN] = {};
    std::atomic<int> sgw { 0 };
    std::atomic<float> imm { 0 }, iss { 0 }, omm { 0 }, oss { 0 }, sll { 0 }, srr { 0 }, slr { 0 };
    std::atomic<unsigned> smpc { 0 };     // lets the panel see whether audio is being processed
    std::atomic<bool> clearMeters { false };

    double srate = 48000;
    int latency = 0;

    void prepare (double sampleRate)
    {
        srate = sampleRate;
        FL = srate <= 50000 ? 2048 : srate <= 100000 ? 4096 : 8192;
        FN = FL * 2; FHOP = FL / 4; FCEN = FL / 2;
        for (auto* v : { &d1, &d2, &d3, &d4, &vb, &raw, &rmid, &rsid, &dm, &ds }) v->assign (BUF, 0.0);
        kern.assign ((size_t) FN * 2, 0.0); fb.assign ((size_t) FN * 2, 0.0); ola.assign ((size_t) FN, 0.0); inb.assign ((size_t) FHOP, 0.0);
        fph.assign ((size_t) FCEN + 1, 0.0); ftmp.assign ((size_t) FL * 2, 0.0);
        pos = 0; ip = 0; snap = true; gliding = false; gctr = 0;
        glk = 1 - std::exp (-16 / (0.012 * srate));        // 12 ms to cover 63 % of the way
        KLA = (int) std::floor (0.001 * srate);
        gr_rel = std::exp (-1 / (0.020 * srate)); gr_c = 1 - std::exp (-1 / (0.060 * srate));
        gc_a = 1 - std::exp (-1 / (0.0002 * srate)); gc_r = 1 - std::exp (-1 / (0.010 * srate));
        mt_c = 1 - std::exp (-1 / (0.3 * srate));
        ef = 0; esl = 0; gs = 1; kscat = -1; ltyp = -1;
        he1 = he2 = 0;
        for (int i = 0; i < NV; ++i) { he1 += vg[i] * vg[i]; he2 += vg2[i] * vg2[i]; }
        sf_c = 1 - std::exp (-1 / (0.030 * srate)); sf_g = 1 - std::exp (-1 / (0.010 * srate));
        sf_fb = 1 / (0.15 * srate);
        for (auto& b : sb) b = {};
        for (auto& b : tb) b = {};
        ts.clear(); tm.clear(); tom.clear(); tos.clear();
        scw = 0;
        m_imm = m_iss = m_omm = m_oss = m_sll = m_srr = m_slr = 0;
    }

    // p = the parameter values: the first 29 in the units of the JSFX sliders, then the number of bands (0..3 = 1..4)
    void update (const double* p)
    {
        const int bands = std::max (1, std::min (4, (int) std::floor (p[pBands] + 0.5) + 1));
        bool jump = snap;                         // the first settings after a start are taken at once
        if (bands != nbands)
        {
            // filters that come into use must not start from what was left in them
            ts.clear(); tm.clear(); tom.clear(); tos.clear();
            for (auto& b : sb) b = {};
            for (auto& b : tb) b = {};
            nbands = bands; jump = true;
        }
        // a crossover at or above half the sample rate would make its filters unstable (8 kHz, 22.05 kHz): kept below
        const double fmax = 0.45 * srate;
        // the crossovers in use (one fewer than the bands), in rising order whatever order the host has set them in
        double fq[3] = { std::min (p[pXo1], fmax), std::min (p[pXo2], fmax), std::min (p[pXo3], fmax) };
        std::sort (fq, fq + (bands - 1));
        const double f1 = fq[0], f2 = fq[1], f3 = fq[2];
        // The filters do not jump to a new crossover: they glide there in a few hundredths of a second (see glide()).
        // A filter whose frequency jumps keeps what the old one left in it, and that can come out as a burst many
        // times louder than the sound (a host can automate a crossover from one end to the other in one step).
        for (int k = 0; k < 3; ++k) ftgt[k] = fq[k];
        if (jump) { for (int k = 0; k < 3; ++k) fcur[k] = ftgt[k]; setTrees(); snap = false; gliding = false; }
        else gliding = fcur[0] != ftgt[0] || fcur[1] != ftgt[1] || fcur[2] != ftgt[2];
        tk.set (bands, f1, f2, f3, srate);        // the split as it will be, for the FVN kernel
        double a[4];
        bool on[4];
        for (int k = 0; k < 4; ++k)
        {
            on[k] = k < bands && p[pOn1 + k] > 0.5;
            // a band that is switched off passes as it came: width 100 %, no stereoize
            w[k] = on[k] ? p[pW1 + k] / 100 - 1 : 0;
            a[k] = on[k] ? p[pA1 + k] / 100 : 0;
        }
        typ = (int) std::floor (p[pType] + 0.5); int solo = (int) std::floor (p[pSolo] + 0.5); mon = (int) std::floor (p[pMonitor] + 0.5);
        if (solo > bands) solo = 0;               // a solo on a band that is not there counts as no solo
        guard = p[pGuard] / 100;
        bool sfa = false;
        for (int k = 0; k < 4; ++k)
        {
            // which bands are heard: with a solo, only that band; otherwise every band that is not muted
            pp[k] = k >= bands ? 0.0 : solo > 0 ? (solo == k + 1 ? 1.0 : 0.0) : (p[pMute1 + k] < 0.5 ? 1.0 : 0.0);
            q[k] = pp[k] * (1 + w[k]);
            aa[k] = pp[k] * a[k];
            qs[k] = pp[k] * std::max (w[k], 0.0);      // the part of the side gain that safe may take away
            wb[k] = 1 + std::min (w[k], 0.0);          // the part that stays
            sf[k] = p[pSafe1 + k] > 0.5 && on[k];      // safe works only on a band that is switched on
            if (! sf[k]) { sb[k].g = 1; tb[k].fb = 1; }
            sfa = sfa || sf[k];
        }
        sfany = sfa;
        hev = typ == 2 ? he2 : typ == 1 ? he1 : 1;
        // a different delay per band keeps the comb teeth of the bands from lining up
        n[0] = std::min (BUF - 1, (int) std::floor (p[pDelay] * 0.001 * srate + 0.5));
        n[1] = std::max (1, (int) std::floor (n[0] * 0.81)); n[2] = std::max (1, (int) std::floor (n[0] * 0.62)); n[3] = std::max (1, (int) std::floor (n[0] * 0.47));
        // the velvet buffer is filled KLA samples late (look-ahead of the guard), so the impulses sit KLA samples earlier
        for (int k = 0; k < NV; ++k)
        {
            vn[k] = std::max (1, (int) std::floor ((typ == 2 ? vt2[k] : vt[k]) * 0.001 * srate + 0.5) - KLA);
            vga[k] = typ == 2 ? vg2[k] : vg[k];
        }
        if (typ == 3)
        {
            const double ksig = p[pScatter] + 3 * f1 + 7 * f2 + 11 * f3 + 1301 * aa[0] + 1709 * aa[1] + 2203 * aa[2] + 2707 * aa[3] + 0.37 * bands;
            if (kscat != ksig) { fvnMake (p[pScatter] / 100); kscat = ksig; }
            if (ltyp != 3) { std::fill (ola.begin(), ola.end(), 0.0); std::fill (inb.begin(), inb.end(), 0.0); ip = 0; }
            latency = FHOP - 1 + FCEN + KLA;
        }
        else latency = 0;
        ltyp = typ;
    }

    int fvnLatency() const { return FHOP - 1 + FCEN + KLA; }

    template <typename Float>
    void process (Float* L, Float* R, int num)
    {
        if (clearMeters.exchange (false)) { m_imm = m_iss = m_omm = m_oss = m_sll = m_srr = m_slr = 0; }
        for (int i = 0; i < num; ++i)
        {
            if (gliding && (++gctr & 15) == 0) glide();
            // a sample that is not a number (or absurdly large) would stay in the filters for ever: taken as silence
            const double in0 = L[i], in1 = R[i];
            const double spl0 = std::abs (in0) < 1e6 ? in0 : 0.0, spl1 = std::abs (in1) < 1e6 ? in1 : 0.0;
            const double m = (spl0 + spl1) * 0.5, s = (spl0 - spl1) * 0.5;
            ts.run (s); tm.run (m);
            const double tmo[4] = { tm.o1, tm.o2, tm.o3, tm.o4 }, tso[4] = { ts.o1, ts.o2, ts.o3, ts.o4 };
            double mo = pp[0] * tmo[0] + pp[1] * tmo[1] + pp[2] * tmo[2] + pp[3] * tmo[3];
            double sd = 0, ab[4];
            if (sfany)
            {
                // safe: each band's widening is turned down by its own gain (1 = untouched)
                for (int k = 0; k < 4; ++k)
                {
                    const double sg = sf[k] ? std::min (1.0, safeg (sb[k], tmo[k], tso[k], wb[k], std::max (w[k], 0.0), aa[k]) * tb[k].fb) : 1.0;
                    sd += (q[k] + (sg - 1) * qs[k]) * tso[k];
                    ab[k] = aa[k] * sg;
                }
            }
            else
            {
                for (int k = 0; k < 4; ++k) { sd += q[k] * tso[k]; ab[k] = aa[k]; }
            }
            // transient guard: fast peak follower against a slow one; a new attack turns the stereoize input down
            const double am = std::abs (m);
            ef = std::max (am, ef * gr_rel); esl += (ef - esl) * gr_c;
            const double rr = ef / (esl + 0.00001);
            const double gt = rr > 3 ? 0 : rr < 1.8 ? 1 : (3 - rr) / 1.2;
            gs += (gt - gs) * (gt < gs ? gc_a : gc_r);
            const double gg = 1 - guard * (1 - gs);
            d1[pos] = tmo[0]; d2[pos] = tmo[1]; d3[pos] = tmo[2]; d4[pos] = tmo[3];
            rmid[pos] = tmo[0] + tmo[1] + tmo[2] + tmo[3];   // the whole mid through the all-pass of the split
            rsid[pos] = tso[0] + tso[1] + tso[2] + tso[3];   // and the whole side: with rmid, what comes out when nothing is turned
            raw[pos] = ab[0] * tmo[0] + ab[1] * tmo[1] + ab[2] * tmo[2] + ab[3] * tmo[3];
            const double xg = raw[at (pos - KLA)] * gg;
            vb[pos] = xg;
            double s2;
            if (typ == 0)
            {
                s2 = sd + gg * (ab[0] * d1[at (pos - n[0])] + ab[1] * d2[at (pos - n[1])] + ab[2] * d3[at (pos - n[2])] + ab[3] * d4[at (pos - n[3])]);
            }
            else if (typ == 3)
            {
                // all-pass side filter by block convolution; the mid and the direct side wait for it
                dm[pos] = mo; ds[pos] = sd;
                mo = dm[at (pos - latency)]; sd = ds[at (pos - latency)];
                inb[(size_t) ip] = rmid[at (pos - KLA)] * gg; ++ip;
                if (ip >= FHOP) { fvnBlock(); ip = 0; }
                s2 = sd + ola[(size_t) ip];
            }
            else
            {
                // velvet noise: the bands of the mid, weighted by their amounts, go through one sparse sequence
                double vacc = 0;
                for (int k = 0; k < NV; ++k) vacc += vga[k] * vb[at (pos - vn[k])];
                s2 = sd + vacc;
            }
            // what comes out when nothing is turned (every band heard, every width 100 %, no stereoize), as late as the
            // output is: the delta monitor plays the output less this, which is what the settings add or take away
            const double nmid = rmid[at (pos - latency)], nsid = rsid[at (pos - latency)];
            pos = (pos + 1) % BUF;
            if (sfany)
            {
                tom.run (mo); tos.run (s2);
                const double omb[4] = { tom.o1, tom.o2, tom.o3, tom.o4 }, osb[4] = { tos.o1, tos.o2, tos.o3, tos.o4 };
                for (int k = 0; k < 4; ++k) if (sf[k]) safefb (tb[k], omb[k], osb[k], sb[k].g);
            }
            const double ol = mo + s2, orr = mo - s2;
            m_sll += (ol * ol - m_sll) * mt_c; m_srr += (orr * orr - m_srr) * mt_c; m_slr += (ol * orr - m_slr) * mt_c;
            if (scw & 1) { scl[scw >> 1] = (float) ol; scr[scw >> 1] = (float) orr; }
            scw = (scw + 1) % (SCN * 2);
            sgl[m_sgw] = (float) ol; sgr[m_sgw] = (float) orr; m_sgw = (m_sgw + 1) % SPN;
            // for the dot meter: how strong mid and side are, going in and coming out
            m_imm += (m * m - m_imm) * mt_c; m_iss += (s * s - m_iss) * mt_c; m_omm += (mo * mo - m_omm) * mt_c; m_oss += (s2 * s2 - m_oss) * mt_c;
            if (mon == 1) { L[i] = (Float) mo; R[i] = (Float) mo; }
            else if (mon == 2) { L[i] = (Float) s2; R[i] = (Float) s2; }
            else if (mon == 3) { L[i] = (Float) ((mo - nmid) + (s2 - nsid)); R[i] = (Float) ((mo - nmid) - (s2 - nsid)); }
            else { L[i] = (Float) ol; R[i] = (Float) orr; }
        }
        sgw.store (m_sgw, std::memory_order_relaxed);
        imm = (float) m_imm; iss = (float) m_iss; omm = (float) m_omm; oss = (float) m_oss;
        sll = (float) m_sll; srr = (float) m_srr; slr = (float) m_slr;
        smpc.fetch_add ((unsigned) num, std::memory_order_relaxed);
    }

private:
    static size_t at (int i) { return (size_t) ((i + BUF) % BUF); }

    // Safe turns down only what the effect adds to a band: the width above 100 % and the stereoize amount, both by one
    // gain g (1 = untouched, 0 = the band's side is the input's side again). It never narrows a band below what came in.
    // Side that will come out = (base + g*wp)^2 * side + (g*aa)^2 * (energy of the stereoize filter) * mid <= mid.
    // FVN: the stereoize part cannot be turned per band (it sits in the kernel), so only the added width is turned down.
    double safeg (SafeBand& b, double mo2, double so2, double base, double wp, double a) const
    {
        b.em += (mo2 * mo2 - b.em) * sf_c; b.es += (so2 * so2 - b.es) * sf_c;
        double t;
        if (typ == 3)
            t = wp > 0 ? (std::sqrt (std::max (0.0, 1 - a * a) * b.em / (b.es + 1e-24)) - base) / wp : 1;
        else
        {
            const double qa = wp * wp * b.es + a * a * hev * b.em, qb = 2 * base * wp * b.es, qc = base * base * b.es - b.em;
            t = qc >= 0 ? 0 : qa > 0 ? (-qb + std::sqrt (qb * qb - 4 * qa * qc)) / (2 * qa) : 1;
        }
        t = std::min (1.0, std::max (0.0, t));
        b.g += (t - b.g) * sf_g;
        return b.g;
    }
    // the output is measured too, band by band, and a slow trim (about 0.15 s) on top of that gain closes the rest
    void safefb (TrimBand& b, double mo2, double so2, double gff) const
    {
        b.em += (mo2 * mo2 - b.em) * sf_c; b.es += (so2 * so2 - b.es) * sf_c;
        b.fb = std::min (1 / std::max (gff, 0.05), std::max (0.05, b.fb * (1 + sf_fb * (0.9 * b.em - b.es) / (b.em + b.es + 1e-24))));
    }

    double prng()   // Park-Miller, exact in doubles
    {
        double x = rseed * 16807; rseed = x - std::floor (x / 2147483647.0) * 2147483647.0; return rseed / 2147483647.0;
    }
    static double bump (double u)
    {
        return 0.2624710164 + 0.4265335164 * std::cos (u) + 0.2250165621 * std::cos (2 * u) + 0.0726831633 * std::cos (3 * u) + 0.0125124215 * std::cos (4 * u) + 0.0007833203 * std::cos (5 * u);
    }
    // FVN side kernel (type 3): phase(f) = t(f) * (90 deg + sum of +/- phimax six-term cosine bumps);
    // magnitude(f) = sum over the bands of amount * |band filter|
    void fvnMake (double scat)
    {
        rseed = 20261115; const double pm = scat * kPi / 4;
        const double df = srate / FL, Fd = 1 / (5 * 0.008), B = 10 * Fd;
        std::fill (fph.begin(), fph.end(), 0.0);
        const int nb = (int) std::ceil (srate * 0.5 / Fd);
        for (int m = 0; m < nb; ++m)
        {
            const double r1 = prng(); const double fc = (m + r1) * Fd; const double sg = prng() < 0.5 ? pm : -pm;
            int k0 = std::max (0, (int) std::ceil ((fc - B) / df)), k1 = std::min (FCEN, (int) std::floor ((fc + B) / df));
            for (int k = k0; k <= k1; ++k) fph[(size_t) k] += sg * bump (kPi * (k * df - fc) / B);
            k1 = std::min (FCEN, (int) std::floor ((B - fc) / df));       // the mirror image of the bump around 0 Hz
            for (int k = 0; k <= k1; ++k) fph[(size_t) k] -= sg * bump (kPi * (k * df + fc) / B);
        }
        for (int k = 0; k <= FCEN; ++k)
        {
            const double up = std::min (1.0, k * df / 150), dn = std::min (1.0, std::max (0.0, (srate * 0.5 - k * df) / 2000));
            const double t = (0.5 - 0.5 * std::cos (kPi * up)) * (0.5 - 0.5 * std::cos (kPi * dn));
            const double om = 2 * kPi * k / FL;
            const double ww = aa[0] * tk.bandMag (0, om) + aa[1] * tk.bandMag (1, om) + aa[2] * tk.bandMag (2, om) + aa[3] * tk.bandMag (3, om);
            const double d = (fph[(size_t) k] + kPi * 0.5) * t, c = ww * std::cos (d), s = ww * std::sin (d);
            ftmp[(size_t) (2 * k)] = c; ftmp[(size_t) (2 * k + 1)] = s;
            if (k > 0 && k < FCEN) { ftmp[(size_t) (2 * (FL - k))] = c; ftmp[(size_t) (2 * (FL - k) + 1)] = -s; }
        }
        fft (ftmp.data(), FL, true);
        std::fill (kern.begin(), kern.end(), 0.0);
        for (int k = 0; k < FL; ++k) kern[(size_t) (2 * ((k + FCEN) % FL))] = ftmp[(size_t) (2 * k)] / FL;      // centre of the response at FCEN
        fft (kern.data(), FN, false);
    }
    void fvnBlock()
    {
        std::memmove (ola.data(), ola.data() + FHOP, sizeof (double) * (size_t) (FN - FHOP));
        std::fill (ola.begin() + (FN - FHOP), ola.end(), 0.0);
        std::fill (fb.begin(), fb.end(), 0.0);
        for (int k = 0; k < FHOP; ++k) fb[(size_t) (2 * k)] = inb[(size_t) k];
        fft (fb.data(), FN, false);
        for (int k = 0; k < FN; ++k)
        {
            const double ar = fb[(size_t) (2 * k)], ai = fb[(size_t) (2 * k + 1)], br = kern[(size_t) (2 * k)], bi = kern[(size_t) (2 * k + 1)];
            fb[(size_t) (2 * k)] = ar * br - ai * bi; fb[(size_t) (2 * k + 1)] = ar * bi + ai * br;
        }
        fft (fb.data(), FN, true);
        const double inv = 1.0 / FN;
        for (int k = 0; k < FN; ++k) ola[(size_t) k] += fb[(size_t) (2 * k)] * inv;
    }

    int FL = 2048, FN = 4096, FHOP = 512, FCEN = 1024, KLA = 48, nbands = 4;
    std::vector<double> d1, d2, d3, d4, vb, raw, rmid, rsid, dm, ds, kern, fb, ola, inb, fph, ftmp;
    Tree ts, tm, tom, tos, tk;
    double fcur[3] = { 120, 1000, 5000 }, ftgt[3] = { 120, 1000, 5000 }, glk = 0;
    bool snap = true, gliding = false; int gctr = 0;

    void setTrees()
    {
        ts.set (nbands, fcur[0], fcur[1], fcur[2], srate); tm.set (nbands, fcur[0], fcur[1], fcur[2], srate);
        tom.set (nbands, fcur[0], fcur[1], fcur[2], srate); tos.set (nbands, fcur[0], fcur[1], fcur[2], srate);
    }
    // every 16 samples while a crossover is on its way: a step towards where it has to go, the same part of the way
    // left (in octaves) each time, and the last hundredth of a semitone in one go
    void glide()
    {
        bool more = false;
        for (int k = 0; k < 3; ++k)
        {
            if (fcur[k] == ftgt[k]) continue;
            const double r = std::log (ftgt[k] / fcur[k]);
            if (std::abs (r) < 0.0006) fcur[k] = ftgt[k]; else { fcur[k] *= std::exp (r * glk); more = true; }
        }
        setTrees(); gliding = more;
    }
    SafeBand sb[4]; TrimBand tb[4];
    double w[4] = {}, q[4] = { 1, 1, 1, 1 }, aa[4] = {}, qs[4] = {}, wb[4] = { 1, 1, 1, 1 }, pp[4] = { 1, 1, 1, 1 };
    bool sf[4] = {}, sfany = false;
    int n[4] = { 1, 1, 1, 1 }, vn[NV] = {}, typ = 1, mon = 0, ltyp = -1, pos = 0, ip = 0, scw = 0, m_sgw = 0;
    double vga[NV] = {}, guard = 0, hev = 1, he1 = 0, he2 = 0, kscat = -1, rseed = 1;
    double gr_rel = 0, gr_c = 0, gc_a = 0, gc_r = 0, mt_c = 0, sf_c = 0, sf_g = 0, sf_fb = 0, ef = 0, esl = 0, gs = 1;
    double m_imm = 0, m_iss = 0, m_omm = 0, m_oss = 0, m_sll = 0, m_srr = 0, m_slr = 0;

    static constexpr double vt[NV] = { 1.1338, 1.7687, 3.22, 4.5578, 4.6939, 5.6689, 7.5964, 8.458, 8.5941, 10.0, 11.3152, 11.9728, 13.0159, 14.3537, 15.3741, 16.3492, 16.4853, 17.5964, 18.8662, 20.1814, 21.0204, 21.7914, 23.0612, 23.1973, 24.8526, 25.8277, 26.7574, 27.0975, 28.7302, 29.6599 };
    static constexpr double vg[NV] = { 0.32917, 0.26657, -0.21528, 0.43221, -0.40862, -0.35203, 0.26695, -0.25135, -0.21971, 0.16989, 0.15468, -0.1273, 0.10449, -0.09495, 0.08611, -0.06764, -0.06407, -0.04925, 0.04539, -0.0388, -0.03239, 0.02636, -0.01106, -0.02021, 0.00753, 0.01602, 0.00604, -0.00479, 0.01014, -0.00874 };
    static constexpr double vt2[NV] = { 1.746, 1.7914, 3.3107, 4.2404, 5.1927, 6.4172, 6.7347, 7.619, 8.5941, 10.2494, 11.3832, 11.7687, 13.1519, 13.4467, 15.3515, 15.3968, 17.1882, 18.1633, 18.7528, 19.9773, 20.2721, 21.542, 22.9478, 23.6961, 24.7846, 25.6463, 26.5306, 27.2336, 28.254, 29.3651 };
    static constexpr double vg2[NV] = { 0.45594, -0.44459, 0.44529, -0.3798, -0.31676, -0.23756, 0.21066, -0.11117, 0.13024, 0.09048, -0.07157, 0.06332, 0.03371, 0.03533, 0.02088, 0.0246, 0.01703, -0.01009, 0.01182, -0.00626, 0.00835, 0.00611, -0.00302, 0.00246, -0.00186, 0.00248, 0.00191, 0.0016, -0.00087, -0.0007 };
};
} // namespace tobo
