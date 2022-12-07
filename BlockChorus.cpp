#include "daisysp.h"
#include "BlockChorus.h"
#include <math.h>

using namespace daisysp;

//ChorusEngine stuff
void BlockChorusEngine::Init(float sample_rate)
{
    sample_rate_ = sample_rate;

    del_.Init();
    lfo_amp_  = 0.f;
    feedback_ = .2f;
    SetDelay(.75);

    lfo_phase_ = 0.f;
    SetLfoFreq(.3f);
    SetLfoDepth(.9f);
}

void BlockChorusEngine::Process( float *buf, size_t size ){
    for( size_t i = 0; i < size; i++ ){
        float lfo_sig = ProcessLfo();
        del_.SetDelay( lfo_sig + delay_ );
        float out = del_.Read();
        del_.Write( buf[ i ] + out * feedback_ );

        buf[ i ] = ( buf[ i ]+ out) * .5f; //equal mix 
    }
}

void BlockChorusEngine::SetLfoDepth(float depth)
{
    depth    = fclamp(depth, 0.f, .93f);
    lfo_amp_ = depth * delay_;
}

void BlockChorusEngine::SetLfoFreq(float freq)
{
    freq = 4.f * freq / sample_rate_;
    freq *= lfo_freq_ < 0.f ? -1.f : 1.f;  //if we're headed down, keep going
    lfo_freq_ = fclamp(freq, -.25f, .25f); //clip at +/- .125 * sr
}

void BlockChorusEngine::SetDelay(float delay)
{
    delay = (.1f + delay * 7.9f); //.1 to 8 ms
    SetDelayMs(delay);
}

void BlockChorusEngine::SetDelayMs(float ms)
{
    ms     = fmax(.1f, ms);
    delay_ = ms * .001f * sample_rate_; //ms to samples

    lfo_amp_ = fmin(lfo_amp_, delay_); //clip this if needed
}

void BlockChorusEngine::SetFeedback(float feedback)
{
    feedback_ = fclamp(feedback, 0.f, 1.f);
}

float BlockChorusEngine::ProcessLfo()
{
    lfo_phase_ += lfo_freq_;

    //wrap around and flip direction
    if(lfo_phase_ > 1.f)
    {
        lfo_phase_ = 1.f - (lfo_phase_ - 1.f);
        lfo_freq_ *= -1.f;
    }
    else if(lfo_phase_ < -1.f)
    {
        lfo_phase_ = -1.f - (lfo_phase_ + 1.f);
        lfo_freq_ *= -1.f;
    }

    return lfo_phase_ * lfo_amp_;
}

//Chorus Stuff
void BlockChorus::Init(float sample_rate)
{
    engines_[0].Init(sample_rate);
    engines_[1].Init(sample_rate);
    SetPan(.25f, .75f);

    gain_frac_ = .5f;
    sigl_ = sigr_ = 0.f;
}

void BlockChorus::Process( float *buffer, size_t size ){
    engines_[ 0 ].Process( buffer, size );
    for( size_t i = 0; i < size; i++ ) buffer[ i ] *= gain_frac_;
        
    // sigl_ = 0.f;
    // sigr_ = 0.f;

    // for( int i = 0; i < 2; i++ ){
    //     float sig = engines_[i].Process(in);
    //     sigl_ += (1.f - pan_[i]) * sig;
    //     sigr_ += pan_[i] * sig;
    // }

    // sigl_ *= gain_frac_;
    // sigr_ *= gain_frac_;

    // return sigl_;
}

float BlockChorus::GetLeft()
{
    return sigl_;
}

float BlockChorus::GetRight()
{
    return sigr_;
}

void BlockChorus::SetPan(float panl, float panr)
{
    pan_[0] = fclamp(panl, 0.f, 1.f);
    pan_[1] = fclamp(panr, 0.f, 1.f);
}

void BlockChorus::SetPan(float pan)
{
    SetPan(pan, pan);
}

void BlockChorus::SetLfoDepth(float depthl, float depthr)
{
    engines_[0].SetLfoDepth(depthl);
    engines_[1].SetLfoDepth(depthr);
}

void BlockChorus::SetLfoDepth(float depth)
{
    SetLfoDepth(depth, depth);
}

void BlockChorus::SetLfoFreq(float freql, float freqr)
{
    engines_[0].SetLfoFreq(freql);
    engines_[1].SetLfoFreq(freqr);
}

void BlockChorus::SetLfoFreq(float freq)
{
    SetLfoFreq(freq, freq);
}

void BlockChorus::SetDelay(float delayl, float delayr)
{
    engines_[0].SetDelay(delayl);
    engines_[1].SetDelay(delayr);
}

void BlockChorus::SetDelay(float delay)
{
    SetDelay(delay, delay);
}

void BlockChorus::SetDelayMs(float msl, float msr)
{
    engines_[0].SetDelayMs(msl);
    engines_[1].SetDelayMs(msr);
}

void BlockChorus::SetDelayMs(float ms)
{
    SetDelayMs(ms, ms);
}

void BlockChorus::SetFeedback(float feedbackl, float feedbackr)
{
    engines_[0].SetFeedback(feedbackl);
    engines_[1].SetFeedback(feedbackr);
}

void BlockChorus::SetFeedback(float feedback)
{
    SetFeedback(feedback, feedback);
}
