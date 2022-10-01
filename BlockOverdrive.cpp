#include "dsp.h"
#include "BlockOverdrive.h"
#include <algorithm>

namespace daisysp
{
void BlockOverdrive::Init()
{
    SetDrive(.5f);
}

void BlockOverdrive::Process( float *buf, size_t bufferSize ){
    for( int i = 0; i < bufferSize; i++ ){
        float pre = pre_gain_ * buf[ i ];
        buf[ i ] = SoftClip( pre ) * post_gain_;
    }
}

void BlockOverdrive::SetDrive(float drive)
{
    drive  = fclamp(drive, 0.f, 1.f);
    drive_ = 2.f * drive;

    const float drive_2    = drive_ * drive_;
    const float pre_gain_a = drive_ * 0.5f;
    const float pre_gain_b = drive_2 * drive_2 * drive_ * 24.0f;
    pre_gain_              = pre_gain_a + (pre_gain_b - pre_gain_a) * drive_2;

    const float drive_squashed = drive_ * (2.0f - drive_);
    post_gain_ = 1.0f / SoftClip(0.33f + drive_squashed * (pre_gain_ - 0.33f));
}

} // namespace daisysp