#include "OfflineProcessor.h"

#if JUCE_DEBUG

#include <juce_audio_formats/juce_audio_formats.h>
#include "../dsp/AmpSimGold.h"
#include "../dsp/AmpSimPlatinum.h"

#ifdef _WIN32
  #include <windows.h>
  #include <cstdio>
#endif

//==============================================================================
// Attach to the parent console on Windows so std::cout works from cmd/bash.
static void ensureConsoleOutput()
{
#ifdef _WIN32
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        FILE* dummy = nullptr;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
    }
#endif
}

//==============================================================================
bool runOfflineProcessor(const juce::StringArray& args)
{
    int idx = args.indexOf("--process-file");
    if (idx < 0)
        return false;

    ensureConsoleOutput();

    if (idx + 2 >= args.size())
    {
        std::cout << "Usage: OpenRiffBox --process-file input.flac output.flac [options]\n"
                  << "\nOptions:\n"
                  << "  --gain <0-1>           Preamp gain (default: 1.0)\n"
                  << "  --bass <0-1>           Bass (default: 0.5)\n"
                  << "  --mid <0-1>            Mid (default: 0.6)\n"
                  << "  --treble <0-1>         Treble (default: 0.5)\n"
                  << "  --mic-position <0-1>   Mic position (default: 0.5)\n"
                  << "  --cabinet <0-15>       Cabinet (0-13=named, 14=none, 15=custom)\n"
                  << "  --boost                Enable preamp boost (Gold only)\n"
                  << "\nGold internal overrides (experimentation):\n"
                  << "  --x-char-gain <float>  Character stage input gain (default: 0.3)\n"
                  << "  --x-drive-exp <float>  Preamp drive curve exponent (default: 2.0)\n"
                  << "  --x-nfb-gain <float>   Push-pull NFB gain (default: 0.05)\n"
                  << "\n  --engine <gold|platinum> Engine (default: gold)\n"
                  << "  --ov-level <0-1>       OV Level (Platinum, default: 0.7)\n"
                  << "  --master <0-1>         Master volume (Platinum, default: 0.3)\n"
                  << "  --gain-mode <0-1>      0=GAIN1, 1=GAIN2 (Platinum)\n"
                  << "  --stage-limit <1-9>    Tap after stage N (Platinum debug)\n"
                  << std::flush;
        return true;
    }

    juce::File inputFile(args[idx + 1]);
    juce::File outputFile(args[idx + 2]);

    //--------------------------------------------------------------------------
    // Parse parameters
    //--------------------------------------------------------------------------
    auto parseParam = [&](const juce::String& flag, float defaultVal) -> float
    {
        int i = args.indexOf(flag);
        if (i >= 0 && i + 1 < args.size())
            return args[i + 1].getFloatValue();
        return defaultVal;
    };

    auto parseString = [&](const juce::String& flag, const juce::String& defaultVal) -> juce::String
    {
        int i = args.indexOf(flag);
        if (i >= 0 && i + 1 < args.size())
            return args[i + 1];
        return defaultVal;
    };

    juce::String engine = parseString("--engine", "gold");
    bool usePlatinum    = engine.equalsIgnoreCase("platinum");

    float gain         = parseParam("--gain",          1.0f);
    float bass         = parseParam("--bass",          0.5f);
    float mid          = parseParam("--mid",           0.6f);
    float treble       = parseParam("--treble",        0.5f);
    float speakerDrive = parseParam("--speaker-drive", 0.2f);
    float brightness   = parseParam("--brightness",    0.6f);
    float micPosition  = parseParam("--mic-position",  0.5f);
    int   cabinet      = static_cast<int>(parseParam("--cabinet", 0.0f));
    bool  boost        = args.contains("--boost");

    // Platinum-specific params
    float ovLevel      = parseParam("--ov-level",      0.7f);
    float master       = parseParam("--master",        0.3f);
    int   gainMode     = static_cast<int>(parseParam("--gain-mode", 0.0f));

    // Debug: stage limit for noise tracing (Platinum only)
    int   stageLimit   = static_cast<int>(parseParam("--stage-limit", 0.0f));

    // Gold internal overrides
    float xCharGain    = parseParam("--x-char-gain",   0.3f);
    float xDriveExp    = parseParam("--x-drive-exp",   2.0f);
    float xNfbGain     = parseParam("--x-nfb-gain",    0.05f);

    //--------------------------------------------------------------------------
    // Read input file
    //--------------------------------------------------------------------------
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(inputFile));

    if (!reader)
    {
        std::cerr << "ERROR: Cannot read " << inputFile.getFullPathName() << std::endl;
        return true;
    }

    const double sampleRate  = reader->sampleRate;
    const int totalFrames    = static_cast<int>(reader->lengthInSamples);
    const int numChannels    = static_cast<int>(reader->numChannels);
    const int bitsPerSample  = static_cast<int>(reader->bitsPerSample);
    const int outChannels    = juce::jmax(2, numChannels);  // force stereo minimum

    std::cout << "\n=== OpenRiffBox Offline Processor ===\n"
              << "Engine: " << (usePlatinum ? "Platinum" : "Gold") << "\n"
              << "Input:  " << inputFile.getFullPathName() << "\n"
              << "Output: " << outputFile.getFullPathName() << "\n"
              << "Format: " << sampleRate << " Hz, " << bitsPerSample << "-bit, "
              << numChannels << "ch, " << (totalFrames / sampleRate) << "s\n"
              << "\nParameters:\n"
              << "  Gain:          " << gain << "\n"
              << "  Bass:          " << bass << "\n"
              << "  Mid:           " << mid << "\n"
              << "  Treble:        " << treble << "\n"
              << "  Mic Position:  " << micPosition << "\n"
              << "  Cabinet:       " << cabinet
              << " (" << AmpSimGold::getCabinetName(cabinet) << ")\n";
    if (usePlatinum)
    {
        std::cout << "  OV Level:      " << ovLevel << "\n"
                  << "  Master:        " << master << "\n"
                  << "  Gain Mode:     " << (gainMode == 0 ? "GAIN1" : "GAIN2") << "\n";
        if (stageLimit > 0)
            std::cout << "  Stage Limit:   " << stageLimit << " (debug tap)\n";
    }
    else
    {
        std::cout << "  Boost:         " << (boost ? "ON" : "OFF") << "\n";
        if (xCharGain != 0.3f || xDriveExp != 2.0f || xNfbGain != 0.05f)
        {
            std::cout << "  [Internal overrides]\n"
                      << "  x-char-gain:   " << xCharGain << "\n"
                      << "  x-drive-exp:   " << xDriveExp << "\n"
                      << "  x-nfb-gain:    " << xNfbGain << "\n";
        }
    }
    std::cout << std::flush;

    //--------------------------------------------------------------------------
    // Create and configure effect engine
    //--------------------------------------------------------------------------
    std::unique_ptr<EffectProcessor> effect;

    if (usePlatinum)
    {
        auto plat = std::make_unique<AmpSimPlatinum>();
        plat->setBypassed(false);
        plat->setGain(gain);
        plat->setOvLevel(ovLevel);
        plat->setBass(bass);
        plat->setMid(mid);
        plat->setTreble(treble);
        plat->setMaster(master);
        plat->setGainMode(gainMode);
        plat->setMicPosition(micPosition);
        plat->setCabinetType(cabinet);
        if (stageLimit > 0)
            plat->setStageLimit(stageLimit);
        effect = std::move(plat);
    }
    else
    {
        auto gold = std::make_unique<AmpSimGold>();
        gold->setBypassed(false);
        gold->setGain(gain);
        gold->setBass(bass);
        gold->setMid(mid);
        gold->setTreble(treble);
        gold->setSpeakerDrive(speakerDrive);
        gold->setBrightness(brightness);
        gold->setMicPosition(micPosition);
        gold->setCabinetType(cabinet);
        gold->setPreampBoost(boost);
        gold->setInternalCharacterGain(xCharGain);
        gold->setInternalDriveExponent(xDriveExp);
        gold->setInternalNfbGain(xNfbGain);
        effect = std::move(gold);
    }

    const int blockSize = 512;
    effect->prepare(sampleRate, blockSize);

    const int latency = effect->getLatencySamples();
    std::cout << "\nProcessing... (latency: " << latency << " samples, "
              << (1000.0 * latency / sampleRate) << " ms)\n" << std::flush;

    //--------------------------------------------------------------------------
    // Load entire input into memory (stereo)
    //--------------------------------------------------------------------------
    juce::AudioBuffer<float> inputBuffer(outChannels, totalFrames);
    inputBuffer.clear();
    reader->read(&inputBuffer, 0, totalFrames, 0, true, true);

    // Mono input → duplicate to stereo
    if (numChannels == 1)
        inputBuffer.copyFrom(1, 0, inputBuffer, 0, 0, totalFrames);

    //--------------------------------------------------------------------------
    // Process with latency compensation
    //
    // The oversampler + convolution introduce 'latency' samples of delay.
    // Strategy: pad the input with 'latency' samples of silence at the end,
    // process the padded buffer, then skip the first 'latency' output samples.
    // This gives us time-aligned output of the same length as the input.
    //--------------------------------------------------------------------------
    const int paddedFrames = totalFrames + latency;
    juce::AudioBuffer<float> paddedInput(outChannels, paddedFrames);
    paddedInput.clear();
    for (int ch = 0; ch < outChannels; ++ch)
        paddedInput.copyFrom(ch, 0, inputBuffer, ch, 0, totalFrames);
    // Trailing 'latency' samples are already zeroed (silence padding)

    juce::AudioBuffer<float> fullOutput(outChannels, paddedFrames);
    fullOutput.clear();

    juce::AudioBuffer<float> block(outChannels, blockSize);
    int framesProcessed = 0;
    int nextProgressPct = 10;

    while (framesProcessed < paddedFrames)
    {
        const int framesToProcess = juce::jmin(blockSize, paddedFrames - framesProcessed);

        // Copy input chunk into processing block
        block.clear();
        for (int ch = 0; ch < outChannels; ++ch)
            block.copyFrom(ch, 0, paddedInput, ch, framesProcessed,
                           juce::jmin(framesToProcess, blockSize));

        // Handle partial last block: AmpSimGold processes buffer.getNumSamples(),
        // so for partial blocks we need a correctly-sized buffer.
        if (framesToProcess < blockSize)
        {
            juce::AudioBuffer<float> partialBlock(outChannels, framesToProcess);
            for (int ch = 0; ch < outChannels; ++ch)
                partialBlock.copyFrom(ch, 0, paddedInput, ch, framesProcessed, framesToProcess);

            effect->process(partialBlock);

            for (int ch = 0; ch < outChannels; ++ch)
                fullOutput.copyFrom(ch, framesProcessed, partialBlock, ch, 0, framesToProcess);
        }
        else
        {
            effect->process(block);

            for (int ch = 0; ch < outChannels; ++ch)
                fullOutput.copyFrom(ch, framesProcessed, block, ch, 0, blockSize);
        }

        framesProcessed += framesToProcess;

        // Progress indicator
        int pct = static_cast<int>(100.0 * static_cast<double>(framesProcessed) / paddedFrames);
        if (pct >= nextProgressPct)
        {
            std::cout << "  " << pct << "%\n" << std::flush;
            nextProgressPct += 10;
        }
    }

    //--------------------------------------------------------------------------
    // Trim latency: skip the first 'latency' output samples
    //--------------------------------------------------------------------------
    juce::AudioBuffer<float> finalOutput(outChannels, totalFrames);
    for (int ch = 0; ch < outChannels; ++ch)
        finalOutput.copyFrom(ch, 0, fullOutput, ch, latency, totalFrames);

    //--------------------------------------------------------------------------
    // Write output file
    //--------------------------------------------------------------------------
    outputFile.deleteFile();
    std::unique_ptr<juce::FileOutputStream> outputStream(outputFile.createOutputStream());
    if (!outputStream)
    {
        std::cerr << "ERROR: Cannot write to " << outputFile.getFullPathName() << std::endl;
        return true;
    }

    auto* format = formatManager.findFormatForFileExtension(outputFile.getFileExtension());
    if (!format)
    {
        std::cerr << "ERROR: Unsupported output format: "
                  << outputFile.getFileExtension() << std::endl;
        return true;
    }

    // FLAC: always 24-bit for quality. WAV: match input bit depth.
    int outBits = outputFile.getFileExtension().equalsIgnoreCase(".flac") ? 24 : bitsPerSample;

    std::unique_ptr<juce::AudioFormatWriter> writer(
        format->createWriterFor(outputStream.release(), sampleRate,
                                static_cast<unsigned int>(outChannels),
                                outBits, {}, 0));
    if (!writer)
    {
        std::cerr << "ERROR: Cannot create writer for "
                  << outputFile.getFullPathName() << std::endl;
        return true;
    }

    writer->writeFromAudioSampleBuffer(finalOutput, 0, totalFrames);
    writer.reset();  // flush and close

    //--------------------------------------------------------------------------
    // Summary
    //--------------------------------------------------------------------------
    float rmsL  = finalOutput.getRMSLevel(0, 0, totalFrames);
    float rmsR  = finalOutput.getRMSLevel(1, 0, totalFrames);
    float peakL = finalOutput.getMagnitude(0, 0, totalFrames);
    float peakR = finalOutput.getMagnitude(1, 0, totalFrames);

    std::cout << "\nDone! Wrote " << totalFrames << " frames ("
              << (totalFrames / sampleRate) << "s) to "
              << outputFile.getFileName() << "\n"
              << "  RMS:  L=" << juce::Decibels::gainToDecibels(rmsL, -100.0f) << " dB"
              << "  R=" << juce::Decibels::gainToDecibels(rmsR, -100.0f) << " dB\n"
              << "  Peak: L=" << juce::Decibels::gainToDecibels(peakL, -100.0f) << " dB"
              << "  R=" << juce::Decibels::gainToDecibels(peakR, -100.0f) << " dB\n"
              << std::endl;

    return true;
}

#endif
