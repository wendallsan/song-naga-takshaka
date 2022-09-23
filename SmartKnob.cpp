#include "daisy_seed.h"
#include "SmartKnob.h"
namespace daisysp {
    void SmartKnob::Update( float currentActualKnobValue ){
        if( isActive_ ){
            if( isWaitingToInterpolate_ ){
                // IF THE KNOB IS NOT WHERE IT WAS WHEN IT WAS ACTIVATED...
                if( !fcompare( valueAtActivation_, currentActualKnobValue ) ){
                    isWaitingToInterpolate_ = false;
                    isInterpolating_ = true;
                }
                outputValue_ = lastActiveValue_;
            } else if( isInterpolating_ ){
                float interpolationValue;
                // MOVE TOWARDS THE CURRENT KNOB VALUE
                interpolationValue = valueAtLastUpdate_ < currentActualKnobValue?
                     valueAtLastUpdate_ + 0.002 :
                     valueAtLastUpdate_ - 0.002;
                if( fcompare( interpolationValue, currentActualKnobValue, 0.002 ) ){
                    isInterpolating_ = false;
                    outputValue_ = lastActiveValue_ = valueAtLastUpdate_ = currentActualKnobValue;
                } else outputValue_ = valueAtLastUpdate_ = interpolationValue;
            } else outputValue_ = lastActiveValue_ = currentActualKnobValue;
        } else outputValue_ = lastActiveValue_;
    }
    void SmartKnob::Activate( float currentActualKnobValue ){
        isActive_ = isWaitingToInterpolate_ = true;
        valueAtActivation_ = currentActualKnobValue;
    }
    void SmartKnob::Deactivate(){ isActive_ = false; }
    float SmartKnob::GetValue(){ return outputValue_; }
    bool SmartKnob::fcompare( float a, float b, float epsilon ){ return abs( a - b ) <= epsilon; }
}