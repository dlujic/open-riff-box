#pragma once

#include <cmath>
#include <algorithm>

class TriodeStage
{
public:
    TriodeStage() = default;

    //==========================================================================
    // Circuit configuration
    //==========================================================================
    struct Config
    {
        float Rp     = 100000.0f;  // Plate load resistor (ohms)
        float Rk     = 1000.0f;    // Cathode resistor (ohms)
        float Ck     = 0.0f;       // Cathode bypass cap (farads), 0 = unbypassed
        float Cp     = 0.0f;       // Plate cap in parallel with Rp (farads), 0 = none
        float Bplus  = 300.0f;     // Supply voltage (volts)

        // Tube parameters (Munro/Piazza model):
        //   ca = offset + Vpk + mu * Vgk
        //   Ip = re * Ks * max(0, ca)^Ex    [amps]
        //   re = clamp(Vpk / 5, 0, 1)       [low-voltage reduction]
        float mu     = 95.43f;     // Amplification factor (12AX7=95.43, 12AT7~60)
        float Ks     = 1.147e-6f;  // Current coefficient (amps / volt^Ex)
        float Ex     = 1.5f;       // Exponent
        float offset = 45.0f;      // Effective voltage offset (shifts operating region)
    };

    void configure(const Config& cfg);

    void prepare(double sampleRate);

    void reset();

    float processSample(float Vg_ac);

    //==========================================================================
    // Operating point info (useful for debugging / normalization)
    //==========================================================================
    float getQuiescentIp()   const { return Ip_q; }
    float getQuiescentVp()   const { return Vp_q; }
    float getQuiescentVk()   const { return Vk_q; }
    float getSmallSignalGain() const { return smallSignalGain; }

    // Maximum expected AC plate swing (volts, peak). Used for normalization
    // by AmpSimPlatinum when converting between stages.
    float getOutputSwing() const { return outputSwing; }

    //==========================================================================
    // Runtime cathode cap switching (for GAIN1/GAIN2 mode change)
    //==========================================================================

    // Switch cathode bypass cap value at runtime (e.g. GAIN1 full bypass
    // vs GAIN2 partial bypass through a series resistor).
    // newCk: new capacitance in farads (0 = unbypassed)
    // newRk: new effective cathode resistance (if the bypass path changes Rk)
    void setCathodeCap(float newCk, float newRk);

private:
    Config config {};

    //==========================================================================
    // Quiescent operating point (computed in configure)
    //==========================================================================
    float Ip_q           = 0.0f;   // Quiescent plate current (amps)
    float Vp_q           = 0.0f;   // Quiescent plate voltage (volts)
    float Vk_q           = 0.0f;   // Quiescent cathode voltage (volts)
    float smallSignalGain = 0.0f;  // Approximate voltage gain (negative for inverting)
    float outputSwing    = 1.0f;   // Max AC plate swing (volts, for normalization)

    //==========================================================================
    // State variables
    //==========================================================================
    float Vck     = 0.0f;   // Cathode bypass cap voltage (volts)
    float Vpc     = 0.0f;   // Plate cap voltage (volts), only used if Cp > 0
    float Ip_prev = 0.0f;   // Previous plate current (NR initial guess)
    float Ik_prev = 0.0f;   // Previous net cathode current (trapezoidal rule)

    //==========================================================================
    // Sample period
    //==========================================================================
    float T = 1.0f / 192000.0f;

    //==========================================================================
    // Flags
    //==========================================================================
    bool hasCathodeCap = false;
    bool hasPlateCap   = false;

    //==========================================================================
    // Newton-Raphson
    //==========================================================================
    static constexpr int   kMaxNR  = 8;
    static constexpr float kNRTol  = 1e-10f;

    // Low-voltage reduction factor (Munro model).
    // Smoothstep version of clamp(Vpk/5, 0, 1) -- zero derivative at both
    // endpoints prevents NR oscillation at the saturation boundary.
    // For Vpk >= 5V (all normal operating points), returns exactly 1.0.
    static inline float reductionFactor(float Vpk)
    {
        if (Vpk <= 0.0f) return 0.0f;
        if (Vpk >= 5.0f) return 1.0f;
        float t = Vpk * 0.2f;  // t = Vpk / 5
        return t * t * (3.0f - 2.0f * t);  // Hermite smoothstep
    }

