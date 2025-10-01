#include "librevna_headless/stub_host_core.hpp"

#include <cmath>
#include <random>

namespace librevna::headless
{
StubVNAMeasurement::StubVNAMeasurement(double frequency_hz,
                                       std::complex<double> s11,
                                       std::complex<double> s21,
                                       std::complex<double> s12,
                                       std::complex<double> s22)
    : frequency(frequency_hz),
      parameters{{"S11", s11}, {"S21", s21}, {"S12", s12}, {"S22", s22}}
{
}

std::complex<double> StubVNAMeasurement::get(const std::string &key) const
{
    auto it = parameters.find(key);
    if(it == parameters.end())
    {
        return {};
    }
    return it->second;
}

StubHostCore::StubHostCore()
{
}

bool StubHostCore::connect(const std::string &serial)
{
    last_error.clear();
    (void)serial;
    connected = true;
    return true;
}

void StubHostCore::disconnect()
{
    connected = false;
}

bool StubHostCore::is_connected() const
{
    return connected;
}

bool StubHostCore::load_calibration(const std::filesystem::path &path)
{
    last_error.clear();
    if(!std::filesystem::exists(path))
    {
        last_error = "Calibration file not found";
        return false;
    }

    calibration_path = path;
    return true;
}

std::vector<StubVNAMeasurement> StubHostCore::run_sweep(const SweepConfiguration &config)
{
    std::vector<StubVNAMeasurement> result;
    if(config.points == 0)
    {
        return result;
    }
    const double step = (config.stop_frequency_hz - config.start_frequency_hz) /
                        static_cast<double>(config.points > 1 ? config.points - 1 : 1);
    std::mt19937_64 rng(static_cast<std::mt19937_64::result_type>(config.start_frequency_hz) ^
                        static_cast<std::mt19937_64::result_type>(config.stop_frequency_hz));
    std::normal_distribution<double> magnitude_noise(0.0, 0.02);
    std::normal_distribution<double> phase_noise(0.0, 0.01);

    for(std::uint32_t i = 0; i < config.points; ++i)
    {
        const double frequency = config.start_frequency_hz + step * static_cast<double>(i);
        auto make_point = [&](double base_db) {
            const double mag = std::pow(10.0, base_db / 20.0) * (1.0 + magnitude_noise(rng));
            const double phase = phase_noise(rng);
            return std::polar(mag, phase);
        };

        result.emplace_back(
            frequency,
            make_point(-15.0),
            make_point(-3.0),
            make_point(-30.0),
            make_point(-12.0));
    }

    return result;
}

std::string StubHostCore::last_error_message() const
{
    return last_error;
}

std::filesystem::path StubHostCore::calibration_file() const
{
    return calibration_path;
}

} // namespace librevna::headless
