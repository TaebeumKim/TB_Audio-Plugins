#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace
{
constexpr int legacyStateSchemaVersion = 1;
constexpr int legacyPresetFormatVersion = 1;
constexpr size_t legacyParameterCount = 11;

size_t parameterCountForVersion (int schemaVersion, int formatVersion) noexcept
{
    if (schemaVersion != formatVersion)
        return 0;

    switch (schemaVersion)
    {
        case 4:  return 17;
        case 3:  return 15;
        case 2:  return 14;
        case 1:  return legacyParameterCount;
        default: return 0;
    }
}

constexpr std::array<TBShimmerAudioProcessor::FactoryPreset, 100> factoryPresets
{{
    { "init", "START", "Init",
      240.0f, 160.0f, 9.0f, 32.0f, 12.0f, 18.0f, 42.0f, 9000.0f, 135.0f, 48.0f, 0.0f,
      0.0f, 32.0f, 32.0f * 0.0035f },
    { "soft_halo", "START", "Soft Halo",
      180.0f, 220.0f, 12.0f, 15.0f, 7.0f, 5.0f, 28.0f, 12000.0f, 115.0f, 35.0f, 0.0f,
      0.0f, 15.0f, 15.0f * 0.0035f },
    { "open_air", "START", "Open Air",
      120.0f, 80.0f, 16.0f, 22.0f, 12.0f, 0.0f, 18.0f, 16000.0f, 150.0f, 30.0f, 0.0f,
      0.0f, 22.0f, 22.0f * 0.0035f },

    { "glass_halo", "SHIMMER", "Glass Halo",
      320.0f, 200.0f, 13.0f, 24.0f, 12.0f, 10.0f, 50.0f, 12000.0f, 145.0f, 50.0f, 0.0f,
      0.0f, 24.0f, 24.0f * 0.0035f },
    { "guitar_ascension", "SHIMMER", "Guitar Ascension",
      260.0f, 140.0f, 10.0f, 18.0f, 12.0f, 8.0f, 38.0f, 10500.0f, 125.0f, 40.0f, 0.0f,
      0.0f, 18.0f, 18.0f * 0.0035f },
    { "vocal_starlight", "SHIMMER", "Vocal Starlight",
      180.0f, 120.0f, 16.0f, 14.0f, 7.0f, 6.0f, 26.0f, 12000.0f, 120.0f, 34.0f, 0.0f,
      0.0f, 14.0f, 14.0f * 0.0035f },

    { "pad_aurora", "CLOUD", "Pad Aurora",
      480.0f, 360.0f, 20.0f, 45.0f, 12.0f, 22.0f, 52.0f, 10000.0f, 160.0f, 54.0f, 0.0f,
      0.0f, 45.0f, 45.0f * 0.0035f },
    { "slow_nebula", "CLOUD", "Slow Nebula",
      800.0f, 500.0f, 15.0f, 60.0f, 7.0f, 30.0f, 58.0f, 6000.0f, 175.0f, 60.0f, 0.0f,
      0.0f, 60.0f, 60.0f * 0.0035f },
    { "wide_harmonic_fog", "CLOUD", "Wide Harmonic Fog",
      600.0f, 280.0f, 22.0f, 62.0f, 19.0f, 18.0f, 44.0f, 11000.0f, 185.0f, 54.0f, 0.0f,
      0.0f, 62.0f, 62.0f * 0.0035f },

    { "reverse_comet", "REVERSE", "Reverse Comet",
      420.0f, 220.0f, 9.0f, 38.0f, 12.0f, 100.0f, 40.0f, 10500.0f, 155.0f, 54.0f, 0.0f,
      0.0f, 38.0f, 38.0f * 0.0035f },
    { "falling_glass", "REVERSE", "Falling Glass",
      360.0f, 180.0f, 12.0f, 52.0f, -7.0f, 80.0f, 35.0f, 7500.0f, 150.0f, 50.0f, 0.0f,
      0.0f, 52.0f, 52.0f * 0.0035f },
    { "dark_orbit", "REVERSE", "Dark Orbit",
      900.0f, 420.0f, 10.0f, 58.0f, -12.0f, 60.0f, 54.0f, 3200.0f, 175.0f, 58.0f, 0.0f,
      0.0f, 58.0f, 58.0f * 0.0035f },

    { "subtle_lift", "UTILITY", "Subtle Lift", 90.f, 70.f, 18.f, 8.f, 12.f, 0.f, 10.f, 15000.f, 105.f, 15.f, 0.f, 1.f, 4.f, 0.1f },
    { "mono_bloom", "UTILITY", "Mono Bloom", 160.f, 180.f, 14.f, 18.f, 7.f, 4.f, 24.f, 10500.f, 0.f, 28.f, 0.f, 1.5f, 10.f, 0.2f },
    { "centered_glow", "UTILITY", "Centered Glow", 140.f, 120.f, 16.f, 12.f, 12.f, 0.f, 20.f, 13500.f, 75.f, 24.f, 0.f, 0.8f, 6.f, 0.1f },
    { "bright_ambience", "UTILITY", "Bright Ambience", 80.f, 90.f, 20.f, 20.f, 7.f, 3.f, 16.f, 18000.f, 125.f, 22.f, 0.f, 1.f, 12.f, 0.3f },
    { "warm_ambience", "UTILITY", "Warm Ambience", 220.f, 260.f, 11.f, 18.f, 0.f, 5.f, 30.f, 5200.f, 110.f, 28.f, 0.f, 1.2f, 10.f, 0.15f },
    { "transient_guard", "UTILITY", "Transient Guard", 60.f, 40.f, 24.f, 5.f, 12.f, 0.f, 8.f, 12500.f, 115.f, 12.f, 0.f, 0.5f, 0.f, 0.f },
    { "wet_shimmer_bus", "UTILITY", "Wet Shimmer Bus", 260.f, 180.f, 14.f, 22.f, 12.f, 8.f, 38.f, 13000.f, 145.f, 100.f, 0.f, 1.f, 14.f, 0.3f },
    { "wet_diffuse_bus", "UTILITY", "Wet Diffuse Bus", 360.f, 420.f, 22.f, 28.f, 0.f, 5.f, 34.f, 8000.f, 155.f, 100.f, 0.f, 1.5f, 18.f, 0.25f },

    { "opal_chamber", "DIFFUSE", "Opal Chamber", 240.f, 320.f, 20.f, 16.f, 0.f, 5.f, 30.f, 11000.f, 135.f, 38.f, 0.f, 1.f, 10.f, 0.2f },
    { "silken_room", "DIFFUSE", "Silken Room", 180.f, 400.f, 24.f, 10.f, 0.f, 0.f, 22.f, 9000.f, 120.f, 34.f, 0.f, 0.5f, 6.f, 0.1f },
    { "wide_diffusion", "DIFFUSE", "Wide Diffusion", 320.f, 500.f, 23.f, 34.f, 0.f, 8.f, 36.f, 12000.f, 190.f, 46.f, 0.f, 2.f, 20.f, 0.4f },
    { "warm_current", "DIFFUSE", "Warm Current", 420.f, 360.f, 18.f, 24.f, 0.f, 12.f, 40.f, 4800.f, 145.f, 44.f, 0.f, 1.5f, 14.f, 0.2f },
    { "dense_air", "DIFFUSE", "Dense Air", 100.f, 460.f, 24.f, 12.f, 7.f, 2.f, 26.f, 15000.f, 130.f, 32.f, 0.f, 1.f, 8.f, 0.15f },
    { "slow_chamber", "DIFFUSE", "Slow Chamber", 700.f, 500.f, 16.f, 30.f, 0.f, 18.f, 58.f, 7000.f, 160.f, 52.f, 0.f, 2.f, 22.f, 0.3f },
    { "short_bloom", "DIFFUSE", "Short Bloom", 90.f, 280.f, 22.f, 20.f, 12.f, 4.f, 18.f, 13500.f, 140.f, 30.f, 0.f, 1.f, 10.f, 0.2f },
    { "deep_diffusion", "DIFFUSE", "Deep Diffusion", 1000.f, 480.f, 14.f, 38.f, -5.f, 25.f, 62.f, 3600.f, 170.f, 56.f, 0.f, 2.5f, 28.f, 0.5f },

    { "silver_dust", "SPARKLE", "Silver Dust", 120.f, 70.f, 22.f, 35.f, 12.f, 0.f, 18.f, 18000.f, 160.f, 28.f, 0.f, 3.f, 18.f, 1.2f, 0.f, 45.f, 0.f },
    { "prism_ticks", "SPARKLE", "Prism Ticks", 80.f, 45.f, 24.f, 42.f, 19.f, 8.f, 14.f, 17500.f, 175.f, 24.f, 0.f, 4.f, 22.f, 2.f, 0.f, 60.f, 85.f },
    { "high_constellation", "SPARKLE", "High Constellation", 240.f, 90.f, 18.f, 55.f, 24.f, 12.f, 30.f, 16000.f, 185.f, 40.f, 0.f, 3.f, 28.f, 1.5f, 0.f, 60.f, 15.f },
    { "fifth_glint", "SPARKLE", "Fifth Glint", 160.f, 110.f, 20.f, 30.f, 7.f, 3.f, 24.f, 17000.f, 150.f, 32.f, 0.f, 2.f, 16.f, 0.7f, 0.f, 35.f, 30.f },
    { "octave_sparks", "SPARKLE", "Octave Sparks", 300.f, 65.f, 16.f, 60.f, 12.f, 15.f, 46.f, 15500.f, 190.f, 45.f, 0.f, 5.f, 35.f, 2.5f, 0.f, 65.f, 70.f },
    { "bright_rain", "SPARKLE", "Bright Rain", 500.f, 80.f, 24.f, 68.f, 19.f, 25.f, 38.f, 18000.f, 175.f, 48.f, 0.f, 6.f, 45.f, 3.f, 0.f, 70.f, 45.f },
    { "fine_chimes", "SPARKLE", "Fine Chimes", 700.f, 120.f, 12.f, 48.f, 24.f, 35.f, 52.f, 15000.f, 165.f, 52.f, 0.f, 4.f, 32.f, 1.f, 0.f, 50.f, 60.f },
    { "star_shower", "SPARKLE", "Star Shower", 1100.f, 160.f, 20.f, 78.f, 12.f, 20.f, 58.f, 17000.f, 200.f, 60.f, 0.f, 7.f, 55.f, 4.f, 0.f, 80.f, 100.f },

    { "gentle_variation", "RANDOM", "Gentle Variation", 220.f, 180.f, 15.f, 24.f, 7.f, 10.f, 30.f, 11000.f, 135.f, 38.f, 0.f, 3.f, 18.f, 0.8f },
    { "loose_grains", "RANDOM", "Loose Grains", 360.f, 130.f, 10.f, 46.f, 12.f, 16.f, 36.f, 12000.f, 155.f, 44.f, 0.f, 6.f, 45.f, 2.f },
    { "wandering_pitch", "RANDOM", "Wandering Pitch", 280.f, 160.f, 14.f, 30.f, 0.f, 12.f, 34.f, 10500.f, 150.f, 42.f, 0.f, 2.f, 30.f, 6.f },
    { "soft_roulette", "RANDOM", "Soft Roulette", 480.f, 240.f, 12.f, 38.f, 7.f, 45.f, 42.f, 8500.f, 165.f, 48.f, 0.f, 8.f, 60.f, 3.f },
    { "broken_halo", "RANDOM", "Broken Halo", 650.f, 100.f, 8.f, 72.f, 12.f, 25.f, 48.f, 9500.f, 185.f, 52.f, 0.f, 10.f, 75.f, 5.f },
    { "random_current", "RANDOM", "Random Current", 900.f, 320.f, 18.f, 55.f, 0.f, 50.f, 56.f, 7200.f, 170.f, 56.f, 0.f, 7.f, 80.f, 8.f },
    { "wide_uncertainty", "RANDOM", "Wide Uncertainty", 1200.f, 260.f, 20.f, 88.f, 5.f, 35.f, 60.f, 13000.f, 200.f, 62.f, 0.f, 12.f, 100.f, 10.f },
    { "total_scatter", "RANDOM", "Total Scatter", 750.f, 40.f, 24.f, 100.f, -3.f, 100.f, 44.f, 14500.f, 200.f, 58.f, 0.f, 12.f, 100.f, 12.f },

    { "tight_glimmer", "PULSE", "Tight Glimmer", 125.f, 55.f, 16.f, 12.f, 12.f, 0.f, 24.f, 14000.f, 130.f, 28.f, 0.f, 1.f, 3.f, 0.f },
    { "broad_cloud", "PULSE", "Broad Cloud", 250.f, 90.f, 12.f, 18.f, 7.f, 5.f, 32.f, 11000.f, 140.f, 34.f, 0.f, 2.f, 6.f, 0.2f },
    { "staggered_drift", "PULSE", "Staggered Drift", 375.f, 120.f, 9.f, 26.f, 12.f, 15.f, 40.f, 12000.f, 150.f, 40.f, 0.f, 2.5f, 12.f, 0.4f },
    { "slow_bloom", "PULSE", "Slow Bloom", 500.f, 180.f, 8.f, 32.f, 12.f, 30.f, 46.f, 9000.f, 160.f, 44.f, 0.f, 3.f, 18.f, 0.6f },
    { "quick_spark", "PULSE", "Quick Spark", 167.f, 45.f, 18.f, 40.f, 19.f, 10.f, 28.f, 16500.f, 175.f, 30.f, 0.f, 4.f, 20.f, 1.2f },
    { "reverse_cadence", "PULSE", "Reverse Cadence", 333.f, 80.f, 11.f, 48.f, 7.f, 75.f, 38.f, 10000.f, 165.f, 42.f, 0.f, 5.f, 35.f, 1.5f },
    { "slow_pulse", "PULSE", "Slow Pulse", 750.f, 240.f, 6.f, 20.f, 0.f, 8.f, 52.f, 6000.f, 145.f, 48.f, 0.f, 2.f, 10.f, 0.2f },
    { "long_cycle", "PULSE", "Long Cycle", 1000.f, 300.f, 4.f, 35.f, 12.f, 22.f, 64.f, 8000.f, 175.f, 55.f, 0.f, 3.f, 25.f, 0.8f },

    { "vocal_sheen", "VOCAL", "Vocal Sheen", 110.f, 85.f, 18.f, 12.f, 7.f, 0.f, 18.f, 14500.f, 110.f, 20.f, 0.f, 1.f, 5.f, 0.1f },
    { "vocal_octave", "VOCAL", "Vocal Octave", 180.f, 110.f, 14.f, 16.f, 12.f, 3.f, 26.f, 12000.f, 120.f, 28.f, 0.f, 1.5f, 8.f, 0.2f },
    { "vocal_air_trail", "VOCAL", "Vocal Air Trail", 260.f, 150.f, 20.f, 22.f, 7.f, 10.f, 32.f, 16000.f, 145.f, 34.f, 0.f, 2.f, 14.f, 0.5f },
    { "vocal_velvet", "VOCAL", "Vocal Velvet", 320.f, 240.f, 16.f, 18.f, 0.f, 8.f, 36.f, 6200.f, 115.f, 32.f, 0.f, 1.f, 12.f, 0.2f },
    { "vocal_reverse_bloom", "VOCAL", "Vocal Reverse Bloom", 420.f, 200.f, 12.f, 30.f, 7.f, 70.f, 40.f, 10000.f, 150.f, 42.f, 0.f, 3.f, 25.f, 0.8f },
    { "vocal_choir_cloud", "VOCAL", "Vocal Choir Cloud", 600.f, 340.f, 22.f, 42.f, 12.f, 18.f, 50.f, 11000.f, 170.f, 50.f, 0.f, 4.f, 35.f, 1.5f },
    { "vocal_shadow", "VOCAL", "Vocal Shadow", 800.f, 280.f, 10.f, 38.f, -5.f, 35.f, 54.f, 4000.f, 150.f, 48.f, 0.f, 3.f, 40.f, 1.f },
    { "vocal_stars", "VOCAL", "Vocal Stars", 1000.f, 180.f, 18.f, 58.f, 19.f, 25.f, 46.f, 15000.f, 180.f, 55.f, 0.f, 5.f, 50.f, 2.5f },

    { "guitar_glow", "GUITAR", "Guitar Glow", 120.f, 70.f, 18.f, 10.f, 12.f, 0.f, 20.f, 12500.f, 120.f, 22.f, 0.f, 1.f, 5.f, 0.f },
    { "guitar_fifth", "GUITAR", "Guitar Fifth", 220.f, 120.f, 12.f, 18.f, 7.f, 4.f, 30.f, 11000.f, 130.f, 30.f, 0.f, 1.5f, 10.f, 0.2f },
    { "guitar_octave_trail", "GUITAR", "Guitar Octave Trail", 350.f, 150.f, 10.f, 22.f, 12.f, 8.f, 42.f, 10500.f, 145.f, 38.f, 0.f, 2.f, 12.f, 0.3f },
    { "guitar_reverse_swell", "GUITAR", "Guitar Reverse Swell", 480.f, 220.f, 9.f, 35.f, 12.f, 85.f, 44.f, 9000.f, 160.f, 46.f, 0.f, 3.f, 28.f, 0.8f },
    { "guitar_warm_cloud", "GUITAR", "Guitar Warm Cloud", 600.f, 300.f, 16.f, 30.f, 0.f, 12.f, 48.f, 5200.f, 150.f, 44.f, 0.f, 2.f, 20.f, 0.4f },
    { "guitar_high_mist", "GUITAR", "Guitar High Mist", 750.f, 180.f, 20.f, 45.f, 19.f, 15.f, 52.f, 13500.f, 175.f, 50.f, 0.f, 4.f, 35.f, 1.5f },
    { "guitar_low_orbit", "GUITAR", "Guitar Low Orbit", 900.f, 400.f, 8.f, 52.f, -12.f, 30.f, 60.f, 3200.f, 165.f, 55.f, 0.f, 3.f, 45.f, 1.f },
    { "guitar_grain_storm", "GUITAR", "Guitar Grain Storm", 1100.f, 80.f, 24.f, 82.f, 7.f, 40.f, 56.f, 12000.f, 190.f, 58.f, 0.f, 8.f, 70.f, 5.f },

    { "piano_air", "KEYS", "Piano Air", 100.f, 60.f, 20.f, 8.f, 12.f, 0.f, 14.f, 13500.f, 115.f, 18.f, 0.f, 0.5f, 3.f, 0.f },
    { "piano_halo", "KEYS", "Piano Halo", 220.f, 140.f, 14.f, 16.f, 7.f, 5.f, 28.f, 10000.f, 125.f, 28.f, 0.f, 1.f, 8.f, 0.2f },
    { "electric_silk", "KEYS", "Electric Silk", 300.f, 220.f, 18.f, 22.f, 0.f, 4.f, 34.f, 7200.f, 135.f, 34.f, 0.f, 1.5f, 12.f, 0.3f },
    { "organ_ascension", "KEYS", "Organ Ascension", 420.f, 320.f, 22.f, 28.f, 12.f, 8.f, 50.f, 9000.f, 155.f, 46.f, 0.f, 2.f, 18.f, 0.4f },
    { "synth_prism", "KEYS", "Synth Prism", 500.f, 110.f, 16.f, 48.f, 19.f, 15.f, 42.f, 15000.f, 180.f, 45.f, 0.f, 4.f, 38.f, 2.f },
    { "bell_mist", "KEYS", "Bell Mist", 650.f, 90.f, 12.f, 55.f, 24.f, 22.f, 48.f, 14500.f, 175.f, 50.f, 0.f, 5.f, 45.f, 1.5f },
    { "keys_reverse_air", "KEYS", "Keys Reverse Air", 800.f, 180.f, 10.f, 36.f, 7.f, 80.f, 46.f, 9500.f, 160.f, 48.f, 0.f, 3.f, 32.f, 1.f },
    { "low_key_cloud", "KEYS", "Low Key Cloud", 1000.f, 420.f, 14.f, 44.f, -12.f, 25.f, 62.f, 3500.f, 170.f, 56.f, 0.f, 4.f, 50.f, 2.f },

    { "pad_soft_field", "PADS", "Pad Soft Field", 300.f, 300.f, 20.f, 18.f, 0.f, 5.f, 38.f, 8000.f, 145.f, 42.f, 0.f, 1.f, 10.f, 0.2f },
    { "pad_octave_wash", "PADS", "Pad Octave Wash", 450.f, 360.f, 22.f, 26.f, 12.f, 10.f, 52.f, 10500.f, 165.f, 52.f, 0.f, 2.f, 18.f, 0.4f },
    { "pad_fifth_veil", "PADS", "Pad Fifth Veil", 600.f, 420.f, 18.f, 34.f, 7.f, 18.f, 58.f, 9000.f, 175.f, 56.f, 0.f, 3.f, 26.f, 0.7f },
    { "pad_reverse_tide", "PADS", "Pad Reverse Tide", 750.f, 500.f, 16.f, 42.f, 0.f, 75.f, 60.f, 7000.f, 180.f, 60.f, 0.f, 4.f, 40.f, 1.f },
    { "pad_high_horizon", "PADS", "Pad High Horizon", 900.f, 440.f, 24.f, 48.f, 19.f, 22.f, 64.f, 13000.f, 190.f, 62.f, 0.f, 4.f, 35.f, 1.5f },
    { "pad_low_horizon", "PADS", "Pad Low Horizon", 1050.f, 480.f, 12.f, 50.f, -12.f, 30.f, 66.f, 2800.f, 175.f, 64.f, 0.f, 3.f, 45.f, 1.f },
    { "pad_random_choir", "PADS", "Pad Random Choir", 1200.f, 380.f, 20.f, 65.f, 7.f, 35.f, 62.f, 8500.f, 190.f, 66.f, 0.f, 8.f, 75.f, 5.f },
    { "pad_endless_light", "PADS", "Pad Endless Light", 1100.f, 500.f, 24.f, 38.f, 12.f, 15.f, 78.f, 11500.f, 200.f, 70.f, 0.f, 2.f, 28.f, 0.6f },

    { "smoked_glass", "DARK", "Smoked Glass", 240.f, 180.f, 12.f, 24.f, -5.f, 15.f, 34.f, 3600.f, 130.f, 38.f, 0.f, 2.f, 18.f, 0.4f },
    { "low_tide", "DARK", "Low Tide", 400.f, 300.f, 10.f, 28.f, -12.f, 20.f, 42.f, 2200.f, 145.f, 45.f, 0.f, 2.f, 25.f, 0.5f },
    { "dim_chamber", "DARK", "Dim Chamber", 600.f, 440.f, 18.f, 20.f, 0.f, 12.f, 50.f, 3000.f, 150.f, 50.f, 0.f, 1.f, 18.f, 0.2f },
    { "shadow_reverse", "DARK", "Shadow Reverse", 750.f, 260.f, 8.f, 45.f, -7.f, 90.f, 48.f, 2600.f, 160.f, 52.f, 0.f, 4.f, 45.f, 1.f },
    { "black_velvet", "DARK", "Black Velvet", 900.f, 500.f, 14.f, 34.f, -12.f, 40.f, 62.f, 1800.f, 170.f, 60.f, 0.f, 3.f, 40.f, 0.8f },
    { "dusk_spray", "DARK", "Dusk Spray", 1000.f, 180.f, 20.f, 70.f, -3.f, 35.f, 54.f, 4200.f, 185.f, 58.f, 0.f, 8.f, 70.f, 6.f },
    { "deep_random", "DARK", "Deep Random", 1200.f, 360.f, 9.f, 60.f, -12.f, 55.f, 68.f, 1200.f, 180.f, 65.f, 0.f, 10.f, 90.f, 8.f },
    { "night_bloom", "DARK", "Night Bloom", 1100.f, 500.f, 22.f, 55.f, 0.f, 65.f, 74.f, 2400.f, 195.f, 68.f, 0.f, 6.f, 65.f, 3.f },

    { "frozen_fragments", "EXPERIMENTAL", "Frozen Fragments", 1200.f, 500.f, 2.f, 0.f, 24.f, 0.f, 0.f, 18000.f, 100.f, 100.f, 0.f, 0.f, 0.f, 0.f },
    { "micro_swarm", "EXPERIMENTAL", "Micro Swarm", 60.f, 40.f, 24.f, 100.f, 12.f, 50.f, 20.f, 18000.f, 200.f, 50.f, 0.f, 12.f, 100.f, 12.f },
    { "descending_mirage", "EXPERIMENTAL", "Descending Mirage", 700.f, 200.f, 11.f, 50.f, -12.f, 20.f, 55.f, 9000.f, 175.f, 58.f, 0.f, 5.f, 50.f, 4.f },
    { "split_direction", "EXPERIMENTAL", "Split Direction", 500.f, 160.f, 16.f, 58.f, 7.f, 50.f, 46.f, 11000.f, 200.f, 55.f, 0.f, 4.f, 45.f, 2.f },
    { "no_pitch_cloud", "EXPERIMENTAL", "No Pitch Cloud", 300.f, 400.f, 24.f, 30.f, 0.f, 0.f, 40.f, 8000.f, 150.f, 60.f, 0.f, 2.f, 20.f, 0.f },
    { "reverse_only_field", "EXPERIMENTAL", "Reverse Only Field", 900.f, 260.f, 12.f, 40.f, 0.f, 100.f, 50.f, 6500.f, 175.f, 65.f, 0.f, 3.f, 35.f, 1.f },
    { "shifting_constellation", "EXPERIMENTAL", "Shifting Constellation", 1000.f, 90.f, 20.f, 85.f, 19.f, 45.f, 65.f, 14000.f, 200.f, 70.f, 0.f, 12.f, 100.f, 10.f },
    { "outer_limit", "EXPERIMENTAL", "Outer Limit", 1200.f, 500.f, 2.f, 100.f, 24.f, 100.f, 88.f, 18000.f, 200.f, 100.f, 0.f, 12.f, 100.f, 12.f }
}};

float numericValueFromText (const juce::String& source)
{
    return static_cast<float> (source.trim()
                                   .retainCharacters ("+-0123456789.,")
                                   .replaceCharacter (',', '.')
                                   .getDoubleValue());
}

float frequencyFromText (const juce::String& source)
{
    const auto lower = source.trim().toLowerCase();
    const auto multiplier = lower.contains ("khz") || lower.endsWithChar ('k')
                          ? 1000.0f : 1.0f;
    return numericValueFromText (source) * multiplier;
}

juce::String pitchToHostText (float value, int)
{
    return (value > 0.0f ? "+" : "") + juce::String (value, 2);
}

juce::String integerToHostText (float value, int)
{
    return juce::String (value, 0);
}

juce::String oneDecimalToHostText (float value, int)
{
    return juce::String (value, 1);
}

void clearRootProperties (juce::ValueTree& state)
{
    while (state.getNumProperties() > 0)
        state.removeProperty (state.getPropertyName (0), nullptr);
}

bool isDirectChildOf (const juce::File& file, const juce::File& directory)
{
    return file.getParentDirectory().getFullPathName().equalsIgnoreCase (
        directory.getFullPathName());
}

bool readStrictNumber (const juce::var& storedValue, double& result)
{
    if (storedValue.isDouble() || storedValue.isInt()
        || storedValue.isInt64() || storedValue.isBool())
    {
        result = static_cast<double> (storedValue);
        return std::isfinite (result);
    }

    if (! storedValue.isString())
        return false;

    const auto text = storedValue.toString().trim();
    const auto length = text.length();
    if (length == 0)
        return false;

    auto index = 0;
    if (text[index] == '+' || text[index] == '-')
        ++index;

    auto hasDigits = false;
    while (index < length && juce::CharacterFunctions::isDigit (text[index]))
    {
        hasDigits = true;
        ++index;
    }

    if (index < length && text[index] == '.')
    {
        ++index;
        while (index < length && juce::CharacterFunctions::isDigit (text[index]))
        {
            hasDigits = true;
            ++index;
        }
    }

    if (! hasDigits)
        return false;

    if (index < length && (text[index] == 'e' || text[index] == 'E'))
    {
        ++index;
        if (index < length && (text[index] == '+' || text[index] == '-'))
            ++index;

        const auto exponentStart = index;
        while (index < length && juce::CharacterFunctions::isDigit (text[index]))
            ++index;
        if (index == exponentStart)
            return false;
    }

    if (index != length)
        return false;

    result = text.getDoubleValue();
    return std::isfinite (result);
}
}

