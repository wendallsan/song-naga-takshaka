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
	// filterModesKnob, // TODO: FOR NOW WE'LL CONTROL FILTER SETTINGS WITH A KNOB RATHER THAN TOUCHPLATES
	driveKnob,
	attackKnob,
	sustainKnob,
	decayReleaseKnob,
	clawsKnob,
	ADC_CHANNELS_COUNT 
};
enum SubOscWaveforms {
	SUBOSC_SINE_1,
	SUBOSC_SINE_2,
	SUBOSC_TRI_1,
	SUBOSC_TRI_2,
	SUBOSC_SQUARE_1,
	SUBOSC_SQUARE_2,
	SUBOSC_WAVEFORMS_COUNT
};
enum FilterModes {
	LP1,
	LP2,
	HP1,
	HP2,
	BP1,
	BP2
};
enum operationModes {
	OP_MODE_ALT,
	OP_MODE_NORMAL
};
DaisySeed hw;
MidiUsbHandler midi;
Switch modeSwitch;
float driftKnobValue,
	shiftKnobValue,
	subMixKnobValue,
	cutoffKnobValue,
	resKnobValue,
	filterSettingsKnobValue,
	driveKnobValue,
	midiFreq,
	attackKnobValue,
	sustainKnobValue,
	decayReleaseKnobValue,
	clawsKnobValue,
	currentSubMixKnobInterpolationValue = 2.0,
	lastSubMixKnobValue = 0.0,
	subTypeKnobValue = 0.0,
	lastSubTypeKnobValue = 0.0,
	lastSubMixKnobValueAtModeChange = 0.0, // THIS STORES THE LAST VALUE OF THIS KNOB AT MODE CHANGE
	lastSubTypeKnobValueAtModeChange = 0.0,
	lastSubKnobValueAtModeChange = 0.0;
int detuneKnobCurveIndex, 
	intensityKnobCurveIndex, 
	subOscOctave = 1,
	filterSetting = LP1,
	midiNote = ROOT_MIDI_NOTE;
bool debugMode = true,
	operationMode = OP_MODE_NORMAL,
	lastOperationMode = OP_MODE_NORMAL,
	envGate = false,
	normalInterpolates[ 1 ] = { false }, // STORES WHETHER NORMAL KNOBS ARE CURRENTLY INTERPOLATING
	altInterpolates[ 1 ] = { false }; // STORES WHETHER ALT KNOBS ARE CURRENTLY INTERPOLATING
