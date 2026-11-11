#pragma once

// Loads head-related impulse responses from a SOFA (AES69) file, e.g. from the SOFA conventions
// database or a personal HRTF measurement. Uses libmysofa. Not real-time safe.

#include <sf2/Binaural.h>

#include <optional>
#include <string>

namespace sf2
{

/** Returns the HRIR set, or nullopt with a readable reason in `error`. */
std::optional<HrirSet> loadSofaFile (const std::string& path, std::string& error);

/** Same, from a file already in memory. */
std::optional<HrirSet> loadSofaData (const void* data, std::size_t size, std::string& error);

} // namespace sf2