TBShimmerAudioProcessor::TBShimmerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                      ),
      parameters (*this, &undoManager, "PARAMETERS", createParameterLayout())
#else
    : parameters (*this, &undoManager, "PARAMETERS", createParameterLayout())
#endif
{
    timeParameter = parameters.getRawParameterValue (timeId);
    sizeParameter = parameters.getRawParameterValue (sizeId);
    densityParameter = parameters.getRawParameterValue (densityId);
    scatterParameter = parameters.getRawParameterValue (scatterId);
    pitchParameter = parameters.getRawParameterValue (pitchId);
    reverseParameter = parameters.getRawParameterValue (reverseId);
    feedbackParameter = parameters.getRawParameterValue (feedbackId);
    toneParameter = parameters.getRawParameterValue (toneId);
    widthParameter = parameters.getRawParameterValue (widthId);
    mixParameter = parameters.getRawParameterValue (mixId);
    bypassParameter = parameters.getRawParameterValue (bypassId);
    levelVarParameter = parameters.getRawParameterValue (levelVarId);
    positionRandParameter = parameters.getRawParameterValue (positionRandId);
    pitchSprayParameter = parameters.getRawParameterValue (pitchSprayId);
    freezeParameter = parameters.getRawParameterValue (freezeId);
    sparkleParameter = parameters.getRawParameterValue (sparkleId);
    shatterParameter = parameters.getRawParameterValue (shatterId);
    hostBypassParameter = parameters.getParameter (bypassId);

    jassert (timeParameter != nullptr
             && sizeParameter != nullptr
             && densityParameter != nullptr
             && scatterParameter != nullptr
             && pitchParameter != nullptr
             && reverseParameter != nullptr
             && feedbackParameter != nullptr
             && toneParameter != nullptr
             && widthParameter != nullptr
             && mixParameter != nullptr
             && bypassParameter != nullptr
             && levelVarParameter != nullptr
             && positionRandParameter != nullptr
             && pitchSprayParameter != nullptr
             && freezeParameter != nullptr
             && sparkleParameter != nullptr
             && shatterParameter != nullptr
             && hostBypassParameter != nullptr);

    for (const auto* id : parameterIds)
        parameters.addParameterListener (id, this);
}

