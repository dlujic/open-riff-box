#include "AmpSimSilver.h"
#include "BinaryData.h"

#include <cmath>

static const char* cabinetNames[AmpSimSilver::kNumCabinets] = {
    "Studio 57",
    "In Your Face",
    "Tight Box",
    "Crystal 412",
    "British Bite",
    "Velvet Blues",
    "Bass Colossus",
    "Acoustic Body",
    "Ambient Hall",
    "Blackface Clean",
    "Plexi Roar",
    "Thunder Box",
    "Tweed Gold",
    "Vox Chime"
};

const char* AmpSimSilver::getCabinetName(int index)
{
    if (index >= 0 && index < kNumCabinets)
        return cabinetNames[index];
    if (index == kNoCabinet)
        return "No Cabinet";
    if (index == kCustomCabinet)
        return "Custom IR";
    return "";
}

void AmpSimSilver::getIRData(int index, const void*& data, int& dataSize)
{
    switch (index)
    {
        case 0:  data = BinaryData::Studio_57_wav;      dataSize = BinaryData::Studio_57_wavSize;      break;
        case 1:  data = BinaryData::In_Your_Face_wav;    dataSize = BinaryData::In_Your_Face_wavSize;    break;
        case 2:  data = BinaryData::Tight_Box_wav;       dataSize = BinaryData::Tight_Box_wavSize;       break;
        case 3:  data = BinaryData::Crystal_412_wav;     dataSize = BinaryData::Crystal_412_wavSize;     break;
        case 4:  data = BinaryData::British_Bite_wav;    dataSize = BinaryData::British_Bite_wavSize;    break;
        case 5:  data = BinaryData::Velvet_Blues_wav;      dataSize = BinaryData::Velvet_Blues_wavSize;      break;
        case 6:  data = BinaryData::Bass_Colossus_wav;   dataSize = BinaryData::Bass_Colossus_wavSize;   break;
        case 7:  data = BinaryData::Acoustic_Body_wav;   dataSize = BinaryData::Acoustic_Body_wavSize;   break;
        case 8:  data = BinaryData::Ambient_Hall_wav;    dataSize = BinaryData::Ambient_Hall_wavSize;    break;
        case 9:  data = BinaryData::Blackface_Clean_wav; dataSize = BinaryData::Blackface_Clean_wavSize; break;
        case 10: data = BinaryData::Plexi_Roar_wav;      dataSize = BinaryData::Plexi_Roar_wavSize;      break;
        case 11: data = BinaryData::Thunder_Box_wav;     dataSize = BinaryData::Thunder_Box_wavSize;     break;
        case 12: data = BinaryData::Tweed_Gold_wav;      dataSize = BinaryData::Tweed_Gold_wavSize;      break;
        case 13: data = BinaryData::Vox_Chime_wav;       dataSize = BinaryData::Vox_Chime_wavSize;       break;
        default: data = BinaryData::Studio_57_wav;       dataSize = BinaryData::Studio_57_wavSize;       break;
    }
}

AmpSimSilver::AmpSimSilver()
{
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
}

void AmpSimSilver::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    oversampling->initProcessing(static_cast<size_t>(samplesPerBlock));
    oversampling->reset();

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels      = 2;

    bassFilter.prepare(spec);
    midFilter.prepare(spec);
    trebleFilter.prepare(spec);
    updateEQFilters();

    preampLPF.prepare(spec);
    updatePreampLPF();

    *dcBlocker.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(sampleRate, 7.0f);
    dcBlocker.prepare(spec);

    presenceFilter.prepare(spec);
    updatePresenceFilter();

    cabinetConvolution.prepare(spec);
    cabMakeupGain.reset(sampleRate, 0.2);

    lastCabinetType = -1;
    if (cabinetTypeParam.load(std::memory_order_acquire) == kCustomCabinet
        && customIRFile.existsAsFile())
    {
        cabinetConvolution.loadImpulseResponse(
            customIRFile,
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::yes, 0);
        lastCabinetType = kCustomCabinet;
    }
    else
    {
        updateCabinet();
    }
    updateCabGainTarget();
    cabMakeupGain.setCurrentAndTargetValue(cabMakeupGain.getTargetValue());

    brightnessFilter.prepare(spec);
    micPositionFilter.prepare(spec);
    updateBrightnessFilter();
    updateMicPositionFilter();

    gainSmoothed.reset(sampleRate, 0.02);
    speakerDriveSmoothed.reset(sampleRate, 0.02);

    float initGain = 1.0f + 49.0f * gainParam * gainParam;
    gainSmoothed.setCurrentAndTargetValue(initGain);
    speakerDriveSmoothed.setCurrentAndTargetValue(speakerDriveParam);
}

