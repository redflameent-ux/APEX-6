#include "Modules.h"
#include "../Params.h"

namespace apex
{
using Buf = juce::AudioBuffer<float>;
static constexpr float twoPi = juce::MathConstants<float>::twoPi;

static void copyInto (Buf& dst, const Buf& src, int n)
{
    for (int ch = 0; ch < 2; ++ch)
        dst.copyFrom (ch, 0, src, ch, 0, n);
}

// =============================================================================================
void Module::prepare (double sampleRate, int maxBlockSize)
{
    fs = sampleRate;
    maxBlock = maxBlockSize;
    mixGain.reset (sampleRate, 0.015);
    mixGain.setCurrentAndTargetValue (0.0f);
}

void Module::run (Buf& buf, bool on, Buf& scratch, const BlockInfo& bi)
{
    const int n = buf.getNumSamples();
    mixGain.setTargetValue (on ? 1.0f : 0.0f);
    const bool ramping = mixGain.isSmoothing();
    const float g = mixGain.getCurrentValue();
    const int lat = latency();

    if (lat > 0)
    {
        copyInto (scratch, buf, n);   // keep the delayed dry path fed at all times
        delayDry (scratch);
    }

    if (! ramping && g <= 0.0f)
    {
        if (lat > 0)
            copyInto (buf, scratch, n);
        return;
    }

    if (! ramping && g >= 1.0f)
    {
        processWet (buf, bi);
        return;
    }

    if (lat == 0)
        copyInto (scratch, buf, n);

    processWet (buf, bi);

    auto* wl = buf.getWritePointer (0);
    auto* wr = buf.getWritePointer (1);
    auto* dl = scratch.getReadPointer (0);
    auto* dr = scratch.getReadPointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float k = mixGain.getNextValue();
        wl[i] = dl[i] + (wl[i] - dl[i]) * k;
        wr[i] = dr[i] + (wr[i] - dr[i]) * k;
    }
}

void Module::runAbsent (Buf& buf, Buf& scratch)
{
    mixGain.setCurrentAndTargetValue (0.0f);
    if (latency() > 0)
    {
        const int n = buf.getNumSamples();
        copyInto (scratch, buf, n);
        delayDry (scratch);
        copyInto (buf, scratch, n);
    }
}

// =============================================================================================
// GATEKEEPER - noise gate with hysteresis and hold
class GateModule : public Module
{
public:
    explicit GateModule (juce::AudioProcessorValueTreeState& s) : Module (s)
    {
        th = p ("gate_thresh");
        rel = p ("gate_release");
    }
    void reset() override { env = 0; gain = 0; open = false; hold = 0; }

protected:
    void processWet (Buf& buf, const BlockInfo&) override
    {
        const int n = buf.getNumSamples();
        auto* L = buf.getWritePointer (0);
        auto* R = buf.getWritePointer (1);
        const float openTh = juce::Decibels::decibelsToGain (th->load());
        const float closeTh = openTh * 0.5f;
        const float envRel = std::exp (-1.0f / (0.010f * (float) fs));
        const float gAtt = std::exp (-1.0f / (0.0008f * (float) fs));
        const float gRel = std::exp (-1.0f / (rel->load() * 0.001f * (float) fs));
        const int holdLen = (int) (0.02 * fs);

        for (int i = 0; i < n; ++i)
        {
            const float lvl = juce::jmax (std::abs (L[i]), std::abs (R[i]));
            env = juce::jmax (lvl, env * envRel);
            if (env > openTh) { open = true; hold = holdLen; }
            else if (env < closeTh) { if (hold > 0) --hold; else open = false; }

            const float tgt = open ? 1.0f : 0.0f;
            const float c = tgt > gain ? gAtt : gRel;
            gain = tgt + c * (gain - tgt);
            L[i] *= gain;
            R[i] *= gain;
        }
    }

private:
    std::atomic<float>*th, *rel;
    float env = 0, gain = 0;
    bool open = false;
    int hold = 0;
};

