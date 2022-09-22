#pragma once
#ifndef DSY_SN_SMARTKNOB_H
#include "daisy_seed.h"
#ifdef __cplusplus

namespace daisysp {    
    class SmartKnob {
        public:
            SmartKnob(){}
            ~SmartKnob(){}
            float GetValueModeA(),
                GetValueModeB();                ;
            void SetMode( bool ),
                Update( float );
        private:
            bool needsToInterpolateModeA_ = false,
                isInterpolatingModeA_ = false,
                needsToInterpolateModeB_ = false,
                isInterpolatingModeB_ = false,
                currentModeIsA_ = true,
                fcompare( float, float, float );
            float currentKnobValue_,
                modeAInterpolateValue_,
                modeBInterpolateValue_,
                modeAValue_,
                modeBValue_,
                lastKnobValueAtModeChange_ = 0.0,
                lastKnobValueModeA_ = 0.0,
                lastKnobValueModeB_ = 0.0;
    };
};
#endif
#endif