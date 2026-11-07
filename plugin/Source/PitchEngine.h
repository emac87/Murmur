// PitchEngine — real-time monophonic pitch → MIDI note tracker.
// Plain C++17, no JUCE dependency. All allocation happens in prepare();
// process() is allocation-free and safe to call on the audio thread.
#pragma once

#include <cstdint>
#include <vector>

namespace murmur
{

enum class Mode     : int { Voice = 0, Guitar = 1, Bass = 2 };
enum class Response : int { Fast = 0, Balanced = 1, Stable = 2 };

struct NoteEvent
{
    enum Type : uint8_t { NoteOn, NoteOff, Bend };
    Type  type;
    int   note;          // raw detected MIDI note (no transpose)
    int   velocity;      // 1..127 for NoteOn
    float bendSemis;     // for Bend: detected pitch minus sounding note, semitones
    int   sampleOffset;  // position inside the processed block
};

struct ModeConfig
{
    float minHz, maxHz, targetRate, lowpassHz, hopMs, threshold;
    float startMs, releaseMs, changeMs, changeSemis, onsetDb;
    int   medianLen;
};

const ModeConfig& configFor (Mode m);

class PitchEngine
{
public:
    void prepare (double sampleRate);          // allocates; call off the audio thread
    void reset();                              // clears state (does not emit note-offs)

    void setMode (Mode m);                     // cheap; reconfigures without allocating
    void setResponse (Response r);
    void setGateDb (float db)        { gateDb = db; }
    void setBendEnabled (bool b)     { bendEnabled = b; }

    // Process a block of mono samples. Writes up to maxOut events, returns count.
    int process (const float* mono, int numSamples, NoteEvent* out, int maxOut);

    // If a note is sounding, emit its NoteOff (e.g. on bypass / mode change).
    int flush (NoteEvent* out, int maxOut, int sampleOffset);

    float currentMidi()  const { return curMidi; }   // -1 when unvoiced
    float currentDb()    const { return curDb; }
    int   activeNote()   const { return noteActive ? notePitch : -1; }
    int   latencySamples() const;                    // analysis window, at input rate (approx.)

private:
    void configure();
    void analyseFrame (int sampleOffset);
    bool yin (float& freqOut);
    void segment (float midi, float db, int off);
    bool candidate (float midi, float db, int& noteOut);
    void emitOn  (int note, int vel, int off);
    void emitOff (int off);
    int  velocityFrom (float db) const;

    struct Biquad
    {
        float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        void design (double sr, double fc, double q);
        inline float tick (float x) noexcept
        {
            const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
        void clear() { x1 = x2 = y1 = y2 = 0; }
    };

    // config
    double inRate = 48000.0;
    Mode mode = Mode::Guitar;
    Response response = Response::Balanced;
    ModeConfig cfg {};
    float gateDb = -50.0f;
    bool  bendEnabled = false;

    int decim = 2;
    double sr = 24000.0;
    int minTau = 2, maxTau = 300, N = 600, W = 300, hop = 200;
    int startFrames = 3, releaseFrames = 4, changeFrames = 4, medianLen = 5;

    // buffers (sized in prepare for the worst case of all modes)
    std::vector<float> win, pend, d;
    int pendN = 0, phase = 0;
    int64_t decCount = 0;
    Biquad f1, f2;
    float prevX = 0.0f;
    double hfAcc = 0.0;

    // history
    static constexpr int kHist = 8;
    float hist[kHist] {}; int histN = 0;
    float dbHist[kHist] {}; int dbHistN = 0;
    float hfHist[kHist] {}; int hfHistN = 0;
    int unvoicedRun = 0;

    // idle candidate
    float stableSum = 0.0f; int stableN = 0; float stablePeak = -120.0f;
    bool  onsetPending = false; int onsetWait = 0;
    // active note
    bool  noteActive = false;
    int   notePitch = 0;
    int   noteFrames = 0;     // frames since note-on
    float noteRef = 0.0f;     // smoothed pitch inside the note
    float devFirst = 0.0f; int devN = 0; float devSum = 0.0f;
    int   framesSinceOnset = 1000;
    float lastBend = 0.0f;

    // output
    NoteEvent* outBuf = nullptr; int outMax = 0, outN = 0;

    float curMidi = -1.0f, curDb = -120.0f;
};

} // namespace murmur
