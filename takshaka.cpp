#include "daisy_seed.h"
#include "daisysp.h"
#include "SuperSawOsc.h"

#define ROOT_FREQ 110.0
#define SAMPLE_RATE 48000.0

using namespace daisy;
using namespace daisysp;

DaisySeed hw;

float driftKnobValue,
	shiftKnobValue,
	subKnobValue;
int detuneKnobCurveIndex, 
	intensityKnobCurveIndex, 
	subOscOctave = 1;
// TODO: SUBOSC OCTAVE IS SET VIA MENU, YET TO BE IMPLEPMENTED
SuperSawOsc superSaw;
Oscillator subOsc;
enum AdcChannel { 
	driftKnob, 
	shiftKnob, 
	subKnob,
	NUM_ADC_CHANNELS 
};
void AudioCallback( AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size ){
	for( size_t i = 0; i < size; i++ ){
		// SET ADJUST TO 1.0 - 0.8 DEPENDING ON THE SUB KNOB
		float superSawAdjust = 1.0 - fmap( subKnobValue, 0.0, 0.2 );
		float outputSignal = superSaw.Process() * superSawAdjust;
		outputSignal += subOsc.Process() * subKnobValue;
		out[0][i] = out[1][i] = superSaw.Process();
	}
}
int main(){
	hw.Init();
	superSaw.Init( SAMPLE_RATE );
	superSaw.SetFreq( ROOT_FREQ );
	subOsc.Init( SAMPLE_RATE );
	subOsc.SetWaveform( subOsc.WAVE_SIN );
	subOsc.SetFreq( ROOT_FREQ / ( subOscOctave + 1 ) );
	AdcChannelConfig adcConfig[ NUM_ADC_CHANNELS ];
    adcConfig[ driftKnob ].InitSingle( daisy::seed::A0 );
    adcConfig[ shiftKnob ].InitSingle( daisy::seed::A1 );
    adcConfig[ subKnob ].InitSingle( daisy::seed::A2 );
	hw.adc.Init( adcConfig, NUM_ADC_CHANNELS );
    hw.adc.Start();
	hw.SetAudioBlockSize( 4 );
	hw.SetAudioSampleRate( SaiHandle::Config::SampleRate::SAI_48KHZ );
	hw.StartAudio( AudioCallback );
	while( true ){
		driftKnobValue = hw.adc.GetFloat( driftKnob );
		shiftKnobValue = hw.adc.GetFloat( shiftKnob );
		subKnobValue = hw.adc.GetFloat( subKnob );
		superSaw.SetDrift( driftKnobValue );
		superSaw.SetShift( shiftKnobValue );
		System::Delay( 1 );
	}
}