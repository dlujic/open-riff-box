#include "TriodeStage.h"

//==============================================================================
void TriodeStage::configure(const Config& cfg)
{
    config = cfg;
    hasCathodeCap = (cfg.Ck > 0.0f);
    hasPlateCap   = (cfg.Cp > 0.0f);

    // Compute quiescent operating point by solving the DC load line
    Ip_q = solveQuiescent();
    Vk_q = Ip_q * config.Rk;
    Vp_q = config.Bplus - Ip_q * config.Rp;

    // Compute small-signal gain from transconductance at operating point.
    // gm = dIp/dVgk at the quiescent point.
    float Vgk_q = -Vk_q;                         // grid at 0V, cathode at Vk_q
    float Vpk_q = Vp_q - Vk_q;

    float gm = tubeJacobianVgk(Vgk_q, Vpk_q);

    // Dynamic plate resistance: rp = mu / gm
    float rp = (gm > 1e-9f) ? config.mu / gm : 1e9f;

    if (hasCathodeCap)
    {
        // Bypassed cathode at audio frequencies: Av = -mu * Rp / (rp + Rp)
        smallSignalGain = -config.mu * config.Rp / (rp + config.Rp);
    }
    else
    {
        // Unbypassed: Av = -mu * Rp / (rp + Rp + (mu+1)*Rk)
        smallSignalGain = -config.mu * config.Rp
            / (rp + config.Rp + (config.mu + 1.0f) * config.Rk);
    }

    // Maximum plate swing: from quiescent down to near 0V, and up to Bplus.
    // The useful swing is limited by cutoff (plate rises to Bplus when Ip=0)
    // and saturation (plate drops toward cathode).
    outputSwing = std::max(Vp_q, config.Bplus - Vp_q);
    if (outputSwing < 1.0f) outputSwing = 1.0f;

    // Initialize state to quiescent
    reset();
}

//==============================================================================
void TriodeStage::prepare(double sampleRate)
{
    T = static_cast<float>(1.0 / sampleRate);
}

//==============================================================================
void TriodeStage::reset()
{
    Vck     = Vk_q;    // Cathode cap charged to quiescent cathode voltage
    Vpc     = Vp_q;    // Plate cap at quiescent plate voltage
    Ip_prev = Ip_q;    // NR initial guess at quiescent
    Ik_prev = 0.0f;    // Net cap current at quiescent = 0 (steady state)
}

//==============================================================================
float TriodeStage::processSample(float Vg_ac)
{
    float Ip = Ip_prev;  // Previous sample = excellent initial guess at 192 kHz

    if (hasCathodeCap)
    {
        //==================================================================
        // BYPASSED CATHODE: Vck is a state variable (cap voltage).
        // Vck is fixed during the NR solve (from previous sample's state).
        // The cap evolves slowly (Rk*Ck >> sample period), so the one-sample
        // lag is negligible.
        //
        // Implicit equation:
        //   f(Ip) = Ip - tubeIp(Vg_ac - Vck, Bplus - Ip*Rp - Vck)
        // Jacobian:
        //   df/dIp = 1 + Rp * dIp/dVpk
        //==================================================================
        const float Vgk = Vg_ac - Vck;

        // Physical current limit: Ip cannot exceed what makes Vpk = 0
        // (plate at cathode potential = maximum conduction, all B+ across Rp).
        const float IpMaxBypassed = std::max(0.0f, (config.Bplus - Vck) / config.Rp);

        for (int iter = 0; iter < kMaxNR; ++iter)
        {
            const float Vpk      = config.Bplus - Ip * config.Rp - Vck;
            const float Ip_model = tubeIp(Vgk, Vpk);
            const float residual = Ip - Ip_model;

            if (std::abs(residual) < kNRTol)
                break;

            const float jac = 1.0f + config.Rp * tubeJacobianVpk(Vgk, Vpk);
            if (std::abs(jac) < 1e-12f) break;

            Ip -= residual / jac;
            Ip = std::clamp(Ip, 0.0f, IpMaxBypassed);
        }

        // Update cathode cap state (trapezoidal rule)
        const float Ik = Ip - Vck / config.Rk;
        Vck += (T / (2.0f * config.Ck)) * (Ik + Ik_prev);
        Ik_prev = Ik;

        // Safety clamp
        Vck = std::max(0.0f, std::min(Vck, config.Bplus * 0.1f));
    }
    else
    {
        //==================================================================
        // UNBYPASSED CATHODE: Vck = Ip * Rk instantaneously.
        // The cathode degeneration MUST be inside the NR loop, otherwise
        // Vck oscillates wildly between samples (conducts hard -> cathode
        // jumps high -> cutoff -> cathode drops to zero -> repeat).
        //
        // Implicit equation (cathode coupled into the solve):
        //   Vgk(Ip) = Vg_ac - Ip*Rk
        //   Vpk(Ip) = Bplus - Ip*(Rp + Rk)
        //   f(Ip) = Ip - tubeIp(Vg_ac - Ip*Rk, Bplus - Ip*(Rp+Rk))
        //
        // Jacobian:
        //   df/dIp = 1 + Rk * dIp/dVgk + (Rp+Rk) * dIp/dVpk
        //==================================================================
        const float RpRk = config.Rp + config.Rk;

        // Physical current limit: Ip cannot exceed what makes Vpk = 0
        // (all B+ dropped across Rp+Rk).
        const float IpMaxUnbypassed = std::max(0.0f, config.Bplus / RpRk);

        for (int iter = 0; iter < kMaxNR; ++iter)
        {
            const float Vgk      = Vg_ac - Ip * config.Rk;
            const float Vpk      = config.Bplus - Ip * RpRk;
            const float Ip_model = tubeIp(Vgk, Vpk);
            const float residual = Ip - Ip_model;

            if (std::abs(residual) < kNRTol)
                break;

            const float dIp_dVgk = tubeJacobianVgk(Vgk, Vpk);
            const float dIp_dVpk = tubeJacobianVpk(Vgk, Vpk);
            const float jac = 1.0f
                + config.Rk * dIp_dVgk     // Rk * dIp/dVgk
                + RpRk * dIp_dVpk;          // (Rp+Rk) * dIp/dVpk
            if (std::abs(jac) < 1e-12f) break;

            Ip -= residual / jac;
            Ip = std::clamp(Ip, 0.0f, IpMaxUnbypassed);
        }

        // Cathode tracks instantly (no cap)
        Vck = Ip * config.Rk;
    }

    // Compute plate voltage from converged Ip
    float Vp = config.Bplus - Ip * config.Rp;

    if (hasPlateCap)
    {
        // Plate cap (Cp) in parallel with plate load Rp creates a pole
        // on the plate voltage. Models V1B's 470pF plate cap (3.4 kHz pole).
        //
        // The cap voltage Vpc lags the instantaneous plate voltage:
        //   Cp * dVpc/dt = (Vp - Vpc) / Rp_eff
        //
        // Using a one-pole with trapezoidal coefficient:
        //   a = T / (2*Rp*Cp + T)
        //   Vpc = Vpc + a * (Vp - Vpc)

        const float a = T / (2.0f * config.Rp * config.Cp + T);
        Vpc += a * (Vp - Vpc);
        Vp = Vpc;  // Output uses the filtered (cap-smoothed) plate voltage
    }

    Ip_prev = Ip;

    // Output: AC plate voltage swing relative to quiescent.
    // Real tube stage is inverting: positive grid signal -> lower plate voltage.
    return Vp - Vp_q;
}

