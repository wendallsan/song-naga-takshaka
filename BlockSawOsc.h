#pragma once
#ifndef SN_BLOCKSAWOSC_H
#define SN_BLOCKSAWOSC_H
#include <stdint.h>
#include "Utility/dsp.h"
#ifdef __cplusplus
namespace daisysp{
    class BlockSawOsc{
    public:
        BlockSawOsc() {}
        ~BlockSawOsc() {}
        void Init(float sample_rate){
            sr_ = sample_rate;
            sr_recip_  = 1.0f / sample_rate;
            freq_ = 100.0f;
            amp_ = 0.5f;
            phase_ = 0.0f;
            phase_inc_ = CalcPhaseInc(freq_);
        }
        inline void SetFreq(const float f){
            freq_ = f;
            phase_inc_ = CalcPhaseInc(f);
        }
        inline void SetAmp(const float a) { amp_ = a; }
        void Process( float *buf, size_t size );
        void Reset(float _phase = 0.0f) { phase_ = _phase; }
        void PhaseAdd(float _phase) { phase_ += (_phase * TWOPI_F); }
    private:
        float CalcPhaseInc(float f);
        float amp_, freq_;
        float sr_, sr_recip_, phase_, phase_inc_;
    };
}
#endif
#endif