TBShimmerAudioProcessor::~TBShimmerAudioProcessor()
{
    for (const auto* id : parameterIds)
        parameters.removeParameterListener (id, this);
}

bool TBShimmerAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool TBShimmerAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool TBShimmerAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

void TBShimmerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    setLatencySamples (0);
    engine.prepare (sampleRate, samplesPerBlock,
                    juce::jmax (1, getTotalNumOutputChannels()));
}

void TBShimmerAudioProcessor::releaseResources()
{
    engine.reset();
}

void TBShimmerAudioProcessor::reset()
{
    engine.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool TBShimmerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
   #else
    const auto outputLayout = layouts.getMainOutputChannelSet();

    if (outputLayout != juce::AudioChannelSet::mono()
        && outputLayout != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainInputChannelSet() != outputLayout)
        return false;
   #endif

    return true;
   #endif
}
#endif

float TBShimmerAudioProcessor::sanitise (float value, float fallback,
                                         float minimum, float maximum) noexcept
{
    return std::isfinite (value) ? juce::jlimit (minimum, maximum, value) : fallback;
}

void TBShimmerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    processAudio (buffer, false);
}

void TBShimmerAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer,
                                                     juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    processAudio (buffer, true);
}

void TBShimmerAudioProcessor::processAudio (juce::AudioBuffer<float>& buffer,
                                             bool hostBypassed)
{
    const auto numInputChannels = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();

    for (auto channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    GranularShimmerEngine::Parameters current;
    current.timeMs = sanitise (timeParameter->load(), 240.0f, 60.0f, 1200.0f);
    current.sizeMs = sanitise (sizeParameter->load(), 160.0f, 1.0f, 500.0f);
    current.density = sanitise (densityParameter->load(), 9.0f, 2.0f, 2000.0f);
    current.scatterPercent = sanitise (scatterParameter->load(), 32.0f, 0.0f, 100.0f);
    current.pitchSemitones = sanitise (pitchParameter->load(), 12.0f, -12.0f, 24.0f);
    current.reversePercent = sanitise (reverseParameter->load(), 18.0f, 0.0f, 100.0f);
    current.feedbackPercent = sanitise (feedbackParameter->load(), 42.0f, 0.0f, 88.0f);
    current.toneHz = sanitise (toneParameter->load(), 9000.0f, 1200.0f, 18000.0f);
    current.widthPercent = sanitise (widthParameter->load(), 135.0f, 0.0f, 200.0f);
    current.mixPercent = sanitise (mixParameter->load(), 48.0f, 0.0f, 100.0f);
    current.levelVariationDb = sanitise (levelVarParameter->load(), 0.0f, 0.0f, 12.0f);
    current.positionRandomPercent = sanitise (positionRandParameter->load(), 32.0f, 0.0f, 100.0f);
    current.pitchSpraySemitones = sanitise (pitchSprayParameter->load(), 0.112f, 0.0f, 12.0f);
    current.frozen = freezeParameter->load() >= 0.5f;
    current.sparklePercent = sanitise (sparkleParameter->load(), 0.0f, 0.0f, 100.0f);
    current.shatterPercent = sanitise (shatterParameter->load(), 0.0f, 0.0f, 100.0f);

    const auto parameterBypassed = bypassParameter->load() >= 0.5f;
    engine.process (buffer, current, hostBypassed || parameterBypassed);
}

void TBShimmerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    clearRootProperties (state);
    state.setProperty ("pluginId", pluginStateId, nullptr);
    state.setProperty ("stateSchemaVersion", stateSchemaVersion, nullptr);
    state.setProperty ("presetFormatVersion", presetFormatVersion, nullptr);
    state.setProperty ("selectedFactoryPreset", selectedFactoryPreset.load(), nullptr);
    state.setProperty ("selectedUserPresetName", getSelectedUserPresetName(), nullptr);
    state.setProperty ("presetDirty", isCurrentPresetDirty(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TBShimmerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! isValidState (state))
            return;

        state = migrateStateToCurrent (state);
        if (! state.isValid() || ! isValidState (state))
            return;

        applyingStateOrPreset.store (true);
        parameters.replaceState (state);
        applyingStateOrPreset.store (false);

        const auto revision = parameterRevision.fetch_add (1) + 1;
        auto factoryIndex = static_cast<int> (
            state.getProperty ("selectedFactoryPreset", -1));
        if (! juce::isPositiveAndBelow (factoryIndex, getFactoryPresetCount()))
            factoryIndex = -1;

        auto userName = state.getProperty ("selectedUserPresetName").toString();
        if (factoryIndex >= 0)
            userName.clear();

        setPresetSelection (factoryIndex, userName, revision);
        if (static_cast<bool> (state.getProperty ("presetDirty", false)))
            selectedPresetRevision.store (revision - 1);
    }
}

