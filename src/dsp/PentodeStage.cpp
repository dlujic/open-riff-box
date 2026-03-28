#include "PentodeStage.h"

//==============================================================================
void PentodeStage::configure(const Config& cfg)
{
    config = cfg;

    // Cache constants
    Rp_total = cfg.Rp_signal + cfg.Rk;
    invKG1   = 1.0f / cfg.KG1;
    invKVB   = 1.0f / cfg.KVB;
    KVB_sq   = cfg.KVB * cfg.KVB;
    invMU    = 1.0f / cfg.MU;
    Vg2k_KP  = std::max(cfg.Vg2k, 1.0f) / cfg.KP;

    // Compute quiescent operating point using DC plate load
    Ip_q = solveQuiescent();
    Vk_q = Ip_q * cfg.Rk;
    Vp_q = cfg.Bplus - Ip_q * cfg.Rp_dc;

    // Virtual B+ for signal processing.
    //
    // At DC, the plate sees only transformer DCR (Rp_dc ~50 ohms).
    // At signal frequencies, the plate sees reflected impedance (Rp_signal ~848 ohms).
    //
    // The signal NR solve uses: Vpk = Bplus_virtual - Ip * Rp_total
    // At quiescent, this must give the correct Vpk_q:
    //   Vpk_q = Bplus_virtual - Ip_q * Rp_total
    //   Bplus_virtual = Vpk_q + Ip_q * Rp_total
    //                 = (Vp_q - Vk_q) + Ip_q * (Rp_signal + Rk)
    float Vpk_q = Vp_q - Vk_q;
    Bplus_virtual = Vpk_q + Ip_q * Rp_total;

    reset();
}

//==============================================================================
void PentodeStage::reset()
{
    Ip_prev = Ip_q;
}

//==============================================================================
float PentodeStage::processSample(float Vg1_ac)
{
    float Ip = Ip_prev;

    // Total grid voltage (bias + AC signal)
    const float Vg1_total = config.Vbias + Vg1_ac;

    // Physical current limit: Ip cannot exceed what makes Vpk = 0
    const float IpMax = std::max(0.0f, Bplus_virtual / Rp_total);

    for (int iter = 0; iter < kMaxNR; ++iter)
    {
        const float Vk   = Ip * config.Rk;
        const float Vg1k = Vg1_total - Vk;
        const float Vpk  = Bplus_virtual - Ip * Rp_total;

        // Compute E1 and sigmoid
        float E1, sigma;
        korenE1(Vg1k, E1, sigma);

        // Plate current from model
        float Ip_model = 0.0f;
        if (E1 > 0.0f && Vpk > 0.0f)
        {
            // Ip = E1 / KG1 * atan(Vpk / KVB)   [EX=1.0, no pow]
            Ip_model = E1 * invKG1 * std::atan(Vpk * invKVB);
        }

        const float residual = Ip - Ip_model;
        if (std::abs(residual) < kNRTol)
            break;

        // Jacobian: df/dIp = 1 + Rk * dIp/dVg1k + Rp_total * dIp/dVpk
        //
        // dIp/dVg1k = (1/KG1) * atan(Vpk/KVB) * sigma
        // dIp/dVpk  = E1/KG1 * KVB / (KVB^2 + Vpk^2)
        float jac = 1.0f;
        if (E1 > 0.0f && Vpk > 0.0f)
        {
            const float atan_term = std::atan(Vpk * invKVB);
            const float dIp_dVg1k = invKG1 * atan_term * sigma;
            const float dIp_dVpk  = E1 * invKG1 * config.KVB / (KVB_sq + Vpk * Vpk);
            jac += config.Rk * dIp_dVg1k + Rp_total * dIp_dVpk;
        }

        if (std::abs(jac) < 1e-12f) break;

        Ip -= residual / jac;
        Ip = std::clamp(Ip, 0.0f, IpMax);
    }

    Ip_prev = Ip;
    return Ip;
}

//==============================================================================
float PentodeStage::solveQuiescent() const
{
    // DC operating point: grid at Vbias, screen at Vg2k.
    //
    // At DC, the plate sees only the transformer DCR (Rp_dc ~ 50 ohms).
    //   Vk    = Ip * Rk
    //   Vg1k  = Vbias - Vk
    //   Vpk   = Bplus - Ip * (Rp_dc + Rk)
    //   Ip    = korenIp(Vg1k, Vg2k, Vpk)
    //
    // Expected result for EL34: Ip_q ~ 46.7 mA

    const float Rp_dc_total = config.Rp_dc + config.Rk;
    const float Vg2k_safe = std::max(config.Vg2k, 1.0f);

    float Ip = 0.050f;  // Initial guess: 50 mA (typical EL34)

    for (int i = 0; i < 50; ++i)
    {
        const float Vk   = Ip * config.Rk;
        const float Vg1k = config.Vbias - Vk;
        const float Vpk  = config.Bplus - Ip * Rp_dc_total;

        if (Vpk < 0.0f)
        {
            Ip *= 0.5f;
            continue;
        }

        // Compute E1 and sigmoid
        float u = config.KP * (1.0f / config.MU + Vg1k / Vg2k_safe);
        u = std::clamp(u, -50.0f, 50.0f);
        float exp_u = std::exp(u);
        float sigma = exp_u / (1.0f + exp_u);
        float E1 = (Vg2k_safe / config.KP) * std::log(1.0f + exp_u);

        if (E1 <= 0.0f)
        {
            Ip *= 0.5f;
            continue;
        }

        // Ip from model (EX=1.0, no pow)
        float atan_term = std::atan(Vpk / config.KVB);
        float Ip_model = E1 / config.KG1 * atan_term;
        float residual = Ip - Ip_model;

        if (std::abs(residual) < 1e-10f)
            break;

        // Jacobian: df/dIp = 1 - dIp_model/dIp
        // dVg1k/dIp = -Rk, dVpk/dIp = -Rp_dc_total
        float dIp_dVg1k = (1.0f / config.KG1) * atan_term * sigma;
        float dIp_dVpk  = E1 / config.KG1 * config.KVB
                         / (config.KVB * config.KVB + Vpk * Vpk);

        float dIp_model_dIp = -config.Rk * dIp_dVg1k - Rp_dc_total * dIp_dVpk;
        float jac = 1.0f - dIp_model_dIp;

        if (std::abs(jac) < 1e-12f) break;

        Ip -= residual / jac;
        if (Ip < 0.0f) Ip = 1e-6f;
    }

    return Ip;
}
