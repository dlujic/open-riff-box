#include "AmpSimGold.h"
#include "BinaryData.h"

#include <cmath>

// Cabinet IR names
static const char* cabinetNames2[AmpSimGold::kNumCabinets] = {
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

const char* AmpSimGold::getCabinetName(int index)
{
    if (index >= 0 && index < kNumCabinets)
        return cabinetNames2[index];
    if (index == kNoCabinet)
        return "No Cabinet";
    if (index == kCustomCabinet)
        return "Custom IR";
    return "";
}

void AmpSimGold::getIRData(int index, const void*& data, int& dataSize)
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

//==============================================================================
AmpSimGold::AmpSimGold()
{
    setBypassed(true);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);

    auto initPreamp = [](WaveshaperEngine& e) {
        e.setCoefficients(WaveshaperEngine::CoefficientPreset::SignFoldChebyshev);
        e.setSignFold(true);
    };
    initPreamp(preampL);       initPreamp(preampR);

    auto initCharacter = [](WaveshaperEngine& e) {
        e.setCoefficients(WaveshaperEngine::CoefficientPreset::SignFoldCharacter);
        e.setSignFold(true);
    };
    initCharacter(characterL);    initCharacter(characterR);

    auto initPowerAmp = [](WaveshaperEngine& e) {
        e.setCoefficients(WaveshaperEngine::CoefficientPreset::SignFoldPowerAmp);
        e.setSignFold(true);
    };
    initPowerAmp(powerAmpPosL);  initPowerAmp(powerAmpPosR);
    initPowerAmp(powerAmpNegL);  initPowerAmp(powerAmpNegR);
}

//==============================================================================
void AmpSimGold::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    oversampledRate = sampleRate * 4.0;

    oversampling->initProcessing(static_cast<size_t>(samplesPerBlock));
    oversampling->reset();

    const int osBlockSize = samplesPerBlock * 4;

    preampL.prepareRaw(oversampledRate, osBlockSize);
    preampR.prepareRaw(oversampledRate, osBlockSize);
    characterL.prepareRaw(oversampledRate, osBlockSize);
    characterR.prepareRaw(oversampledRate, osBlockSize);
    powerAmpPosL.prepareRaw(oversampledRate, osBlockSize);
    powerAmpPosR.prepareRaw(oversampledRate, osBlockSize);
    powerAmpNegL.prepareRaw(oversampledRate, osBlockSize);
    powerAmpNegR.prepareRaw(oversampledRate, osBlockSize);

    preampL.setFeedbackDelayLength(3);
    preampR.setFeedbackDelayLength(3);
    characterL.setFeedbackDelayLength(3);
    characterR.setFeedbackDelayLength(3);

    configurePreampEngines();
    configureCharacterEngines();
    configurePowerAmpEngines();

    couplingCapR = 1.0f - (2.0f * juce::MathConstants<float>::pi * 20.0f
                           / static_cast<float>(oversampledRate));
    couplingCap1L.reset(); couplingCap1R.reset();
    couplingCap2L.reset(); couplingCap2R.reset();

    dcBlockerR_coeff = 1.0f - (2.0f * juce::MathConstants<float>::pi * 7.0f
                                / static_cast<float>(oversampledRate));
    dcBlockerL.reset(); dcBlockerR.reset();

    juce::dsp::ProcessSpec monoOsSpec;
    monoOsSpec.sampleRate = oversampledRate;
    monoOsSpec.maximumBlockSize = static_cast<juce::uint32>(osBlockSize);
    monoOsSpec.numChannels = 1;

    toneLpfL.prepare(monoOsSpec);   toneLpfR.prepare(monoOsSpec);
    toneMidScoopL.prepare(monoOsSpec); toneMidScoopR.prepare(monoOsSpec);

    xfmrHpfL.prepare(monoOsSpec);  xfmrHpfR.prepare(monoOsSpec);
    xfmrLpfL.prepare(monoOsSpec);  xfmrLpfR.prepare(monoOsSpec);
    *xfmrHpfL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(oversampledRate, 70.0f, 0.707f);
    *xfmrHpfR.coefficients = *xfmrHpfL.coefficients;
    *xfmrLpfL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(oversampledRate, 7500.0f);
    *xfmrLpfR.coefficients = *xfmrLpfL.coefficients;
	
    lastGainParam = -1.0f;
    lastBassParam = -1.0f;
    lastMidParam = -1.0f;
    lastTrebleParam = -1.0f;
    lastSpeakerDriveParam = -1.0f;
    lastBrightnessParam = -1.0f;
    lastMicPositionParam = -1.0f;

    updateToneShapingFilters(gainParam);
    updateToneStackCoeffs();

    powerAmpTempBuffer.setSize(1, osBlockSize);

    nfbStateL = 0.0f;
    nfbStateR = 0.0f;
    nfbFilteredStateL = 0.0f;
    nfbFilteredStateR = 0.0f;
    lastPresenceParam = -1.0f;
    updatePresenceNfbCoeff();

    sagAttackCoeff  = 1.0f - std::exp(-1.0f / (static_cast<float>(oversampledRate) * 0.020f));
    sagReleaseCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(oversampledRate) * 0.200f));
    sagEnvelopeL = 0.0f;
    sagEnvelopeR = 0.0f;

    juce::dsp::ProcessSpec baseSpec;
    baseSpec.sampleRate = sampleRate;
    baseSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    baseSpec.numChannels = 2;

    cabinetConvolution.prepare(baseSpec);

    cabMakeupGain.reset(sampleRate, 0.2);

    lastCabinetType = -1;  // force update
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

    brightnessFilter.prepare(baseSpec);
    micPositionFilter.prepare(baseSpec);
    updateBrightnessFilter();
    updateMicPositionFilter();
}

