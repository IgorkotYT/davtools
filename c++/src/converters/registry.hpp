#pragma once
#include "../app.hpp"

#include <string>
#include <string_view>
#include <vector>

struct ConverterStatus {
    std::string name;
    bool enabled = false;
    std::string reason;
};

std::vector<OutputArtifact> run_converter(
    std::string_view op,
    const std::string& input_name,
    const std::vector<std::uint8_t>& input
);

// Runs startup probes + tiny functional tests.
// If disable_broken=true, failing converters are marked disabled.
std::vector<ConverterStatus> converter_self_test_all(bool disable_broken = true);

// Query current state (after tests or lazy init)
std::vector<ConverterStatus> converter_statuses();
bool converter_is_enabled(std::string_view op);