bool TBShimmerAudioProcessor::isValidState (const juce::ValueTree& state) const
{
    if (! state.isValid()
        || state.getType() != parameters.state.getType()
        || state.getProperty ("pluginId").toString() != pluginStateId)
        return false;

    const auto schemaVersion = static_cast<int> (
        state.getProperty ("stateSchemaVersion", -1));
    const auto formatVersion = static_cast<int> (
        state.getProperty ("presetFormatVersion", -1));
    const auto expectedParameterCount = parameterCountForVersion (
        schemaVersion, formatVersion);

    if (expectedParameterCount == 0
        || state.getNumChildren() != static_cast<int> (expectedParameterCount))
        return false;

    std::array<bool, parameterIds.size()> found {};
    const auto expectedParameterType = parameters.state.getChild (0).getType();
    if (! expectedParameterType.isValid())
        return false;

    for (const auto& child : state)
    {
        if (child.getType() != expectedParameterType
            || ! child.hasProperty ("id") || ! child.hasProperty ("value"))
            return false;

        const auto id = child.getProperty ("id").toString();
        const auto expectedEnd = parameterIds.begin()
                               + static_cast<std::ptrdiff_t> (expectedParameterCount);
        const auto foundId = std::find_if (parameterIds.begin(), expectedEnd,
                                          [&id] (const auto* expected)
                                          { return id == expected; });
        if (foundId == expectedEnd)
            return false;

        const auto index = static_cast<size_t> (
            std::distance (parameterIds.begin(), foundId));
        if (found[index])
            return false;
        found[index] = true;

        double value = 0.0;
        if (! readStrictNumber (child.getProperty ("value"), value))
            return false;

        const auto* parameter = parameters.getParameter (parameterIds[index]);
        if (parameter == nullptr)
            return false;

        const auto& range = parameter->getNormalisableRange();
        if (value < static_cast<double> (range.start)
            || value > static_cast<double> (range.end))
            return false;

        const auto snapped = range.snapToLegalValue (static_cast<float> (value));
        const auto tolerance = juce::jmax (1.0e-4f, range.interval * 0.01f);
        if (std::abs (snapped - static_cast<float> (value)) > tolerance)
            return false;
    }

    return std::all_of (found.begin(),
                        found.begin() + static_cast<std::ptrdiff_t> (expectedParameterCount),
                        [] (bool value) { return value; });
}