void AmpSimSilver::reset()
{
    oversampling->reset();
    bassFilter.reset();
    midFilter.reset();
    trebleFilter.reset();
    preampLPF.reset();
    dcBlocker.reset();
    presenceFilter.reset();
    cabinetConvolution.reset();
    cabMakeupGain.reset(currentSampleRate, 0.2);
    brightnessFilter.reset();
    micPositionFilter.reset();
    gainSmoothed.reset(currentSampleRate, 0.02);
    speakerDriveSmoothed.reset(currentSampleRate, 0.02);

    lastCabinetType = -1;
}

int AmpSimSilver::getLatencySamples() const
{
    int latency = static_cast<int>(oversampling->getLatencyInSamples());
    if (cabinetTypeParam.load(std::memory_order_acquire) != kNoCabinet)
        latency += static_cast<int>(cabinetConvolution.getLatency());
    return latency;
}

void AmpSimSilver::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    updateEQFilters();
    updateCabinet();
    updateBrightnessFilter();
    updateMicPositionFilter();

    gainSmoothed.setTargetValue(1.0f + 49.0f * gainParam * gainParam);
    speakerDriveSmoothed.setTargetValue(speakerDriveParam);

    //--------------------------------------------------------------------------
    // 1. Amp EQ (Bass / Mid / Treble shelves)
    //--------------------------------------------------------------------------
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        bassFilter.process(context);
        midFilter.process(context);
        trebleFilter.process(context);
    }

    //--------------------------------------------------------------------------
    // 2. Pre-clip HF rolloff
    //--------------------------------------------------------------------------
    updatePreampLPF();
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        preampLPF.process(context);
    }

    //--------------------------------------------------------------------------
    // 3. Power amp
    //--------------------------------------------------------------------------
    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = gainSmoothed.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer(ch)[i] *= gain;
    }

    {
        juce::dsp::AudioBlock<float> block(buffer);
        auto oversampledBlock = oversampling->processSamplesUp(block);

        const auto osNumSamples  = static_cast<int>(oversampledBlock.getNumSamples());
        const auto osNumChannels = static_cast<int>(oversampledBlock.getNumChannels());
        const bool boost = preampBoostParam.load(std::memory_order_acquire);

        for (int ch = 0; ch < osNumChannels; ++ch)
        {
            auto* samples = oversampledBlock.getChannelPointer(static_cast<size_t>(ch));
            for (int i = 0; i < osNumSamples; ++i)
            {
                float x = samples[i];

                if (boost)
                    x = std::tanh(x * 3.981f);

                samples[i] = (x >= 0.0f) ? std::tanh(x) : std::tanh(x * 1.2f) / 1.2f;
            }
        }

        oversampling->processSamplesDown(block);
    }

    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        dcBlocker.process(context);
    }

    //--------------------------------------------------------------------------
    // 4. Speaker distortion
    //--------------------------------------------------------------------------
    for (int i = 0; i < numSamples; ++i)
    {
        const float drive = speakerDriveSmoothed.getNextValue();
        const float amt = drive * 0.4f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float x = buffer.getWritePointer(ch)[i];
            buffer.getWritePointer(ch)[i] = x + amt * (std::tanh(2.0f * x) - x);
        }
    }

    //--------------------------------------------------------------------------
    // 4b. Presence peak
    //--------------------------------------------------------------------------
    updatePresenceFilter();
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        presenceFilter.process(context);
    }

    //--------------------------------------------------------------------------
    // 5. Cabinet IR
    //--------------------------------------------------------------------------
    if (cabinetTypeParam.load(std::memory_order_acquire) != kNoCabinet)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        cabinetConvolution.process(context);
    }

    // Cabinet makeup gain
    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = cabMakeupGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer(ch)[i] *= gain;
    }

    //--------------------------------------------------------------------------
    // 6. Brightness
    //--------------------------------------------------------------------------
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        brightnessFilter.process(context);
    }

    //--------------------------------------------------------------------------
    // 7. Mic position
    //--------------------------------------------------------------------------
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        micPositionFilter.process(context);
    }
}

void AmpSimSilver::resetToDefaults()
{
    setGain(0.3f);
    setBass(0.5f);
    setMid(0.5f);
    setTreble(0.5f);
    setPreampBoost(false);
    setSpeakerDrive(0.2f);
    setCabinetType(0);
    setBrightness(0.5f);
    setMicPosition(0.3f);
    setCabTrim(0.0f);
}

