#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace sylredhel::origami
{

/**
 * Stereo Symmetric Linear-Phase Kaiser Windowed-Sinc Crossover.
 *
 * Length M = 2*K + 1:
 *   - 513 taps  @ <= 64 kHz -> K = 256 samples (5.33 ms @ 48 kHz)
 *   - 1025 taps @ >  64 kHz -> K = 512 samples (5.33 ms @ 96 kHz)
 *
 * Bit-exact complementary split: xLow[n] + xHigh[n] == x[n - K].
 * Double-buffered 12 ms kernel crossfade guarantees click-free frequency sweeps.
 */
class LinearPhaseFirXover
{
public:
    void prepare(double sampleRate)
    {
        fs = std::max(sampleRate, 8000.0);
        K = (fs > 64000.0) ? 512 : 256;
        M = 2 * K + 1;
        ringSize = (K == 512) ? 2048 : 1024;
        ringMask = ringSize - 1;

        for (int ch = 0; ch < 2; ++ch)
            ring[ch].assign(static_cast<size_t>(ringSize), 0.0);

        kernA.assign(static_cast<size_t>(K + 1), 0.0);
        kernB.assign(static_cast<size_t>(K + 1), 0.0);
        xfadeStep = 1.0 / (0.012 * fs);

        buildKaiserLowPass(1400.0, kernA);
        kernB = kernA;
        currentFc = 1400.0;
        activeKernel = 0;
        xfadePos = 1.0;

        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
            std::fill(ring[ch].begin(), ring[ch].end(), 0.0);
        writePos = 0;
    }

    int getLatencySamples() const noexcept { return K; }

    void setCutoffHz(double fcHz) noexcept
    {
        const double fc = std::clamp(fcHz, 120.0, 0.40 * fs);
        if (xfadePos >= 1.0 && std::abs(fc - currentFc) > currentFc * 0.006)
        {
            const int nextIdx = 1 - activeKernel;
            buildKaiserLowPass(fc, nextIdx == 0 ? kernA : kernB);
            activeKernel = nextIdx;
            currentFc = fc;
            xfadePos = 0.0;
        }
    }

    inline void processStereoSample(double inL, double inR,
                                    double& lowL, double& lowR,
                                    double& highL, double& highR) noexcept
    {
        ring[0][static_cast<size_t>(writePos)] = inL;
        ring[1][static_cast<size_t>(writePos)] = inR;

        const int centerIdx = (writePos - K) & ringMask;
        const double delayedL = ring[0][static_cast<size_t>(centerIdx)];
        const double delayedR = ring[1][static_cast<size_t>(centerIdx)];

        if (xfadePos >= 1.0)
        {
            const auto& h = (activeKernel == 0) ? kernA : kernB;
            evalSymmetricFir(centerIdx, h, lowL, lowR);
        }
        else
        {
            double lA = 0.0, rA = 0.0, lB = 0.0, rB = 0.0;
            const auto& hNew = (activeKernel == 0) ? kernA : kernB;
            const auto& hOld = (activeKernel == 0) ? kernB : kernA;
            evalSymmetricFir(centerIdx, hOld, lA, rA);
            evalSymmetricFir(centerIdx, hNew, lB, rB);

            const double g = xfadePos;
            lowL = lA + g * (lB - lA);
            lowR = rA + g * (rB - rA);
            xfadePos = std::min(1.0, xfadePos + xfadeStep);
        }

        highL = delayedL - lowL;
        highR = delayedR - lowR;

        writePos = (writePos + 1) & ringMask;
    }

private:
    inline void evalSymmetricFir(int centerIdx, const std::vector<double>& h,
                                 double& outL, double& outR) const noexcept
    {
        const double* r0 = ring[0].data();
        const double* r1 = ring[1].data();
        const double* hp = h.data();

        double accL = hp[0] * r0[centerIdx];
        double accR = hp[0] * r1[centerIdx];

        for (int d = 1; d <= K; ++d)
        {
            const int iP = (centerIdx + d) & ringMask;
            const int iM = (centerIdx - d) & ringMask;
            const double w = hp[d];
            accL += w * (r0[iP] + r0[iM]);
            accR += w * (r1[iP] + r1[iM]);
        }

        outL = accL;
        outR = accR;
    }

    static double besselI0(double x) noexcept
    {
        double sum = 1.0;
        double term = 1.0;
        const double halfXSq = 0.25 * x * x;
        for (int k = 1; k <= 24; ++k)
        {
            term *= halfXSq / (static_cast<double>(k) * static_cast<double>(k));
            sum += term;
            if (term < 1e-15 * sum)
                break;
        }
        return sum;
    }

    void buildKaiserLowPass(double fcHz, std::vector<double>& halfKernel) const noexcept
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr double beta = 6.5;
        const double invI0Beta = 1.0 / besselI0(beta);
        const double fcNorm = std::clamp(fcHz / fs, 0.001, 0.44);
        const double invK = 1.0 / static_cast<double>(K);

        halfKernel[0] = 2.0 * fcNorm;
        double sum = halfKernel[0];

        for (int d = 1; d <= K; ++d)
        {
            const double x = static_cast<double>(d) * invK;
            const double arg = std::max(0.0, 1.0 - x * x);
            const double w = besselI0(beta * std::sqrt(arg)) * invI0Beta;
            const double s = std::sin(2.0 * pi * fcNorm * static_cast<double>(d)) / (pi * static_cast<double>(d));
            const double tap = s * w;
            halfKernel[static_cast<size_t>(d)] = tap;
            sum += 2.0 * tap;
        }

        const double invSum = 1.0 / std::max(sum, 1e-12);
        for (int d = 0; d <= K; ++d)
            halfKernel[static_cast<size_t>(d)] *= invSum;
    }

    double fs = 48000.0;
    int K = 256;
    int M = 513;
    int ringSize = 1024;
    int ringMask = 1023;
    int writePos = 0;

    std::vector<double> ring[2];
    std::vector<double> kernA, kernB;
    int activeKernel = 0;
    double currentFc = 1400.0;
    double xfadePos = 1.0;
    double xfadeStep = 0.002;
};