//==============================================================================
void AmpSimGold::reset()
{
    oversampling->reset();

    preampL.reset();     preampR.reset();
    characterL.reset();  characterR.reset();
    powerAmpPosL.reset(); powerAmpPosR.reset();
    powerAmpNegL.reset(); powerAmpNegR.reset();

    couplingCap1L.reset(); couplingCap1R.reset();
    couplingCap2L.reset(); couplingCap2R.reset();
    dcBlockerL.reset();    dcBlockerR.reset();

    toneLpfL.reset();      toneLpfR.reset();
    toneMidScoopL.reset(); toneMidScoopR.reset();
    toneStackL.reset();    toneStackR.reset();
    xfmrHpfL.reset();      xfmrHpfR.reset();
    xfmrLpfL.reset();      xfmrLpfR.reset();

    cabinetConvolution.reset();
    cabMakeupGain.reset(currentSampleRate, 0.2);
    brightnessFilter.reset();
    micPositionFilter.reset();

    nfbStateL = 0.0f;
    nfbStateR = 0.0f;
    nfbFilteredStateL = 0.0f;
    nfbFilteredStateR = 0.0f;
    sagEnvelopeL = 0.0f;
    sagEnvelopeR = 0.0f;

    lastCabinetType = -1;
}

//==============================================================================
int AmpSimGold::getLatencySamples() const
{
    int latency = static_cast<int>(oversampling->getLatencyInSamples());
    if (cabinetTypeParam.load(std::memory_order_acquire) != kNoCabinet)
        latency += static_cast<int>(cabinetConvolution.getLatency());
    return latency;
}

