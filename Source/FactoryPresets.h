#pragma once

#include "LonglandDSP.h"
#include <array>

namespace longland
{
struct FactoryPreset
{
    const char* name;
    Parameters parameters;
};

inline const std::array<FactoryPreset, 24>& factoryPresets()
{
    static const std::array<FactoryPreset, 24> presets {{
        { "School Hall Organ",   { Waveform::organ,       0.43f, 0.38f, 2800.0f, 0.08f, 0.0f, 0.018f, 0.34f, 0.82f, 0.48f, 0.09f, 0.72f } },
        { "Thin Brass",          { Waveform::saw,         0.34f, 0.27f, 2300.0f, 0.28f, 0.0f, 0.045f, 0.46f, 0.64f, 0.38f, 0.07f, 0.76f } },
        { "Old Practice Room",   { Waveform::square,      0.62f, 0.34f, 1850.0f, 0.10f, 0.0f, 0.022f, 0.72f, 0.66f, 0.92f, 0.13f, 0.70f } },
        { "Plastic Strings",     { Waveform::saw,         0.48f, 0.31f, 4100.0f, 0.06f, 0.0f, 0.62f, 1.28f, 0.76f, 1.42f, 0.42f, 0.68f } },
        { "Morning Assembly",    { Waveform::organ,       0.29f, 0.45f, 3200.0f, 0.12f, 0.0f, 0.012f, 0.26f, 0.86f, 0.44f, 0.06f, 0.74f } },
        { "Faulty Celeste",      { Waveform::triangle,    0.76f, 0.24f, 5900.0f, 0.18f, 0.0f, 0.004f, 0.92f, 0.34f, 1.62f, 0.16f, 0.73f } },
        { "Living Room 1978",    { Waveform::square,      0.51f, 0.52f, 2650.0f, 0.09f, 0.0f, 0.015f, 0.38f, 0.78f, 0.72f, 0.12f, 0.71f } },
        { "Warm Circuit",        { Waveform::triangle,    0.41f, 0.68f, 2150.0f, 0.14f, 0.0f, 0.028f, 0.64f, 0.70f, 0.76f, 0.05f, 0.78f } },
        { "Cold Circuit",        { Waveform::narrowPulse, 0.18f, 0.25f, 4800.0f, 0.05f, 0.0f, 0.002f, 0.18f, 0.88f, 0.21f, 0.02f, 0.72f } },
        { "Sunday Keyboard",     { Waveform::organ,       0.58f, 0.43f, 2250.0f, 0.16f, 0.0f, 0.016f, 0.45f, 0.74f, 0.68f, 0.18f, 0.70f } },
        { "Paper Reed",          { Waveform::narrowPulse, 0.37f, 0.18f, 1600.0f, 0.22f, 0.35f,0.036f, 0.52f, 0.62f, 0.42f, 0.02f, 0.78f } },
        { "Assembly Flute",      { Waveform::triangle,    0.32f, 0.22f, 3100.0f, 0.08f, 0.18f,0.075f, 0.56f, 0.78f, 0.66f, 0.04f, 0.82f } },
        { "Cardboard Piano",     { Waveform::square,      0.46f, 0.30f, 3550.0f, 0.10f, 0.0f, 0.003f, 0.72f, 0.08f, 0.54f, 0.03f, 0.84f } },
        { "Silver Pins",         { Waveform::triangle,    0.27f, 0.16f, 7200.0f, 0.04f, 0.0f, 0.002f, 0.84f, 0.22f, 1.86f, 0.14f, 0.68f } },
        { "Faded Ensemble",      { Waveform::saw,         0.68f, 0.36f, 2950.0f, 0.12f, 0.0f, 0.44f, 1.44f, 0.71f, 1.78f, 0.55f, 0.65f } },
        { "Small Transistors",   { Waveform::narrowPulse, 0.55f, 0.13f, 2400.0f, 0.17f, 0.0f, 0.009f, 0.29f, 0.68f, 0.33f, 0.02f, 0.80f } },
        { "Distant Classroom",   { Waveform::organ,       0.72f, 0.28f, 1450.0f, 0.06f, 0.0f, 0.085f, 0.92f, 0.61f, 1.36f, 0.21f, 0.70f } },
        { "Unlabelled Switch",   { Waveform::square,      0.63f, 0.20f, 3900.0f, 0.34f, 0.58f,0.006f, 0.48f, 0.52f, 0.46f, 0.08f, 0.74f } },
        { "Wood Veneer",         { Waveform::organ,       0.39f, 0.62f, 2050.0f, 0.05f, 0.0f, 0.021f, 0.36f, 0.83f, 0.64f, 0.10f, 0.77f } },
        { "Eight Voice Memory",  { Waveform::saw,         0.57f, 0.40f, 3350.0f, 0.15f, 0.0f, 0.028f, 0.58f, 0.70f, 0.81f, 0.16f, 0.69f } },
        { "Narrow Windows",      { Waveform::narrowPulse, 0.44f, 0.11f, 5100.0f, 0.24f, 0.42f,0.014f, 0.33f, 0.73f, 0.37f, 0.06f, 0.75f } },
        { "Capacitor Weather",   { Waveform::triangle,    0.82f, 0.38f, 2600.0f, 0.11f, 0.0f, 0.16f, 0.82f, 0.68f, 1.24f, 0.11f, 0.71f } },
        { "Quiet Demonstration", { Waveform::square,      0.24f, 0.32f, 4200.0f, 0.04f, 0.0f, 0.006f, 0.21f, 0.54f, 0.28f, 0.01f, 0.61f } },
        { "Unknown Familiar",    { Waveform::organ,       0.66f, 0.29f, 2750.0f, 0.19f, 0.12f,0.031f, 0.67f, 0.69f, 0.73f, 0.14f, 0.72f } }
    }};
    return presets;
}
}

