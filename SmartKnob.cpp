#include "daisy_seed.h"
#include "SmartKnob.h"
namespace daisysp {
    void SmartKnob::SetMode( bool setToModeA ){
        currentModeIsA_ = setToModeA;
        if( currentModeIsA_ ) {
            needsToInterpolateModeA_ = true;
            lastKnobValueAtModeChange_ = lastKnobValueModeB_ = currentKnobValue_;
        } else {
            needsToInterpolateModeB_ = true;
            lastKnobValueAtModeChange_ = lastKnobValueModeA_ = currentKnobValue_;
        }
    }
    bool SmartKnob::fcompare( float a, float b, float epsilon = 0.01 ) {
        return fabs( a - b ) <= epsilon;
    }
    float SmartKnob::GetValueModeA(){
        return modeAValue_;
    }
    float SmartKnob::GetValueModeB(){
        return modeBValue_;
    }
    void SmartKnob::Update( float currentKnobValue ){
        currentKnobValue_ = currentKnobValue;
        if( currentModeIsA_ ){
            modeBValue_ = lastKnobValueModeB_;
            // IF WE NEED TO INTERPOLATE MODE A AND THE KNOB HAS BEEN MOVED FROM WHERE IT WAS WHEN WE LAST CHANGED MODES...
            if( needsToInterpolateModeA_ && !fcompare( currentKnobValue_, lastKnobValueAtModeChange_ ) ){                     
                needsToInterpolateModeA_ = false;
                isInterpolatingModeA_ = true;
                modeAInterpolateValue_ = lastKnobValueModeA_;
            }
            // IF WE ARE INTERPOLATING MODE A...
            if( isInterpolatingModeA_ ){
                // MOVE FROM THE SAVED VALUE TOWARDS THE CURRENT KNOB POSITION
                modeAInterpolateValue_ = ( modeAInterpolateValue_ < currentKnobValue_ )?
                    modeAInterpolateValue_ + 0.002 : 
                    modeAInterpolateValue_ - 0.002;
                // IF INTERPOLATE VALUE MATCHES THE CURRENT KNOB VALUE...
                if( fcompare( modeAInterpolateValue_, currentKnobValue_, 0.002 ) ){
                    isInterpolatingModeA_ = false;
                    modeAValue_ = currentKnobValue_;
                } else modeAValue_ = modeAInterpolateValue_;
            } else if( needsToInterpolateModeA_ ) modeAValue_ = lastKnobValueModeA_;            
            else modeAValue_ = currentKnobValue_;
        } else {
            modeAValue_ = lastKnobValueModeA_;
            if( needsToInterpolateModeB_ && !fcompare( currentKnobValue_, lastKnobValueAtModeChange_ ) ){
                needsToInterpolateModeB_ = false;
                isInterpolatingModeB_ = true;
                modeBInterpolateValue_ = lastKnobValueModeB_;
            }
            if( isInterpolatingModeB_ ){
                // MOVE FROM THE SAVED VALUE TOWARDS THE CURRENT KNOB POSITION
                modeBInterpolateValue_ = ( modeBInterpolateValue_ < currentKnobValue_ )?
                    modeBInterpolateValue_ + 0.002 : 
                    modeBInterpolateValue_ - 0.002;
                // IF INTERPOLATE VALUE MATCHES THE CURRENT KNOB VALUE...
                if( fcompare( modeBInterpolateValue_, currentKnobValue_, 0.002 ) ){
                    isInterpolatingModeB_ = false;
                    modeBValue_ = currentKnobValue_;
                } else modeBValue_ = modeBInterpolateValue_;
            } else if( needsToInterpolateModeB_ ) modeBValue_ = lastKnobValueModeB_;          
            else modeBValue_ = currentKnobValue_;
        }
    }
}