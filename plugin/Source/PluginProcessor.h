#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include <algorithm>
#include <cstring>
#include <iterator>
#include "PitchEngine.h"

// Lock-free single-producer/single-consumer queue of short MIDI messages,
// filled on the audio thread and drained by the virtual-port sender thread.
struct MidiFifo
{
    static constexpr int kSize = 1024;
    struct Msg { uint8_t b[3]; uint8_t len; };
    juce::AbstractFifo fifo { kSize };
    Msg data[kSize] {};

    bool push (const juce::MidiMessage& m)
    {
        if (m.getRawDataSize() > 3) return false;
        int s1, n1, s2, n2;
        fifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 + n2 < 1) return false;
        Msg& d = data[n1 > 0 ? s1 : s2];
        d.len = (uint8_t) m.getRawDataSize();
        std::memcpy (d.b, m.getRawData(), d.len);
        fifo.finishedWrite (1);
        return true;
    }
};

class VirtualMidiPort; // shared "Murmur Out" CoreMIDI source (one per process)

class MurmurProcessor : public juce::AudioProcessor
{
public:
    MurmurProcessor();
    ~MurmurProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Murmur"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Display state (audio thread writes, UI reads)
    std::atomic<float>    uiMidi        { -1.0f };
    std::atomic<float>    uiLevelDb     { -100.0f };
    std::atomic<int>      uiNote        { -1 };
    std::atomic<int>      uiVelocity    { 0 };
    std::atomic<uint32_t> uiNoteCounter { 0 };

    bool         isVirtualPortOpen() const;
    juce::String getVirtualPortName() const;
    void         panic() { panicRequested.store (true); }

private:
    void emit (const juce::MidiMessage& m, int offset, juce::MidiBuffer& out, bool toPort);
    void silenceActive (int offset, juce::MidiBuffer& out, bool toPort);

    murmur::PitchEngine engine;
    std::vector<float> mono;
    murmur::NoteEvent events[128];

    // cached parameter pointers
    std::atomic<float>* pMode = nullptr;  std::atomic<float>* pGate = nullptr;
    std::atomic<float>* pResp = nullptr;  std::atomic<float>* pChan = nullptr;
    std::atomic<float>* pTrans = nullptr; std::atomic<float>* pVelMode = nullptr;
    std::atomic<float>* pFixedVel = nullptr; std::atomic<float>* pBend = nullptr;
    std::atomic<float>* pBendRange = nullptr; std::atomic<float>* pThru = nullptr;
    std::atomic<float>* pPort = nullptr;

    int lastMode = -1, lastResp = -1;
    bool lastBendOn = false;

    // the note we actually sent (after transpose/channel), so note-offs always match
    int outNote = -1, outChannel = 1;
    bool outBent = false;

    std::atomic<bool> panicRequested { false };

    std::shared_ptr<VirtualMidiPort> port;
    std::shared_ptr<MidiFifo> fifo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MurmurProcessor)
};
