#pragma once
#include <cmath>
#include <algorithm>

namespace sylredhel::origami
{

enum class EngineMode : int
{
    Nest = 0, // Clean Origami: carves ceiling pocket by env(delta_HF) & restores +delta_HF in-phase
    Fold = 1, // Origami Crease: adds +delta_HF & folds ceiling overshoot inward (bright folded edge)
    Dpc  = 2  // Classic DPC: subtracts -delta_HF (Au5 ring-mod crunch) + ceiling guard
};

enum class PhaseMode : int
{
    LinearPhase = 0, // 513/1025-tap Kaiser Linear-Phase FIR
    MinPhaseLR4 = 1  // 24 dB/oct Cytomic TPT LR4
};

enum class SatCurve : int
{
    Tape     = 0, // Smooth symmetric tanh magnetic saturation
    Tube     = 1, // Asymmetric triode warmth (2nd + 3rd harmonics, DC-blocked)
    SoftClip = 2, // Rational mastering soft clipper (transparent core, tight knee)
    HardClip = 3  // Brickwall clipper (maximum punch)
};

enum class FoldShape : int
{
    SmoothFold   = 0, // Infinite periodic C^2 rounded origami crease (exact identity inside +/-0.825C)
    CrispOrigami = 1, // Exact geometric infinite triangle mirror wavefold
    SoftCeiling  = 2  // Transparent soft-clamp guard (no reflection)
};

enum class AuditionMode : int
{
    Normal      = 0,
    FoldedDelta = 1, // Hear ONLY the preserved/folded lost high-frequency detail
    BodyOnly    = 2  // Hear ONLY the saturated/clipped base without detail restoration
};

/**
 * Stereo-Linked Self-Calibrated Input Envelope Follower for Dynamic Drive.
 *
 * Prevents high static Drive from boosting and smearing quiet tails/reverb/drum decays:
 *   - Measures stereo input envelope relative to saturation threshold T = C / baseDriveLin.
 *   - Loud hits / transients -> m(t) -> 1.0 -> full Drive!
 *   - Quiet tails (-24 dB below T) -> m(t) -> 0.0 -> Drive drops smoothly toward 0 dB!
 */
class DriveEnvFollower
{
public:
    void prepareForRate(double effectiveRate)
    {
        fs = std::max(effectiveRate, 8000.0);
        // Fast 1.2 ms attack catches sharp percussive hits cleanly without clicking
        atkAlpha = 1.0 - std::exp(-1.0 / (0.0012 * fs));
        lastRelMs = -1.0;
        setReleaseMs(75.0);
    }

    void reset() noexcept
    {
        env = 0.0;
    }

    inline void setReleaseMs(double releaseMs) noexcept
    {
        const double clamped = std::clamp(releaseMs, 10.0, 500.0);
        if (std::abs(clamped - lastRelMs) > 0.25)
        {
            lastRelMs = clamped;
            relAlpha = 1.0 - std::exp(-1.0 / (clamped * 0.001 * fs));
        }
    }

