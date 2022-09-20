#include "daisy_seed.h"
#include "daisysp.h"
#include "SuperSawOsc.h"
#define ROOT_MIDI_NOTE 48
#define SAMPLE_RATE 48000.0
using namespace daisy;
using namespace daisysp;
enum AdcChannel { 
	driftKnob, 
	shiftKnob, 
	subKnob,
	cutoffKnob,
	resKnob,
	// filterSettingsKnob, // TODO: FOR NOW WE'LL CONTROL FILTER SETTINGS WITH A KNOB RATHER THAN TOUCHPLATES
	driveKnob,
	NUM_ADC_CHANNELS 
};
enum FilterSettings {
	LP1,
	LP2,
	HP1,
	HP2,
	BP1,
	BP2
};
enum Modes {
	DEBUG,
	MIDI
};
DaisySeed hw;
MidiUsbHandler midi;
Switch modeSwitch;
float driftKnobValue,
	shiftKnobValue,
	subKnobValue,
	cutoffKnobValue,
	resKnobValue,
	filterSettingsKnobValue,
	driveKnobValue;
int detuneKnobCurveIndex, 
	intensityKnobCurveIndex, 
	subOscOctave = 1,
	filterSetting = LP1,
	midiNote = ROOT_MIDI_NOTE;
// bool mode = MIDI;
bool mode = DEBUG;
// TODO: SUBOSC OCTAVE IS SET VIA MENU, YET TO BE IMPLEPMENTED
SuperSawOsc superSaw;
Oscillator subOsc, lfo;
Svf filter1, filter2;
// FILTER SETTINGS KNOB: LP1, LP2, HP1, HP2, BP1, BP2 (?)
void AudioCallback( AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size ){
	for( size_t i = 0; i < size; i++ ){
		// SET ADJUST TO 1.0 - 0.8 DEPENDING ON THE SUB KNOB
		float superSawAdjust = fmap( 1.0 - subKnobValue, 0.6, 1.0 );
		float mixedSignal = superSaw.Process() * superSawAdjust;
		mixedSignal += subOsc.Process() * subKnobValue;

		float lfoSignal = lfo.Process();
		
		float filteredSignal = 0.0;
		// FIRST PASS THROUGH THE FILTERS
		filter1.Process( mixedSignal );
		switch( filterSetting ){
			case LP1:
			case LP2:
				filteredSignal = filter1.Low();
				break;
			case HP1:
			case HP2:
				filteredSignal = filter1.High();
				break;
			case BP1:
			case BP2:
				filteredSignal = filter1.Band();
		}		
		// SECOND PASS THROUGH THE FILTERS
		// if( filterSetting == LP2 || filterSetting == HP2 || filterSetting == BP2 ){
		// 	filter2.Process( filteredSignal );
		// 	switch( filterSetting ){
		// 		case LP2:
		// 			filteredSignal = filter2.Low();
		// 			break;
		// 		case HP2:			
		// 			filteredSignal = filter2.High();
		// 			break;
		// 		case BP2:
		// 			filteredSignal = filter2.Band();
		// 	}	
		// }	
		out[0][i] = out[1][i] = filteredSignal;
	}
}
void handleMidi(){
	midi.Listen();
	while( midi.HasEvents() ){
		auto midiEvent = midi.PopEvent();
		if( midiEvent.type == NoteOn ){
			auto noteMessage = midiEvent.AsNoteOn();
			if( noteMessage.velocity != 0 ) midiNote = noteMessage.note;
		}
	}
}
int main(){
	hw.Init();

	if( mode != DEBUG ){		
		MidiUsbHandler::Config midiConfig;
		midiConfig.transport_config.periph = MidiUsbTransport::Config::INTERNAL;
		midi.Init( midiConfig );
	} else {
		hw.StartLog();
	}

	superSaw.Init( SAMPLE_RATE );
	subOsc.Init( SAMPLE_RATE );
	subOsc.SetWaveform( subOsc.WAVE_SIN );
	filter1.Init( SAMPLE_RATE );	
	filter2.Init( SAMPLE_RATE );
	// lfo.Init( SAMPLE_RATE );
	// lfo.SetWaveform( lfo.WAVE_SIN );
	// lfo.SetFreq( 1.0 );
	AdcChannelConfig adcConfig[ NUM_ADC_CHANNELS ];
    adcConfig[ driftKnob ].InitSingle( daisy::seed::A0 );
    adcConfig[ shiftKnob ].InitSingle( daisy::seed::A1 );
    adcConfig[ subKnob ].InitSingle( daisy::seed::A2 );
    adcConfig[ cutoffKnob ].InitSingle( daisy::seed::A3 );
    adcConfig[ resKnob ].InitSingle( daisy::seed::A4 );
    adcConfig[ driveKnob ].InitSingle( daisy::seed::A5 );
	hw.adc.Init( adcConfig, NUM_ADC_CHANNELS );
    hw.adc.Start();
	modeSwitch.Init( hw.GetPin( 14 ), 100 );
	hw.SetAudioSampleRate( SaiHandle::Config::SampleRate::SAI_48KHZ );
	hw.StartAudio( AudioCallback );
	int debugCount = 0;
	while( true ){
		if( mode != DEBUG ) handleMidi();
		else midiNote = ROOT_MIDI_NOTE;
		modeSwitch.Debounce();
		float midiFreq = mtof( midiNote );
		superSaw.SetFreq( midiFreq );
		subOsc.SetFreq( midiFreq / ( subOscOctave + 1 ) );
		driftKnobValue = 1.0 - hw.adc.GetFloat( driftKnob );
		shiftKnobValue = 1.0 - hw.adc.GetFloat( shiftKnob );
		subKnobValue = 1.0 - hw.adc.GetFloat( subKnob );
		cutoffKnobValue = 1.0 - hw.adc.GetFloat( cutoffKnob );
		resKnobValue = 1.0 - hw.adc.GetFloat( resKnob );
		// filterSettingsKnobValue = hw.adc.GetFloat( filterSettingsKnob );
		driveKnobValue = 1.0 - hw.adc.GetFloat( driveKnob );
		superSaw.SetDrift( driftKnobValue );
		superSaw.SetShift( shiftKnobValue );
		filter1.SetFreq( fmap( cutoffKnobValue, 1.0, midiFreq ) );
		filter1.SetRes( fmap( resKnobValue, 0.0, 0.8 ) );
		filter1.SetDrive( driveKnobValue );
		if( mode == DEBUG ){
			debugCount++;
			if( debugCount == 1000 ){ // REPORT ONCE PER SECOND
				debugCount = 0;
				// hw.PrintLine( FLT_FMT3, FLT_VAR3( filterSetting ) );
				hw.PrintLine( modeSwitch.Pressed()? "pressed" : "meow"  );
			}
		}
		System::Delay( 1 );
	}
}