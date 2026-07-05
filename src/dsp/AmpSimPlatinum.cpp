#include "AmpSimPlatinum.h"
#include "BinaryData.h"

#include <cmath>

#if ORB_OFFLINE_TOOLS
// C59 diag arm (out of line to keep the hot loop lean). Any code added to
// this function perturbs MSVC's scheduling of the float32 NR loops, so
// builds carrying the arm sit <= 1 ULP from pre-arm renders - codegen,
// not semantics. Bench nulls re-baselined accordingly.
#if defined(_MSC_VER)
 #define ORB_NOINLINE __declspec(noinline)
#else
 #define ORB_NOINLINE __attribute__((noinline))
#endif
static ORB_NOINLINE void applyC59Damper(float& diffState, float coeff,
                                        float& plateA, float& plateB)
{
    const float cm = 0.5f * (plateA + plateB);
    diffState += coeff * ((plateA - plateB) - diffState);
    plateA = cm + 0.5f * diffState;
    plateB = cm - 0.5f * diffState;
}
#endif

//==============================================================================
// Cabinet names
//==============================================================================
static const char* cabinetNamesPlatinum[AmpSimPlatinum::kNumCabinets] = {
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

const char* AmpSimPlatinum::getCabinetName(int index)
{
    if (index >= 0 && index < kNumCabinets)
        return cabinetNamesPlatinum[index];
    if (index == kNoCabinet)
        return "No Cabinet";
    if (index == kCustomCabinet)
        return "Custom IR";
    return "";
}

void AmpSimPlatinum::getIRData(int index, const void*& data, int& dataSize)
{
    switch (index)
    {
        case 0:  data = BinaryData::Studio_57_wav;       dataSize = BinaryData::Studio_57_wavSize;       break;
        case 1:  data = BinaryData::In_Your_Face_wav;    dataSize = BinaryData::In_Your_Face_wavSize;    break;
        case 2:  data = BinaryData::Tight_Box_wav;       dataSize = BinaryData::Tight_Box_wavSize;       break;
        case 3:  data = BinaryData::Crystal_412_wav;     dataSize = BinaryData::Crystal_412_wavSize;     break;
        case 4:  data = BinaryData::British_Bite_wav;    dataSize = BinaryData::British_Bite_wavSize;    break;
        case 5:  data = BinaryData::Velvet_Blues_wav;    dataSize = BinaryData::Velvet_Blues_wavSize;    break;
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
// Constructor
//==============================================================================
AmpSimPlatinum::AmpSimPlatinum()
{
    setBypassed(true);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        2, kOsStages, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);

    configureTriodeStages();
    configurePowerAmpStages();
}

//==============================================================================
// configureTriodeStages
//==============================================================================
void AmpSimPlatinum::configureTriodeStages()
{
    // Common 12AX7 tube parameters
    const float mu     = 95.43f;
    const float Ks     = 1.147e-6f;
    const float Ex     = 1.5f;
    const float offset = 45.0f;

    // V1A
    {
        TriodeStage::Config cfg;
        cfg.Rp    = 100000.0f;
        cfg.Rk    = 1000.0f;
        cfg.Ck    = 4.7e-6f;
        cfg.Cp    = 0.0f;
        cfg.Bplus = 348.0f;
        cfg.mu = mu; cfg.Ks = Ks; cfg.Ex = Ex; cfg.offset = offset;
        v1aL.configure(cfg);
        v1aR.configure(cfg);
    }

    // V1B
    {
        TriodeStage::Config cfg;
        cfg.Rp    = 100000.0f;
        cfg.Rk    = 1800.0f;
        cfg.Ck    = 1e-6f;
        cfg.Cp    = 470e-12f;
        cfg.Bplus = 343.2f;
        cfg.mu = mu; cfg.Ks = Ks; cfg.Ex = Ex; cfg.offset = offset;
        v1bL.configure(cfg);
        v1bR.configure(cfg);
    }

    // V2A
    {
        TriodeStage::Config cfg;
        cfg.Rp    = 100000.0f;
        cfg.Rk    = 4700.0f;
        cfg.Ck    = 0.0f;
        cfg.Cp    = 0.0f;
        cfg.Bplus = 345.6f;
        cfg.mu = mu; cfg.Ks = Ks; cfg.Ex = Ex; cfg.offset = offset;
        v2aL.configure(cfg);
        v2aR.configure(cfg);
    }

    // V2B
    {
        TriodeStage::Config cfg;
        cfg.Rp    = 100000.0f;
        cfg.Rk    = 2200.0f;
        cfg.Ck    = 10e-6f;
        cfg.Cp    = 0.0f;
        cfg.Bplus = 362.2f;
        cfg.mu = mu; cfg.Ks = Ks; cfg.Ex = Ex; cfg.offset = offset;
        v2bL.configure(cfg);
        v2bR.configure(cfg);
    }

    // V3A
    {
        TriodeStage::Config cfg;
        cfg.Rp    = 100000.0f;
        cfg.Rk    = 1000.0f;
        cfg.Ck    = 10e-6f;
        cfg.Cp    = 0.0f;
        cfg.Bplus = 381.4f;
        cfg.mu = mu; cfg.Ks = Ks; cfg.Ex = Ex; cfg.offset = offset;
        v3aL.configure(cfg);
        v3aR.configure(cfg);
    }
}

//==============================================================================
// configurePowerAmpStages
//==============================================================================
void AmpSimPlatinum::configurePowerAmpStages()
{
    static constexpr float BPLUS_J4 = 389.3f;
    static constexpr float RP_V4A   = 100000.0f;  // R95
    static constexpr float RP_V4B   = 82000.0f;   // R94
    static constexpr float R93_VAL  = 470.0f;     // Tail resistor

    v4_Vk_q  = 39.94f;
    v4a_Ip_q = 2.23e-3f;
    v4b_Ip_q = 0.406e-3f;
    v4a_Vp_q = BPLUS_J4 - v4a_Ip_q * RP_V4A;
    v4b_Vp_q = BPLUS_J4 - v4b_Ip_q * RP_V4B;

    v4_Vtail_q = v4_Vk_q - (v4a_Ip_q + v4b_Ip_q) * R93_VAL;

    v4a_Vg_q = v4_Vtail_q;
    v4b_Vg_q = v4_Vtail_q * (10.0f / 11.0f);

    // V5/V6 pentode stages
    PentodeStage::Config pentCfg;
    pentCfg.Rp_dc     = 50.0f;      // Transformer DCR per half
    pentCfg.Rp_signal = 848.0f;     // Reflected half-load: (N/2)^2 * R_speaker = 10.3^2 * 8
    pentCfg.Rk        = 1.0f;       // R107/R108 cathode resistor
    pentCfg.Bplus     = 439.0f;     // Raw B+ supply
    pentCfg.Vg2k      = 424.7f;     // Screen voltage: 432V - Ig2*R106 (1K) ~ 425V
    pentCfg.Vbias     = -42.0f;     // Suppressor bias from -42V supply
    pentCfg.MU        = 8.12f;
    pentCfg.EX        = 1.0f;
    pentCfg.KG1       = 373.9f;
    pentCfg.KP        = 55.47f;
    pentCfg.KVB       = 52.71f;

    v5L.configure(pentCfg);  v5R.configure(pentCfg);
    v6L.configure(pentCfg);  v6R.configure(pentCfg);

    speakerNormFactor = 1.0f / (v5L.getQuiescentIp() + v6L.getQuiescentIp());
}

//==============================================================================
// applyGainMode
//==============================================================================
void AmpSimPlatinum::applyGainMode()
{
    const int mode = gainModeParam.load(std::memory_order_acquire);
    if (mode == 0)
    {
        // GAIN1: full cathode bypass
        v1bL.setCathodeCap(1e-6f, 1800.0f);
        v1bR.setCathodeCap(1e-6f, 1800.0f);
    }
    else
    {
        // GAIN2: partial bypass (10K series in bypass path, effective Rk=11.8K)
        v1bL.setCathodeCap(1e-6f, 11800.0f);
        v1bR.setCathodeCap(1e-6f, 11800.0f);
    }
}

//==============================================================================
// prepare
//==============================================================================
void AmpSimPlatinum::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    size_t osStages = kOsStages;
    auto   osFilter = juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR;
#if ORB_OFFLINE_TOOLS
    osStages = static_cast<size_t>(diag.osStages);
    if (diag.osFir)
        osFilter = juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple;
#endif
    const int osFactor = 1 << osStages;
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        2, osStages, osFilter, true);
    oversampledRate = sampleRate * osFactor;

    oversampling->initProcessing(static_cast<size_t>(samplesPerBlock));
    oversampling->reset();

    const int osBlockSize = samplesPerBlock * osFactor;

    v1aL.prepare(oversampledRate); v1aR.prepare(oversampledRate);
    v1bL.prepare(oversampledRate); v1bR.prepare(oversampledRate);
    v2aL.prepare(oversampledRate); v2aR.prepare(oversampledRate);
    v2bL.prepare(oversampledRate); v2bR.prepare(oversampledRate);
    v3aL.prepare(oversampledRate); v3aR.prepare(oversampledRate);

    configureTriodeStages();
    configurePowerAmpStages();

    applyGainMode();
    lastGainMode = gainModeParam.load(std::memory_order_acquire);

    coupCapR_34Hz = 1.0f - (2.0f * juce::MathConstants<float>::pi * 34.0f
                             / static_cast<float>(oversampledRate));
    coupCapR_15Hz = 1.0f - (2.0f * juce::MathConstants<float>::pi * 15.0f
                             / static_cast<float>(oversampledRate));

    v1aMillerCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi
                                      * 1620.0f / static_cast<float>(oversampledRate));
    v1aMillerLpfL = 0.0f;
    v1aMillerLpfR = 0.0f;

    odC25Coeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi
                                  * 10000.0f / static_cast<float>(oversampledRate));
    odC25LpfL = 0.0f;
    odC25LpfR = 0.0f;

    v2bMillerCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi
                                      * 4760.0f / static_cast<float>(oversampledRate));
    v2bMillerLpfL = 0.0f;
    v2bMillerLpfR = 0.0f;

    coupCapAL.reset(); coupCapAR.reset();
    coupCapBL.reset(); coupCapBR.reset();
    coupCapCL.reset(); coupCapCR.reset();
    coupCapDL.reset(); coupCapDR.reset();

    dcBlockerR_coeff = 1.0f - (2.0f * juce::MathConstants<float>::pi * 7.0f
                                / static_cast<float>(oversampledRate));
    dcBlockerL.reset(); dcBlockerR.reset();

    juce::dsp::ProcessSpec monoOsSpec;
    monoOsSpec.sampleRate       = oversampledRate;
    monoOsSpec.maximumBlockSize = static_cast<juce::uint32>(osBlockSize);
    monoOsSpec.numChannels      = 1;

    gainFilterL.prepare(monoOsSpec);  gainFilterR.prepare(monoOsSpec);
    xfmrHpfL.prepare(monoOsSpec);    xfmrHpfR.prepare(monoOsSpec);

    *xfmrHpfL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(
        oversampledRate, 80.0f, 0.5f);
    *xfmrHpfR.coefficients = *xfmrHpfL.coefficients;

    leakageLpfCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi
                                       * 5402.0f / static_cast<float>(oversampledRate));
    leakageLpfStateL = 0.0f;
    leakageLpfStateR = 0.0f;

