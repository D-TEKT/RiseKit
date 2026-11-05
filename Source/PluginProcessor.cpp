#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::AudioParameterFloatAttributes percent()
    {
        return juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
            .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue() / 100.0f; });
    }

    juce::AudioParameterFloatAttributes decibels()
    {
        return juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; })
            .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); });
    }

    template <size_t N>
    juce::StringArray toStringArray (const std::array<const char*, N>& names)
    {
        juce::StringArray out;
        for (auto* n : names) out.add (n);
        return out;
    }
}

RiseKitProcessor::RiseKitProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pSmile      = raw (params::smile);
    pRise       = raw (params::rise);
    pLength     = raw (params::length);
    pCurve      = raw (params::curve);
    pAutoDrop   = raw (params::autoDrop);
    pFilter     = raw (params::filter);
    pResonance  = raw (params::resonance);
    pPhaser     = raw (params::phaser);
    pDrive      = raw (params::drive);
    pWidth      = raw (params::width);
    pChop       = raw (params::chop);
    pReverb     = raw (params::reverb);
    pDecay      = raw (params::decay);
    pReverbType = raw (params::reverbType);
    pDelay      = raw (params::delay);
    pFeedback   = raw (params::feedback);
    pDelayTime  = raw (params::delayTime);
    pPingPong   = raw (params::pingPong);
    pWash       = raw (params::wash);
    pRide       = raw (params::ride);
    pMix        = raw (params::mix);
    pOutput     = raw (params::output);
    pSafety     = raw (params::safety);
}

juce::AudioProcessorValueTreeState::ParameterLayout RiseKitProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const auto& c = params::presets[0];   // defaults = "Classic Smile"

    // version 1 = first release, version 2 = added in the delay/reverb update
    auto unit = [&] (const char* id, const char* name, float def, int version = 1)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, version }, name,
                                                            NormalisableRange<float> (0.0f, 1.0f), def, percent()));
    };

    unit (params::smile, "Smile", 0.0f);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { params::rise, 1 }, "Rise", false));

    StringArray lengths;
    for (auto bars : params::lengthBars)
        lengths.add (String (bars) + (bars == 1 ? " bar" : " bars"));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { params::length, 1 }, "Length",
                                                         lengths, params::defaultLengthIndex));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { params::curve, 1 }, "Curve",
                                                        NormalisableRange<float> (-1.0f, 1.0f), c.curve,
                                                        AudioParameterFloatAttributes()
                                                            .withStringFromValueFunction ([] (float v, int)
                                                            {
                                                                if (std::abs (v) < 0.02f) return String ("Linear");
                                                                return String (v < 0.0f ? "Fast " : "Slow ") + String (roundToInt (std::abs (v) * 100.0f)) + " %";
                                                            })));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { params::autoDrop, 1 }, "Auto Drop", false));

    unit (params::filter,    "Filter",    c.filter);
    unit (params::resonance, "Resonance", c.resonance);
    unit (params::phaser,    "Phaser",    c.phaser);
    unit (params::drive,     "Drive",     c.drive);
    unit (params::width,     "Width",     c.width);
    unit (params::chop,      "Chop",      c.chop);

    unit (params::reverb,    "Reverb",    c.reverb);
    unit (params::decay,     "Decay",     c.decay, 2);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { params::reverbType, 2 }, "Reverb Type",
                                                         toStringArray (params::reverbTypeNames), c.reverbType));
    unit (params::delay,     "Delay",     c.delay, 2);
    unit (params::feedback,  "Feedback",  c.feedback, 2);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { params::delayTime, 2 }, "Delay Time",
                                                         toStringArray (params::delayTimeNames), params::defaultDelayTimeIndex));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { params::pingPong, 2 }, "Ping-Pong", c.pingPong));
    unit (params::wash,      "Wash",      c.wash, 2);

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { params::ride, 1 }, "Level Ride",
                                                        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), c.ride, decibels()));
    unit (params::mix, "Mix", 1.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { params::output, 1 }, "Output",
                                                        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, decibels()));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { params::safety, 1 }, "Safety Clip", true));

    return { p.begin(), p.end() };
}

bool RiseKitProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

void RiseKitProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    ramp.reset();
}

void RiseKitProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (buffer.getNumChannels() < 2 || numSamples == 0)
        return;

    // Host tempo / position
    double bpm = 128.0, ppq = -1.0, beatsPerBar = 4.0;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())          bpm = *b;
            if (pos->getIsPlaying())
                if (auto q = pos->getPpqPosition()) ppq = *q;
            if (auto ts = pos->getTimeSignature())
                if (ts->denominator > 0)
                    beatsPerBar = ts->numerator * 4.0 / ts->denominator;
        }
    }

    const int lengthIndex = juce::jlimit (0, (int) params::lengthBars.size() - 1, (int) pLength->load());
    const double lengthBeats = params::lengthBars[(size_t) lengthIndex] * beatsPerBar;

    const float autoValue = ramp.advance (pRise->load() > 0.5f, bpm, numSamples, getSampleRate(),
                                          lengthBeats, pCurve->load(), pAutoDrop->load() > 0.5f);

    rise::Settings s;
    s.smile      = std::max (pSmile->load(), autoValue);
    s.filter     = pFilter->load();
    s.resonance  = pResonance->load();
    s.phaser     = pPhaser->load();
    s.drive      = pDrive->load();
    s.width      = pWidth->load();
    s.chop       = pChop->load();
    s.reverb     = pReverb->load();
    s.decay      = pDecay->load();
    s.reverbType = juce::roundToInt (pReverbType->load());
    s.delay      = pDelay->load();
    s.feedback   = pFeedback->load();
    s.delayTime  = juce::roundToInt (pDelayTime->load());
    s.pingPong   = pPingPong->load() > 0.5f;
    s.wash       = pWash->load();
    s.rideDb     = pRide->load();
    s.mix        = pMix->load();
    s.outputDb   = pOutput->load();
    s.safetyClip = pSafety->load() > 0.5f;
    s.bpm        = bpm;
    s.ppq        = ppq;

    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples, s);

    liveSmile.store (engine.getSmoothedSmile());
    riseProgress.store (ramp.getProgress());
}

void RiseKitProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;

    currentProgram = index;
    const auto& pr = params::presets[(size_t) index];

    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    set (params::filter,     pr.filter);
    set (params::resonance,  pr.resonance);
    set (params::phaser,     pr.phaser);
    set (params::drive,      pr.drive);
    set (params::width,      pr.width);
    set (params::chop,       pr.chop);
    set (params::reverb,     pr.reverb);
    set (params::decay,      pr.decay);
    set (params::reverbType, (float) pr.reverbType);
    set (params::delay,      pr.delay);
    set (params::feedback,   pr.feedback);
    set (params::delayTime,  (float) pr.delayTime);
    set (params::pingPong,   pr.pingPong ? 1.0f : 0.0f);
    set (params::wash,       pr.wash);
    set (params::ride,       pr.ride);
    set (params::curve,      pr.curve);
}

const juce::String RiseKitProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, getNumPrograms()))
        return params::presets[(size_t) index].name;
    return {};
}

void RiseKitProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RiseKitProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* RiseKitProcessor::createEditor()
{
    return new RiseKitEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RiseKitProcessor();
}
