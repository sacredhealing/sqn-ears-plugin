// Zion Ark — dub studio engine (pure C++, no JUCE). Siddha Quantum Nexus Music.
// Signal flow (insert): IN -> Big Knob HPF -> Twin Phase -> [dry | tape echo send | spring send] -> sum -> Ark Tape -> match EQ -> mix -> OUT
// The spring convolution itself runs outside (processor) between processA() and processB().
#pragma once
#include <cmath>
#include <vector>
#include <atomic>
#include <algorithm>
#include <cstdint>

namespace za {

constexpr float PI = 3.14159265358979f;
inline float clampf (float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lin (float a, float b, float p) { return a + (b - a) * p; }
inline float ex (float a, float b, float p) { return a * std::pow (b / a, p); }
inline float dbToGain (float d) { return std::pow (10.0f, d / 20.0f); }
inline float sat (float x, float k) { return std::tanh (k * x) / k; }

//============================================================================== RBJ biquad, stereo state
struct Biquad
{
    enum Type { LP, HP, BP, Peak, LowShelf, HighShelf };
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] {}, z2[2] {};
    void set (Type t, float f, float q, float gainDb, float sr)
    {
        f = clampf (f, 10.0f, 0.45f * sr);
        const float w = 2 * PI * f / sr, cw = std::cos (w), sw = std::sin (w), al = sw / (2 * q), A = std::pow (10.0f, gainDb / 40.0f);
        float B0, B1, B2, A0, A1, A2;
        switch (t)
        {
            case LP: B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = B0; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
            case HP: B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = B0; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
            case BP: B0 = al; B1 = 0; B2 = -al; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
            case Peak: B0 = 1 + al * A; B1 = -2 * cw; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * cw; A2 = 1 - al / A; break;
            case LowShelf: { const float s = 2 * std::sqrt (A) * al;
                B0 = A * ((A + 1) - (A - 1) * cw + s); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - s);
                A0 = (A + 1) + (A - 1) * cw + s; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - s; break; }
            default: { const float s = 2 * std::sqrt (A) * al;
                B0 = A * ((A + 1) + (A - 1) * cw + s); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - s);
                A0 = (A + 1) - (A - 1) * cw + s; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - s; break; }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    inline float proc (float x, int c)
    {
        const float y = b0 * x + z1[c];
        z1[c] = b1 * x - a1 * y + z2[c];
        z2[c] = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
};

struct Smooth { float v = 0, k = 0.001f; void setTime (float sec, float sr) { k = 1.0f - std::exp (-1.0f / (std::max (1e-4f, sec) * sr)); } inline float next (float t) { v += (t - v) * k; return v; } };

struct Noise { uint32_t s = 22222; inline float next() { s = s * 1664525u + 1013904223u; return (float) ((int32_t) s) * (1.0f / 2147483648.0f); } };

//============================================================================== parameters (normalised 0..1 like the UI, mapped here)
struct Params
{
    float inG = .5f, mix = 1, outG = .5f, bk = 0;
    float phRA = .3f, phRB = .42f, phDepth = .75f, phFb = .5f, phMix = .8f; bool phOn = true, phSq = false;
    float eSend = .15f, time = .5f, eInt = .45f, eAge = .55f, eSpr = .2f, eRet = .7f; int mode = 0; bool sync = true;
    float sSend = .25f, sDrive = .45f, sDecay = .45f, sTone = .55f, sRet = .6f;
    float srPitch = .5f, srRate = .45f, srDepth = .5f, srWave = 2.0f / 3.0f;
    float tDrive = .35f, tWow = .2f, tMatch = .85f; int match = 1; bool mono = true;
    bool aux = false, byp = false, cut = false, thE = false, thS = false, warp = false, siren = false, kick = false;
    double hostBpm = 0;
};

static const float REFS[4][10] = {
    { 0.15f, 9.33f, 3.6f, -1.34f, 1.22f, 0.0f, -0.73f, -1.76f, -2.38f, -18.78f },   // Ark '77 (Party Time, 1977)
    { -17.84f, 9.29f, 0.78f, -8.47f, -3.18f, 0.0f, -5.84f, -12.12f, -7.19f, -24.08f }, // Rebel '70 (Soul Rebel)
    { -15.41f, 6.72f, 17.23f, 11.62f, 7.4f, 0.0f, 0.24f, -1.6f, -5.79f, -21.1f },     // One '72 (Studio One)
    { 7.03f, 13.65f, 6.92f, 2.94f, 1.21f, 0.0f, -1.32f, -7.05f, -8.87f, -15.05f } };  // Now (modern steppers)
static const float ERA[5] = { 20000, 13500, 12500, 10500, 16000 };
static const float CENT[10] = { 31.5f, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
static const float BK[11] = { 0, 70, 100, 150, 250, 500, 1000, 2000, 3000, 5000, 7500 };
// analyser calibration: makes the Party Time '77 record read as itself through this analyser (filter-bank vs FFT octave sums)
static const float ACAL[10] = { -1.33f, -3.33f, -4.67f, -2.0f, -0.33f, 3.0f, 0.67f, 0.67f, 2.33f, 14.33f };
static const float DIVS[7] = { .25f, 1.0f / 3.0f, .5f, .75f, 1, 1.5f, 2 };
static const int MODES[7][3] = { {1,0,0},{0,1,0},{0,0,1},{1,1,0},{0,1,1},{1,0,1},{1,1,1} };

//============================================================================== read-outs for the UI
struct ReadOut
{
    std::atomic<float> inDb { -90 }, outDb { -90 }, bpm { 0 };
    std::atomic<float> mg[10];
    ReadOut() { for (auto& m : mg) m = 0; }
};

//============================================================================== spring impulse response (same recipe as the preview)
inline void makeSpringIR (std::vector<float>& L, std::vector<float>& R, float decay, double sr)
{
    const int len = (int) (sr * (decay + 0.3)), cl = (int) (sr * 0.022);
    L.assign ((size_t) len, 0.0f); R.assign ((size_t) len, 0.0f);
    std::vector<float> chirp ((size_t) cl);
    double ph = 0;
    for (int i = 0; i < cl; ++i) { const double x = (double) i / cl, f = 4800.0 * std::pow (150.0 / 4800.0, x); ph += 2 * 3.14159265358979 * f / sr; chirp[(size_t) i] = (float) (std::sin (ph) * std::sin (3.14159265358979 * x) * (1 - x * 0.5)); }
    Noise n; n.s = 9876;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto& d = ch ? R : L;
        const double Ts[2] = { 0.0331 + ch * 0.0021, 0.0417 - ch * 0.0016 };
        for (int si = 0; si < 2; ++si)
        {
            int k = 0;
            for (double t = 0.006 + si * 0.003; t < decay + 0.25; t += Ts[si], ++k)
            {
                const float a = (float) (std::exp (-6.9 * t / decay) * ((k & 1) ? -1 : 1) * (0.85 + 0.15 * (n.next() + 1)));
                const int st = (int) (t * sr);
                for (int i = 0; i < cl && st + i < len; ++i) d[(size_t) (st + i)] += a * chirp[(size_t) i];
            }
        }
        for (int i = 0; i < len; ++i) { const double t = i / sr; d[(size_t) i] += (float) (n.next() * 0.12 * std::exp (-6.9 * t / decay) * std::min (1.0, t / 0.04)); }
        double e = 0; for (float v : d) e += (double) v * v;
        const float sc = (float) (0.9 / std::sqrt (e + 1e-12));
        for (float& v : d) v *= sc;
    }
}

//============================================================================== engine
class Engine
{
public:
    ReadOut ro;
    std::vector<float> springL, springR;   // spring send buffer (filled by processA, convolved outside, read by processB)

    void prepare (double sampleRate, int maxBlock)
    {
        sr = (float) sampleRate;
        echoLen = (int) (sr * 6.5f) + 8; echoBuf[0].assign ((size_t) echoLen, 0); echoBuf[1].assign ((size_t) echoLen, 0); ew = 0;
        wowLen = (int) (sr * 0.1f) + 8; wowBuf[0].assign ((size_t) wowLen, 0); wowBuf[1].assign ((size_t) wowLen, 0); ww = 0;
        springL.assign ((size_t) maxBlock, 0); springR.assign ((size_t) maxBlock, 0);
        dryS.assign ((size_t) maxBlock * 2, 0); retS.assign ((size_t) maxBlock * 2, 0); cleanS.assign ((size_t) maxBlock * 2, 0); sirS.assign ((size_t) maxBlock, 0);
        for (auto* s : { &sIn, &sOut, &sMix, &sClean, &sDry, &sPhOn, &sEret, &sEsend, &sSsend, &sThE, &sThS, &sMono, &sSirEnv, &sSret, &sHiss })
            s->setTime (0.02f, sr);
        sSirEnv.setTime (0.012f, sr);
        for (int i = 0; i < 3; ++i) { sHead[i].setTime (0.02f, sr); sTime[i].setTime (0.09f, sr); }
        for (auto& b : phA) b = {};
        for (auto& b : phB) b = {};
        lastBk = -1; lastTone = lastSt = lastEra = -1;
        bump.set (Biquad::Peak, 95, 0.9f, 2.0f, sr); hf.set (Biquad::LP, 15000, 0.6f, 0, sr);
        eHp.set (Biquad::HP, 140, 0.6f, 0, sr); sHp.set (Biquad::HP, 200, 0.7f, 0, sr); hissHp.set (Biquad::HP, 2500, 0.707f, 0, sr);
        sirLp.set (Biquad::LP, 3200, 0.7f, 0, sr);
        for (int i = 0; i < 10; ++i) { anBp[i].set (Biquad::BP, CENT[i], 2.0f, 0, sr); anBp2[i].set (Biquad::BP, CENT[i], 2.0f, 0, sr); anE[i] = 0; mGain[i] = 0; mGainS[i] = 0; mq[i].set (eqType (i), CENT[i], 1.2f, 0, sr); }
        era1.set (Biquad::LP, 20000, 0.6f, 0, sr); era2.set (Biquad::LP, 20000, 0.9f, 0, sr);
        haveAvg = false; anCount = 0; anSamples = (int) (sr * 0.35f);
        sTime[0].v = -1; // first-sample init flag
    }

    // Pass A: input, filter, phase, echo, siren. Fills spring send buffer.
    void processA (float* L, float* R, int n, const Params& p)
    {
        P = p;
        const float bpm = (float) (p.hostBpm > 20 ? p.hostBpm : 120.0);
        ro.bpm = bpm;
        // --- targets
        const float inG = dbToGain (lin (-12, 12, p.inG));
        const int bki = (int) std::lround (clampf (p.bk, 0, 1) * 10);
        if (bki != lastBk) { lastBk = bki; const float f = bki ? BK[bki] : 10.0f; bkA = std::exp (-2 * PI * f / sr); bkBq.set (Biquad::HP, f, 1.0f, 0, sr); }
        const float rA = ex (.03f, 8, p.phRA), rB = ex (.03f, 8, p.phRB), dep = p.phDepth, pfb = lin (0, .85f, p.phFb);
        float base = p.sync ? DIVS[(int) std::lround (clampf (p.time, 0, 1) * 6)] * 60.0f / bpm : ex (.05f, .8f, p.time);
        base = clampf (base, 0.02f, 2.0f);
        const float tScale = p.warp ? 2.3f : 1.0f;
        const float tc = p.warp ? 0.35f : (warpWas ? 0.25f : 0.09f); warpWas = p.warp ? true : (warpWas && std::fabs (sTime[0].v - base) > 0.002f);
        for (int i = 0; i < 3; ++i) sTime[i].setTime (tc, sr);
        const float eFb = lin (0, 1.15f, p.eInt), age = p.eAge;
        const float tone = 7000 * std::pow (1800.0f / 7000.0f, age);
        if (std::fabs (tone - lastTone) > 1) { lastTone = tone; eTone.set (Biquad::LP, tone, 0.5f, 0, sr); }
        const float eWow = 0.0002f + 0.0016f * age, eFlut = 0.00002f + 0.0001f * age;
        const int md = std::clamp (p.mode, 0, 6); int nh = 0; for (int i = 0; i < 3; ++i) nh += MODES[md][i];
        const float hg = 1.0f / std::sqrt ((float) std::max (1, nh));
        const float eSend = lin (0, 1.2f, p.eSend), eRet = lin (0, 1.3f, p.eRet), eSpr = p.eSpr;
        const float sSend = lin (0, 1.2f, p.sSend), sDrive = ex (.5f, 5, p.sDrive);
        const float dryT = (p.aux || p.cut) ? 0.0f : 1.0f;
        // siren
        const float sp = ex (120, 1800, p.srPitch), srr = ex (.2f, 14, p.srRate), sdep = p.srDepth; const int sw = (int) std::lround (clampf (p.srWave, 0, 1) * 3);
        // kick (rising edge)
        if (p.kick && ! kickWas) kickPos = 0;
        kickWas = p.kick;
        if (sTime[0].v < 0) for (int i = 0; i < 3; ++i) sTime[i].v = base * ratio[i];

        double inAcc = 0;
        for (int i = 0; i < n; ++i)
        {
            float x[2] = { L[i] * sIn.next (inG), R[i] * inG };
            x[1] = R[i] * sIn.v;
            inAcc += 0.5 * ((double) x[0] * x[0] + (double) x[1] * x[1]);
            cleanS[(size_t) (2 * i)] = x[0]; cleanS[(size_t) (2 * i + 1)] = x[1];
            // Big Knob: 3rd-order high-pass (1st-order + biquad Q=1) = 18 dB/oct, like the passive Altec
            if (bki)
                for (int c = 0; c < 2; ++c) { const float y = bkA * (bkY[c] + x[c] - bkX[c]); bkX[c] = x[c]; bkY[c] = y; x[c] = bkBq.proc (y, c); }
            // Twin Phase: two 6-stage first-order allpass chains in series, photocell-style exponential sweep
            const float on = sPhOn.next (p.phOn ? 1.0f : 0.0f);
            if (on > 0.0005f)
            {
                lfoA += rA / sr; if (lfoA >= 1) lfoA -= 1; lfoB += rB / sr; if (lfoB >= 1) lfoB -= 1;
                const float kSm = p.phSq ? (1 - std::exp (-2 * PI * 7 / sr)) : (1 - std::exp (-2 * PI * 30 / sr));
                const float ta = p.phSq ? (lfoA < .5f ? 1.0f : -1.0f) : std::sin (2 * PI * lfoA), tb = p.phSq ? (lfoB < .5f ? 1.0f : -1.0f) : std::sin (2 * PI * lfoB);
                smA += (ta - smA) * kSm; smB += (tb - smB) * kSm;
                const float fa = 400 * std::pow (4.0f, smA * dep), fb = 400 * std::pow (4.0f, smB * dep);
                const float tA = std::tan (PI * fa / sr), tB = std::tan (PI * fb / sr), ca = (tA - 1) / (tA + 1), cb = (tB - 1) / (tB + 1);
                for (int c = 0; c < 2; ++c)
                {
                    float v = x[c] + pfb * fbA[c];
                    for (int s = 0; s < 6; ++s) { auto& st = phA[s]; const float y = ca * v + st.x1[c] - ca * st.y1[c]; st.x1[c] = v; st.y1[c] = y; v = y; }
                    fbA[c] = v;
                    float w = v + pfb * fbB[c];
                    for (int s = 0; s < 6; ++s) { auto& st = phB[s]; const float y = cb * w + st.x1[c] - cb * st.y1[c]; st.x1[c] = w; st.y1[c] = y; w = y; }
                    fbB[c] = w;
                    const float wet = (x[c] + w * lin (0, 1, p.phMix)) * 0.62f;
                    x[c] = x[c] + (wet - x[c]) * on;
                }
            }
            // siren
            float sir = 0;
            const float env = sSirEnv.next (p.siren ? 0.18f : 0.0f);
            if (env > 1e-5f)
            {
                sLfo += srr / sr; if (sLfo >= 1) sLfo -= 1;
                const float l = sw == 0 ? std::sin (2 * PI * sLfo) : sw == 1 ? (sLfo < .5f ? 4 * sLfo - 1 : 3 - 4 * sLfo) : sw == 2 ? (sLfo < .5f ? 1.0f : -1.0f) : 2 * sLfo - 1;
                const float f = std::max (20.0f, sp + l * sp * 0.6f * sdep);
                sOsc += f / sr; if (sOsc >= 1) sOsc -= 1;
                sir = sirLp.proc (sOsc < .5f ? 1.0f : -1.0f, 0) * env;
            }
            sirS[(size_t) i] = sir;
            // Space Tape Echo: 3 heads on one tape loop, feedback through tone + saturation
            const float es = sEsend.next (eSend) + sThE.next (p.thE ? 1.1f : 0.0f);
            eWowPh += 0.7f / sr; if (eWowPh >= 1) eWowPh -= 1; eFlPh += 7.2f / sr; if (eFlPh >= 1) eFlPh -= 1;
            const float mod = eWow * std::sin (2 * PI * eWowPh) + eFlut * std::sin (2 * PI * eFlPh);
            float hsum[2] = { 0, 0 };
            for (int h = 0; h < 3; ++h)
            {
                const float d = sTime[h].next (base * tScale * ratio[h]);
                const float g = sHead[h].next (MODES[md][h] ? hg : 0.0f);
                if (g < 1e-5f) continue;
                float rp = (float) ew - (d + mod) * sr; while (rp < 0) rp += (float) echoLen;
                const int i0 = (int) rp; const float fr = rp - (float) i0; const int i1 = (i0 + 1) % echoLen;
                for (int c = 0; c < 2; ++c) hsum[c] += g * (echoBuf[c][(size_t) i0] + fr * (echoBuf[c][(size_t) i1] - echoBuf[c][(size_t) i0]));
            }
            const float er = sEret.next (eRet), ssend = sSsend.next (sSend), ths = sThS.next (p.thS ? 1.1f : 0.0f);
            for (int c = 0; c < 2; ++c)
            {
                const float e = eHp.proc (eTone.proc (hsum[c], c), c);
                echoBuf[c][(size_t) ew] = x[c] * es + sir * 0.7f + sat (e * eFb, 1.6f);
                retS[(size_t) (2 * i + c)] = e * er;
                dryS[(size_t) (2 * i + c)] = x[c] * sDry.next (dryT);
                float spIn = x[c] * (ssend + ths) + e * eSpr;
                if (kickPos >= 0 && c == 0)
                {
                    const float t = kickPos / sr;
                    kickV = (t < 0.06f ? noise.next() * 3 * std::exp (-t / 0.012f) : 0) + (t < 0.13f ? 2.5f * std::sin (kickPh) * std::exp (-t / 0.03f) : 0);
                    kickPh += 2 * PI * (30 + 60 * std::exp (-t / 0.04f)) / sr;
                    if (++kickPos > (int) (sr * 0.14f)) kickPos = -1;
                }
                spIn += kickPos >= 0 || c == 1 ? kickV : 0;
                spIn = sHp.proc (sat (spIn * sDrive, 1.2f), c);
                (c ? springR : springL)[(size_t) i] = spIn;
            }
            if (kickPos < 0) kickV = 0;
            ew = (ew + 1) % echoLen;
        }
        ro.inDb = 10.0f * std::log10 ((float) (inAcc / std::max (1, n)) + 1e-12f);
    }

    // Pass B: sum dry + returns + siren, Ark Tape, reference match, mix.
    void processB (float* L, float* R, int n)
    {
        const auto& p = P;
        const float sTone = ex (1500, 9000, p.sTone), sRet = lin (0, 1.5f, p.sRet);
        if (std::fabs (sTone - lastSt) > 1) { lastSt = sTone; sLp.set (Biquad::LP, sTone, 0.6f, 0, sr); }
        const float k = lin (.6f, 5, p.tDrive), wow = p.tWow * 0.0012f, flut = p.tWow * 0.00006f;
        const int mt = std::clamp (p.match, 0, 4); const bool mOn = mt > 0 && ! p.byp;
        const float eraF = mOn ? ERA[mt] : 20000.0f;
        if (std::fabs (eraF - lastEra) > 1) { lastEra = eraF; era1.set (Biquad::LP, eraF, 0.6f, 0, sr); era2.set (Biquad::LP, eraF, 0.9f, 0, sr); }
        const float mixT = p.byp ? 0.0f : p.mix, outG = dbToGain (lin (-12, 12, p.outG));
        double outAcc = 0;
        for (int i = 0; i < n; ++i)
        {
            float s[2];
            const float sr0 = sSret.next (sRet);
            for (int c = 0; c < 2; ++c)
                s[c] = dryS[(size_t) (2 * i + c)] + retS[(size_t) (2 * i + c)] + sLp.proc ((c ? springR : springL)[(size_t) i], c) * sr0 + sirS[(size_t) i] * 0.5f;
            // mono
            const float mo = sMono.next (p.mono ? 1.0f : 0.0f), m = 0.5f * (s[0] + s[1]);
            s[0] += (m - s[0]) * mo; s[1] += (m - s[1]) * mo;
            // wow & flutter (short modulated delay)
            wPh += 0.55f / sr; if (wPh >= 1) wPh -= 1; fPh += 6.3f / sr; if (fPh >= 1) fPh -= 1;
            const float d = 0.01f + wow * std::sin (2 * PI * wPh) + flut * std::sin (2 * PI * fPh);
            float rp = (float) ww - d * sr; while (rp < 0) rp += (float) wowLen;
            const int i0 = (int) rp; const float fr = rp - (float) i0; const int i1 = (i0 + 1) % wowLen;
            for (int c = 0; c < 2; ++c)
            {
                wowBuf[c][(size_t) ww] = s[c];
                float v = wowBuf[c][(size_t) i0] + fr * (wowBuf[c][(size_t) i1] - wowBuf[c][(size_t) i0]);
                v = sat (v * 0.6f, k) * 1.6f;                 // tape drive
                v = hf.proc (bump.proc (v, c), c);             // head bump + top
                if (c == 0) analyse (0.5f * (v + hfLast));     // match listens before its own EQ
                else hfLast = v;
                s[c] = v;
            }
            ww = (ww + 1) % wowLen;
            const float mx = sMix.next (mixT), cl = 1.0f - mx, og = sOut.next (outG);
            for (int c = 0; c < 2; ++c)
            {
                float v = s[c];
                for (int b = 0; b < 10; ++b) v = mq[b].proc (v, c);
                v = era2.proc (era1.proc (v, c), c);
                const float o = (v * mx + cleanS[(size_t) (2 * i + c)] * cl) * og;
                outAcc += 0.5 * (double) o * o;
                (c ? R : L)[i] = std::isfinite (o) ? o : 0.0f;
            }
            if ((++eqTick & 31) == 0) updateEq (mOn);
        }
        ro.outDb = 10.0f * std::log10 ((float) (outAcc / std::max (1, n)) + 1e-12f);
    }

private:
    struct AP { float x1[2] {}, y1[2] {}; };
    Params P;
    float sr = 44100;
    const float ratio[3] = { 1.0f, 1.95f, 2.85f };
    std::vector<float> echoBuf[2], wowBuf[2], dryS, retS, cleanS, sirS;
    int echoLen = 1, ew = 0, wowLen = 1, ww = 0;
    Smooth sIn, sOut, sMix, sClean, sDry, sPhOn, sEret, sEsend, sSsend, sThE, sThS, sMono, sSirEnv, sSret, sHiss, sHead[3], sTime[3];
    int lastBk = -1; float bkA = 0, bkX[2] {}, bkY[2] {}; Biquad bkBq;
    AP phA[6], phB[6]; float fbA[2] {}, fbB[2] {}, lfoA = 0, lfoB = 0.37f, smA = 0, smB = 0;
    Biquad eTone, eHp, sHp, sLp, bump, hf, hissHp, sirLp, era1, era2, mq[10], anBp[10], anBp2[10];
    float lastTone = -1, lastSt = -1, lastEra = -1, eWowPh = 0, eFlPh = 0, wPh = 0, fPh = 0, sLfo = 0, sOsc = 0, hfLast = 0;
    bool warpWas = false, kickWas = false; int kickPos = -1; float kickV = 0, kickPh = 0;
    Noise noise;
    // reference match
    float anE[10] {}, avg[10] {}, mGain[10] {}, mGainS[10] {}; bool haveAvg = false; int anCount = 0, anSamples = 1; unsigned eqTick = 0;

    static Biquad::Type eqType (int i) { return i == 0 ? Biquad::LowShelf : i == 9 ? Biquad::HighShelf : Biquad::Peak; }

    void analyse (float x)
    {
        for (int b = 0; b < 10; ++b) { const float y = anBp2[b].proc (anBp[b].proc (x, 1), 1); anE[b] += y * y; }
        if (++anCount < anSamples) return;
        anCount = 0;
        const auto& p = P;
        float band[10];
        for (int b = 0; b < 10; ++b) { band[b] = 10.0f * std::log10 (anE[b] + 1e-20f); anE[b] = 0; }
        if (p.match <= 0 || p.byp || band[5] < -95.0f + 10.0f * std::log10 ((float) anSamples)) return;
        float rel[10];
        for (int b = 0; b < 10; ++b) rel[b] = (band[b] + ACAL[b]) - (band[5] + ACAL[5]);
        for (int b = 0; b < 10; ++b) avg[b] = haveAvg ? avg[b] + 0.2f * (rel[b] - avg[b]) : rel[b];
        haveAvg = true;
        const float* ref = REFS[std::clamp (p.match, 1, 4) - 1];
        float g[10], mean = 0;
        for (int b = 0; b < 10; ++b) g[b] = ref[b] - avg[b];
        for (int b = 2; b < 9; ++b) mean += g[b];
        mean /= 7.0f;
        for (int b = 0; b < 10; ++b) mGain[b] = clampf ((g[b] - mean) * p.tMatch, -9, 9);
    }

    void updateEq (bool on)
    {
        for (int b = 0; b < 10; ++b)
        {
            const float t = on ? mGain[b] : 0.0f;
            const float nv = mGainS[b] + (t - mGainS[b]) * 0.012f;   // ~0.25 s glide at 32-sample steps
            if (std::fabs (nv - mGainS[b]) > 1e-4f || (std::fabs (nv) < 1e-4f && mGainS[b] != 0)) { mGainS[b] = std::fabs (nv) < 1e-4f && ! on ? 0.0f : nv; mq[b].set (eqType (b), CENT[b], 1.2f, mGainS[b], sr); }
            ro.mg[b] = mGainS[b];
        }
    }
};

} // namespace za