// =============================================================================================
// TIGHT - low-end tightener, mid "edge" and clean-ish boost into the amp
class BoostModule : public Module
{
public:
    explicit BoostModule (juce::AudioProcessorValueTreeState& s) : Module (s)
    {
        tight = p ("boost_tight"); edge = p ("boost_edge"); lvl = p ("boost_level");
    }
    void prepare (double sr, int mb) override
    {
        Module::prepare (sr, mb);
        gain.reset (sr, 0.05);
        lastT = lastE = -1;
    }
    void reset() override { for (int c = 0; c < 2; ++c) { hp[c].reset(); hp2[c].reset(); pk[c].reset(); } }

protected:
    void processWet (Buf& buf, const BlockInfo&) override
    {
        const float t = tight->load(), e = edge->load();
        if (t != lastT || e != lastE)
        {
            const double f = 40.0 * std::pow (10.0, (double) t);
            for (int c = 0; c < 2; ++c)
            {
                hp[c].set (Biquad::HP, fs, f, 0.6);
                hp2[c].set (Biquad::HP, fs, f * 0.8, 0.5);
                pk[c].set (Biquad::Peak, fs, 1400.0, 0.8, e * 9.0);
            }
            lastT = t; lastE = e;
        }
        gain.setTargetValue (juce::Decibels::decibelsToGain (lvl->load()));

        const int n = buf.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            const float g = gain.getNextValue();
            for (int c = 0; c < 2; ++c)
            {
                float x = buf.getSample (c, i);
                x = pk[c].process (hp2[c].process (hp[c].process (x)));
                buf.setSample (c, i, std::tanh (x * g));
            }
        }
    }

private:
    std::atomic<float>*tight, *edge, *lvl;
    Biquad hp[2], hp2[2], pk[2];
    float lastT = -1, lastE = -1;
    juce::SmoothedValue<float> gain;
};

// =============================================================================================
// APEX HEAD - 4x oversampled multi-stage preamp, tone stack, power amp with sag
class AmpModule : public Module
{
public:
    explicit AmpModule (juce::AudioProcessorValueTreeState& s) : Module (s)
    {
        chan = p ("amp_channel"); gain = p ("amp_gain");
        bass = p ("amp_bass"); mid = p ("amp_mid"); treb = p ("amp_treble");
        pres = p ("amp_presence"); depth = p ("amp_depth"); master = p ("amp_master");
    }

    void prepare (double sr, int mb) override
    {
        Module::prepare (sr, mb);
        os = std::make_unique<juce::dsp::Oversampling<float>> (
            2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        os->initProcessing ((size_t) mb);
        osFs = sr * 4.0;
        lat = (int) std::round (os->getLatencyInSamples());

        dry.prepare ({ sr, (juce::uint32) mb, 2 });
        dry.setMaximumDelayInSamples (juce::jmax (8, lat + 8));
        dry.setDelay ((float) lat);

        stageGain.reset (osFs, 0.03);
        masterGain.reset (osFs, 0.03);
        trim.reset (osFs, 0.03);
        trim.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (voice ((int) chan->load()).trimDb));
        dirty = true;
        reset();
    }

    void reset() override
    {
        if (os) os->reset();
        dry.reset();
        for (auto& s : st) s = State();
        dirty = true;
    }

    int latency() const override { return lat; }

protected:
    struct Voice { float minDb, maxDb; int stages; float bias[3]; double preHP, prePkF, prePkDb, midF, interLP; float trimDb; };

    static const Voice& voice (int c)
    {
        static const Voice v[4] = {
            { -6.0f, 18.0f, 1, { 0.05f, 0.0f, 0.0f },     60.0,  800.0, 0.0, 500.0, 12000.0,  8.0f }, // Clean
            {  6.0f, 36.0f, 2, { 0.10f, -0.05f, 0.0f },   90.0, 1000.0, 1.0, 600.0,  9000.0, -3.0f }, // Crunch
            { 12.0f, 54.0f, 3, { 0.15f, -0.10f, 0.08f }, 110.0, 1000.0, 2.5, 650.0,  7500.0, -7.0f }, // Lead
            { 18.0f, 60.0f, 3, { 0.20f, -0.12f, 0.10f }, 160.0, 1500.0, 4.0, 750.0,  7000.0, -9.0f }, // Modern
        };
        return v[juce::jlimit (0, 3, c)];
    }