//==============================================================================
void AmpSimGold::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    const float currentGain = gainParam;
    const float currentSpeakerDrive = speakerDriveParam;
    const bool  currentBoost = preampBoostParam.load(std::memory_order_acquire);

    const bool gainChanged   = (currentGain != lastGainParam || currentBoost != lastPreampBoost);
    const bool toneChanged   = (bassParam != lastBassParam || midParam != lastMidParam
                                || trebleParam != lastTrebleParam);
    const bool driveChanged  = (currentSpeakerDrive != lastSpeakerDriveParam);
    const bool brightChanged = (brightnessParam != lastBrightnessParam);
    const bool micChanged    = (micPositionParam != lastMicPositionParam);

    if (gainChanged)
    {
        updateToneShapingFilters(currentGain);
        configurePreampEngines();
        lastGainParam = currentGain;
        lastPreampBoost = currentBoost;
    }
    if (toneChanged)
    {
        updateToneStackCoeffs();
        lastBassParam = bassParam;
        lastMidParam = midParam;
        lastTrebleParam = trebleParam;
    }
    if (driveChanged)
    {
        configurePowerAmpEngines();
        lastSpeakerDriveParam = currentSpeakerDrive;
    }
    if (brightChanged)
    {
        updateBrightnessFilter();
        lastBrightnessParam = brightnessParam;
    }
    if (micChanged)
    {
        updateMicPositionFilter();
        lastMicPositionParam = micPositionParam;
    }
    if (presenceParam != lastPresenceParam)
    {
        updatePresenceNfbCoeff();
        lastPresenceParam = presenceParam;
    }

    updateCabinet();

    //--------------------------------------------------------------------------
    // Upsample stereo buffer (4x)
    //--------------------------------------------------------------------------
    juce::dsp::AudioBlock<float> block(buffer);
    auto oversampledBlock = oversampling->processSamplesUp(block);

    const auto osNumSamples = static_cast<int>(oversampledBlock.getNumSamples());
    const auto osNumChannels = juce::jmin(static_cast<int>(oversampledBlock.getNumChannels()), 2);

    const float levelCompDb = 2.0f * currentGain;
    const float levelCompGain = juce::Decibels::decibelsToGain(levelCompDb);

    //--------------------------------------------------------------------------
    // Process each channel at oversampled rate
    //--------------------------------------------------------------------------
    for (int ch = 0; ch < osNumChannels; ++ch)
    {
        auto* samples = oversampledBlock.getChannelPointer(static_cast<size_t>(ch));

        auto& preampEngine     = (ch == 0) ? preampL     : preampR;
        auto& characterEngine  = (ch == 0) ? characterL  : characterR;
        auto& coupCap1         = (ch == 0) ? couplingCap1L : couplingCap1R;
        auto& coupCap2         = (ch == 0) ? couplingCap2L : couplingCap2R;
        auto& toneLpf          = (ch == 0) ? toneLpfL     : toneLpfR;
        auto& toneMidScoop     = (ch == 0) ? toneMidScoopL : toneMidScoopR;
        auto& toneStack        = (ch == 0) ? toneStackL   : toneStackR;
        auto& sagEnvelope      = (ch == 0) ? sagEnvelopeL : sagEnvelopeR;
        auto& nfbState         = (ch == 0) ? nfbStateL    : nfbStateR;
        auto& dcBlocker        = (ch == 0) ? dcBlockerL   : dcBlockerR;

        //----------------------------------------------------------------------
        // Stage 1: Preamp WaveshaperEngine
        //----------------------------------------------------------------------
        preampEngine.processRaw(samples, osNumSamples);

        //----------------------------------------------------------------------
        // Coupling cap HPF after preamp
        //----------------------------------------------------------------------
        for (int i = 0; i < osNumSamples; ++i)
            samples[i] = coupCap1.processSample(samples[i], couplingCapR);

        //----------------------------------------------------------------------
        // Stage 2: Gain-dependent tone shaping (per-sample)
        //   - Auto-darkening LPF
        //   - Parametric mid scoop
        //   - Level compensation
        //----------------------------------------------------------------------
        for (int i = 0; i < osNumSamples; ++i)
        {
            float s = samples[i];
            s = toneLpf.processSample(s);
            s = toneMidScoop.processSample(s);
            s *= levelCompGain;
            samples[i] = s;
        }

        //----------------------------------------------------------------------
        // Stage 3: Character WaveshaperEngine
        //----------------------------------------------------------------------
        characterEngine.processRaw(samples, osNumSamples);

        //----------------------------------------------------------------------
        // Coupling cap HPF after character
        //----------------------------------------------------------------------
        for (int i = 0; i < osNumSamples; ++i)
            samples[i] = coupCap2.processSample(samples[i], couplingCapR);

        //----------------------------------------------------------------------
        // Stage 4: Power supply sag (Thevenin model)
        //----------------------------------------------------------------------
        {
            float sagAmount = 0.35f * currentSpeakerDrive;
            for (int i = 0; i < osNumSamples; ++i)
            {
                float envelope = std::abs(samples[i]);
                float coeff = (envelope > sagEnvelope) ? sagAttackCoeff : sagReleaseCoeff;
                sagEnvelope += coeff * (envelope - sagEnvelope);
                float sagDepth = sagAmount * sagEnvelope;
                float gainReduction = 1.0f - juce::jlimit(0.0f, 0.5f, sagDepth);
                samples[i] *= gainReduction;
            }
        }

        //----------------------------------------------------------------------
        // Stage 5: Tone stack (circuit-derived 3rd-order IIR)
        //----------------------------------------------------------------------
        for (int i = 0; i < osNumSamples; ++i)
            samples[i] = toneStack.processSample(samples[i]) * 3.16f;

        //----------------------------------------------------------------------
        // Stage 6: Push-pull power amp with NFB + Output transformer
        //----------------------------------------------------------------------
        {
            const float nfbGain = internalNfbGain;
            auto& powerPos    = (ch == 0) ? powerAmpPosL : powerAmpPosR;
            auto& powerNeg    = (ch == 0) ? powerAmpNegL : powerAmpNegR;
            auto& xfmrHpf     = (ch == 0) ? xfmrHpfL : xfmrHpfR;
            auto& xfmrLpf     = (ch == 0) ? xfmrLpfL : xfmrLpfR;
            auto& nfbFiltered = (ch == 0) ? nfbFilteredStateL : nfbFilteredStateR;
            float satDrive = 0.8f + 0.8f * currentSpeakerDrive;
            float posSat = satDrive * 1.2f;
            float negSat = satDrive * 1.0f;

            for (int i = 0; i < osNumSamples; ++i)
            {
                float toneOut = samples[i];

                float ppInput = toneOut - nfbGain * nfbFiltered;

                float posOut = powerPos.processSample(ppInput);
                float negOut = powerNeg.processSample(-ppInput);

                float rawOut = posOut - negOut;

                nfbState = rawOut;
                nfbFiltered += nfbLpfCoeff * (rawOut - nfbFiltered);

                rawOut = xfmrHpf.processSample(rawOut);
                rawOut = xfmrLpf.processSample(rawOut);
                if (rawOut >= 0.0f)
                    rawOut = std::tanh(rawOut * posSat);
                else
                    rawOut = std::tanh(rawOut * negSat);

                samples[i] = rawOut;
            }
        }

        //----------------------------------------------------------------------
        // DC blocker (7 Hz HPF, per-sample)
        //----------------------------------------------------------------------
        for (int i = 0; i < osNumSamples; ++i)
            samples[i] = dcBlocker.processSample(samples[i], dcBlockerR_coeff);
    }

    //--------------------------------------------------------------------------
    // Downsample back to base rate
    //--------------------------------------------------------------------------
    oversampling->processSamplesDown(block);

    //--------------------------------------------------------------------------
    // Stage 7: Cabinet IR convolution (base rate, stereo) - skipped for No Cabinet
    //--------------------------------------------------------------------------
    if (cabinetTypeParam.load(std::memory_order_acquire) != kNoCabinet)
    {
        juce::dsp::AudioBlock<float> cabBlock(buffer);
        juce::dsp::ProcessContextReplacing<float> context(cabBlock);
        cabinetConvolution.process(context);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = cabMakeupGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer(ch)[i] *= gain;
    }

    //--------------------------------------------------------------------------
    // Stage 8: Brightness LPF (base rate, stereo)
    //--------------------------------------------------------------------------
    {
        juce::dsp::AudioBlock<float> bBlock(buffer);
        juce::dsp::ProcessContextReplacing<float> context(bBlock);
        brightnessFilter.process(context);
    }

    //--------------------------------------------------------------------------
    // Stage 9: Mic position high shelf (base rate, stereo)
    //--------------------------------------------------------------------------
    {
        juce::dsp::AudioBlock<float> mBlock(buffer);
        juce::dsp::ProcessContextReplacing<float> context(mBlock);
        micPositionFilter.process(context);
    }
}

