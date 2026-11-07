// Offline test for PitchEngine with synthetic voice / plucked-string signals.
// Measures note sequence correctness and note-on latency from true onset.
#include "../Source/PitchEngine.h"
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <random>
#include <vector>
#include <string>
using namespace murmur;
static const double SR = 48000;
static std::mt19937 rng (1);
static float noise (float a) { return std::uniform_real_distribution<float> (-a, a) (rng); }
static double m2f (double m) { return 440.0 * std::pow (2.0, (m - 69) / 12); }
struct Seg { int m; double dur, gap; };

static std::vector<float> voice (const std::vector<Seg>& seq, double vib = 0.3) {
    std::vector<float> o; double ph = 0;
    for (auto s : seq) {
        int n = (int) (s.dur * SR);
        for (int i = 0; i < n; ++i) {
            double t = i / SR, f = m2f (s.m + vib * std::sin (2 * M_PI * 5.5 * t));
            ph += 2 * M_PI * f / SR;
            double env = std::min (1.0, t / 0.03) * std::min (1.0, (s.dur - t) / 0.04), v = 0;
            for (int h = 1; h <= 8; ++h) v += std::sin (ph * h) / (h * h * 0.5 + 0.5);
            o.push_back ((float) (0.25 * env * v) + noise (0.002f));
        }
        for (int i = 0; i < s.gap * SR; ++i) o.push_back (noise (0.002f));
    }
    return o;
}
static std::vector<float> pluck (const std::vector<Seg>& seq, bool legato = false) {
    std::vector<float> o;
    for (auto s : seq) {
        int p = (int) std::lround (SR / m2f (s.m));
        std::vector<float> b (p); for (auto& v : b) v = noise (1);
        int k = 0, n = (int) (s.dur * SR);
        for (int i = 0; i < n; ++i) {
            int nx = (k + 1) % p; float v = b[k]; b[k] = 0.4985f * (b[k] + b[nx]); k = nx;
            float damp = legato ? 1.f : (float) std::min (1.0, (s.dur - i / SR) / 0.02);
            o.push_back (0.5f * v * damp + noise (0.001f));
        }
        for (int i = 0; i < s.gap * SR; ++i) o.push_back (noise (0.001f));
    }
    return o;
}

static int fails = 0;
static void check (const char* name, Mode mode, Response r, const std::vector<Seg>& seq, const std::vector<float>& sig, bool bend = false) {
    PitchEngine e; e.prepare (SR); e.setMode (mode); e.setResponse (r); e.setBendEnabled (bend);
    const int B = 128; NoteEvent ev[64];
    std::vector<int> got; std::vector<double> onT; int ons = 0, offs = 0, bends = 0, maxEv = 0; bool overlap = false; bool on = false;
    for (size_t pos = 0; pos < sig.size(); pos += B) {
        int n = (int) std::min<size_t> (B, sig.size() - pos);
        int c = e.process (sig.data() + pos, n, ev, 64); maxEv = std::max (maxEv, c);
        for (int i = 0; i < c; ++i) {
            if (ev[i].type == NoteEvent::NoteOn) { if (on) overlap = true; on = true; ++ons; got.push_back (ev[i].note); onT.push_back ((pos + ev[i].sampleOffset) / SR); }
            else if (ev[i].type == NoteEvent::NoteOff) { on = false; ++offs; }
            if (getenv("DBG") && ev[i].type != NoteEvent::Bend) std::printf("   %s %d @%.3f\n", ev[i].type==NoteEvent::NoteOn?"on ":"off", ev[i].note, (pos+ev[i].sampleOffset)/SR);
            else ++bends;
        }
    }
    int c = e.flush (ev, 64, 0); for (int i = 0; i < c; ++i) if (ev[i].type == NoteEvent::NoteOff) { ++offs; on = false; }
    std::vector<int> want; std::vector<double> onsets; double t = 0;
    for (auto s : seq) { want.push_back (s.m); onsets.push_back (t); t += s.dur + s.gap; }
    bool ok = got == want && ons == offs && ! overlap;
    std::string lat;
    double worst = 0;
    if (got == want) for (size_t i = 0; i < onT.size(); ++i) { double l = (onT[i] - onsets[i]) * 1000; worst = std::max (worst, l); lat += std::to_string ((int) l) + " "; }
    if (worst > (r == Response::Stable ? 170 : 120)) ok = false;
    if (! ok) ++fails;
    std::printf ("%s %-28s %-9s latency ms: %s%s\n", ok ? "PASS" : "FAIL", name, r == Response::Fast ? "fast" : r == Response::Stable ? "stable" : "balanced", lat.c_str(), bend ? (" bends=" + std::to_string (bends)).c_str() : "");
    if (! ok) { std::printf ("   want"); for (int w : want) std::printf (" %d", w); std::printf ("\n   got "); for (int g : got) std::printf (" %d", g); std::printf ("  on=%d off=%d overlap=%d\n", ons, offs, (int) overlap); }
}