    struct State
    {
        Biquad preHP, prePk, bass, mid, treb, depth, pres;
        OnePole ihp[3], ilp[3], post;
        float sag = 0;
    };

    void updateFilters()
    {
        const int c = (int) chan->load();
        const float b = bass->load(), m = mid->load(), t = treb->load(), pr = pres->load(), d = depth->load();
        if (! dirty && c == lastC && b == lastB && m == lastM && t == lastT && pr == lastP && d == lastD)
            return;

        const auto& v = voice (c);
        for (auto& s : st)
        {
            s.preHP.set (Biquad::HP, osFs, v.preHP, 0.7);
            s.prePk.set (Biquad::Peak, osFs, v.prePkF, 0.7, v.prePkDb);
            for (int k = 0; k < 3; ++k)
            {
                s.ihp[k].set (osFs, 25.0);
                s.ilp[k].set (osFs, v.interLP);
            }
            s.bass.set (Biquad::LowShelf, osFs, 110.0, 0.7, (b - 5.0f) * 2.4f);
            s.mid.set (Biquad::Peak, osFs, v.midF, 0.7, m < 5.0f ? (m - 5.0f) * 2.4f : (m - 5.0f) * 1.8f);
            s.treb.set (Biquad::HighShelf, osFs, 2800.0, 0.7, (t - 5.0f) * 2.4f);
            s.depth.set (Biquad::Peak, osFs, 85.0, 0.9, d * 1.0);
            s.pres.set (Biquad::HighShelf, osFs, 4500.0, 0.7, pr * 1.0);
            s.post.set (osFs, 12000.0);
        }
        lastC = c; lastB = b; lastM = m; lastT = t; lastP = pr; lastD = d;
        dirty = false;
    }

    void processWet (Buf& buf, const BlockInfo&) override
    {
        updateFilters();
        const auto& v = voice ((int) chan->load());
        const float totalDb = v.minDb + (gain->load() / 10.0f) * (v.maxDb - v.minDb);
        stageGain.setTargetValue (juce::Decibels::decibelsToGain (totalDb / (float) v.stages));
        masterGain.setTargetValue (master->load());
        trim.setTargetValue (juce::Decibels::decibelsToGain (v.trimDb));

        float tb[3];
        for (int k = 0; k < 3; ++k) tb[k] = std::tanh (v.bias[k]);

        juce::dsp::AudioBlock<float> block (buf);
        auto up = os->processSamplesUp (block);
        const int N = (int) up.getNumSamples();
        float* d0 = up.getChannelPointer (0);
        float* d1 = up.getChannelPointer (1);
        const float sagAtt = 1.0f - std::exp (-1.0f / (0.005f * (float) osFs));
        const float sagRel = std::exp (-1.0f / (0.15f * (float) osFs));

        for (int i = 0; i < N; ++i)
        {
            const float g = stageGain.getNextValue();
            const float m = masterGain.getNextValue();
            const float drive = juce::Decibels::decibelsToGain ((m - 5.0f) * 1.6f);
            const float outG = 0.5f * trim.getNextValue() * juce::Decibels::decibelsToGain ((m - 5.0f) * 0.8f);

            for (int c = 0; c < 2; ++c)
            {
                auto& s = st[c];
                float* d = c == 0 ? d0 : d1;
                float x = s.prePk.process (s.preHP.process (d[i]));

                for (int k = 0; k < v.stages; ++k)
                {
                    x = std::tanh (x * g + v.bias[k]) - tb[k];
                    x = s.ilp[k].lp (s.ihp[k].hp (x));
                }

                x = s.treb.process (s.mid.process (s.bass.process (x)));
                x = s.pres.process (s.depth.process (x));

                const float a = std::abs (x);
                s.sag = a > s.sag ? s.sag + (a - s.sag) * sagAtt : s.sag * sagRel;
                x = std::tanh (x * drive * (1.0f - 0.25f * juce::jmin (s.sag, 1.0f)));
                d[i] = s.post.lp (x) * outG;
            }
        }

        os->processSamplesDown (block);
    }

    void delayDry (Buf& b) override
    {
        const int n = b.getNumSamples();
        for (int c = 0; c < 2; ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                dry.pushSample (c, d[i]);
                d[i] = dry.popSample (c);
            }
        }
    }

