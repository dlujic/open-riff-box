#pragma once

#include <cmath>
#include <algorithm>

class PentodeStage
{
public:
    PentodeStage() = default;

    //==========================================================================
    // Circuit configuration
    //==========================================================================
    struct Config
    {
        float Rp_dc     = 50.0f;     // DC plate load -- transformer DCR (ohms)
        float Rp_signal = 848.0f;    // AC plate load -- reflected impedance (ohms)
        float Rk        = 1.0f;      // Cathode resistor (ohms)
        float Bplus     = 439.0f;    // Supply voltage (volts)
        float Vg2k      = 424.7f;    // Fixed screen-cathode voltage (volts)
        float Vbias     = -42.0f;    // Grid bias voltage (volts)

        // Koren pentode parameters (fitted for EL34, fit_el34_params.py)
        float MU  = 8.12f;
        float EX  = 1.0f;           // EX=1.0 eliminates pow()
        float KG1 = 373.9f;
        float KP  = 55.47f;
        float KVB = 52.71f;
    };

    void configure(const Config& cfg);

    void reset();

    float processSample(float Vg1_ac);

    //==========================================================================
    // Operating point info
    //==========================================================================
    float getQuiescentIp() const { return Ip_q; }
    float getQuiescentVp() const { return Vp_q; }
    float getQuiescentVk() const { return Vk_q; }

private:
    Config config {};

    //==========================================================================
    // Quiescent operating point (computed in configure)
    //==========================================================================
    float Ip_q          = 0.0f;     // Quiescent plate current (amps)
    float Vp_q          = 0.0f;     // Quiescent plate voltage (volts)
    float Vk_q          = 0.0f;     // Quiescent cathode voltage (volts)
    float Bplus_virtual = 0.0f;     // Virtual B+ for signal NR solve

    //==========================================================================
    // State variables
    //==========================================================================
    float Ip_prev = 0.0f;           // Previous plate current (NR initial guess)

    //==========================================================================
    // Cached constants (computed once in configure)
    //==========================================================================
    float Rp_total = 0.0f;          // Rp_signal + Rk
    float invKG1   = 0.0f;
    float invKVB   = 0.0f;
    float KVB_sq   = 0.0f;
    float invMU    = 0.0f;
    float Vg2k_KP  = 0.0f;         // Vg2k / KP (for E1 computation)

    //==========================================================================
    // Newton-Raphson
    //==========================================================================
    static constexpr int   kMaxNR  = 8;
    static constexpr float kNRTol  = 1e-10f;

    // Koren E1 (effective voltage) + sigmoid for Jacobian.
    //
    // E1 = (Vg2k/KP) * ln(1 + exp(u))
    // sigma = exp(u) / (1 + exp(u))
    // where u = KP * (1/MU + Vg1k/Vg2k)
    //
    // sigma = dE1/dVg1k, used in the Jacobian for NR convergence.
    inline void korenE1(float Vg1k, float& E1, float& sigma) const
    {
        float u = config.KP * (invMU + Vg1k / std::max(config.Vg2k, 1.0f));
        u = std::clamp(u, -50.0f, 50.0f);
        float exp_u = std::exp(u);
        sigma = exp_u / (1.0f + exp_u);
        E1 = Vg2k_KP * std::log(1.0f + exp_u);
    }

    // Solve quiescent Ip via NR using DC plate load.
    float solveQuiescent() const;
};