juce::ValueTree TBShimmerAudioProcessor::migrateStateToCurrent (
    const juce::ValueTree& state) const
{
    if (! isValidState (state))
        return {};

    if (static_cast<int> (state.getProperty ("stateSchemaVersion", -1))
            == stateSchemaVersion)
        return state.createCopy();

    const auto sourceSchemaVersion = static_cast<int> (
        state.getProperty ("stateSchemaVersion", -1));

    auto migrated = state.createCopy();
    const auto parameterTreeType = migrated.getChild (0).getType();
    const auto appendParameter = [&migrated, &parameterTreeType]
                                 (const char* id, double value)
    {
        juce::ValueTree parameter (parameterTreeType);
        parameter.setProperty ("id", id, nullptr);
        parameter.setProperty ("value", value, nullptr);
        migrated.appendChild (parameter, nullptr);
    };

    if (sourceSchemaVersion == legacyStateSchemaVersion)
    {
        double oldScatter = 0.0;
        const auto scatterChild = state.getChildWithProperty ("id", scatterId);
        if (! scatterChild.isValid()
            || ! readStrictNumber (scatterChild.getProperty ("value"), oldScatter))
            return {};

        appendParameter (levelVarId, 0.0);
        appendParameter (positionRandId, oldScatter);
        appendParameter (pitchSprayId, oldScatter * 0.0035);
    }

    if (sourceSchemaVersion < 3)
        appendParameter (freezeId, 0.0);

    appendParameter (sparkleId, 0.0);
    appendParameter (shatterId, 0.0);
    migrated.setProperty ("stateSchemaVersion", stateSchemaVersion, nullptr);
    migrated.setProperty ("presetFormatVersion", presetFormatVersion, nullptr);

    return isValidState (migrated) ? migrated : juce::ValueTree {};
}

