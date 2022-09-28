#include "daisy_seed.h"
#include "daisysp.h"
#include "SuperSawOsc.h"
#include "SmartKnob.h"
#define ROOT_MIDI_NOTE 48
#define SAMPLE_RATE 48000.0
#define MAX_DELAY 96000
#define NUM_VOICES 4

/*
FANGS TIME:
disable the drive knob, slither knob, and slither drift mod knob
so we can repopose them to:
Fangs Time
Fangs Amount
Fangs Mix
*/

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
enum lfoWaveforms {
	SINE,
	TRI,
	SAW,
	RAMP,
	SQUARE,
	RANDOM,
	LFO_WAVEFORMS_COUNT
};
enum FilterModes {
	FILTER_MODE_LP,
	FILTER_MODE_HP,
	FILTER_MODE_BP,
	FILTER_MODE_COMB,
	FILTER_MODES_COUNT
};
enum EffectModes {
	EFFECT_MODE_ECHO,
	EFFECT_MODE_REVERB,
	EFFECT_MODE_CHORUS,
	EFFECT_MODE_FLANGER,
	EFFECT_MODES_COUNT
};
enum operationModes { OP_MODE_ALT, OP_MODE_NORMAL };
float driftValue,
	shiftValue,
	driveValue,
	midiFreqs[ NUM_VOICES ],
	slitherValue,
	slitherDriftModValue,
	combFilterBuffers[ NUM_VOICES ][ 9600 ],
	fangsTimeCurrentValue,
	fangsMixValue,
	delayTime;
int subOscOctave = 1,
	fangsEffectType = EFFECT_MODE_ECHO,
	midiNotes[ NUM_VOICES ] = { ROOT_MIDI_NOTE, ROOT_MIDI_NOTE, ROOT_MIDI_NOTE, ROOT_MIDI_NOTE },
	currentFilterMode = FILTER_MODE_LP,
	nextVoice = 0; 
bool debugMode = true,
	operationMode = OP_MODE_NORMAL,
	lastOperationMode = OP_MODE_NORMAL,
	envGates[ NUM_VOICES ] = { false, false, false, false };
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
	ampEnvModSmartKnob,
	lfoFrequencySmartKnob,
	lfoTypeSmartKnob,
	fangsTimeSmartKnob,
	fangsEffectTypeSmartKnob,
	fangsEffectTimeSmartKnob,
	fangsAmount1SmartKnob,
	fangsAmount2SmartKnob;
