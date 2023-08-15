#include "dsp.h"
#include "BlockSawOsc.h"

using namespace daisysp;

void BlockSawOsc::Process(float *buf, size_t bufferSize){
    for( size_t i = 0; i < bufferSize; i++  ){
        float out, t;
        out = -1.0f * (((phase_ * 2.0f)) - 1.0f);
        phase_ += phase_inc_;
        if(phase_ > 1.0f) phase_ -= 1.0f;
        buf[ i ] = out * amp_;
    }
}

float BlockSawOsc::CalcPhaseInc(float f){
    return f * sr_recip_;
}