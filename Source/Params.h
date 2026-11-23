#pragma once

namespace ParamIDs
{
    inline constexpr const char* swellLength   = "swellLength";
    inline constexpr const char* roomSize      = "roomSize";
    inline constexpr const char* decay         = "decay";
    inline constexpr const char* damping       = "damping";
    inline constexpr const char* preDelay      = "preDelay";
    inline constexpr const char* width         = "width";
    inline constexpr const char* swellCurve    = "swellCurve";
    inline constexpr const char* swellLevel    = "swellLevel";
    inline constexpr const char* dryLevel      = "dryLevel";
    inline constexpr const char* includeDry    = "includeDry";
    inline constexpr const char* normalize     = "normalize";
    inline constexpr const char* selectionOnly = "selectionOnly";

    inline constexpr const char* all[] = { swellLength, roomSize, decay, damping, preDelay, width,
                                           swellCurve, swellLevel, dryLevel, includeDry, normalize,
                                           selectionOnly };
}
