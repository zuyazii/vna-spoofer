#pragma once

#include "cli_options.hpp"
#include "librevna_headless/stub_host_core.hpp"

#include <string>

namespace librevna::headless
{
int run_cli(const CLIOptions &options);

std::string usage();

} // namespace librevna::headless
