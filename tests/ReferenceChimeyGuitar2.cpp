// Compile the unmodified upstream implementation separately to isolate VST2 symbols.
#include "shim/audioeffectx.h"
#include <set>
#include <string>
#include <cmath>
namespace upstreamChimeyGuitar2 {
#include "../third_party/airwindows/ChimeyGuitar2/ChimeyGuitar2.cpp"
#include "../third_party/airwindows/ChimeyGuitar2/ChimeyGuitar2Proc.cpp"
}
void referenceChimeyGuitar2(double rate, const float* params, double** data, int samples, int blockSize) {
    upstreamChimeyGuitar2::ChimeyGuitar2 engine(nullptr); engine.setSampleRate(rate);
    for(int i=0;i<10;++i) engine.setParameter(i,params[i]);
    for(int offset=0;offset<samples;offset+=blockSize) {
        double* block[]{data[0]+offset,data[1]+offset};
        engine.processDoubleReplacing(block,block,std::min(blockSize,samples-offset));
    }
}
