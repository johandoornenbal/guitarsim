#pragma once
#include <array>
namespace controls {
inline constexpr std::array<const char*,3> models{"ChimeyGuitar", "ChimeyGuitar2", "ChimeyDeluxe"};
inline constexpr std::array<const char*,10> guitarNames{"Compression","Presence","High","Mid","Low","Sub","High Speaker","Low Speaker","Speed","Output"};
inline constexpr std::array<const char*,10> deluxeNames{"Hell","Fizz","Pick","Satan","Danger","Critical","High Meat","Low Meat","Swing","Rarely"};
inline constexpr std::array<std::array<float,10>,3> defaults{{
    {{.5f,.8f,.7f,.5f,.7f,.4f,.8f,.4f,.3f,1.f}},
    {{.6f,.8f,.7f,.5f,.7f,.4f,.8f,.4f,.8f,.8f}},
    {{.5f,.5f,.5f,.5f,.5f,.5f,.5f,.5f,.5f,.5f}}
}};
inline const char* name(int model, int index) { return (model==2?deluxeNames:guitarNames)[static_cast<size_t>(index)]; }
inline juce::String id(int model, int index) { return juce::String(controls::models[static_cast<size_t>(model)])+"_"+juce::String(index); }
}