//==============================================================================
void AmpSimGold::resetToDefaults()
{
    setGain(0.4f);
    setBass(0.5f);
    setMid(0.6f);
    setTreble(0.5f);
    setPreampBoost(false);
    setSpeakerDrive(0.2f);
    setCabinetType(0);
    setBrightness(0.6f);
    setMicPosition(0.5f);
    setPresence(0.70f);
    setCabTrim(0.0f);
}

//==============================================================================
// Parameter setters
//==============================================================================
void AmpSimGold::setGain(float value)         { gainParam = juce::jlimit(0.0f, 1.0f, value); }
void AmpSimGold::setBass(float value)         { bassParam = juce::jlimit(0.0f, 1.0f, value); }
void AmpSimGold::setMid(float value)          { midParam = juce::jlimit(0.0f, 1.0f, value); }
void AmpSimGold::setTreble(float value)       { trebleParam = juce::jlimit(0.0f, 1.0f, value); }
void AmpSimGold::setPreampBoost(bool on)      { preampBoostParam.store(on, std::memory_order_release); }
void AmpSimGold::setSpeakerDrive(float value) { speakerDriveParam = juce::jlimit(0.0f, 1.0f, value); }
void AmpSimGold::setCabinetType(int type)     { cabinetTypeParam.store(juce::jlimit(0, kCustomCabinet, type), std::memory_order_release); }

void AmpSimGold::loadCustomIR(const juce::File& irFile)
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

void AmpSimGold::setBrightness(float value)   { brightnessParam = juce::jlimit(0.0f, 1.0f, value); }
void AmpSimGold::setMicPosition(float value)  { micPositionParam = juce::jlimit(0.0f, 1.0f, value); }
void AmpSimGold::setPresence(float value)
{
    presenceParam = juce::jlimit(0.0f, 1.0f, value);
}

void AmpSimGold::setCabTrim(float dB)
{
    cabTrimDb = juce::jlimit(-12.0f, 12.0f, dB);
    updateCabGainTarget();
}

void AmpSimGold::setInternalCharacterGain(float g)  { internalCharacterGain = juce::jlimit(0.05f, 1.0f, g); }
void AmpSimGold::setInternalDriveExponent(float e)   { internalDriveExponent = juce::jlimit(0.5f, 4.0f, e); }
void AmpSimGold::setInternalNfbGain(float g)         { internalNfbGain = juce::jlimit(0.0f, 0.5f, g); }

