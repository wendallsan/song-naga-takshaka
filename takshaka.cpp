#include "daisy_seed.h"
#include "daisysp.h"
#include "SuperSawOsc.h"
#include "SmartKnob.h"
#define ROOT_MIDI_NOTE 48
#define SAMPLE_RATE 48000.0
using namespace daisy;
using namespace daisysp;
enum AdcChannel { 
	driftKnob, 
	shiftKnob, 
	growlKnob,
	howlKnob,
	resKnob,
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
	FILTER_MODE_LP, 
	FILTER_MODE_HP, 
	FILTER_MODE_BP, 
	FILTER_MODE_COMB,
	FILTER_MODES_COUNT
};
enum operationModes { OP_MODE_ALT, OP_MODE_NORMAL };
float driftKnobValue,
	shiftKnobValue,
	subMixValue,
	howlKnobValue,
	cutoffValue,
	filterModeValue,
	resValue,
	polesValue,
	driveKnobValue,
	midiFreq,
	attackKnobValue,
	sustainKnobValue,
	decayReleaseKnobValue,
	clawsKnobValue,
	ampValue,
	ampEnvModAmountValue,
	subTypeValue = 0.0,
	combFilterBuffer[ 9600 ],
	pounceAttackValue,
	ampEnvAttackValue,
	pounceSustainValue,
	ampEnvSustainValue,
	pounceDecayReleaseValue,
	ampEnvDecayReleaseValue;
int detuneKnobCurveIndex, 
	intensityKnobCurveIndex, 
	subOscOctave = 1,
	midiNote = ROOT_MIDI_NOTE;
bool debugMode = false,
	operationMode = OP_MODE_NORMAL,
	lastOperationMode = OP_MODE_NORMAL,
	envGate = false;
DaisySeed hw;
MidiUsbHandler midi;
Switch modeSwitch;
SmartKnob growlSmartKnob,
	howlSmartKnob,
	resSmartKnob,
	attackSmartKnob,
	sustainSmartKnob,
	decayReleaseSmartKnob,
	clawsSmartKnob;
