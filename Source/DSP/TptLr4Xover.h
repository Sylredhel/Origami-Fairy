#pragma once
#include <cmath>
#include <algorithm>

namespace sylredhel::origami
{

class TptLr4Xover
{
public:
    void prepare(double sampleRate)
    {
        fs = std::max(sampleRate, 8000.0);
        invFs = 1.0 / fs;
        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            lp1[ch].reset(); lp2[ch].reset();
            hp1[ch].reset(); hp2[ch].reset();
        }
    }

    inline void processStereoSample(double inL, double inR, double fcHz,
                                    double& lowL, double& lowR,
                                    double& highL, double& highR) noexcept
    {
        const double fc = std::clamp(fcHz, 80.0, 0.42 * fs);
        const double g  = std::tan(3.14159265358979323846 * fc * invFs);
        constexpr double k = 1.4142135623730951;
        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double a2 = g * a1;
        const double a3 = g * a2;

        lowL  = lp2[0].processLP(lp1[0].processLP(inL, a1, a2, a3), a1, a2, a3);
        lowR  = lp2[1].processLP(lp1[1].processLP(inR, a1, a2, a3), a1, a2, a3);
        highL = hp2[0].processHP(hp1[0].processHP(inL, k, a1, a2, a3), k, a1, a2, a3);
        highR = hp2[1].processHP(hp1[1].processHP(inR, k, a1, a2, a3), k, a1, a2, a3);
    }

private:
    struct SvfCore
    {
        double ic1 = 0.0, ic2 = 0.0;
        void reset() noexcept { ic1 = 0.0; ic2 = 0.0; }

        inline double processLP(double x, double a1, double a2, double a3) noexcept
        {
            const double v3 = x - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            return v2;
        }

        inline double processHP(double x, double k, double a1, double a2, double a3) noexcept
        {
            const double v3 = x - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            return x - k * v1 - v2;
        }
    };

    double fs = 48000.0;
    double invFs = 1.0 / 48000.0;
    SvfCore lp1[2], lp2[2], hp1[2], hp2[2];
};

} // namespace sylredhel::origami
