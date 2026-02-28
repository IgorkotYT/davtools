// registry.hpp
#pragma once
#include "../app.hpp"
#include <string_view>
#include <vector>

std::vector<OutputArtifact> run_converter(
    std::string_view op,
    const std::string& input_name,
    const std::vector<std::uint8_t>& input
);