#if ORB_OFFLINE_TOOLS
    // C59 47p plate-to-plate at the LTP: differential corner 1/(2pi*Zdiff*C59),
    // Zdiff = 29.9K from the small-signal solve at the quiescent point
    // (cathode coupling + 220K EL34 grid leaks included) -> 113.4 kHz.
    // Supersonic by construction; the arm closes the damper trail with a
    // measured null rather than an assumption.
    c59LpfCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi
                                   * 113400.0f / static_cast<float>(oversampledRate));
    c59DiffStateL = 0.0f;
    c59DiffStateR = 0.0f;
#endif

    nfbLpfCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 723.0f
                                   / static_cast<float>(oversampledRate));
    nfbFilteredStateL = 0.0f;
    nfbFilteredStateR = 0.0f;
    prevSpeakerOutL   = 0.0f;
    prevSpeakerOutR   = 0.0f;

    coupCapR_339Hz = 1.0f - (2.0f * juce::MathConstants<float>::pi * 339.0f
                              / static_cast<float>(oversampledRate));

    // Tail ride onto the LTP grid refs: R97*C63 (1M x 0.1uF) and (R92||R91)*C58
    v4RideCoeffA = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 1.6f
                                    / static_cast<float>(oversampledRate));
    v4RideCoeffB = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 8.0f
                                    / static_cast<float>(oversampledRate));
    v4aRideL = 0.0f; v4aRideR = 0.0f;
    v4bRideL = 0.0f; v4bRideR = 0.0f;

    coupCapC58L.reset(); coupCapC58R.reset();
    coupCapC61L.reset(); coupCapC61R.reset();
    coupCapC62L.reset(); coupCapC62R.reset();
    coupCapNfbL.reset(); coupCapNfbR.reset();

    sagAttackCoeff  = 1.0f - std::exp(-1.0f / (static_cast<float>(oversampledRate) * 0.020f));
    sagReleaseCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(oversampledRate) * 0.200f));
    sagEnvelopeL = 0.0f;
    sagEnvelopeR = 0.0f;

    ovLevelSmoothed.reset(sampleRate, 0.01);
    ovLevelSmoothed.setCurrentAndTargetValue(ovLevelParam);

    masterSmoothed.reset(sampleRate, 0.01);
    masterSmoothed.setCurrentAndTargetValue(masterParam);

    lastGainParam        = -1.0f;
    lastBassParam        = -1.0f;
    lastMidParam         = -1.0f;
    lastTrebleParam      = -1.0f;
    lastMicPositionParam = -1.0f;

    updateGainFilter();
    updateToneStackCoeffs();

    juce::dsp::ProcessSpec baseSpec;
    baseSpec.sampleRate       = sampleRate;
    baseSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    baseSpec.numChannels      = 2;

    cabinetConvolution.prepare(baseSpec);
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

    micPositionFilter.prepare(baseSpec);
    updateMicPositionFilter();
}

//==============================================================================
// reset
//==============================================================================
void AmpSimPlatinum::reset()
{
    oversampling->reset();

    v1aL.reset(); v1aR.reset();
    v1bL.reset(); v1bR.reset();
    v2aL.reset(); v2aR.reset();
    v2bL.reset(); v2bR.reset();
    v3aL.reset(); v3aR.reset();

    coupCapAL.reset(); coupCapAR.reset();
    coupCapBL.reset(); coupCapBR.reset();
    coupCapCL.reset(); coupCapCR.reset();
    coupCapDL.reset(); coupCapDR.reset();
    dcBlockerL.reset(); dcBlockerR.reset();

    v1aMillerLpfL = 0.0f; v1aMillerLpfR = 0.0f;
    odC25LpfL = 0.0f; odC25LpfR = 0.0f;
    v2bMillerLpfL = 0.0f; v2bMillerLpfR = 0.0f;
    gainFilterL.reset(); gainFilterR.reset();
#if ORB_OFFLINE_TOOLS
    brightNetL.reset();  brightNetR.reset();
    c59DiffStateL = 0.0f; c59DiffStateR = 0.0f;
#endif
    toneStackL.reset();  toneStackR.reset();
    xfmrHpfL.reset();    xfmrHpfR.reset();

    // V4 LTP state
    v4a_IpPrevL = v4a_Ip_q;  v4a_IpPrevR = v4a_Ip_q;
    v4b_IpPrevL = v4b_Ip_q;  v4b_IpPrevR = v4b_Ip_q;
    v4aRideL = 0.0f; v4aRideR = 0.0f;
    v4bRideL = 0.0f; v4bRideR = 0.0f;

    // V5/V6 pentode stages
    v5L.reset(); v5R.reset();
    v6L.reset(); v6R.reset();

    coupCapC58L.reset(); coupCapC58R.reset();
    coupCapC61L.reset(); coupCapC61R.reset();
    coupCapC62L.reset(); coupCapC62R.reset();
    coupCapNfbL.reset(); coupCapNfbR.reset();

    leakageLpfStateL = 0.0f; leakageLpfStateR = 0.0f;

    cabinetConvolution.reset();
    cabMakeupGain.reset(currentSampleRate, 0.2);
    micPositionFilter.reset();

    nfbFilteredStateL = 0.0f;
    nfbFilteredStateR = 0.0f;
    prevSpeakerOutL = 0.0f;
    prevSpeakerOutR = 0.0f;
    sagEnvelopeL = 0.0f;
    sagEnvelopeR = 0.0f;

    lastCabinetType = -1;
}