SuperSawOsc superSaws[ NUM_VOICES ];
Oscillator subOscs[ NUM_VOICES ], lfo;
Svf filters1[ NUM_VOICES ], filters2[ NUM_VOICES ];
Comb combFilters[ NUM_VOICES ];
Overdrive distortions[ NUM_VOICES ];
Adsr pounces[ NUM_VOICES ], ampEnvs[ NUM_VOICES ];
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayLine;
static ReverbSc DSY_SDRAM_BSS reverb;
Chorus chorus;
Flanger flanger;
float getFiltered1Signal( int i, float combSignal ){	
	float output = 0.0;
	switch( currentFilterMode ){
		case FILTER_MODE_LP:
			output = filters1[ i ].Low();
			break;
		case FILTER_MODE_HP:
			output = filters1[ i ].High();
			break;
		case FILTER_MODE_BP:
			output = filters1[ i ].Band();
			break;
		case FILTER_MODE_COMB:
			output = combSignal;
			break;
	}
	return output;
}
float getFiltered2Signal( int i, float input ){ // PASS SIGNAL THROUGH SECOND FILTER
	float output = 0.f;
	filters2[ i ].Process( input );
	switch( currentFilterMode ){
		case FILTER_MODE_LP:
			output = filters2[ i ].Low();
			break;
		case FILTER_MODE_HP:
			output = filters2[ i ].High();
			break;
		case FILTER_MODE_BP:
			output = filters2[ i ].Band();
			break; 
	}
	return output;
}
void AudioCallback( AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size ){
	float reverbSignal;
	for( size_t i = 0; i < size; i++ ){
		float finalSignal = 0.0;
		// MUX: float slitherValue = lfo.Process();
		float superSawMixMod = fmap( 1.0 - subMixSmartKnob.GetValue(), 0.8, 1.0 );
		for( int j = 0; j < NUM_VOICES; j++ ){
			float pounceValue = pounces[ j ].Process( envGates[ j ] );	
			float modDriftValue = driftValue;
			// MUX: modDriftValue += pounceValue * pounceDriftModValue;
			// DISABLED FOR PRE-MUX TESTING: THIS IS THE ACTUAL CORRECT SETTING, BUT WE'RE USING THIS
			// KNOB VALUE FOR TESTING OTHER FEATURES CURRENTLY.
			// modDriftValue += slitherValue * ( ( slitherValue * slitherDriftModValue ) - ( slitherDriftModValue / 2.0 ) );
			float modShiftValue = shiftValue;
			// MUX: modShiftValue += slitherValue * ( ( slitherValue * slitherShiftModValue ) - ( slitherShiftModValue / 2.0 ) );	
			// MUX: modShiftValue +=  pounceValue * pounceShiftModValue;
			superSaws[ j ].SetDrift( fclamp( modDriftValue, 0.0, 0.999 ) );
			superSaws[ j ].SetShift( fclamp( modShiftValue, 0.0, 0.999 ) );
			// SET MIX KNOB TO 1.0 - 0.8 DEPENDING ON THE SUB KNOB
			float mixedSignal = superSaws[ j ].Process() * superSawMixMod;
			float subSignal = subOscs[ j ].Process();
			mixedSignal += subSignal * subMixSmartKnob.GetValue();
			// TODO: DISTORTION ALWAYS SEEM TO AFFECT THE WAVESHAPE, EVEN WHEN IT
			// HAS GAIN == 0.  RATHER THAN LEAVING IT ALWAYS IN THE SIGNAL CHAIN,
			// MAYBE WE NEED TO MIX IT WITH THE CLEAN SIGNAL, AT LEAST AT LOW 
			// DRIVE SETTINGS?
			// FANGS: mixedSignal = distortion.Process( mixedSignal );
			// MUX: float cutoffMod = pounceValue * pounceHowlModValue;
			// FOR NOW WE SET MOD TO A FIXED VALUE
			float cutoffMod = pounceValue * 0.5; 
			// MUX: cutoffMod += slitherValue * slitherHowlModValue;
			cutoffMod += filterCutoffSmartKnob.GetValue();
			float filterFreq = fmap( cutoffMod, 1.0, fclamp( midiFreqs[ j ] * 16.0, 20.0, 20000.0 ) );
			filters1[ j ].SetFreq( filterFreq );
			filters2[ j ].SetFreq( filterFreq );
			combFilters[ j ].SetFreq( fclamp( filterFreq, 20.0, 10000.0 ) );
			filters1[ j ].Process( mixedSignal );			
			float combSignal = currentFilterMode == FILTER_MODE_COMB? 
				combFilters[ j ].Process( mixedSignal ) : 0.f;
			float filteredSignal = getFiltered1Signal( j, combSignal );
			if( 
				filterPolesSmartKnob.GetValue() > 0.5 &&
				(
					currentFilterMode == FILTER_MODE_LP ||
					currentFilterMode == FILTER_MODE_HP ||
					currentFilterMode == FILTER_MODE_BP
				)
			) filteredSignal = getFiltered2Signal( j, filteredSignal );
			float ampMod = ampEnvs[ j ].Process( envGates[ j ] ) * ampEnvModSmartKnob.GetValue();
			// MUX: ampMod += slitherValue * ( ( slitherValue * slitherDriftModValue ) - ( slitherDriftModValue / 2.0 ) );
			ampMod = fclamp( ampMod + ampSmartKnob.GetValue(), 0.0, 1.0 );
			float finalVoice = filteredSignal * ampMod;	
			finalSignal += finalVoice;
		}
		fonepole( fangsTimeCurrentValue, fangsTimeSmartKnob.GetValue(), 0.0002f );
		if( fangsEffectType == EFFECT_MODE_ECHO ){
			delayLine.SetDelay( fangsTimeCurrentValue * MAX_DELAY );
			float delaySignal = delayLine.Read(); // READ FROM THE DELAY LINE
			delayLine.Write(  // WRITE TO THE DELAY LINE
				( finalSignal * ( 1.0 - fangsAmount1SmartKnob.GetValue() ) ) + ( delaySignal * fangsAmount1SmartKnob.GetValue() )
			);
			finalSignal = ( finalSignal * (1.0 - fangsMixValue ) ) + ( delaySignal * fangsMixValue );
		} else if( fangsEffectType == EFFECT_MODE_REVERB ) {
			reverb.Process( finalSignal, 0.f, &reverbSignal, 0 );
			finalSignal = ( finalSignal * ( 1.0 - fangsMixValue ) ) + ( reverbSignal * fangsMixValue );
		} else if( fangsEffectType == EFFECT_MODE_CHORUS ){
			chorus.Process( finalSignal );
			finalSignal = ( finalSignal * ( 1.0 - fangsMixValue ) ) + ( chorus.GetLeft() * fangsMixValue );
		} else if( fangsEffectType == EFFECT_MODE_FLANGER){
			float flangerSignal = flanger.Process( finalSignal );
			finalSignal = ( finalSignal * ( 1.0 - fangsMixValue ) ) + ( flangerSignal * fangsMixValue );
		}
		out[0][i] = out[1][i] = finalSignal;
	}
}
void handleMidi(){
	midi.Listen();
	while( midi.HasEvents() ){
		auto midiEvent = midi.PopEvent();
		auto noteMessage = midiEvent.AsNoteOn();
		if( midiEvent.type == NoteOn && noteMessage.velocity != 0 ){
			nextVoice = ( nextVoice + 1 ) % NUM_VOICES;
			midiNotes[ nextVoice ] = noteMessage.note;
			envGates[ nextVoice ] = true;
		} else if( midiEvent.type == NoteOff ) {
			for( int i = 0; i < NUM_VOICES; i++ ){
				if( midiNotes[ i ] == noteMessage.note ){
					midiNotes[ i ] = 0;
					envGates[ i ] = false;
				}
			}
		}
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
	float fangsTimeValue = 1.0 - hw.adc.GetFloat( driveKnob );
	fangsTimeSmartKnob.Update( fangsTimeValue );
	fangsEffectTypeSmartKnob.Update( fmap( fangsTimeValue, 0.f, 0.99f ) );
	float fangsAmountValue = 1.0 - hw.adc.GetFloat( slitherKnob );
	fangsAmount1SmartKnob.Update( fangsAmountValue );
	fangsAmount2SmartKnob.Update( fangsAmountValue );	
	float clawsKnobValue = 1.0 - hw.adc.GetFloat( clawsKnob );
	ampSmartKnob.Update( clawsKnobValue );
	ampEnvModSmartKnob.Update( clawsKnobValue );
	// MUX: slitherValue = 1.0 - hw.adc.GetFloat( slitherKnob );
	// MUX: lfoFrequencySmartKnob.Update( slitherValue );
	// MUX: lfoTypeSmartKnob.Update( slitherValue );

	driftValue = 1.0 - hw.adc.GetFloat( driftKnob );
	shiftValue = 1.0 - hw.adc.GetFloat( shiftKnob );
	// FANGS: driveValue = 1.0 - hw.adc.GetFloat( driveKnob );
	// FANGS: slitherDriftModValue = 1.0 - hw.adc.GetFloat( slitherDriftModKnob );
	fangsMixValue = 1.0 - hw.adc.GetFloat( slitherDriftModKnob );
}
void updateLfoWave(){
	int lfoWave = lfoTypeSmartKnob.GetValue() * LFO_WAVEFORMS_COUNT ;  // int range 0 - 2
	switch( lfoWave ){
		case 0:
			lfo.SetWaveform( lfo.WAVE_SIN );
			break;
		case 1:
			lfo.SetWaveform( lfo.WAVE_TRI );
			break;
		case 2:
			lfo.SetWaveform( lfo.WAVE_SAW );
			break;
		case 3:
			lfo.SetWaveform( lfo.WAVE_RAMP );
			break;
		case 4:
			lfo.SetWaveform( lfo.WAVE_SQUARE );
			break;
		case 5:
			// TODO: IMPLEMENT RANDOM
			lfo.SetWaveform( lfo.WAVE_SIN );
			break;
	}
}
void updateSubOscWave(){
	int subWave = subTypeSmartKnob.GetValue() * SUBOSC_WAVEFORMS_COUNT ;  // int range 0 - 2
	switch( subWave ){
		case 0:
		for( int i = 0; i < NUM_VOICES; i++ ) subOscs[ i ].SetWaveform( subOscs[ i ].WAVE_SIN );
			subOscOctave = 1;
			break;
		case 1:
			for( int i = 0; i < NUM_VOICES; i++ ) subOscs[ i ].SetWaveform( subOscs[ i ].WAVE_SIN );
			subOscOctave = 2;
			break;
		case 2:
			for( int i = 0; i < NUM_VOICES; i++ ) subOscs[ i ].SetWaveform( subOscs[ i ].WAVE_POLYBLEP_TRI );
			subOscOctave = 1;
			break;
		case 3:
			for( int i = 0; i < NUM_VOICES; i++ ) subOscs[ i ].SetWaveform( subOscs[ i ].WAVE_POLYBLEP_TRI );
			subOscOctave = 2;
			break;
		case 4:
			for( int i = 0; i < NUM_VOICES; i++ ) subOscs[ i ].SetWaveform( subOscs[ i ].WAVE_POLYBLEP_SQUARE );
			subOscOctave = 1;
			break;
		case 5:
			for( int i = 0; i < NUM_VOICES; i++ ) subOscs[ i ].SetWaveform( subOscs[ i ].WAVE_POLYBLEP_SQUARE );
			subOscOctave = 2;
	}
}
void initAdc(){
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
		lfoFrequencySmartKnob.Activate();
		lfoTypeSmartKnob.Deactivate();
		fangsTimeSmartKnob.Activate();
		fangsEffectTypeSmartKnob.Deactivate();
		fangsAmount1SmartKnob.Activate();
		fangsAmount2SmartKnob.Deactivate();
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
		lfoFrequencySmartKnob.Deactivate();
		lfoTypeSmartKnob.Activate();
		fangsTimeSmartKnob.Deactivate();
		fangsEffectTypeSmartKnob.Activate();
		fangsAmount1SmartKnob.Deactivate();
		fangsAmount2SmartKnob.Activate();
	}
}
void updateSuperSaw(){
	for( int i = 0; i < NUM_VOICES; i++ ) {
		superSaws[ i ].SetFreq( midiFreqs[ i ] );
		superSaws[ i ].SetShift( shiftValue );
	}
}
void updateSubOsc(){
	updateSubOscWave();
	for( int i = 0; i < NUM_VOICES; i++ ) subOscs[ i ].SetFreq( midiFreqs[ i ] / ( subOscOctave + 1 ) );
}
void updateDistortion(){
	// FANGS: distortion.SetDrive( fmap( driveValue, 0.25, 0.9 ) );
}
void updateFilters(){
	currentFilterMode = filterTypeSmartKnob.GetValue() * FILTER_MODES_COUNT;
	float resValue = fmap( filterResSmartKnob.GetValue(), 0.0, 0.85 );	
	for( int i = 0; i < NUM_VOICES; i++ ){
		filters1[ i ].SetRes( resValue );
		filters2[ i ].SetRes( resValue );
		combFilters[ i ].SetRevTime( fmap( filterResSmartKnob.GetValue(), 0.0, 1.0 ) );
	}
}
void updatePounce(){
	for( int i = 0; i < NUM_VOICES; i++ ){
		pounces[ i ].SetAttackTime( fmap( pounceAttackSmartKnob.GetValue(), 0.005, 4.0 ) );
		pounces[ i ].SetSustainLevel( pounceSustainSmartKnob.GetValue() );
		pounces[ i ].SetDecayTime( fmap( pounceDecaySmartKnob.GetValue(), 0.005, 4.0 ) );
		pounces[ i ].SetReleaseTime( fmap( pounceDecaySmartKnob.GetValue(), 0.005, 4.0 ) );
	}
}
void updateAmpEnv(){
	for( int i = 0; i < NUM_VOICES; i++ ){
		ampEnvs[ i ].SetAttackTime( fmap( ampEnvAttackSmartKnob.GetValue(), 0.005, 4.0 ) );
		ampEnvs[ i ].SetSustainLevel( ampEnvSustainSmartKnob.GetValue() );
		ampEnvs[ i ].SetDecayTime( fmap( ampEnvDecaySmartKnob.GetValue(), 0.005, 4.0 ) );
		ampEnvs[ i ].SetReleaseTime( fmap( ampEnvDecaySmartKnob.GetValue(), 0.005, 4.0 ) );
	}
}
void initSmartKnobs(){
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
	ampEnvModSmartKnob.Init( false, 0.8 );
	lfoFrequencySmartKnob.Init( true, 0.5 );
	lfoTypeSmartKnob.Init( false, 0.0 );
	fangsTimeSmartKnob.Init( true, 0.5 );
	fangsEffectTypeSmartKnob.Init( false, 0.0 );
	fangsAmount1SmartKnob.Init( true, 0.2 );
	fangsAmount2SmartKnob.Init( false, 0.2 );
}
void initDsp(){
	for( int i = 0; i < NUM_VOICES; i++ ){
		superSaws[ i ].Init( SAMPLE_RATE );
		subOscs[ i ].Init( SAMPLE_RATE );
		distortions[ i ].Init();  // SETS GAIN TO ZERO	
		filters1[ i ].Init( SAMPLE_RATE );
		filters1[ i ].SetDrive( 0.0 );
		filters2[ i ].Init( SAMPLE_RATE );
		filters2[ i ].SetDrive( 0.0 );
		int combfilterBufferLength = sizeof( combFilterBuffers[ i ] ) / sizeof( combFilterBuffers[ i ][ 0 ] );
		for( int j = 0; j < combfilterBufferLength; j++ ) combFilterBuffers[ i ][ j ] = 0.0;
		combFilters[ i ].Init( SAMPLE_RATE, combFilterBuffers[ i ], combfilterBufferLength );
		combFilters[ i ].SetPeriod( 2.0 ); // FOR NOW, THIS IS A FIXED VALUE = THE MAX BUFFER SIZE
		pounces[ i ].Init( SAMPLE_RATE );
		ampEnvs[ i ].Init( SAMPLE_RATE );
	}	
	lfo.Init( SAMPLE_RATE );
	lfo.SetWaveform( lfo.WAVE_SIN );
	delayLine.Init();
	reverb.Init( SAMPLE_RATE );
	chorus.Init( SAMPLE_RATE );
	flanger.Init( SAMPLE_RATE );
}
void updateLfo(){
	updateLfoWave();
	// FANGS: lfo.SetFreq( fmap( lfoFrequencySmartKnob.GetValue(), 0.05, 20.0 ) );	
}
void updateFangs(){
	fangsEffectType = fangsEffectTypeSmartKnob.GetValue() * EFFECT_MODES_COUNT;
	reverb.SetFeedback( fangsAmount1SmartKnob.GetValue() );
	reverb.SetLpFreq( fmap( fangsTimeSmartKnob.GetValue(), 70.f, 18000.f ) );
	chorus.SetDelay( fangsTimeSmartKnob.GetValue(), fangsTimeSmartKnob.GetValue() ); // TODO: 2 ARGS R 4 STEREO OUTS
	chorus.SetLfoDepth( fangsAmount1SmartKnob.GetValue() );
	chorus.SetLfoFreq( fmap( fangsAmount2SmartKnob.GetValue(), 0.01f, 5.f ), 0.2f ); // TODO: 2 ARGS R 4 STEREO OUTS
	flanger.SetLfoFreq( fmap( fangsTimeSmartKnob.GetValue(), 0.01f, 5.f ) );
    flanger.SetFeedback( fangsAmount1SmartKnob.GetValue() );
	flanger.SetLfoDepth( fmap( fangsAmount2SmartKnob.GetValue(), 0.01f, 5.f ) );
}
int main(){
	hw.Init();
	if( !debugMode ){
		MidiUsbHandler::Config midiConfig;
		midiConfig.transport_config.periph = MidiUsbTransport::Config::INTERNAL;
		midi.Init( midiConfig );
	} else hw.StartLog();
	initSmartKnobs();
	initDsp();
	initAdc();
	modeSwitch.Init( hw.GetPin( 14 ), 100 );
	hw.SetAudioSampleRate( SaiHandle::Config::SampleRate::SAI_48KHZ );
	hw.StartAudio( AudioCallback );
	int debugCount = 0;
	while( true ){
		if( !debugMode ) handleMidi();
		else{
			int sevenChord[ 4 ] = { 0, 4, 6, 10 };
			for( int i = 0; i < NUM_VOICES; i++ ) midiNotes[ i ] = ROOT_MIDI_NOTE + sevenChord[ i ];
		}
		for( int i = 0; i < NUM_VOICES; i++ ) midiFreqs[ i ] = mtof( midiNotes[ i ] );
		modeSwitch.Debounce();
		operationMode = !modeSwitch.Pressed();
		// IF THE OPERATING MODE CHANGED, CHANGE MODE ON THE SMART KNOBS
		if( operationMode != lastOperationMode ) handleSmartKnobSwitching();
		lastOperationMode = operationMode;
		handleKnobs();
		updateLfo();
		updateSuperSaw();
		updateSubOsc();
		updateDistortion();
		updateFilters();
		updatePounce();
		updateAmpEnv();
		updateFangs();
		bool midiNotesOn = false;
		for( int i = 0; i < NUM_VOICES; i++ ) if( envGates[ i ] ) midiNotesOn = true;
		hw.SetLed( midiNotesOn );
		if( debugMode ){
			debugCount++;
			if( debugCount >= 500 ){
				// REPORT STUFF
				debugCount = 0;
			}
		}
		System::Delay( 1 );
	}
}