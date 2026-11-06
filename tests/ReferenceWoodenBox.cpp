// Compile the unmodified upstream implementation separately to isolate VST2 symbols.
#include "shim/audioeffectx.h"
#include <set>
#include <string>
#include <cmath>
namespace upstreamWoodenBox {
#include "../third_party/airwindows/WoodenBox/WoodenBox.cpp"
#include "../third_party/airwindows/WoodenBox/WoodenBoxProc.cpp"
}
void referenceWoodenBox(double rate, const float* params, double** data, int samples, int blockSize) {
    upstreamWoodenBox::WoodenBox engine(nullptr); engine.setSampleRate(rate);
    for(int i=0;i<3;++i) engine.setParameter(i,params[i]);
    for(int offset=0;offset<samples;offset+=blockSize) {
        double* block[]{data[0]+offset,data[1]+offset};
        engine.processDoubleReplacing(block,block,std::min(blockSize,samples-offset));
    }
}