//==============================================================================
// getLatencySamples
//==============================================================================
int AmpSimPlatinum::getLatencySamples() const
{
    int latency = static_cast<int>(oversampling->getLatencyInSamples());
    if (cabinetTypeParam.load(std::memory_order_acquire) != kNoCabinet)
        latency += static_cast<int>(cabinetConvolution.getLatency());
    return latency;
}

//==============================================================================
// process
//==============================================================================
void AmpSimPlatinum::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    const float currentGain        = gainParam;
    const float currentOvLevel     = ovLevelParam;
    const float currentMaster      = masterParam;
    const float currentSpeakerDrive = speakerDriveParam;
    const int   currentGainMode    = gainModeParam.load(std::memory_order_acquire);

    const bool gainChanged    = (currentGain != lastGainParam);
    const bool toneChanged    = (bassParam  != lastBassParam  ||
                                  midParam   != lastMidParam   ||
                                  trebleParam != lastTrebleParam);
    const bool micChanged     = (micPositionParam != lastMicPositionParam);
    const bool gainModeChanged = (currentGainMode != lastGainMode);

    if (gainChanged)
    {
        updateGainFilter();
        lastGainParam = currentGain;
    }
    if (toneChanged)
    {
        updateToneStackCoeffs();
        lastBassParam   = bassParam;
        lastMidParam    = midParam;
        lastTrebleParam = trebleParam;
    }
    if (micChanged)
    {
        updateMicPositionFilter();
        lastMicPositionParam = micPositionParam;
    }
    if (gainModeChanged)
    {
        applyGainMode();
        lastGainMode = currentGainMode;
    }

    ovLevelSmoothed.setTargetValue(currentOvLevel);
    masterSmoothed.setTargetValue(currentMaster);

    updateCabinet();

    //------------------------------------------------------------------------
    // Upsample
    //------------------------------------------------------------------------
    juce::dsp::AudioBlock<float> block(buffer);
    auto oversampledBlock = oversampling->processSamplesUp(block);

    const int osNumSamples  = static_cast<int>(oversampledBlock.getNumSamples());
    const int osNumChannels = juce::jmin(
        static_cast<int>(oversampledBlock.getNumChannels()), 2);

    const float v3aNormL = 1.0f / std::max(v3aL.getOutputSwing(), 1.0f);
    const float v3aNormR = 1.0f / std::max(v3aR.getOutputSwing(), 1.0f);

    for (int ch = 0; ch < osNumChannels; ++ch)
    {
        auto* samples = oversampledBlock.getChannelPointer(static_cast<size_t>(ch));

        auto& v1a       = (ch == 0) ? v1aL : v1aR;
        auto& v1b       = (ch == 0) ? v1bL : v1bR;
        auto& v2a       = (ch == 0) ? v2aL : v2aR;
        auto& v2b       = (ch == 0) ? v2bL : v2bR;
        auto& v3a       = (ch == 0) ? v3aL : v3aR;
        auto& coupA     = (ch == 0) ? coupCapAL : coupCapAR;
        auto& coupB     = (ch == 0) ? coupCapBL : coupCapBR;
        auto& coupC     = (ch == 0) ? coupCapCL : coupCapCR;
        auto& coupD     = (ch == 0) ? coupCapDL : coupCapDR;
        auto& gainFilt  = (ch == 0) ? gainFilterL : gainFilterR;
        auto& toneStack = (ch == 0) ? toneStackL  : toneStackR;
        auto& sagEnv    = (ch == 0) ? sagEnvelopeL : sagEnvelopeR;
        auto& nfbFilt   = (ch == 0) ? nfbFilteredStateL : nfbFilteredStateR;
        auto& xfmrHpf   = (ch == 0) ? xfmrHpfL : xfmrHpfR;
        auto& dcBlock   = (ch == 0) ? dcBlockerL : dcBlockerR;

        auto& coupC58   = (ch == 0) ? coupCapC58L : coupCapC58R;
        auto& coupC61   = (ch == 0) ? coupCapC61L : coupCapC61R;
        auto& coupC62   = (ch == 0) ? coupCapC62L : coupCapC62R;
        auto& coupNfb   = (ch == 0) ? coupCapNfbL : coupCapNfbR;
        auto& v4a_IpPr  = (ch == 0) ? v4a_IpPrevL : v4a_IpPrevR;
        auto& v4b_IpPr  = (ch == 0) ? v4b_IpPrevL : v4b_IpPrevR;
        auto& v4aRide   = (ch == 0) ? v4aRideL : v4aRideR;
        auto& v4bRide   = (ch == 0) ? v4bRideL : v4bRideR;
        auto& v5        = (ch == 0) ? v5L : v5R;
        auto& v6        = (ch == 0) ? v6L : v6R;
        auto& leakLpf   = (ch == 0) ? leakageLpfStateL : leakageLpfStateR;
        auto& prevSpkr  = (ch == 0) ? prevSpeakerOutL : prevSpeakerOutR;
        const float v3aNorm = (ch == 0) ? v3aNormL : v3aNormR;
#if ORB_OFFLINE_TOOLS
        const bool  c59Enabled    = diag.c59;
        const bool  v4tailEnabled = diag.v4tail;
        const int   v4OuterIters  = diag.v4outers;
        const bool  v4RidesOn     = diag.v4rides;
        const float v4NfbScale    = diag.v4nfb;
        const int   v4DumpMode    = diag.v4dump;
#endif

        const float master    = masterSmoothed.getCurrentValue();

        const float ovLevelPot = ovLevelSmoothed.getCurrentValue();
        float ovLevelAtten;
        {
            constexpr float R45  = 470000.0f;
            constexpr float R154 = 1000000.0f;
            constexpr float VR2  = 200000.0f;
            constexpr float R66  = 47000.0f;

            float R_above = VR2 * (1.0f - ovLevelPot);
            float R_below = VR2 * ovLevelPot + 1.0f;  // +1 to avoid div-by-zero at pot=0
            float R_below_loaded = (R_below * R66) / (R_below + R66);
            ovLevelAtten = R_below_loaded / (R45 + R154 + R_above + R_below_loaded);
        }

        for (int i = 0; i < osNumSamples; ++i)
        {
            float s = samples[i];

            //------------------------------------------------------------------
            // Thermal noise injection at V1A input
            //------------------------------------------------------------------
            {
                auto& ns = (ch == 0) ? noiseStateL : noiseStateR;
                ns ^= ns << 13;
                ns ^= ns >> 17;
                ns ^= ns << 5;
                float noise = (static_cast<float>(ns) / 2147483648.0f - 1.0f);
                s += noise * noiseInjectLevel;
            }

            //------------------------------------------------------------------
            // Stage 2: V1A Miller cap LPF + triode
            //------------------------------------------------------------------
            {
                auto& millerState = (ch == 0) ? v1aMillerLpfL : v1aMillerLpfR;
                millerState += v1aMillerCoeff * (s - millerState);
                s = millerState;
            }
            s = v1a.processSample(s);

            // Tap 1: after V1A (voltage domain)
            if (stageLimit == 1) { samples[i] = s * v3aNorm; continue; }

            //------------------------------------------------------------------
            // Stage 3: Coupling cap A (4.7nF, fc~34Hz)
            //------------------------------------------------------------------
            s = coupA.processSample(s, coupCapR_34Hz);

            //------------------------------------------------------------------
            // Stage 4: GAIN network + GAIN pot
            //------------------------------------------------------------------
#if ORB_OFFLINE_TOOLS
            if (diag.brightFix)
            {
                auto& brightNet = (ch == 0) ? brightNetL : brightNetR;
                s = brightNet.processSample(s);
            }
            else
#endif
            {
                s = gainFilt.processSample(s);
                s *= 0.242f * gainParam;
            }

            //------------------------------------------------------------------
            // Stage 5: V1B triode
            //------------------------------------------------------------------
            {
                constexpr float gc = 3.0f;
                if (s > gc) s = gc + std::tanh((s - gc) * 0.3f) * 0.5f;
            }
            s = v1b.processSample(s);

            // Tap 2: after V1B (voltage domain)
            if (stageLimit == 2) { samples[i] = s * v3aNorm; continue; }

            //------------------------------------------------------------------
            // Stage 6: Coupling cap B + OD network C25 shunt pole
            //------------------------------------------------------------------
            s = coupB.processSample(s, coupCapR_34Hz);
            {
                auto& odC25 = (ch == 0) ? odC25LpfL : odC25LpfR;
                odC25 += odC25Coeff * (s - odC25);
                s = odC25;
            }

            //------------------------------------------------------------------
            // Stage 7: V2A triode
            //------------------------------------------------------------------
            {
                constexpr float gc = 4.0f;
                if (s > gc) s = gc + std::tanh((s - gc) * 0.3f) * 0.5f;
            }
            s = v2a.processSample(s);

            // Tap 3: after V2A (voltage domain)
            if (stageLimit == 3) { samples[i] = s * v3aNorm; continue; }

            //------------------------------------------------------------------
            // Stage 8: Coupling cap C + R41/R42 interstage divider
            //------------------------------------------------------------------
            s = coupC.processSample(s, coupCapR_15Hz);
            s *= 0.680f;

            //------------------------------------------------------------------
            // Stage 9: V2B Miller cap + triode
            //------------------------------------------------------------------
            {
                auto& v2bMiller = (ch == 0) ? v2bMillerLpfL : v2bMillerLpfR;
                v2bMiller += v2bMillerCoeff * (s - v2bMiller);
                s = v2bMiller;
            }
            {
                constexpr float gc = 3.5f;
                if (s > gc) s = gc + std::tanh((s - gc) * 0.3f) * 0.5f;
            }
            s = v2b.processSample(s);

            // Tap 4: after V2B (voltage domain)
            if (stageLimit == 4) { samples[i] = s * v3aNorm; continue; }

            //------------------------------------------------------------------
            // Stage 10: Coupling cap D (22nF, fc~15Hz)
            //------------------------------------------------------------------
            s = coupD.processSample(s, coupCapR_15Hz);

            //------------------------------------------------------------------
            // Stage 11: OV Level attenuator
            //------------------------------------------------------------------
            s *= ovLevelAtten;

            //------------------------------------------------------------------
            // Stage 12: V3A triode
            //------------------------------------------------------------------
            {
                constexpr float gc = 2.5f;
                if (s > gc) s = gc + std::tanh((s - gc) * 0.3f) * 0.5f;
            }
            s = v3a.processSample(s);

            //------------------------------------------------------------------
            // Stage 13: Normalize to [-1, +1]
            //------------------------------------------------------------------
            s *= v3aNorm;

            // Tap 5: after V3A + normalize (normalized domain)
            if (stageLimit == 5) { samples[i] = s; continue; }

            //------------------------------------------------------------------
            // Stage 14: V3B cathode follower buffer
            //------------------------------------------------------------------
            s = std::tanh(s * 1.5f) * 0.95f;

            // Tap 6: after cathode follower (normalized domain)
            if (stageLimit == 6) { samples[i] = s; continue; }

            //------------------------------------------------------------------
            // Stage 15: Tone stack (3rd-order IIR)
            //------------------------------------------------------------------
            s = toneStack.processSample(s) * 3.16f;

            // Tap 7: after tone stack (normalized domain)
            if (stageLimit == 7) { samples[i] = s; continue; }

            //------------------------------------------------------------------
            // Stage 16: Power supply sag
            //------------------------------------------------------------------
            {
                const float sagAmount = 0.35f * currentSpeakerDrive;
                const float envelope  = std::abs(s);
                const float coeff = (envelope > sagEnv) ? sagAttackCoeff : sagReleaseCoeff;
                sagEnv += coeff * (envelope - sagEnv);
                const float sagDepth = sagAmount * sagEnv;
                const float gainReduction = 1.0f - juce::jlimit(0.0f, 0.5f, sagDepth);
                s *= gainReduction;
            }

            //------------------------------------------------------------------
            // Stage 17: Master volume
            //------------------------------------------------------------------
            s *= master;

#if ORB_OFFLINE_TOOLS
            float v4Dbg = 0.0f;
#endif
            //------------------------------------------------------------------
            // Stage 18a: C58 coupling cap
            //------------------------------------------------------------------
            {
                constexpr float V_MASTER_SCALE = 1.5f;
                float masterVolts = s * V_MASTER_SCALE;
                float c58Out = coupC58.processSample(masterVolts, coupCapR_34Hz);

                //--------------------------------------------------------------
                // Stage 18b: NFB path (one-sample loop delay)
                //--------------------------------------------------------------
                constexpr float NFB_GAIN = 0.22f;
                float nfbRaw = prevSpkr * NFB_GAIN;
                nfbFilt += nfbLpfCoeff * (nfbRaw - nfbFilt);
                float nfbHpf = coupNfb.processSample(nfbFilt, coupCapR_339Hz);

                //--------------------------------------------------------------
                // Stage 18c: V4 12AT7 LTP phase splitter
                //--------------------------------------------------------------
                constexpr float BPLUS_J4  = 389.3f;
                constexpr float RP_V4A    = 100000.0f;
                constexpr float RP_V4B    = 82000.0f;
                constexpr float R93_VAL   = 470.0f;
                constexpr float MU_12AT7  = 63.19f;
                constexpr float KS_12AT7  = 6.717e-6f;

                float Ip_A = v4a_IpPr;
                float Ip_B = v4b_IpPr;

#if ORB_OFFLINE_TOOLS
                if (!v4tailEnabled)
                {
                    // Frozen-tail reference arm (pre-fix behavior) for A/B renders.
                    float v4a_grid = v4a_Vg_q + nfbHpf;
                    float v4b_grid = v4b_Vg_q + c58Out;

                    constexpr float RpR93_A = RP_V4A + R93_VAL;
                    constexpr float RpR93_B = RP_V4B + R93_VAL;

                    for (int outer = 0; outer < 2; ++outer)
                    {
                        // --- V4A NR (Rp=100K, uses current Ip_B) ---
                        {
                            const float IpMax_A = std::max(0.0f,
                                (BPLUS_J4 - Ip_B * R93_VAL - v4_Vtail_q) / RpR93_A);
                            for (int nr = 0; nr < 4; ++nr)
                            {
                                float Vk  = (Ip_A + Ip_B) * R93_VAL + v4_Vtail_q;
                                float Vgk = v4a_grid - Vk;
                                float Vpk = BPLUS_J4 - Ip_A * RP_V4A - Vk;
                                float ca  = Vpk + MU_12AT7 * Vgk;
                                if (ca <= 0.0f) { Ip_A = 0.0f; break; }
                                float re = 1.0f, dre = 0.0f;
                                if (Vpk < 5.0f) {
                                    if (Vpk <= 0.0f) { Ip_A = 0.0f; break; }
                                    float t = Vpk * 0.2f;
                                    re = t * t * (3.0f - 2.0f * t);
                                    dre = 1.2f * t * (1.0f - t);
                                }
                                float ca_sqrt  = std::sqrt(ca);
                                float Ip_model = re * KS_12AT7 * ca * ca_sqrt;
                                float residual = Ip_A - Ip_model;
                                if (std::abs(residual) < 1e-10f) break;
                                float dIp_dVgk = re * KS_12AT7 * 1.5f * MU_12AT7 * ca_sqrt;
                                float dIp_dVpk = KS_12AT7 * ca_sqrt * (re * 1.5f + dre * ca);
                                float jac = 1.0f + R93_VAL * dIp_dVgk + RpR93_A * dIp_dVpk;
                                if (std::abs(jac) < 1e-12f) break;
                                Ip_A -= residual / jac;
                                Ip_A = std::clamp(Ip_A, 0.0f, IpMax_A);
                            }
                        }

                        // --- V4B NR (Rp=82K, uses current Ip_A) ---
                        {
                            const float IpMax_B = std::max(0.0f,
                                (BPLUS_J4 - Ip_A * R93_VAL - v4_Vtail_q) / RpR93_B);
                            for (int nr = 0; nr < 4; ++nr)
                            {
                                float Vk  = (Ip_A + Ip_B) * R93_VAL + v4_Vtail_q;
                                float Vgk = v4b_grid - Vk;
                                float Vpk = BPLUS_J4 - Ip_B * RP_V4B - Vk;
                                float ca  = Vpk + MU_12AT7 * Vgk;
                                if (ca <= 0.0f) { Ip_B = 0.0f; break; }
                                float re = 1.0f, dre = 0.0f;
                                if (Vpk < 5.0f) {
                                    if (Vpk <= 0.0f) { Ip_B = 0.0f; break; }
                                    float t = Vpk * 0.2f;
                                    re = t * t * (3.0f - 2.0f * t);
                                    dre = 1.2f * t * (1.0f - t);
                                }
                                float ca_sqrt  = std::sqrt(ca);
                                float Ip_model = re * KS_12AT7 * ca * ca_sqrt;
                                float residual = Ip_B - Ip_model;
                                if (std::abs(residual) < 1e-10f) break;
                                float dIp_dVgk = re * KS_12AT7 * 1.5f * MU_12AT7 * ca_sqrt;
                                float dIp_dVpk = KS_12AT7 * ca_sqrt * (re * 1.5f + dre * ca);
                                float jac = 1.0f + R93_VAL * dIp_dVgk + RpR93_B * dIp_dVpk;
                                if (std::abs(jac) < 1e-12f) break;
                                Ip_B -= residual / jac;
                                Ip_B = std::clamp(Ip_B, 0.0f, IpMax_B);
                            }
                        }
                    }
                }
                else
#endif
                {
                    // Dynamic tail: below R93 the real network runs R98 (10K) to the
                    // NFB driver node JX1, whose effective impedance (4.6K) and the
                    // NFB arrival there were regressed from MNA node traces
                    // (tools/f2_tail_thevenin.py). Grid bias refs derive from the
                    // tail (R97 -> grid A, R92/R91 divider -> grid B), and grid A
                    // additionally rides JX1 through C63 - so V4A sees only R93+R98
                    // of differential tail while V4B sees all of it. Solved as a
                    // scalar Newton on Vk; each triode solves at fixed Vk (the old
                    // alternating solve stalls at this coupling strength).
                    constexpr float R_JX1    = 4600.0f;
                    constexpr float R_TAIL   = R93_VAL + 10000.0f + R_JX1;
                    constexpr float G_RIDE_A = R_JX1 / R_TAIL;

                    // NFB stays on grid A only: the engine's nfbHpf is loop-shaping
                    // calibration, not a faithful JX1 voltage - injecting it into
                    // the tail adds a common-mode path that rings at ~2.2 kHz
                    // through the asymmetric plate loads.
#if ORB_OFFLINE_TOOLS
                    const int   nOuter   = v4OuterIters;
                    const float nfbGridA = nfbHpf * v4NfbScale;
#else
                    // 12: the V4B cutoff kink rejects Newton steps, so worst-case
                    // samples bisect the full bracket; early break keeps the
                    // steady state at 1-2 iterations
                    constexpr int nOuter = 12;
                    const float nfbGridA = nfbHpf;
#endif
                    const float iSumQ    = v4a_Ip_q + v4b_Ip_q;
                    const float gridA0   = v4a_Vg_q + v4aRide + nfbGridA
                                         - G_RIDE_A * v4_Vk_q;
                    const float v4b_grid = v4b_Vg_q + (10.0f / 11.0f) * v4bRide + c58Out;

                    float lo = v4_Vk_q - 8.0f, hi = v4_Vk_q + 8.0f;
                    float Vk = juce::jlimit(lo, hi,
                                            v4_Vk_q + R_TAIL * (Ip_A + Ip_B - iSumQ));
#if ORB_OFFLINE_TOOLS
                    float v4LastF = 0.0f;
#endif
                    for (int outer = 0; outer < nOuter; ++outer)
                    {
                        const float v4a_grid = gridA0 + G_RIDE_A * Vk;
                        float dIpA_dVk = 0.0f, dIpB_dVk = 0.0f;

                        // --- V4A at fixed Vk ---
                        {
                            const float IpMax = std::max(0.0f, (BPLUS_J4 - Vk) / RP_V4A);
                            Ip_A = std::min(Ip_A, IpMax);
                            // must converge from any start: the outer bisection can
                            // move Vk by volts, and a half-solved Ip feeds F a wrong
                            // sign, collapsing the bracket onto a false root
                            for (int nr = 0; nr < 8; ++nr)
                            {
                                float Vgk = v4a_grid - Vk;
                                float Vpk = BPLUS_J4 - Ip_A * RP_V4A - Vk;
                                float ca  = Vpk + MU_12AT7 * Vgk;
                                if (ca <= 0.0f) { Ip_A = 0.0f; dIpA_dVk = 0.0f; break; }
                                float re = 1.0f, dre = 0.0f;
                                if (Vpk < 5.0f) {
                                    if (Vpk <= 0.0f) { Ip_A = 0.0f; dIpA_dVk = 0.0f; break; }
                                    float t = Vpk * 0.2f;
                                    re = t * t * (3.0f - 2.0f * t);
                                    dre = 1.2f * t * (1.0f - t);
                                }
                                float ca_sqrt  = std::sqrt(ca);
                                float Ip_model = re * KS_12AT7 * ca * ca_sqrt;
                                float dIp_dVgk = re * KS_12AT7 * 1.5f * MU_12AT7 * ca_sqrt;
                                float dIp_dVpk = KS_12AT7 * ca_sqrt * (re * 1.5f + dre * ca);
                                float jac = 1.0f + RP_V4A * dIp_dVpk;
                                dIpA_dVk = -(dIp_dVgk * (1.0f - G_RIDE_A) + dIp_dVpk) / jac;
                                float residual = Ip_A - Ip_model;
                                if (std::abs(residual) < 1e-8f) break;
                                Ip_A = std::clamp(Ip_A - residual / jac, 0.0f, IpMax);
                            }
                        }

                        // --- V4B at fixed Vk ---
                        {
                            const float IpMax = std::max(0.0f, (BPLUS_J4 - Vk) / RP_V4B);
                            Ip_B = std::min(Ip_B, IpMax);
                            for (int nr = 0; nr < 8; ++nr)
                            {
                                float Vgk = v4b_grid - Vk;
                                float Vpk = BPLUS_J4 - Ip_B * RP_V4B - Vk;
                                float ca  = Vpk + MU_12AT7 * Vgk;
                                if (ca <= 0.0f) { Ip_B = 0.0f; dIpB_dVk = 0.0f; break; }
                                float re = 1.0f, dre = 0.0f;
                                if (Vpk < 5.0f) {
                                    if (Vpk <= 0.0f) { Ip_B = 0.0f; dIpB_dVk = 0.0f; break; }
                                    float t = Vpk * 0.2f;
                                    re = t * t * (3.0f - 2.0f * t);
                                    dre = 1.2f * t * (1.0f - t);
                                }
                                float ca_sqrt  = std::sqrt(ca);
                                float Ip_model = re * KS_12AT7 * ca * ca_sqrt;
                                float dIp_dVgk = re * KS_12AT7 * 1.5f * MU_12AT7 * ca_sqrt;
                                float dIp_dVpk = KS_12AT7 * ca_sqrt * (re * 1.5f + dre * ca);
                                float jac = 1.0f + RP_V4B * dIp_dVpk;
                                dIpB_dVk = -(dIp_dVgk + dIp_dVpk) / jac;
                                float residual = Ip_B - Ip_model;
                                if (std::abs(residual) < 1e-8f) break;
                                Ip_B = std::clamp(Ip_B - residual / jac, 0.0f, IpMax);
                            }
                        }

                        const float F = v4_Vk_q + R_TAIL * (Ip_A + Ip_B - iSumQ) - Vk;
#if ORB_OFFLINE_TOOLS
                        v4LastF = F;
#endif
                        if (std::abs(F) < 1e-4f) break;
                        // Bisection-guarded Newton: F is monotone decreasing in Vk,
                        // but raw Newton overshoots at the V4B cutoff corner (kinked
                        // dF) and needles the cathode. Bracket instead - never leaves
                        // [lo, hi], converges regardless of the kink.
                        if (F > 0.0f) lo = Vk; else hi = Vk;
                        const float dF = R_TAIL * (dIpA_dVk + dIpB_dVk) - 1.0f;
                        const float next = Vk - F / dF;
                        Vk = (next > lo && next < hi) ? next : 0.5f * (lo + hi);
                    }

                    const float dIsum = Ip_A + Ip_B - iSumQ;
#if ORB_OFFLINE_TOOLS
                    if (v4RidesOn)
#endif
                    {
                        // Grid A already rides JX1 through the in-solve G_RIDE_A
                        // term, so its LF ride is only the R98 drop - feeding it
                        // the full tail deviation double-counts the JX1 part and
                        // the 1.6 Hz pole integrates the excess into DC runaway.
                        v4aRide += v4RideCoeffA * (10000.0f * dIsum - v4aRide);
                        v4bRide += v4RideCoeffB * ((R_TAIL - R93_VAL) * dIsum - v4bRide);
                    }
#if ORB_OFFLINE_TOOLS
                    switch (v4DumpMode)   // 0.1x so the writer's +/-1 clamp clears
                    {
                        case 1: v4Dbg = 0.1f * (R_TAIL - R93_VAL) * dIsum; break;
                        case 2: v4Dbg = 0.1f * v4bRide; break;
                        case 3: v4Dbg = 0.1f * v4aRide; break;
                        case 4: v4Dbg = 0.1f * (Vk - v4_Vk_q); break;
                        case 5: v4Dbg = 0.1f * c58Out; break;
                        case 6: v4Dbg = 0.1f * nfbHpf; break;
                        case 7: v4Dbg = 0.1f * v4LastF; break;
                        default: break;
                    }
#endif
                }

                v4a_IpPr = Ip_A;
                v4b_IpPr = Ip_B;

                float v4a_plateAC = (BPLUS_J4 - Ip_A * RP_V4A) - v4a_Vp_q;
                float v4b_plateAC = (BPLUS_J4 - Ip_B * RP_V4B) - v4b_Vp_q;

#if ORB_OFFLINE_TOOLS
                if (c59Enabled)
                    applyC59Damper((ch == 0) ? c59DiffStateL : c59DiffStateR,
                                   c59LpfCoeff, v4a_plateAC, v4b_plateAC);
#endif

                //--------------------------------------------------------------
                // Stage 18d: V5/V6 EL34 push-pull
                //--------------------------------------------------------------
                float v5_gridAC = coupC61.processSample(v4a_plateAC, coupCapR_34Hz);
                float v6_gridAC = coupC62.processSample(v4b_plateAC, coupCapR_34Hz);

                float Ip_V5 = v5.processSample(v5_gridAC);
                float Ip_V6 = v6.processSample(v6_gridAC);

                float speakerOut = (Ip_V5 - Ip_V6) * speakerNormFactor;

                prevSpkr = speakerOut;

                s = speakerOut;
            }

#if ORB_OFFLINE_TOOLS
            if (v4DumpMode != 0) { samples[i] = v4Dbg; continue; }
#endif
            // Tap 8: after power amp (normalized domain)
            if (stageLimit == 8) { samples[i] = s; continue; }

            //------------------------------------------------------------------
            // Stage 19: Output transformer
            //------------------------------------------------------------------
            s = xfmrHpf.processSample(s);
            leakLpf += leakageLpfCoeff * (s - leakLpf);
            s = leakLpf;
            {
                float xfmrSat = (s >= 0.0f)
                    ? std::tanh(s * 1.2f)
                    : std::tanh(s * 1.0f);
                s = xfmrSat;
            }

            //------------------------------------------------------------------
            // Stage 20: DC blocker (7Hz HPF)
            //------------------------------------------------------------------
            s = dcBlock.processSample(s, dcBlockerR_coeff);

            samples[i] = s;
        }
    }

    ovLevelSmoothed.skip(numSamples);
    masterSmoothed.skip(numSamples);

    //------------------------------------------------------------------------
    // Downsample
    //------------------------------------------------------------------------
    oversampling->processSamplesDown(block);

    //------------------------------------------------------------------------
    // Cabinet IR convolution
    //------------------------------------------------------------------------
    if (stageLimit == 0)
    {
        if (cabinetTypeParam.load(std::memory_order_acquire) != kNoCabinet)
        {
            juce::dsp::AudioBlock<float> cabBlock(buffer);
            juce::dsp::ProcessContextReplacing<float> context(cabBlock);
            cabinetConvolution.process(context);
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const float g = cabMakeupGain.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer(ch)[i] *= g;
        }

        //--------------------------------------------------------------------
        // Mic position
        //--------------------------------------------------------------------
        {
            juce::dsp::AudioBlock<float> mBlock(buffer);
            juce::dsp::ProcessContextReplacing<float> context(mBlock);
            micPositionFilter.process(context);
        }
    }
}