private:
    std::atomic<float>*chan, *gain, *bass, *mid, *treb, *pres, *depth, *master;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dry { 64 };
    double osFs = 176400.0;
    int lat = 0;
    State st[2];
    juce::SmoothedValue<float> stageGain, masterGain, trim;
    bool dirty = true;
    int lastC = -1;
    float lastB = -1, lastM = -1, lastT = -1, lastP = -1, lastD = -1;
};

// =============================================================================================
// IR MATRIX - four IR slots, each with its own mix level and polarity, summed together
CabModule::CabModule (juce::AudioProcessorValueTreeState& s) : Module (s)
{
    static const char* mixIds[numSlots]   = { "cab_mixA", "cab_mixB", "cab_mixC", "cab_mixD" };
    static const char* phaseIds[numSlots] = { "cab_phaseA", "cab_phaseB", "cab_phaseC", "cab_phaseD" };
    for (int i = 0; i < numSlots; ++i) { pMix[i] = p (mixIds[i]); pPhase[i] = p (phaseIds[i]); }
    pLow = p ("cab_lowcut"); pHigh = p ("cab_highcut"); pLevel = p ("cab_level");
}

void CabModule::Sim::set (double fs)
{
    // A built-in closed-back 4x12 voicing, used whenever a slot has no IR loaded
    hp.set (Biquad::HP, fs, 75.0, 0.8);
    low.set (Biquad::Peak, fs, 110.0, 1.0, 3.0);
    dip.set (Biquad::Peak, fs, 500.0, 1.0, -4.0);
    pres.set (Biquad::Peak, fs, 2600.0, 1.4, 5.0);
    lp1.set (Biquad::LP, fs, 5200.0, 0.7);
    lp2.set (Biquad::LP, fs, 6500.0, 0.6);
}

void CabModule::Sim::reset() { hp.reset(); low.reset(); dip.reset(); pres.reset(); lp1.reset(); lp2.reset(); }

void CabModule::prepare (double sr, int mb)
{
    Module::prepare (sr, mb);
    const juce::dsp::ProcessSpec spec { sr, (juce::uint32) mb, 2 };
    for (auto& c : conv) c.prepare (spec);
    for (auto& slot : sim) for (auto& s : slot) s.set (sr);
    for (auto* b : { &in, &tmp, &acc }) b->setSize (2, mb, false, false, true);
    level.reset (sr, 0.05);
    for (auto& w : weight) { w.reset (sr, 0.05); w.setCurrentAndTargetValue (0.0f); }
    lastLow = lastHigh = -1;
    reset();
}

void CabModule::reset()
{
    for (auto& c : conv) c.reset();
    for (auto& slot : sim) for (auto& s : slot) s.reset();
    for (int c = 0; c < 2; ++c) { lowcut[c].reset(); highcut[c].reset(); }
}

bool CabModule::loadIR (int slot, const juce::File& file)
{
    slot = juce::jlimit (0, numSlots - 1, slot);
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (file));
    if (r == nullptr || r->lengthInSamples < 2)
        return false;

    conv[slot].loadImpulseResponse (file, juce::dsp::Convolution::Stereo::no,
                                    juce::dsp::Convolution::Trim::yes, 0,
                                    juce::dsp::Convolution::Normalise::yes);
    loaded[slot] = true;
    return true;
}

void CabModule::clearIR (int slot) { loaded[juce::jlimit (0, numSlots - 1, slot)] = false; }

