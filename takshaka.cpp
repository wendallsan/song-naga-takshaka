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
	decayKnob,
	clawsKnob,
	slitherKnob,
	slitherDriftModKnob,
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
float driftValue,
	shiftValue,
	driveValue,
	midiFreq,
	slitherValue,
	slitherDriftModValue,
	combFilterBuffer[ 9600 ];
int subOscOctave = 1,
	midiNote = ROOT_MIDI_NOTE;
bool debugMode = false,
	operationMode = OP_MODE_NORMAL,
	lastOperationMode = OP_MODE_NORMAL,
	envGate = false;
DaisySeed hw;
MidiUsbHandler midi;
Switch modeSwitch;
SmartKnob subMixSmartKnob,
	subTypeSmartKnob,
	filterCutoffSmartKnob,
	filterTypeSmartKnob,
	filterResSmartKnob,
	filterPolesSmartKnob,	
	pounceAttackSmartKnob,
	ampEnvAttackSmartKnob,
	pounceSustainSmartKnob,
	ampEnvSustainSmartKnob,
	pounceDecaySmartKnob,
	ampEnvDecaySmartKnob,
	ampSmartKnob, 
	ampEnvModSmartKnob;
SuperSawOsc superSaw;
Oscillator subOsc, lfo;
Svf filter1, filter2;
Comb combFilter;
Overdrive distortion;
Adsr pounce, ampEnv;
void AudioCallback( AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size ){
	for( size_t i = 0; i < size; i++ ){
		float pounceValue = pounce.Process( envGate );
		float slitherValue = lfo.Process();
		// MUX: float modDriftValue = driftValue + ( pounceValue * pounceDriftModValue );
		float modDriftValue = driftValue;
		// DISABLED FOR PRE-MUX TESTING: THIS IS THE ACTUAL CORRECT SETTING, BUT WE'RE USING THIS
		// KNOB VALUE FOR TESTING OTHER FEATURES CURRENTLY.
		// modDriftValue += slitherValue * 
		// 	( ( slitherValue * slitherDriftModValue ) - ( slitherDriftModValue / 2.0 ) );
		superSaw.SetDrift( fclamp( modDriftValue, 0.0, 1.0 ) );
		// MUX: float modShiftValue = shiftValue + ( pounceValue * pounceShiftModValue );
		float modShiftValue = shiftValue;
		// MUX: modShiftValue += slitherValue * 
		//  	( ( slitherValue * slitherShiftModValue ) - ( slitherShiftModValue / 2.0 ) );
		superSaw.SetShift( fclamp( modShiftValue, 0.0, 1.0 ) );
		// SET ADJUST TO 1.0 - 0.8 DEPENDING ON THE SUB KNOB
		float superSawAdjust = fmap( 1.0 - subMixSmartKnob.GetValue(), 0.8, 1.0 );
		float mixedSignal = superSaw.Process() * superSawAdjust;
		float subSignal = subOsc.Process();
		mixedSignal += subSignal * subMixSmartKnob.GetValue();
		mixedSignal = distortion.Process( mixedSignal );
		// MUX: float cutoffMod = pounceValue * pounceHowlModValue;
		float cutoffMod = pounce.Process( envGate ) * 0.5;
		// MUX: cutoffMod += slitherValue * slitherHowlModValue;
		cutoffMod += filterCutoffSmartKnob.GetValue();
		float filterFreq = fmap( cutoffMod, 1.0, fclamp( midiFreq * 16.0, 20.0, 20000.0 ) );
		filter1.SetFreq( filterFreq );
		filter2.SetFreq( filterFreq );
		combFilter.SetFreq( filterFreq );
		float filteredSignal = 0.0;
		filter1.Process( mixedSignal );
		float combSignal = combFilter.Process( mixedSignal );
		int currentFilterMode = filterTypeSmartKnob.GetValue() * FILTER_MODES_COUNT;
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
		if( filterPolesSmartKnob.GetValue() > 0.5 &&
			(
				currentFilterMode == FILTER_MODE_LP ||
				currentFilterMode == FILTER_MODE_HP ||
				currentFilterMode == FILTER_MODE_BP
			)
		){ // PASS SIGNAL THROUGH SECOND FILTER
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
		float ampMod = ampEnv.Process( envGate ) * ampEnvModSmartKnob.GetValue();
		// MUX: ampMod += slitherValue *
		//	( ( slitherValue * slitherDriftModValue ) - ( slitherDriftModValue / 2.0 ) );
		ampMod = fclamp( ampMod + ampSmartKnob.GetValue(), 0.0, 1.0 );
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
	float growlValue = 1.0 - hw.adc.GetFloat( growlKnob );
	subMixSmartKnob.Update( growlValue );
	subTypeSmartKnob.Update( growlValue );
	float howlKnobValue = 1.0 - hw.adc.GetFloat( howlKnob );
	filterCutoffSmartKnob.Update( howlKnobValue );
	filterTypeSmartKnob.Update( howlKnobValue );
	float resKnobValue = 1.0 - hw.adc.GetFloat( resKnob );
	filterResSmartKnob.Update( resKnobValue );
	filterPolesSmartKnob.Update( resKnobValue );
	float attackKnobValue = 1.0 - hw.adc.GetFloat( attackKnob );
	pounceAttackSmartKnob.Update( attackKnobValue );
	ampEnvAttackSmartKnob.Update( attackKnobValue );
	float sustainKnobValue = 1.0 - hw.adc.GetFloat( sustainKnob );
	pounceSustainSmartKnob.Update( sustainKnobValue );
	ampEnvSustainSmartKnob.Update( sustainKnobValue );
	float decayKnobValue = 1.0 - hw.adc.GetFloat( decayKnob );
	pounceDecaySmartKnob.Update( decayKnobValue );
	ampEnvDecaySmartKnob.Update( decayKnobValue );
	float clawsKnobValue = 1.0 - hw.adc.GetFloat( clawsKnob );
	ampSmartKnob.Update( clawsKnobValue );
	ampEnvModSmartKnob.Update( clawsKnobValue );
	driftValue = 1.0 - hw.adc.GetFloat( driftKnob );
	shiftValue = 1.0 - hw.adc.GetFloat( shiftKnob );
	driveValue = 1.0 - hw.adc.GetFloat( driveKnob );
	slitherValue = 1.0 - hw.adc.GetFloat( slitherKnob );
	slitherDriftModValue = 1.0 - hw.adc.GetFloat( slitherDriftModKnob );
}
void updateSubOscWave(){
	int subWave = subTypeSmartKnob.GetValue() * SUBOSC_WAVEFORMS_COUNT ;  // int range 0 - 2
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
    adcConfig[ decayKnob ].InitSingle( daisy::seed::A8 );
    adcConfig[ clawsKnob ].InitSingle( daisy::seed::A9 );
    adcConfig[ slitherKnob ].InitSingle( daisy::seed::A10 );
    adcConfig[ slitherDriftModKnob ].InitSingle( daisy::seed::A11 );
	hw.adc.Init( adcConfig, ADC_CHANNELS_COUNT );
    hw.adc.Start();
}
void handleSmartKnobSwitching(){
	if( operationMode ){
		subMixSmartKnob.Activate();
		subTypeSmartKnob.Deactivate();
		filterCutoffSmartKnob.Activate();
		filterTypeSmartKnob.Deactivate();
		filterResSmartKnob.Activate();
		filterPolesSmartKnob.Deactivate();				
		pounceAttackSmartKnob.Activate();
		ampEnvAttackSmartKnob.Deactivate();
		pounceSustainSmartKnob.Activate();
		ampEnvSustainSmartKnob.Deactivate();
		pounceDecaySmartKnob.Activate();
		ampEnvDecaySmartKnob.Deactivate();
		ampSmartKnob.Activate();
		ampEnvModSmartKnob.Deactivate();
	} else {
		subMixSmartKnob.Deactivate();
		subTypeSmartKnob.Activate();
		filterCutoffSmartKnob.Deactivate();
		filterTypeSmartKnob.Activate();
		filterResSmartKnob.Deactivate();
		filterPolesSmartKnob.Activate();				
		pounceAttackSmartKnob.Deactivate();
		ampEnvAttackSmartKnob.Activate();
		pounceSustainSmartKnob.Deactivate();
		ampEnvSustainSmartKnob.Activate();
		pounceDecaySmartKnob.Deactivate();
		ampEnvDecaySmartKnob.Activate();
		ampSmartKnob.Deactivate();
		ampEnvModSmartKnob.Activate();
	}
}
void updateSuperSaw(){
	superSaw.SetFreq( midiFreq );
	superSaw.SetShift( shiftValue );
}
void updateSubOsc(){
	subOsc.SetFreq( midiFreq / ( subOscOctave + 1 ) );
}
void updateDistortion(){
	distortion.SetDrive( fmap( driveValue, 0.25, 0.9 ) );
}
void updateFilters(){
	float resValue = fmap( filterResSmartKnob.GetValue(), 0.0, 0.85 );
	filter1.SetRes( resValue );
	filter2.SetRes( resValue );	
	combFilter.SetRevTime( fmap( filterResSmartKnob.GetValue(), 0.0, 1.0 ) );
}
void updatePounce(){
	pounce.SetAttackTime( fmap( pounceAttackSmartKnob.GetValue(), 0.005, 4.0 ) );
	pounce.SetSustainLevel( pounceSustainSmartKnob.GetValue() );
	pounce.SetDecayTime( fmap( pounceDecaySmartKnob.GetValue(), 0.005, 4.0 ) );
	pounce.SetReleaseTime( fmap( pounceDecaySmartKnob.GetValue(), 0.005, 4.0 ) );
}
void updateAmpEnv(){
	ampEnv.SetAttackTime( fmap( ampEnvAttackSmartKnob.GetValue(), 0.005, 4.0 ) );
	ampEnv.SetSustainLevel( ampEnvSustainSmartKnob.GetValue() );
	ampEnv.SetDecayTime( fmap( ampEnvDecaySmartKnob.GetValue(), 0.005, 4.0 ) );
	ampEnv.SetReleaseTime( fmap( ampEnvDecaySmartKnob.GetValue(), 0.005, 4.0 ) );
}
int main(){
	hw.Init();
	if( !debugMode ){		
		MidiUsbHandler::Config midiConfig;
		midiConfig.transport_config.periph = MidiUsbTransport::Config::INTERNAL;
		midi.Init( midiConfig );
	} else hw.StartLog();
	// INIT THE SMART KNOBS
	subMixSmartKnob.Init( true, 0.0 );
	subTypeSmartKnob.Init( false, 0.0 );
	filterCutoffSmartKnob.Init( true, 0.5 );
	filterTypeSmartKnob.Init( false, 0.0 );
	filterResSmartKnob.Init( true, 0.0 );
	filterPolesSmartKnob.Init( false, 0.0 );
	pounceAttackSmartKnob.Init( true, 0.0 );
	ampEnvAttackSmartKnob.Init( false, 0.0 );
	pounceSustainSmartKnob.Init( true, 1.0 );
	ampEnvSustainSmartKnob.Init( false, 1.0 );
	pounceDecaySmartKnob.Init( true,  0.2 );
	ampEnvDecaySmartKnob.Init( false, 0.2 );
	ampSmartKnob.Init( true, 0.5 );
	ampEnvModSmartKnob.Init( false, 0.5 );
	// INIT DAISY SP OBJECTS
	superSaw.Init( SAMPLE_RATE );
	subOsc.Init( SAMPLE_RATE );
	distortion.Init();  // SETS GAIN TO ZERO
	filter1.Init( SAMPLE_RATE );
	filter1.SetDrive( 0.0 );
	filter2.Init( SAMPLE_RATE );
	filter2.SetDrive( 0.0 );
	int combfilterBufferLength = sizeof( combFilterBuffer ) / sizeof( combFilterBuffer[ 0 ] );
	for( int i = 0; i < combfilterBufferLength; i++ ) combFilterBuffer[ i ] = 0.0;
	combFilter.Init( SAMPLE_RATE, combFilterBuffer, combfilterBufferLength );
	combFilter.SetPeriod( 2.0 ); // FOR NOW, THIS IS A FIXED VALUE = THE MAX BUFFER SIZE
	pounce.Init( SAMPLE_RATE );
	ampEnv.Init( SAMPLE_RATE );
	lfo.Init( SAMPLE_RATE );
	lfo.SetWaveform( lfo.WAVE_SIN );
	initADC();
	modeSwitch.Init( hw.GetPin( 14 ), 100 );
	hw.SetAudioSampleRate( SaiHandle::Config::SampleRate::SAI_48KHZ );
	hw.StartAudio( AudioCallback );
	int debugCount = 0;
	while( true ){
		if( !debugMode ) handleMidi();
		else midiNote = ROOT_MIDI_NOTE;
		midiFreq = mtof( midiNote );
		modeSwitch.Debounce();
		operationMode = !modeSwitch.Pressed();		
		// IF THE OPERATING MODE CHANGED, CHANGE MODE ON THE SMART KNOBS
		if( operationMode != lastOperationMode ) handleSmartKnobSwitching();
		lastOperationMode = operationMode;
		handleKnobs();
		lfo.SetFreq( fmap( slitherValue, 0.05, 20.0 ) );
		updateSuperSaw();
		updateSubOsc();
		updateDistortion();
		updateFilters();
		updatePounce();
		updateAmpEnv();
		updateSubOscWave();
		hw.SetLed( !operationMode ); // LIGHT THE LED WHEN IN ALT MODE
		if( debugMode ){
			debugCount++;
			if( debugCount >= 500 ){	
				// REPORT STUFF
				hw.Print( FLT_FMT3, FLT_VAR3( slitherValue ) );
				hw.Print( " | " );
				hw.PrintLine( FLT_FMT3, FLT_VAR3( slitherDriftModValue ) );
				debugCount = 0;
			}
		}
		System::Delay( 1 );
	}
}