//==============================================================================
// resetToDefaults
//==============================================================================
void AmpSimPlatinum::resetToDefaults()
{
    setGain(0.5f);
    setOvLevel(0.7f);
    setBass(0.5f);
    setMid(0.5f);
    setTreble(0.5f);
    setMaster(0.3f);
    setSpeakerDrive(0.3f);
    setGainMode(0);
    setCabinetType(0);
    setMicPosition(0.5f);
    setCabTrim(0.0f);
}

//==============================================================================
// Parameter setters
//==============================================================================
void AmpSimPlatinum::setGain(float value)
{
    gainParam = juce::jlimit(0.0f, 1.0f, value);
}

void AmpSimPlatinum::setOvLevel(float value)
{
    ovLevelParam = juce::jlimit(0.0f, 1.0f, value);
    ovLevelSmoothed.setTargetValue(ovLevelParam);
}

void AmpSimPlatinum::setBass(float value)
{
    bassParam = juce::jlimit(0.0f, 1.0f, value);
}

void AmpSimPlatinum::setMid(float value)
{
    midParam = juce::jlimit(0.0f, 1.0f, value);
}

void AmpSimPlatinum::setTreble(float value)
{
    trebleParam = juce::jlimit(0.0f, 1.0f, value);
}

void AmpSimPlatinum::setMaster(float value)
{
    masterParam = juce::jlimit(0.0f, 1.0f, value);
    masterSmoothed.setTargetValue(masterParam);
}