void CabModule::processWet (Buf& buf, const BlockInfo&)
{
    const int n = buf.getNumSamples();
    const float lo = pLow->load(), hi = pHigh->load();
    if (lo != lastLow || hi != lastHigh)
    {
        for (int c = 0; c < 2; ++c)
        {
            lowcut[c].set (Biquad::HP, fs, lo, 0.7);
            highcut[c].set (Biquad::LP, fs, hi, 0.7);
        }
        lastLow = lo; lastHigh = hi;
    }
    level.setTargetValue (juce::Decibels::decibelsToGain (pLevel->load()));

    // Mix levels are scaled by their combined power, so stacking slots doesn't blow up the
    // volume (one slot at full = unity, four at full = half each) and blends stay about as loud as one IR.
    float power = 0;
    for (int i = 0; i < numSlots; ++i) power += juce::square (pMix[i]->load());
    const float norm = 1.0f / juce::jmax (1.0f, std::sqrt (power));
    for (int i = 0; i < numSlots; ++i)
        weight[i].setTargetValue (pMix[i]->load() * norm * (pPhase[i]->load() > 0.5f ? -1.0f : 1.0f));

    copyInto (in, buf, n);
    acc.clear (0, n);

    for (int slot = 0; slot < numSlots; ++slot)
    {
        auto& w = weight[slot];
        if (! w.isSmoothing() && w.getCurrentValue() == 0.0f)
            continue;

        copyInto (tmp, in, n);
        if (loaded[slot])
        {
            juce::dsp::AudioBlock<float> blk (tmp);
            auto sub = blk.getSubBlock (0, (size_t) n);
            conv[slot].process (juce::dsp::ProcessContextReplacing<float> (sub));
            sub.multiplyBy (8.0f);   // JUCE normalises IRs to -18 dB; bring them back to about unity
        }
        else
        {
            for (int c = 0; c < 2; ++c)
            {
                auto* d = tmp.getWritePointer (c);
                for (int i = 0; i < n; ++i) d[i] = sim[slot][c].process (d[i]);
            }
        }

        auto* t0 = tmp.getReadPointer (0); auto* t1 = tmp.getReadPointer (1);
        auto* a0 = acc.getWritePointer (0); auto* a1 = acc.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float g = w.getNextValue();
            a0[i] += t0[i] * g;
            a1[i] += t1[i] * g;
        }
    }

    for (int i = 0; i < n; ++i)
    {
        const float lv = level.getNextValue();
        for (int c = 0; c < 2; ++c)
        {
            const float x = highcut[c].process (lowcut[c].process (acc.getSample (c, i)));
            buf.setSample (c, i, x * lv);
        }
    }
}

// =============================================================================================
// SCRAPYARD - fuzz into bit crush and sample-rate crush
class CrushModule : public Module
{
public:
    explicit CrushModule (juce::AudioProcessorValueTreeState& s) : Module (s)
    {
        bits = p ("crush_bits"); rate = p ("crush_rate"); fuzz = p ("crush_fuzz"); mix = p ("crush_mix");
    }
    void reset() override { for (int c = 0; c < 2; ++c) { phase[c] = 0; held[c] = 0; dc[c].reset(); } }
    void prepare (double sr, int mb) override
    {
        Module::prepare (sr, mb);
        for (auto& d : dc) d.set (sr, 15.0);
        reset();
    }

protected:
    void processWet (Buf& buf, const BlockInfo&) override
    {
        const int n = buf.getNumSamples();
        const float steps = std::pow (2.0f, bits->load() - 1.0f);
        const float r = rate->load();
        const float fz = fuzz->load();
        const float drive = 1.0f + fz * fz * 40.0f;
        const float m = mix->load();

        for (int c = 0; c < 2; ++c)
        {
            auto* d = buf.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                const float dryS = d[i];
                float x = std::tanh (dryS * drive + fz * 0.2f);   // a little asymmetry makes it gnarly
                phase[c] += 1.0f;
                if (phase[c] >= r)
                {
                    phase[c] -= r;
                    held[c] = std::round (x * steps) / steps;
                }
                const float wet = dc[c].hp (held[c]) * (fz > 0.0f ? 0.8f : 1.0f);
                d[i] = dryS + (wet - dryS) * m;
            }
        }
    }

private:
    std::atomic<float>*bits, *rate, *fuzz, *mix;
    float phase[2] {}, held[2] {};
    OnePole dc[2];
};

