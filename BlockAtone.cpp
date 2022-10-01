#include "BlockAtone.h"
#include <math.h>
#include "daisysp.h"

using namespace daisysp;

void BlockATone::Init(float sample_rate)
{
    prevout_     = 0.0f;
    freq_        = 1000.0f;
    c2_          = 0.5f;
    sample_rate_ = sample_rate;
}

void BlockATone::Process( float *buf, size_t size ){
    for ( size_t i = 0; i < size; i++ ){
        float out;
        out      = c2_ * ( prevout_ + buf[ i ] );
        prevout_ = out - buf[ i ];
        buf[ i ] = out; 
    }
}

void BlockATone::CalculateCoefficients()
{
    float b, c2;

    b   = 2.0f - cosf(TWOPI_F * freq_ / sample_rate_);
    c2  = b - sqrtf(b * b - 1.0f);
    c2_ = c2;
}
