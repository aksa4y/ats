#include "particle_logic.hpp"
#include <cassert>
#include <limits>
#include <vector>
using namespace ats_exhaust;
int main() {
    constexpr std::uintptr_t base=0x140000000ULL;
    ParticleFields f{base+generator_vtable_rva,base+container_vtable_rva,40,0.090F,0.090F,3.0F};
    assert(classify(f,base)==0);
    assert(eligible(f,base,base+caller_return_rva));
    assert(!eligible(f,base,base+caller_return_rva+1));
    auto bad=f;bad.generator_vtable++; assert(classify(bad,base)==-1);
    bad=f;bad.container_vtable++; assert(classify(bad,base)==-1);
    bad=f;bad.count=41;assert(classify(bad,base)==-1);
    bad=f;bad.size_y=std::numeric_limits<float>::quiet_NaN();assert(classify(bad,base)==-1);
    bad=f;bad.lifetime=0.7F;assert(classify(bad,base)==-1);
    assert(classify(f,base+0x1000)==-1);
    f.count=20;f.size_x=f.size_y=0.04F;f.lifetime=0.7F;assert(classify(f,base)==1);
    f.count=4;f.size_x=f.size_y=0.02F;f.lifetime=0.2F;assert(classify(f,base)==2);
    f.count=24;f.size_x=f.size_y=0.03F;f.lifetime=3;assert(classify(f,base)==-1);
    std::vector<unsigned char> object(container_offset+sizeof(std::uintptr_t));
    const std::uint32_t count=40;const float size=0.09F,life=3.0F;
    auto vt=base+generator_vtable_rva, cv=base+container_vtable_rva;
    std::memcpy(object.data(),&vt,sizeof(vt));std::memcpy(object.data()+container_offset,&cv,sizeof(cv));
    std::memcpy(object.data()+0x638,&count,4);std::memcpy(object.data()+0x54,&size,4);
    std::memcpy(object.data()+0x58,&size,4);std::memcpy(object.data()+0x634,&life,4);
    assert(classify(read_fields(object.data()),base)==0);
    unsigned passed=0;
    for (std::uint32_t i=0;i<100000;i++) {
        assert(!admit(0,i));assert(admit(1,i));
        assert(!admit(std::numeric_limits<float>::quiet_NaN(),i));
        if(admit(0.25F,i))++passed;
    }
    assert(passed>24900 && passed<25100);
}