void TBShimmerAudioProcessor::parameterChanged (const juce::String&, float)
{
    if (! applyingStateOrPreset.load())
        parameterRevision.fetch_add (1);
}

int TBShimmerAudioProcessor::getNumPrograms()
{
    return getFactoryPresetCount();
}

int TBShimmerAudioProcessor::getCurrentProgram()
{
    return juce::jmax (0, selectedFactoryPreset.load());
}

void TBShimmerAudioProcessor::setCurrentProgram (int index)
{
    applyFactoryPreset (index);
}

const juce::String TBShimmerAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getFactoryPresetCount())
             ? juce::String (getFactoryPreset (index).name)
             : juce::String();
}

int TBShimmerAudioProcessor::getFactoryPresetCount() const noexcept
{
    return static_cast<int> (factoryPresets.size());
}

const TBShimmerAudioProcessor::FactoryPreset&
TBShimmerAudioProcessor::getFactoryPreset (int index) const
{
    jassert (juce::isPositiveAndBelow (index, getFactoryPresetCount()));
    return factoryPresets[static_cast<size_t> (
        juce::jlimit (0, getFactoryPresetCount() - 1, index))];
}

void TBShimmerAudioProcessor::setPresetSelection (
    int factoryIndex, const juce::String& userName, std::uint64_t revision)
{
    selectedFactoryPreset.store (factoryIndex);
    {
        const juce::ScopedLock lock (presetMetadataLock);
        selectedUserPresetName = userName;
    }
    selectedPresetRevision.store (revision);
}

