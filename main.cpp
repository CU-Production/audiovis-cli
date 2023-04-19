#include "LabSound/LabSound.h"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/canvas.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include "ftxui/screen/color.hpp"
#include <thread>
#include <iostream>

enum VisualizerMode
{
    WAVEFORM_OSCILLOSCOPE = 0,
    FREQUENCY_BAR_GRAPH,
    VIS_MODE_COUNT,
};

std::pair<lab::AudioStreamConfig, lab::AudioStreamConfig> GetDefaultAudioDeviceConfiguration(const bool with_input = true);

int main(int argc, char** argv) {
    std::string sndfileName;
    if (argc < 2) {
        printf("No input file.");
        return EXIT_FAILURE;
    }
    sndfileName = argv[1];

    VisualizerMode curremtMode = VisualizerMode::WAVEFORM_OSCILLOSCOPE;

    log_set_level(LOGLEVEL_ERROR);
    const auto defaultAudioDeviceConfigurations = GetDefaultAudioDeviceConfiguration();
    auto context = lab::MakeRealtimeAudioContext(defaultAudioDeviceConfigurations.second, defaultAudioDeviceConfigurations.first);
    lab::AudioContext& ac = *context.get();

    auto audioClipBus = lab::MakeBusFromFile(sndfileName, false);
    auto audioClipNode = std::make_shared<lab::SampledAudioNode>(ac);
    {
        lab::ContextRenderLock r(context.get(), "audioNode setBus");
        audioClipNode->setBus(r, audioClipBus);
    }

    auto analyserNode = std::make_shared<lab::AnalyserNode>(ac);
    {
        lab::ContextRenderLock r(context.get(), "analyserNode setFftSize");
        analyserNode->setFftSize(r, 2048);
    }

    // osc -> analyser -> destination
    context->connect(analyserNode, audioClipNode, 0, 0);
    context->connect(context->device(), analyserNode, 0, 0);

    audioClipNode->schedule(0.0);

    auto bufferLength = analyserNode->frequencyBinCount();
    std::vector<uint8_t> wave(1024);


    // main UI loop
    using namespace ftxui;
    const int CanvesWidth = 200;
    const int CanvesHeight = 50;

    auto board_renderer = CatchEvent(Renderer([&] {
        auto c = Canvas(CanvesWidth, CanvesHeight);

        c.DrawText(0, 0, sndfileName, [](Pixel& p) {
            p.foreground_color = Color::Red;
            p.underlined = true;
        });


        if (curremtMode == VisualizerMode::WAVEFORM_OSCILLOSCOPE)
        {
            const float sliceWidth = CanvesWidth / (float)bufferLength;
            analyserNode->getByteTimeDomainData(wave);

            int x1 = 0;
            int y1 = CanvesHeight / 2;
            int x2 = x1 + sliceWidth;
            int y2 = (wave[0] / 128.0f) * (CanvesHeight / 2);
            c.DrawPointLine(x1, y1, x2, y2, Color::Green);
            for (int i = 1; i < bufferLength; i++) {
                x1 = x2;
                y1 = y2;
                x2 = i * sliceWidth;
                y2 = (wave[i] / 128.0f) * (CanvesHeight / 2);
                c.DrawPointLine(x1, y1, x2, y2, Color::Green);
            }
        }



        return window(text("Audio Visualizer"), canvas(c));
    }), [&](const Event &e) {
        if (e == Event::ArrowLeft) {
            int curmode = (int)curremtMode;
            curmode -= 1;
            if (curmode < 0) {
                curmode = (int)VisualizerMode::VIS_MODE_COUNT - 1;
            }
            curremtMode = (VisualizerMode)curmode;
        }
        if (e == Event::ArrowRight) {
            int curmode = (int)curremtMode;
            curmode += 1;
            if (curmode >= (int)VisualizerMode::VIS_MODE_COUNT) {
                curmode = 0;
            }
            curremtMode = (VisualizerMode)curmode;
        }
        return false;
    });

    auto screen = ScreenInteractive::FitComponent();
    std::atomic<bool> refresh_ui_continue = true;
    std::thread refresh_ui([&] {
        while (refresh_ui_continue) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1.0s / 60.0);// NOLINT magic numbers
            screen.PostEvent(Event::Custom);
        }
    });
    screen.Loop(board_renderer);
    refresh_ui_continue = false;
    refresh_ui.join();

    //    while (true) {
//        analyserNode->getByteTimeDomainData(wave);
//        for (int i = 0; i < bufferLength; i++) {
//            std::cout << (wave[i] / 128.0f);
//        }
//        std::cout << std::endl;
//        std::this_thread::sleep_for(std::chrono::milliseconds (33));
//    }
//
//    std::this_thread::sleep_for(std::chrono::seconds(1));
//
//    std::cout << "Press any key to stop play!" << std::endl;
//    getchar();

    return 0;
}

inline std::pair<lab::AudioStreamConfig, lab::AudioStreamConfig> GetDefaultAudioDeviceConfiguration(const bool with_input)
{
    lab::AudioStreamConfig inputConfig;
    lab::AudioStreamConfig outputConfig;

    const std::vector<lab::AudioDeviceInfo> audioDevices = lab::MakeAudioDeviceList();
    const lab::AudioDeviceIndex default_output_device = lab::GetDefaultOutputAudioDeviceIndex();
    const lab::AudioDeviceIndex default_input_device = lab::GetDefaultInputAudioDeviceIndex();

    lab::AudioDeviceInfo defaultOutputInfo, defaultInputInfo;
    for (auto & info : audioDevices) {
        if (info.index == default_output_device.index)
            defaultOutputInfo = info;
        else if (info.index == default_input_device.index)
            defaultInputInfo = info;
    }

    if (defaultOutputInfo.index != -1) {
        outputConfig.device_index = defaultOutputInfo.index;
        outputConfig.desired_channels = std::min(uint32_t(2), defaultOutputInfo.num_output_channels);
        outputConfig.desired_samplerate = defaultOutputInfo.nominal_samplerate;
    }

    if (with_input) {
        if (defaultInputInfo.index != -1) {
            inputConfig.device_index = defaultInputInfo.index;
            inputConfig.desired_channels = std::min(uint32_t(1), defaultInputInfo.num_input_channels);
            inputConfig.desired_samplerate = defaultInputInfo.nominal_samplerate;
        }
        else {
            throw std::invalid_argument("the default audio input device was requested but none were found");
        }
    }

    // RtAudio doesn't support mismatched input and output rates.
    // this may be a pecularity of RtAudio, but for now, force an RtAudio
    // compatible configuration
    if (defaultOutputInfo.nominal_samplerate != defaultInputInfo.nominal_samplerate) {
        float min_rate = std::min(defaultOutputInfo.nominal_samplerate, defaultInputInfo.nominal_samplerate);
        inputConfig.desired_samplerate = min_rate;
        outputConfig.desired_samplerate = min_rate;
        printf("Warning ~ input and output sample rates don't match, attempting to set minimum");
    }
    return {inputConfig, outputConfig};
}