void AmpSimPlatinum::setSpeakerDrive(float value)
{
    speakerDriveParam = juce::jlimit(0.0f, 1.0f, value);
}

void AmpSimPlatinum::setGainMode(int mode)
{
    gainModeParam.store(juce::jlimit(0, 1, mode), std::memory_order_release);
}

void AmpSimPlatinum::setCabinetType(int type)
{
    cabinetTypeParam.store(juce::jlimit(0, kCustomCabinet, type), std::memory_order_release);
}

void AmpSimPlatinum::loadCustomIR(const juce::File& irFile)
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

void AmpSimPlatinum::setMicPosition(float value)
{
    micPositionParam = juce::jlimit(0.0f, 1.0f, value);
}

void AmpSimPlatinum::setCabTrim(float dB)
{
    cabTrimDb = juce::jlimit(-12.0f, 12.0f, dB);
    updateCabGainTarget();
}

//==============================================================================
// updateCabGainTarget
//==============================================================================
void AmpSimPlatinum::updateCabGainTarget()
{
    float trimGain = juce::Decibels::decibelsToGain(cabTrimDb);

    if (cabinetTypeParam.load(std::memory_order_acquire) == kNoCabinet)
        trimGain *= juce::Decibels::decibelsToGain(-8.0f);

    cabMakeupGain.setTargetValue(trimGain * juce::Decibels::decibelsToGain(kLevelMakeupDb));
}