juce::String TBShimmerAudioProcessor::getSelectedUserPresetName() const
{
    const juce::ScopedLock lock (presetMetadataLock);
    return selectedUserPresetName;
}

bool TBShimmerAudioProcessor::applyPresetValues (
    const std::array<float, parameterIds.size()>& values,
    const juce::String& transactionName,
    int factoryIndex,
    const juce::String& userName)
{
    std::array<juce::RangedAudioParameter*, parameterIds.size()> targets {};

    for (size_t index = 0; index < parameterIds.size(); ++index)
    {
        targets[index] = parameters.getParameter (parameterIds[index]);
        if (targets[index] == nullptr || ! std::isfinite (values[index]))
            return false;

        const auto& range = targets[index]->getNormalisableRange();
        if (values[index] < range.start || values[index] > range.end)
            return false;
    }

    undoManager.beginNewTransaction (transactionName);
    applyingStateOrPreset.store (true);

    for (size_t index = 0; index < targets.size(); ++index)
    {
        targets[index]->beginChangeGesture();
        targets[index]->setValueNotifyingHost (
            targets[index]->convertTo0to1 (values[index]));
        targets[index]->endChangeGesture();
    }

    applyingStateOrPreset.store (false);
    const auto revision = parameterRevision.fetch_add (1) + 1;
    setPresetSelection (factoryIndex, userName, revision);
    return true;
}

bool TBShimmerAudioProcessor::applyFactoryPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, getFactoryPresetCount()))
        return false;

    const auto& preset = getFactoryPreset (index);
    return applyPresetValues (
        {{ preset.time, preset.size, preset.density, preset.scatter,
           preset.pitch, preset.reverse, preset.feedback, preset.tone,
           preset.width, preset.mix, preset.bypass, preset.levelVar,
           preset.positionRand, preset.pitchSpray, preset.freeze,
           preset.sparkle, preset.shatter }},
        "Preset: " + juce::String (preset.name), index, {});
}

juce::File TBShimmerAudioProcessor::getUserPresetDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("TB_Shimmer")
        .getChildFile ("Presets")
        .getChildFile ("User");
}

