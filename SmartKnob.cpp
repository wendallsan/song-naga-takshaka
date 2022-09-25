#include "daisy_seed.h"
#include "Utility/dsp.h"
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
                } outputValue_ = lastActiveValue_;
            } else if( isInterpolating_ ){
                fonepole( outputValue_, currentActualKnobValue_, 0.002 );
                lastActiveValue_ = outputValue_;
                if( fcompare( outputValue_, currentActualKnobValue_, 0.002 ) ) isInterpolating_ = false;
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