//==============================================================================
// updateGainFilter
//==============================================================================
void AmpSimPlatinum::updateGainFilter()
{
#if ORB_OFFLINE_TOOLS
    if (diag.brightFix)
    {
        updateBrightNetworkCoeffs();
        return;
    }
#endif
    *gainFilterL.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        oversampledRate, 1500.0f, 0.707f,
        juce::Decibels::decibelsToGain(10.0f));
    *gainFilterR.coefficients = *gainFilterL.coefficients;
}

#if ORB_OFFLINE_TOOLS
//==============================================================================
// updateBrightNetworkCoeffs
//==============================================================================
// RG50TC GAIN1 pot network, wire-traced from the schematic (2026-07-04):
//   n1 --[R25 || (C20+R24)]-- n2,  n2 --[R26]-- gnd,  VR3 track n2 -> gnd,
//   C21 bridges n2 -> wiper, wiper feeds V1B grid directly (SW1 GAIN1 throw),
//   R28 grid leak. C22+R27 wiper->gnd branch dropped (>=10M at audio, <0.2 dB).
// gainParam is the wiper division fraction (linear), same semantics as the
// stock 0.242*gain scaling - the A1M taper remap is a separate user-facing
// change. Cin models V1B Miller input capacitance (physical tube parasitic,
// not on the schematic). kTrim re-anchors 120 Hz body drive to the stock
// model at g0.5-0.7 so bench operating points stay drive-comparable.
void AmpSimPlatinum::updateBrightNetworkCoeffs()
{
    constexpr double R24 = 47e3, R25 = 470e3, R26 = 150e3, R28 = 1e6, Rpot = 1e6;
    constexpr double C20 = 2.2e-9, C21 = 470e-12;
    constexpr double kTrim = 1.1018;

    const double a   = juce::jlimit(0.0, 0.99, static_cast<double>(gainParam));
    const double cin = static_cast<double>(diag.brightCinPf) * 1e-12;
    const double Gt  = 1.0 / (Rpot * (1.0 - a) + 1.0);
    const double Gb  = 1.0 / (Rpot * a + 1.0);
    const double G26 = 1.0 / R26;

    // Polynomials in s (ascending powers). With alpha/beta from
    // Ys = alpha/beta (the R25 || (C20+R24) series element):
    //   H = alpha*Ytop / [(alpha + beta*(G26 + Ytop))*(Ytop + Ybot) - beta*Ytop^2]
    const double al0 = 1.0,            al1 = C20 * (R24 + R25);
    const double be0 = R25,            be1 = R25 * C20 * R24;
    const double yt0 = Gt,             yt1 = C21;
    const double yb0 = Gb + 1.0 / R28, yb1 = cin;

    const double N0 = al0 * yt0;
    const double N1 = al0 * yt1 + al1 * yt0;
    const double N2 = al1 * yt1;

    const double t0 = al0 + be0 * G26 + be0 * yt0;
    const double t1 = al1 + be1 * G26 + be0 * yt1 + be1 * yt0;
    const double t2 = be1 * yt1;
    const double u0 = yt0 + yb0;
    const double u1 = yt1 + yb1;
    const double q0 = yt0 * yt0, q1 = 2.0 * yt0 * yt1, q2 = yt1 * yt1;

    const double D0 = t0 * u0           - be0 * q0;
    const double D1 = t0 * u1 + t1 * u0 - (be0 * q1 + be1 * q0);
    const double D2 = t1 * u1 + t2 * u0 - (be0 * q2 + be1 * q1);
    const double D3 = t2 * u1           - be1 * q2;

    // Bilinear transform at the oversampled rate.
    const double K  = 2.0 * oversampledRate;
    const double K2 = K * K;
    static constexpr double T[4][4] = {
        { 1.0,  3.0,  3.0,  1.0 },   // (z+1)^3
        { 1.0,  1.0, -1.0, -1.0 },   // (z-1)(z+1)^2
        { 1.0, -1.0, -1.0,  1.0 },   // (z-1)^2(z+1)
        { 1.0, -3.0,  3.0, -1.0 },   // (z-1)^3
    };
    const double n[4] = { N0, N1 * K, N2 * K2, 0.0 };
    const double d[4] = { D0, D1 * K, D2 * K2, D3 * K2 * K };
    double bz[4] = {}, az[4] = {};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
        {
            bz[j] += n[i] * T[i][j];
            az[j] += d[i] * T[i][j];
        }
    const double norm = 1.0 / az[0];
    for (int j = 0; j < 4; ++j)
        brightNetL.b[j] = static_cast<float>(kTrim * bz[j] * norm);
    for (int j = 1; j < 4; ++j)
        brightNetL.a[j - 1] = static_cast<float>(az[j] * norm);

    for (int j = 0; j < 4; ++j)
        brightNetR.b[j] = brightNetL.b[j];
    for (int j = 0; j < 3; ++j)
        brightNetR.a[j] = brightNetL.a[j];
}
#endif

