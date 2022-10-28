#include "daisy_seed.h"
#include "daisysp.h"
#include "SmartKnob.h"
#include "BlockSuperSawOsc.h"
#include "BlockOscillator.h"
#include "BlockSvf.h"
#include "BlockComb.h"
// #include "BlockReverbSc.h"
// #include "BlockChorus.h"
// #include "BlockFlanger.h"
#define ROOT_MIDI_NOTE 48
#define SAMPLE_RATE 48000.f
#define MAX_DELAY 96000
#define NUM_VOICES 4
#define MUX_CHANNELS_COUNT 8
#define PIN_MUX_CH_A 12
#define PIN_MUX_CH_B 13
#define PIN_MUX_CH_C 14
#define PIN_ADC_MUX1_IN daisy::seed::A0
#define PIN_ADC_MUX2_IN daisy::seed::A1
#define PIN_SC_MOD_IN daisy::seed::A2
#define PIN_FT_IN daisy::seed::A3
#define PIN_FA_IN daisy::seed::A4
#define PIN_FM_IN daisy::seed::A5
#define PIN_CLAWS_IN daisy::seed::A6
#define MODE_SWITCH_PIN 11

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
	ADC_MUX1,
	ADC_MUX2,
	ADC_SC_MOD,
	ADC_FT,
	ADC_FA,
	ADC_FM,
	ADC_CLAWS,
	ADC_CHANNELS_COUNT
};
enum mux1Signals {
	MUX1_DRIFT,
	MUX1_SHIFT,
	MUX1_GROWL,
	MUX1_HOWL,
	MUX1_RES,
	MUX1_DRIVE,
	MUX1_ATTACK,
	MUX1_SUSTAIN
}; 
enum mux2Signals{
	MUX2_DECAY,
	MUX2_PD_MOD,
	MUX2_PS_MOD,
	MUX2_PH_MOD,
	MUX2_SLITHER,
	MUX2_SD_MOD,
	MUX2_SS_MOD,
	MUX2_SH_MOD
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
	slitherShiftModValue,
	slitherHowlModValue,
	slitherClawsModValue,
	combFilterBuffers[ NUM_VOICES ][ 9600 ],
	fangsTimeCurrentValue,
	fangsMixValue,
	pounceDriftModValue,
	pounceShiftModValue,
	pounceHowlModValue; 
int subOscOctave = 1,
	fangsEffectType = EFFECT_MODE_ECHO,
	midiNotes[ NUM_VOICES ] = { ROOT_MIDI_NOTE, ROOT_MIDI_NOTE, ROOT_MIDI_NOTE, ROOT_MIDI_NOTE },
	currentFilterMode = FILTER_MODE_LP,
	nextVoice = 0;
bool debugMode = false,
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
	fangsAdjust1SmartKnob,
	fangsAdjust2SmartKnob;
BlockSuperSawOsc superSaws[ NUM_VOICES ];
BlockOscillator subOscs[ NUM_VOICES ];
Oscillator lfo;
BlockSvf filters1[ NUM_VOICES ], filters2[ NUM_VOICES ];
BlockComb combFilters[ NUM_VOICES ];
Adsr pounces[ NUM_VOICES ], ampEnvs[ NUM_VOICES ];
// TODO: CAN WE BLOCK THE DELAY LINE?
// static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayLine;
// static BlockReverbSc DSY_SDRAM_BSS reverb;
// BlockChorus chorus;
// Flanger flanger;

static Metro clock;
const size_t sampleBlockSize = 16;

void filterSignal( int i, float *buff, size_t size ){
	if( currentFilterMode == FILTER_MODE_COMB ) combFilters[ i ].Process( buff, size );
	else {
		switch( currentFilterMode ){
			case FILTER_MODE_LP:
				filters1[ i ].Process( buff, buff, nullptr, nullptr, size );
				break;
			case FILTER_MODE_HP:			
				filters1[ i ].Process( buff, nullptr, buff, nullptr, size );
				break;
			case FILTER_MODE_BP:			
				filters1[ i ].Process( buff, nullptr, nullptr, buff, size );
				break;
		}
		if( filterPolesSmartKnob.GetValue() > 0.5 ) switch( currentFilterMode ){
			case FILTER_MODE_LP:
				filters2[ i ].Process( buff, buff, nullptr, nullptr, size );
				break;
			case FILTER_MODE_HP:
				filters2[ i ].Process( buff, nullptr, buff, nullptr, size );
				break;
			case FILTER_MODE_BP:
				filters2[ i ].Process( buff, nullptr, nullptr, buff, size );
				break;
		}
	}
}
void AudioCallback( AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size ){	
	// PROCESS LFO FIRST -- WE ONLY NEED TO CALL ONCE PER AUDIO CALLBACK
	float lfoValue = lfo.Process();
	// SET INITIAL MOD VALUES FOR CONTROLS AFFECTED BY THE LFO
	float modDriftValue = driftValue +
		lfoValue * ( ( lfoValue * slitherDriftModValue ) - ( slitherDriftModValue / 2.f ) );
	float modShiftValue = shiftValue + 
		lfoValue * ( ( lfoValue * slitherShiftModValue ) - ( slitherShiftModValue / 2.f ) );
	float modCutoffValue = filterCutoffSmartKnob.GetValue() + 
		lfoValue * ( ( lfoValue * slitherHowlModValue ) - ( slitherHowlModValue / 2.f ) );
	float modClawsValue = ampSmartKnob.GetValue() + 
		lfoValue * ( ( lfoValue * slitherClawsModValue ) - ( slitherClawsModValue / 2.f ) );
		
	// SET MIX TO 1.0 - 0.8 DEPENDING ON THE SUB KNOB
	float superSawMixMod = fmap( 1.0 - subMixSmartKnob.GetValue(), 0.8, 1.0 );
	/*
		float filterFreqs[ size ];
		fliters[ i ].
		
		//  = fmap( cutoffMod, 1.0, fclamp( midiFreqs[ j ] * 16.0, 20.f, 20000.f ) );
		filters1[ j ].SetFreq( filterFreq );
		filters2[ j ].SetFreq( filterFreq );
		combFilters[ j ].SetFreq( fclamp( filterFreq, 20.f, 10000.f ) );
	*/

	if( debugMode ){
		uint8_t tic = clock.Process();
        if( tic ){
			for( int i = 0; i < NUM_VOICES; i++ ) envGates[ i ] = false;
			// RANDOMLY TRIGGER A GATE ENV
			envGates[ rand() % NUM_VOICES ] = true;
		}
	}
	float output[ size ];
	for( size_t i = 0; i < size; i++ ) output[ i ] = 0.f;
	for( int currentVoice = 0; currentVoice < NUM_VOICES; currentVoice++ ){
		// GET THE POUNCE ENV VALUE
		float pounceValue = pounces[ currentVoice ].Process( envGates[ currentVoice ] );
		// APPLY POUNCE ENV TO MODDABLE VALUES
		float thisModDriftValue = modDriftValue + ( pounceValue * pounceDriftModValue );
		float thisModShiftValue = modShiftValue + ( pounceValue * pounceShiftModValue );
		float thisModCutoffValue = modCutoffValue + ( pounceValue * pounceHowlModValue );
		// GET THE AMPENV VALUE
		float ampEnvValue = ampEnvs[ currentVoice ].Process( envGates[ currentVoice ] );

		superSaws[ currentVoice ].SetDrift( fclamp( thisModDriftValue, 0.f, 0.999 ) );
		superSaws[ currentVoice ].SetShift( fclamp( thisModShiftValue, 0.f, 0.999 ) );
		float cutoffFreq = fclamp(
			exp2f( thisModCutoffValue * 14.288 ),
			1.f,
			20000.f
		);
		filters1[ currentVoice ].SetFreq( cutoffFreq );
		filters2[ currentVoice ].SetFreq( cutoffFreq );
		float ampModValue = fclamp(
				modClawsValue + ( ampEnvValue * ampEnvModSmartKnob.GetValue() ),
				0.f,
				1.f 
			) * ( 1.f / sqrt( NUM_VOICES ) ); // ADJUST VOLUME BY NUMBER OF VOICES
		
		// ONCE ALL THE MODS ARE ASSIGNED, DEAL WITH THE AUDIO BUFFERS
		float mixedSignalBuffer[ size ];
		superSaws[ currentVoice ].Process( mixedSignalBuffer, size );

		// GET SUBOSC SIGNALS AND PUT THEM IN THE SUBOSC BUFFER
		float subSampleBuffer[ size ];
		subOscs[ currentVoice ].Process( subSampleBuffer, size );	

		// MIX SUBOSC IN TO THE MIXED SIGNAL BUFFER		
		for( size_t currentSample = 0; currentSample < size; currentSample++ ) {
			mixedSignalBuffer[ currentSample ] = mixedSignalBuffer[ currentSample ] * superSawMixMod + 
			subSampleBuffer[ currentSample ] * subMixSmartKnob.GetValue();
		}
		for( size_t i = 0; i < size; i++ ) // HANDLE DRIVE LEVEL INTO THE FILTER
			mixedSignalBuffer[ i ] = SoftClip( mixedSignalBuffer[ i ] * ( 1.f + fmap( driveValue, 0.f, 8.f ) ) );
		filterSignal( currentVoice, mixedSignalBuffer, size );
		// TODO: IMPLEMENT FANGS EFFECTS SECTION
		// fonepole( fangsTimeCurrentValue, fangsTimeSmartKnob.GetValue(), 0.002f );
		// if( fangsEffectType == EFFECT_MODE_ECHO ){
		// 	delayLine.SetDelay( fangsTimeCurrentValue * MAX_DELAY );
		// 	float delaySignal = delayLine.Read(); // READ FROM THE DELAY LINE
		// 	delayLine.Write(  // WRITE TO THE DELAY LINE
		// 		( finalSignal * ( 1.0 - fangsAmount1SmartKnob.GetValue() ) ) + ( delaySignal * fangsAmount1SmartKnob.GetValue() )
		// 	);
		// 	finalSignal = ( finalSignal * (1.0 - fangsMixValue ) ) + ( delaySignal * fangsMixValue );
		// } else if( fangsEffectType == EFFECT_MODE_REVERB ) {			
		// float reverbSignalBuffer[ size ];
		// for( size_t i  = 0; i < size; size++ ) reverbSignalBuffer[ i ] = mixedSignalBuffer[ i ];
		// reverb.Process( reverbSignalBuffer, size );
		// for( size_t i = 0; i < size; i++ )
		// 	mixedSignalBuffer[ i ] = ( mixedSignalBuffer[ i ] * ( 1.f - fangsMixValue ) ) + 
		// 		( reverbSignalBuffer[ i ] * fangsMixValue );
		// } else if( fangsEffectType == EFFECT_MODE_CHORUS ){
		// float chorusSignalBuffer[ size ];
		// for( size_t i  = 0; i < size; size++ ) chorusSignalBuffer[ i ] = mixedSignalBuffer[ i ];
		// chorus.Process( chorusSignalBuffer, sampleBlockSize );
		// for( size_t i  = 0; i < size; size++ ) 
		// 	mixedSignalBuffer[ i ] = ( mixedSignalBuffer[ i ] * ( 1.f - fangsMixValue ) ) + 
		// 		( chorusSignalBuffer[ i ] * fangsMixValue );			
		// } else if( fangsEffectType == EFFECT_MODE_FLANGER){
		// 	float flangerSignal = flanger.Process( finalSignal );
		// 	finalSignal = ( finalSignal * ( 1.0 - fangsMixValue ) ) + ( flangerSignal * fangsMixValue );
		// }
		for( size_t currentSample = 0; currentSample < size; currentSample++ )
			output[ currentSample ] += SoftClip( mixedSignalBuffer[ currentSample ] * ampModValue );
	}
	for( size_t i = 0; i < size; i++ ) out[ 0 ][ i ] = out [ 1 ][ i ] = output[ i ];
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
					// midiNotes[ i ] = 0;
					envGates[ i ] = false;
				}
			}
		}
	}
}
void handleKnobs(){
	driftValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX1, MUX1_DRIFT );
	shiftValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX1, MUX1_SHIFT );
	float growlValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX1, MUX1_GROWL );
	subMixSmartKnob.Update( growlValue );
	subTypeSmartKnob.Update( growlValue );
	float howlKnobValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX1, MUX1_HOWL );
	filterCutoffSmartKnob.Update( howlKnobValue );
	filterTypeSmartKnob.Update( howlKnobValue );
	float resKnobValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX1, MUX1_RES );
	filterResSmartKnob.Update( resKnobValue );
	filterPolesSmartKnob.Update( resKnobValue );	
	driveValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX1, MUX1_DRIVE );
	float attackKnobValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX1, MUX1_ATTACK );
	pounceAttackSmartKnob.Update( attackKnobValue );
	ampEnvAttackSmartKnob.Update( attackKnobValue );
	float sustainKnobValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX1, MUX1_SUSTAIN );
	pounceSustainSmartKnob.Update( sustainKnobValue );
	ampEnvSustainSmartKnob.Update( sustainKnobValue );
	float decayKnobValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX2, MUX2_DECAY );
	pounceDecaySmartKnob.Update( decayKnobValue );
	ampEnvDecaySmartKnob.Update( decayKnobValue );
	pounceDriftModValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX2, MUX2_PD_MOD );
	pounceShiftModValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX2, MUX2_PS_MOD );
	pounceHowlModValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX2, MUX2_PH_MOD );
	float slitherValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX2, MUX2_SLITHER );
	lfoFrequencySmartKnob.Update( slitherValue );
	lfoTypeSmartKnob.Update( slitherValue );
	slitherDriftModValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX2, MUX2_SD_MOD );
	slitherShiftModValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX2, MUX2_SS_MOD );
	slitherHowlModValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX2, MUX2_SH_MOD );
	slitherClawsModValue = 1.0 - hw.adc.GetFloat( ADC_SC_MOD );
	float fangsTimeValue = 1.0 - hw.adc.GetFloat( ADC_FT );
	fangsTimeSmartKnob.Update( fangsTimeValue );
	fangsEffectTypeSmartKnob.Update( fmap( fangsTimeValue, 0.f, 0.99f ) );
	float fangsAdjustValue = 1.0 - hw.adc.GetFloat( ADC_FA );	
	fangsAdjust1SmartKnob.Update( fangsAdjustValue );
	fangsAdjust2SmartKnob.Update( fangsAdjustValue );
	fangsMixValue = 1.0 - hw.adc.GetFloat( ADC_FM );
	float clawsValue = 1.0 - hw.adc.GetFloat( ADC_CLAWS );
	ampSmartKnob.Update( clawsValue );
	ampEnvModSmartKnob.Update( clawsValue );
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
	adcConfig[ ADC_MUX1 ].InitMux( // INIT MUX 1
        PIN_ADC_MUX1_IN, 
        MUX_CHANNELS_COUNT, 
        hw.GetPin( PIN_MUX_CH_A ), 
        hw.GetPin( PIN_MUX_CH_B ), 
        hw.GetPin( PIN_MUX_CH_C )
    );
	adcConfig[ ADC_MUX2 ].InitMux( // INIT MUX 2
        PIN_ADC_MUX2_IN, 
        MUX_CHANNELS_COUNT, 
        hw.GetPin( PIN_MUX_CH_A ), 
        hw.GetPin( PIN_MUX_CH_B ), 
        hw.GetPin( PIN_MUX_CH_C )
    );
	// INIT NON-MUXED PINS
	adcConfig[ ADC_SC_MOD ].InitSingle( PIN_SC_MOD_IN );
	adcConfig[ ADC_FT ].InitSingle( PIN_FT_IN );
	adcConfig[ ADC_FA ].InitSingle( PIN_FA_IN );
	adcConfig[ ADC_FM ].InitSingle( PIN_FM_IN );
	adcConfig[ ADC_CLAWS ].InitSingle( PIN_CLAWS_IN );
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
		fangsAdjust1SmartKnob.Activate();
		fangsAdjust2SmartKnob.Deactivate();
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
		fangsAdjust1SmartKnob.Deactivate();
		fangsAdjust2SmartKnob.Activate();
	}
}
void updateFilters(){
	currentFilterMode = filterTypeSmartKnob.GetValue() * FILTER_MODES_COUNT;
	float resValue = fmap( filterResSmartKnob.GetValue(), 0.f, 0.85 );
	for( int i = 0; i < NUM_VOICES; i++ ){
		filters1[ i ].SetRes( resValue );
		filters2[ i ].SetRes( resValue );
		combFilters[ i ].SetRevTime( fmap( filterResSmartKnob.GetValue(), 0.f, 1.0 ) );
	}
}
void updatePounces(){
	for( int i = 0; i < NUM_VOICES; i++ ){
		pounces[ i ].SetAttackTime( fmap( pounceAttackSmartKnob.GetValue(), 0.005f, 4.f ) );
		pounces[ i ].SetSustainLevel( pounceSustainSmartKnob.GetValue() );
		pounces[ i ].SetDecayTime( fmap( pounceDecaySmartKnob.GetValue(), 0.005f, 4.f ) );
		pounces[ i ].SetReleaseTime( fmap( pounceDecaySmartKnob.GetValue(), 0.005f, 4.f ) );
	}
}
void updateAmpEnvs(){
	for( int i = 0; i < NUM_VOICES; i++ ){
		ampEnvs[ i ].SetAttackTime( fmap( ampEnvAttackSmartKnob.GetValue(), 0.005f, 4.f ) );
		ampEnvs[ i ].SetSustainLevel( ampEnvSustainSmartKnob.GetValue() );
		ampEnvs[ i ].SetDecayTime( fmap( ampEnvDecaySmartKnob.GetValue(), 0.005f, 4.f ) );
		ampEnvs[ i ].SetReleaseTime( fmap( ampEnvDecaySmartKnob.GetValue(), 0.005f, 4.f ) );
	}
}
void initSmartKnobs(){
	subMixSmartKnob.Init( true, 0.f );
	subTypeSmartKnob.Init( false, 0.f );
	filterCutoffSmartKnob.Init( true, 0.5f );
	filterTypeSmartKnob.Init( false, 0.f );
	filterResSmartKnob.Init( true, 0.f );
	filterPolesSmartKnob.Init( false, 0.f );
	pounceAttackSmartKnob.Init( true, 0.f );
	ampEnvAttackSmartKnob.Init( false, 0.f );
	pounceSustainSmartKnob.Init( true, 1.f );
	ampEnvSustainSmartKnob.Init( false, 1.f );
	pounceDecaySmartKnob.Init( true,  0.2f );
	ampEnvDecaySmartKnob.Init( false, 0.2f );
	ampSmartKnob.Init( true, 0.f );
	ampEnvModSmartKnob.Init( false, 0.8f );
	lfoFrequencySmartKnob.Init( true, 0.5f );
	lfoTypeSmartKnob.Init( false, 0.f );
	fangsTimeSmartKnob.Init( true, 0.5f );
	fangsEffectTypeSmartKnob.Init( false, 0.f );
	fangsAdjust1SmartKnob.Init( true, 0.2f );
	fangsAdjust2SmartKnob.Init( false, 0.2f );
}
void initDsp(){
	for( int i = 0; i < NUM_VOICES; i++ ){
		superSaws[ i ].Init( SAMPLE_RATE );
		subOscs[ i ].Init( SAMPLE_RATE );
		filters1[ i ].Init( SAMPLE_RATE );
		filters1[ i ].SetDrive( 0.f );
		filters2[ i ].Init( SAMPLE_RATE );
		filters2[ i ].SetDrive( 0.f );
		int combfilterBufferLength = sizeof( combFilterBuffers[ i ] ) / sizeof( combFilterBuffers[ i ][ 0 ] );
		for( int j = 0; j < combfilterBufferLength; j++ ) combFilterBuffers[ i ][ j ] = 0.f;
		combFilters[ i ].Init( SAMPLE_RATE, combFilterBuffers[ i ], combfilterBufferLength );
		combFilters[ i ].SetPeriod( 2.0 ); // FOR NOW, THIS IS A FIXED VALUE = THE MAX BUFFER SIZE
		pounces[ i ].Init( SAMPLE_RATE, 32 );
		ampEnvs[ i ].Init( SAMPLE_RATE, 32 );
	}
	lfo.Init( SAMPLE_RATE / 32.f );
	lfo.SetWaveform( lfo.WAVE_SIN );
	// delayLine.Init();
	// reverb.Init( SAMPLE_RATE );
	// chorus.Init( SAMPLE_RATE );
	// flanger.Init( SAMPLE_RATE );
}
void updateLfo(){
	updateLfoWave();
	lfo.SetFreq( fmap( lfoFrequencySmartKnob.GetValue(), 0.5f, 20.f ) );
}
void updateFangs(){
	// fangsEffectType = fangsEffectTypeSmartKnob.GetValue() * EFFECT_MODES_COUNT;
	// reverb.SetFeedback( fangsAmount1SmartKnob.GetValue() );
	// reverb.SetLpFreq( fmap( fangsTimeSmartKnob.GetValue(), 70.f, 18000.f ) );
	// chorus.SetDelay( fangsTimeSmartKnob.GetValue(), fangsTimeSmartKnob.GetValue() ); // TODO: 2 ARGS R 4 STEREO OUTS
	// chorus.SetLfoDepth( fangsAmount1SmartKnob.GetValue() );
	// chorus.SetLfoFreq( fmap( fangsAmount2SmartKnob.GetValue(), 0.1f, 5.f ), 0.2f ); // TODO: 2 ARGS R 4 STEREO OUTS
	// flanger.SetLfoFreq( fmap( fangsTimeSmartKnob.GetValue(), 0.1f, 5.f ) );
    // flanger.SetFeedback( fangsAmount1SmartKnob.GetValue() );
	// flanger.SetLfoDepth( fmap( fangsAmount2SmartKnob.GetValue(), 0.1f, 5.f ) );
}
int main(){
	hw.Init();
	hw.SetAudioBlockSize( sampleBlockSize ); // number of samples handled per callback
	hw.SetAudioSampleRate( SaiHandle::Config::SampleRate::SAI_48KHZ );
	if( !debugMode ){
		MidiUsbHandler::Config midiConfig;
		midiConfig.transport_config.periph = MidiUsbTransport::Config::INTERNAL;
		midi.Init( midiConfig );
	} else {
		hw.StartLog();
		clock.Init( 1.f / sampleBlockSize, SAMPLE_RATE );
	}
	initSmartKnobs();
	initDsp();
	initAdc();
	modeSwitch.Init( hw.GetPin( MODE_SWITCH_PIN ), 100 );
	hw.StartAudio( AudioCallback ); 
	int debugCount = 0;
	while( true ){
		if( !debugMode ) handleMidi();
		else{
			int sevenChord[ 4 ] = { 0, 4, 6, 10 };
			for( int i = 0; i < NUM_VOICES; i++ ) midiNotes[ i ] = ROOT_MIDI_NOTE + sevenChord[ i ];
		}
		for( int i = 0; i < NUM_VOICES; i++ ) {
			midiFreqs[ i ] = mtof( midiNotes[ i ] );
 			superSaws[ i ].SetFreq( midiFreqs[ i ] );
			subOscs[ i ].SetFreq( midiFreqs[ i ] / ( subOscOctave + 1 ) );
		}
		modeSwitch.Debounce();
		operationMode = !modeSwitch.Pressed();
		// IF THE OPERATING MODE CHANGED, CHANGE MODE ON THE SMART KNOBS
		if( operationMode != lastOperationMode ) handleSmartKnobSwitching();
		lastOperationMode = operationMode;		
		handleKnobs();
		updateLfo();
		updatePounces();
		updateAmpEnvs();		
		updateSubOscWave();
		updateFilters();
		updateFangs();
		bool midiNotesOn = false;
		for( int i = 0; i < NUM_VOICES; i++ ) if( envGates[ i ] ) midiNotesOn = true;
		hw.SetLed( midiNotesOn );
		if( debugMode ){
			debugCount++;
			if( debugCount >= 500 ){

				debugCount = 0;
			}
		}
		System::Delay( 1 );
	}
}