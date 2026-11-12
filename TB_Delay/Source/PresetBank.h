#pragma once

#include <array>
#include <cstddef>

namespace TBDelayParameterIDs
{
inline constexpr const char* time = "time";
inline constexpr const char* offset = "offset";
inline constexpr const char* feedback = "feedback";
inline constexpr const char* spacing = "spacing";
inline constexpr const char* swing = "swing";
inline constexpr const char* width = "width";
inline constexpr const char* mix = "mix";
inline constexpr const char* duck = "duck";
inline constexpr const char* output = "output";
inline constexpr const char* lowCut = "low_cut";
inline constexpr const char* highCut = "high_cut";
inline constexpr const char* drive = "drive";
inline constexpr const char* age = "age";
inline constexpr const char* modRate = "mod_rate";
inline constexpr const char* modDepth = "mod_depth";
inline constexpr const char* smear = "smear";
inline constexpr const char* size = "size";
inline constexpr const char* shift = "shift";
inline constexpr const char* diverge = "diverge";
inline constexpr const char* grain = "grain";
inline constexpr const char* grainSize = "grain_size";
inline constexpr const char* density = "density";
inline constexpr const char* spray = "spray";
inline constexpr const char* pitch = "pitch";
inline constexpr const char* reverse = "reverse";
inline constexpr const char* timeMode = "time_mode";
inline constexpr const char* division = "division";
inline constexpr const char* style = "style";
inline constexpr const char* taps = "taps";
inline constexpr const char* character = "character";
inline constexpr const char* timeMotion = "time_motion";
inline constexpr const char* transformRoute = "transform_route";
inline constexpr const char* freeze = "freeze";
inline constexpr const char* stereoBypass = "stereo_bypass";
inline constexpr const char* colorSpaceBypass = "color_space_bypass";
inline constexpr const char* grainTransformBypass = "grain_transform_bypass";
inline constexpr const char* colorMacro = "color_macro";
inline constexpr const char* spaceMacro = "space_macro";

inline constexpr std::size_t count = 38;

// This order is the contract between APVTS parameters and TBDelayPreset::values.
inline constexpr std::array<const char*, count> all
{
    time, offset, feedback, spacing, swing, width, mix, duck, output,
    lowCut, highCut, drive, age, modRate, modDepth, smear, size, shift,
    diverge, grain, grainSize, density, spray, pitch, reverse, timeMode,
    division, style, taps, character, timeMotion, transformRoute, freeze,
    stereoBypass, colorSpaceBypass, grainTransformBypass,
    colorMacro, spaceMacro
};
} // namespace TBDelayParameterIDs

struct TBDelayPreset
{
    const char* group;
    const char* name;
    std::array<float, TBDelayParameterIDs::count> values;
};

