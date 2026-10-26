#pragma once
#include <cmath>
#include <juce_core/juce_core.h>

namespace apex
{
// Plain RBJ-cookbook biquad (transposed direct form II)
struct Biquad
{
    enum Type { LP, HP, BP, Peak, LowShelf, HighShelf };

    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0; }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void set (Type t, double fs, double f, double q, double gainDb = 0.0)
    {
        f = juce::jlimit (10.0, fs * 0.45, f);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = juce::MathConstants<double>::twoPi * f / fs;
        const double cw = std::cos (w), sw = std::sin (w), al = sw / (2.0 * q);
        const double sq = 2.0 * std::sqrt (A) * al;
        double B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;

        switch (t)
        {
            case LP:   B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = B0; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
            case HP:   B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = B0; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
            case BP:   B0 = al; B1 = 0; B2 = -al; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
            case Peak: B0 = 1 + al * A; B1 = -2 * cw; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * cw; A2 = 1 - al / A; break;
            case LowShelf:
                B0 = A * ((A + 1) - (A - 1) * cw + sq); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - sq);
                A0 = (A + 1) + (A - 1) * cw + sq;       A1 = -2 * ((A - 1) + (A + 1) * cw);    A2 = (A + 1) + (A - 1) * cw - sq;
                break;
            case HighShelf:
                B0 = A * ((A + 1) + (A - 1) * cw + sq); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - sq);
                A0 = (A + 1) - (A - 1) * cw + sq;       A1 = 2 * ((A - 1) - (A + 1) * cw);      A2 = (A + 1) - (A - 1) * cw - sq;
                break;
        }

        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
};

// One-pole low/high pass
struct OnePole
{
    float a = 0, z = 0;
    void reset() noexcept { z = 0; }
    void set (double fs, double f) { a = (float) std::exp (-juce::MathConstants<double>::twoPi * juce::jlimit (1.0, fs * 0.45, f) / fs); }
    inline float lp (float x) noexcept { z = x + a * (z - x); return z; }
    inline float hp (float x) noexcept { return x - lp (x); }
};
} // namespace apex