    // Reduction factor + its derivative w.r.t. Vpk (for Jacobian).
    static inline void reductionFactorWithDerivative(float Vpk, float& re, float& dre)
    {
        if (Vpk <= 0.0f)      { re = 0.0f; dre = 0.0f; return; }
        if (Vpk >= 5.0f)      { re = 1.0f; dre = 0.0f; return; }
        float t = Vpk * 0.2f;
        re  = t * t * (3.0f - 2.0f * t);
        dre = 1.2f * t * (1.0f - t);  // d(smoothstep)/dVpk = 6t(1-t)/5
    }

    // Munro/Piazza tube model:
    //   ca = offset + Vpk + mu * Vgk
    //   re = smoothstep(Vpk / 5)   [low-voltage reduction, re=1 for Vpk >= 5V]
    //   Ip = re * Ks * max(0, ca)^Ex
    //
    // The original Munro model uses re = clamp(Vpk/5, 0, 1) which has a derivative
    // discontinuity at Vpk=0 and Vpk=5. This causes NR oscillation when the tube
    // is driven into saturation (Vpk near 0). Smoothstep gives identical behavior
    // at normal operating points (Vpk >> 5V) but provides a smooth gradient through
    // the saturation region so the NR solver can converge.
    //
    // Returns plate current in amps.
    // Softened cutoff knee for the Munro tube model.
    //
    // The original model uses max(0, ca)^1.5 which has infinite curvature
    // at ca=0 (d2Ip/dca2 -> infinity). With 5 cascaded stages, each sharp
    // knee generates broadband intermodulation that sounds grainy.
    //
    // Real tubes have residual leakage current below cutoff, creating a
    // smooth knee. We model this with a soft-minimum:
    //   ca_soft = 0.5 * (ca + sqrt(ca^2 + k^2))
    // At ca >> k: ca_soft ~ ca (unchanged). At ca = 0: ca_soft = k/2.
    // At ca << -k: ca_soft ~ k^2/(4*|ca|) (smooth decay to zero).
    //
    // k = kKneeWidth controls knee softness. k=0 recovers the hard model.
    static constexpr float kKneeWidth = 2.0f;

    inline void softenedCa(float ca_raw, float& ca_soft, float& dsoft_draw) const
    {
        float disc = std::sqrt(ca_raw * ca_raw + kKneeWidth * kKneeWidth);
        ca_soft = 0.5f * (ca_raw + disc);
        dsoft_draw = 0.5f * (1.0f + ca_raw / disc);  // dca_soft / dca_raw
    }

    // Ip = re * Ks * ca_soft^1.5
    inline float tubeIp(float Vgk, float Vpk) const
    {
        float ca_raw = config.offset + Vpk + config.mu * Vgk;
        float ca_s, ds;
        softenedCa(ca_raw, ca_s, ds);
        if (ca_s <= 0.0f) return 0.0f;
        float re = reductionFactor(Vpk);
        return re * config.Ks * ca_s * std::sqrt(ca_s);
    }

    // dIp/dVgk = re * Ks * 1.5 * mu * sqrt(ca_soft) * dca_soft/dca
    inline float tubeJacobianVgk(float Vgk, float Vpk) const
    {
        float ca_raw = config.offset + Vpk + config.mu * Vgk;
        float ca_s, ds;
        softenedCa(ca_raw, ca_s, ds);
        if (ca_s <= 0.0f) return 0.0f;
        float re = reductionFactor(Vpk);
        return re * config.Ks * 1.5f * config.mu * std::sqrt(ca_s) * ds;
    }

    // dIp/dVpk = Ks * sqrt(ca_soft) * (re * 1.5 * ds + dre * ca_soft)
    inline float tubeJacobianVpk(float Vgk, float Vpk) const
    {
        float ca_raw = config.offset + Vpk + config.mu * Vgk;
        float ca_s, ds;
        softenedCa(ca_raw, ca_s, ds);
        if (ca_s <= 0.0f) return 0.0f;

        float re, dre;
        reductionFactorWithDerivative(Vpk, re, dre);

        float ca_s_sqrt = std::sqrt(ca_s);
        return config.Ks * ca_s_sqrt * (re * 1.5f * ds + dre * ca_s);
    }

    // Solve quiescent Ip: Ip = childLaw(-Ip*Rk, Bplus - Ip*(Rp+Rk))
    float solveQuiescent() const;
};