// =============================================================================================
// GLITCH LOOM - tempo-synced stutter: on random grid steps it repeats (or reverses) slices
class GlitchModule : public Module
{
public:
    explicit GlitchModule (juce::AudioProcessorValueTreeState& s) : Module (s)
    {
        div = p ("glitch_div"); shred = p ("glitch_shred"); chance = p ("glitch_chance");
        rev = p ("glitch_reverse"); mix = p ("glitch_mix");
    }
    void prepare (double sr, int mb) override
    {
        Module::prepare (sr, mb);
        ring.setSize (2, (int) (sr * 4.0), false, true, false);
        reset();
    }
    void reset() override
    {
        ring.clear();
        writePos = 0; lastStep = -1; active = false; gg = 0; t = 0; freeBeat = 0;
    }

protected:
    void processWet (Buf& buf, const BlockInfo& bi) override
    {
        static const double stepBeatsTab[4] = { 1.0, 0.5, 0.25, 0.125 };
        static const int shredTab[4] = { 2, 4, 8, 16 };
        const int n = buf.getNumSamples();
        const int size = ring.getNumSamples();
        const double bpm = juce::jlimit (30.0, 300.0, bi.bpm);
        const double spb = fs * 60.0 / bpm;
        const double stepBeats = stepBeatsTab[juce::jlimit (0, 3, (int) div->load())];
        const int stepLen = (int) (stepBeats * spb);
        const int L = juce::jlimit (64, size / 2, stepLen / shredTab[juce::jlimit (0, 3, (int) shred->load())]);
        const int fade = juce::jmax (8, juce::jmin ((int) (0.002 * fs), L / 4));
        const float ch = chance->load(), rv = rev->load(), m = mix->load();
        const float ggCoef = std::exp (-1.0f / (0.003f * (float) fs));
        const bool useHost = bi.playing && bi.hasPpq;

        auto* L0 = buf.getWritePointer (0);
        auto* L1 = buf.getWritePointer (1);
        auto* r0 = ring.getWritePointer (0);
        auto* r1 = ring.getWritePointer (1);

        for (int i = 0; i < n; ++i)
        {
            const double beat = (useHost ? bi.ppq : freeBeat) + i / spb;
            const auto step = (juce::int64) std::floor (beat / stepBeats);
            if (step != lastStep)
            {
                lastStep = step;
                active = rnd.nextFloat() < ch;
                reverse = rnd.nextFloat() < rv;
                anchor = writePos;
                t = 0;
            }

            r0[writePos] = L0[i];
            r1[writePos] = L1[i];

            const int k = t % L;
            int src;
            float w;
            if (t < L) { src = anchor + k; w = juce::jmin (1.0f, (float) (L - 1 - k) / (float) fade); }
            else
            {
                src = reverse ? anchor + (L - 1 - k) : anchor + k;
                w = juce::jmin (1.0f, juce::jmin ((float) k / (float) fade, (float) (L - 1 - k) / (float) fade));
            }
            src %= size;
            ++t;

            const float tgt = active ? 1.0f : 0.0f;
            gg = tgt + ggCoef * (gg - tgt);
            const float amt = gg * m;
            L0[i] += (r0[src] * w - L0[i]) * amt;
            L1[i] += (r1[src] * w - L1[i]) * amt;

            if (++writePos >= size) writePos = 0;
        }

        if (! useHost)
            freeBeat += n / spb;
    }

private:
    std::atomic<float>*div, *shred, *chance, *rev, *mix;
    Buf ring;
    int writePos = 0, anchor = 0, t = 0;
    juce::int64 lastStep = -1;
    bool active = false, reverse = false;
    float gg = 0;
    double freeBeat = 0;
    juce::Random rnd;
};

// =============================================================================================
// Simple fractional delay line used by the tape and delay units
struct Ring
{
    std::vector<float> d;
    int w = 0;
    void setSize (int n) { d.assign ((size_t) juce::jmax (4, n), 0.0f); w = 0; }
    void clear() { std::fill (d.begin(), d.end(), 0.0f); w = 0; }
    inline void push (float x) noexcept { d[(size_t) w] = x; if (++w >= (int) d.size()) w = 0; }
    inline float read (float delay) const noexcept   // delay in samples, >= 1
    {
        const int size = (int) d.size();
        float pos = (float) w - delay;
        while (pos < 0) pos += (float) size;
        const int i0 = (int) pos;
        const float fr = pos - (float) i0;
        const int i1 = (i0 + 1) % size;
        return d[(size_t) (i0 % size)] + (d[(size_t) i1] - d[(size_t) (i0 % size)]) * fr;
    }
};