namespace TBDelayPresetDetail
{
enum class Parameter : std::size_t
{
    time,
    offset,
    feedback,
    spacing,
    swing,
    width,
    mix,
    duck,
    output,
    lowCut,
    highCut,
    drive,
    age,
    modRate,
    modDepth,
    smear,
    size,
    shift,
    diverge,
    grain,
    grainSize,
    density,
    spray,
    pitch,
    reverse,
    timeMode,
    division,
    style,
    taps,
    character,
    timeMotion,
    transformRoute,
    freeze,
    stereoBypass,
    colorSpaceBypass,
    grainTransformBypass,
    colorMacro,
    spaceMacro,
    count
};

enum class TimeMode : int
{
    free = 0,
    sync,
    tune
};

// Each note length is ordered Straight, Triplet, Dotted.
enum class Division : int
{
    note1_32 = 0,
    note1_32Triplet,
    note1_32Dotted,
    note1_16,
    note1_16Triplet,
    note1_16Dotted,
    note1_8,
    note1_8Triplet,
    note1_8Dotted,
    note1_4,
    note1_4Triplet,
    note1_4Dotted,
    note1_2,
    note1_2Triplet,
    note1_2Dotted,
    note1_1,
    note1_1Triplet,
    note1_1Dotted
};

enum class Style : int
{
    single = 0,
    dual,
    pingPong,
    ratio
};

enum class Taps : int
{
    one = 0,
    two,
    four,
    eight
};

enum class Character : int
{
    clean = 0,
    tape,
    bbd,
    digital
};

enum class TimeMotion : int
{
    clean = 0,
    tape
};

enum class TransformRoute : int
{
    loop = 0,
    post
};

enum class Freeze : int
{
    off = 0,
    on
};

constexpr float choice(TimeMode value) noexcept
{
    return static_cast<float>(value);
}

constexpr float choice(Division value) noexcept
{
    return static_cast<float>(value);
}

constexpr float choice(Style value) noexcept
{
    return static_cast<float>(value);
}

constexpr float choice(Taps value) noexcept
{
    return static_cast<float>(value);
}

constexpr float choice(Character value) noexcept
{
    return static_cast<float>(value);
}

constexpr float choice(TimeMotion value) noexcept
{
    return static_cast<float>(value);
}

constexpr float choice(TransformRoute value) noexcept
{
    return static_cast<float>(value);
}

constexpr float choice(Freeze value) noexcept
{
    return static_cast<float>(value);
}

struct Edit
{
    Parameter parameter;
    float value;
};

constexpr Edit set(Parameter parameter, float value) noexcept
{
    return { parameter, value };
}

template <typename... Edits>
constexpr std::array<float, TBDelayParameterIDs::count>
configured(std::array<float, TBDelayParameterIDs::count> values, Edits... edits) noexcept
{
    ((values[static_cast<std::size_t>(edits.parameter)] = edits.value), ...);
    return values;
}

inline constexpr std::array<float, TBDelayParameterIDs::count> initValues
{
    375.0f, 0.0f, 48.0f, 100.0f, 0.0f,
    130.0f, 30.0f, 25.0f, 0.0f,
    120.0f, 9000.0f, 18.0f, 28.0f,
    0.25f, 0.8f, 12.0f, 32.0f,
    0.0f, 0.0f,
    0.0f, 90.0f, 4.0f, 15.0f, 0.0f, 0.0f,
    choice(TimeMode::sync),
    choice(Division::note1_8Dotted),
    choice(Style::pingPong),
    choice(Taps::one),
    choice(Character::tape),
    choice(TimeMotion::tape),
    choice(TransformRoute::loop),
    choice(Freeze::off),
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f
};

inline constexpr auto rhythmBase = configured(
    initValues,
    set(Parameter::feedback, 52.0f),
    set(Parameter::width, 150.0f),
    set(Parameter::mix, 32.0f),
    set(Parameter::duck, 24.0f),
    set(Parameter::output, -1.0f),
    set(Parameter::lowCut, 160.0f),
    set(Parameter::highCut, 10500.0f),
    set(Parameter::drive, 10.0f),
    set(Parameter::age, 12.0f),
    set(Parameter::modDepth, 0.4f),
    set(Parameter::smear, 6.0f),
    set(Parameter::size, 28.0f),
    set(Parameter::style, choice(Style::pingPong)),
    set(Parameter::taps, choice(Taps::four)),
    set(Parameter::character, choice(Character::clean)),
    set(Parameter::timeMotion, choice(TimeMotion::clean)));

inline constexpr auto stereoBase = configured(
    initValues,
    set(Parameter::feedback, 42.0f),
    set(Parameter::width, 160.0f),
    set(Parameter::mix, 30.0f),
    set(Parameter::duck, 18.0f),
    set(Parameter::lowCut, 120.0f),
    set(Parameter::highCut, 12000.0f),
    set(Parameter::drive, 6.0f),
    set(Parameter::age, 8.0f),
    set(Parameter::modDepth, 0.3f),
    set(Parameter::smear, 5.0f),
    set(Parameter::division, choice(Division::note1_8)),
    set(Parameter::style, choice(Style::dual)),
    set(Parameter::character, choice(Character::clean)),
    set(Parameter::timeMotion, choice(TimeMotion::clean)));

inline constexpr auto vocalSlapBase = configured(
    initValues,
    set(Parameter::feedback, 32.0f),
    set(Parameter::width, 110.0f),
    set(Parameter::mix, 22.0f),
    set(Parameter::duck, 60.0f),
    set(Parameter::output, -1.0f),
    set(Parameter::lowCut, 240.0f),
    set(Parameter::highCut, 7500.0f),
    set(Parameter::drive, 12.0f),
    set(Parameter::age, 16.0f),
    set(Parameter::modDepth, 0.4f),
    set(Parameter::smear, 5.0f),
    set(Parameter::size, 24.0f),
    set(Parameter::division, choice(Division::note1_8)),
    set(Parameter::style, choice(Style::single)));

inline constexpr auto dubAmbientBase = configured(
    initValues,
    set(Parameter::feedback, 72.0f),
    set(Parameter::width, 160.0f),
    set(Parameter::mix, 38.0f),
    set(Parameter::duck, 35.0f),
    set(Parameter::output, -2.0f),
    set(Parameter::lowCut, 280.0f),
    set(Parameter::highCut, 4800.0f),
    set(Parameter::drive, 42.0f),
    set(Parameter::age, 55.0f),
    set(Parameter::modRate, 0.18f),
    set(Parameter::modDepth, 1.5f),
    set(Parameter::smear, 25.0f),
    set(Parameter::size, 60.0f),
    set(Parameter::division, choice(Division::note1_4Dotted)),
    set(Parameter::style, choice(Style::pingPong)),
    set(Parameter::character, choice(Character::tape)));

inline constexpr auto pitchResonanceBase = configured(
    initValues,
    set(Parameter::feedback, 62.0f),
    set(Parameter::width, 155.0f),
    set(Parameter::mix, 36.0f),
    set(Parameter::duck, 18.0f),
    set(Parameter::output, -2.0f),
    set(Parameter::lowCut, 150.0f),
    set(Parameter::highCut, 11000.0f),
    set(Parameter::drive, 12.0f),
    set(Parameter::age, 10.0f),
    set(Parameter::smear, 20.0f),
    set(Parameter::size, 40.0f),
    set(Parameter::division, choice(Division::note1_8)),
    set(Parameter::style, choice(Style::dual)),
    set(Parameter::character, choice(Character::clean)),
    set(Parameter::timeMotion, choice(TimeMotion::clean)));

inline constexpr auto experimentalBase = configured(
    initValues,
    set(Parameter::feedback, 60.0f),
    set(Parameter::width, 175.0f),
    set(Parameter::mix, 42.0f),
    set(Parameter::duck, 10.0f),
    set(Parameter::output, -4.0f),
    set(Parameter::lowCut, 220.0f),
    set(Parameter::highCut, 8500.0f),
    set(Parameter::drive, 35.0f),
    set(Parameter::age, 35.0f),
    set(Parameter::modRate, 1.2f),
    set(Parameter::modDepth, 2.0f),
    set(Parameter::smear, 30.0f),
    set(Parameter::size, 50.0f),
    set(Parameter::grain, 35.0f),
    set(Parameter::grainSize, 80.0f),
    set(Parameter::density, 6.0f),
    set(Parameter::spray, 50.0f),
    set(Parameter::reverse, 20.0f),
    set(Parameter::division, choice(Division::note1_16Dotted)),
    set(Parameter::style, choice(Style::dual)),
    set(Parameter::taps, choice(Taps::four)),
    set(Parameter::character, choice(Character::digital)),
    set(Parameter::timeMotion, choice(TimeMotion::clean)));

inline constexpr auto spiralShiftBase = configured(
    initValues,
    set(Parameter::feedback, 61.0f),
    set(Parameter::width, 170.0f),
    set(Parameter::mix, 36.0f),
    set(Parameter::duck, 15.0f),
    set(Parameter::output, -2.0f),
    set(Parameter::lowCut, 180.0f),
    set(Parameter::highCut, 11500.0f),
    set(Parameter::drive, 15.0f),
    set(Parameter::age, 12.0f),
    set(Parameter::smear, 25.0f),
    set(Parameter::size, 45.0f),
    set(Parameter::shift, 60.0f),
    set(Parameter::diverge, 20.0f),
    set(Parameter::division, choice(Division::note1_8)),
    set(Parameter::style, choice(Style::pingPong)),
    set(Parameter::character, choice(Character::clean)),
    set(Parameter::timeMotion, choice(TimeMotion::clean)));

inline constexpr auto spaceDiffusionBase = configured(
    initValues,
    set(Parameter::feedback, 58.0f),
    set(Parameter::width, 175.0f),
    set(Parameter::mix, 38.0f),
    set(Parameter::duck, 28.0f),
    set(Parameter::output, -2.0f),
    set(Parameter::lowCut, 250.0f),
    set(Parameter::highCut, 8000.0f),
    set(Parameter::drive, 12.0f),
    set(Parameter::age, 15.0f),
    set(Parameter::smear, 65.0f),
    set(Parameter::size, 80.0f),
    set(Parameter::division, choice(Division::note1_8Dotted)),
    set(Parameter::style, choice(Style::dual)),
    set(Parameter::taps, choice(Taps::four)),
    set(Parameter::character, choice(Character::clean)),
    set(Parameter::timeMotion, choice(TimeMotion::clean)));

inline constexpr auto grainFreezeBase = configured(
    initValues,
    set(Parameter::feedback, 55.0f),
    set(Parameter::width, 180.0f),
    set(Parameter::mix, 42.0f),
    set(Parameter::duck, 20.0f),
    set(Parameter::output, -3.0f),
    set(Parameter::lowCut, 260.0f),
    set(Parameter::highCut, 9500.0f),
    set(Parameter::drive, 10.0f),
    set(Parameter::age, 12.0f),
    set(Parameter::smear, 45.0f),
    set(Parameter::size, 70.0f),
    set(Parameter::grain, 70.0f),
    set(Parameter::grainSize, 150.0f),
    set(Parameter::density, 6.0f),
    set(Parameter::spray, 55.0f),
    set(Parameter::reverse, 20.0f),
    set(Parameter::style, choice(Style::dual)),
    set(Parameter::character, choice(Character::clean)),
    set(Parameter::timeMotion, choice(TimeMotion::clean)),
    set(Parameter::freeze, choice(Freeze::off)));

inline constexpr std::array<TBDelayPreset, 32> originalFactoryPresets
{{
    // ESSENTIAL ECHO
    {
        "ESSENTIAL ECHO", "Init", initValues
    },
    {
        "ESSENTIAL ECHO", "Clean Utility",
        configured(initValues,
                   set(Parameter::time, 250.0f),
                   set(Parameter::feedback, 36.0f),
                   set(Parameter::width, 100.0f),
                   set(Parameter::mix, 22.0f),
                   set(Parameter::duck, 0.0f),
                   set(Parameter::highCut, 18000.0f),
                   set(Parameter::drive, 0.0f),
                   set(Parameter::age, 0.0f),
                   set(Parameter::modDepth, 0.0f),
                   set(Parameter::smear, 0.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::single)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)),
                   set(Parameter::transformRoute, choice(TransformRoute::post)))
    },
    {
        "ESSENTIAL ECHO", "Vocal Pocket",
        configured(initValues,
                   set(Parameter::feedback, 31.0f),
                   set(Parameter::width, 92.0f),
                   set(Parameter::mix, 19.0f),
                   set(Parameter::duck, 58.0f),
                   set(Parameter::lowCut, 220.0f),
                   set(Parameter::highCut, 6800.0f),
                   set(Parameter::drive, 12.0f),
                   set(Parameter::age, 18.0f),
                   set(Parameter::modDepth, 0.35f),
                   set(Parameter::smear, 7.0f),
                   set(Parameter::division, choice(Division::note1_8)),
                   set(Parameter::style, choice(Style::single)))
    },
    {
        "ESSENTIAL ECHO", "Wide Dual Guitar",
        configured(initValues,
                   set(Parameter::offset, 7.0f),
                   set(Parameter::feedback, 42.0f),
                   set(Parameter::width, 176.0f),
                   set(Parameter::mix, 27.0f),
                   set(Parameter::duck, 18.0f),
                   set(Parameter::highCut, 10500.0f),
                   set(Parameter::drive, 9.0f),
                   set(Parameter::age, 14.0f),
                   set(Parameter::modRate, 0.38f),
                   set(Parameter::modDepth, 1.2f),
                   set(Parameter::division, choice(Division::note1_4)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "ESSENTIAL ECHO", "Classic Ping Pong",
        configured(initValues,
                   set(Parameter::feedback, 54.0f),
                   set(Parameter::width, 150.0f),
                   set(Parameter::mix, 32.0f),
                   set(Parameter::duck, 15.0f),
                   set(Parameter::highCut, 12000.0f),
                   set(Parameter::drive, 5.0f),
                   set(Parameter::age, 8.0f),
                   set(Parameter::modDepth, 0.2f),
                   set(Parameter::smear, 3.0f),
                   set(Parameter::division, choice(Division::note1_4Triplet)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "ESSENTIAL ECHO", "Golden Ratio",
        configured(initValues,
                   set(Parameter::offset, 61.8f),
                   set(Parameter::feedback, 49.0f),
                   set(Parameter::width, 165.0f),
                   set(Parameter::mix, 29.0f),
                   set(Parameter::duck, 12.0f),
                   set(Parameter::highCut, 11000.0f),
                   set(Parameter::drive, 8.0f),
                   set(Parameter::age, 12.0f),
                   set(Parameter::modDepth, 0.4f),
                   set(Parameter::smear, 6.0f),
                   set(Parameter::division, choice(Division::note1_8)),
                   set(Parameter::style, choice(Style::ratio)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "ESSENTIAL ECHO", "Tape Slap",
        configured(initValues,
                   set(Parameter::time, 108.0f),
                   set(Parameter::feedback, 14.0f),
                   set(Parameter::width, 82.0f),
                   set(Parameter::mix, 24.0f),
                   set(Parameter::duck, 8.0f),
                   set(Parameter::lowCut, 160.0f),
                   set(Parameter::highCut, 7200.0f),
                   set(Parameter::drive, 31.0f),
                   set(Parameter::age, 42.0f),
                   set(Parameter::modRate, 0.62f),
                   set(Parameter::modDepth, 0.7f),
                   set(Parameter::smear, 2.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::single)))
    },
    {
        "ESSENTIAL ECHO", "Dub Throw",
        configured(initValues,
                   set(Parameter::feedback, 76.0f),
                   set(Parameter::width, 158.0f),
                   set(Parameter::mix, 38.0f),
                   set(Parameter::duck, 42.0f),
                   set(Parameter::lowCut, 280.0f),
                   set(Parameter::highCut, 4300.0f),
                   set(Parameter::drive, 47.0f),
                   set(Parameter::age, 58.0f),
                   set(Parameter::modRate, 0.19f),
                   set(Parameter::modDepth, 1.8f),
                   set(Parameter::smear, 20.0f),
                   set(Parameter::division, choice(Division::note1_4Dotted)))
    },

    // COLOR & MOTION
    {
        "COLOR & MOTION", "Comb Bloom",
        configured(initValues,
                   set(Parameter::time, 10.0f),
                   set(Parameter::offset, 10.0f),
                   set(Parameter::feedback, 79.0f),
                   set(Parameter::width, 124.0f),
                   set(Parameter::mix, 39.0f),
                   set(Parameter::duck, 0.0f),
                   set(Parameter::lowCut, 80.0f),
                   set(Parameter::highCut, 15000.0f),
                   set(Parameter::drive, 13.0f),
                   set(Parameter::age, 0.0f),
                   set(Parameter::modDepth, 0.0f),
                   set(Parameter::smear, 24.0f),
                   set(Parameter::size, 9.0f),
                   set(Parameter::timeMode, choice(TimeMode::tune)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "COLOR & MOTION", "Hollow Notch",
        configured(initValues,
                   set(Parameter::time, 12.5f),
                   set(Parameter::offset, -42.0f),
                   set(Parameter::feedback, 66.0f),
                   set(Parameter::width, 142.0f),
                   set(Parameter::mix, 34.0f),
                   set(Parameter::duck, 0.0f),
                   set(Parameter::lowCut, 170.0f),
                   set(Parameter::highCut, 12500.0f),
                   set(Parameter::drive, 6.0f),
                   set(Parameter::age, 18.0f),
                   set(Parameter::modRate, 0.08f),
                   set(Parameter::modDepth, 0.12f),
                   set(Parameter::smear, 0.0f),
                   set(Parameter::timeMode, choice(TimeMode::tune)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::digital)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "COLOR & MOTION", "Stereo Chorus Drift",
        configured(initValues,
                   set(Parameter::time, 18.0f),
                   set(Parameter::offset, 24.0f),
                   set(Parameter::feedback, 7.0f),
                   set(Parameter::width, 190.0f),
                   set(Parameter::mix, 43.0f),
                   set(Parameter::duck, 0.0f),
                   set(Parameter::highCut, 17000.0f),
                   set(Parameter::drive, 0.0f),
                   set(Parameter::age, 0.0f),
                   set(Parameter::modRate, 0.46f),
                   set(Parameter::modDepth, 5.6f),
                   set(Parameter::smear, 0.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "COLOR & MOTION", "Jet Flange",
        configured(initValues,
                   set(Parameter::time, 12.0f),
                   set(Parameter::offset, -18.0f),
                   set(Parameter::feedback, 72.0f),
                   set(Parameter::width, 154.0f),
                   set(Parameter::mix, 50.0f),
                   set(Parameter::duck, 0.0f),
                   set(Parameter::lowCut, 90.0f),
                   set(Parameter::highCut, 13500.0f),
                   set(Parameter::drive, 19.0f),
                   set(Parameter::age, 6.0f),
                   set(Parameter::modRate, 0.14f),
                   set(Parameter::modDepth, 1.05f),
                   set(Parameter::smear, 0.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "COLOR & MOTION", "Worn Tape",
        configured(initValues,
                   set(Parameter::time, 460.0f),
                   set(Parameter::feedback, 59.0f),
                   set(Parameter::width, 126.0f),
                   set(Parameter::mix, 34.0f),
                   set(Parameter::duck, 14.0f),
                   set(Parameter::lowCut, 180.0f),
                   set(Parameter::highCut, 5100.0f),
                   set(Parameter::drive, 46.0f),
                   set(Parameter::age, 76.0f),
                   set(Parameter::modRate, 0.31f),
                   set(Parameter::modDepth, 2.4f),
                   set(Parameter::smear, 15.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::single)))
    },
    {
        "COLOR & MOTION", "BBD Haze",
        configured(initValues,
                   set(Parameter::time, 310.0f),
                   set(Parameter::offset, 4.0f),
                   set(Parameter::feedback, 55.0f),
                   set(Parameter::width, 138.0f),
                   set(Parameter::mix, 33.0f),
                   set(Parameter::duck, 10.0f),
                   set(Parameter::lowCut, 210.0f),
                   set(Parameter::highCut, 4400.0f),
                   set(Parameter::drive, 35.0f),
                   set(Parameter::age, 64.0f),
                   set(Parameter::modRate, 0.57f),
                   set(Parameter::modDepth, 1.6f),
                   set(Parameter::smear, 10.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::bbd)))
    },
    {
        "COLOR & MOTION", "Digital Crunch",
        configured(initValues,
                   set(Parameter::feedback, 51.0f),
                   set(Parameter::spacing, 68.0f),
                   set(Parameter::swing, -14.0f),
                   set(Parameter::width, 144.0f),
                   set(Parameter::mix, 31.0f),
                   set(Parameter::duck, 5.0f),
                   set(Parameter::lowCut, 150.0f),
                   set(Parameter::highCut, 6900.0f),
                   set(Parameter::drive, 54.0f),
                   set(Parameter::age, 72.0f),
                   set(Parameter::modRate, 3.2f),
                   set(Parameter::modDepth, 0.25f),
                   set(Parameter::smear, 4.0f),
                   set(Parameter::division, choice(Division::note1_16)),
                   set(Parameter::taps, choice(Taps::two)),
                   set(Parameter::character, choice(Character::digital)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "COLOR & MOTION", "Broken Cassette",
        configured(initValues,
                   set(Parameter::time, 225.0f),
                   set(Parameter::offset, -8.0f),
                   set(Parameter::feedback, 47.0f),
                   set(Parameter::spacing, 82.0f),
                   set(Parameter::swing, 11.0f),
                   set(Parameter::width, 118.0f),
                   set(Parameter::mix, 36.0f),
                   set(Parameter::duck, 6.0f),
                   set(Parameter::lowCut, 240.0f),
                   set(Parameter::highCut, 3600.0f),
                   set(Parameter::drive, 66.0f),
                   set(Parameter::age, 93.0f),
                   set(Parameter::modRate, 1.9f),
                   set(Parameter::modDepth, 5.8f),
                   set(Parameter::smear, 18.0f),
                   set(Parameter::grain, 19.0f),
                   set(Parameter::grainSize, 48.0f),
                   set(Parameter::density, 3.0f),
                   set(Parameter::spray, 76.0f),
                   set(Parameter::reverse, 24.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::single)))
    },

    // SPIRAL & SPACE
    {
        "SPIRAL & SPACE", "Spiral Ascender",
        configured(initValues,
                   set(Parameter::feedback, 62.0f),
                   set(Parameter::width, 152.0f),
                   set(Parameter::mix, 35.0f),
                   set(Parameter::duck, 18.0f),
                   set(Parameter::lowCut, 180.0f),
                   set(Parameter::highCut, 11200.0f),
                   set(Parameter::drive, 22.0f),
                   set(Parameter::age, 16.0f),
                   set(Parameter::smear, 22.0f),
                   set(Parameter::size, 44.0f),
                   set(Parameter::shift, 72.0f),
                   set(Parameter::diverge, 12.0f),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "SPIRAL & SPACE", "Spiral Descender",
        configured(initValues,
                   set(Parameter::feedback, 64.0f),
                   set(Parameter::width, 148.0f),
                   set(Parameter::mix, 35.0f),
                   set(Parameter::duck, 16.0f),
                   set(Parameter::lowCut, 140.0f),
                   set(Parameter::highCut, 9400.0f),
                   set(Parameter::drive, 24.0f),
                   set(Parameter::age, 20.0f),
                   set(Parameter::smear, 26.0f),
                   set(Parameter::size, 48.0f),
                   set(Parameter::shift, -86.0f),
                   set(Parameter::diverge, 10.0f),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "SPIRAL & SPACE", "Mirror Orbit",
        configured(initValues,
                   set(Parameter::offset, 15.0f),
                   set(Parameter::feedback, 58.0f),
                   set(Parameter::width, 188.0f),
                   set(Parameter::mix, 37.0f),
                   set(Parameter::duck, 10.0f),
                   set(Parameter::highCut, 12500.0f),
                   set(Parameter::drive, 15.0f),
                   set(Parameter::age, 10.0f),
                   set(Parameter::smear, 18.0f),
                   set(Parameter::size, 38.0f),
                   set(Parameter::shift, 128.0f),
                   set(Parameter::diverge, 100.0f),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "SPIRAL & SPACE", "Diffuse Chamber",
        configured(initValues,
                   set(Parameter::feedback, 57.0f),
                   set(Parameter::spacing, 78.0f),
                   set(Parameter::width, 156.0f),
                   set(Parameter::mix, 34.0f),
                   set(Parameter::duck, 22.0f),
                   set(Parameter::lowCut, 250.0f),
                   set(Parameter::highCut, 7600.0f),
                   set(Parameter::drive, 14.0f),
                   set(Parameter::age, 20.0f),
                   set(Parameter::smear, 69.0f),
                   set(Parameter::size, 56.0f),
                   set(Parameter::division, choice(Division::note1_8Triplet)),
                   set(Parameter::taps, choice(Taps::four)))
    },
    {
        "SPIRAL & SPACE", "Eight Tap Nebula",
        configured(initValues,
                   set(Parameter::feedback, 68.0f),
                   set(Parameter::spacing, 58.0f),
                   set(Parameter::swing, 9.0f),
                   set(Parameter::width, 174.0f),
                   set(Parameter::mix, 40.0f),
                   set(Parameter::duck, 30.0f),
                   set(Parameter::lowCut, 320.0f),
                   set(Parameter::highCut, 5400.0f),
                   set(Parameter::drive, 25.0f),
                   set(Parameter::age, 38.0f),
                   set(Parameter::smear, 86.0f),
                   set(Parameter::size, 106.0f),
                   set(Parameter::division, choice(Division::note1_16Dotted)),
                   set(Parameter::taps, choice(Taps::eight)))
    },
    {
        "SPIRAL & SPACE", "Shimmer Twelve",
        configured(initValues,
                   set(Parameter::feedback, 61.0f),
                   set(Parameter::width, 170.0f),
                   set(Parameter::mix, 39.0f),
                   set(Parameter::duck, 28.0f),
                   set(Parameter::lowCut, 300.0f),
                   set(Parameter::highCut, 13000.0f),
                   set(Parameter::drive, 8.0f),
                   set(Parameter::age, 5.0f),
                   set(Parameter::smear, 72.0f),
                   set(Parameter::size, 88.0f),
                   set(Parameter::grain, 67.0f),
                   set(Parameter::grainSize, 180.0f),
                   set(Parameter::density, 6.0f),
                   set(Parameter::spray, 28.0f),
                   set(Parameter::pitch, 12.0f),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "SPIRAL & SPACE", "Falling Fifth",
        configured(initValues,
                   set(Parameter::feedback, 65.0f),
                   set(Parameter::width, 166.0f),
                   set(Parameter::mix, 38.0f),
                   set(Parameter::duck, 24.0f),
                   set(Parameter::lowCut, 190.0f),
                   set(Parameter::highCut, 7000.0f),
                   set(Parameter::drive, 18.0f),
                   set(Parameter::age, 28.0f),
                   set(Parameter::smear, 62.0f),
                   set(Parameter::size, 78.0f),
                   set(Parameter::grain, 62.0f),
                   set(Parameter::grainSize, 165.0f),
                   set(Parameter::density, 5.0f),
                   set(Parameter::spray, 34.0f),
                   set(Parameter::pitch, -7.0f),
                   set(Parameter::reverse, 12.0f))
    },
    {
        "SPIRAL & SPACE", "Metallic Barber Pole",
        configured(initValues,
                   set(Parameter::feedback, 71.0f),
                   set(Parameter::spacing, 72.0f),
                   set(Parameter::width, 182.0f),
                   set(Parameter::mix, 36.0f),
                   set(Parameter::duck, 12.0f),
                   set(Parameter::lowCut, 360.0f),
                   set(Parameter::highCut, 10400.0f),
                   set(Parameter::drive, 31.0f),
                   set(Parameter::age, 22.0f),
                   set(Parameter::smear, 39.0f),
                   set(Parameter::size, 41.0f),
                   set(Parameter::shift, 360.0f),
                   set(Parameter::diverge, 72.0f),
                   set(Parameter::division, choice(Division::note1_16Triplet)),
                   set(Parameter::taps, choice(Taps::four)),
                   set(Parameter::character, choice(Character::digital)))
    },

    // GRAIN & FREEZE
    {
        "GRAIN & FREEZE", "Grain Cloud",
        configured(initValues,
                   set(Parameter::feedback, 55.0f),
                   set(Parameter::width, 178.0f),
                   set(Parameter::mix, 42.0f),
                   set(Parameter::duck, 20.0f),
                   set(Parameter::lowCut, 260.0f),
                   set(Parameter::highCut, 9800.0f),
                   set(Parameter::drive, 10.0f),
                   set(Parameter::age, 12.0f),
                   set(Parameter::smear, 51.0f),
                   set(Parameter::size, 72.0f),
                   set(Parameter::grain, 74.0f),
                   set(Parameter::grainSize, 145.0f),
                   set(Parameter::density, 6.0f),
                   set(Parameter::spray, 54.0f),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "GRAIN & FREEZE", "Reverse Cloud",
        configured(initValues,
                   set(Parameter::feedback, 52.0f),
                   set(Parameter::width, 184.0f),
                   set(Parameter::mix, 44.0f),
                   set(Parameter::duck, 24.0f),
                   set(Parameter::lowCut, 300.0f),
                   set(Parameter::highCut, 8200.0f),
                   set(Parameter::drive, 8.0f),
                   set(Parameter::age, 16.0f),
                   set(Parameter::smear, 58.0f),
                   set(Parameter::size, 84.0f),
                   set(Parameter::grain, 86.0f),
                   set(Parameter::grainSize, 220.0f),
                   set(Parameter::density, 7.0f),
                   set(Parameter::spray, 69.0f),
                   set(Parameter::reverse, 84.0f),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "GRAIN & FREEZE", "Freeze Ready Constellation",
        configured(initValues,
                   set(Parameter::feedback, 92.0f),
                   set(Parameter::width, 192.0f),
                   set(Parameter::mix, 50.0f),
                   set(Parameter::duck, 0.0f),
                   set(Parameter::lowCut, 340.0f),
                   set(Parameter::highCut, 11500.0f),
                   set(Parameter::drive, 12.0f),
                   set(Parameter::age, 8.0f),
                   set(Parameter::smear, 74.0f),
                   set(Parameter::size, 98.0f),
                   set(Parameter::grain, 82.0f),
                   set(Parameter::grainSize, 260.0f),
                   set(Parameter::density, 8.0f),
                   set(Parameter::spray, 62.0f),
                   set(Parameter::pitch, 7.0f),
                   set(Parameter::reverse, 28.0f),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "GRAIN & FREEZE", "Lo-Fi Stutter",
        configured(initValues,
                   set(Parameter::time, 62.5f),
                   set(Parameter::feedback, 63.0f),
                   set(Parameter::spacing, 36.0f),
                   set(Parameter::swing, 27.0f),
                   set(Parameter::width, 136.0f),
                   set(Parameter::mix, 46.0f),
                   set(Parameter::duck, 0.0f),
                   set(Parameter::lowCut, 210.0f),
                   set(Parameter::highCut, 4200.0f),
                   set(Parameter::drive, 58.0f),
                   set(Parameter::age, 81.0f),
                   set(Parameter::modRate, 6.4f),
                   set(Parameter::modDepth, 0.3f),
                   set(Parameter::smear, 8.0f),
                   set(Parameter::grain, 43.0f),
                   set(Parameter::grainSize, 24.0f),
                   set(Parameter::density, 8.0f),
                   set(Parameter::spray, 37.0f),
                   set(Parameter::reverse, 18.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::single)),
                   set(Parameter::taps, choice(Taps::eight)),
                   set(Parameter::character, choice(Character::digital)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "GRAIN & FREEZE", "Scatter Echo",
        configured(initValues,
                   set(Parameter::feedback, 49.0f),
                   set(Parameter::spacing, 125.0f),
                   set(Parameter::swing, -22.0f),
                   set(Parameter::width, 188.0f),
                   set(Parameter::mix, 41.0f),
                   set(Parameter::duck, 16.0f),
                   set(Parameter::lowCut, 180.0f),
                   set(Parameter::highCut, 12200.0f),
                   set(Parameter::drive, 17.0f),
                   set(Parameter::age, 18.0f),
                   set(Parameter::smear, 29.0f),
                   set(Parameter::size, 45.0f),
                   set(Parameter::grain, 57.0f),
                   set(Parameter::grainSize, 76.0f),
                   set(Parameter::density, 7.0f),
                   set(Parameter::spray, 96.0f),
                   set(Parameter::pitch, 5.0f),
                   set(Parameter::reverse, 36.0f),
                   set(Parameter::division, choice(Division::note1_16Dotted)),
                   set(Parameter::taps, choice(Taps::four)),
                   set(Parameter::transformRoute, choice(TransformRoute::post)))
    },
    {
        "GRAIN & FREEZE", "Micrograin Chorus",
        configured(initValues,
                   set(Parameter::time, 24.0f),
                   set(Parameter::offset, 19.0f),
                   set(Parameter::feedback, 18.0f),
                   set(Parameter::width, 196.0f),
                   set(Parameter::mix, 47.0f),
                   set(Parameter::duck, 0.0f),
                   set(Parameter::highCut, 16000.0f),
                   set(Parameter::drive, 0.0f),
                   set(Parameter::age, 0.0f),
                   set(Parameter::modRate, 0.73f),
                   set(Parameter::modDepth, 3.2f),
                   set(Parameter::smear, 16.0f),
                   set(Parameter::size, 18.0f),
                   set(Parameter::grain, 39.0f),
                   set(Parameter::grainSize, 18.0f),
                   set(Parameter::density, 8.0f),
                   set(Parameter::spray, 23.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)),
                   set(Parameter::transformRoute, choice(TransformRoute::post)))
    },
    {
        "GRAIN & FREEZE", "Tape Granules",
        configured(initValues,
                   set(Parameter::time, 285.0f),
                   set(Parameter::offset, -5.0f),
                   set(Parameter::feedback, 58.0f),
                   set(Parameter::width, 146.0f),
                   set(Parameter::mix, 39.0f),
                   set(Parameter::duck, 12.0f),
                   set(Parameter::lowCut, 230.0f),
                   set(Parameter::highCut, 4700.0f),
                   set(Parameter::drive, 49.0f),
                   set(Parameter::age, 78.0f),
                   set(Parameter::modRate, 0.84f),
                   set(Parameter::modDepth, 2.8f),
                   set(Parameter::smear, 31.0f),
                   set(Parameter::size, 52.0f),
                   set(Parameter::grain, 52.0f),
                   set(Parameter::grainSize, 68.0f),
                   set(Parameter::density, 5.0f),
                   set(Parameter::spray, 72.0f),
                   set(Parameter::reverse, 19.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)))
    },
    {
        "GRAIN & FREEZE", "Freeze Ready Reverse Swell",
        configured(initValues,
                   set(Parameter::feedback, 100.0f),
                   set(Parameter::spacing, 142.0f),
                   set(Parameter::width, 200.0f),
                   set(Parameter::mix, 55.0f),
                   set(Parameter::duck, 0.0f),
                   set(Parameter::lowCut, 420.0f),
                   set(Parameter::highCut, 6400.0f),
                   set(Parameter::drive, 27.0f),
                   set(Parameter::age, 35.0f),
                   set(Parameter::smear, 89.0f),
                   set(Parameter::size, 116.0f),
                   set(Parameter::grain, 100.0f),
                   set(Parameter::grainSize, 310.0f),
                   set(Parameter::density, 8.0f),
                   set(Parameter::spray, 84.0f),
                   set(Parameter::pitch, -12.0f),
                   set(Parameter::reverse, 100.0f),
                   set(Parameter::taps, choice(Taps::eight)),
                   set(Parameter::transformRoute, choice(TransformRoute::post)))
    }
}};

inline constexpr std::size_t factoryPresetsPerGroup = 10;
using FactoryPresetGroup =
    std::array<TBDelayPreset, factoryPresetsPerGroup>;

inline constexpr FactoryPresetGroup essentialEchoPresets
{{
    { "ESSENTIAL ECHO", "Init", originalFactoryPresets[0].values },
    { "ESSENTIAL ECHO", "Clean Utility", originalFactoryPresets[1].values },
    { "ESSENTIAL ECHO", "Vocal Pocket", originalFactoryPresets[2].values },
    { "ESSENTIAL ECHO", "Wide Dual Guitar", originalFactoryPresets[3].values },
    { "ESSENTIAL ECHO", "Classic Ping Pong", originalFactoryPresets[4].values },
    { "ESSENTIAL ECHO", "Golden Ratio", originalFactoryPresets[5].values },
    { "ESSENTIAL ECHO", "Tape Slap", originalFactoryPresets[6].values },
    { "ESSENTIAL ECHO", "Dub Throw", originalFactoryPresets[7].values },
    {
        "ESSENTIAL ECHO", "Mono Safe Quarter",
        configured(initValues,
                   set(Parameter::feedback, 34.0f),
                   set(Parameter::width, 0.0f),
                   set(Parameter::mix, 22.0f),
                   set(Parameter::duck, 20.0f),
                   set(Parameter::highCut, 16000.0f),
                   set(Parameter::drive, 0.0f),
                   set(Parameter::age, 0.0f),
                   set(Parameter::modDepth, 0.0f),
                   set(Parameter::smear, 0.0f),
                   set(Parameter::division, choice(Division::note1_4)),
                   set(Parameter::style, choice(Style::single)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "ESSENTIAL ECHO", "Clean Dotted Eighth",
        configured(initValues,
                   set(Parameter::feedback, 42.0f),
                   set(Parameter::width, 145.0f),
                   set(Parameter::mix, 26.0f),
                   set(Parameter::duck, 30.0f),
                   set(Parameter::highCut, 15000.0f),
                   set(Parameter::drive, 2.0f),
                   set(Parameter::age, 0.0f),
                   set(Parameter::modDepth, 0.15f),
                   set(Parameter::smear, 2.0f),
                   set(Parameter::division, choice(Division::note1_8Dotted)),
                   set(Parameter::style, choice(Style::pingPong)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    }
}};

inline constexpr FactoryPresetGroup slapDoublePresets
{{
    {
        "SLAP & DOUBLE", "Studio Slap 80",
        configured(vocalSlapBase,
                   set(Parameter::time, 80.0f),
                   set(Parameter::feedback, 8.0f),
                   set(Parameter::width, 80.0f),
                   set(Parameter::mix, 20.0f),
                   set(Parameter::duck, 45.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "SLAP & DOUBLE", "Vintage Slap 120",
        configured(vocalSlapBase,
                   set(Parameter::time, 120.0f),
                   set(Parameter::feedback, 15.0f),
                   set(Parameter::width, 90.0f),
                   set(Parameter::mix, 24.0f),
                   set(Parameter::drive, 32.0f),
                   set(Parameter::age, 48.0f),
                   set(Parameter::highCut, 6500.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)))
    },
    {
        "SLAP & DOUBLE", "Dark Vocal Slap",
        configured(vocalSlapBase,
                   set(Parameter::time, 96.0f),
                   set(Parameter::feedback, 12.0f),
                   set(Parameter::width, 70.0f),
                   set(Parameter::mix, 21.0f),
                   set(Parameter::duck, 65.0f),
                   set(Parameter::lowCut, 280.0f),
                   set(Parameter::highCut, 4200.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)))
    },
    {
        "SLAP & DOUBLE", "Bright Guitar Slap",
        configured(vocalSlapBase,
                   set(Parameter::time, 112.0f),
                   set(Parameter::feedback, 18.0f),
                   set(Parameter::width, 115.0f),
                   set(Parameter::mix, 23.0f),
                   set(Parameter::duck, 15.0f),
                   set(Parameter::lowCut, 180.0f),
                   set(Parameter::highCut, 15000.0f),
                   set(Parameter::drive, 8.0f),
                   set(Parameter::age, 5.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "SLAP & DOUBLE", "Stereo Double 18",
        configured(vocalSlapBase,
                   set(Parameter::time, 18.0f),
                   set(Parameter::offset, 22.0f),
                   set(Parameter::feedback, 0.0f),
                   set(Parameter::width, 185.0f),
                   set(Parameter::mix, 38.0f),
                   set(Parameter::duck, 0.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "SLAP & DOUBLE", "Loose Double 28",
        configured(vocalSlapBase,
                   set(Parameter::time, 28.0f),
                   set(Parameter::offset, -18.0f),
                   set(Parameter::feedback, 4.0f),
                   set(Parameter::width, 190.0f),
                   set(Parameter::mix, 40.0f),
                   set(Parameter::duck, 0.0f),
                   set(Parameter::modRate, 0.4f),
                   set(Parameter::modDepth, 4.8f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::dual)))
    },
    {
        "SLAP & DOUBLE", "Microshift Double",
        configured(vocalSlapBase,
                   set(Parameter::time, 24.0f),
                   set(Parameter::offset, 15.0f),
                   set(Parameter::feedback, 3.0f),
                   set(Parameter::width, 195.0f),
                   set(Parameter::mix, 40.0f),
                   set(Parameter::duck, 0.0f),
                   set(Parameter::shift, 18.0f),
                   set(Parameter::diverge, 100.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)),
                   set(Parameter::transformRoute, choice(TransformRoute::post)))
    },
    {
        "SLAP & DOUBLE", "Stereo Chorus Drift",
        originalFactoryPresets[10].values
    },
    {
        "SLAP & DOUBLE", "Reverse Tail Slap",
        configured(vocalSlapBase,
                   set(Parameter::time, 105.0f),
                   set(Parameter::feedback, 13.0f),
                   set(Parameter::width, 130.0f),
                   set(Parameter::mix, 27.0f),
                   set(Parameter::grain, 28.0f),
                   set(Parameter::grainSize, 35.0f),
                   set(Parameter::density, 4.0f),
                   set(Parameter::spray, 35.0f),
                   set(Parameter::reverse, 85.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::transformRoute, choice(TransformRoute::post)))
    },
    {
        "SLAP & DOUBLE", "Wide ADT",
        configured(vocalSlapBase,
                   set(Parameter::time, 35.0f),
                   set(Parameter::offset, 28.0f),
                   set(Parameter::feedback, 6.0f),
                   set(Parameter::width, 200.0f),
                   set(Parameter::mix, 42.0f),
                   set(Parameter::duck, 0.0f),
                   set(Parameter::age, 25.0f),
                   set(Parameter::modRate, 0.55f),
                   set(Parameter::modDepth, 5.5f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::dual)))
    }
}};

inline constexpr FactoryPresetGroup rhythmMultitapPresets
{{
    {
        "RHYTHM & MULTITAP", "Straight Eighth Grid",
        configured(rhythmBase,
                   set(Parameter::feedback, 45.0f),
                   set(Parameter::spacing, 100.0f),
                   set(Parameter::swing, 0.0f),
                   set(Parameter::division, choice(Division::note1_8)),
                   set(Parameter::style, choice(Style::single)),
                   set(Parameter::taps, choice(Taps::four)))
    },
    {
        "RHYTHM & MULTITAP", "Dotted Pulse Train",
        configured(rhythmBase,
                   set(Parameter::feedback, 50.0f),
                   set(Parameter::spacing, 100.0f),
                   set(Parameter::drive, 18.0f),
                   set(Parameter::division, choice(Division::note1_8Dotted)),
                   set(Parameter::taps, choice(Taps::four)))
    },
    {
        "RHYTHM & MULTITAP", "Triplet Relay",
        configured(rhythmBase,
                   set(Parameter::feedback, 54.0f),
                   set(Parameter::spacing, 92.0f),
                   set(Parameter::swing, 18.0f),
                   set(Parameter::division, choice(Division::note1_4Triplet)),
                   set(Parameter::taps, choice(Taps::four)))
    },
    {
        "RHYTHM & MULTITAP", "Sixteenth Cascade",
        configured(rhythmBase,
                   set(Parameter::feedback, 47.0f),
                   set(Parameter::spacing, 62.0f),
                   set(Parameter::swing, 22.0f),
                   set(Parameter::division, choice(Division::note1_16)),
                   set(Parameter::taps, choice(Taps::eight)))
    },
    {
        "RHYTHM & MULTITAP", "Quarter Note Call",
        configured(rhythmBase,
                   set(Parameter::feedback, 38.0f),
                   set(Parameter::spacing, 145.0f),
                   set(Parameter::width, 125.0f),
                   set(Parameter::mix, 27.0f),
                   set(Parameter::division, choice(Division::note1_4)),
                   set(Parameter::style, choice(Style::single)),
                   set(Parameter::taps, choice(Taps::two)))
    },
    {
        "RHYTHM & MULTITAP", "Late Swing Taps",
        configured(rhythmBase,
                   set(Parameter::feedback, 51.0f),
                   set(Parameter::spacing, 104.0f),
                   set(Parameter::swing, 35.0f),
                   set(Parameter::division, choice(Division::note1_8)),
                   set(Parameter::taps, choice(Taps::four)))
    },
    {
        "RHYTHM & MULTITAP", "Early Swing Taps",
        configured(rhythmBase,
                   set(Parameter::feedback, 49.0f),
                   set(Parameter::spacing, 96.0f),
                   set(Parameter::swing, -35.0f),
                   set(Parameter::division, choice(Division::note1_8)),
                   set(Parameter::taps, choice(Taps::four)))
    },
    {
        "RHYTHM & MULTITAP", "Contracting Steps",
        configured(rhythmBase,
                   set(Parameter::feedback, 56.0f),
                   set(Parameter::spacing, 45.0f),
                   set(Parameter::swing, -10.0f),
                   set(Parameter::division, choice(Division::note1_8Dotted)),
                   set(Parameter::taps, choice(Taps::eight)))
    },
    {
        "RHYTHM & MULTITAP", "Expanding Steps",
        configured(rhythmBase,
                   set(Parameter::feedback, 57.0f),
                   set(Parameter::spacing, 175.0f),
                   set(Parameter::swing, 10.0f),
                   set(Parameter::division, choice(Division::note1_16Dotted)),
                   set(Parameter::taps, choice(Taps::four)))
    },
    {
        "RHYTHM & MULTITAP", "Eight Tap Machine",
        configured(rhythmBase,
                   set(Parameter::feedback, 60.0f),
                   set(Parameter::spacing, 74.0f),
                   set(Parameter::swing, 8.0f),
                   set(Parameter::width, 185.0f),
                   set(Parameter::smear, 2.0f),
                   set(Parameter::division, choice(Division::note1_16)),
                   set(Parameter::style, choice(Style::ratio)),
                   set(Parameter::taps, choice(Taps::eight)),
                   set(Parameter::character, choice(Character::digital)))
    }
}};

inline constexpr FactoryPresetGroup pingPongStereoPresets
{{
    {
        "PING PONG & STEREO", "Narrow Relay",
        configured(stereoBase,
                   set(Parameter::feedback, 44.0f),
                   set(Parameter::width, 70.0f),
                   set(Parameter::mix, 26.0f),
                   set(Parameter::division, choice(Division::note1_8)),
                   set(Parameter::style, choice(Style::pingPong)))
    },
    {
        "PING PONG & STEREO", "Panoramic Relay",
        configured(stereoBase,
                   set(Parameter::feedback, 58.0f),
                   set(Parameter::width, 200.0f),
                   set(Parameter::mix, 35.0f),
                   set(Parameter::division, choice(Division::note1_4Dotted)),
                   set(Parameter::style, choice(Style::pingPong)))
    },
    {
        "PING PONG & STEREO", "Lazy Right Return",
        configured(stereoBase,
                   set(Parameter::offset, 38.0f),
                   set(Parameter::feedback, 46.0f),
                   set(Parameter::width, 165.0f),
                   set(Parameter::division, choice(Division::note1_4)),
                   set(Parameter::style, choice(Style::dual)))
    },
    {
        "PING PONG & STEREO", "Fast Right Return",
        configured(stereoBase,
                   set(Parameter::offset, -32.0f),
                   set(Parameter::feedback, 43.0f),
                   set(Parameter::width, 170.0f),
                   set(Parameter::division, choice(Division::note1_8Dotted)),
                   set(Parameter::style, choice(Style::dual)))
    },
    {
        "PING PONG & STEREO", "Dual Counterstep",
        configured(stereoBase,
                   set(Parameter::offset, 16.0f),
                   set(Parameter::feedback, 50.0f),
                   set(Parameter::spacing, 72.0f),
                   set(Parameter::swing, -18.0f),
                   set(Parameter::width, 185.0f),
                   set(Parameter::division, choice(Division::note1_8Triplet)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::taps, choice(Taps::two)))
    },
    {
        "PING PONG & STEREO", "Ratio Crosscurrent",
        configured(stereoBase,
                   set(Parameter::offset, 61.8f),
                   set(Parameter::feedback, 52.0f),
                   set(Parameter::width, 180.0f),
                   set(Parameter::division, choice(Division::note1_4Triplet)),
                   set(Parameter::style, choice(Style::ratio)))
    },
    {
        "PING PONG & STEREO", "Opposed Orbit",
        configured(stereoBase,
                   set(Parameter::offset, 12.0f),
                   set(Parameter::feedback, 54.0f),
                   set(Parameter::width, 195.0f),
                   set(Parameter::shift, 110.0f),
                   set(Parameter::diverge, 100.0f),
                   set(Parameter::style, choice(Style::dual)))
    },
    {
        "PING PONG & STEREO", "Diffuse Ping Pong",
        configured(stereoBase,
                   set(Parameter::feedback, 55.0f),
                   set(Parameter::width, 185.0f),
                   set(Parameter::smear, 62.0f),
                   set(Parameter::size, 72.0f),
                   set(Parameter::division, choice(Division::note1_8Dotted)),
                   set(Parameter::style, choice(Style::pingPong)))
    },
    {
        "PING PONG & STEREO", "Mono Input Expander",
        configured(stereoBase,
                   set(Parameter::time, 32.0f),
                   set(Parameter::offset, 20.0f),
                   set(Parameter::feedback, 16.0f),
                   set(Parameter::width, 178.0f),
                   set(Parameter::mix, 34.0f),
                   set(Parameter::modDepth, 1.8f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::dual)))
    },
    {
        "PING PONG & STEREO", "Stereo Dub Circle",
        configured(stereoBase,
                   set(Parameter::feedback, 78.0f),
                   set(Parameter::width, 190.0f),
                   set(Parameter::mix, 38.0f),
                   set(Parameter::duck, 44.0f),
                   set(Parameter::output, -3.0f),
                   set(Parameter::lowCut, 300.0f),
                   set(Parameter::highCut, 4300.0f),
                   set(Parameter::drive, 46.0f),
                   set(Parameter::age, 60.0f),
                   set(Parameter::division, choice(Division::note1_2)),
                   set(Parameter::style, choice(Style::pingPong)),
                   set(Parameter::character, choice(Character::tape)))
    }
}};

inline constexpr FactoryPresetGroup tapeAnalogPresets
{{
    {
        "TAPE & ANALOG", "Fresh Tape Repeat",
        configured(initValues,
                   set(Parameter::feedback, 40.0f),
                   set(Parameter::width, 135.0f),
                   set(Parameter::mix, 28.0f),
                   set(Parameter::duck, 20.0f),
                   set(Parameter::highCut, 11000.0f),
                   set(Parameter::drive, 14.0f),
                   set(Parameter::age, 18.0f),
                   set(Parameter::modDepth, 0.5f),
                   set(Parameter::smear, 4.0f),
                   set(Parameter::division, choice(Division::note1_8)),
                   set(Parameter::style, choice(Style::single)),
                   set(Parameter::character, choice(Character::tape)))
    },
    {
        "TAPE & ANALOG", "Warm Reel Echo",
        configured(initValues,
                   set(Parameter::feedback, 55.0f),
                   set(Parameter::width, 145.0f),
                   set(Parameter::mix, 32.0f),
                   set(Parameter::duck, 24.0f),
                   set(Parameter::highCut, 6500.0f),
                   set(Parameter::drive, 32.0f),
                   set(Parameter::age, 40.0f),
                   set(Parameter::modRate, 0.4f),
                   set(Parameter::modDepth, 1.0f),
                   set(Parameter::smear, 10.0f),
                   set(Parameter::division, choice(Division::note1_4)),
                   set(Parameter::style, choice(Style::single)),
                   set(Parameter::character, choice(Character::tape)))
    },
    { "TAPE & ANALOG", "Worn Tape", originalFactoryPresets[12].values },
    {
        "TAPE & ANALOG", "Saturated Tape Loop",
        configured(dubAmbientBase,
                   set(Parameter::feedback, 78.0f),
                   set(Parameter::mix, 36.0f),
                   set(Parameter::output, -4.0f),
                   set(Parameter::highCut, 5200.0f),
                   set(Parameter::drive, 65.0f),
                   set(Parameter::age, 45.0f),
                   set(Parameter::smear, 18.0f),
                   set(Parameter::division, choice(Division::note1_4)),
                   set(Parameter::style, choice(Style::single)))
    },
    {
        "TAPE & ANALOG", "Slow Wow Memory",
        configured(dubAmbientBase,
                   set(Parameter::time, 650.0f),
                   set(Parameter::feedback, 60.0f),
                   set(Parameter::width, 130.0f),
                   set(Parameter::mix, 34.0f),
                   set(Parameter::highCut, 5800.0f),
                   set(Parameter::age, 65.0f),
                   set(Parameter::modRate, 0.08f),
                   set(Parameter::modDepth, 5.5f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::single)))
    },
    {
        "TAPE & ANALOG", "Fast Flutter Reel",
        configured(dubAmbientBase,
                   set(Parameter::time, 360.0f),
                   set(Parameter::feedback, 52.0f),
                   set(Parameter::width, 140.0f),
                   set(Parameter::mix, 31.0f),
                   set(Parameter::highCut, 7200.0f),
                   set(Parameter::drive, 28.0f),
                   set(Parameter::age, 38.0f),
                   set(Parameter::modRate, 5.2f),
                   set(Parameter::modDepth, 0.45f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::dual)))
    },
    {
        "TAPE & ANALOG", "Broken Cassette",
        originalFactoryPresets[15].values
    },
    { "TAPE & ANALOG", "BBD Haze", originalFactoryPresets[13].values },
    {
        "TAPE & ANALOG", "BBD Chorus Echo",
        configured(initValues,
                   set(Parameter::time, 240.0f),
                   set(Parameter::offset, 8.0f),
                   set(Parameter::feedback, 48.0f),
                   set(Parameter::width, 180.0f),
                   set(Parameter::mix, 35.0f),
                   set(Parameter::duck, 12.0f),
                   set(Parameter::lowCut, 180.0f),
                   set(Parameter::highCut, 4600.0f),
                   set(Parameter::drive, 25.0f),
                   set(Parameter::age, 55.0f),
                   set(Parameter::modRate, 0.8f),
                   set(Parameter::modDepth, 3.8f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::bbd)))
    },
    {
        "TAPE & ANALOG", "Dark Bucket Trail",
        configured(initValues,
                   set(Parameter::feedback, 69.0f),
                   set(Parameter::width, 150.0f),
                   set(Parameter::mix, 35.0f),
                   set(Parameter::duck, 28.0f),
                   set(Parameter::output, -3.0f),
                   set(Parameter::lowCut, 300.0f),
                   set(Parameter::highCut, 2600.0f),
                   set(Parameter::drive, 38.0f),
                   set(Parameter::age, 72.0f),
                   set(Parameter::modRate, 0.34f),
                   set(Parameter::modDepth, 1.4f),
                   set(Parameter::division, choice(Division::note1_4Dotted)),
                   set(Parameter::style, choice(Style::pingPong)),
                   set(Parameter::character, choice(Character::bbd)))
    }
}};

inline constexpr FactoryPresetGroup digitalLofiPresets
{{
    {
        "DIGITAL & LO-FI", "Pristine Digital",
        configured(initValues,
                   set(Parameter::feedback, 42.0f),
                   set(Parameter::width, 150.0f),
                   set(Parameter::mix, 27.0f),
                   set(Parameter::duck, 18.0f),
                   set(Parameter::highCut, 19000.0f),
                   set(Parameter::drive, 0.0f),
                   set(Parameter::age, 0.0f),
                   set(Parameter::modDepth, 0.0f),
                   set(Parameter::smear, 0.0f),
                   set(Parameter::division, choice(Division::note1_8)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::digital)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "DIGITAL & LO-FI", "Digital Crunch",
        originalFactoryPresets[14].values
    },
    {
        "DIGITAL & LO-FI", "12-Bit Repeat",
        configured(experimentalBase,
                   set(Parameter::feedback, 50.0f),
                   set(Parameter::mix, 32.0f),
                   set(Parameter::output, -2.0f),
                   set(Parameter::highCut, 7600.0f),
                   set(Parameter::drive, 25.0f),
                   set(Parameter::age, 55.0f),
                   set(Parameter::modDepth, 0.2f),
                   set(Parameter::smear, 5.0f),
                   set(Parameter::grain, 0.0f),
                   set(Parameter::reverse, 0.0f),
                   set(Parameter::division, choice(Division::note1_8)),
                   set(Parameter::taps, choice(Taps::two)))
    },
    {
        "DIGITAL & LO-FI", "6-Bit Collapse",
        configured(experimentalBase,
                   set(Parameter::feedback, 65.0f),
                   set(Parameter::mix, 38.0f),
                   set(Parameter::output, -6.0f),
                   set(Parameter::lowCut, 260.0f),
                   set(Parameter::highCut, 5000.0f),
                   set(Parameter::drive, 75.0f),
                   set(Parameter::age, 100.0f),
                   set(Parameter::modRate, 4.5f),
                   set(Parameter::modDepth, 0.15f),
                   set(Parameter::grain, 12.0f),
                   set(Parameter::spray, 18.0f),
                   set(Parameter::reverse, 8.0f))
    },
    {
        "DIGITAL & LO-FI", "Clocked Ping Pong",
        configured(experimentalBase,
                   set(Parameter::feedback, 58.0f),
                   set(Parameter::mix, 34.0f),
                   set(Parameter::output, -3.0f),
                   set(Parameter::drive, 40.0f),
                   set(Parameter::age, 70.0f),
                   set(Parameter::modRate, 4.0f),
                   set(Parameter::modDepth, 0.2f),
                   set(Parameter::grain, 0.0f),
                   set(Parameter::reverse, 0.0f),
                   set(Parameter::style, choice(Style::pingPong)))
    },
    {
        "DIGITAL & LO-FI", "Telephone Memory",
        configured(experimentalBase,
                   set(Parameter::feedback, 62.0f),
                   set(Parameter::mix, 35.0f),
                   set(Parameter::output, -3.0f),
                   set(Parameter::lowCut, 300.0f),
                   set(Parameter::highCut, 3200.0f),
                   set(Parameter::drive, 48.0f),
                   set(Parameter::age, 68.0f),
                   set(Parameter::grain, 0.0f),
                   set(Parameter::reverse, 0.0f),
                   set(Parameter::division, choice(Division::note1_4)))
    },
    {
        "DIGITAL & LO-FI", "Narrowband Pager",
        configured(experimentalBase,
                   set(Parameter::feedback, 46.0f),
                   set(Parameter::spacing, 68.0f),
                   set(Parameter::mix, 30.0f),
                   set(Parameter::output, -3.0f),
                   set(Parameter::lowCut, 900.0f),
                   set(Parameter::highCut, 5500.0f),
                   set(Parameter::drive, 36.0f),
                   set(Parameter::age, 65.0f),
                   set(Parameter::grain, 0.0f),
                   set(Parameter::reverse, 0.0f),
                   set(Parameter::division, choice(Division::note1_16)),
                   set(Parameter::taps, choice(Taps::four)))
    },
    {
        "DIGITAL & LO-FI", "Lo-Fi Stutter",
        originalFactoryPresets[27].values
    },
    {
        "DIGITAL & LO-FI", "Buffer Skip",
        configured(experimentalBase,
                   set(Parameter::time, 90.0f),
                   set(Parameter::feedback, 55.0f),
                   set(Parameter::spacing, 40.0f),
                   set(Parameter::swing, 32.0f),
                   set(Parameter::grain, 65.0f),
                   set(Parameter::grainSize, 18.0f),
                   set(Parameter::density, 8.0f),
                   set(Parameter::spray, 22.0f),
                   set(Parameter::reverse, 15.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::taps, choice(Taps::eight)))
    },
    {
        "DIGITAL & LO-FI", "Pixel Dust",
        configured(experimentalBase,
                   set(Parameter::feedback, 48.0f),
                   set(Parameter::mix, 39.0f),
                   set(Parameter::output, -5.0f),
                   set(Parameter::highCut, 6800.0f),
                   set(Parameter::drive, 58.0f),
                   set(Parameter::age, 85.0f),
                   set(Parameter::grain, 45.0f),
                   set(Parameter::grainSize, 32.0f),
                   set(Parameter::density, 5.0f),
                   set(Parameter::spray, 80.0f),
                   set(Parameter::reverse, 35.0f),
                   set(Parameter::division, choice(Division::note1_16Dotted)))
    }
}};

inline constexpr FactoryPresetGroup combResonancePresets
{{
    {
        "COMB & RESONANCE", "Comb Bloom",
        originalFactoryPresets[8].values
    },
    {
        "COMB & RESONANCE", "Hollow Notch",
        originalFactoryPresets[9].values
    },
    {
        "COMB & RESONANCE", "Plucked Wire",
        configured(pitchResonanceBase,
                   set(Parameter::time, 10.0f),
                   set(Parameter::feedback, 84.0f),
                   set(Parameter::width, 110.0f),
                   set(Parameter::mix, 30.0f),
                   set(Parameter::output, -5.0f),
                   set(Parameter::highCut, 13000.0f),
                   set(Parameter::smear, 8.0f),
                   set(Parameter::size, 18.0f),
                   set(Parameter::timeMode, choice(TimeMode::tune)),
                   set(Parameter::style, choice(Style::single)))
    },
    {
        "COMB & RESONANCE", "Glass String",
        configured(pitchResonanceBase,
                   set(Parameter::time, 11.5f),
                   set(Parameter::feedback, 78.0f),
                   set(Parameter::width, 150.0f),
                   set(Parameter::mix, 32.0f),
                   set(Parameter::output, -4.0f),
                   set(Parameter::lowCut, 500.0f),
                   set(Parameter::highCut, 18000.0f),
                   set(Parameter::drive, 4.0f),
                   set(Parameter::age, 0.0f),
                   set(Parameter::smear, 12.0f),
                   set(Parameter::timeMode, choice(TimeMode::tune)),
                   set(Parameter::style, choice(Style::dual)))
    },
    {
        "COMB & RESONANCE", "Bass Tube",
        configured(pitchResonanceBase,
                   set(Parameter::time, 30.0f),
                   set(Parameter::feedback, 88.0f),
                   set(Parameter::width, 95.0f),
                   set(Parameter::mix, 34.0f),
                   set(Parameter::output, -6.0f),
                   set(Parameter::lowCut, 40.0f),
                   set(Parameter::highCut, 6000.0f),
                   set(Parameter::drive, 20.0f),
                   set(Parameter::age, 18.0f),
                   set(Parameter::timeMode, choice(TimeMode::tune)),
                   set(Parameter::style, choice(Style::single)))
    },
    {
        "COMB & RESONANCE", "Damped Resonator",
        configured(pitchResonanceBase,
                   set(Parameter::time, 17.0f),
                   set(Parameter::feedback, 65.0f),
                   set(Parameter::width, 125.0f),
                   set(Parameter::mix, 29.0f),
                   set(Parameter::output, -2.0f),
                   set(Parameter::lowCut, 260.0f),
                   set(Parameter::highCut, 5000.0f),
                   set(Parameter::smear, 4.0f),
                   set(Parameter::timeMode, choice(TimeMode::tune)),
                   set(Parameter::style, choice(Style::single)))
    },
    {
        "COMB & RESONANCE", "Subtle Flange",
        configured(pitchResonanceBase,
                   set(Parameter::time, 14.0f),
                   set(Parameter::offset, -10.0f),
                   set(Parameter::feedback, 28.0f),
                   set(Parameter::width, 150.0f),
                   set(Parameter::mix, 35.0f),
                   set(Parameter::modRate, 0.2f),
                   set(Parameter::modDepth, 0.6f),
                   set(Parameter::smear, 0.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::dual)))
    },
    {
        "COMB & RESONANCE", "Jet Flange",
        originalFactoryPresets[11].values
    },
    {
        "COMB & RESONANCE", "Stereo Comb Split",
        configured(pitchResonanceBase,
                   set(Parameter::time, 12.0f),
                   set(Parameter::offset, 35.0f),
                   set(Parameter::feedback, 76.0f),
                   set(Parameter::width, 190.0f),
                   set(Parameter::mix, 36.0f),
                   set(Parameter::output, -4.0f),
                   set(Parameter::smear, 10.0f),
                   set(Parameter::timeMode, choice(TimeMode::tune)),
                   set(Parameter::style, choice(Style::dual)))
    },
    {
        "COMB & RESONANCE", "Digital Bell Bank",
        configured(pitchResonanceBase,
                   set(Parameter::time, 15.7f),
                   set(Parameter::feedback, 73.0f),
                   set(Parameter::spacing, 68.0f),
                   set(Parameter::width, 180.0f),
                   set(Parameter::mix, 35.0f),
                   set(Parameter::output, -5.0f),
                   set(Parameter::drive, 24.0f),
                   set(Parameter::age, 42.0f),
                   set(Parameter::shift, 45.0f),
                   set(Parameter::diverge, 60.0f),
                   set(Parameter::timeMode, choice(TimeMode::tune)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::taps, choice(Taps::four)),
                   set(Parameter::character, choice(Character::digital)))
    }
}};

inline constexpr FactoryPresetGroup spiralShiftPresets
{{
    {
        "SPIRAL & SHIFT", "Spiral Ascender",
        originalFactoryPresets[16].values
    },
    {
        "SPIRAL & SHIFT", "Spiral Descender",
        originalFactoryPresets[17].values
    },
    {
        "SPIRAL & SHIFT", "Mirror Orbit",
        originalFactoryPresets[18].values
    },
    {
        "SPIRAL & SHIFT", "Metallic Barber Pole",
        originalFactoryPresets[23].values
    },
    {
        "SPIRAL & SHIFT", "Gentle Lift",
        configured(spiralShiftBase,
                   set(Parameter::feedback, 55.0f),
                   set(Parameter::shift, 16.0f),
                   set(Parameter::diverge, 8.0f),
                   set(Parameter::smear, 15.0f),
                   set(Parameter::size, 34.0f),
                   set(Parameter::division, choice(Division::note1_8)))
    },
    {
        "SPIRAL & SHIFT", "Gentle Fall",
        configured(spiralShiftBase,
                   set(Parameter::feedback, 57.0f),
                   set(Parameter::shift, -18.0f),
                   set(Parameter::diverge, 10.0f),
                   set(Parameter::smear, 17.0f),
                   set(Parameter::size, 36.0f),
                   set(Parameter::division, choice(Division::note1_8Dotted)))
    },
    {
        "SPIRAL & SHIFT", "Opposed Currents",
        configured(spiralShiftBase,
                   set(Parameter::feedback, 62.0f),
                   set(Parameter::width, 192.0f),
                   set(Parameter::shift, 90.0f),
                   set(Parameter::diverge, 100.0f),
                   set(Parameter::division, choice(Division::note1_4Triplet)),
                   set(Parameter::style, choice(Style::pingPong)))
    },
    {
        "SPIRAL & SHIFT", "Post Shift Halo",
        configured(spiralShiftBase,
                   set(Parameter::feedback, 48.0f),
                   set(Parameter::smear, 52.0f),
                   set(Parameter::size, 68.0f),
                   set(Parameter::shift, 180.0f),
                   set(Parameter::diverge, 45.0f),
                   set(Parameter::grain, 20.0f),
                   set(Parameter::grainSize, 120.0f),
                   set(Parameter::transformRoute, choice(TransformRoute::post)))
    },
    {
        "SPIRAL & SHIFT", "Ping Pong Helix",
        configured(spiralShiftBase,
                   set(Parameter::feedback, 68.0f),
                   set(Parameter::spacing, 80.0f),
                   set(Parameter::width, 190.0f),
                   set(Parameter::output, -4.0f),
                   set(Parameter::shift, 240.0f),
                   set(Parameter::diverge, 25.0f),
                   set(Parameter::division, choice(Division::note1_8Triplet)),
                   set(Parameter::style, choice(Style::pingPong)),
                   set(Parameter::taps, choice(Taps::two)))
    },
    {
        "SPIRAL & SHIFT", "Grain Shift Dust",
        configured(spiralShiftBase,
                   set(Parameter::feedback, 58.0f),
                   set(Parameter::width, 185.0f),
                   set(Parameter::shift, -120.0f),
                   set(Parameter::diverge, 70.0f),
                   set(Parameter::grain, 55.0f),
                   set(Parameter::grainSize, 50.0f),
                   set(Parameter::density, 7.0f),
                   set(Parameter::spray, 75.0f),
                   set(Parameter::pitch, 3.0f),
                   set(Parameter::reverse, 20.0f))
    }
}};

inline constexpr FactoryPresetGroup spaceDiffusionPresets
{{
    {
        "SPACE & DIFFUSION", "Small Room Bloom",
        configured(spaceDiffusionBase,
                   set(Parameter::feedback, 35.0f),
                   set(Parameter::width, 120.0f),
                   set(Parameter::mix, 30.0f),
                   set(Parameter::duck, 12.0f),
                   set(Parameter::smear, 45.0f),
                   set(Parameter::size, 18.0f),
                   set(Parameter::division, choice(Division::note1_16)),
                   set(Parameter::style, choice(Style::single)),
                   set(Parameter::taps, choice(Taps::four)))
    },
    {
        "SPACE & DIFFUSION", "Diffuse Chamber",
        originalFactoryPresets[19].values
    },
    {
        "SPACE & DIFFUSION", "Dark Hallway",
        configured(spaceDiffusionBase,
                   set(Parameter::feedback, 70.0f),
                   set(Parameter::width, 130.0f),
                   set(Parameter::mix, 39.0f),
                   set(Parameter::output, -4.0f),
                   set(Parameter::lowCut, 300.0f),
                   set(Parameter::highCut, 3800.0f),
                   set(Parameter::drive, 24.0f),
                   set(Parameter::age, 38.0f),
                   set(Parameter::smear, 55.0f),
                   set(Parameter::size, 65.0f),
                   set(Parameter::division, choice(Division::note1_4)),
                   set(Parameter::style, choice(Style::single)))
    },
    {
        "SPACE & DIFFUSION", "Bright Plate Trail",
        configured(spaceDiffusionBase,
                   set(Parameter::feedback, 48.0f),
                   set(Parameter::width, 165.0f),
                   set(Parameter::mix, 36.0f),
                   set(Parameter::lowCut, 420.0f),
                   set(Parameter::highCut, 16000.0f),
                   set(Parameter::drive, 4.0f),
                   set(Parameter::age, 0.0f),
                   set(Parameter::smear, 82.0f),
                   set(Parameter::size, 35.0f),
                   set(Parameter::division, choice(Division::note1_8Triplet)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::taps, choice(Taps::eight)))
    },
    {
        "SPACE & DIFFUSION", "Eight Tap Nebula",
        originalFactoryPresets[20].values
    },
    {
        "SPACE & DIFFUSION", "Wide Ambient Wash",
        configured(spaceDiffusionBase,
                   set(Parameter::feedback, 65.0f),
                   set(Parameter::width, 200.0f),
                   set(Parameter::mix, 45.0f),
                   set(Parameter::output, -4.0f),
                   set(Parameter::highCut, 9500.0f),
                   set(Parameter::smear, 90.0f),
                   set(Parameter::size, 110.0f),
                   set(Parameter::grain, 10.0f),
                   set(Parameter::grainSize, 180.0f),
                   set(Parameter::division, choice(Division::note1_2)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::taps, choice(Taps::eight)))
    },
    {
        "SPACE & DIFFUSION", "Mono Fog",
        configured(spaceDiffusionBase,
                   set(Parameter::feedback, 62.0f),
                   set(Parameter::width, 0.0f),
                   set(Parameter::mix, 40.0f),
                   set(Parameter::output, -3.0f),
                   set(Parameter::lowCut, 250.0f),
                   set(Parameter::highCut, 4500.0f),
                   set(Parameter::smear, 85.0f),
                   set(Parameter::size, 100.0f),
                   set(Parameter::division, choice(Division::note1_4Dotted)),
                   set(Parameter::style, choice(Style::single)))
    },
    {
        "SPACE & DIFFUSION", "Rhythmic Bloom",
        configured(spaceDiffusionBase,
                   set(Parameter::feedback, 55.0f),
                   set(Parameter::spacing, 75.0f),
                   set(Parameter::swing, 12.0f),
                   set(Parameter::width, 180.0f),
                   set(Parameter::smear, 60.0f),
                   set(Parameter::size, 45.0f),
                   set(Parameter::division, choice(Division::note1_8)),
                   set(Parameter::style, choice(Style::pingPong)),
                   set(Parameter::taps, choice(Taps::four)))
    },
    {
        "SPACE & DIFFUSION", "Deep Space Return",
        configured(spaceDiffusionBase,
                   set(Parameter::feedback, 88.0f),
                   set(Parameter::width, 190.0f),
                   set(Parameter::mix, 48.0f),
                   set(Parameter::output, -6.0f),
                   set(Parameter::lowCut, 420.0f),
                   set(Parameter::highCut, 5600.0f),
                   set(Parameter::drive, 26.0f),
                   set(Parameter::age, 35.0f),
                   set(Parameter::smear, 95.0f),
                   set(Parameter::size, 120.0f),
                   set(Parameter::grain, 15.0f),
                   set(Parameter::division, choice(Division::note1_1)),
                   set(Parameter::style, choice(Style::pingPong)),
                   set(Parameter::taps, choice(Taps::eight)))
    },
    {
        "SPACE & DIFFUSION", "Ducking Atmosphere",
        configured(spaceDiffusionBase,
                   set(Parameter::feedback, 74.0f),
                   set(Parameter::width, 195.0f),
                   set(Parameter::mix, 42.0f),
                   set(Parameter::duck, 80.0f),
                   set(Parameter::output, -4.0f),
                   set(Parameter::highCut, 7500.0f),
                   set(Parameter::smear, 88.0f),
                   set(Parameter::size, 105.0f),
                   set(Parameter::division, choice(Division::note1_2Dotted)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::taps, choice(Taps::four)))
    }
}};

inline constexpr FactoryPresetGroup grainFreezePresets
{{
    {
        "GRAIN & FREEZE", "Grain Cloud",
        originalFactoryPresets[24].values
    },
    {
        "GRAIN & FREEZE", "Reverse Cloud",
        originalFactoryPresets[25].values
    },
    {
        "GRAIN & FREEZE", "Freeze Ready Constellation",
        originalFactoryPresets[26].values
    },
    {
        "GRAIN & FREEZE", "Scatter Echo",
        originalFactoryPresets[28].values
    },
    {
        "GRAIN & FREEZE", "Micrograin Chorus",
        originalFactoryPresets[29].values
    },
    {
        "GRAIN & FREEZE", "Tape Granules",
        originalFactoryPresets[30].values
    },
    {
        "GRAIN & FREEZE", "Freeze Ready Reverse Swell",
        originalFactoryPresets[31].values
    },
    {
        "GRAIN & FREEZE", "Shimmer Twelve",
        originalFactoryPresets[21].values
    },
    {
        "GRAIN & FREEZE", "Falling Fifth",
        originalFactoryPresets[22].values
    },
    {
        "GRAIN & FREEZE", "Two Octave Spark",
        configured(grainFreezeBase,
                   set(Parameter::feedback, 45.0f),
                   set(Parameter::width, 190.0f),
                   set(Parameter::mix, 45.0f),
                   set(Parameter::output, -5.0f),
                   set(Parameter::highCut, 16000.0f),
                   set(Parameter::drive, 4.0f),
                   set(Parameter::age, 0.0f),
                   set(Parameter::smear, 35.0f),
                   set(Parameter::size, 55.0f),
                   set(Parameter::grain, 80.0f),
                   set(Parameter::grainSize, 45.0f),
                   set(Parameter::density, 8.0f),
                   set(Parameter::spray, 40.0f),
                   set(Parameter::pitch, 24.0f),
                   set(Parameter::reverse, 15.0f),
                   set(Parameter::transformRoute, choice(TransformRoute::post)))
    }
}};

inline constexpr FactoryPresetGroup performanceUtilityPresets
{{
    {
        "PERFORMANCE & UTILITY", "Vocal Throw Quarter",
        configured(initValues,
                   set(Parameter::feedback, 48.0f),
                   set(Parameter::width, 110.0f),
                   set(Parameter::mix, 22.0f),
                   set(Parameter::duck, 75.0f),
                   set(Parameter::lowCut, 180.0f),
                   set(Parameter::highCut, 7000.0f),
                   set(Parameter::drive, 4.0f),
                   set(Parameter::age, 8.0f),
                   set(Parameter::division, choice(Division::note1_4)),
                   set(Parameter::style, choice(Style::single)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)),
                   set(Parameter::colorMacro, -55.0f),
                   set(Parameter::spaceMacro, -35.0f))
    },
    {
        "PERFORMANCE & UTILITY", "Long Feedback Safe",
        configured(initValues,
                   set(Parameter::feedback, 78.0f),
                   set(Parameter::width, 150.0f),
                   set(Parameter::mix, 26.0f),
                   set(Parameter::duck, 25.0f),
                   set(Parameter::lowCut, 120.0f),
                   set(Parameter::highCut, 9000.0f),
                   set(Parameter::drive, 6.0f),
                   set(Parameter::age, 15.0f),
                   set(Parameter::division, choice(Division::note1_2)),
                   set(Parameter::style, choice(Style::single)),
                   set(Parameter::taps, choice(Taps::two)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "PERFORMANCE & UTILITY", "Mono Haas Bloom",
        configured(initValues,
                   set(Parameter::time, 20.0f),
                   set(Parameter::offset, 35.0f),
                   set(Parameter::feedback, 5.0f),
                   set(Parameter::width, 185.0f),
                   set(Parameter::mix, 28.0f),
                   set(Parameter::duck, 0.0f),
                   set(Parameter::lowCut, 120.0f),
                   set(Parameter::highCut, 16000.0f),
                   set(Parameter::modRate, 0.8f),
                   set(Parameter::modDepth, 3.5f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "PERFORMANCE & UTILITY", "True L/R Ping",
        configured(stereoBase,
                   set(Parameter::offset, -45.0f),
                   set(Parameter::feedback, 48.0f),
                   set(Parameter::width, 200.0f),
                   set(Parameter::mix, 31.0f),
                   set(Parameter::division, choice(Division::note1_8)),
                   set(Parameter::style, choice(Style::pingPong)),
                   set(Parameter::taps, choice(Taps::two)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "PERFORMANCE & UTILITY", "Tight 1/32 Grid",
        configured(rhythmBase,
                   set(Parameter::feedback, 38.0f),
                   set(Parameter::spacing, 45.0f),
                   set(Parameter::swing, 0.0f),
                   set(Parameter::width, 120.0f),
                   set(Parameter::mix, 30.0f),
                   set(Parameter::duck, 15.0f),
                   set(Parameter::lowCut, 140.0f),
                   set(Parameter::highCut, 15000.0f),
                   set(Parameter::division, choice(Division::note1_32)),
                   set(Parameter::style, choice(Style::single)),
                   set(Parameter::taps, choice(Taps::eight)),
                   set(Parameter::character, choice(Character::clean)))
    },
    {
        "PERFORMANCE & UTILITY", "3:2 Polyrhythm",
        configured(rhythmBase,
                   set(Parameter::feedback, 52.0f),
                   set(Parameter::spacing, 150.0f),
                   set(Parameter::swing, 0.0f),
                   set(Parameter::width, 180.0f),
                   set(Parameter::mix, 34.0f),
                   set(Parameter::division, choice(Division::note1_8)),
                   set(Parameter::style, choice(Style::ratio)),
                   set(Parameter::taps, choice(Taps::four)),
                   set(Parameter::character, choice(Character::clean)))
    },
    {
        "PERFORMANCE & UTILITY", "Ducking Drum Gate",
        configured(initValues,
                   set(Parameter::time, 110.0f),
                   set(Parameter::feedback, 48.0f),
                   set(Parameter::width, 130.0f),
                   set(Parameter::mix, 28.0f),
                   set(Parameter::duck, 85.0f),
                   set(Parameter::lowCut, 100.0f),
                   set(Parameter::highCut, 9000.0f),
                   set(Parameter::drive, 30.0f),
                   set(Parameter::age, 20.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::single)),
                   set(Parameter::taps, choice(Taps::two)),
                   set(Parameter::character, choice(Character::digital)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "PERFORMANCE & UTILITY", "Clean BBD Clock",
        configured(initValues,
                   set(Parameter::time, 260.0f),
                   set(Parameter::feedback, 44.0f),
                   set(Parameter::width, 150.0f),
                   set(Parameter::mix, 30.0f),
                   set(Parameter::duck, 15.0f),
                   set(Parameter::lowCut, 150.0f),
                   set(Parameter::highCut, 9000.0f),
                   set(Parameter::drive, 10.0f),
                   set(Parameter::age, 15.0f),
                   set(Parameter::modRate, 0.55f),
                   set(Parameter::modDepth, 0.4f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::bbd)),
                   set(Parameter::timeMotion, choice(TimeMotion::clean)))
    },
    {
        "PERFORMANCE & UTILITY", "Controlled Self-Osc",
        configured(initValues,
                   set(Parameter::feedback, 104.0f),
                   set(Parameter::width, 150.0f),
                   set(Parameter::mix, 25.0f),
                   set(Parameter::duck, 65.0f),
                   set(Parameter::output, -6.0f),
                   set(Parameter::lowCut, 180.0f),
                   set(Parameter::highCut, 5200.0f),
                   set(Parameter::drive, 25.0f),
                   set(Parameter::age, 55.0f),
                   set(Parameter::modDepth, 0.8f),
                   set(Parameter::smear, 12.0f),
                   set(Parameter::size, 32.0f),
                   set(Parameter::division, choice(Division::note1_2)),
                   set(Parameter::style, choice(Style::pingPong)),
                   set(Parameter::character, choice(Character::tape)),
                   set(Parameter::timeMotion, choice(TimeMotion::tape)))
    },
    {
        "PERFORMANCE & UTILITY", "Bass Resonant Notch",
        configured(pitchResonanceBase,
                   set(Parameter::time, 31.0f),
                   set(Parameter::offset, -60.0f),
                   set(Parameter::feedback, 84.0f),
                   set(Parameter::width, 95.0f),
                   set(Parameter::mix, 28.0f),
                   set(Parameter::output, -6.0f),
                   set(Parameter::lowCut, 30.0f),
                   set(Parameter::highCut, 3200.0f),
                   set(Parameter::drive, 18.0f),
                   set(Parameter::age, 16.0f),
                   set(Parameter::smear, 0.0f),
                   set(Parameter::size, 20.0f),
                   set(Parameter::timeMode, choice(TimeMode::tune)),
                   set(Parameter::style, choice(Style::single)),
                   set(Parameter::character, choice(Character::clean)))
    }
}};

inline constexpr FactoryPresetGroup hybridExtremePresets
{{
    {
        "HYBRID & EXTREME", "Tape Stop Throw",
        configured(initValues,
                   set(Parameter::time, 350.0f),
                   set(Parameter::feedback, 65.0f),
                   set(Parameter::width, 140.0f),
                   set(Parameter::mix, 32.0f),
                   set(Parameter::duck, 40.0f),
                   set(Parameter::output, -3.0f),
                   set(Parameter::lowCut, 180.0f),
                   set(Parameter::highCut, 5000.0f),
                   set(Parameter::drive, 30.0f),
                   set(Parameter::age, 80.0f),
                   set(Parameter::modRate, 0.05f),
                   set(Parameter::modDepth, 4.0f),
                   set(Parameter::smear, 12.0f),
                   set(Parameter::size, 40.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::single)),
                   set(Parameter::character, choice(Character::tape)),
                   set(Parameter::timeMotion, choice(TimeMotion::tape)),
                   set(Parameter::transformRoute, choice(TransformRoute::post)),
                   set(Parameter::colorMacro, 65.0f),
                   set(Parameter::spaceMacro, 70.0f))
    },
    {
        "HYBRID & EXTREME", "Negative Comb Mirror",
        configured(pitchResonanceBase,
                   set(Parameter::time, 12.0f),
                   set(Parameter::offset, -85.0f),
                   set(Parameter::feedback, 78.0f),
                   set(Parameter::width, 120.0f),
                   set(Parameter::mix, 30.0f),
                   set(Parameter::output, -5.0f),
                   set(Parameter::lowCut, 120.0f),
                   set(Parameter::highCut, 14000.0f),
                   set(Parameter::drive, 8.0f),
                   set(Parameter::age, 10.0f),
                   set(Parameter::smear, 0.0f),
                   set(Parameter::size, 10.0f),
                   set(Parameter::timeMode, choice(TimeMode::tune)),
                   set(Parameter::style, choice(Style::single)),
                   set(Parameter::character, choice(Character::clean)))
    },
    {
        "HYBRID & EXTREME", "Post Ascender Throw",
        configured(spiralShiftBase,
                   set(Parameter::feedback, 55.0f),
                   set(Parameter::width, 165.0f),
                   set(Parameter::mix, 34.0f),
                   set(Parameter::duck, 20.0f),
                   set(Parameter::shift, 240.0f),
                   set(Parameter::diverge, 35.0f),
                   set(Parameter::smear, 35.0f),
                   set(Parameter::size, 55.0f),
                   set(Parameter::division, choice(Division::note1_4)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::taps, choice(Taps::two)),
                   set(Parameter::transformRoute, choice(TransformRoute::post)),
                   set(Parameter::spaceMacro, 25.0f))
    },
    {
        "HYBRID & EXTREME", "Loop Descender Throw",
        configured(spiralShiftBase,
                   set(Parameter::feedback, 60.0f),
                   set(Parameter::width, 170.0f),
                   set(Parameter::mix, 35.0f),
                   set(Parameter::duck, 15.0f),
                   set(Parameter::shift, -240.0f),
                   set(Parameter::diverge, 35.0f),
                   set(Parameter::smear, 25.0f),
                   set(Parameter::size, 45.0f),
                   set(Parameter::division, choice(Division::note1_4)),
                   set(Parameter::style, choice(Style::pingPong)),
                   set(Parameter::taps, choice(Taps::two)),
                   set(Parameter::transformRoute, choice(TransformRoute::loop)))
    },
    {
        "HYBRID & EXTREME", "Reverse Bloom",
        configured(spaceDiffusionBase,
                   set(Parameter::feedback, 55.0f),
                   set(Parameter::width, 175.0f),
                   set(Parameter::mix, 38.0f),
                   set(Parameter::duck, 15.0f),
                   set(Parameter::lowCut, 180.0f),
                   set(Parameter::highCut, 10000.0f),
                   set(Parameter::drive, 12.0f),
                   set(Parameter::age, 20.0f),
                   set(Parameter::smear, 80.0f),
                   set(Parameter::size, 60.0f),
                   set(Parameter::grain, 25.0f),
                   set(Parameter::grainSize, 120.0f),
                   set(Parameter::density, 4.0f),
                   set(Parameter::spray, 70.0f),
                   set(Parameter::reverse, 70.0f),
                   set(Parameter::division, choice(Division::note1_8Dotted)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::taps, choice(Taps::four)),
                   set(Parameter::transformRoute, choice(TransformRoute::post)),
                   set(Parameter::spaceMacro, 55.0f))
    },
    {
        "HYBRID & EXTREME", "Shift+Grain Mirror",
        configured(spiralShiftBase,
                   set(Parameter::feedback, 58.0f),
                   set(Parameter::width, 190.0f),
                   set(Parameter::mix, 40.0f),
                   set(Parameter::duck, 10.0f),
                   set(Parameter::shift, 120.0f),
                   set(Parameter::diverge, 100.0f),
                   set(Parameter::smear, 35.0f),
                   set(Parameter::size, 50.0f),
                   set(Parameter::grain, 45.0f),
                   set(Parameter::grainSize, 80.0f),
                   set(Parameter::density, 6.0f),
                   set(Parameter::spray, 75.0f),
                   set(Parameter::pitch, 7.0f),
                   set(Parameter::reverse, 35.0f),
                   set(Parameter::division, choice(Division::note1_8)),
                   set(Parameter::style, choice(Style::pingPong)),
                   set(Parameter::taps, choice(Taps::four)),
                   set(Parameter::transformRoute, choice(TransformRoute::loop)))
    },
    {
        "HYBRID & EXTREME", "Tight Micrograin",
        configured(initValues,
                   set(Parameter::time, 45.0f),
                   set(Parameter::feedback, 42.0f),
                   set(Parameter::width, 160.0f),
                   set(Parameter::mix, 30.0f),
                   set(Parameter::duck, 20.0f),
                   set(Parameter::lowCut, 200.0f),
                   set(Parameter::highCut, 14000.0f),
                   set(Parameter::drive, 8.0f),
                   set(Parameter::age, 10.0f),
                   set(Parameter::smear, 15.0f),
                   set(Parameter::size, 18.0f),
                   set(Parameter::grain, 65.0f),
                   set(Parameter::grainSize, 20.0f),
                   set(Parameter::density, 8.0f),
                   set(Parameter::spray, 10.0f),
                   set(Parameter::reverse, 5.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::transformRoute, choice(TransformRoute::post)),
                   set(Parameter::spaceMacro, -75.0f))
    },
    {
        "HYBRID & EXTREME", "Chaotic Spray",
        configured(experimentalBase,
                   set(Parameter::time, 180.0f),
                   set(Parameter::feedback, 52.0f),
                   set(Parameter::width, 180.0f),
                   set(Parameter::mix, 42.0f),
                   set(Parameter::duck, 10.0f),
                   set(Parameter::lowCut, 250.0f),
                   set(Parameter::highCut, 11000.0f),
                   set(Parameter::drive, 20.0f),
                   set(Parameter::age, 30.0f),
                   set(Parameter::smear, 35.0f),
                   set(Parameter::size, 50.0f),
                   set(Parameter::grain, 70.0f),
                   set(Parameter::grainSize, 110.0f),
                   set(Parameter::density, 3.0f),
                   set(Parameter::spray, 95.0f),
                   set(Parameter::pitch, -5.0f),
                   set(Parameter::reverse, 55.0f),
                   set(Parameter::timeMode, choice(TimeMode::free)),
                   set(Parameter::style, choice(Style::dual)),
                   set(Parameter::character, choice(Character::digital)),
                   set(Parameter::transformRoute, choice(TransformRoute::loop)),
                   set(Parameter::colorMacro, 35.0f))
    },
    {
        "HYBRID & EXTREME", "Octave Cloud",
        configured(grainFreezeBase,
                   set(Parameter::feedback, 68.0f),
                   set(Parameter::width, 185.0f),
                   set(Parameter::mix, 40.0f),
                   set(Parameter::duck, 18.0f),
                   set(Parameter::lowCut, 160.0f),
                   set(Parameter::highCut, 15000.0f),
                   set(Parameter::drive, 10.0f),
                   set(Parameter::age, 15.0f),
                   set(Parameter::smear, 45.0f),
                   set(Parameter::size, 65.0f),
                   set(Parameter::grain, 80.0f),
                   set(Parameter::grainSize, 100.0f),
                   set(Parameter::density, 6.0f),
                   set(Parameter::spray, 45.0f),
                   set(Parameter::pitch, 12.0f),
                   set(Parameter::reverse, 20.0f),
                   set(Parameter::division, choice(Division::note1_4)),
                   set(Parameter::style, choice(Style::pingPong)),
                   set(Parameter::taps, choice(Taps::two)),
                   set(Parameter::character, choice(Character::clean)),
                   set(Parameter::transformRoute, choice(TransformRoute::post)),
                   set(Parameter::colorMacro, -25.0f))
    },
    {
        "HYBRID & EXTREME", "Wash Freeze Bed",
        configured(grainFreezeBase,
                   set(Parameter::feedback, 90.0f),
                   set(Parameter::width, 190.0f),
                   set(Parameter::mix, 45.0f),
                   set(Parameter::duck, 10.0f),
                   set(Parameter::output, -4.0f),
                   set(Parameter::lowCut, 180.0f),
                   set(Parameter::highCut, 10000.0f),
                   set(Parameter::drive, 20.0f),
                   set(Parameter::age, 25.0f),
                   set(Parameter::smear, 75.0f),
                   set(Parameter::size, 100.0f),
                   set(Parameter::grain, 90.0f),
                   set(Parameter::grainSize, 200.0f),
                   set(Parameter::density, 8.0f),
                   set(Parameter::spray, 45.0f),
                   set(Parameter::pitch, 12.0f),
                   set(Parameter::reverse, 35.0f),
                   set(Parameter::division, choice(Division::note1_2)),
                   set(Parameter::style, choice(Style::pingPong)),
                   set(Parameter::taps, choice(Taps::four)),
                   set(Parameter::character, choice(Character::tape)),
                   set(Parameter::transformRoute, choice(TransformRoute::loop)),
                   set(Parameter::spaceMacro, 90.0f))
    }
}};

template <std::size_t GroupCount>
constexpr std::array<TBDelayPreset,
                     GroupCount * factoryPresetsPerGroup>
flattenFactoryPresetGroups(
    const std::array<FactoryPresetGroup, GroupCount>& groups) noexcept
{
    std::array<TBDelayPreset,
               GroupCount * factoryPresetsPerGroup> presets {};
    std::size_t destination = 0;

    for (const auto& group : groups)
        for (const auto& preset : group)
            presets[destination++] = preset;

    return presets;
}

inline constexpr std::array<FactoryPresetGroup, 12> factoryPresetGroups
{{
    essentialEchoPresets,
    slapDoublePresets,
    rhythmMultitapPresets,
    pingPongStereoPresets,
    tapeAnalogPresets,
    digitalLofiPresets,
    combResonancePresets,
    spiralShiftPresets,
    spaceDiffusionPresets,
    grainFreezePresets,
    performanceUtilityPresets,
    hybridExtremePresets
}};

inline constexpr auto factoryPresets =
    flattenFactoryPresetGroups(factoryPresetGroups);

struct ParameterRange
{
    float minimum;
    float maximum;
    bool integral;
};

inline constexpr std::array<ParameterRange, TBDelayParameterIDs::count> parameterRanges
{{
    { 10.0f, 8000.0f, false }, // time
    { -100.0f, 100.0f, false },
    { 0.0f, 110.0f, false },
    { 25.0f, 200.0f, false },
    { -50.0f, 50.0f, false },
    { 0.0f, 200.0f, false },
    { 0.0f, 100.0f, false },
    { 0.0f, 100.0f, false },
    { -18.0f, 6.0f, false },
    { 20.0f, 2000.0f, false },
    { 500.0f, 20000.0f, false },
    { 0.0f, 100.0f, false },
    { 0.0f, 100.0f, false },
    { 0.02f, 10.0f, false },
    { 0.0f, 20.0f, false },
    { 0.0f, 100.0f, false },
    { 2.0f, 120.0f, false },
    { -2000.0f, 2000.0f, false },
    { 0.0f, 100.0f, false },
    { 0.0f, 100.0f, false },
    { 10.0f, 500.0f, false },
    { 1.0f, 8.0f, true },
    { 0.0f, 100.0f, false },
    { -24.0f, 24.0f, false },
    { 0.0f, 100.0f, false },
    { 0.0f, 2.0f, true },   // time mode
    { 0.0f, 17.0f, true },  // division
    { 0.0f, 3.0f, true },   // style
    { 0.0f, 3.0f, true },   // taps
    { 0.0f, 3.0f, true },   // character
    { 0.0f, 1.0f, true },   // time motion
    { 0.0f, 1.0f, true },   // transform route
    { 0.0f, 1.0f, true },   // freeze
    { 0.0f, 1.0f, true },   // stereo bypass
    { 0.0f, 1.0f, true },   // color/space bypass
    { 0.0f, 1.0f, true },   // grain/transform bypass
    { -100.0f, 100.0f, false }, // color macro
    { -100.0f, 100.0f, false }  // space macro
}};

constexpr bool isIntegral(float value) noexcept
{
    return value == static_cast<float>(static_cast<int>(value));
}

constexpr bool presetIsValid(const TBDelayPreset& preset) noexcept
{
    if (preset.group == nullptr || preset.name == nullptr
        || preset.group[0] == '\0' || preset.name[0] == '\0')
        return false;

    for (std::size_t i = 0; i < preset.values.size(); ++i)
    {
        const auto value = preset.values[i];
        const auto range = parameterRanges[i];

        if (!(value >= range.minimum && value <= range.maximum))
            return false;

        if (range.integral && !isIntegral(value))
            return false;
    }

    return true;
}

constexpr bool sameSettings(const TBDelayPreset& first,
                            const TBDelayPreset& second) noexcept
{
    for (std::size_t i = 0; i < first.values.size(); ++i)
        if (first.values[i] != second.values[i])
            return false;

    return true;
}

constexpr bool cStringsEqual(const char* first, const char* second) noexcept
{
    if (first == nullptr || second == nullptr)
        return first == second;

    for (std::size_t i = 0;; ++i)
    {
        if (first[i] != second[i])
            return false;

        if (first[i] == '\0')
            return true;
    }
}

constexpr bool factoryBankIsValid() noexcept
{
    for (std::size_t i = 0; i < factoryPresets.size(); ++i)
    {
        if (!presetIsValid(factoryPresets[i]))
            return false;

        for (std::size_t j = i + 1; j < factoryPresets.size(); ++j)
            if (cStringsEqual(factoryPresets[i].name, factoryPresets[j].name)
                || sameSettings(factoryPresets[i], factoryPresets[j]))
                return false;
    }

    return true;
}

constexpr bool factoryGroupsAreValid() noexcept
{
    constexpr std::array<const char*, 12> expectedGroups
    {
        "ESSENTIAL ECHO",
        "SLAP & DOUBLE",
        "RHYTHM & MULTITAP",
        "PING PONG & STEREO",
        "TAPE & ANALOG",
        "DIGITAL & LO-FI",
        "COMB & RESONANCE",
        "SPIRAL & SHIFT",
        "SPACE & DIFFUSION",
        "GRAIN & FREEZE",
        "PERFORMANCE & UTILITY",
        "HYBRID & EXTREME"
    };

    if (factoryPresets.size()
        != expectedGroups.size() * factoryPresetsPerGroup)
        return false;

    std::size_t presetIndex = 0;
    for (const auto* expectedGroup : expectedGroups)
    {
        for (std::size_t item = 0;
             item < factoryPresetsPerGroup;
             ++item, ++presetIndex)
        {
            if (!cStringsEqual(factoryPresets[presetIndex].group,
                               expectedGroup))
                return false;
        }
    }

    return true;
}

constexpr bool parameterIdsAreUnique() noexcept
{
    for (std::size_t i = 0; i < TBDelayParameterIDs::all.size(); ++i)
        for (std::size_t j = i + 1; j < TBDelayParameterIDs::all.size(); ++j)
            if (cStringsEqual(TBDelayParameterIDs::all[i],
                              TBDelayParameterIDs::all[j]))
                return false;

    return true;
}

constexpr bool initMatchesBrief() noexcept
{
    constexpr std::array<float, TBDelayParameterIDs::count> briefDefaults
    {
        375.0f, 0.0f, 48.0f, 100.0f, 0.0f,
        130.0f, 30.0f, 25.0f, 0.0f,
        120.0f, 9000.0f, 18.0f, 28.0f,
        0.25f, 0.8f, 12.0f, 32.0f,
        0.0f, 0.0f,
        0.0f, 90.0f, 4.0f, 15.0f, 0.0f, 0.0f,
        1.0f, 8.0f, 2.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f
    };

    for (std::size_t i = 0; i < initValues.size(); ++i)
        if (initValues[i] != briefDefaults[i])
            return false;

    return true;
}

static_assert(static_cast<std::size_t>(Parameter::count)
                   == TBDelayParameterIDs::count,
              "Preset parameter order must contain exactly 38 entries.");
static_assert(initMatchesBrief(),
              "The Init preset must exactly match PRODUCT_BRIEF.md.");
static_assert(factoryBankIsValid(),
              "Factory presets must be in range, integral where required, uniquely named, and sonically unique.");
static_assert(factoryPresets.size() == 120,
              "The product brief requires exactly 120 factory presets.");
static_assert(factoryGroupsAreValid(),
              "Factory presets must be ordered as twelve logical groups of ten.");
static_assert(parameterIdsAreUnique(),
              "Every APVTS parameter ID must be unique.");
} // namespace TBDelayPresetDetail

inline const auto& getTBDelayFactoryPresets() noexcept
{
    return TBDelayPresetDetail::factoryPresets;
}
