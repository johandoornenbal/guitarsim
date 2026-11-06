#pragma once
// Test-only minimal VST2 interface. No Steinberg SDK code is used.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
using VstInt32=int;
using audioMasterCallback=void*;
enum VstPlugCategory { kPlugCategEffect };
constexpr int kVstMaxProgNameLen=24,kVstMaxParamStrLen=32,kVstMaxProductStrLen=64,kVstMaxVendorStrLen=64;
inline void vst_strncpy(char* dst,const char* src,int n) { std::strncpy(dst,src,static_cast<size_t>(n)); dst[n]=0; }
inline void float2string(float v,char* text,int n) { std::snprintf(text,static_cast<size_t>(n),"%f",static_cast<double>(v)); }
class AudioEffect { public: virtual ~AudioEffect()=default; };
class AudioEffectX : public AudioEffect {
public:
    AudioEffectX(audioMasterCallback,int,int) {}
    double getSampleRate() const { return rate; }
    void setSampleRate(double v) { rate=v; }
    void setNumInputs(int) {} void setNumOutputs(int) {} void setUniqueID(unsigned long) {}
    void canProcessReplacing() {} void canDoubleReplacing() {} void programsAreChunks(bool) {}
private: double rate=44100;
};

