// Offline check: plays a synthetic guitar riff through every factory preset, writes WAVs,
// reports levels / broken samples, and saves a screenshot of the editor.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/Presets.h"

// Karplus-Strong plucked string, good enough as a fake guitar DI
struct Pluck
{
    std::vector<float> buf; size_t pos = 0; float damp = 0.996f, last = 0;
    void start (double fs, double hz, float amp, juce::Random& r, float d)
    {
        buf.assign ((size_t) juce::jmax (2.0, fs / hz), 0.0f);
        for (auto& s : buf) s = (r.nextFloat() * 2 - 1) * amp;
        pos = 0; damp = d;
    }
    float next()
    {
        if (buf.empty()) return 0;
        const size_t nx = (pos + 1) % buf.size();
        const float y = buf[pos];
        buf[pos] = damp * 0.5f * (buf[pos] + buf[nx]);
        pos = nx;
        return y;
    }
};

static juce::AudioBuffer<float> makeRiff (double fs, double seconds)
{
    juce::AudioBuffer<float> out (1, (int) (fs * seconds));
    out.clear();
    juce::Random r (42);
    const double bpm = 120, eighth = fs * 60.0 / bpm / 2.0;
    std::vector<Pluck> strings (3);
    const double E = 82.41;
    for (int step = 0; step * eighth < out.getNumSamples(); ++step)
    {
        const bool chug = (step % 8) < 6;                 // palm-muted chugs, then open power chord hits
        const double root = (step / 8) % 2 == 0 ? E : E * std::pow (2.0, 3.0 / 12.0);
        const double notes[3] = { root, root * 1.4983, root * 2.0 };
        for (int s = 0; s < 3; ++s)
            strings[(size_t) s].start (fs, notes[s], 0.18f, r, chug ? 0.95f : 0.997f);
        const int a = (int) (step * eighth), b = juce::jmin (out.getNumSamples(), (int) ((step + 1) * eighth));
        for (int i = a; i < b; ++i)
        {
            float x = 0;
            for (auto& st : strings) x += st.next();
            out.setSample (0, i, x);
        }
    }
    return out;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? argv[1] : "render-out");
    outDir.createDirectory();

    const double fs = 48000.0;
    const int block = 512;
    auto riff = makeRiff (fs, 8.0);
    std::printf ("DI peak %.3f\n", riff.getMagnitude (0, 0, riff.getNumSamples()));

    ApexProcessor proc;
    proc.setPlayConfigDetails (2, 2, fs, block);
    proc.prepareToPlay (fs, block);
    std::printf ("latency %d samples\n", proc.getLatencySamples());

    juce::WavAudioFormat wav;
    bool allOk = true;
    const auto& presets = apex::factoryPresets();

    for (int p = 0; p < (int) presets.size(); ++p)
    {
        proc.applyFactoryPreset (p);
        proc.prepareToPlay (fs, block);   // also resets state between presets
        juce::AudioBuffer<float> result (2, riff.getNumSamples());
        juce::MidiBuffer midi;
        int bad = 0;
        for (int pos = 0; pos < riff.getNumSamples(); pos += block)
        {
            const int n = juce::jmin (block, riff.getNumSamples() - pos);
            juce::AudioBuffer<float> b (2, n);
            b.copyFrom (0, 0, riff, 0, pos, n);
            b.copyFrom (1, 0, riff, 0, pos, n);
            proc.processBlock (b, midi);
            for (int c = 0; c < 2; ++c)
            {
                for (int i = 0; i < n; ++i)
                    if (! std::isfinite (b.getSample (c, i))) ++bad;
                result.copyFrom (c, pos, b, c, 0, n);
            }
        }
        const float peak = result.getMagnitude (0, result.getNumSamples());
        const float rms = result.getRMSLevel (0, 0, result.getNumSamples());
        const bool ok = bad == 0 && peak > 0.01f && peak <= 1.0f;
        allOk &= ok;
        std::printf ("%-20s peak %6.1f dB  rms %6.1f dB  %s\n", presets[(size_t) p].name.toRawUTF8(),
                     juce::Decibels::gainToDecibels (peak), juce::Decibels::gainToDecibels (rms), ok ? "ok" : "PROBLEM");

        auto f = outDir.getChildFile (presets[(size_t) p].name.replaceCharacter (' ', '_') + ".wav");
        f.deleteFile();
        std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
        if (auto* w = wav.createWriterFor (os.get(), fs, 2, 24, {}, 0))
        {
            os.release();
            std::unique_ptr<juce::AudioFormatWriter> writer (w);
            writer->writeFromAudioSampleBuffer (result, 0, result.getNumSamples());
        }
    }

    // four-IR blend: make four fake IRs, load them, blend them all, check the result
    {
        juce::Random r (7);
        const char* names[6] = { "V30_SM57_cap", "Greenback_R121", "OS_4x12_MD421", "Dark_1x12_room", "Bass_8x10_D112", "Room_mic_far" };
        for (int slot = 0; slot < 6; ++slot)
        {
            juce::AudioBuffer<float> ir (1, 2048);
            float lp = 0;
            for (int i = 0; i < ir.getNumSamples(); ++i)
            {
                lp += (r.nextFloat() * 2 - 1 - lp) * (0.15f + 0.1f * (float) slot);
                ir.setSample (0, i, lp * std::exp (-(float) i / (150.0f + 120.0f * (float) slot)));
            }
            auto f = outDir.getChildFile (juce::String (names[slot]) + ".wav");
            f.deleteFile();
            std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
            if (auto* w = wav.createWriterFor (os.get(), fs, 1, 24, {}, 0))
            {
                os.release();
                std::unique_ptr<juce::AudioFormatWriter> writer (w);
                writer->writeFromAudioSampleBuffer (ir, 0, ir.getNumSamples());
            }
            if (! proc.loadIR (slot, f)) { std::printf ("IR load FAILED slot %d\n", slot); allOk = false; }
        }
        proc.applyFactoryPreset (1);
        const float mixes[6] = { 0.8f, 0.6f, 0.4f, 0.25f, 0.15f, 0.3f };
        const char* L[6] = { "A", "B", "C", "D", "E", "F" };
        for (int i = 0; i < 6; ++i)
            proc.apvts.getParameter (juce::String ("cab_mix") + L[i])->setValueNotifyingHost (mixes[i]);
        // reference: slot A alone, then the full blend
        for (int pass = 0; pass < 2; ++pass)
        {
        if (pass == 0)
            for (int i = 1; i < 6; ++i) proc.apvts.getParameter (juce::String ("cab_mix") + L[i])->setValueNotifyingHost (0.0f);
        else
            for (int i = 0; i < 6; ++i) proc.apvts.getParameter (juce::String ("cab_mix") + L[i])->setValueNotifyingHost (mixes[i]);
        juce::Thread::sleep (300);   // IRs load in the background
        proc.prepareToPlay (fs, block);
        juce::AudioBuffer<float> b (2, riff.getNumSamples());
        b.copyFrom (0, 0, riff, 0, 0, riff.getNumSamples());
        b.copyFrom (1, 0, riff, 0, 0, riff.getNumSamples());
        juce::MidiBuffer midi;
        for (int pos = 0; pos < b.getNumSamples(); pos += block)
        {
            juce::AudioBuffer<float> sub (b.getArrayOfWritePointers(), 2, pos, juce::jmin (block, b.getNumSamples() - pos));
            proc.processBlock (sub, midi);
        }
        const float pk = b.getMagnitude (0, b.getNumSamples());
        const bool ok = pk > 0.01f && pk <= 1.0f && std::isfinite (pk);
        allOk &= ok;
        std::printf ("%s peak %6.1f dB  rms %6.1f dB  %s\n", pass == 0 ? "IR A alone          " : "6-IR blend          ", juce::Decibels::gainToDecibels (pk),
                     juce::Decibels::gainToDecibels (b.getRMSLevel (0, 0, b.getNumSamples())), ok ? "ok" : "PROBLEM");
        }
    }

    // editor screenshot (keeps the 4-IR blend loaded)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = outDir.getChildFile ("editor.png");
        f.deleteFile();
        juce::FileOutputStream fos (f);
        juce::PNGImageFormat().writeImageToStream (img, fos);
    }

    std::printf (allOk ? "ALL OK\n" : "SOMETHING IS WRONG\n");
    return allOk ? 0 : 1;
}
