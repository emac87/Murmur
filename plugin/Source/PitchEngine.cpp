#include "PitchEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace murmur
{

namespace
{
    // minHz maxHz targetRate lowpass hopMs threshold | startMs releaseMs changeMs changeSemis onsetDb | medianLen
    const ModeConfig kVoice  { 65.0f, 1100.0f, 22050.0f, 3000.0f, 8.0f, 0.25f,  32.0f, 70.0f, 70.0f, 0.70f, 0.0f, 5 };
    const ModeConfig kGuitar { 70.0f, 1400.0f, 22050.0f, 4000.0f, 5.0f, 0.18f,  16.0f, 45.0f, 30.0f, 0.55f, 7.0f, 5 };
    const ModeConfig kBass   { 29.0f,  450.0f, 11025.0f, 1500.0f, 6.0f, 0.18f,  24.0f, 55.0f, 40.0f, 0.55f, 7.0f, 5 };

    inline float hzToMidi (float f) { return 69.0f + 12.0f * std::log2 (f / 440.0f); }
    template <typename T> inline T clampv (T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

    float medianOf (const float* v, int n)
    {
        float s[16];
        for (int i = 0; i < n; ++i) s[i] = v[i];
        std::sort (s, s + n);
        return (n & 1) ? s[n / 2] : 0.5f * (s[n / 2 - 1] + s[n / 2]);
    }

    inline void pushHist (float* h, int& n, int cap, float v)
    {
        if (n < cap) { h[n++] = v; return; }
        for (int i = 1; i < cap; ++i) h[i - 1] = h[i];
        h[cap - 1] = v;
    }

    inline float minOf (const float* v, int n)
    {
        float m = v[0];
        for (int i = 1; i < n; ++i) m = std::min (m, v[i]);
        return m;
    }
}

const ModeConfig& configFor (Mode m)
{
    switch (m)
    {
        case Mode::Voice: return kVoice;
        case Mode::Bass:  return kBass;
        default:          return kGuitar;
    }
}

void PitchEngine::Biquad::design (double fs, double fc, double q)
{
    const double w = 2.0 * 3.14159265358979323846 * fc / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1.0 + al;
    b0 = (float) ((1.0 - c) / 2.0 / a0); b1 = (float) ((1.0 - c) / a0); b2 = b0;
    a1 = (float) (-2.0 * c / a0);        a2 = (float) ((1.0 - al) / a0);
    clear();
}

void PitchEngine::prepare (double sampleRate)
{
    inRate = sampleRate > 0 ? sampleRate : 48000.0;

    // size buffers for the worst case across all modes so setMode never allocates
    int worstN = 0, worstHop = 0;
    for (Mode m : { Mode::Voice, Mode::Guitar, Mode::Bass })
    {
        const auto& c = configFor (m);
        const int dm = std::max (1, (int) std::lround (inRate / c.targetRate));
        const double s = inRate / dm;
        const int mt = (int) std::ceil (s / c.minHz);
        worstN   = std::max (worstN, mt * 2);
        worstHop = std::max (worstHop, std::max (32, (int) std::lround (c.hopMs / 1000.0 * s)));
    }
    win.assign ((size_t) worstN + 4, 0.0f);
    pend.assign ((size_t) worstHop + 4, 0.0f);
    d.assign ((size_t) worstN + 4, 0.0f);

    configure();
}

void PitchEngine::setMode (Mode m)
{
    if (m == mode) return;
    mode = m;
    if (! win.empty()) configure();
}

void PitchEngine::setResponse (Response r)
{
    if (r == response) return;
    response = r;
    if (! win.empty()) configure();
}

void PitchEngine::configure()
{
    cfg = configFor (mode);
    const float k = response == Response::Fast ? 0.6f : (response == Response::Stable ? 1.7f : 1.0f);

    decim = std::max (1, (int) std::lround (inRate / cfg.targetRate));
    sr = inRate / decim;
    const double fc = std::min ((double) cfg.lowpassHz, sr * 0.42);
    f1.design (inRate, fc, 0.541);
    f2.design (inRate, fc, 1.307);

    minTau = std::max (2, (int) std::floor (sr / cfg.maxHz));
    maxTau = (int) std::ceil (sr / cfg.minHz);
    N = maxTau * 2;
    W = N - maxTau;
    hop = std::max (32, (int) std::lround (cfg.hopMs / 1000.0 * sr));
    const double hopSec = hop / sr;

    startFrames   = std::max (2, (int) std::lround (cfg.startMs   * k / 1000.0 / hopSec));
    releaseFrames = std::max (2, (int) std::lround (cfg.releaseMs * k / 1000.0 / hopSec));
    changeFrames  = std::max (2, (int) std::lround (cfg.changeMs  * k / 1000.0 / hopSec));
    medianLen     = response == Response::Fast ? 3 : (response == Response::Stable ? 7 : cfg.medianLen);

    reset();
}

void PitchEngine::reset()
{
    std::fill (win.begin(), win.end(), 0.0f);
    pendN = 0; phase = 0; decCount = 0; prevX = 0; hfAcc = 0;
    f1.clear(); f2.clear();
    histN = dbHistN = hfHistN = 0;
    unvoicedRun = 0;
    stableN = 0; stablePeak = -120.0f; stableSum = 0.0f; onsetPending = false; onsetWait = 0;
    noteActive = false; noteFrames = 0; devN = 0; devSum = 0;
    framesSinceOnset = 1000; lastBend = 0;
    curMidi = -1.0f; curDb = -120.0f;
}

int PitchEngine::latencySamples() const
{
    return (int) ((N / 2 + startFrames * hop) * decim);
}

// ------------------------------------------------------------------ processing
int PitchEngine::process (const float* x, int n, NoteEvent* out, int maxOut)
{
    outBuf = out; outMax = maxOut; outN = 0;
    if (win.empty()) return 0;

    for (int i = 0; i < n; ++i)
    {
        const float xi = x[i];
        const float dx = xi - prevX;
        prevX = xi;
        hfAcc += (double) dx * dx;
        const float y = f2.tick (f1.tick (xi));
        if (++phase >= decim)
        {
            phase = 0;
            pend[(size_t) pendN++] = y;
            ++decCount;
            if (pendN == hop) { pendN = 0; analyseFrame (i); }
        }
    }
    // denormal guard
    if (std::fabs (f1.y1) < 1e-20f) f1.clear();
    if (std::fabs (f2.y1) < 1e-20f) f2.clear();
    return outN;
}

int PitchEngine::flush (NoteEvent* out, int maxOut, int off)
{
    outBuf = out; outMax = maxOut; outN = 0;
    if (noteActive) emitOff (off);
    stableN = 0; devN = 0; onsetPending = false;
    return outN;
}

void PitchEngine::emitOn (int note, int vel, int off)
{
    if (bendEnabled && outN < outMax) { outBuf[outN++] = { NoteEvent::Bend, note, 0, 0.0f, off }; lastBend = 0; }
    if (outN < outMax) outBuf[outN++] = { NoteEvent::NoteOn, note, vel, 0.0f, off };
    noteActive = true; notePitch = note; noteFrames = 0; noteRef = (float) note; devN = 0; devSum = 0;
}

void PitchEngine::emitOff (int off)
{
    if (! noteActive) return;
    if (outN < outMax) outBuf[outN++] = { NoteEvent::NoteOff, notePitch, 0, 0.0f, off };
    noteActive = false; devN = 0; devSum = 0;
}

int PitchEngine::velocityFrom (float db) const
{
    const float top = -6.0f, lo = gateDb;
    const float v = 30.0f + (db - lo) / std::max (6.0f, top - lo) * 97.0f;
    return clampv ((int) std::lround (v), 1, 127);
}

void PitchEngine::analyseFrame (int off)
{
    float* w = win.data();
    std::memmove (w, w + hop, sizeof (float) * (size_t) (N - hop));
    std::copy (pend.begin(), pend.begin() + hop, w + (N - hop));

    double e = 0.0;
    for (int i = N - hop; i < N; ++i) e += (double) w[i] * w[i];
    const float db   = (float) (10.0 * std::log10 (e / hop + 1e-12));
    const float hfDb = (float) (10.0 * std::log10 (hfAcc / ((double) hop * decim) + 1e-14));
    hfAcc = 0.0;
    pushHist (hfHist, hfHistN, kHist, hfDb);
    pushHist (dbHist, dbHistN, kHist, db);
    curDb = db;

    float midi = -1.0f;
    float freq = 0.0f;
    if (db > gateDb && decCount >= N && yin (freq))
    {
        const float raw = hzToMidi (freq);
        if (unvoicedRun > 2) histN = 0;
        pushHist (hist, histN, medianLen, raw);
        midi = medianOf (hist, histN);
        unvoicedRun = 0;
    }
    else
    {
        ++unvoicedRun;
    }
    curMidi = midi;
    segment (midi, db, off);
}

bool PitchEngine::yin (float& freqOut)
{
    const float* x = win.data();
    float* dd = d.data();
    dd[0] = 1.0f;
    double run = 0.0;
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        float s = 0.0f;
        // align to the newest samples so short periods are detected with less delay
        const float* a = x + (N - W - tau);
        const float* b = a + tau;
        for (int j = 0; j < W; ++j) { const float v = a[j] - b[j]; s += v * v; }
        run += s;
        dd[tau] = run > 0.0 ? (float) (s * tau / run) : 1.0f;
    }

    int tau = -1;
    for (int k = minTau; k <= maxTau; ++k)
    {
        if (dd[k] < cfg.threshold)
        {
            while (k + 1 <= maxTau && dd[k + 1] < dd[k]) ++k;
            tau = k;
            break;
        }
    }
    if (tau < 0) return false;

    float better = (float) tau;
    if (tau > 1 && tau < maxTau)
    {
        const float a = dd[tau - 1], b = dd[tau], c = dd[tau + 1];
        const float den = a - 2.0f * b + c;
        if (den > 0.0f) better = tau + (a - c) / (2.0f * den);
    }
    const float f = (float) (sr / better);
    if (f < cfg.minHz * 0.97f || f > cfg.maxHz * 1.03f) return false;
    freqOut = f;
    return true;
}

// ------------------------------------------------------------------ segmentation
// Accumulates stable voiced frames; returns true (and the note) once the pitch has
// held for startFrames. Pitches near a semitone boundary wait a little longer.
bool PitchEngine::candidate (float midi, float db, int& noteOut)
{
    if (stableN > 0 && std::fabs (midi - stableSum / stableN) > 0.6f) stableN = 0;
    if (stableN == 0) { stableSum = 0.0f; stablePeak = db; }
    stableSum += midi;
    stablePeak = std::max (stablePeak, db);
    ++stableN;
    if (stableN < startFrames) return false;
    const float mean = stableSum / stableN;
    const float frac = std::fabs (mean - std::round (mean));
    if (frac > 0.3f && stableN < 3 * startFrames) return false;
    noteOut = clampv ((int) std::lround (mean), 0, 127);
    stableN = 0;
    return true;
}

void PitchEngine::segment (float midi, float db, int off)
{
    const bool voiced = midi >= 0.0f;
    ++framesSinceOnset;

    if (! noteActive)
    {
        if (! voiced) { stableN = 0; return; }
        int nn = 0;
        if (candidate (midi, db, nn)) emitOn (nn, velocityFrom (stablePeak), off);
        return;
    }

    ++noteFrames;

    // re-pluck: sharp level or pick-transient rise on a sounding note
    if (cfg.onsetDb > 0.0f && voiced && noteFrames > startFrames + 4 && framesSinceOnset > 12
        && dbHistN >= 5 && hfHistN >= 5)
    {
        const float before   = minOf (dbHist, dbHistN - 2);
        const float hfBefore = minOf (hfHist, hfHistN - 2);
        const float hfNow    = hfHist[hfHistN - 1];
        const bool levelRise = db - before > cfg.onsetDb;
        const bool pickRise  = hfNow - hfBefore > cfg.onsetDb + 5.0f && db - before > 1.0f;
        if ((levelRise || pickRise) && db > gateDb + 10.0f)
        {
            // re-pluck: confirm the (possibly new) pitch before re-triggering
            framesSinceOnset = 0;
            onsetPending = true;
            onsetWait = 0;
            stableN = 0;
            histN = 0;   // don't let the old note's pitch history bias the new one
        }
    }

    if (onsetPending)
    {
        if (! voiced)
        {
            if (unvoicedRun >= releaseFrames) { onsetPending = false; emitOff (off); }
            return;
        }
        int nn = 0;
        const bool ready = candidate (midi, db, nn);
        if (ready || ++onsetWait > 4 * startFrames)
        {
            if (! ready) nn = clampv ((int) std::lround (midi), 0, 127);
            onsetPending = false;
            emitOff (off);
            emitOn (nn, velocityFrom (std::max (db, stablePeak)), off);
        }
        return;
    }

    if (! voiced)
    {
        if (unvoicedRun >= releaseFrames) emitOff (off);
        return;
    }

    if (std::fabs (midi - (float) notePitch) > cfg.changeSemis)
    {
        if (devN > 0 && std::fabs (midi - devFirst) > 0.6f) { devN = 0; devSum = 0; }
        if (devN == 0) devFirst = midi;
        devSum += midi;
        if (++devN >= changeFrames)
        {
            const int np = clampv ((int) std::lround (devSum / devN), 0, 127);
            emitOff (off);
            emitOn (np, velocityFrom (db), off);
        }
        return;
    }

    devN = 0; devSum = 0;
    noteRef += 0.3f * (midi - noteRef);

    if (bendEnabled && outN < outMax)
    {
        const float semis = midi - (float) notePitch;
        if (std::fabs (semis - lastBend) > 0.02f)
        {
            outBuf[outN++] = { NoteEvent::Bend, notePitch, 0, semis, off };
            lastBend = semis;
        }
    }
}

} // namespace murmur