SuperSawOsc superSaw;
Oscillator subOsc, lfo;
Svf filter1, filter2;
Comb combFilter;
Adsr pounce, ampEnv;
void AudioCallback( AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size ){
	for( size_t i = 0; i < size; i++ ){
		// SET ADJUST TO 1.0 - 0.8 DEPENDING ON THE SUB KNOB
		float superSawAdjust = fmap( 1.0 - subMixValue, 0.8, 1.0 );
		float mixedSignal = superSaw.Process() * superSawAdjust;
		float subSignal = subOsc.Process();
		mixedSignal += subSignal * subMixValue;

		float lfoSignal = lfo.Process();
		
		// FOR NOW, JUST USE POUNCE TO MODULATE THE FILTER ENVELOPE
		// FOR NOW MODULATION AMOUNT IS FIXED
		float cutoffMod = pounce.Process( envGate ) * 0.5;
		cutoffMod += cutoffValue;
		float filterFreq = fmap( cutoffMod, 1.0, fclamp( midiFreq * 16.0, 20.0, 20000.0 ) );
		filter1.SetFreq( filterFreq );
		filter2.SetFreq( filterFreq );
		float filteredSignal = 0.0;
		// FIRST PASS THROUGH THE FILTERS
		filter1.Process( mixedSignal );
		float combSignal = combFilter.Process( mixedSignal );
		int currentFilterMode = filterModeValue * FILTER_MODES_COUNT;
		switch( currentFilterMode ){
			case FILTER_MODE_LP:
				filteredSignal = filter1.Low();
				break;
			case FILTER_MODE_HP:
				filteredSignal = filter1.High();
				break;
			case FILTER_MODE_BP:
				filteredSignal = filter1.Band();
				break;
			case FILTER_MODE_COMB:
				filteredSignal = combSignal;
				break;
		}
		if( polesValue > 0.5 &&
			(
				currentFilterMode == FILTER_MODE_LP ||
				currentFilterMode == FILTER_MODE_HP ||
				currentFilterMode == FILTER_MODE_BP
			)
		){
			filter2.Process( filteredSignal );
			switch( currentFilterMode ){
				case FILTER_MODE_LP:
					filteredSignal = filter2.Low();
					break;
				case FILTER_MODE_HP:
					filteredSignal = filter2.High();
					break;
				case FILTER_MODE_BP:
					filteredSignal = filter2.Band();
					break;
			}
		}
		float ampMod = ampEnv.Process( envGate ) * ampEnvModAmountValue;
		ampMod = fclamp( ampMod + ampValue, 0.0, 1.0 );
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
void handleKnobs(){
	// HANDLE THE SMART KNOBS
	growlSmartKnob.Update( 1.0 - hw.adc.GetFloat( growlKnob ) );
	subMixValue = growlSmartKnob.GetValueModeA();
	subTypeValue = growlSmartKnob.GetValueModeB();
	howlSmartKnob.Update( 1.0 - hw.adc.GetFloat( howlKnob ) );
	cutoffValue = howlSmartKnob.GetValueModeA();
	filterModeValue = howlSmartKnob.GetValueModeB();
	resSmartKnob.Update( 1.0 - hw.adc.GetFloat( resKnob ) );
	resValue = resSmartKnob.GetValueModeA();
	polesValue = resSmartKnob.GetValueModeB();
	attackSmartKnob.Update( 1.0 - hw.adc.GetFloat( attackKnob ) );
	pounceAttackValue = attackSmartKnob.GetValueModeA();
	ampEnvAttackValue = attackSmartKnob.GetValueModeB();
	sustainSmartKnob.Update( 1.0 - hw.adc.GetFloat( sustainKnob ) );
	pounceSustainValue = sustainSmartKnob.GetValueModeA();
	ampEnvSustainValue = sustainSmartKnob.GetValueModeB();
	decayReleaseSmartKnob.Update( 1.0 - hw.adc.GetFloat( decayReleaseKnob ) );
	pounceDecayReleaseValue = decayReleaseSmartKnob.GetValueModeA();
	ampEnvDecayReleaseValue = decayReleaseSmartKnob.GetValueModeB();
	clawsSmartKnob.Update( 1.0 - hw.adc.GetFloat( clawsKnob ) );
	ampValue = clawsSmartKnob.GetValueModeA();
	ampEnvModAmountValue = clawsSmartKnob.GetValueModeB();

	// HANDLE THE SINGLE-MODE KNOBS
	driftKnobValue = 1.0 - hw.adc.GetFloat( driftKnob );
	shiftKnobValue = 1.0 - hw.adc.GetFloat( shiftKnob );
	driveKnobValue = 1.0 - hw.adc.GetFloat( driveKnob );
}
void handleSubOscWave(){
	// SUBWAVE HAS 6 MODES
	int subWave = subTypeValue * SUBOSC_WAVEFORMS_COUNT ;  // int range 0 - 2
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
    adcConfig[ growlKnob ].InitSingle( daisy::seed::A2 );
    adcConfig[ howlKnob ].InitSingle( daisy::seed::A3 );
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
	int combfilterBufferLength = sizeof( combFilterBuffer ) / sizeof( combFilterBuffer[ 0 ] );
	for( int i = 0; i < combfilterBufferLength; i++ ) combFilterBuffer[ i ] = 0.0;
	combFilter.Init( SAMPLE_RATE, combFilterBuffer, combfilterBufferLength );
	combFilter.SetPeriod( 2.0 ); // FOR NOW, THIS IS A FIXED VALUE = THE MAX BUFFER SIZE
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
		// IF THE OPERATING MODE CHANGED, CHANGE MODE ON THE SMART KNOBS
		if( operationMode != lastOperationMode ) {
			growlSmartKnob.SetMode( operationMode );
			howlSmartKnob.SetMode( operationMode );
			resSmartKnob.SetMode( operationMode );
			attackSmartKnob.SetMode( operationMode );
			sustainSmartKnob.SetMode( operationMode );
			decayReleaseSmartKnob.SetMode( operationMode );
			clawsSmartKnob.SetMode( operationMode );
		}
		lastOperationMode = operationMode;
		handleKnobs();
		
		// DO STUFF
		superSaw.SetDrift( driftKnobValue );
		superSaw.SetShift( shiftKnobValue );

		filter1.SetRes( fmap( resValue, 0.0, 0.85 ) );
		filter1.SetDrive( driveKnobValue );
		filter2.SetRes( fmap( resValue, 0.0, 0.85 ) );
		filter2.SetDrive( driveKnobValue );
		
		combFilter.SetFreq( fmap( cutoffValue, midiFreq, fclamp( midiFreq * 2.0, 20.0, 20000.0 ) ) );
		combFilter.SetRevTime( fmap( resValue, 0.0, 1.0 ) );

		pounce.SetAttackTime( fmap( pounceAttackValue, 0.005, 4.0 ) );
		pounce.SetSustainLevel( pounceSustainValue );
		pounce.SetDecayTime( fmap( pounceDecayReleaseValue, 0.005, 4.0 ) );
		pounce.SetReleaseTime( fmap( pounceDecayReleaseValue, 0.005, 4.0 ) );

		// TODO: GIVE AMPENV ITS OWN SETTINGS VIA ALT MODE
		// FOR NOW: SET AMPENV TO SAME SETTINGS AS POUNCE
		ampEnv.SetAttackTime( fmap( ampEnvAttackValue, 0.005, 4.0 ) );
		ampEnv.SetSustainLevel( ampEnvSustainValue );
		ampEnv.SetDecayTime( fmap( ampEnvDecayReleaseValue, 0.005, 4.0 ) );
		ampEnv.SetReleaseTime( fmap( ampEnvDecayReleaseValue, 0.005, 4.0 ) );

		handleSubOscWave();
		// LIGHT THE LED WHEN IN ALT MODE
		hw.SetLed( !operationMode );
		if( debugMode ){
			debugCount++;
			if( debugCount >= 500 ){	
				// REPORT ENVELOPE STATES
				hw.Print( "p: " );
				hw.Print( FLT_FMT3, FLT_VAR3( pounceAttackValue ) );
				hw.Print( " | " );
				hw.Print( FLT_FMT3, FLT_VAR3( pounceSustainValue ) );
				hw.Print( " | " );
				hw.Print( FLT_FMT3, FLT_VAR3( pounceDecayReleaseValue ) );
				hw.Print( ", e: " );
				hw.Print( FLT_FMT3, FLT_VAR3( ampEnvAttackValue ) );
				hw.Print( " | " );
				hw.Print( FLT_FMT3, FLT_VAR3( ampEnvSustainValue ) );
				hw.Print( " | " );
				hw.PrintLine( FLT_FMT3, FLT_VAR3( ampEnvDecayReleaseValue ) );
				debugCount = 0;
			}
		}
		System::Delay( 1 );
	}
}