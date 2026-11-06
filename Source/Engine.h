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
    double magDb (double f, double sr) const
    {
        const double w = 2.0 * kPi * f / sr, c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2 * w), s2 = std::sin (2 * w);
        const double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2), dr = 1 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
        return 10.0 * std::log10 ((nr * nr + ni * ni) / (dr * dr + di * di));
    }
};


//============================================================================== key + tuning + tempo (learned while it plays)
static const char* const kKeyNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
struct MusicSense
{
    double sr = 44100;
    Biquad ch[2][48]; double ceF[2][48] {}, C[2][12] {}, tot[2] {};   // 48 semitone filters C3..B6, for A=440 and A=432
    Biquad lowB, hiB; int fLen = 441, cLen = 4410, fN = 0, cN = 0; double eLo = 0, eHi = 0, pLo = 0, pHi = 0; std::vector<float> onset;
    void prepare (double rate)
    {
        sr = rate; fLen = (int) std::round (sr / 100.0); cLen = fLen * 10;
        for (int t = 0; t < 2; ++t)
            for (int k = 0; k < 48; ++k)
                ch[t][k].set (Bq::BandPass0dB, (t == 0 ? 440.0 : 432.0) * std::pow (2.0, (48 + k - 69) / 12.0), 25.0, 0, sr);
        lowB.set (Bq::LowPass, 150, 0.7071, 0, sr); hiB.set (Bq::HighPass, 2000, 0.7071, 0, sr);
        onset.reserve (9000); reset();
    }
    void reset()
    {
        for (int t = 0; t < 2; ++t) { tot[t] = 0; for (auto& v : C[t]) v = 0; for (auto& v : ceF[t]) v = 0; for (auto& b : ch[t]) b.reset(); }
        fN = cN = 0; eLo = eHi = pLo = pHi = 0; onset.clear(); lowB.reset(); hiB.reset();
    }
    inline void feed (double m)
    {
        for (int t = 0; t < 2; ++t) for (int k = 0; k < 48; ++k) { const double y = ch[t][k].run (m); ceF[t][k] += y * y; }
        const double lo = lowB.run (m), hi = hiB.run (m); eLo += lo * lo; eHi += hi * hi;
        if (++fN >= fLen)                    // 10 ms onset frame: kick band + hat/snare band
        {
            const double l1 = std::log10 (eLo / fN + 1e-10), h1 = std::log10 (eHi / fN + 1e-10);
            const double d = std::max (0.0, l1 - pLo) + std::max (0.0, h1 - pHi); pLo = l1; pHi = h1; eLo = eHi = 0; fN = 0;
            if (onset.size() >= 9000) onset.erase (onset.begin(), onset.begin() + 1000);
            onset.push_back ((float) d);
        }
        if (++cN >= cLen)                    // 100 ms chroma frame, normalised so loud hits don't dominate
        {
            for (int t = 0; t < 2; ++t)
            {
                double c[12] {}, mx = 1e-20, sum = 0;
                for (int k = 0; k < 48; ++k) { c[(48 + k) % 12] += std::sqrt (ceF[t][k]); sum += ceF[t][k]; ceF[t][k] = 0; }
                for (double v : c) mx = std::max (mx, v);
                if (sum > 1e-9) { for (int i = 0; i < 12; ++i) C[t][i] += c[i] / mx; tot[t] += sum; }
            }
            cN = 0;
        }
    }
    int key = -1; bool minor = false; double aRef = 440, bpm = 0, keyConf = 0;
    void analyse()
    {
        const int t = tot[1] > tot[0] * 1.12 ? 1 : 0; aRef = t ? 432 : 440;
        const double* c = C[t];
        static const double maj[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double mnr[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        auto corr = [&] (const double* prof, int root)
        {
            double mx = 0, my = 0; for (int i = 0; i < 12; ++i) { mx += c[i]; my += prof[i]; } mx /= 12; my /= 12;
            double sxy = 0, sxx = 0, syy = 0;
            for (int i = 0; i < 12; ++i) { const double x = c[(root + i) % 12] - mx, y = prof[i] - my; sxy += x * y; sxx += x * x; syy += y * y; }
            return sxy / std::sqrt (std::max (sxx * syy, 1e-20));
        };
        double best = -2, second = -2; key = 0; minor = false;
        for (int r = 0; r < 12; ++r) for (int m = 0; m < 2; ++m)
        {
            const double v = corr (m ? mnr : maj, r);
            if (v > best) { second = best; best = v; key = r; minor = m == 1; } else if (v > second) second = v;
        }
        keyConf = best - second;
        // tempo: autocorrelation of the onset curve with its double (harmonic check), prior around 70-140 BPM
        const int N = (int) onset.size(); bpm = 0;
        if (N > 800)
        {
            double mean = 0; for (auto v : onset) mean += v; mean /= N;
            std::vector<double> o ((size_t) N); for (int i = 0; i < N; ++i) o[(size_t) i] = onset[(size_t) i] - mean;
            static double r[241]; for (auto& v : r) v = 0;
            for (int L = 20; L <= 240; ++L) { double acc = 0; for (int i = 0; i + L < N; ++i) acc += o[(size_t) i] * o[(size_t) (i + L)]; r[L] = acc; }
            int bestL = 0; double bestV = -1e30;
            for (int L = 33; L <= 100; ++L)      // 60..180 BPM
            {
                const double b = 6000.0 / L, w = std::exp (-0.5 * std::pow (std::log2 (b / 95.0) / 0.55, 2));
                const double v = (r[L] + 0.5 * r[std::min (240, 2 * L)] + 0.25 * r[std::min (240, 4 * L)]) * w;
                if (v > bestV) { bestV = v; bestL = L; }
            }
            double Lf = bestL;
            { const double a = r[bestL - 1], b = r[bestL], cc = r[bestL + 1], den = a - 2 * b + cc; if (std::abs (den) > 1e-12) Lf += clampv (0.5 * (a - cc) / den, -0.5, 0.5); }
            bpm = 6000.0 / Lf;
        }
    }
};

//============================================================================== binaural generator (L and R carriers differ by the beat)
struct Binaural
{
    double phL = 0, phR = 0, amp = 0, ampT = 0, fc = 216, beat = 6, k = 0.0001;
    inline void tick (double sr, double& l, double& r)
    {
        amp += (ampT - amp) * k;
        if (amp < 1e-7 && ampT == 0) return;
        phL += 2 * kPi * (fc - beat * 0.5) / sr; phR += 2 * kPi * (fc + beat * 0.5) / sr;
        if (phL > 2 * kPi) phL -= 2 * kPi; if (phR > 2 * kPi) phR -= 2 * kPi;
        l += amp * std::sin (phL); r += amp * std::sin (phR);
    }
};
static const double kSacred[11] = { 108, 174, 285, 396, 417, 432, 528, 639, 741, 852, 963 };
static const double kStates[8] = { 0, 0.5, 2, 6, 7.83, 10, 14, 40 };   // Auto, Epsilon, Delta, Theta, Schumann, Alpha, Beta, Gamma

//============================================================================== the sounds
struct Style { const char* name; float lufs; float off[9]; float vc, glue, harsh, wid; float corrT = 0; float wT[9] {}; float wConf = 0; };  // wT: measured side-vs-mid per octave (dB), wConf 0 = not measured  // corrT: measured L/R similarity of the reference (0 = use wid)
inline const std::array<Style, 12>& styles()
{
    // Sounds marked MEASURED come from real reference records analysed with this engine (tone per octave, width, loudness).
    static const std::array<Style, 12> s { {
        { "Sovereign",       -11.0f, { 0, 0, 0, 0, 0, 0, 0, 0, 0 },                      0.00f, 0.5f, 1.0f,  0.0f },
        // MEASURED: Midnite (4 reference songs) - mono bass, open top, light glue (they recorded live, little compression)
        { "Zion Roots",      -12.5f, { 1.0f, -2.2f, -1.1f, 0.2f, 0.0f, -0.1f, -0.2f, 1.3f, 1.2f }, 0.30f, 0.3f, 1.0f, 0.0f, 0.97f, { -30.1f, -22.6f, -17.8f, -15.8f, -14.6f, -14.6f, -14.9f, -17.7f, -19.4f }, 0.75f },
        // MEASURED (1 song, blended 60/40 with design): early-70s Upsetter-era roots - mono, big low end
        { "Kingston 1973",   -13.0f, { 3.4f, 0.7f, -1.3f, -0.5f, 0.7f, -0.6f, -1.9f, 0.1f, -2.7f }, 0.70f, 0.6f, 1.3f, 0.0f, 0.97f, { -30.4f, -30.5f, -30.9f, -31.0f, -30.9f, -31.5f, -33.0f, -33.6f, -34.1f }, 0.5f },
        // MEASURED (1 song, blended 60/40 with design): late-70s Island-era roots - forward mids, open stereo
        { "Island '77",      -14.0f, { -1.2f, -2.0f, -0.5f, 0.5f, 1.4f, 1.4f, 0.4f, 0.7f, -1.1f }, 0.50f, 0.5f, 1.1f, 0.0f, 0.85f, { -20.9f, -16.0f, -12.1f, -9.6f, -8.9f, -5.9f, -4.5f, -5.4f, -6.1f }, 0.5f },
        { "Dub Chamber",     -12.0f, { 2.5f, 2, 0, -0.5f, -0.5f, -1, -1.5f, -2, -3 },     0.50f, 0.5f, 1.2f,  1.0f },
        // MEASURED: 1992-93 West Coast (4 reference songs)
        { "West Coast '92",  -10.2f, { 0.1f, -2.6f, -1.7f, -0.6f, -1.4f, -0.1f, 2.3f, 3.1f, 1.0f }, 0.25f, 0.8f, 0.8f, 0.0f, 0.87f, { -22.0f, -18.8f, -14.5f, -13.0f, -9.4f, -8.3f, -8.4f, -8.6f, -7.7f }, 0.75f },
        // MEASURED: 1996 G-funk (4 reference songs)
        { "G-Funk '96",      -10.4f, { 0.0f, -0.3f, -0.5f, -0.2f, -1.4f, 0.3f, 1.9f, 1.7f, -1.6f },  0.25f, 0.8f, 1.0f, 0.0f, 0.97f, { -26.3f, -24.3f, -17.6f, -13.7f, -13.6f, -14.9f, -16.1f, -14.6f, -14.4f }, 0.75f },
        // MEASURED: 1999 West Coast (2 reference songs)
        { "West Coast '99",   -9.0f, { 0.5f, -1.0f, 0.0f, 1.0f, 0.5f, -0.3f, 0.1f, 1.0f, -1.8f },   0.10f, 0.7f, 1.0f, 0.0f, 0.93f, { -23.4f, -18.5f, -15.4f, -14.8f, -14.4f, -13.8f, -11.9f, -8.6f, -6.0f }, 0.75f },
        // MEASURED: 1993-95 New York (4 reference songs)
        { "Golden Era '94",  -12.0f, { 0.2f, 0.2f, -0.5f, -0.6f, -1.3f, -0.3f, 2.1f, 1.2f, -1.0f }, 0.30f, 0.7f, 0.9f, 0.0f, 0.92f, { -17.4f, -18.2f, -18.2f, -17.8f, -17.9f, -18.1f, -16.6f, -13.9f, -11.4f }, 0.75f },
        { "808 Modern",       -9.0f, { 2.5f, 1.5f, -1, -0.5f, 0, 0, 0.5f, 1.5f, 2 },      0.00f, 0.6f, 1.0f,  1.0f },
        // MEASURED: early-80s pop (1 reference song) - clean, bright top, wide, very dynamic
        { "Crystal Pop '82", -13.5f, { -1.3f, -2.3f, -2.4f, -1.1f, -0.1f, 0.3f, 1.4f, 2.7f, 2.8f }, 0.10f, 0.4f, 1.0f, 0.0f, 0.82f, { -17.6f, -13.7f, -8.2f, -6.8f, -6.6f, -6.6f, -8.2f, -9.3f, -9.3f }, 0.5f },
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
struct Analysis { bool have = false; float mb[9] {}, mm[6] {}, mhb = 0.12f, mhba = 1e-6f, corr = 1, lcorr = 1, I = -70, tp = -70; int key = -1; bool minor = false; float aRef = 440, bpm = 0; float mw[9] {}; bool haveW = false; };
struct Plan
{
    bool ready = false, monobass = false, cal = false; int style = 0, caln = 0;
    float eq[9] {}, weq[9] {}; bool useW = false; float gain = 0, trim = 0, ls = 0, lm = 0, pr = 0, hs = 0, mudf = 250, mudg = 0, width = 0, hbase = 0.12f, hbabs = 1e-6f;
};
constexpr double kLearnSecs = 30.0;   // seconds of music SQN Ears listens to before it locks
struct Params { bool listen = false, fix = false, limiter = true, binaural = false; int sound = 0, binState = 0, binCarrier = 0; float amount = 100, console = 0, harsh = 50, glue = 50, binVol = -22, loudOff = 0; double hostBpm = 0; };

struct Readout
{
    std::atomic<float> lufsIn { -70 }, tpIn { -70 }, corr { 0 }, gain { 0 }, secs { 0 }, lufsOut { -70 }, peakOut { -70 },
        turnDown { 0 }, harsh { 0 }, glue { 0 }, lim { 0 }, ls { 0 }, lm { 0 }, pr { 0 }, hs { 0 }, mudf { 250 }, mudg { 0 }, width { 0 };
    std::atomic<int> state { 0 }, style { 0 }, key { -1 }, bpmSrc { 0 }, carrierIdx { -1 }; std::atomic<bool> cal { false }, minor { false }, binOn { false };
    std::atomic<float> target { -13 }, bpm { 0 }, aRef { 440 }, carrierHz { 0 }, carrierPlayed { 0 }, beatHz { 0 };
    std::array<std::atomic<float>, 32> spec {}; std::array<std::atomic<float>, 9> eq {};
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
        for (int b = 0; b < 9; ++b) { band[b].set (Bq::BandPass0dB, centers[b], 1.41, 0, sr); band[b].reset(); sband[b].set (Bq::BandPass0dB, centers[b], 1.41, 0, sr); sband[b].reset(); }
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
        for (int j = 0; j < 9; ++j)
        {
            Biquad t; t.set (Bq::Peaking, centers[j], 1.41, 1.0, sr);
            for (int i = 0; i < 9; ++i) octResp[i][j] = t.magDb (std::min (centers[i], sr * 0.45), sr);
        }
        sense.prepare (sr); bin.k = coef (1.0);
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
        const int style = clampv (p.sound, 0, (int) styles().size() - 1), limI = p.limiter ? 1 : 0;
        loudOff = clampv (p.loudOff, -4.0f, 4.0f);
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
            const bool lock = totN >= sr * kLearnSecs || (! p.listen && lastListen && ! p.fix);   // 30 s of music, then it stops by itself
            if (totN >= sr * 8 && (totN - planAt >= sr * 4 || lock) && silent == 0) { lastAmount = p.amount; if (snapshot()) computeFix (style, p.amount, p.limiter); planAt = totN; }
            if (lock && totN >= sr * 8) { learning = false; if (snapshot()) computeFix (style, p.amount, p.limiter); }
        }
        else if (an.have && lastStyle >= 0 && (style != lastStyle || std::abs (p.amount - lastAmount) > 0.5f || limI != lastLim || std::abs (p.loudOff - lastOff) > 0.01f))
        { lastAmount = p.amount; computeFix (style, p.amount, p.limiter); }
        lastListen = p.listen; lastFixP = p.fix; lastOff = p.loudOff; lastStyle = style; lastAmount = p.amount; lastLim = limI;

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
        const auto& st = styles()[(size_t) clampv (fx.style, 0, (int) styles().size() - 1)];
        const double a = lastAmount / 100.0;
        if (active && coeffDirty) updateCoeffs (st, a);
        const double hshare = fx.hbase, hlev = fx.hbabs * 4.0 * std::pow (10.0, fx.pr / 10.0);
        const double hkk = (p.harsh / 50.0) * st.harsh * a;
        const double ge = (p.glue / 100.0) * st.glue * 2.0 * a;
        const double gth = an.I + 3 - 6 * std::min (ge, 1.0), gratio = 2 + ge;
        const double gt = std::pow (10.0, (fx.gain + fx.trim) / 20.0) * std::pow (10.0, 1.5 * std::min (ge, 1.0) / 20.0);
        const double wg = std::pow (10.0, fx.width / 20.0) - 1.0, mb = fx.monobass ? 1.0 : 0.0;
        const bool useW = fx.useW; const bool dow = useW || wg != 0 || fx.monobass;
        const double vs = st.vc * a;
        const double ceil = std::pow (10.0, -1.5 / 20.0), step = 1.0 / std::max (1.0, sr * 0.05);
        const bool dlim = active && p.limiter;
        const int W = LA, D = LA - 1;
        setupBinaural (p, style);

        for (int i = 0; i < n; ++i)
        {
            double tl = 0, tr = 0; bin.tick (sr, tl, tr);              // binaural tone, added after the mastering stages
            const double xl = L[i] + tl, xr = R[i] + tr;
            double wl = xl, wr = xr;
            if (active)
            {
                double l = L[i], r = R[i];
                for (int c = 0; c < 2; ++c)
                {
                    auto& f = cf[c]; double& v = c == 0 ? l : r;
                    v = f.rum.run (v); for (auto& o : f.oct) v = o.run (v);
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
                if (dow)
                {
                    const double mm = (l + r) * 0.5; double ss = (l - r) * 0.5;
                    if (useW) { for (auto& f : sideEq) ss = f.run (ss); } else ss += wg * sh - mb * sl;
                    l = mm + ss; r = mm - ss;
                }
                const double g = gc * gs; l *= g; r *= g;
                if (vs > 0.001) { l = console (cf[0].slo, cf[0].shi, cf[0].sdc, l, vs); r = console (cf[1].slo, cf[1].shi, cf[1].sdc, r, vs); }
                hmax = std::min (hmax, hg); gmax = std::min (gmax, gc);
                wl = l + tl; wr = r + tr;
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
                const double tg = st.lufs + loudOff, want = tg + (1 - a) * (an.I - tg);
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
        ro.gain = target == 1 ? fx.gain + fx.trim : 0.0f; ro.secs = (float) (totN / sr);
        ro.lufsOut = (float) Io; ro.peakOut = target == 1 ? (float) dB20 (std::max (opk, 1e-9)) : -70.0f;
        ro.turnDown = Io > -70 ? (float) std::max (0.0, Io + 14) : 0.0f;
        ro.harsh = (float) -dB20 (hmax); ro.glue = (float) -dB20 (gmax); ro.lim = (float) -dB20 (lmax);
        ro.target = styles()[(size_t) clampv (target == 1 ? fx.style : style, 0, (int) styles().size() - 1)].lufs + loudOff;
        for (int b = 0; b < 9; ++b) ro.eq[(size_t) b] = fx.eq[b]; ro.ls = fx.ls; ro.lm = fx.lm; ro.pr = fx.pr; ro.hs = fx.hs; ro.mudf = fx.mudf; ro.mudg = fx.mudg; ro.width = fx.width;
        // 0 idle, 1 learning (not mastering yet), 2 mastered + locked, 3 ready (Master off), 4 mastering while still learning
        ro.state = learning ? (target == 1 ? 4 : 1) : (target == 1 ? 2 : (fx.ready ? 3 : 0));
        ro.style = target == 1 ? fx.style : style; ro.cal = fx.cal;
    }

    void restored() { lastStyle = -1; coeffDirty = true; }

private:
    struct Ch { Biquad oct[9]; Biquad rum, ls, lm, mud, pr, hs, hb, tlo, thi, tdc, slo, shi, sdc; };
    static constexpr double centers[9] = { 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    static constexpr double baseT[9] = { -6, -6, -9, -10.5, -11.5, -13.5, -16.5, -20, -28 };
    static constexpr double mudf[6] = { 160, 200, 250, 315, 400, 500 };
    double sr = 44100; int LA = 66;
    KWeight kin[2], kout[2]; Meter min_, mout; Biquad lp[2], band[9], sband[9], sideEq[9], mudb[6], hbM, sideHP, sideLP, scHP, specBp[32];
    Ch cf[2];
    double specE[32] {}; double octResp[9][9] {};
    double sLL = 0, sRR = 0, sLR = 0, lLL = 0, lRR = 0, lLR = 0, nsamp = 0, bandE[9] {}, sideE[9] {}, mudE[6] {}, totE = 0, totN = 0, hbE = 0, tp = 0, opk = 0;
    double hist[2][16] {}; int hpos = 0; double tpc[3][16] {};
    double ha = 0, hr = 0, ga = 0, grl = 0, gsk = 0, rc = 0, specRel = 0;
    double hb = 0, hx = 0, hg = 1, hgt = 1, ge2 = 0, gc = 1, gtg = 1, gs = 1; unsigned tick = 0;
    std::vector<double> dqv, box; std::vector<long long> dqi; long long dh = 0, dt = 0, lt = 0; int cap = 70; double rel = 1, bsum = 0;
    std::vector<float> dDL, dDR, dWL, dWR; int dp = 0;
    MusicSense sense; Binaural bin;
    float loudOff = 0, lastOff = 0;
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
        for (auto& e : bandE) e = 0; for (auto& e : sideE) e = 0; for (auto& e : mudE) e = 0; totE = totN = hbE = tp = 0;
        for (auto& h : hist) for (auto& v : h) v = 0;
        sense.reset();
    }
    void limReset() { std::fill (dqv.begin(), dqv.end(), 1.0); std::fill (dqi.begin(), dqi.end(), 0); std::fill (box.begin(), box.end(), 1.0); dh = dt = lt = 0; rel = 1; bsum = LA; }

    void listen (const float* L, const float* R, int n)
    {
        double bE = 0, tE[9] {}, tS[9] {}, tM[6] {}, tH = 0;
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
            const double v = (l + r) * 0.5; bE += v * v; sense.feed (v);
            const double sv = (l - r) * 0.5;
            for (int k = 0; k < 9; ++k) { const double y = band[k].run (v); tE[k] += y * y; const double z = sband[k].run (sv); tS[k] += z * z; }
            for (int k = 0; k < 6; ++k) { const double y = mudb[k].run (v); tM[k] += y * y; }
            const double y = hbM.run (v); tH += y * y;
        }
        nsamp += n;
        if (bE / n > 1e-7) { totE += bE; totN += n; for (int k = 0; k < 9; ++k) { bandE[k] += tE[k]; sideE[k] += tS[k]; } for (int k = 0; k < 6; ++k) mudE[k] += tM[k]; hbE += tH; }
    }

    bool snapshot()
    {
        if (totN < sr * 5) return false;   // never plan from silence
        for (int b = 0; b < 9; ++b) an.mb[b] = (float) dB10 (bandE[b] / std::max (totE, 1e-20));
        for (int b = 0; b < 6; ++b) an.mm[b] = (float) dB10 (mudE[b]);
        for (int b = 0; b < 9; ++b) an.mw[b] = (float) std::max (-40.0, dB10 (sideE[b] / std::max (bandE[b], 1e-20)));
        an.haveW = true;
        an.mhb = (float) (hbE / std::max (totE, 1e-20)); an.mhba = (float) (hbE / std::max (totN, 1.0));
        an.corr = (float) (sLR / std::sqrt (std::max (sLL * sRR, 1e-30)));
        an.lcorr = (float) (lLR / std::sqrt (std::max (lLL * lRR, 1e-30)));
        an.I = (float) min_.integrated(); an.tp = (float) dB20 (std::max (tp, 1e-9)); an.have = true;
        sense.analyse(); an.key = sense.key; an.minor = sense.minor; an.aRef = (float) sense.aRef; an.bpm = (float) sense.bpm;
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
        // match EQ: 9 octave bells solved together so overlapping bands land on the target curve
        double t9[9], g9[9];
        for (int b = 0; b < 9; ++b) { t9[b] = clampv (0.85 * dev[b], -5.0, 5.0) * a; g9[b] = t9[b]; }
        for (int it = 0; it < 8; ++it)
        {
            double got[9];
            for (int i = 0; i < 9; ++i) { got[i] = 0; for (int j = 0; j < 9; ++j) got[i] += octResp[i][j] * g9[j]; }
            for (int b = 0; b < 9; ++b) g9[b] = clampv (g9[b] + 0.8 * (t9[b] - got[b]), -6.0, 6.0);
        }
        for (int b = 0; b < 9; ++b) fx.eq[b] = (float) g9[b];
        fx.ls = (float) ((g9[0] + g9[1]) / 2); fx.lm = (float) g9[2]; fx.pr = (float) ((g9[5] + g9[6]) / 2); fx.hs = (float) ((g9[7] + g9[8]) / 2);
        double best = 0; int bk = 0;
        for (int k = 1; k < 5; ++k) { const double ex = an.mm[k] - (an.mm[k - 1] + an.mm[k + 1]) / 2; if (ex > best) { best = ex; bk = k; } }
        if (best > 1.2) { fx.mudf = (float) mudf[bk]; fx.mudg = (float) (-clampv ((best - 0.7) * 0.8, 0.0, 3.0) * a); } else { fx.mudf = 250; fx.mudg = 0; }
        fx.hbase = std::max (an.mhb, 1e-4f); fx.hbabs = std::max (an.mhba, 1e-12f);
        double w = 0;
        if (an.corr > 0.92) w = clampv ((an.corr - 0.85) * 30.0, 0.0, 3.0); else if (an.corr < 0.35) w = -2;
        double sw = st.wid; if (an.corr < 0.5 && sw > 0) sw = 0;
        fx.width = (float) (clampv (w + sw, -2.0, 3.0) * a);
        if (st.corrT > 0)   // measured reference: move the side/mid balance to the record's own width
        {
            const double c = clampv ((double) an.corr, -0.95, 0.995), ct = st.corrT;
            fx.width = (float) (clampv (10.0 * std::log10 (((1 - ct) / (1 + ct)) / ((1 - c) / (1 + c))), -3.0, 3.0) * a);
        }
        fx.monobass = (an.lcorr < 0.9f || st.wid > 0) && a > 0;
        // width per octave: move the song's side-vs-mid balance toward the records, band by band
        fx.useW = st.wConf > 0 && an.haveW;
        if (fx.useW)
        {
            double t9[9], g9[9];
            for (int b = 0; b < 9; ++b)
            {
                const double d = st.wConf * (st.wT[b] - an.mw[b]);
                t9[b] = (b < 2 ? clampv (d, -9.0, 0.0) : clampv (d, -6.0, 4.0)) * a;   // bass may only get narrower
                g9[b] = t9[b];
            }
            for (int it = 0; it < 8; ++it)
            {
                double got[9];
                for (int i = 0; i < 9; ++i) { got[i] = 0; for (int j = 0; j < 9; ++j) got[i] += octResp[i][j] * g9[j]; }
                for (int b = 0; b < 9; ++b) g9[b] = clampv (g9[b] + 0.8 * (t9[b] - got[b]), -10.0, 5.0);
            }
            double avg = 0; for (int b = 0; b < 9; ++b) { fx.weq[b] = (float) g9[b]; if (b >= 3) avg += g9[b] / 6; }
            fx.width = (float) avg; fx.monobass = false;
        }
        double g = clampv ((double) st.lufs + loudOff - an.I, -12.0, 9.0);
        if (limon) g = std::min (g, -1.5 - an.tp + 6); else if (g > 0) g = std::min (g, std::max (0.0, -1.0 - an.tp - 0.3));
        fx.gain = (float) (g * a); fx.trim = 0; fx.cal = false; fx.caln = 0; fx.style = style; fx.ready = true;
        coeffDirty = true; mout.reset(); opk = 0;
    }

    // the song's tempo: Mixbus tempo when it agrees with what we hear, otherwise what we hear
    double songBpm (double host, int& src) const
    {
        src = 0;
        const double heard = an.bpm;
        if (host > 0 && std::abs (host - 120.0) > 0.01) { src = 1; return host; }   // the session tempo was set: trust it
        if (host > 0 && heard > 0)
            for (double m : { 0.5, 1.0, 2.0 })
                if (std::abs (host * m / heard - 1.0) < 0.03) { src = 1; return host; }
        if (heard > 0) { src = 2; return heard; }
        if (host > 0) { src = 1; return host; }
        return 0;
    }
    void setupBinaural (const Params& p, int style)
    {
        int src = 0; const double bpm = songBpm (p.hostBpm, src);
        ro.bpm = (float) bpm; ro.bpmSrc = src; ro.key = an.key; ro.minor = an.minor; ro.aRef = an.aRef;
        // beat: chosen state, or Auto = the sound's vibe, locked to the groove (BPM/60 x 2^n) when close
        double beat = kStates[clampv (p.binState, 0, 7)];
        if (p.binState == 0)
        {
            const double tgt = style == 11 ? 6.0 : (style >= 1 && style <= 4 ? 7.83 : 10.0);
            beat = tgt;
            if (bpm > 0 && style != 1 && style != 2 && style != 3 && style != 4)
            {
                double bestB = 0, bestD = 1e9;
                for (int n = -2; n <= 6; ++n) { const double b = bpm / 60.0 * std::pow (2.0, n), d = std::abs (std::log2 (b / tgt)); if (d < bestD) { bestD = d; bestB = b; } }
                if (bestD < 0.27) beat = bestB;
            }
        }
        // carrier: chosen frequency, or Auto = the sacred frequency that sits in tune with the song's key
        double fc = 216, play = 216; int idx = -1;
        if (p.binCarrier > 0) { idx = clampv (p.binCarrier - 1, 0, 10); fc = play = kSacred[idx]; }
        else if (an.key >= 0)
        {
            static const int majS[7] = { 0, 2, 4, 5, 7, 9, 11 }, minS[7] = { 0, 2, 3, 5, 7, 8, 10 };
            const int* sc = an.minor ? minS : majS; double bestScore = 1e9;
            for (int s = 0; s < 11; ++s)
                for (int o = 0; o > -4; --o)
                {
                    const double f = kSacred[s] * std::pow (2.0, o);
                    if (f < 90 || f > 500) continue;
                    const double midi = 69 + 12 * std::log2 (f / an.aRef), cents = (midi - std::round (midi)) * 100;
                    const int pc = (((int) std::round (midi) % 12) + 12) % 12, deg = ((pc - an.key) % 12 + 12) % 12;
                    double w = 0; for (int d = 0; d < 7; ++d) if (sc[d] == deg) w = (d == 0 || d == 2 || d == 4) ? 1.0 : 0.6;
                    if (w == 0 || std::abs (cents) > 20) continue;
                    const double score = std::abs (cents) / w + std::abs (std::log2 (f / 220.0)) * 4;
                    if (score < bestScore) { bestScore = score; idx = s; fc = kSacred[s]; play = f; }
                }
            if (idx < 0) { fc = 0; play = an.aRef * std::pow (2.0, (an.key - 9) / 12.0) / 2; while (play < 110) play *= 2; while (play > 220) play /= 2; }
        }
        bin.fc = play; bin.beat = beat;
        // level: under the mix by binVol dB (sine at the song's loudness)
        const double ref = fx.ready ? styles()[(size_t) clampv (fx.style, 0, (int) styles().size() - 1)].lufs + loudOff : (an.I > -60 ? an.I : -14.0);
        bin.ampT = p.binaural ? std::pow (10.0, (ref + p.binVol + 0.7) / 20.0) : 0.0;
        ro.binOn = p.binaural; ro.carrierHz = (float) fc; ro.carrierPlayed = (float) play; ro.beatHz = (float) beat; ro.carrierIdx = idx;
    }

    void updateCoeffs (const Style& st, double a)
    {
        for (auto& f : cf)
        {
            for (int b = 0; b < 9; ++b) f.oct[b].set (Bq::Peaking, centers[b], 1.41, fx.eq[b], sr);
            for (int b = 0; b < 9; ++b) sideEq[b].set (Bq::Peaking, centers[b], 1.41, fx.weq[b], sr);
            f.mud.set (Bq::Peaking, fx.mudf, 4.3, fx.mudg, sr);
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
