#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// One virtual MIDI source per process, shared by every Murmur instance.
// Each instance owns a lock-free FIFO; a background thread drains them all.
class VirtualMidiPort : private juce::Thread
{
public:
    static constexpr const char* kName = "Murmur Out";

    static std::shared_ptr<VirtualMidiPort> get()
    {
        static std::mutex m;
        static std::weak_ptr<VirtualMidiPort> weak;
        std::lock_guard<std::mutex> lock (m);
        auto p = weak.lock();
        if (! p) { p = std::shared_ptr<VirtualMidiPort> (new VirtualMidiPort()); weak = p; }
        return p;
    }

    ~VirtualMidiPort() override
    {
        stopThread (500);
        output.reset();
    }

    bool isOpen() const { return output != nullptr; }

    void add (std::shared_ptr<MidiFifo> f)
    {
        std::lock_guard<std::mutex> lock (listLock);
        fifos.push_back (std::move (f));
    }

    void remove (const std::shared_ptr<MidiFifo>& f)
    {
        drain (*f);  // deliver anything still queued (e.g. final note-offs)
        std::lock_guard<std::mutex> lock (listLock);
        fifos.erase (std::remove (fifos.begin(), fifos.end(), f), fifos.end());
    }

private:
    VirtualMidiPort() : juce::Thread ("Murmur MIDI out")
    {
       #if JUCE_MAC || JUCE_LINUX || JUCE_IOS
        output = juce::MidiOutput::createNewDevice (kName);
       #endif
        if (output != nullptr)
            startThread (juce::Thread::Priority::highest);
    }

    void drain (MidiFifo& f)
    {
        std::lock_guard<std::mutex> lock (sendLock);
        int s1, n1, s2, n2;
        const int ready = f.fifo.getNumReady();
        if (ready == 0) return;
        f.fifo.prepareToRead (ready, s1, n1, s2, n2);
        auto sendRange = [&] (int start, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                const auto& d = f.data[start + i];
                if (output != nullptr) output->sendMessageNow (juce::MidiMessage (d.b, d.len));
            }
        };
        sendRange (s1, n1);
        sendRange (s2, n2);
        f.fifo.finishedRead (n1 + n2);
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            {
                std::lock_guard<std::mutex> lock (listLock);
                for (auto& f : fifos) drain (*f);
            }
            wait (1); // ~1 ms polling keeps added latency below a millisecond
        }
    }

    std::unique_ptr<juce::MidiOutput> output;
    std::mutex listLock, sendLock;
    std::vector<std::shared_ptr<MidiFifo>> fifos;
};

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MurmurProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Voice", "Guitar", "Bass" }, 1));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "gate", 1 }, "Noise Gate", NormalisableRange<float> (-80.0f, -20.0f, 0.5f), -50.0f,
                                                         AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "response", 1 }, "Response", StringArray { "Fast", "Balanced", "Stable" }, 1));
    p.push_back (std::make_unique<AudioParameterInt>    (ParameterID { "channel", 1 }, "MIDI Channel", 1, 16, 1));
    p.push_back (std::make_unique<AudioParameterInt>    (ParameterID { "transpose", 1 }, "Transpose", -24, 24, 0,
                                                         AudioParameterIntAttributes().withLabel ("st")));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "velMode", 1 }, "Velocity Mode", StringArray { "Dynamic", "Fixed" }, 0));
    p.push_back (std::make_unique<AudioParameterInt>    (ParameterID { "fixedVel", 1 }, "Fixed Velocity", 1, 127, 100));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "bend", 1 }, "Pitch Bend", false));
    p.push_back (std::make_unique<AudioParameterInt>    (ParameterID { "bendRange", 1 }, "Bend Range", 1, 12, 2,
                                                         AudioParameterIntAttributes().withLabel ("st")));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "thru", 1 }, "Audio Thru", true));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "virtualPort", 1 }, "Virtual MIDI Port", true));
    return { p.begin(), p.end() };
}

MurmurProcessor::MurmurProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MURMUR", createLayout())
{
    pMode      = apvts.getRawParameterValue ("mode");
    pGate      = apvts.getRawParameterValue ("gate");
    pResp      = apvts.getRawParameterValue ("response");
    pChan      = apvts.getRawParameterValue ("channel");
    pTrans     = apvts.getRawParameterValue ("transpose");
    pVelMode   = apvts.getRawParameterValue ("velMode");
    pFixedVel  = apvts.getRawParameterValue ("fixedVel");
    pBend      = apvts.getRawParameterValue ("bend");
    pBendRange = apvts.getRawParameterValue ("bendRange");
    pThru      = apvts.getRawParameterValue ("thru");
    pPort      = apvts.getRawParameterValue ("virtualPort");

    fifo = std::make_shared<MidiFifo>();
    port = VirtualMidiPort::get();
    port->add (fifo);
}

MurmurProcessor::~MurmurProcessor()
{
    // make sure nothing is left hanging on the virtual port
    if (outNote >= 0)
    {
        fifo->push (juce::MidiMessage::noteOff (outChannel, outNote));
        if (outBent) fifo->push (juce::MidiMessage::pitchWheel (outChannel, 8192));
    }
    port->remove (fifo);
}

bool MurmurProcessor::isVirtualPortOpen() const        { return port != nullptr && port->isOpen(); }
juce::String MurmurProcessor::getVirtualPortName() const { return VirtualMidiPort::kName; }

bool MurmurProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == out;
}

void MurmurProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    mono.assign ((size_t) juce::jmax (samplesPerBlock, 4096), 0.0f);
    lastMode = lastResp = -1;
}