//==============================================================================
void TriodeStage::setCathodeCap(float newCk, float newRk)
{
    config.Ck = newCk;
    config.Rk = newRk;
    hasCathodeCap = (newCk > 0.0f);

    // Don't reset state -- let the cap voltage evolve naturally
    // from its current value. This avoids clicks on mode switch.
}

//==============================================================================
float TriodeStage::solveQuiescent() const
{
    // DC operating point: grid at 0V through grid leak, no signal.
    //
    // At DC, the cathode bypass cap (if present) is fully charged to Vk,
    // so both bypassed and unbypassed stages have the same DC solution:
    //   Vgk = -Ip*Rk
    //   Vpk = Bplus - Ip*(Rp+Rk)
    //   Ip  = tubeIp(Vgk, Vpk)
    //
    // For the Munro model, the effective voltage is:
    //   ca = offset + Vpk + mu*Vgk
    //      = offset + Bplus - Ip*(Rp + (mu+1)*Rk)
    //   dca/dIp = -(Rp + (mu+1)*Rk)
    //
    // Solve with Newton-Raphson (generous iterations for accuracy).

    float Ip = 1.0e-3f;  // Initial guess: 1.0 mA (typical 12AX7 with Munro model)

    for (int i = 0; i < 50; ++i)
    {
        const float Vgk = -Ip * config.Rk;
        const float Vpk = config.Bplus - Ip * (config.Rp + config.Rk);

        if (Vpk < 0.0f)
        {
            // Plate voltage below cathode -- not physical, reduce current guess
            Ip *= 0.5f;
            continue;
        }

        const float Ip_model = tubeIp(Vgk, Vpk);
        const float residual = Ip - Ip_model;

        if (std::abs(residual) < 1e-10f)
            break;

        // Jacobian: df/dIp = 1 - dIp_model/dIp
        // ca = offset + Vpk + mu*Vgk = offset + Bplus - Ip*(Rp + (mu+1)*Rk)
        const float ca = config.offset + Vpk + config.mu * Vgk;

        if (ca <= 0.0f)
        {
            Ip *= 0.5f;  // In cutoff region, reduce guess
            continue;
        }

        const float dca_dIp = -(config.Rp + (config.mu + 1.0f) * config.Rk);
        const float dIp_dca = config.Ks * 1.5f * std::sqrt(ca);
        const float dIp_model_dIp = dIp_dca * dca_dIp;
        const float jacobian = 1.0f - dIp_model_dIp;

        if (std::abs(jacobian) < 1e-12f)
            break;

        Ip -= residual / jacobian;

        if (Ip < 0.0f) Ip = 1e-6f;  // Keep positive
    }

    return Ip;
}
