#include "sf2/Speakers.h"

namespace sf2
{

Speaker speakerForRole (SpeakerRole role) noexcept
{
    switch (role)
    {
        case SpeakerRole::Left:              return { 30.0, 0.0, false };
        case SpeakerRole::Right:             return { -30.0, 0.0, false };
        case SpeakerRole::Centre:            return { 0.0, 0.0, false };
        case SpeakerRole::LFE:               return { 0.0, 0.0, true };
        case SpeakerRole::LFE2:              return { 0.0, 0.0, true };
        case SpeakerRole::LeftSurround:      return { 110.0, 0.0, false };
        case SpeakerRole::RightSurround:     return { -110.0, 0.0, false };
        case SpeakerRole::LeftSurroundSide:  return { 90.0, 0.0, false };
        case SpeakerRole::RightSurroundSide: return { -90.0, 0.0, false };
        case SpeakerRole::LeftSurroundRear:  return { 150.0, 0.0, false };
        case SpeakerRole::RightSurroundRear: return { -150.0, 0.0, false };
        case SpeakerRole::LeftCentre:        return { 15.0, 0.0, false };
        case SpeakerRole::RightCentre:       return { -15.0, 0.0, false };
        case SpeakerRole::CentreSurround:    return { 180.0, 0.0, false };
        case SpeakerRole::WideLeft:          return { 60.0, 0.0, false };
        case SpeakerRole::WideRight:         return { -60.0, 0.0, false };
        case SpeakerRole::TopFrontLeft:      return { 45.0, 45.0, false };
        case SpeakerRole::TopFrontCentre:    return { 0.0, 45.0, false };
        case SpeakerRole::TopFrontRight:     return { -45.0, 45.0, false };
        case SpeakerRole::TopSideLeft:       return { 90.0, 45.0, false };
        case SpeakerRole::TopSideRight:      return { -90.0, 45.0, false };
        case SpeakerRole::TopRearLeft:       return { 135.0, 45.0, false };
        case SpeakerRole::TopRearCentre:     return { 180.0, 45.0, false };
        case SpeakerRole::TopRearRight:      return { -135.0, 45.0, false };
        case SpeakerRole::TopMiddle:         return { 0.0, 90.0, false };
    }
    return {};
}

} // namespace sf2