void MurmurProcessor::releaseResources()
{
    if (outNote >= 0)
    {
        fifo->push (juce::MidiMessage::noteOff (outChannel, outNote));
        if (outBent) fifo->push (juce::MidiMessage::pitchWheel (outChannel, 8192));
        outNote = -1; outBent = false;
    }
    engine.reset();
}

//==============================================================================
void MurmurProcessor::emit (const juce::MidiMessage& m, int offset, juce::MidiBuffer& out, bool toPort)
{
    out.addEvent (m, offset);
    if (toPort) fifo->push (m);
}

void MurmurProcessor::silenceActive (int offset, juce::MidiBuffer& out, bool toPort)
{
    if (outNote >= 0) emit (juce::MidiMessage::noteOff (outChannel, outNote), offset, out, toPort);
    if (outBent) emit (juce::MidiMessage::pitchWheel (outChannel, 8192), offset, out, toPort);
    outNote = -1; outBent = false;
    uiNote.store (-1, std::memory_order_relaxed);
}

void MurmurProcessor::processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer& midi)
{
    midi.clear();
    const bool toPort = pPort->load() > 0.5f;
    murmur::NoteEvent ev[4];
    engine.flush (ev, 4, 0);
    silenceActive (0, midi, toPort);
    uiMidi.store (-1.0f, std::memory_order_relaxed);
}

void MurmurProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();   // we don't accept MIDI input; output only what we generate

    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());
    const bool toPort = pPort->load() > 0.5f;

    // ---- panic
    if (panicRequested.exchange (false))
    {
        murmur::NoteEvent ev[4];
        engine.flush (ev, 4, 0);
        silenceActive (0, midi, toPort);
        for (int ch = 1; ch <= 16; ++ch)
            emit (juce::MidiMessage::allNotesOff (ch), 0, midi, toPort);
    }

    // ---- parameters
    const int mode = (int) pMode->load();
    const int resp = (int) pResp->load();
    const bool bendOn = pBend->load() > 0.5f;
    if (mode != lastMode || resp != lastResp)
    {
        if (lastMode >= 0)
        {
            murmur::NoteEvent ev[4];
            engine.flush (ev, 4, 0);
            silenceActive (0, midi, toPort);
        }
        engine.setMode ((murmur::Mode) mode);
        engine.setResponse ((murmur::Response) resp);
        lastMode = mode; lastResp = resp;
    }
    if (bendOn != lastBendOn)
    {
        if (! bendOn && outBent) { emit (juce::MidiMessage::pitchWheel (outChannel, 8192), 0, midi, toPort); outBent = false; }
        lastBendOn = bendOn;
    }
    engine.setGateDb (pGate->load());
    engine.setBendEnabled (bendOn);

    const int channel   = juce::jlimit (1, 16, (int) pChan->load());
    const int transpose = (int) pTrans->load();
    const bool fixedVel = pVelMode->load() > 0.5f;
    const int  velFixed = juce::jlimit (1, 127, (int) pFixedVel->load());
    const float bendRange = (float) juce::jlimit (1, 12, (int) pBendRange->load());

    // ---- analyse in chunks of the preallocated mono buffer
    const int chunk = (int) mono.size();
    for (int start = 0; start < numSamples; start += chunk)
    {
        const int n = juce::jmin (chunk, numSamples - start);
        float* m = mono.data();
        if (numCh <= 0) std::fill (m, m + n, 0.0f);
        else
        {
            juce::FloatVectorOperations::copy (m, buffer.getReadPointer (0, start), n);
            for (int ch = 1; ch < numCh; ++ch)
                juce::FloatVectorOperations::add (m, buffer.getReadPointer (ch, start), n);
            if (numCh > 1) juce::FloatVectorOperations::multiply (m, 1.0f / (float) numCh, n);
        }

        const int count = engine.process (m, n, events, (int) std::size (events));
        for (int i = 0; i < count; ++i)
        {
            const auto& e = events[i];
            const int off = juce::jlimit (0, numSamples - 1, start + e.sampleOffset);
            switch (e.type)
            {
                case murmur::NoteEvent::NoteOn:
                {
                    silenceActive (off, midi, toPort);            // mono: never overlap
                    const int note = juce::jlimit (0, 127, e.note + transpose);
                    const int vel  = fixedVel ? velFixed : e.velocity;
                    emit (juce::MidiMessage::noteOn (channel, note, (juce::uint8) vel), off, midi, toPort);
                    outNote = note; outChannel = channel;
                    uiNote.store (note, std::memory_order_relaxed);
                    uiVelocity.store (vel, std::memory_order_relaxed);
                    uiNoteCounter.fetch_add (1, std::memory_order_relaxed);
                    break;
                }
                case murmur::NoteEvent::NoteOff:
                    silenceActive (off, midi, toPort);
                    break;
                case murmur::NoteEvent::Bend:
                {
                    if (! bendOn) break;
                    const float semis = juce::jlimit (-bendRange, bendRange, e.bendSemis);
                    const int value = juce::jlimit (0, 16383, (int) std::lround (8192.0f + semis / bendRange * 8191.0f));
                    // bend belongs to the note's channel; before the first note use the current channel
                    emit (juce::MidiMessage::pitchWheel (outNote >= 0 ? outChannel : channel, value), off, midi, toPort);
                    outBent = value != 8192;
                    break;
                }
            }
        }
    }

    uiMidi.store (engine.currentMidi(), std::memory_order_relaxed);
    uiLevelDb.store (engine.currentDb(), std::memory_order_relaxed);

    if (pThru->load() < 0.5f)
        buffer.clear();
    else
        for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
            buffer.clear (ch, 0, numSamples);
}

//==============================================================================
juce::AudioProcessorEditor* MurmurProcessor::createEditor() { return new MurmurEditor (*this); }

void MurmurProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void MurmurProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MurmurProcessor(); }