    inline double processAndGetDriveLin(double inL, double inR,
                                        double baseDriveDb, double baseDriveLin,
                                        double ceilingLin, double envFollow01,
                                        double& outEffDriveDb) noexcept
    {
        const double mag = std::max(std::abs(inL), std::abs(inR));
        const double a = (mag > env) ? atkAlpha : relAlpha;
        env += a * (mag - env);

        if (envFollow01 <= 1e-4 || baseDriveDb <= 1e-4)
        {
            outEffDriveDb = baseDriveDb;
            return baseDriveLin;
        }

        // Self-calibrated threshold T = C / baseDriveLin (where driven peak reaches ceiling)
        const double refLin = std::max(ceilingLin / std::max(baseDriveLin, 1.0), 1e-5);
        const double envRelDb = 20.0 * std::log10(std::max(env, 1e-7) / refLin);

        // Smoothstep ramp over [-24 dB .. 0 dB] relative to saturation threshold
        const double u = std::clamp((envRelDb + 24.0) * (1.0 / 24.0), 0.0, 1.0);
        const double m = u * u * (3.0 - 2.0 * u);

        const double effDb = baseDriveDb * ((1.0 - envFollow01) + envFollow01 * m);
        outEffDriveDb = effDb;
        return std::pow(10.0, effDb * 0.05);
    }

private:
    double fs        = 384000.0;
    double atkAlpha  = 0.002;
    double relAlpha  = 0.00003;
    double lastRelMs = -1.0;
    double env       = 0.0;
};

class OrigamiWaveshaper
{
public:
    void prepareForRate(double effectiveRate)
    {
        const double fs = std::max(effectiveRate, 8000.0);
        if (std::abs(fs - currentFs) < 1.0)
            return;
        currentFs = fs;

        // 12 Hz DC blocker for asymmetric Tube saturation
        dcAlpha = std::exp(-6.283185307179586 * 12.0 / fs);

        // Ultra-fast attack (0.10 ms) & smooth release (5.0 ms) envelope for NEST ceiling carving
        envAtkAlpha = 1.0 - std::exp(-1.0 / (0.00010 * fs));
        envRelAlpha = 1.0 - std::exp(-1.0 / (0.00500 * fs));
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            dcState[ch]  = 0.0;
            dcPrevIn[ch] = 0.0;
            nestEnv[ch]  = 0.0;
        }
    }

    static inline double evalSatStatic(double x, double C, SatCurve curve) noexcept
    {
        const double cSafe = std::max(C, 0.05);
        const double u = x / cSafe;

        switch (curve)
        {
            case SatCurve::Tape:
                return cSafe * std::tanh(u);

            case SatCurve::Tube:
            {
                constexpr double bias = 0.16;
                const double zeroOff = std::tanh(bias);
                const double posScale = 1.0 - zeroOff;
                const double negScale = 1.0 + zeroOff;
                const double raw = std::tanh(u * 1.08 + bias) - zeroOff;
                const double norm = (raw >= 0.0) ? (raw / posScale) : (raw / negScale);
                return cSafe * norm;
            }

            case SatCurve::SoftClip:
            {
                const double au = std::abs(u);
                const double denom = std::cbrt(1.0 + au * au * au);
                return cSafe * (u / denom);
            }

            case SatCurve::HardClip:
            default:
                return std::clamp(x, -cSafe, cSafe);
        }
    }

    inline double processSaturation(double x, double C, SatCurve curve, int ch) noexcept
    {
        double y = evalSatStatic(x, C, curve);
        if (curve == SatCurve::Tube)
        {
            const double hp = dcAlpha * (dcState[ch] + y - dcPrevIn[ch]);
            dcPrevIn[ch] = y;
            dcState[ch]  = hp;
            y = hp;
        }
        else
        {
            dcPrevIn[ch] = y;
            dcState[ch]  = y;
        }
        return y;
    }

    /**
     * Reconstruct lost high-frequency waveform `dHF` back into saturated signal `sat`
     * strictly bounded inside [-C, +C].
     * Invariant: when `dHF == 0` (unclipped OR preserve == 0%), output == sat!
     */
    inline double processLostDetailReconstruct(double sat, double dHF, double C,
                                               EngineMode mode, FoldShape shape,
                                               double foldDepth01, int ch) noexcept
    {
        const double cSafe = std::max(C, 0.05);

        const double absD = std::abs(dHF);
        const double aEnv = (absD > nestEnv[ch]) ? envAtkAlpha : envRelAlpha;
        nestEnv[ch] += aEnv * (absD - nestEnv[ch]);

        if (mode == EngineMode::Nest)
        {
            const double absSat = std::abs(sat);
            const double needed = std::max(0.0, absSat + nestEnv[ch] - cSafe);
            const double carve  = std::min(needed, absSat * 0.92);
            const double satCarved = sat - std::copysign(carve, sat);
            const double combined  = satCarved + dHF;

            return evalOrigamiFold(combined, cSafe, shape, foldDepth01 * 0.35);
        }
        else if (mode == EngineMode::Fold)
        {
            const double combined = sat + dHF;
            return evalOrigamiFold(combined, cSafe, shape, foldDepth01);
        }
        else // EngineMode::Dpc
        {
            const double combined = sat - dHF;
            return evalOrigamiFold(combined, cSafe, shape, foldDepth01);
        }
    }

