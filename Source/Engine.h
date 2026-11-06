// SQN Ears engine - Siddha Quantum Nexus Music. Framework-free DSP (C++17).
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace sqn
{
constexpr double kPi = 3.14159265358979323846;
inline double dB10 (double x) { return x <= 0.0 ? -144.0 : 10.0 * std::log10 (x); }
inline double dB20 (double x) { return x <= 0.0 ? -144.0 : 20.0 * std::log10 (x); }
template <typename T> inline T clampv (T v, T a, T b) { return v < a ? a : (v > b ? b : v); }

//============================================================================== RBJ biquad
enum class Bq { LowPass, HighPass, BandPass0dB, Peaking, LowShelf, HighShelf };
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (Bq t, double f, double Q, double g, double sr)
    {
        f = std::min (f, sr * 0.45);
        const double A = std::pow (10.0, g / 40.0), w = 2.0 * kPi * f / sr, cs = std::cos (w), sn = std::sin (w), al = sn / (2.0 * Q);
        double B0, B1, B2, A0, A1, A2;
        switch (t)
        {
            case Bq::LowPass:     B0 = (1 - cs) / 2; B1 = 1 - cs; B2 = B0; A0 = 1 + al; A1 = -2 * cs; A2 = 1 - al; break;
            case Bq::HighPass:    B0 = (1 + cs) / 2; B1 = -(1 + cs); B2 = B0; A0 = 1 + al; A1 = -2 * cs; A2 = 1 - al; break;
            case Bq::BandPass0dB: B0 = al; B1 = 0; B2 = -al; A0 = 1 + al; A1 = -2 * cs; A2 = 1 - al; break;
            case Bq::Peaking:     B0 = 1 + al * A; B1 = -2 * cs; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * cs; A2 = 1 - al / A; break;
            case Bq::LowShelf:
            {
                const double s = 2 * std::sqrt (A) * al;
                B0 = A * ((A + 1) - (A - 1) * cs + s); B1 = 2 * A * ((A - 1) - (A + 1) * cs); B2 = A * ((A + 1) - (A - 1) * cs - s);
                A0 = (A + 1) + (A - 1) * cs + s; A1 = -2 * ((A - 1) + (A + 1) * cs); A2 = (A + 1) + (A - 1) * cs - s; break;
            }
            default:
            {
                const double s = 2 * std::sqrt (A) * al;
                B0 = A * ((A + 1) + (A - 1) * cs + s); B1 = -2 * A * ((A - 1) + (A + 1) * cs); B2 = A * ((A + 1) + (A - 1) * cs - s);
                A0 = (A + 1) - (A - 1) * cs + s; A1 = 2 * ((A - 1) - (A + 1) * cs); A2 = (A + 1) - (A - 1) * cs - s; break;
            }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    inline double run (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
};

//============================================================================== the 11 sounds
struct Style { const char* name; float lufs; float off[9]; float vc, glue, harsh, wid; };
inline const std::array<Style, 11>& styles()
{
    static const std::array<Style, 11> s { {
        { "Sovereign",       -11.0f, { 0, 0, 0, 0, 0, 0, 0, 0, 0 },                      0.00f, 0.5f, 1.0f,  0.0f },
        { "Zion Roots",      -13.0f, { 1.5f, 1.5f, 0.5f, 0.5f, 0.5f, 0, -1, -1.5f, -2.5f }, 0.40f, 0.4f, 1.2f, -0.5f },
        { "Kingston 1973",   -13.0f, { 1.5f, 1.5f, 1, 0.5f, 0, -0.5f, -1.5f, -3, -5 },    0.70f, 0.6f, 1.3f, -1.0f },
        { "Island '77",      -12.0f, { 2, 1.5f, 0.5f, 0, 0, 0, -0.5f, -1.5f, -3 },        0.50f, 0.6f, 1.1f,  0.0f },
        { "Dub Chamber",     -12.0f, { 2.5f, 2, 0, -0.5f, -0.5f, -1, -1.5f, -2, -3 },     0.50f, 0.5f, 1.2f,  1.0f },
        { "West Coast Gold", -10.0f, { 1.5f, 1, -1.5f, -0.5f, 0, 0.5f, 1, 1.5f, 1.5f },   0.15f, 0.7f, 1.0f,  1.0f },
        { "G-Funk '96",      -10.0f, { 1.5f, 1.5f, -0.5f, 0, 0.5f, 0.5f, 0, 0, -0.5f },   0.25f, 0.7f, 1.0f,  1.0f },
        { "Golden Era '94",  -10.0f, { 0, 0.5f, 0.5f, 1, 1, 0.5f, 0, -1.5f, -3 },         0.30f, 0.8f, 1.0f, -0.5f },
        { "808 Modern",       -9.0f, { 2.5f, 1.5f, -1, -0.5f, 0, 0, 0.5f, 1.5f, 2 },      0.00f, 0.6f, 1.0f,  1.0f },
        { "Crystal Pop '82", -11.5f, { 0.5f, 0.5f, -1, -0.5f, 0, 0.5f, 1, 1.5f, 1.5f },   0.10f, 0.5f, 1.0f,  1.5f },
        { "Mantra Temple",   -16.0f, { -1, -1, -0.5f, 0, 0, 0, 0, 0.5f, 1 },              0.20f, 0.2f, 1.2f,  1.5f },
    } };
    return s;
}

//============================================================================== BS.1770 gated loudness
struct Meter
{
    std::array<int, 701> hn {}; std::array<double, 701> he {};
    double se = 0, ring[4] = { 0, 0, 0, 0 }; int sn = 0, ri = 0, blocks = 0, sublen = 4410;
    void reset() { hn.fill (0); he.fill (0); se = 0; sn = 0; ri = 0; blocks = 0; for (auto& r : ring) r = 0; }
    inline void feed (double a, double b)
    {
        se += a * a + b * b;
        if (++sn >= sublen)
        {
            ri = (ri + 1) % 4; ring[ri] = se / sn; se = 0; sn = 0;
            const double e = (ring[0] + ring[1] + ring[2] + ring[3]) / 4, lu = -0.691 + dB10 (e);
            if (lu > -70) { int bin = std::min (700, (int) std::floor ((lu + 70) * 10)); hn[(size_t) bin]++; he[(size_t) bin] += e; blocks++; }
        }
    }
    double integrated() const
    {
        double n = 0, e = 0;
        for (size_t i = 0; i <= 700; ++i) { n += hn[i]; e += he[i]; }
        if (n == 0) return -70;
        const double rel = -0.691 + dB10 (e / n) - 10;
        double n2 = 0, e2 = 0;
        for (size_t i = 0; i <= 700; ++i) if ((double) i / 10 - 70 > rel) { n2 += hn[i]; e2 += he[i]; }
        return n2 == 0 ? -70 : -0.691 + dB10 (e2 / n2);
    }
};
struct KWeight
{
    Biquad s, h;
    void prepare (double sr) { s.set (Bq::HighShelf, 1681.97, 0.7072, 4.0, sr); h.set (Bq::HighPass, 38.135, 0.5003, 0, sr); s.reset(); h.reset(); }
    inline double run (double x) { return h.run (s.run (x)); }
};

//============================================================================== what Listen heard (saved with the session)
struct Analysis { bool have = false; float mb[9] {}, mm[6] {}, mhb = 0.12f, mhba = 1e-6f, corr = 1, lcorr = 1, I = -70, tp = -70; };
struct Plan
{
    bool ready = false, monobass = false, cal = false; int style = 0, caln = 0;
    float gain = 0, trim = 0, ls = 0, lm = 0, pr = 0, hs = 0, mudf = 250, mudg = 0, width = 0, hbase = 0.12f, hbabs = 1e-6f;
};
struct Params { bool listen = false, fix = false, limiter = true; int sound = 0; float amount = 100, console = 0, harsh = 50, glue = 50; };

struct Readout
{
    std::atomic<float> lufsIn { -70 }, tpIn { -70 }, corr { 0 }, gain { 0 }, secs { 0 }, lufsOut { -70 }, peakOut { -70 },
        turnDown { 0 }, harsh { 0 }, glue { 0 }, lim { 0 }, ls { 0 }, lm { 0 }, pr { 0 }, hs { 0 }, mudf { 250 }, mudg { 0 }, width { 0 };
    std::atomic<int> state { 0 }, style { 0 }; std::atomic<bool> cal { false };
    std::array<std::atomic<float>, 32> spec {};
};

//============================================================================== engine
class Engine
{
public:
    Analysis an; Plan fx; Readout ro;
    std::atomic<bool> uiOpen { false };

    int latency() const { return LA - 1; }

    void prepare (double rate)
    {
        sr = rate;
        LA = std::max (16, (int) std::floor (rate * 0.0015));
        kin[0].prepare (sr); kin[1].prepare (sr); kout[0].prepare (sr); kout[1].prepare (sr);
        min_.sublen = mout.sublen = (int) std::floor (sr * 0.1);
        for (int c = 0; c < 2; ++c) { lp[c].set (Bq::LowPass, 120, 0.7071, 0, sr); lp[c].reset(); }
        for (int b = 0; b < 9; ++b) { band[b].set (Bq::BandPass0dB, centers[b], 1.41, 0, sr); band[b].reset(); }
        for (int b = 0; b < 6; ++b) { mudb[b].set (Bq::BandPass0dB, mudf[b], 4.3, 0, sr); mudb[b].reset(); }
        hbM.set (Bq::BandPass0dB, 4500, 0.8, 0, sr);
        for (int c = 0; c < 2; ++c)
        {
            auto& f = cf[c];
            f.rum.set (Bq::HighPass, 25, 0.7071, 0, sr);
            f.hb.set (Bq::BandPass0dB, 4500, 0.8, 0, sr);
            f.tdc.set (Bq::HighPass, 5, 0.7071, 0, sr); f.sdc.set (Bq::HighPass, 5, 0.7071, 0, sr);
        }
        sideHP.set (Bq::HighPass, 250, 0.7071, 0, sr); sideLP.set (Bq::LowPass, 120, 0.7071, 0, sr); scHP.set (Bq::HighPass, 100, 0.7071, 0, sr);
        for (int k = 0; k < 32; ++k) { const double f = 40.0 * std::pow (400.0, k / 31.0); specBp[k].set (Bq::BandPass0dB, f, 4.0, 0, sr); specE[k] = 0; }
        ha = coef (0.005); hr = coef (0.08); ga = coef (0.02); grl = coef (0.2); gsk = coef (0.2); rc = coef (0.08); specRel = coef (0.25);
        for (int p = 1; p <= 3; ++p)
        {
            const double mu = p / 4.0; double s = 0;
            for (int m = -7; m <= 8; ++m)
            {
                const double x = m - mu, sinc = std::abs (x) < 1e-9 ? 1 : std::sin (kPi * x) / (kPi * x), t = (x + 8) / 16;
                const double w = 0.42 - 0.5 * std::cos (2 * kPi * t) + 0.08 * std::cos (4 * kPi * t);
                tpc[p - 1][m + 7] = sinc * w; s += sinc * w;
            }
            for (auto& c : tpc[p - 1]) c /= s;
        }
        cap = LA + 4; dqv.assign ((size_t) cap, 1); dqi.assign ((size_t) cap, 0); box.assign ((size_t) LA, 1);
        for (auto* d : { &dDL, &dDR, &dWL, &dWR }) d->assign ((size_t) (LA - 1), 0.0f);
        dp = 0; resetMeas(); mout.reset(); limReset(); coeffDirty = true; lastVc = -1;
    }

    void process (float* L, float* R, int n, const Params& p)
    {
        const int style = clampv (p.sound, 0, 10), limI = p.limiter ? 1 : 0;
        // LANDR-style: press Master (fix) and play. It learns while it plays, masters after ~8 s,
        // refines every 4 s, and locks when playback stops (2 s of silence after 30 s heard).
        // "Listen" = learn this song again from the start.
        if ((p.fix && ! lastFixP && ! an.have) || (p.listen && ! lastListen)) { learning = true; resetMeas(); planAt = 0; silent = 0; }
        if (! p.fix && ! p.listen) learning = false;
        if (learning)
        {
            listen (L, R, n);
            double pk = 0; for (int i = 0; i < n; ++i) pk = std::max (pk, (double) std::max (std::abs (L[i]), std::abs (R[i])));
            silent = pk < 1e-3 ? silent + n : 0;
            const bool lock = (nsamp >= sr * 30 && silent >= sr * 2) || (! p.listen && lastListen && ! p.fix);
            if (nsamp >= sr * 8 && (nsamp - planAt >= sr * 4 || lock) && silent == 0) { lastAmount = p.amount; if (snapshot()) computeFix (style, p.amount, p.limiter); planAt = nsamp; }
            if (lock) { learning = false; if (snapshot()) computeFix (style, p.amount, p.limiter); }
        }
        else if (an.have && lastStyle >= 0 && (style != lastStyle || std::abs (p.amount - lastAmount) > 0.5f || limI != lastLim))
        { lastAmount = p.amount; computeFix (style, p.amount, p.limiter); }
        lastListen = p.listen; lastFixP = p.fix; lastStyle = style; lastAmount = p.amount; lastLim = limI;

        // manual Vintage Console 1973 (works with or without Fix)
        const float vc = p.console / 100.0f;
        if (vc > 0.001f)
        {
            if (std::abs (vc - lastVc) > 0.005f)
            {
                for (auto& f : cf) { f.tlo.set (Bq::LowShelf, 70, 0.7071, 0.8 * vc, sr); f.thi.set (Bq::HighShelf, 14000, 0.7071, -1.5 * vc, sr); }
                lastVc = vc;
            }
            for (int i = 0; i < n; ++i)
            {
                L[i] = (float) console (cf[0].tlo, cf[0].thi, cf[0].tdc, L[i], vc);
                R[i] = (float) console (cf[1].tlo, cf[1].thi, cf[1].tdc, R[i], vc);
            }
        }

        const int target = (p.fix && fx.ready) ? 1 : 0;
        const bool active = target == 1 || mixv > 0;
        if (target == 1 && lastFix == 0) { mout.reset(); opk = 0; limReset(); }
        lastFix = target;
        double hmax = 1, gmax = 1, lmax = 1;

        // per-block settings
        const auto& st = styles()[(size_t) clampv (fx.style, 0, 10)];
        const double a = lastAmount / 100.0;
        if (active && coeffDirty) updateCoeffs (st, a);
        const double hshare = fx.hbase, hlev = fx.hbabs * 4.0 * std::pow (10.0, fx.pr / 10.0);
        const double hkk = (p.harsh / 50.0) * st.harsh * a;
        const double ge = (p.glue / 100.0) * st.glue * 2.0 * a;
        const double gth = an.I + 3 - 6 * std::min (ge, 1.0), gratio = 2 + ge;
        const double gt = std::pow (10.0, (fx.gain + fx.trim) / 20.0) * std::pow (10.0, 1.5 * std::min (ge, 1.0) / 20.0);
        const double wg = std::pow (10.0, fx.width / 20.0) - 1.0, mb = fx.monobass ? 1.0 : 0.0;
        const bool dow = wg != 0 || fx.monobass;
        const double vs = st.vc * a;
        const double ceil = std::pow (10.0, -1.5 / 20.0), step = 1.0 / std::max (1.0, sr * 0.05);
        const bool dlim = active && p.limiter;
        const int W = LA, D = LA - 1;

        for (int i = 0; i < n; ++i)
        {
            const double xl = L[i], xr = R[i];
            double wl = xl, wr = xr;
            if (active)
            {
                double l = xl, r = xr;
                for (int c = 0; c < 2; ++c)
                {
                    auto& f = cf[c]; double& v = c == 0 ? l : r;
                    v = f.hs.run (f.pr.run (f.lm.run (f.ls.run (f.rum.run (v)))));
                    if (fx.mudg < 0) v = f.mud.run (v);
                }
                const double m = (l + r) * 0.5, s = (l - r) * 0.5;
                const double sh = sideHP.run (s), sl = sideLP.run (s), sc = scHP.run (m);
                const double bl = cf[0].hb.run (l), br = cf[1].hb.run (r);
                const double bm = (bl + br) * 0.5, b2 = bm * bm, m2 = m * m, s2 = sc * sc;
                hb += (b2 > hb ? ha : hr) * (b2 - hb);
                hx += (m2 > hx ? ha : hr) * (m2 - hx);
                ge2 += (s2 > ge2 ? ga : grl) * (s2 - ge2);
                if ((tick++ & 15) == 0)
                {
                    hgt = 1;
                    if (hkk > 0 && hx > 1e-12 && hb > hshare * hx)
                        hgt = std::pow (10.0, -clampv (dB10 (hb / hlev) * 0.6 * hkk, 0.0, 6.0) / 20.0);
                    gtg = 1;
                    if (ge > 0.001)
                    {
                        const double o = dB10 (ge2) - gth; double red = 0;
                        if (o > 3) red = o * (1 - 1 / gratio); else if (o > -3) red = (o + 3) * (o + 3) / 12 * (1 - 1 / gratio);
                        gtg = std::pow (10.0, -std::min (red, 6.0) / 20.0);
                    }
                }
                hg += 0.1 * (hgt - hg); gc += 0.05 * (gtg - gc); gs += gsk * (gt - gs);
                const double k = hg - 1;
                l += k * bl; r += k * br;
                if (dow) { const double mm = (l + r) * 0.5, ss = (l - r) * 0.5 + wg * sh - mb * sl; l = mm + ss; r = mm - ss; }
                const double g = gc * gs; l *= g; r *= g;
                if (vs > 0.001) { l = console (cf[0].slo, cf[0].shi, cf[0].sdc, l, vs); r = console (cf[1].slo, cf[1].shi, cf[1].sdc, r, vs); }
                hmax = std::min (hmax, hg); gmax = std::min (gmax, gc);
                wl = l; wr = r;
            }
            // look-ahead limiter (hold-min + box smoothing: never exceeds the ceiling)
            double b = 1;
            if (dlim)
            {
                const double av = std::max (std::abs (wl), std::abs (wr)), req = av > ceil ? ceil / av : 1.0;
                ++lt;
                while (dt > dh && dqv[(size_t) ((dt - 1) % cap)] >= req) --dt;
                dqv[(size_t) (dt % cap)] = req; dqi[(size_t) (dt % cap)] = lt; ++dt;
                if (dqi[(size_t) (dh % cap)] <= lt - W) ++dh;
                const double mn = dqv[(size_t) (dh % cap)];
                rel += (1 - rel) * rc; if (mn < rel) rel = mn;
                const size_t bi = (size_t) (lt % W);
                bsum += rel - box[bi]; box[bi] = rel;
                b = std::min (1.0, bsum / W); lmax = std::min (lmax, b);
                if ((lt & 8191) == 0) { double s = 0; for (auto v : box) s += v; bsum = s; }
            }
            const double odl = dDL[(size_t) dp], odr = dDR[(size_t) dp], owl = dWL[(size_t) dp], owr = dWR[(size_t) dp];
            dDL[(size_t) dp] = (float) xl; dDR[(size_t) dp] = (float) xr; dWL[(size_t) dp] = (float) wl; dWR[(size_t) dp] = (float) wr;
            if (++dp >= D) dp = 0;
            if (mixv < target) mixv = std::min ((double) target, mixv + step); else if (mixv > target) mixv = std::max ((double) target, mixv - step);
            L[i] = (float) (odl + mixv * (owl * b - odl));
            R[i] = (float) (odr + mixv * (owr * b - odr));
        }

        // measure the result + self-calibrate loudness (twice at most)
        if (target == 1)
        {
            for (int i = 0; i < n; ++i)
            {
                mout.feed (kout[0].run (L[i]), kout[1].run (R[i]));
                opk = std::max (opk, (double) std::max (std::abs (L[i]), std::abs (R[i])));
            }
            if (! learning && ! fx.cal && mout.blocks >= 120)
            {
                const double want = st.lufs + (1 - a) * (an.I - st.lufs);
                const double t = clampv (want - mout.integrated(), -3.0, 3.0);
                const double capg = (p.limiter ? (-1.5 - an.tp + 8) : std::max (0.0, -1.0 - an.tp - 0.3)) - fx.gain;
                if (std::abs (t) > 0.3) fx.trim = (float) clampv (std::min (fx.trim + t, capg), -6.0, 6.0);
                fx.caln++; fx.cal = fx.caln >= 2 || std::abs (t) <= 0.3;
                mout.reset(); opk = 0;
            }
        }
        if (uiOpen.load (std::memory_order_relaxed)) spectrum (L, R, n);

        // readouts
        const double I = learning ? min_.integrated() : an.I;
        const double tpd = learning ? dB20 (tp) : an.tp;
        const double cr = learning ? sLR / std::sqrt (std::max (sLL * sRR, 1e-30)) : an.corr;
        const double Io = (target == 1 && mout.blocks > 10) ? mout.integrated() : -70;
        ro.lufsIn = (float) I; ro.tpIn = (float) tpd; ro.corr = (float) cr;
        ro.gain = target == 1 ? fx.gain + fx.trim : 0.0f; ro.secs = (float) (nsamp / sr);
        ro.lufsOut = (float) Io; ro.peakOut = target == 1 ? (float) dB20 (std::max (opk, 1e-9)) : -70.0f;
        ro.turnDown = Io > -70 ? (float) std::max (0.0, Io + 14) : 0.0f;
        ro.harsh = (float) -dB20 (hmax); ro.glue = (float) -dB20 (gmax); ro.lim = (float) -dB20 (lmax);
        ro.ls = fx.ls; ro.lm = fx.lm; ro.pr = fx.pr; ro.hs = fx.hs; ro.mudf = fx.mudf; ro.mudg = fx.mudg; ro.width = fx.width;
        // 0 idle, 1 learning (not mastering yet), 2 mastered + locked, 3 ready (Master off), 4 mastering while still learning
        ro.state = learning ? (target == 1 ? 4 : 1) : (target == 1 ? 2 : (fx.ready ? 3 : 0));
        ro.style = target == 1 ? fx.style : style; ro.cal = fx.cal;
    }

    void restored() { lastStyle = -1; coeffDirty = true; }

private:
    struct Ch { Biquad rum, ls, lm, mud, pr, hs, hb, tlo, thi, tdc, slo, shi, sdc; };
    static constexpr double centers[9] = { 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    static constexpr double baseT[9] = { -6, -6, -9, -10.5, -11.5, -13.5, -16.5, -20, -28 };
    static constexpr double mudf[6] = { 160, 200, 250, 315, 400, 500 };
    double sr = 44100; int LA = 66;
    KWeight kin[2], kout[2]; Meter min_, mout; Biquad lp[2], band[9], mudb[6], hbM, sideHP, sideLP, scHP, specBp[32];
    Ch cf[2];
    double specE[32] {};
    double sLL = 0, sRR = 0, sLR = 0, lLL = 0, lRR = 0, lLR = 0, nsamp = 0, bandE[9] {}, mudE[6] {}, totE = 0, totN = 0, hbE = 0, tp = 0, opk = 0;
    double hist[2][16] {}; int hpos = 0; double tpc[3][16] {};
    double ha = 0, hr = 0, ga = 0, grl = 0, gsk = 0, rc = 0, specRel = 0;
    double hb = 0, hx = 0, hg = 1, hgt = 1, ge2 = 0, gc = 1, gtg = 1, gs = 1; unsigned tick = 0;
    std::vector<double> dqv, box; std::vector<long long> dqi; long long dh = 0, dt = 0, lt = 0; int cap = 70; double rel = 1, bsum = 0;
    std::vector<float> dDL, dDR, dWL, dWR; int dp = 0;
    bool learning = false, lastFixP = false; double planAt = 0, silent = 0;
    double mixv = 0; bool lastListen = false; int lastStyle = -1, lastLim = -1, lastFix = 0; float lastAmount = 100, lastVc = -1; bool coeffDirty = true;

    double coef (double secs) const { return 1.0 - std::exp (-1.0 / (secs * sr)); }
    static double console (Biquad& lo, Biquad& hi, Biquad& dc, double x, double vc)
    {
        x = hi.run (lo.run (x));
        x = x + 0.05 * vc * x * x - 0.05 * vc * x * x * x;   // transformer + class-A curve, 2nd harmonic
        return dc.run (x);
    }
    void resetMeas()
    {
        min_.reset(); sLL = sRR = sLR = lLL = lRR = lLR = nsamp = 0;
        for (auto& e : bandE) e = 0; for (auto& e : mudE) e = 0; totE = totN = hbE = tp = 0;
        for (auto& h : hist) for (auto& v : h) v = 0;
    }
    void limReset() { std::fill (dqv.begin(), dqv.end(), 1.0); std::fill (dqi.begin(), dqi.end(), 0); std::fill (box.begin(), box.end(), 1.0); dh = dt = lt = 0; rel = 1; bsum = LA; }

    void listen (const float* L, const float* R, int n)
    {
        double bE = 0, tE[9] {}, tM[6] {}, tH = 0;
        for (int i = 0; i < n; ++i)
        {
            const double l = L[i], r = R[i];
            sLL += l * l; sRR += r * r; sLR += l * r;
            min_.feed (kin[0].run (l), kin[1].run (r));
            const double al = lp[0].run (l), ar = lp[1].run (r);
            lLL += al * al; lRR += ar * ar; lLR += al * ar;
            // 4x true peak around big samples
            hist[0][hpos] = l; hist[1][hpos] = r; hpos = (hpos + 1) & 15;
            const double pk = std::max (std::abs (l), std::abs (r));
            if (pk > tp) tp = pk;
            if (pk > 0.6 * tp)
                for (int c = 0; c < 2; ++c)
                    for (int ph = 0; ph < 3; ++ph)
                    {
                        double y = 0;
                        for (int m = 0; m < 16; ++m) y += tpc[ph][m] * hist[c][(hpos + m) & 15];
                        tp = std::max (tp, std::abs (y));
                    }
            const double v = (l + r) * 0.5; bE += v * v;
            for (int k = 0; k < 9; ++k) { const double y = band[k].run (v); tE[k] += y * y; }
            for (int k = 0; k < 6; ++k) { const double y = mudb[k].run (v); tM[k] += y * y; }
            const double y = hbM.run (v); tH += y * y;
        }
        nsamp += n;
        if (bE / n > 1e-7) { totE += bE; totN += n; for (int k = 0; k < 9; ++k) bandE[k] += tE[k]; for (int k = 0; k < 6; ++k) mudE[k] += tM[k]; hbE += tH; }
    }

    bool snapshot()
    {
        if (nsamp < sr * 5) return false;
        for (int b = 0; b < 9; ++b) an.mb[b] = (float) dB10 (bandE[b] / std::max (totE, 1e-20));
        for (int b = 0; b < 6; ++b) an.mm[b] = (float) dB10 (mudE[b]);
        an.mhb = (float) (hbE / std::max (totE, 1e-20)); an.mhba = (float) (hbE / std::max (totN, 1.0));
        an.corr = (float) (sLR / std::sqrt (std::max (sLL * sRR, 1e-30)));
        an.lcorr = (float) (lLR / std::sqrt (std::max (lLL * lRR, 1e-30)));
        an.I = (float) min_.integrated(); an.tp = (float) dB20 (std::max (tp, 1e-9)); an.have = true;
        return true;
    }

    void computeFix (int style, float amount, bool limon)
    {
        if (! an.have) return;
        const auto& st = styles()[(size_t) style];
        const double a = amount / 100.0;
        double dev[9], mean = 0;
        for (int b = 0; b < 9; ++b) { dev[b] = (baseT[b] + st.off[b]) - an.mb[b]; mean += dev[b]; }
        mean /= 9; for (auto& d : dev) d -= mean;
        fx.ls = (float) (clampv (0.6 * (dev[0] + dev[1]) / 2, -3.0, 3.0) * a);
        fx.lm = (float) (clampv (0.6 * dev[2], -3.0, 2.0) * a);
        fx.pr = (float) (clampv (0.6 * (dev[5] + dev[6]) / 2, -2.5, 2.5) * a);
        fx.hs = (float) (clampv (0.6 * (dev[7] + dev[8]) / 2, -3.0, 3.0) * a);
        double best = 0; int bk = 0;
        for (int k = 1; k < 5; ++k) { const double ex = an.mm[k] - (an.mm[k - 1] + an.mm[k + 1]) / 2; if (ex > best) { best = ex; bk = k; } }
        if (best > 1.2) { fx.mudf = (float) mudf[bk]; fx.mudg = (float) (-clampv ((best - 0.7) * 0.8, 0.0, 3.0) * a); } else { fx.mudf = 250; fx.mudg = 0; }
        fx.hbase = std::max (an.mhb, 1e-4f); fx.hbabs = std::max (an.mhba, 1e-12f);
        double w = 0;
        if (an.corr > 0.92) w = clampv ((an.corr - 0.85) * 30.0, 0.0, 3.0); else if (an.corr < 0.35) w = -2;
        double sw = st.wid; if (an.corr < 0.5 && sw > 0) sw = 0;
        fx.width = (float) (clampv (w + sw, -2.0, 3.0) * a);
        fx.monobass = (an.lcorr < 0.9f || st.wid > 0) && a > 0;
        double g = clampv ((double) st.lufs - an.I, -12.0, 9.0);
        if (limon) g = std::min (g, -1.5 - an.tp + 6); else if (g > 0) g = std::min (g, std::max (0.0, -1.0 - an.tp - 0.3));
        fx.gain = (float) (g * a); fx.trim = 0; fx.cal = false; fx.caln = 0; fx.style = style; fx.ready = true;
        coeffDirty = true; mout.reset(); opk = 0;
    }

    void updateCoeffs (const Style& st, double a)
    {
        for (auto& f : cf)
        {
            f.ls.set (Bq::LowShelf, 110, 0.7071, fx.ls, sr); f.lm.set (Bq::Peaking, 260, 1.0, fx.lm, sr);
            f.mud.set (Bq::Peaking, fx.mudf, 4.3, fx.mudg, sr); f.pr.set (Bq::Peaking, 3500, 0.9, fx.pr, sr);
            f.hs.set (Bq::HighShelf, 9000, 0.7071, fx.hs, sr);
            f.slo.set (Bq::LowShelf, 70, 0.7071, 0.8 * st.vc * a, sr); f.shi.set (Bq::HighShelf, 14000, 0.7071, -1.5 * st.vc * a, sr);
        }
        coeffDirty = false;
    }

    void spectrum (const float* L, const float* R, int n)
    {
        for (int k = 0; k < 32; ++k)
        {
            double e = 0;
            for (int i = 0; i < n; ++i) { const double y = specBp[k].run ((L[i] + R[i]) * 0.5); e = std::max (e, y * y); }
            specE[k] = e > specE[k] ? e : specE[k] * std::pow (1 - specRel, n);
            ro.spec[(size_t) k] = (float) dB10 (specE[k] + 1e-12);
        }
    }
};
} // namespace sqn
