#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

static constexpr float kBaseDelaySec = 0.004f; // atraso base para o wow/flutter (reportado como latencia)

TapeBoxProcessor::TapeBoxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pDrive  = apvts.getRawParameterValue ("drive");
    pColor  = apvts.getRawParameterValue ("color");
    pWow    = apvts.getRawParameterValue ("wow");
    pNoise  = apvts.getRawParameterValue ("noise");
    pTone   = apvts.getRawParameterValue ("tone");
    pOutput = apvts.getRawParameterValue ("output");
    pAge    = apvts.getRawParameterValue ("age");
    pTape   = apvts.getRawParameterValue ("tape");
    pComp   = apvts.getRawParameterValue ("comp");
}

AudioProcessorValueTreeState::ParameterLayout TapeBoxProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;

    auto knob = [&] (const char* id, const char* name, float def)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                      NormalisableRange<float> (0.f, 10.f, 0.01f), def));
    };
    knob ("drive", "Drive", 3.f);
    knob ("color", "Color", 4.f);
    knob ("wow", "Wow Flutter", 2.f);
    knob ("noise", "Noise", 2.f);
    knob ("tone", "Tone", 5.f);
    knob ("output", "Output", 7.f);

    auto sw = [&] (const char* id, const char* name, StringArray choices, int def)
    {
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, def));
    };
    sw ("age", "Age", { "New", "Used", "Worn" }, 1);
    sw ("tape", "Tape", { "Low", "Std", "High" }, 1);
    sw ("comp", "Comp", { "Off", "Lite", "Heavy" }, 0);

    return l;
}

bool TapeBoxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void TapeBoxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    dsp::ProcessSpec spec { sampleRate, (uint32) samplesPerBlock, 2 };

    baseDelaySamples = (float) (kBaseDelaySec * sampleRate);
    delay.setMaximumDelayInSamples ((int) (0.02 * sampleRate));
    delay.prepare (spec);
    delay.reset();

    for (auto* f : { &lpA, &lpB, &bump, &toneLP })
    {
        f->prepare (spec);
        f->reset();
        f->setResonance (0.7071f);
        f->setType (dsp::StateVariableTPTFilterType::lowpass);
    }
    bump.setCutoffFrequency (90.f);
    toneLP.setCutoffFrequency (1000.f);

    driveSm.reset (sampleRate, 0.02);
    outSm.reset (sampleRate, 0.02);
    firstBlock = true;

    wowPhase = flutPhase = drift = env = 0.f;
    for (int c = 0; c < 2; ++c) dcX[c] = dcY[c] = nLP[c] = 0.f;

    setLatencySamples ((int) std::round (baseDelaySamples));
}

void TapeBoxProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int nCh = jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (nCh == 0) return;

    // ---------- parametros do bloco ----------
    const float drive  = pDrive->load();
    const float color  = pColor->load();
    const float wow    = pWow->load();
    const float noise  = pNoise->load();
    const float tone   = pTone->load();
    const float output = pOutput->load();
    const int age   = jlimit (0, 2, (int) pAge->load());
    const int tape  = jlimit (0, 2, (int) pTape->load());
    const int comp  = jlimit (0, 2, (int) pComp->load());

    static const float lpMult[3]    = { 1.f, 0.7f, 0.45f };   // perda de agudos por desgaste
    static const float wowMult[3]   = { 0.5f, 1.f, 1.6f };    // instabilidade
    static const float noiseMult[3] = { 0.6f, 1.f, 1.8f };    // hiss
    static const float tapeCut[3]   = { 8000.f, 12000.f, 17000.f };
    static const float tapeSat[3]   = { 1.5f, 1.f, 0.7f };
    static const float tapeNoise[3] = { 1.3f, 1.f, 0.7f };

    const float driveDb = drive * 2.4f;                       // 0..+24 dB
    const float makeupDb = -driveDb * 0.45f;                  // compensacao automatica
    const float satScale = tapeSat[tape];
    const float outLin = output <= 0.01f ? 0.f : square (output / 7.f);
    driveSm.setTargetValue (Decibels::decibelsToGain (driveDb));
    outSm.setTargetValue (outLin * Decibels::decibelsToGain (makeupDb));
    if (firstBlock)
    {
        driveSm.setCurrentAndTargetValue (Decibels::decibelsToGain (driveDb));
        outSm.setCurrentAndTargetValue (outLin * Decibels::decibelsToGain (makeupDb));
        firstBlock = false;
    }

    const float cutoff = jmin (tapeCut[tape] * lpMult[age], 0.45f * (float) sr);
    lpA.setCutoffFrequency (cutoff);
    lpB.setCutoffFrequency (cutoff);

    const float bias = (color / 10.f) * 0.35f;
    const float biasT = std::tanh (bias);
    const float bumpGain = (color / 10.f) * 0.7f;

    const float wAmt = (wow / 10.f) * wowMult[age];
    const float wowDepthSec = wAmt * 0.0012f;
    const float flutDepthSec = wAmt * 0.00025f;
    const float driftDepthSec = wAmt * 0.0004f;
    const float wowInc = MathConstants<float>::twoPi * 0.8f / (float) sr;
    const float flutInc = MathConstants<float>::twoPi * 7.3f / (float) sr;

    const float nAmp = square (noise / 10.f) * 0.03f * noiseMult[age] * tapeNoise[tape];

    const float t = (tone - 5.f) / 5.f;
    const float gLow = Decibels::decibelsToGain (-t * 6.f);
    const float gHigh = Decibels::decibelsToGain (t * 6.f);

    const bool compOn = comp > 0;
    const float thr = comp == 1 ? -18.f : -28.f;
    const float ratio = comp == 1 ? 2.f : 6.f;
    const float atkMs = comp == 1 ? 10.f : 3.f;
    const float relMs = comp == 1 ? 150.f : 100.f;
    const float compMakeup = Decibels::decibelsToGain (comp == 1 ? 3.f : 8.f);
    const float atk = std::exp (-1.f / ((float) sr * atkMs * 0.001f));
    const float rel = std::exp (-1.f / ((float) sr * relMs * 0.001f));

    float* ptr[2] = { buffer.getWritePointer (0), nCh > 1 ? buffer.getWritePointer (1) : nullptr };
    float peak[2] = { 0.f, 0.f };

    for (int i = 0; i < n; ++i)
    {
        const float dg = driveSm.getNextValue();
        const float og = outSm.getNextValue();

        // wow (lento) + flutter (rapido) + drift aleatorio, compartilhados entre canais
        wowPhase += wowInc;   if (wowPhase > MathConstants<float>::twoPi)  wowPhase -= MathConstants<float>::twoPi;
        flutPhase += flutInc; if (flutPhase > MathConstants<float>::twoPi) flutPhase -= MathConstants<float>::twoPi;
        drift = drift * 0.9999f + (rnd.nextFloat() * 2.f - 1.f) * 0.002f;

        const float modSec = wowDepthSec * std::sin (wowPhase)
                           + flutDepthSec * std::sin (flutPhase)
                           + driftDepthSec * drift;
        const float delaySamples = jmax (1.f, baseDelaySamples + modSec * (float) sr);

        float s[2] = { 0.f, 0.f };
        for (int ch = 0; ch < nCh; ++ch)
        {
            float x = ptr[ch][i] * dg * satScale;
            float y = (std::tanh (x + bias) - biasT) / satScale;      // saturacao assimetrica
            y += bump.processSample (ch, y) * bumpGain;               // "head bump" nos graves
            delay.pushSample (ch, y);
            y = delay.popSample (ch, delaySamples);                   // wow/flutter
            y = lpB.processSample (ch, lpA.processSample (ch, y));    // perda de agudos da fita
            s[ch] = y;
        }

        float g = 1.f;
        if (compOn)
        {
            const float det = jmax (std::abs (s[0]), nCh > 1 ? std::abs (s[1]) : 0.f);
            const float c = det > env ? atk : rel;
            env = c * env + (1.f - c) * det;
            const float over = Decibels::gainToDecibels (env, -100.f) - thr;
            if (over > 0.f)
                g = Decibels::decibelsToGain (-over * (1.f - 1.f / ratio));
            g *= compMakeup;
        }

        for (int ch = 0; ch < nCh; ++ch)
        {
            float y = s[ch] * g;

            // hiss de fita
            const float w = rnd.nextFloat() * 2.f - 1.f;
            nLP[ch] += 0.15f * (w - nLP[ch]);
            y += (w - nLP[ch] * 0.8f) * nAmp;

            // tone (tilt)
            const float lp = toneLP.processSample (ch, y);
            y = lp * gLow + (y - lp) * gHigh;

            // bloqueio de DC
            const float d = y - dcX[ch] + 0.995f * dcY[ch];
            dcX[ch] = y;
            dcY[ch] = d;
            y = d * og;

            ptr[ch][i] = y;
            peak[ch] = jmax (peak[ch], std::abs (y));
        }
    }

    meterL.store (jmax (peak[0], meterL.load() * 0.85f));
    meterR.store (jmax (nCh > 1 ? peak[1] : peak[0], meterR.load() * 0.85f));
}

AudioProcessorEditor* TapeBoxProcessor::createEditor() { return new TapeBoxEditor (*this); }

void TapeBoxProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TapeBoxProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TapeBoxProcessor(); }