// =============================================================================================
// WOW TAPE - wow, flutter, drift, tape-age darkening, saturation and hiss
class TapeModule : public Module
{
public:
    explicit TapeModule (juce::AudioProcessorValueTreeState& s) : Module (s)
    {
        wow = p ("tape_wow"); flut = p ("tape_flutter"); age = p ("tape_age"); mix = p ("tape_mix");
    }
    void prepare (double sr, int mb) override
    {
        Module::prepare (sr, mb);
        for (auto& r : ring) r.setSize ((int) (sr * 0.1));
        driftLP.set (sr, 0.7);
        lastAge = -1;
        reset();
    }
    void reset() override
    {
        for (auto& r : ring) r.clear();
        for (int c = 0; c < 2; ++c) { lp1[c].reset(); lp2[c].reset(); }
        driftLP.reset();
    }

protected:
    void processWet (Buf& buf, const BlockInfo&) override
    {
        const int n = buf.getNumSamples();
        const float w = wow->load(), f = flut->load(), a = age->load(), m = mix->load();
        if (a != lastAge)
        {
            const double cutoff = 18000.0 * (1.0 - a * 0.85);
            for (int c = 0; c < 2; ++c) { lp1[c].set (fs, cutoff); lp2[c].set (fs, cutoff * 1.3); }
            lastAge = a;
        }
        const float sr = (float) fs;
        const float msToS = sr * 0.001f;
        const float sat = 1.0f + a * 2.5f;
        const float satNorm = 1.0f / std::tanh (sat);
        const float hiss = a * a * 0.004f;

        for (int i = 0; i < n; ++i)
        {
            wowPh += 0.55f / sr;  if (wowPh >= 1.0f) wowPh -= 1.0f;
            flPh += 7.3f / sr;    if (flPh >= 1.0f) flPh -= 1.0f;
            fl2Ph += 11.9f / sr;  if (fl2Ph >= 1.0f) fl2Ph -= 1.0f;
            const float drift = driftLP.lp (rnd.nextFloat() * 2.0f - 1.0f) * 30.0f;

            for (int c = 0; c < 2; ++c)
            {
                const float off = c == 0 ? 0.0f : 0.13f;
                const float dMs = 6.0f
                                + w * 3.5f * std::sin (twoPi * (wowPh + off))
                                + w * drift * 0.8f
                                + f * 0.35f * std::sin (twoPi * (flPh + off))
                                + f * 0.12f * std::sin (twoPi * fl2Ph);
                const float dryS = buf.getSample (c, i);
                ring[c].push (dryS);
                float x = ring[c].read (juce::jmax (1.0f, dMs * msToS));
                x = std::tanh (x * sat) * satNorm;
                x = lp2[c].lp (lp1[c].lp (x));
                x += (rnd.nextFloat() * 2.0f - 1.0f) * hiss;
                buf.setSample (c, i, dryS + (x - dryS) * m);
            }
        }
    }

private:
    std::atomic<float>*wow, *flut, *age, *mix;
    Ring ring[2];
    OnePole lp1[2], lp2[2], driftLP;
    float wowPh = 0, flPh = 0, fl2Ph = 0, lastAge = -1;
    juce::Random rnd;
};