int main() {
    std::vector<Seg> vseq { {57,.5,.15},{60,.4,.1},{64,.6,.1},{67,.3,.1},{72,.5,.2},{48,.5,.1} };
    std::vector<Seg> vleg { {60,.4,0},{62,.4,0},{64,.4,0},{65,.5,.2} };
    std::vector<Seg> gseq { {40,.5,.05},{45,.4,.05},{52,.4,.05},{59,.3,.05},{64,.5,.05},{76,.4,.05},{88,.3,.1} };
    std::vector<Seg> grep { {55,.3,0},{55,.3,0},{55,.3,0},{57,.4,0} };
    std::vector<Seg> bseq { {28,.6,.05},{33,.5,.05},{38,.5,.05},{43,.4,.05},{31,.5,.05},{24,.7,.1} };
    std::vector<Seg> brep { {33,.35,0},{33,.35,0},{36,.35,0},{33,.4,0} };
    for (Response r : { Response::Fast, Response::Balanced, Response::Stable }) {
        check ("sung melody", Mode::Voice, r, vseq, voice (vseq));
        check ("legato sung", Mode::Voice, r, vleg, voice (vleg));
        check ("guitar notes", Mode::Guitar, r, gseq, pluck (gseq));
        check ("guitar re-plucks", Mode::Guitar, r, grep, pluck (grep, true));
        check ("bass line", Mode::Bass, r, bseq, pluck (bseq));
        check ("bass re-plucks", Mode::Bass, r, brep, pluck (brep, true));
    }
    std::vector<Seg> sus { {52,2.5,.1} }; check ("long guitar sustain", Mode::Guitar, Response::Balanced, sus, pluck (sus));
    std::vector<Seg> bs { {28,3,.1} };    check ("long bass sustain", Mode::Bass, Response::Balanced, bs, pluck (bs));
    std::vector<Seg> wv { {62,2,0} };     check ("wide vibrato", Mode::Voice, Response::Balanced, wv, voice (wv, 0.55));
    check ("vibrato with bend", Mode::Voice, Response::Balanced, wv, voice (wv, 0.55), true);
    { std::vector<Seg> s { {45,.5,.1},{50,.5,.1},{57,.5,.1} }; auto x = pluck (s); for (auto& v : x) v += noise (0.02f); check ("noisy guitar", Mode::Guitar, Response::Balanced, s, x); }
    for (Mode m : { Mode::Voice, Mode::Guitar, Mode::Bass }) {
        std::vector<float> x (SR * 2); for (auto& v : x) v = noise (0.003f);
        check ("room noise", m, Response::Fast, {}, x);
    }
    // sample-rate sweep
    for (double rate : { 44100.0, 96000.0 }) {
        PitchEngine e; e.prepare (rate); e.setMode (Mode::Guitar); std::printf ("rate %.0f: latency estimate %d samples (%.1f ms)\n", rate, e.latencySamples(), e.latencySamples() * 1000 / rate);
    }
    // CPU
    { auto x = pluck (gseq); PitchEngine e; e.prepare (SR); e.setMode (Mode::Bass); NoteEvent ev[64];
      auto t0 = std::clock(); for (int rep = 0; rep < 20; ++rep) for (size_t p = 0; p + 128 <= x.size(); p += 128) e.process (x.data() + p, 128, ev, 64);
      double cpu = double (std::clock() - t0) / CLOCKS_PER_SEC, audio = 20.0 * x.size() / SR;
      std::printf ("CPU: %.2f%% of one core (bass mode)\n", 100 * cpu / audio); }
    std::printf (fails ? "\n%d FAILED\n" : "\nALL PASS\n", fails);
    return fails ? 1 : 0;
}