/**
 * Stereo 4-point Cubic Lagrange Fractional Delay Line for Exact Latency Alignment.
 */
class FractionalDelayLine
{
public:
    void prepare(int maxDelaySamples)
    {
        int sz = 16;
        while (sz < maxDelaySamples + 16)
            sz <<= 1;
        bufSize = sz;
        bufMask = sz - 1;
        for (int ch = 0; ch < 2; ++ch)
            buf[ch].assign(static_cast<size_t>(bufSize), 0.0);
        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
            std::fill(buf[ch].begin(), buf[ch].end(), 0.0);
        writePos = 0;
    }

    inline void pushAndRead(double inL, double inR, double delaySamples,
                            double& outL, double& outR) noexcept
    {
        buf[0][static_cast<size_t>(writePos)] = inL;
        buf[1][static_cast<size_t>(writePos)] = inR;

        if (delaySamples < 1.0)
        {
            outL = inL;
            outR = inR;
            writePos = (writePos + 1) & bufMask;
            return;
        }

        const double d = std::clamp(delaySamples, 1.0, static_cast<double>(bufSize - 4));
        const double readPos = static_cast<double>(writePos) - d;
        const int i1 = static_cast<int>(std::floor(readPos));
        const double f = readPos - static_cast<double>(i1);

        const int idx0 = (i1 - 1) & bufMask;
        const int idx1 = (i1)     & bufMask;
        const int idx2 = (i1 + 1) & bufMask;
        const int idx3 = (i1 + 2) & bufMask;

        const double c0 = -f * (f - 1.0) * (f - 2.0) * (1.0 / 6.0);
        const double c1 =  (f + 1.0) * (f - 1.0) * (f - 2.0) * 0.5;
        const double c2 = -(f + 1.0) * f * (f - 2.0) * 0.5;
        const double c3 =  (f + 1.0) * f * (f - 1.0) * (1.0 / 6.0);

        outL = c0 * buf[0][static_cast<size_t>(idx0)] + c1 * buf[0][static_cast<size_t>(idx1)]
             + c2 * buf[0][static_cast<size_t>(idx2)] + c3 * buf[0][static_cast<size_t>(idx3)];
        outR = c0 * buf[1][static_cast<size_t>(idx0)] + c1 * buf[1][static_cast<size_t>(idx1)]
             + c2 * buf[1][static_cast<size_t>(idx2)] + c3 * buf[1][static_cast<size_t>(idx3)];

        writePos = (writePos + 1) & bufMask;
    }

private:
    int bufSize = 2048;
    int bufMask = 2047;
    int writePos = 0;
    std::vector<double> buf[2];
};

} // namespace sylredhel::origami