juce::Result TBShimmerAudioProcessor::saveUserPreset (
    const juce::File& requestedFile)
{
    auto file = requestedFile;
    if (file.getFileExtension().isEmpty())
        file = file.withFileExtension (userPresetExtension);
    else if (! file.hasFileExtension (userPresetExtension))
        return juce::Result::fail (
            "Preset files must use the " + juce::String (userPresetExtension)
            + " extension.");

    const auto directory = getUserPresetDirectory();
    if (! isDirectChildOf (file, directory))
        return juce::Result::fail ("Preset must be saved in the TB_Shimmer User preset folder.");
    if (file.getFileNameWithoutExtension().trim().isEmpty())
        return juce::Result::fail ("Preset name cannot be empty.");

    const auto directoryResult = directory.createDirectory();
    if (directoryResult.failed())
        return directoryResult;

    auto state = parameters.copyState();
    clearRootProperties (state);
    state.setProperty ("pluginId", pluginStateId, nullptr);
    state.setProperty ("stateSchemaVersion", stateSchemaVersion, nullptr);
    state.setProperty ("presetFormatVersion", presetFormatVersion, nullptr);
    if (! isValidState (state))
        return juce::Result::fail ("Current parameter state is invalid.");

    juce::ValueTree preset ("TBShimmerPreset");
    preset.setProperty ("product", pluginStateId, nullptr);
    preset.setProperty ("presetFormatVersion", presetFormatVersion, nullptr);
    preset.setProperty ("stateSchemaVersion", stateSchemaVersion, nullptr);
    preset.setProperty ("name", file.getFileNameWithoutExtension(), nullptr);
    preset.appendChild (state, nullptr);

    const auto xml = preset.createXml();
    if (xml == nullptr)
        return juce::Result::fail ("Could not encode the preset.");

    juce::TemporaryFile temporaryPreset (file);
    if (! xml->writeTo (temporaryPreset.getFile(), {}))
        return juce::Result::fail ("Could not write the preset file.");

    if (! temporaryPreset.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("Could not replace the preset file.");

    setPresetSelection (-1, file.getFileNameWithoutExtension(),
                        parameterRevision.load());
    return juce::Result::ok();
}

juce::Result TBShimmerAudioProcessor::loadUserPreset (const juce::File& file)
{
    constexpr juce::int64 maximumPresetBytes = 512 * 1024;

    if (! isDirectChildOf (file, getUserPresetDirectory()))
        return juce::Result::fail ("Preset must be in the TB_Shimmer User preset folder.");
    if (! file.existsAsFile())
        return juce::Result::fail ("Preset file does not exist.");
    if (! file.hasFileExtension (userPresetExtension))
        return juce::Result::fail ("Unsupported preset file extension.");
    if (file.getSize() <= 0 || file.getSize() > maximumPresetBytes)
        return juce::Result::fail ("Preset file size is invalid.");

    const auto xml = juce::parseXML (file);
    if (xml == nullptr || ! xml->hasTagName ("TBShimmerPreset"))
        return juce::Result::fail ("Preset file is not valid XML.");

    const auto fileFormatVersion = xml->getIntAttribute ("presetFormatVersion");
    const auto fileSchemaVersion = xml->getIntAttribute ("stateSchemaVersion");
    const auto isKnownVersion = parameterCountForVersion (
        fileSchemaVersion, fileFormatVersion) != 0;

    if (xml->getStringAttribute ("product") != pluginStateId
        || ! isKnownVersion)
        return juce::Result::fail (
            "Preset belongs to a different product or version.");

    const auto preset = juce::ValueTree::fromXml (*xml);
    if (preset.getNumChildren() != 1)
        return juce::Result::fail ("Preset parameter data is missing.");

    auto state = preset.getChild (0);
    if (static_cast<int> (state.getProperty ("presetFormatVersion", -1))
            != fileFormatVersion
        || static_cast<int> (state.getProperty ("stateSchemaVersion", -1))
            != fileSchemaVersion
        || ! isValidState (state))
        return juce::Result::fail ("Preset parameter data is invalid.");

    state = migrateStateToCurrent (state);
    if (! state.isValid() || ! isValidState (state))
        return juce::Result::fail ("Preset parameter migration failed.");

    std::array<float, parameterIds.size()> values {};
    for (size_t index = 0; index < parameterIds.size(); ++index)
        values[index] = static_cast<float> (
            state.getChildWithProperty ("id", parameterIds[index])
                 .getProperty ("value"));

    if (! applyPresetValues (values,
                             "Preset: " + file.getFileNameWithoutExtension(),
                             -1, file.getFileNameWithoutExtension()))
        return juce::Result::fail ("Preset parameters could not be applied.");

    return juce::Result::ok();
}

juce::AudioProcessorEditor* TBShimmerAudioProcessor::createEditor()
{
    return new TBShimmerAudioProcessorEditor (*this);
}

juce::AudioProcessorValueTreeState::ParameterLayout
TBShimmerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameterList;

    auto timeRange = juce::NormalisableRange<float> (60.0f, 1200.0f, 1.0f);
    timeRange.setSkewForCentre (240.0f);
    auto sizeRange = juce::NormalisableRange<float> (1.0f, 500.0f, 0.1f);
    sizeRange.setSkewForCentre (60.0f);
    auto densityRange = juce::NormalisableRange<float> (2.0f, 2000.0f, 0.1f);
    densityRange.setSkewForCentre (24.0f);
    auto toneRange = juce::NormalisableRange<float> (1200.0f, 18000.0f, 1.0f);
    toneRange.setSkewForCentre (9000.0f);

    const auto millisecondsAttributes = juce::AudioParameterFloatAttributes()
        .withLabel ("ms")
        .withStringFromValueFunction (integerToHostText)
        .withValueFromStringFunction (numericValueFromText);
    const auto grainSizeAttributes = juce::AudioParameterFloatAttributes()
        .withLabel ("ms")
        .withStringFromValueFunction (oneDecimalToHostText)
        .withValueFromStringFunction (numericValueFromText);
    const auto densityAttributes = juce::AudioParameterFloatAttributes()
        .withLabel ("gr/s")
        .withStringFromValueFunction (oneDecimalToHostText)
        .withValueFromStringFunction (numericValueFromText);
    const auto percentAttributes = juce::AudioParameterFloatAttributes()
        .withLabel ("%")
        .withStringFromValueFunction (oneDecimalToHostText)
        .withValueFromStringFunction (numericValueFromText);
    const auto pitchAttributes = juce::AudioParameterFloatAttributes()
        .withLabel ("st")
        .withStringFromValueFunction (pitchToHostText)
        .withValueFromStringFunction (numericValueFromText);
    const auto frequencyAttributes = juce::AudioParameterFloatAttributes()
        .withLabel ("Hz")
        .withStringFromValueFunction (integerToHostText)
        .withValueFromStringFunction (frequencyFromText);
    const auto decibelsAttributes = juce::AudioParameterFloatAttributes()
        .withLabel ("dB")
        .withStringFromValueFunction (oneDecimalToHostText)
        .withValueFromStringFunction (numericValueFromText);
    const auto semitonesAttributes = juce::AudioParameterFloatAttributes()
        .withLabel ("st")
        .withStringFromValueFunction (oneDecimalToHostText)
        .withValueFromStringFunction (numericValueFromText);

    parameterList.push_back (std::make_unique<juce::AudioParameterFloat>
                             (juce::ParameterID { timeId, 1 }, "Time",
                              timeRange, 240.0f, millisecondsAttributes));
    parameterList.push_back (std::make_unique<juce::AudioParameterFloat>
                             (juce::ParameterID { sizeId, 1 }, "Size",
                              sizeRange, 160.0f, grainSizeAttributes));
    parameterList.push_back (std::make_unique<juce::AudioParameterFloat>
                             (juce::ParameterID { densityId, 1 }, "Density",
                              densityRange, 9.0f, densityAttributes));
    parameterList.push_back (std::make_unique<juce::AudioParameterFloat>
                             (juce::ParameterID { scatterId, 1 }, "Scatter",
                              juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 32.0f,
                              percentAttributes));
    parameterList.push_back (std::make_unique<juce::AudioParameterFloat>
                             (juce::ParameterID { pitchId, 1 }, "Pitch",
                              juce::NormalisableRange<float> (-12.0f, 24.0f, 0.01f), 12.0f,
                              pitchAttributes));
    parameterList.push_back (std::make_unique<juce::AudioParameterFloat>
                             (juce::ParameterID { reverseId, 1 }, "Reverse",
                              juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 18.0f,
                              percentAttributes));
    parameterList.push_back (std::make_unique<juce::AudioParameterFloat>
                             (juce::ParameterID { feedbackId, 1 }, "Feedback",
                              juce::NormalisableRange<float> (0.0f, 88.0f, 0.1f), 42.0f,
                              percentAttributes));
    parameterList.push_back (std::make_unique<juce::AudioParameterFloat>
                             (juce::ParameterID { toneId, 1 }, "Tone",
                              toneRange, 9000.0f, frequencyAttributes));
    parameterList.push_back (std::make_unique<juce::AudioParameterFloat>
                             (juce::ParameterID { widthId, 1 }, "Width",
                              juce::NormalisableRange<float> (0.0f, 200.0f, 0.1f), 135.0f,
                              percentAttributes));
    parameterList.push_back (std::make_unique<juce::AudioParameterFloat>
                             (juce::ParameterID { mixId, 1 }, "Mix",
                              juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 48.0f,
                              percentAttributes));
    parameterList.push_back (std::make_unique<juce::AudioParameterBool>
                             (juce::ParameterID { bypassId, 1 }, "Bypass", false));
    parameterList.push_back (std::make_unique<juce::AudioParameterFloat>
                             (juce::ParameterID { levelVarId, 1 }, "Level Var",
                              juce::NormalisableRange<float> (0.0f, 12.0f, 0.1f), 0.0f,
                              decibelsAttributes));
    parameterList.push_back (std::make_unique<juce::AudioParameterFloat>
                             (juce::ParameterID { positionRandId, 1 }, "Position Rand",
                              juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 32.0f,
                              percentAttributes));
    parameterList.push_back (std::make_unique<juce::AudioParameterFloat>
                             (juce::ParameterID { pitchSprayId, 1 }, "Pitch Spray",
                              juce::NormalisableRange<float> (0.0f, 12.0f), 0.112f,
                              semitonesAttributes));
    parameterList.push_back (std::make_unique<juce::AudioParameterBool>
                             (juce::ParameterID { freezeId, 1 }, "Freeze", false));
    parameterList.push_back (std::make_unique<juce::AudioParameterFloat>
                             (juce::ParameterID { sparkleId, 1 }, "Sparkle",
                              juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
                              percentAttributes));
    parameterList.push_back (std::make_unique<juce::AudioParameterFloat>
                             (juce::ParameterID { shatterId, 1 }, "Shatter",
                              juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
                              percentAttributes));

    return { parameterList.begin(), parameterList.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TBShimmerAudioProcessor();
}