// TODO: SUBOSC OCTAVE IS SET VIA MENU, YET TO BE IMPLEPMENTED
SuperSawOsc superSaw;
Oscillator subOsc, lfo;
Svf filter1, filter2;
Adsr pounce, ampEnv;
// FILTER SETTINGS KNOB: LP1, LP2, HP1, HP2, BP1, BP2 (?)
void AudioCallback( AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size ){
	for( size_t i = 0; i < size; i++ ){
		// SET ADJUST TO 1.0 - 0.8 DEPENDING ON THE SUB KNOB
		float superSawAdjust = fmap( 1.0 - subMixKnobValue, 0.6, 1.0 );
		float mixedSignal = superSaw.Process() * superSawAdjust;
		float subSignal = subOsc.Process();
		mixedSignal += subSignal * subMixKnobValue;

		float lfoSignal = lfo.Process();
		
		// FOR NOW, JUST USE POUNCE TO MODULATE THE FILTER ENVELOPE
		// FOR NOW MODULATION AMOUNT IS FIXED
		float cutoffMod = pounce.Process( envGate ) * 0.5;
		cutoffMod += cutoffKnobValue;
		float filterFreq = fmap( cutoffMod, 1.0, fclamp( midiFreq * 16.0, 20.0, 20000.0 ) );
		filter1.SetFreq( filterFreq );

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

		// FOR NOW AMP MOD AMOUNT IS FIXED
		// float ampMod = ampEnv.Process( envGate ) * 0.5;
		float ampMod = ampEnv.Process( envGate );
		ampMod += clawsKnobValue;
		out[0][i] = out[1][i] = filteredSignal * ampMod;
	}
}
void handleMidi(){
	midi.Listen();
	while( midi.HasEvents() ){
		auto midiEvent = midi.PopEvent();
		if( midiEvent.type == NoteOn ){
			auto noteMessage = midiEvent.AsNoteOn();
			if( noteMessage.velocity != 0 ){
				midiNote = noteMessage.note;
				envGate = true;
			} else envGate = false;
		} else if( midiEvent.type == NoteOff ) envGate = false;
	}
}
// RETURNS TRUE WHEN A AND BE ARE CLOSE TOGETHER
bool fcompare( float a, float b, float epsilon = 0.01 ) {
  return fabs( a - b ) <= epsilon;
}
void handleSubKnob(){
	float currentSubKnobValue = 1.0 - hw.adc.GetFloat( subKnob );
	if( operationMode != lastOperationMode ){ // IF WE JUST SWITCHED BETWEEN MODES ...
		if( operationMode == OP_MODE_NORMAL ){ // IF WE JUST SWITCHED INTO NORMAL MODE ...
			normalInterpolates[ 0 ] = true;
			lastSubTypeKnobValue = 
				lastSubKnobValueAtModeChange = 
				lastSubTypeKnobValueAtModeChange = 
				currentSubKnobValue;
		} else { // ELSE WE JUST SWITCHED INTO ALT MODE
			lastSubMixKnobValueAtModeChange = currentSubKnobValue;
			altInterpolates[ 0 ] = true;
			lastSubMixKnobValue = 
				lastSubKnobValueAtModeChange  = 
				lastSubMixKnobValueAtModeChange = 
				currentSubKnobValue;
		}			
	}
	// HANDLE DUAL-MODE KNOB
	if( operationMode == OP_MODE_NORMAL ){ // IF WE ARE IN NORMAL MODE ...
		if( normalInterpolates[ 0 ] ){ // IF THE SUB KNOB IS INTERPOLATING ...
			float currentSubMixKnobValue = currentSubKnobValue;

			// DID THE CURRENT KNOB VALUE CHANGE FROM WHERE IT WAS WHEN THE MODE WAS LAST CHANGED?
			if( !fcompare( currentSubMixKnobValue, lastSubKnobValueAtModeChange ) ){
				// IF CURRENT SUB KNOB INTERPOLATION VALUE IS NOT VALID (AKA IN A RANGE BETWEEN 0.0 AND 1.0),
				//  THIS IS OUR FIRST PASS AT INTERPOLATION. SET IT TO THE LAST SUBMIX KNOB VALUE
				if( currentSubMixKnobInterpolationValue < 0.0 || currentSubMixKnobInterpolationValue > 1.0 )
					currentSubMixKnobInterpolationValue = lastSubMixKnobValue;				
				// MOVE FROM THE SAVED VALUE TOWARDS THE CURRENT KNOB POSITION
				if( currentSubMixKnobInterpolationValue < currentSubKnobValue ) 
					currentSubMixKnobInterpolationValue += 0.0002;
				else currentSubMixKnobInterpolationValue -= 0.0002;				
				// IF INTERPOLATE VALUE MATCHES THE CURRENT KNOB VALUE...
				if( fcompare( currentSubMixKnobInterpolationValue, currentSubKnobValue, 0.002 ) ){
					// TURN OFF INTERPOLATE
					normalInterpolates[ 0 ] = false;
					// SET INTERPOLATE VALUE TO AN INVALID STATE FOR NEXT TIME
					currentSubMixKnobInterpolationValue = 2.0;
					subMixKnobValue = currentSubKnobValue;
				} else subMixKnobValue = currentSubMixKnobInterpolationValue;				
			} else subMixKnobValue = lastSubMixKnobValueAtModeChange;
		} else subMixKnobValue = lastSubMixKnobValue = currentSubKnobValue;
	} else subTypeKnobValue = lastSubTypeKnobValue = currentSubKnobValue;
}
void handleKnobs(){
	handleSubKnob(); // HANDLE SUB KNOB, WHICH HAS 2 MODES
	// HANDLE THE SINGLE-MODE KNOBS
	driftKnobValue = 1.0 - hw.adc.GetFloat( driftKnob );
	shiftKnobValue = 1.0 - hw.adc.GetFloat( shiftKnob );
	cutoffKnobValue = 1.0 - hw.adc.GetFloat( cutoffKnob );
	resKnobValue = 1.0 - hw.adc.GetFloat( resKnob );
	// filterSettingsKnobValue = hw.adc.GetFloat( filterSettingsKnob );
	driveKnobValue = 1.0 - hw.adc.GetFloat( driveKnob );
	attackKnobValue = 1.0 - hw.adc.GetFloat( attackKnob );
	sustainKnobValue = 1.0 - hw.adc.GetFloat( sustainKnob );
	decayReleaseKnobValue = 1.0 - hw.adc.GetFloat( decayReleaseKnob );
	clawsKnobValue = 1.0 - hw.adc.GetFloat( clawsKnob );
}
void handleSubOscWave(){
	// SUBWAVE HAS 6 MODES
	int subWave = subTypeKnobValue * SUBOSC_WAVEFORMS_COUNT ;  // int range 0 - 2
	switch( subWave ){
		case 0:
			subOsc.SetWaveform( subOsc.WAVE_SIN );
			subOscOctave = 1;
			break;
		case 1:
			subOsc.SetWaveform( subOsc.WAVE_SIN );
			subOscOctave = 2;
			break;
		case 2:
			subOsc.SetWaveform( subOsc.WAVE_POLYBLEP_TRI );
			subOscOctave = 1;
			break;
		case 3:
			subOsc.SetWaveform( subOsc.WAVE_POLYBLEP_TRI );
			subOscOctave = 2;
			break;
		case 4:
			subOsc.SetWaveform( subOsc.WAVE_POLYBLEP_SQUARE );
			subOscOctave = 1;
			break;
		case 5:
			subOsc.SetWaveform( subOsc.WAVE_POLYBLEP_SQUARE );
			subOscOctave = 2;
	}
}
void initADC(){
	AdcChannelConfig adcConfig[ ADC_CHANNELS_COUNT ];
    adcConfig[ driftKnob ].InitSingle( daisy::seed::A0 );
    adcConfig[ shiftKnob ].InitSingle( daisy::seed::A1 );
    adcConfig[ subKnob ].InitSingle( daisy::seed::A2 );
    adcConfig[ cutoffKnob ].InitSingle( daisy::seed::A3 );
    adcConfig[ resKnob ].InitSingle( daisy::seed::A4 );
    adcConfig[ driveKnob ].InitSingle( daisy::seed::A5 );
    adcConfig[ attackKnob ].InitSingle( daisy::seed::A6 );
    adcConfig[ sustainKnob ].InitSingle( daisy::seed::A7 );
    adcConfig[ decayReleaseKnob ].InitSingle( daisy::seed::A8 );
    adcConfig[ clawsKnob ].InitSingle( daisy::seed::A9 );
	hw.adc.Init( adcConfig, ADC_CHANNELS_COUNT );
    hw.adc.Start();
}
int main(){
	hw.Init();
	if( !debugMode ){		
		MidiUsbHandler::Config midiConfig;
		midiConfig.transport_config.periph = MidiUsbTransport::Config::INTERNAL;
		midi.Init( midiConfig );
	} else {
		hw.StartLog();
	}
	superSaw.Init( SAMPLE_RATE );
	subOsc.Init( SAMPLE_RATE );
	filter1.Init( SAMPLE_RATE );
	filter2.Init( SAMPLE_RATE );
	pounce.Init( SAMPLE_RATE );
	ampEnv.Init( SAMPLE_RATE );
	// lfo.Init( SAMPLE_RATE );
	// lfo.SetWaveform( lfo.WAVE_SIN );
	// lfo.SetFreq( 1.0 );
	initADC();
	modeSwitch.Init( hw.GetPin( 14 ), 100 );
	hw.SetAudioSampleRate( SaiHandle::Config::SampleRate::SAI_48KHZ );
	hw.StartAudio( AudioCallback );
	int debugCount = 0;
	while( true ){
		if( !debugMode ) handleMidi();
		else midiNote = ROOT_MIDI_NOTE;
		midiFreq = mtof( midiNote );
		superSaw.SetFreq( midiFreq );
		subOsc.SetFreq( midiFreq / ( subOscOctave + 1 ) );
		modeSwitch.Debounce();
		operationMode = !modeSwitch.Pressed();
		handleKnobs();
		lastOperationMode = operationMode; // UPDATE lastOperationMode
		
		// DO STUFF
		superSaw.SetDrift( driftKnobValue );
		superSaw.SetShift( shiftKnobValue );
		filter1.SetRes( fmap( resKnobValue, 0.0, 0.8 ) );
		filter1.SetDrive( driveKnobValue );

		pounce.SetAttackTime( fmap( attackKnobValue, 0.005, 4.0 ) );
		pounce.SetSustainLevel( sustainKnobValue );
		pounce.SetDecayTime( fmap( decayReleaseKnobValue, 0.005, 4.0 ) );
		pounce.SetReleaseTime( fmap( decayReleaseKnobValue, 0.005, 4.0 ) );

		// TODO: GIVE AMPENV ITS OWN SETTINGS VIA ALT MODE
		// FOR NOW: SET AMPENV TO SAME SETTINGS AS POUNCE
		ampEnv.SetAttackTime( fmap( attackKnobValue, 0.005, 4.0 ) );
		ampEnv.SetSustainLevel( sustainKnobValue );
		ampEnv.SetDecayTime( fmap( decayReleaseKnobValue, 0.005, 4.0 ) );
		ampEnv.SetReleaseTime( fmap( decayReleaseKnobValue, 0.005, 4.0 ) );

		handleSubOscWave();
		// LIGHT THE LED WHEN IN ALT MODE
		hw.SetLed( !operationMode );
		if( debugMode ){
			debugCount++;
			if( debugCount >= 1000  ){
				hw.Print( operationMode? ". " : "! " );
				hw.Print( normalInterpolates[ 0 ]? "n! " : "n. " );
				// hw.Print( altInterpolates[ 0 ]? "a! " : "a. " );
				// hw.Print( "type: " );
				// hw.Print( FLT_FMT3, FLT_VAR3( subTypeKnobValue ) );
				hw.Print( " MIX: " );
				hw.Print( FLT_FMT3, FLT_VAR3( subMixKnobValue ) );
				hw.Print( ", KNOB: " );
				hw.Print( FLT_FMT3, FLT_VAR3( 1.0 - hw.adc.GetFloat( subKnob ) ) );
				hw.Print( ", I: " );
				hw.PrintLine( FLT_FMT3, FLT_VAR3( currentSubMixKnobInterpolationValue ) );
				debugCount = 0;
			}
		}
		System::Delay( 1 );
	}
}