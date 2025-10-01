#pragma once

#include <complex>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace librevna::headless
{
struct SweepConfiguration
{
    double start_frequency_hz = 0.0;
    double stop_frequency_hz = 0.0;
    std::uint32_t points = 0;
    double if_bandwidth_hz = 0.0;
    double power_dbm = 0.0;
    double timeout_ms = 0.0;
};

struct StubVNAMeasurement
{
    StubVNAMeasurement(double frequency_hz,
                       std::complex<double> s11,
                       std::complex<double> s21,
                       std::complex<double> s12,
                       std::complex<double> s22);

    double frequency;
    std::map<std::string, std::complex<double>> parameters;

    [[nodiscard]] std::complex<double> get(const std::string &key) const;
};

class StubHostCore
{
public:
    StubHostCore();

    bool connect(const std::string &serial);
    void disconnect();
    [[nodiscard]] bool is_connected() const;

    bool load_calibration(const std::filesystem::path &path);
    [[nodiscard]] std::filesystem::path calibration_file() const;

    std::vector<StubVNAMeasurement> run_sweep(const SweepConfiguration &config);

    [[nodiscard]] std::string last_error_message() const;

private:
    bool connected = false;
    std::filesystem::path calibration_path;
    std::string last_error;
};

} // namespace librevna::headless
