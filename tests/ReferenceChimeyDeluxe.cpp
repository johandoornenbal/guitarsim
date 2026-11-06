// Compile the unmodified upstream implementation separately to isolate VST2 symbols.
#include "shim/audioeffectx.h"
#include <set>
#include <string>
#include <cmath>
namespace upstreamChimeyDeluxe {
#include "../third_party/airwindows/ChimeyDeluxe/ChimeyDeluxe.cpp"
#include "../third_party/airwindows/ChimeyDeluxe/ChimeyDeluxeProc.cpp"
}
void referenceChimeyDeluxe(double rate, const float* params, double** data, int samples, int blockSize) {
    upstreamChimeyDeluxe::ChimeyDeluxe engine(nullptr); engine.setSampleRate(rate);
    for(int i=0;i<10;++i) engine.setParameter(i,params[i]);
    for(int offset=0;offset<samples;offset+=blockSize) {
        double* block[]{data[0]+offset,data[1]+offset};
        engine.processDoubleReplacing(block,block,std::min(blockSize,samples-offset));
    }
}