//==============================================================================
// updateToneStackCoeffs
//==============================================================================
void AmpSimPlatinum::updateToneStackCoeffs()
{
    const double RIN = 1300.0;
    const double R1  = 33000.0;
    const double RT  = 200000.0;
    const double RB  = 1000000.0;
    const double RM  = 20000.0;
    const double RL  = 100000.0;
    const double C1  = 470e-12;
    const double C2  = 22e-9;
    const double C3  = 22e-9;

    double rotTreble = trebleParam * 10.0;
    double rotMid    = midParam    * 10.0;
    double rotBass   = bassParam   * 10.0;

    double rotTrebleTapered = rotTreble;
    double rotMidTapered    = rotMid;

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

    double DEN_AIm = RM1*RM2*RT1*RT2*C1*C2*C3 + RBv*RM2*RT1*RT2*C1*C2*C3
     + RIN*RM1*RT1*RT2*C1*C2*C3 + R1*RM1*RT1*RT2*C1*C2*C3
     + RBv*RIN*RT1*RT2*C1*C2*C3 + R1*RBv*RT1*RT2*C1*C2*C3
     + RL*RM1*RM2*RT2*C1*C2*C3 + RIN*RM1*RM2*RT2*C1*C2*C3
     + RBv*RL*RM2*RT2*C1*C2*C3 + RBv*RIN*RM2*RT2*C1*C2*C3
     + RIN*RL*RM1*RT2*C1*C2*C3 + R1*RL*RM1*RT2*C1*C2*C3
     + R1*RIN*RM1*RT2*C1*C2*C3 + RBv*RIN*RL*RT2*C1*C2*C3
     + R1*RBv*RL*RT2*C1*C2*C3 + R1*RBv*RIN*RT2*C1*C2*C3
     + RL*RM1*RM2*RT1*C1*C2*C3 + RIN*RM1*RM2*RT1*C1*C2*C3
     + R1*RM1*RM2*RT1*C1*C2*C3 + RBv*RL*RM2*RT1*C1*C2*C3
     + RBv*RIN*RM2*RT1*C1*C2*C3 + R1*RBv*RM2*RT1*C1*C2*C3
     + RIN*RL*RM1*RT1*C1*C2*C3 + R1*RL*RM1*RT1*C1*C2*C3
     + RBv*RIN*RL*RT1*C1*C2*C3 + R1*RBv*RL*RT1*C1*C2*C3
     + R1*RL*RM1*RM2*C1*C2*C3 + R1*RIN*RM1*RM2*C1*C2*C3
     + R1*RBv*RL*RM2*C1*C2*C3 + R1*RBv*RIN*RM2*C1*C2*C3
     + R1*RIN*RL*RM1*C1*C2*C3 + R1*RBv*RIN*RL*C1*C2*C3;

    double DEN_BRe = RM2*RT1*RT2*C1*C2 + RM1*RT1*RT2*C1*C2 + RIN*RT1*RT2*C1*C2
     + RBv*RT1*RT2*C1*C2 + R1*RT1*RT2*C1*C2 + RL*RM2*RT2*C1*C2
     + RIN*RM2*RT2*C1*C2 + RL*RM1*RT2*C1*C2 + RIN*RM1*RT2*C1*C2
     + RIN*RL*RT2*C1*C2 + RBv*RL*RT2*C1*C2 + R1*RL*RT2*C1*C2
     + RBv*RIN*RT2*C1*C2 + R1*RIN*RT2*C1*C2 + RL*RM2*RT1*C1*C2
     + RIN*RM2*RT1*C1*C2 + R1*RM2*RT1*C1*C2 + RL*RM1*RT1*C1*C2
     + RIN*RM1*RT1*C1*C2 + R1*RM1*RT1*C1*C2 + RIN*RL*RT1*C1*C2
     + RBv*RL*RT1*C1*C2 + R1*RL*RT1*C1*C2 + RBv*RIN*RT1*C1*C2
     + R1*RBv*RT1*C1*C2 + R1*RL*RM2*C1*C2 + R1*RIN*RM2*C1*C2
     + R1*RL*RM1*C1*C2 + R1*RIN*RM1*C1*C2 + R1*RIN*RL*C1*C2
     + R1*RBv*RL*C1*C2 + R1*RBv*RIN*C1*C2
     + RM1*RM2*RT2*C2*C3 + RBv*RM2*RT2*C2*C3 + RIN*RM1*RT2*C2*C3
     + R1*RM1*RT2*C2*C3 + RBv*RIN*RT2*C2*C3 + R1*RBv*RT2*C2*C3
     + RL*RM1*RM2*C2*C3 + RIN*RM1*RM2*C2*C3 + R1*RM1*RM2*C2*C3
     + RBv*RL*RM2*C2*C3 + RBv*RIN*RM2*C2*C3 + R1*RBv*RM2*C2*C3
     + RIN*RL*RM1*C2*C3 + R1*RL*RM1*C2*C3 + RBv*RIN*RL*C2*C3
     + R1*RBv*RL*C2*C3
     + RM2*RT1*RT2*C1*C3 + RIN*RT1*RT2*C1*C3 + R1*RT1*RT2*C1*C3
     + RL*RM2*RT2*C1*C3 + RIN*RM2*RT2*C1*C3 + RIN*RL*RT2*C1*C3
     + R1*RL*RT2*C1*C3 + R1*RIN*RT2*C1*C3
     + RM1*RM2*RT1*C1*C3 + RL*RM2*RT1*C1*C3 + RIN*RM2*RT1*C1*C3
     + RBv*RM2*RT1*C1*C3 + R1*RM2*RT1*C1*C3 + RIN*RM1*RT1*C1*C3
     + R1*RM1*RT1*C1*C3 + RIN*RL*RT1*C1*C3 + R1*RL*RT1*C1*C3
     + RBv*RIN*RT1*C1*C3 + R1*RBv*RT1*C1*C3
     + RL*RM1*RM2*C1*C3 + RIN*RM1*RM2*C1*C3 + RBv*RL*RM2*C1*C3
     + R1*RL*RM2*C1*C3 + RBv*RIN*RM2*C1*C3 + R1*RIN*RM2*C1*C3
     + RIN*RL*RM1*C1*C3 + R1*RL*RM1*C1*C3 + R1*RIN*RM1*C1*C3
     + RBv*RIN*RL*C1*C3 + R1*RIN*RL*C1*C3 + R1*RBv*RL*C1*C3
     + R1*RBv*RIN*C1*C3;

    double DEN_CIm = RM2*RT2*C2 + RM1*RT2*C2 + RIN*RT2*C2 + RBv*RT2*C2 + R1*RT2*C2
     + RL*RM2*C2 + RIN*RM2*C2 + R1*RM2*C2 + RL*RM1*C2 + RIN*RM1*C2
     + R1*RM1*C2 + RIN*RL*C2 + RBv*RL*C2 + R1*RL*C2 + RBv*RIN*C2
     + R1*RBv*C2 + RT1*RT2*C1 + RL*RT2*C1 + RIN*RT2*C1
     + RM2*RT1*C1 + RM1*RT1*C1 + RL*RT1*C1 + RBv*RT1*C1
     + RL*RM2*C1 + RIN*RM2*C1 + RL*RM1*C1 + RIN*RM1*C1
     + RIN*RL*C1 + RBv*RL*C1 + RBv*RIN*C1
     + RM2*RT2*C3 + RIN*RT2*C3 + R1*RT2*C3
     + RM1*RM2*C3 + RL*RM2*C3 + RIN*RM2*C3 + RBv*RM2*C3 + R1*RM2*C3
     + RIN*RM1*C3 + R1*RM1*C3 + RIN*RL*C3 + R1*RL*C3
     + RBv*RIN*C3 + R1*RBv*C3;

    double DEN_DRe = RT2 + RM2 + RM1 + RL + RBv;

    double NOM_AIm = C1*C2*C3*RL*RM1*RM2*RT2 + C1*C2*C3*RBv*RL*RM2*RT2
     + C1*C2*C3*R1*RL*RM1*RT2 + C1*C2*C3*R1*RBv*RL*RT2
     + C1*C2*C3*RL*RM1*RM2*RT1 + C1*C2*C3*RBv*RL*RM2*RT1
     + C1*C2*C3*R1*RL*RM1*RM2 + C1*C2*C3*R1*RBv*RL*RM2;

    double NOM_BRe = C1*C3*RL*RM2*RT2 + C1*C2*RL*RM2*RT2 + C1*C2*RL*RM1*RT2
     + C1*C2*RBv*RL*RT2 + C1*C3*R1*RL*RT2 + C1*C2*R1*RL*RT2
     + C1*C3*RL*RM2*RT1 + C1*C2*RL*RM2*RT1 + C1*C2*RL*RM1*RT1
     + C1*C2*RBv*RL*RT1 + C2*C3*RL*RM1*RM2 + C1*C3*RL*RM1*RM2
     + C2*C3*RBv*RL*RM2 + C1*C3*RBv*RL*RM2 + C1*C3*R1*RL*RM2
     + C1*C2*R1*RL*RM2 + C1*C3*R1*RL*RM1 + C1*C2*R1*RL*RM1
     + C1*C3*R1*RBv*RL + C1*C2*R1*RBv*RL;

    double NOM_CIm = C1*RL*RT2 + C3*RL*RM2 + C2*RL*RM2 + C1*RL*RM2
     + C2*RL*RM1 + C1*RL*RM1 + C2*RBv*RL + C1*RBv*RL;

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

    toneStackL.b[0] = nb0; toneStackL.b[1] = nb1;
    toneStackL.b[2] = nb2; toneStackL.b[3] = nb3;
    toneStackL.a[0] = na1; toneStackL.a[1] = na2; toneStackL.a[2] = na3;

    toneStackR.b[0] = nb0; toneStackR.b[1] = nb1;
    toneStackR.b[2] = nb2; toneStackR.b[3] = nb3;
    toneStackR.a[0] = na1; toneStackR.a[1] = na2; toneStackR.a[2] = na3;
}