// =============================================================================================
// ECHO CHAMBER - tempo-synced dub delay with filtered, saturated feedback and ping-pong
class DelayModule : public Module
{
public:
    explicit DelayModule (juce::AudioProcessorValueTreeState& s) : Module (s)
    {
        sync = p ("delay_sync"); time = p ("delay_time"); fb = p ("delay_feedback");
        tone = p ("delay_tone"); mix = p ("delay_mix"); pp = p ("delay_pingpong");
    }
    void prepare (double sr, int mb) override
    {
        Module::prepare (sr, mb);
        for (auto& r : ring) r.setSize ((int) (sr * 4.5));
        for (auto& h : hp) h.set (sr, 120.0);
        lastTone = -1;
        curDelay = -1;
        reset();
    }
    void reset() override
    {
        for (auto& r : ring) r.clear();
        for (int c = 0; c < 2; ++c) { lp[c].reset(); hp[c].reset(); }
    }

protected:
    void processWet (Buf& buf, const BlockInfo& bi) override
    {
        static const double beatsTab[6] = { 0.0, 1.0, 0.75, 0.5, 1.0 / 3.0, 0.25 };
        const int n = buf.getNumSamples();
        const int s = juce::jlimit (0, 5, (int) sync->load());
        const double bpm = juce::jlimit (30.0, 300.0, bi.bpm);
        const double ms = s == 0 ? (double) time->load() : beatsTab[s] * 60000.0 / bpm;
        const float target = (float) juce::jlimit (1.0, fs * 4.0, ms * 0.001 * fs);
        if (curDelay < 0) curDelay = target;

        const float t = tone->load();
        if (t != lastTone)
        {
            const double cutoff = 800.0 * std::pow (15.0, (double) t);
            for (auto& l : lp) l.set (fs, cutoff);
            lastTone = t;
        }
        const float f = fb->load(), m = mix->load();
        const bool pingpong = pp->load() > 0.5f;
        const float glide = 1.0f - std::exp (-1.0f / (0.08f * (float) fs));

        for (int i = 0; i < n; ++i)
        {
            curDelay += (target - curDelay) * glide;   // smooth time changes = tape-style pitch bend
            const float inL = buf.getSample (0, i), inR = buf.getSample (1, i);
            float wl = ring[0].read (curDelay);
            float wr = ring[1].read (curDelay);

            const float fbl = std::tanh (lp[0].lp (hp[0].hp (wl)) * f * 1.1f);
            const float fbr = std::tanh (lp[1].lp (hp[1].hp (wr)) * f * 1.1f);

            if (pingpong)
            {
                ring[0].push ((inL + inR) * 0.5f + fbr);
                ring[1].push (fbl);
            }
            else
            {
                ring[0].push (inL + fbl);
                ring[1].push (inR + fbr);
            }

            buf.setSample (0, i, inL * (1.0f - m * 0.5f) + wl * m);
            buf.setSample (1, i, inR * (1.0f - m * 0.5f) + wr * m);
        }
    }

private:
    std::atomic<float>*sync, *time, *fb, *tone, *mix, *pp;
    Ring ring[2];
    OnePole lp[2], hp[2];
    float lastTone = -1, curDelay = -1;
};

// =============================================================================================
// VOID HALL
class VerbModule : public Module
{
public:
    explicit VerbModule (juce::AudioProcessorValueTreeState& s) : Module (s)
    {
        size = p ("verb_size"); damp = p ("verb_damp"); width = p ("verb_width"); mix = p ("verb_mix");
    }
    void prepare (double sr, int mb) override
    {
        Module::prepare (sr, mb);
        verb.prepare ({ sr, (juce::uint32) mb, 2 });
        reset();
    }
    void reset() override { verb.reset(); }

protected:
    void processWet (Buf& buf, const BlockInfo&) override
    {
        juce::dsp::Reverb::Parameters pr;
        const float m = mix->load();
        pr.roomSize = size->load();
        pr.damping = damp->load();
        pr.width = width->load();
        pr.wetLevel = m;
        pr.dryLevel = 1.0f - m * 0.6f;
        verb.setParameters (pr);
        juce::dsp::AudioBlock<float> blk (buf);
        verb.process (juce::dsp::ProcessContextReplacing<float> (blk));
    }

private:
    std::atomic<float>*size, *damp, *width, *mix;
    juce::dsp::Reverb verb;
};

// =============================================================================================
std::unique_ptr<Module> createModule (int id, juce::AudioProcessorValueTreeState& s)
{
    switch (id)
    {
        case Gate:   return std::make_unique<GateModule> (s);
        case Boost:  return std::make_unique<BoostModule> (s);
        case Amp:    return std::make_unique<AmpModule> (s);
        case Cab:    return std::make_unique<CabModule> (s);
        case Crush:  return std::make_unique<CrushModule> (s);
        case Glitch: return std::make_unique<GlitchModule> (s);
        case Tape:   return std::make_unique<TapeModule> (s);
        case Delay:  return std::make_unique<DelayModule> (s);
        case Verb:   return std::make_unique<VerbModule> (s);
        default:     return nullptr;
    }
}
} // namespace apex
