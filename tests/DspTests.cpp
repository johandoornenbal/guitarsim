#include "ChimeyGuitarEngine.h"
#include "ChimeyGuitar2Engine.h"
#include "ChimeyDeluxeEngine.h"
#include "Reference.h"
#include <array>
#include <vector>
#include <cmath>
#include <iostream>
#include <stdexcept>
static void require(bool b,const char* msg) { if(!b) throw std::runtime_error(msg); }
template<class Engine> void test(void(*ref)(double,const float*,double**,int,int),const char* name) {
    constexpr int count=4096;
    for(double rate:{44100.,48000.,96000.,192000.}) for(float setting:{0.f,.5f,1.f}) {
        Engine engine; engine.reset(rate);
        std::array<float,10> params{}; params.fill(setting);
        for(int i=0;i<10;++i) engine.setParameter(i,params[i]);
        std::vector<double> l(count),r(count);
        for(int i=0;i<count;++i) { l[i]=.3*std::sin(i*.071)+.05; r[i]=.2*std::cos(i*.053)-.03; }
        auto expectedL=l,expectedR=r; double* data[]{l.data(),r.data()}; double* expected[]{expectedL.data(),expectedR.data()};
        ref(rate,params.data(),expected,count,count); engine.processDoubleReplacing(data,data,count);
        for(int i=0;i<count;++i) {
            require(std::isfinite(l[i])&&std::isfinite(r[i]),"Nonfinite DSP output");
            require(std::abs(l[i]-expectedL[i])<1.e-10&&std::abs(r[i]-expectedR[i])<1.e-10,"Upstream mismatch");
        }
        // Compare the upstream algorithm with the same block partition: ChimeyGuitar
        // deliberately ramps its speed over each host block.
        engine.reset(rate); for(int i=0;i<10;++i) engine.setParameter(i,params[i]);
        for(int i=0;i<count;++i) { expectedL[i]=.3*std::sin(i*.071)+.05; expectedR[i]=.2*std::cos(i*.053)-.03; }
        ref(rate,params.data(),expected,count,127);
        for(int offset=0;offset<count;offset+=127) {
            const int n=std::min(127,count-offset);
            std::vector<double> inL(n),inR(n);
            for(int i=0;i<n;++i) { inL[i]=.3*std::sin((offset+i)*.071)+.05; inR[i]=.2*std::cos((offset+i)*.053)-.03; }
            double* block[]{inL.data(),inR.data()}; engine.processDoubleReplacing(block,block,n);
            for(int i=0;i<n;++i) require(std::abs(inL[i]-expectedL[offset+i])<1.e-10&&std::abs(inR[i]-expectedR[offset+i])<1.e-10,"Block continuity mismatch");
        }
        // Float processing, silent input, repeated reset, and zero-length blocks.
        engine.reset(rate); for(int i=0;i<10;++i) engine.setParameter(i,params[i]);
        std::array<float,512> fl{},fr{}; float* f[]{fl.data(),fr.data()};
        engine.processReplacing(f,f,0); engine.processReplacing(f,f,512);
        for(int i=0;i<512;++i) require(std::isfinite(fl[i])&&std::isfinite(fr[i]),"Float silence nonfinite");
    }
    std::cout<<name<<": upstream reference, endpoints, rates, block continuity and float silence passed\n";
}
int main() {
    try { test<ChimeyGuitarEngine>(referenceChimeyGuitar,"ChimeyGuitar"); test<ChimeyGuitar2Engine>(referenceChimeyGuitar2,"ChimeyGuitar2"); test<ChimeyDeluxeEngine>(referenceChimeyDeluxe,"ChimeyDeluxe"); }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