//==============================================================================
// updateCabinet
//==============================================================================
void AmpSimPlatinum::updateCabinet()
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
        0);
}

//==============================================================================
// updateMicPositionFilter
//==============================================================================
void AmpSimPlatinum::updateMicPositionFilter()
{
    float db = -4.0f + 8.0f * micPositionParam;
    *micPositionFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        currentSampleRate, 1500.0f, 0.707f, juce::Decibels::decibelsToGain(db));
}

#if ORB_OFFLINE_TOOLS
bool AmpSimPlatinum::setDiagnostic(const juce::String& key, float value)
{
    if (key == "osfactor")
    {
        const int f = static_cast<int>(value);
        if (f != 2 && f != 4 && f != 8 && f != 16 && f != 32)
            return false;
        int stages = 0;
        for (int v = f; v > 1; v >>= 1)
            ++stages;
        diag.osStages = stages;
        return true;
    }
    if (key == "osfir") { diag.osFir = value >= 0.5f; return true; }
    if (key == "noiselevel")
    {
        if (value < 0.0f || value > 1.0f)
            return false;
        noiseInjectLevel = value;
        return true;
    }
    if (key == "brightfix") { diag.brightFix = value >= 0.5f; return true; }
    if (key == "brightcin")
    {
        if (value < 0.0f || value > 1000.0f)
            return false;
        diag.brightCinPf = value;
        return true;
    }
    if (key == "c59") { diag.c59 = value >= 0.5f; return true; }
    if (key == "v4tail") { diag.v4tail = value >= 0.5f; return true; }
    if (key == "v4outers")
    {
        const int n = static_cast<int>(value);
        if (n < 1 || n > 32) return false;
        diag.v4outers = n;
        return true;
    }
    if (key == "v4rides") { diag.v4rides = value >= 0.5f; return true; }
    if (key == "v4nfb")
    {
        if (value < 0.0f || value > 2.0f) return false;
        diag.v4nfb = value;
        return true;
    }
    if (key == "v4dump")
    {
        const int m = static_cast<int>(value);
        if (m < 0 || m > 7) return false;
        diag.v4dump = m;
        return true;
    }
    return false;
}
#endif
