#include "LabSound/LabSound.h"

#include <thread>
#include <iostream>

inline std::pair<lab::AudioStreamConfig, lab::AudioStreamConfig> GetDefaultAudioDeviceConfiguration(const bool with_input = true)
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

int main() {
    std::cout << "Hello, World!" << std::endl;

    const auto defaultAudioDeviceConfigurations = GetDefaultAudioDeviceConfiguration();
    auto context = lab::MakeRealtimeAudioContext(defaultAudioDeviceConfigurations.second, defaultAudioDeviceConfigurations.first);
    lab::AudioContext& ac = *context.get();

    auto oscillator = std::make_shared<lab::OscillatorNode>(ac);
    auto gain = std::make_shared<lab::GainNode>(ac);
    gain->gain()->setValue(0.0625);

    // osc -> gain -> destination
    context->connect(gain, oscillator, 0, 0);
    context->connect(context->device(), gain, 0, 0);

    oscillator->frequency()->setValue(440.0f);
    oscillator->setType(lab::OscillatorType::SINE);
    oscillator->start(0.0f);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "Press any key to stop play!" << std::endl;
    getchar();

    return 0;
}
