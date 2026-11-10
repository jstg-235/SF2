#include "sf2/SofaFile.h"

#include <mysofa.h>

#include <algorithm>
#include <cmath>
#include <memory>

namespace sf2
{
namespace
{
    struct HrtfDeleter { void operator() (MYSOFA_HRTF* h) const noexcept { mysofa_free (h); } };
    using HrtfPtr = std::unique_ptr<MYSOFA_HRTF, HrtfDeleter>;

    std::string describe (int err)
    {
        switch (err)
        {
            case MYSOFA_INVALID_FORMAT:        return "not a SOFA file";
            case MYSOFA_UNSUPPORTED_FORMAT:    return "unsupported SOFA format";
            case MYSOFA_NO_MEMORY:             return "out of memory";
            case MYSOFA_READ_ERROR:            return "could not read the file";
            case MYSOFA_INVALID_ATTRIBUTES:    return "missing or invalid SOFA attributes";
            case MYSOFA_INVALID_DIMENSIONS:    return "unexpected data dimensions";
            case MYSOFA_INVALID_DIMENSION_LIST: return "invalid dimension list";
            case MYSOFA_INVALID_COORDINATE_TYPE: return "unsupported coordinate type";
            case MYSOFA_ONLY_EMITTER_WITH_ECI_SUPPORTED: return "only single-emitter files are supported";
            case MYSOFA_ONLY_DELAYS_WITH_IR_OR_MR_SUPPORTED: return "unsupported delay layout";
            case MYSOFA_ONLY_THE_SAME_SAMPLING_RATE_SUPPORTED: return "mixed sampling rates are not supported";
            case MYSOFA_RECEIVERS_WITH_RCI_SUPPORTED: return "unsupported receiver layout";
            case MYSOFA_RECEIVERS_WITH_CARTESIAN_SUPPORTED: return "unsupported receiver coordinates";
            case MYSOFA_INVALID_RECEIVER_POSITIONS: return "invalid ear positions";
            case MYSOFA_ONLY_SOURCES_WITH_MC_SUPPORTED: return "unsupported source layout";
            default: return "error " + std::to_string (err);
        }
    }

    std::optional<HrirSet> convert (HrtfPtr hrtf, int err, std::string& error)
    {
        if (hrtf == nullptr)
        {
            error = describe (err);
            return std::nullopt;
        }
        if (const int check = mysofa_check (hrtf.get()); check != MYSOFA_OK)
        {
            error = describe (check);
            return std::nullopt;
        }
        mysofa_tocartesian (hrtf.get());

        const auto& h = *hrtf;
        if (h.R != 2 || h.N == 0 || h.M == 0 || h.C != 3 || h.DataSamplingRate.elements < 1
            || h.DataIR.elements < h.M * h.R * h.N || h.SourcePosition.elements < h.M * h.C)
        {
            error = "expected two ears and one impulse response per source position";
            return std::nullopt;
        }

        // Optional broadband delays (in samples), per measurement and ear or per ear only, are
        // folded into the responses.
        const bool perMeasurement = h.DataDelay.elements >= h.M * h.R;
        const bool hasDelays = perMeasurement || h.DataDelay.elements >= h.R;
        auto delayOf = [&] (unsigned m, unsigned r)
        {
            const float v = h.DataDelay.values[perMeasurement ? m * h.R + r : r];
            return static_cast<unsigned> (std::lround (std::clamp (v, 0.0f, 4096.0f)));
        };

        HrirSet set;
        set.sampleRate = h.DataSamplingRate.values[0];
        unsigned extra = 0;
        if (hasDelays)
            for (unsigned m = 0; m < h.M; ++m)
                for (unsigned r = 0; r < 2; ++r)
                    extra = std::max (extra, delayOf (m, r));
        set.length = static_cast<int> (h.N + extra);

        set.directions.reserve (h.M);
        set.left.assign (static_cast<std::size_t> (h.M) * static_cast<std::size_t> (set.length), 0.0f);
        set.right.assign (set.left.size(), 0.0f);

        for (unsigned m = 0; m < h.M; ++m)
        {
            const float* p = h.SourcePosition.values + m * 3;
            set.directions.push_back (Vec3 { p[0], p[1], p[2] }.normalised());
            for (unsigned r = 0; r < 2; ++r)
            {
                const float* ir = h.DataIR.values + (m * 2 + r) * h.N;
                const unsigned shift = hasDelays ? delayOf (m, r) : 0;
                auto& dst = r == 0 ? set.left : set.right;
                std::copy (ir, ir + h.N, dst.begin() + static_cast<std::ptrdiff_t> (m * static_cast<unsigned> (set.length) + shift));
            }
        }

        if (! set.isValid())
        {
            error = "the file has no usable impulse responses";
            return std::nullopt;
        }
        return set;
    }
} // namespace

std::optional<HrirSet> loadSofaFile (const std::string& path, std::string& error)
{
    int err = 0;
    HrtfPtr h (mysofa_load (path.c_str(), &err));
    return convert (std::move (h), err, error);
}

std::optional<HrirSet> loadSofaData (const void* data, std::size_t size, std::string& error)
{
    int err = 0;
    HrtfPtr h (mysofa_load_data (static_cast<const char*> (data), size, &err));
    return convert (std::move (h), err, error);
}

} // namespace sf2
