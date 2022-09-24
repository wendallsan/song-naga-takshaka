#pragma once
#ifndef DSY_SN_SMARTKNOB_H
#include "daisy_seed.h"
#ifdef __cplusplus

namespace daisysp {  
    class SmartKnob {
        public:
            float GetValue();
            void Update( float ),
                Activate(),
                Deactivate(),
                Init( bool, float );
        private:
            bool fcompare( float, float, float = 0.01 );
            bool isInterpolating_ = false,
                isActive_ = false,
                isWaitingToInterpolate_ = false;
            float lastActiveValue_ = 0.0,
                valueAtActivation_ = 0.0,
                outputValue_ = 0.0,
                valueAtLastUpdate_ = 0.0,
                currentActualKnobValue_;
    };
};
#endif
#endif