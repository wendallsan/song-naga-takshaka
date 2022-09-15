#pragma once
#ifndef DSY_SN_SUPERSAW_H
#include "daisy_seed.h"
#include "daisysp.h"
#ifdef __cplusplus
/* 
IMPLEMENTED USING DETAILS FROM:
https://forum.orthogonaldevices.com/uploads/short-url/rLjREzRcZvvK2527rFnTGvuwY1b.pdf
*/
namespace daisysp {
    #define DSY_SN_SUPERSAW_H
    #define NUMBER_OF_SAWS 7
    #define CURVE_POINTS 17
    #define DRIFT_LOOKUP_TABLE { 0.0, 0.00967268, 0.0220363, 0.0339636, 0.0467636, 0.0591273, 0.0714909, 0.0838545, 0.0967273, 0.121527, 0.147127, 0.193455, 0.243418, 0.2933815, 0.343345, 0.3928, 1.0 }
    #define SHIFT_LOOKUP_TABLE { 0.03836, 0.12, 0.19, 0.25, 0.31, 0.37, 0.42, 0.46, 0.5, 0.53, 0.56, 0.58, 0.59, 0.6, 0.605, 0.6, 0.59 }
    #define DETUNE_AMOUNTS { -0.11002314, -0.0628844, -0.01952357, 0.0, 0.01991221, 0.06216538, 0.10745242 }
    #define AMP_AMOUNTS { 0.4, 0.4, 0.4, 0.4, 0.5, 0.5, 0.45 }
    class SuperSawOsc {
        public:
            SuperSawOsc(){}
            ~SuperSawOsc(){}
            void Init( float );
            float Process();
            void SetDrift( float ),
                SetShift( float ),
                Reset(),
                SetFreq( float );
        private:
            float lookupDetune( float ),
                lookupAmp( float );
            float sampleRate_,
                freq_,
                drift_,
                shift_,
                phases_[ NUMBER_OF_SAWS ],
                detunes_[ NUMBER_OF_SAWS ] = DETUNE_AMOUNTS,
                ampAmounts_[ NUMBER_OF_SAWS ] = AMP_AMOUNTS,
                detuneLookupTable_[ CURVE_POINTS ] = DRIFT_LOOKUP_TABLE,
                ampLookupTable_[ CURVE_POINTS ] = SHIFT_LOOKUP_TABLE;
            Oscillator superSaws_[ NUMBER_OF_SAWS ];
            ATone hpf_;
            int detuneKnobCurveIndex_, 
                intensityKnobCurveIndex_;
    };
};
#endif
#endif