void AmpSimGold::updateCabGainTarget()
{
    float trimGain = juce::Decibels::decibelsToGain(cabTrimDb);

    if (cabinetTypeParam.load(std::memory_order_acquire) == kNoCabinet)
        trimGain *= juce::Decibels::decibelsToGain(-8.0f);

    cabMakeupGain.setTargetValue(trimGain);
}

//==============================================================================
// WaveshaperEngine configuration
//==============================================================================

void AmpSimGold::configurePreampEngines()
{
    const float g = gainParam;

    float driveScale = 0.15f + 1.65f * std::pow(g, internalDriveExponent);
    if (preampBoostParam.load(std::memory_order_acquire))
        driveScale *= 1.5f;  // +3.5 dB boost

    preampL.setInputGain(driveScale);
    preampR.setInputGain(driveScale);

    float preShape = g * 0.4f;
    preampL.setPreShapeIntensity(preShape);
    preampR.setPreShapeIntensity(preShape);

    float feedback;
    if (g <= 0.6f)
        feedback = 0.012f;
    else
    {
        float t = (g - 0.6f) / 0.4f;
        feedback = 0.012f + 0.008f * t * t;
    }
    preampL.setFeedbackAmount(feedback);
    preampR.setFeedbackAmount(feedback);

    preampL.setPostFilterLowEnabled(false);
    preampL.setPostFilterMidEnabled(true);
    preampL.setPostFilterMid(12000.0f, 0.38f);
    preampL.setPostFilterHighEnabled(false);
    preampR.setPostFilterLowEnabled(false);
    preampR.setPostFilterMidEnabled(true);
    preampR.setPostFilterMid(12000.0f, 0.38f);
    preampR.setPostFilterHighEnabled(false);

    preampL.setDryWetMix(1.0f);
    preampR.setDryWetMix(1.0f);
}

void AmpSimGold::configureCharacterEngines()
{
    characterL.setInputGain(internalCharacterGain);
    characterR.setInputGain(internalCharacterGain);

    characterL.setPreShapeIntensity(0.2f);
    characterR.setPreShapeIntensity(0.2f);

    characterL.setFeedbackAmount(0.02f);
    characterR.setFeedbackAmount(0.02f);

    characterL.setPostFilterLowEnabled(false);
    characterL.setPostFilterMidEnabled(true);
    characterL.setPostFilterMid(12000.0f, 0.4f);
    characterL.setPostFilterHighEnabled(false);
    characterR.setPostFilterLowEnabled(false);
    characterR.setPostFilterMidEnabled(true);
    characterR.setPostFilterMid(12000.0f, 0.4f);
    characterR.setPostFilterHighEnabled(false);

    characterL.setDryWetMix(1.0f);
    characterR.setDryWetMix(1.0f);
}

void AmpSimGold::configurePowerAmpEngines()
{
    const float sd = speakerDriveParam;
    float driveScale = 0.1f + 0.4f * sd;

    powerAmpPosL.setInputGain(driveScale);
    powerAmpPosR.setInputGain(driveScale);
    powerAmpNegL.setInputGain(driveScale);
    powerAmpNegR.setInputGain(driveScale);

    float powerPreShape = sd * 0.3f;
    powerAmpPosL.setPreShapeIntensity(powerPreShape);
    powerAmpPosR.setPreShapeIntensity(powerPreShape);
    powerAmpNegL.setPreShapeIntensity(powerPreShape);
    powerAmpNegR.setPreShapeIntensity(powerPreShape);

    powerAmpPosL.setFeedbackAmount(0.0f);
    powerAmpPosR.setFeedbackAmount(0.0f);
    powerAmpNegL.setFeedbackAmount(0.0f);
    powerAmpNegR.setFeedbackAmount(0.0f);

    powerAmpPosL.setPostFilterLowEnabled(false);
    powerAmpPosL.setPostFilterMidEnabled(false);
    powerAmpPosL.setPostFilterHighEnabled(false);
    powerAmpPosR.setPostFilterLowEnabled(false);
    powerAmpPosR.setPostFilterMidEnabled(false);
    powerAmpPosR.setPostFilterHighEnabled(false);
    powerAmpNegL.setPostFilterLowEnabled(false);
    powerAmpNegL.setPostFilterMidEnabled(false);
    powerAmpNegL.setPostFilterHighEnabled(false);
    powerAmpNegR.setPostFilterLowEnabled(false);
    powerAmpNegR.setPostFilterMidEnabled(false);
    powerAmpNegR.setPostFilterHighEnabled(false);

    powerAmpPosL.setDryWetMix(1.0f);
    powerAmpPosR.setDryWetMix(1.0f);
    powerAmpNegL.setDryWetMix(1.0f);
    powerAmpNegR.setDryWetMix(1.0f);
}