void AmpSimSilver::setGain(float value)         { gainParam = juce::jlimit(0.0f, 1.0f, value); }
void AmpSimSilver::setBass(float value)         { bassParam = juce::jlimit(0.0f, 1.0f, value); }
void AmpSimSilver::setMid(float value)          { midParam = juce::jlimit(0.0f, 1.0f, value); }
void AmpSimSilver::setTreble(float value)       { trebleParam = juce::jlimit(0.0f, 1.0f, value); }
void AmpSimSilver::setPreampBoost(bool on)      { preampBoostParam.store(on, std::memory_order_release); }
void AmpSimSilver::setSpeakerDrive(float value) { speakerDriveParam = juce::jlimit(0.0f, 1.0f, value); }
void AmpSimSilver::setCabinetType(int type)     { cabinetTypeParam.store(juce::jlimit(0, kCustomCabinet, type), std::memory_order_release); }

void AmpSimSilver::loadCustomIR(const juce::File& irFile)
{
    if (!irFile.existsAsFile()) return;

    customIRFile = irFile;

    cabinetConvolution.loadImpulseResponse(
        irFile,
        juce::dsp::Convolution::Stereo::no,
        juce::dsp::Convolution::Trim::yes, 0);

    cabinetTypeParam.store(kCustomCabinet, std::memory_order_release);
    lastCabinetType = kCustomCabinet;
}
void AmpSimSilver::setBrightness(float value)   { brightnessParam = juce::jlimit(0.0f, 1.0f, value); }
void AmpSimSilver::setMicPosition(float value)  { micPositionParam = juce::jlimit(0.0f, 1.0f, value); }

void AmpSimSilver::setCabTrim(float dB)
{
    cabTrimDb = juce::jlimit(-12.0f, 12.0f, dB);
    updateCabGainTarget();
}

void AmpSimSilver::updateCabGainTarget()
{
    float trimGain = juce::Decibels::decibelsToGain(cabTrimDb);

    if (cabinetTypeParam.load(std::memory_order_acquire) == kNoCabinet)
        trimGain *= juce::Decibels::decibelsToGain(-8.0f);

    cabMakeupGain.setTargetValue(trimGain);
}

void AmpSimSilver::updateEQFilters()
{
    const auto sr = currentSampleRate;

    float bassDb = (bassParam - 0.5f) * 24.0f;
    *bassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        sr, 150.0f, 0.707f, juce::Decibels::decibelsToGain(bassDb));

    float midDb = (midParam - 0.5f) * 24.0f;
    *midFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sr, 800.0f, 0.8f, juce::Decibels::decibelsToGain(midDb));

    float trebleDb = (trebleParam - 0.5f) * 24.0f;
    *trebleFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        sr, 3000.0f, 0.707f, juce::Decibels::decibelsToGain(trebleDb));
}

void AmpSimSilver::updatePreampLPF()
{
    float freq = 8000.0f - 4000.0f * gainParam;
    *preampLPF.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(
        currentSampleRate, freq, 0.707f);
}

void AmpSimSilver::updatePresenceFilter()
{
    constexpr float centerFreq = 2200.0f;
    constexpr float q = 0.858f;
    constexpr float maxBoostDb = 3.5f;

    float boostDb = (gainParam < 0.5f)
        ? 4.0f * gainParam * gainParam + 2.0f
        : 4.0f * gainParam + 1.0f;
    boostDb = juce::jmin(boostDb, maxBoostDb);

    *presenceFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        currentSampleRate, centerFreq, q, juce::Decibels::decibelsToGain(boostDb));
}

void AmpSimSilver::updateCabinet()
{
    int currentType = cabinetTypeParam.load(std::memory_order_acquire);
    if (currentType == lastCabinetType)
        return;

    lastCabinetType = currentType;

    updateCabGainTarget();

    if (currentType == kNoCabinet || currentType == kCustomCabinet)
        return;

    const void* data = nullptr;
    int dataSize = 0;
    getIRData(currentType, data, dataSize);

    cabinetConvolution.loadImpulseResponse(
        data, static_cast<size_t>(dataSize),
        juce::dsp::Convolution::Stereo::no,
        juce::dsp::Convolution::Trim::yes,
        0
    );
}

void AmpSimSilver::updateBrightnessFilter()
{
    float db = -6.0f + 12.0f * brightnessParam;
    *brightnessFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        currentSampleRate, 3500.0f, 0.707f, juce::Decibels::decibelsToGain(db));
}

void AmpSimSilver::updateMicPositionFilter()
{
    float db = -4.0f + 8.0f * micPositionParam;
    *micPositionFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        currentSampleRate, 1500.0f, 0.707f, juce::Decibels::decibelsToGain(db));
}