    /**
     * Origami Wavefolding Engine:
     *   - SmoothFold   : True infinite periodic C^2 quartic-rounded origami fold!
     *                    Exact identity y = v inside |v| <= 0.825*C, smooth rounded crests
     *                    hitting exact +/-C, repeating infinitely with zero flat-line clamp!
     *   - CrispOrigami : Exact geometric infinite triangle mirror wavefold.
     *   - SoftCeiling  : Transparent C^infinity soft-clamp guard (no foldback).
     */
    static inline double evalOrigamiFold(double v, double C, FoldShape shape, double foldDepth01) noexcept
    {
        const double cSafe = std::max(C, 0.05);
        const double depth = std::clamp(foldDepth01, 0.0, 1.0);

        const double clamped = evalSoftClamp(v, cSafe);
        if (shape == FoldShape::SoftCeiling || depth <= 1e-4)
            return clamped;

        double folded = clamped;
        if (shape == FoldShape::SmoothFold)
        {
            // Infinite Periodic C^2 Smooth Origami Fold:
            //   Crease rounding half-width w = 0.28 * C
            //   Peak center vPeak = C + 3/8 * w = 1.105 * C, Period = 4 * vPeak
            //   Polynomial cap P(t) = 0.75*t^2 - 0.125*t^4 (P(0)=0, P'(1)=1, P''(1)=0 -> exact C^2!)
            const double w      = 0.28 * cSafe;
            const double invW   = 1.0 / w;
            const double vPeak  = cSafe + 0.375 * w;
            const double period = 4.0 * vPeak;

            double m = std::fmod(v + vPeak, period);
            if (m < 0.0) m += period;
            const double u = m - vPeak;          // [-vPeak .. 3*vPeak)
            const double p = std::abs(u - vPeak); // [0 .. 2*vPeak] (0 = +C peak, 2*vPeak = -C valley)

            if (p < w)
            {
                const double t  = p * invW;
                const double t2 = t * t;
                folded = cSafe - w * (0.75 * t2 - 0.125 * t2 * t2);
            }
            else if (p > (2.0 * vPeak - w))
            {
                const double t  = (2.0 * vPeak - p) * invW;
                const double t2 = t * t;
                folded = -cSafe + w * (0.75 * t2 - 0.125 * t2 * t2);
            }
            else
            {
                folded = vPeak - p;
            }
        }
        else // FoldShape::CrispOrigami
        {
            const double u = (v + cSafe) / (2.0 * cSafe);
            double m = std::fmod(u, 2.0);
            if (m < 0.0) m += 2.0;
            const double tri = 1.0 - std::abs(m - 1.0);
            folded = (2.0 * tri - 1.0) * cSafe;
        }

        return clamped + depth * (folded - clamped);
    }

    static inline double evalSoftClamp(double v, double C) noexcept
    {
        const double eps = 0.030 * C;
        const double a = std::abs(v);
        const double excess = softplus(a - C, eps);
        return std::copysign(std::clamp(a - excess, 0.0, C), v);
    }

private:
    static inline double softplus(double x, double eps) noexcept
    {
        const double z = x / eps;
        if (z > 24.0)  return x;
        if (z < -24.0) return 0.0;
        return eps * std::log1p(std::exp(z));
    }

    double currentFs   = 0.0;
    double dcAlpha     = 0.9998;
    double envAtkAlpha = 0.025;
    double envRelAlpha = 0.0005;

    double dcState[2]  = { 0.0, 0.0 };
    double dcPrevIn[2] = { 0.0, 0.0 };
    double nestEnv[2]  = { 0.0, 0.0 };
};

} // namespace sylredhel::origami
