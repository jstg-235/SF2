#pragma once

// Loudspeaker positions for the channel roles SF2 can decode to.
// Angles follow ITU-R BS.775 / BS.2051 and Dolby Atmos bed conventions; azimuth is positive to the left.


namespace sf2
{

enum class SpeakerRole
{
    Left, Right, Centre, LFE, LFE2,
    LeftSurround, RightSurround,
    LeftSurroundSide, RightSurroundSide,
    LeftSurroundRear, RightSurroundRear,
    LeftCentre, RightCentre, CentreSurround,
    WideLeft, WideRight,
    TopFrontLeft, TopFrontCentre, TopFrontRight,
    TopSideLeft, TopSideRight,
    TopRearLeft, TopRearCentre, TopRearRight,
    TopMiddle
};

struct Speaker
{
    double azimuthDeg   = 0.0;
    double elevationDeg = 0.0;
    bool   isLfe        = false;
};

/** Position of a speaker role. */
Speaker speakerForRole (SpeakerRole role) noexcept;

} // namespace sf2