//==============================================================================
// Filter updates
//==============================================================================

void AmpSimGold::updateToneShapingFilters(float gain01)
{
    float lpfFreq = 13000.0f - 7000.0f * gain01;
    lpfFreq = juce::jlimit(20.0f, static_cast<float>(oversampledRate * 0.49), lpfFreq);

    auto lpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(oversampledRate, lpfFreq, 0.707f);
    *toneLpfL.coefficients = *lpfCoeffs;
    *toneLpfR.coefficients = *lpfCoeffs;

    float midScoopDb = -6.0f * gain01;
    auto midCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        oversampledRate, 1220.0f, 0.8f, juce::Decibels::decibelsToGain(midScoopDb));
    *toneMidScoopL.coefficients = *midCoeffs;
    *toneMidScoopR.coefficients = *midCoeffs;
}

void AmpSimGold::updateToneStackCoeffs()
{
    const double RIN = 1300.0;
    const double R1  = 33000.0;
    const double RT  = 220000.0;
    const double RB  = 1000000.0;
    const double RM  = 25000.0;
    const double RL  = 517000.0;
    const double C1  = 470e-12;
    const double C2  = 22e-9;
    const double C3  = 22e-9;

    double rotTreble = trebleParam * 10.0;
    double rotMid    = midParam    * 10.0;
    double rotBass   = bassParam   * 10.0;

    double rotTrebleTapered = rotTreble;  // Linear
    double rotMidTapered    = rotMid;     // Linear
    double rotBassTapered;
    if (rotBass < 5.0)
        rotBassTapered = 0.2 * rotBass;
    else
        rotBassTapered = rotBass * 1.8 - 8.0;

    double RT2 = RT * (rotTrebleTapered / 10.0);
    double RT1 = RT * (1.0 - rotTrebleTapered / 10.0);
    double RBv = RB * (rotBassTapered / 10.0);
    double RM2 = RM * (rotMidTapered / 10.0);
    double RM1 = RM * (1.0 - rotMidTapered / 10.0);

    double DEN_AIm = RM1*RM2*RT1*RT2*C1*C2*C3 + RBv*RM2*RT1*RT2*C1*C2*C3 + RIN*RM1*RT1*RT2*C1*C2*C3 + R1*RM1*RT1*RT2*C1*C2*C3
     + RBv*RIN*RT1*RT2*C1*C2*C3 + R1*RBv*RT1*RT2*C1*C2*C3 + RL*RM1*RM2*RT2*C1*C2*C3 + RIN*RM1*RM2*RT2*C1*C2*C3
     + RBv*RL*RM2*RT2*C1*C2*C3 + RBv*RIN*RM2*RT2*C1*C2*C3 + RIN*RL*RM1*RT2*C1*C2*C3 + R1*RL*RM1*RT2*C1*C2*C3
     + R1*RIN*RM1*RT2*C1*C2*C3 + RBv*RIN*RL*RT2*C1*C2*C3 + R1*RBv*RL*RT2*C1*C2*C3 + R1*RBv*RIN*RT2*C1*C2*C3
     + RL*RM1*RM2*RT1*C1*C2*C3 + RIN*RM1*RM2*RT1*C1*C2*C3 + R1*RM1*RM2*RT1*C1*C2*C3 + RBv*RL*RM2*RT1*C1*C2*C3
     + RBv*RIN*RM2*RT1*C1*C2*C3 + R1*RBv*RM2*RT1*C1*C2*C3 + RIN*RL*RM1*RT1*C1*C2*C3 + R1*RL*RM1*RT1*C1*C2*C3
     + RBv*RIN*RL*RT1*C1*C2*C3 + R1*RBv*RL*RT1*C1*C2*C3 + R1*RL*RM1*RM2*C1*C2*C3 + R1*RIN*RM1*RM2*C1*C2*C3
     + R1*RBv*RL*RM2*C1*C2*C3 + R1*RBv*RIN*RM2*C1*C2*C3 + R1*RIN*RL*RM1*C1*C2*C3 + R1*RBv*RIN*RL*C1*C2*C3;

    double DEN_BRe = RM2*RT1*RT2*C1*C2 + RM1*RT1*RT2*C1*C2 + RIN*RT1*RT2*C1*C2 + RBv*RT1*RT2*C1*C2 + R1*RT1*RT2*C1*C2 + RL*RM2*RT2*C1*C2
     + RIN*RM2*RT2*C1*C2 + RL*RM1*RT2*C1*C2 + RIN*RM1*RT2*C1*C2 + RIN*RL*RT2*C1*C2 + RBv*RL*RT2*C1*C2 + R1*RL*RT2*C1*C2
     + RBv*RIN*RT2*C1*C2 + R1*RIN*RT2*C1*C2 + RL*RM2*RT1*C1*C2 + RIN*RM2*RT1*C1*C2 + R1*RM2*RT1*C1*C2 + RL*RM1*RT1*C1*C2
     + RIN*RM1*RT1*C1*C2 + R1*RM1*RT1*C1*C2 + RIN*RL*RT1*C1*C2 + RBv*RL*RT1*C1*C2 + R1*RL*RT1*C1*C2 + RBv*RIN*RT1*C1*C2
     + R1*RBv*RT1*C1*C2 + R1*RL*RM2*C1*C2 + R1*RIN*RM2*C1*C2 + R1*RL*RM1*C1*C2 + R1*RIN*RM1*C1*C2 + R1*RIN*RL*C1*C2
     + R1*RBv*RL*C1*C2 + R1*RBv*RIN*C1*C2 + RM1*RM2*RT2*C2*C3 + RBv*RM2*RT2*C2*C3 + RIN*RM1*RT2*C2*C3 + R1*RM1*RT2*C2*C3
     + RBv*RIN*RT2*C2*C3 + R1*RBv*RT2*C2*C3 + RL*RM1*RM2*C2*C3 + RIN*RM1*RM2*C2*C3 + R1*RM1*RM2*C2*C3 + RBv*RL*RM2*C2*C3
     + RBv*RIN*RM2*C2*C3 + R1*RBv*RM2*C2*C3 + RIN*RL*RM1*C2*C3 + R1*RL*RM1*C2*C3 + RBv*RIN*RL*C2*C3 + R1*RBv*RL*C2*C3
     + RM2*RT1*RT2*C1*C3 + RIN*RT1*RT2*C1*C3 + R1*RT1*RT2*C1*C3 + RL*RM2*RT2*C1*C3 + RIN*RM2*RT2*C1*C3 + RIN*RL*RT2*C1*C3
     + R1*RL*RT2*C1*C3 + R1*RIN*RT2*C1*C3 + RM1*RM2*RT1*C1*C3 + RL*RM2*RT1*C1*C3 + RIN*RM2*RT1*C1*C3 + RBv*RM2*RT1*C1*C3
     + R1*RM2*RT1*C1*C3 + RIN*RM1*RT1*C1*C3 + R1*RM1*RT1*C1*C3 + RIN*RL*RT1*C1*C3 + R1*RL*RT1*C1*C3 + RBv*RIN*RT1*C1*C3
     + R1*RBv*RT1*C1*C3 + RL*RM1*RM2*C1*C3 + RIN*RM1*RM2*C1*C3 + RBv*RL*RM2*C1*C3 + R1*RL*RM2*C1*C3 + RBv*RIN*RM2*C1*C3
     + R1*RIN*RM2*C1*C3 + RIN*RL*RM1*C1*C3 + R1*RL*RM1*C1*C3 + R1*RIN*RM1*C1*C3 + RBv*RIN*RL*C1*C3 + R1*RIN*RL*C1*C3
     + R1*RBv*RL*C1*C3 + R1*RBv*RIN*C1*C3;

    double DEN_CIm = RM2*RT2*C2 + RM1*RT2*C2 + RIN*RT2*C2 + RBv*RT2*C2 + R1*RT2*C2 + RL*RM2*C2 + RIN*RM2*C2
     + R1*RM2*C2 + RL*RM1*C2 + RIN*RM1*C2 + R1*RM1*C2 + RIN*RL*C2 + RBv*RL*C2 + R1*RL*C2 + RBv*RIN*C2 + R1*RBv*C2 + RT1*RT2*C1 + RL*RT2*C1 + RIN*RT2*C1
     + RM2*RT1*C1 + RM1*RT1*C1 + RL*RT1*C1 + RBv*RT1*C1 + RL*RM2*C1 + RIN*RM2*C1 + RL*RM1*C1 + RIN*RM1*C1 + RIN*RL*C1 + RBv*RL*C1 + RBv*RIN*C1
     + RM2*RT2*C3 + RIN*RT2*C3 + R1*RT2*C3 + RM1*RM2*C3 + RL*RM2*C3 + RIN*RM2*C3 + RBv*RM2*C3  + R1*RM2*C3 + RIN*RM1*C3 + R1*RM1*C3
     + RIN*RL*C3 + R1*RL*C3 + RBv*RIN*C3 + R1*RBv*C3;

    double DEN_DRe = RT2 + RM2 + RM1 + RL + RBv;

    double NOM_AIm = C1*C2*C3*RL*RM1*RM2*RT2 + C1*C2*C3*RBv*RL*RM2*RT2
     + C1*C2*C3*R1*RL*RM1*RT2 + C1*C2*C3*R1*RBv*RL*RT2 + C1*C2*C3*RL*RM1*RM2*RT1
     + C1*C2*C3*RBv*RL*RM2*RT1 + C1*C2*C3*R1*RL*RM1*RM2 + C1*C2*C3*R1*RBv*RL*RM2;

    double NOM_BRe = C1*C3*RL*RM2*RT2 + C1*C2*RL*RM2*RT2 + C1*C2*RL*RM1*RT2
     + C1*C2*RBv*RL*RT2 + C1*C3*R1*RL*RT2 + C1*C2*R1*RL*RT2 + C1*C3*RL*RM2*RT1
     + C1*C2*RL*RM2*RT1 + C1*C2*RL*RM1*RT1 + C1*C2*RBv*RL*RT1 + C2*C3*RL*RM1*RM2
     + C1*C3*RL*RM1*RM2 + C2*C3*RBv*RL*RM2 + C1*C3*RBv*RL*RM2 + C1*C3*R1*RL*RM2
     + C1*C2*R1*RL*RM2 + C1*C3*R1*RL*RM1 + C1*C2*R1*RL*RM1 + C1*C3*R1*RBv*RL
     + C1*C2*R1*RBv*RL;

    double NOM_CIm = C1*RL*RT2 + C3*RL*RM2 + C2*RL*RM2 + C1*RL*RM2 + C2*RL*RM1 + C1*RL*RM1 + C2*RBv*RL + C1*RBv*RL;

    double NOM_DRe = 0.0;

    double b0 = NOM_DRe;
    double b1 = NOM_CIm;
    double b2 = NOM_BRe;
    double b3 = NOM_AIm;
    double a0 = DEN_DRe;
    double a1 = DEN_CIm;
    double a2 = DEN_BRe;
    double a3 = DEN_AIm;

    double sr = oversampledRate;
    double c  = 2.0 * sr;
    double c2 = c * c;
    double c3 = c * c * c;

    double B0 = b0 + b1*c + b2*c2 + b3*c3;
    double B1 = 3.0*b0 + b1*c - b2*c2 - 3.0*b3*c3;
    double B2 = 3.0*b0 - b1*c - b2*c2 + 3.0*b3*c3;
    double B3 = b0 - b1*c + b2*c2 - b3*c3;

    double A0 = a0 + a1*c + a2*c2 + a3*c3;
    double A1 = 3.0*a0 + a1*c - a2*c2 - 3.0*a3*c3;
    double A2 = 3.0*a0 - a1*c - a2*c2 + 3.0*a3*c3;
    double A3 = a0 - a1*c + a2*c2 - a3*c3;

    double invA0 = 1.0 / A0;
    float nb0 = static_cast<float>(B0 * invA0);
    float nb1 = static_cast<float>(B1 * invA0);
    float nb2 = static_cast<float>(B2 * invA0);
    float nb3 = static_cast<float>(B3 * invA0);
    float na1 = static_cast<float>(A1 * invA0);
    float na2 = static_cast<float>(A2 * invA0);
    float na3 = static_cast<float>(A3 * invA0);

    toneStackL.b[0] = nb0;  toneStackL.b[1] = nb1;  toneStackL.b[2] = nb2;  toneStackL.b[3] = nb3;
    toneStackL.a[0] = na1;  toneStackL.a[1] = na2;  toneStackL.a[2] = na3;
    toneStackR.b[0] = nb0;  toneStackR.b[1] = nb1;  toneStackR.b[2] = nb2;  toneStackR.b[3] = nb3;
    toneStackR.a[0] = na1;  toneStackR.a[1] = na2;  toneStackR.a[2] = na3;
}

void AmpSimGold::updateCabinet()
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

void AmpSimGold::updateBrightnessFilter()
{
    float db = -6.0f + 12.0f * brightnessParam;
    *brightnessFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        currentSampleRate, 3500.0f, 0.707f, juce::Decibels::decibelsToGain(db));
}

void AmpSimGold::updatePresenceNfbCoeff()
{
    float freq = 15000.0f * std::pow(800.0f / 15000.0f, presenceParam);
    nfbLpfCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * freq
                                   / static_cast<float>(oversampledRate));
}

void AmpSimGold::updateMicPositionFilter()
{
    float db = -4.0f + 8.0f * micPositionParam;
    *micPositionFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        currentSampleRate, 1500.0f, 0.707f, juce::Decibels::decibelsToGain(db));
}
