#include "daisy_seed.h"
#include "SmartKnob.h"
namespace daisysp {
    void SmartKnob::Init( bool isActive, float initalValue ){
        isActive_ = isActive;
        lastActiveValue_ = 
            valueAtActivation_ = 
            outputValue_ = 
            valueAtLastUpdate_ = 
            initalValue;
    }
    void SmartKnob::Update( float currentActualKnobValue ){
        currentActualKnobValue_ = currentActualKnobValue;
        if( isActive_ ){
            if( isWaitingToInterpolate_ ){
                // IF THE KNOB IS NOT WHERE IT WAS WHEN IT WAS ACTIVATED...
                if( !fcompare( valueAtActivation_, currentActualKnobValue_ ) ){
                    isWaitingToInterpolate_ = false;
                    isInterpolating_ = true;
                }
                outputValue_ = lastActiveValue_;
            } else if( isInterpolating_ ){
                float interpolationValue;
                // MOVE TOWARDS THE CURRENT KNOB VALUE
                interpolationValue = valueAtLastUpdate_ < currentActualKnobValue_?
                     valueAtLastUpdate_ + 0.002 :
                     valueAtLastUpdate_ - 0.002;
                if( fcompare( interpolationValue, currentActualKnobValue_, 0.002 ) ){
                    isInterpolating_ = false;
                    outputValue_ = lastActiveValue_ = valueAtLastUpdate_ = currentActualKnobValue_;
                } else outputValue_ = valueAtLastUpdate_ = interpolationValue;
            } else outputValue_ = lastActiveValue_ = currentActualKnobValue_;
        } else outputValue_ = lastActiveValue_;
    }
    void SmartKnob::Activate(){
        isActive_ = isWaitingToInterpolate_ = true;
        valueAtActivation_ = currentActualKnobValue_;
    }
    void SmartKnob::Deactivate(){ isActive_ = false; }
    float SmartKnob::GetValue(){ return outputValue_; }
    bool SmartKnob::fcompare( float a, float b, float epsilon ){ return abs( a - b ) <= epsilon; }
}