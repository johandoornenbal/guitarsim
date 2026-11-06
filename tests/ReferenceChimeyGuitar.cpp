// Compile the unmodified upstream implementation separately to isolate VST2 symbols.
#include "shim/audioeffectx.h"
#include <set>
#include <string>
#include <cmath>
namespace upstreamChimeyGuitar {
#include "../third_party/airwindows/ChimeyGuitar/ChimeyGuitar.cpp"
#include "../third_party/airwindows/ChimeyGuitar/ChimeyGuitarProc.cpp"
}
void referenceChimeyGuitar(double rate, const float* params, double** data, int samples, int blockSize) {
    upstreamChimeyGuitar::ChimeyGuitar engine(nullptr); engine.setSampleRate(rate);
    for(int i=0;i<10;++i) engine.setParameter(i,params[i]);
    for(int offset=0;offset<samples;offset+=blockSize) {
        double* block[]{data[0]+offset,data[1]+offset};
        engine.processDoubleReplacing(block,block,std::min(blockSize,samples-offset));
    }
}
