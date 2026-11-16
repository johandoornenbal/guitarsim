// Compile the unmodified upstream implementation separately to isolate VST2 symbols.
#include "shim/audioeffectx.h"
#include <set>
#include <string>
#include <cmath>
namespace upstreamPunchyDeluxe2 {
#include "../third_party/airwindows/PunchyDeluxe2/PunchyDeluxe2.cpp"
#include "../third_party/airwindows/PunchyDeluxe2/PunchyDeluxe2Proc.cpp"
}
void referencePunchyDeluxe2(double rate, const float* params, double** data, int samples, int blockSize) {
    upstreamPunchyDeluxe2::PunchyDeluxe2 engine(nullptr); engine.setSampleRate(rate);
    for(int i=0;i<10;++i) engine.setParameter(i,params[i]);
    for(int offset=0;offset<samples;offset+=blockSize) {
        double* block[]{data[0]+offset,data[1]+offset};
        engine.processDoubleReplacing(block,block,std::min(blockSize,samples-offset));
    }
}
