#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nicxlive::core {

// Extension data map mirroring D's puppet.extData (string -> raw bytes)
using PuppetExtData = std::map<std::string, std::vector<uint8_t>>;

} // namespace nicxlive::core
