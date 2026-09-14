#pragma once
#include "game.h"

namespace pond {
enum class Sound { Splash, Bite, Fish, Metal, None };
constexpr unsigned SoundRate = 8000, SoundSamples = 960;
// Only real gameplay transitions sound; closing a book/help screen stays quiet.
inline Sound soundFor(Stage from, Stage to, bool object) {
    if(to==Stage::Waiting&&(from==Stage::Shore||from==Stage::Caught||from==Stage::Lost))return Sound::Splash;
    if(from==Stage::Waiting&&to==Stage::Bite)return Sound::Bite;
    if(from==Stage::Fight&&to==Stage::Caught)return object?Sound::Metal:Sound::Fish;
    return Sound::None;
}
// Four 120ms mono clips synthesized once into stable RAM, never recorded assets.
// Independent fixed noise seed: audio cannot consume the fishing RNG.
inline void synthSound(Sound kind, int8_t* out) {
    uint32_t noise=0x7351u,phase=0;int low=0;
    for(unsigned i=0;i<SoundSamples;++i){
        noise=noise*1664525u+1013904223u;
        low=(low*3+(int(noise>>24)-128))/4;
        unsigned t=i,duration=SoundSamples;
        if(kind==Sound::Bite){t=i%480;duration=320;}
        int sample=0;
        if(t<duration&&kind!=Sound::None){
            unsigned hz=kind==Sound::Metal?1150:kind==Sound::Bite?520-t:360-t/4;
            phase+=hz*65536u/SoundRate;
            int saw=int((phase>>8)&255);
            int triangle=saw<128?saw*2-128:383-saw*2;
            sample=kind==Sound::Metal?(triangle*3+(int((phase>>7)&127)-64))/4:
                (triangle+low*3)/4;
            int envelope=int((duration-t)*96/duration);
            if(t<24)envelope=envelope*int(t)/24;
            if(kind==Sound::Fish)sample=(sample+low)/2;
            sample=sample*envelope/128;
        }
        out[i]=int8_t(sample);
    }
    out[SoundSamples-1]=0;
}
}
