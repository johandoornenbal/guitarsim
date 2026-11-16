#include "PunchyDeluxe2Engine.h"
#include "shim/audioeffectx.h"
#include <set>
#include <string>
#include <cmath>
#include <algorithm>
#include <array>
#include <vector>
#include <iostream>
#include <type_traits>
// Test access only: match the port's established deterministic dither seeds.
#define private public
namespace original {
#include "../third_party/airwindows/PunchyDeluxe2/PunchyDeluxe2.cpp"
#include "../third_party/airwindows/PunchyDeluxe2/PunchyDeluxe2Proc.cpp"
}
#undef private

template<class T> bool check() {
    for(double rate:{44100.,48000.,96000.,192000.})
    for(float setting:{0.f,.5f,1.f}) for(bool silence:{false,true}) {
        PunchyDeluxe2Engine port;
        original::PunchyDeluxe2 ref(nullptr); ref.setSampleRate(rate);
        port.reset(rate); ref.fpdL=0x12345678u; ref.fpdR=0x87654321u;
        for(int block=0;block<24;++block) {
            for(int i=0;i<10;++i) {
                const float value=block<12?setting:static_cast<float>((i+block)%11)/10.f;
                port.setParameter(i,value); ref.setParameter(i,value);
            }
            const int n=std::array<int,6>{0,1,17,127,512,1024}[block%6];
            std::vector<T> l(n+1),r(n+1);
            for(int i=0;i<n;++i) {
                l[i]=silence?T(0):T(1.2*std::sin((i+block*31)*.071));
                r[i]=silence?T(0):T(.7*std::cos((i+block*19)*.053));
            }
            auto a=l,b=r; T* actual[]{l.data(),r.data()}; T* expected[]{a.data(),b.data()};
            if constexpr(std::is_same_v<T,float>) { port.processReplacing(actual,actual,n); ref.processReplacing(expected,expected,n); }
            else { port.processDoubleReplacing(actual,actual,n); ref.processDoubleReplacing(expected,expected,n); }
            for(int i=0;i<n;++i) if(!std::isfinite(l[i]) || !std::isfinite(r[i]) || l[i]!=a[i] || r[i]!=b[i]) return false;
        }
    }
    return true;
}
int main() {
    if(!check<float>() || !check<double>()) { std::cerr<<"PunchyDeluxe2 upstream mismatch\n"; return 1; }
    std::cout<<"PunchyDeluxe2: 48 exact float/double reference cases passed\n";
}
