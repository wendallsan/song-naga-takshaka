#include "daisy_seed.h"
#include "daisysp.h"
#include "SmartKnob.h"
#include "BlockSuperSawOsc.h"
#include "BlockOscillator.h"
#include "BlockMoogLadder.h"
#include "BlockComb.h"

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

using namespace daisy;
using namespace daisysp;

enum AdcChannels { ADC_MUX1, ADC_MUX2, ADC_SC_MOD, ADC_FA, ADC_FS, ADC_FR, ADC_CLAWS, ADC_CHANNELS_COUNT };
enum mux1Signals { MUX1_DRIFT, MUX1_SHIFT, MUX1_GROWL, MUX1_HOWL, MUX1_RES, MUX1_DRIVE, MUX1_ATTACK, MUX1_SUSTAIN, MUX1_CHANNELS_COUNT }; 
enum mux2Signals{ MUX2_DECAY, MUX2_PD_MOD, MUX2_PS_MOD, MUX2_PH_MOD, MUX2_SLITHER, MUX2_SD_MOD, MUX2_SS_MOD, MUX2_SH_MOD, MUX2_CHANNELS_COUNT };
enum SubOscWaveforms { SUBOSC_SINE_1, SUBOSC_SINE_2, SUBOSC_TRI_1, SUBOSC_TRI_2, SUBOSC_SQUARE_1, SUBOSC_SQUARE_2, SUBOSC_WAVEFORMS_COUNT };
enum lfoWaveforms { SINE, TRI, SAW, RAMP, SQUARE, RANDOM, LFO_WAVEFORMS_COUNT };
enum FilterModes { FILTER_MODE_LP, FILTER_MODE_COMB, FILTER_MODES_COUNT };
enum operationModes { OP_MODE_ALT, OP_MODE_NORMAL, OPERATION_MODES_COUNT };
float combFilterBuffers[ NUM_VOICES ][ 9600 ],
	midiFreqs[ NUM_VOICES ],
	voicesGainStageAdjust = 1.f / sqrt( NUM_VOICES ),
	effectGainStageAdjust = 1.f / sqrt( 2 ),
	driftValue,
	shiftValue,
	driveValue,
	slitherValue,
	resValue,
	slitherDriftModValue,
	slitherShiftModValue,
	slitherHowlModValue,
	slitherClawsModValue,
	pounceDriftModValue,
	pounceShiftModValue,
	pounceHowlModValue,
	superSawMixMod,
	pounceAttackValue,
	pounceSustainValue,
	pounceDecayValue,
	fangsAttackValue,
	fangsSustainValue,
	fangsDecayValue,
	lastRandLfoValue = 0.f;
int midiNotes[ NUM_VOICES ] = { ROOT_MIDI_NOTE, ROOT_MIDI_NOTE, ROOT_MIDI_NOTE, ROOT_MIDI_NOTE },
	currentFilterMode = FILTER_MODE_LP,
	subOscOctave = 1,
	nextVoice = 0;
bool envGates[ NUM_VOICES ] = { false, false, false, false },
	operationMode = OP_MODE_NORMAL,
	lastOperationMode = OP_MODE_NORMAL,
	randLfoEnabled = false;
DaisySeed hw;
MidiUsbHandler midi;
Switch modeSwitch;
SmartKnob subMixSmartKnob,
	subTypeSmartKnob,
	filterCutoffSmartKnob,
	filterTypeSmartKnob,
	clawsSmartKnob,
	fangsModSmartKnob,
	slitherFrequencySmartKnob,
	slitherTypeSmartKnob,
	pounceAttackSmartKnob,
	ampEnvAttackSmartKnob,
	pounceSustainSmartKnob,
	ampEnvSustainSmartKnob,
	pounceReleaseSmartKnob,
	ampEnvReleaseSmartKnob;
BlockSuperSawOsc superSaws[ NUM_VOICES ];
BlockOscillator subOscs[ NUM_VOICES ];
Oscillator lfo;
BlockMoogLadder filters[ NUM_VOICES ];
BlockComb combFilters[ NUM_VOICES ];
Adsr pounces[ NUM_VOICES ], ampEnvs[ NUM_VOICES ];
const size_t sampleBlockSize = 16;
void filterSignal( int i, float *buff, size_t size ){
	if( currentFilterMode == FILTER_MODE_COMB ) combFilters[ i ].Process( buff, size );
	else filters[ i ].Process( buff, size );
}
float handleLfo(){
	float lfoValue;
	if( randLfoEnabled ){
		if( lfo.IsEOC() ){
			lfoValue = ( ( (float) rand() / RAND_MAX ) * 2.f ) - 1.f;
			lastRandLfoValue = lfoValue;
		} else lfoValue = lastRandLfoValue;
	} else return lfo.Process();
}
void AudioCallback( AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size ){	
	float lfoValue = handleLfo();
	// SET INITIAL MOD VALUES FOR CONTROLS AFFECTED BY THE LFO
	float modDriftValue = driftValue +
		lfoValue * ( ( lfoValue * slitherDriftModValue ) - ( slitherDriftModValue / 2.f ) );
	float modShiftValue = shiftValue + 
		lfoValue * ( ( lfoValue * slitherShiftModValue ) - ( slitherShiftModValue / 2.f ) );
	float modCutoffValue = filterCutoffSmartKnob.GetValue() + 
		lfoValue * ( ( lfoValue * slitherHowlModValue ) - ( slitherHowlModValue / 2.f ) );
	float modClawsValue = clawsSmartKnob.GetValue() + 
		lfoValue * ( ( lfoValue * slitherClawsModValue ) - ( slitherClawsModValue / 2.f ) );
	// DECLARE OUTPUT BUFFER AND ZERO IT OUT
	float output[ size ]; for( size_t i = 0; i < size; i++ ) output[ i ] = 0.f;
	for( int currentVoice = 0; currentVoice < NUM_VOICES; currentVoice++ ){
		// GET THE ENV VALUES
		float pounceValue = pounces[ currentVoice ].Process( envGates[ currentVoice ] );
		float fangValue = ampEnvs[ currentVoice ].Process( envGates[ currentVoice ] );
		// APPLY POUNCE ENV TO MODDABLE VALUES
		float thisModDriftValue = modDriftValue + ( pounceValue * pounceDriftModValue );
		float thisModShiftValue = modShiftValue + ( pounceValue * pounceShiftModValue );
		float thisModCutoffValue = modCutoffValue + ( pounceValue * pounceHowlModValue );
		superSaws[ currentVoice ].SetDrift( fclamp( thisModDriftValue, 0.f, 0.999 ) );
		superSaws[ currentVoice ].SetShift( fclamp( thisModShiftValue, 0.f, 0.999 ) );
		// TODO REVISIT HOW WE'RE SETTING FILTER CUTOFF
		// filters[ currentVoice ].SetFreq( fclamp( 
		// 	exp2f( thisModCutoffValue * 14.288 ) + 
		// 	mtof( midiNotes[ currentVoice ] - 24 ), 
		// 	0.f, 
		// 	20000.f 
		// ) );
		float cutoffFreq = fmap( 
			thisModCutoffValue,
			0.f,
			fclamp( midiFreqs[ currentVoice ] * 32.f, 0.f, 20000.f ),
			Mapping::EXP
		);
		filters[ currentVoice ].SetFreq( fclamp( cutoffFreq, 0.f, 20000.f ) );
		combFilters[ currentVoice ].SetFreq( fclamp( cutoffFreq, 20.f, 10000.f ) );
		float ampModValue = fclamp(
			modClawsValue + ( fangValue * fangsModSmartKnob.GetValue() ),
			0.f,
			1.f 
		) * voicesGainStageAdjust;
		float mixedSignalBuffer[ size ]; // ONCE ALL THE MODS ARE ASSIGNED, DEAL WITH THE AUDIO BUFFERS
		superSaws[ currentVoice ].Process( mixedSignalBuffer, size );
		float subSampleBuffer[ size ]; // GET SUBOSC SIGNALS AND PUT THEM IN THE SUBOSC BUFFER
		subOscs[ currentVoice ].Process( subSampleBuffer, size );
		// MIX SUBOSC IN TO THE MIXED SIGNAL BUFFER		
		for( size_t currentSample = 0; currentSample < size; currentSample++ ) {
			mixedSignalBuffer[ currentSample ] = mixedSignalBuffer[ currentSample ] * superSawMixMod + 
			subSampleBuffer[ currentSample ] * subMixSmartKnob.GetValue();
		}
		for( size_t i = 0; i < size; i++ ) // HANDLE DRIVE LEVEL INTO THE FILTER
			mixedSignalBuffer[ i ] = SoftClip( mixedSignalBuffer[ i ] * ( 1.f + fmap( driveValue, 0.f, 8.f ) ) );
		filterSignal( currentVoice, mixedSignalBuffer, size );
		for( size_t currentSample = 0; currentSample < size; currentSample++ )
			output[ currentSample ] += SoftClip( mixedSignalBuffer[ currentSample ] * ampModValue * voicesGainStageAdjust );
	}
	// TODO: NICK SAYS:
	// "you can always throw in a for loop when you need to do the delay part in the chain
	// keep in mind if you have feedback into the delay line, if you do it block-based, 
	// there will be an extra N samples of delay in the feedback loop (minimum feedback 
	// time would be the block size)"
	for( size_t i = 0; i < size; i++ ){

		out[ 0 ][ i ] = out [ 1 ][ i ] = output[ i ];
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
		} else if( midiEvent.type == NoteOff )
			for( int i = 0; i < NUM_VOICES; i++ ) if( midiNotes[ i ] == noteMessage.note ) envGates[ i ] = false;
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
	resValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX1, MUX1_RES );
	driveValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX1, MUX1_DRIVE );
	pounceAttackValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX1, MUX1_ATTACK );
	pounceSustainValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX1, MUX1_SUSTAIN );
	pounceDecayValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX2, MUX2_DECAY );
	pounceDriftModValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX2, MUX2_PD_MOD );
	pounceShiftModValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX2, MUX2_PS_MOD );
	pounceHowlModValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX2, MUX2_PH_MOD );
	float slitherValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX2, MUX2_SLITHER );
	slitherFrequencySmartKnob.Update( slitherValue );
	slitherTypeSmartKnob.Update( slitherValue );
	slitherDriftModValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX2, MUX2_SD_MOD );
	slitherShiftModValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX2, MUX2_SS_MOD );
	slitherHowlModValue = 1.0 - hw.adc.GetMuxFloat( ADC_MUX2, MUX2_SH_MOD );
	slitherClawsModValue = 1.0 - hw.adc.GetFloat( ADC_SC_MOD );
	fangsAttackValue = 1.0 - hw.adc.GetFloat( ADC_FA );
	fangsSustainValue = 1.0 - hw.adc.GetFloat( ADC_FS );
	fangsDecayValue = 1.0 - hw.adc.GetFloat( ADC_FR );
	float clawsValue = 1.0 - hw.adc.GetFloat( ADC_CLAWS );
	clawsSmartKnob.Update( clawsValue );
	fangsModSmartKnob.Update( clawsValue );
	superSawMixMod = fmap( 1.0 - subMixSmartKnob.GetValue(), 0.8, 1.0 );
}
void updateLfoWave(){
	int lfoWave = slitherTypeSmartKnob.GetValue() * LFO_WAVEFORMS_COUNT ;  // int range 0 - 2
	switch( lfoWave ){
		case 0:
			lfo.SetWaveform( lfo.WAVE_SIN );
			randLfoEnabled = false;
			break;
		case 1:
			lfo.SetWaveform( lfo.WAVE_TRI );
			randLfoEnabled = false;
			break;
		case 2:
			lfo.SetWaveform( lfo.WAVE_SAW );
			randLfoEnabled = false;
			break;
		case 3:
			lfo.SetWaveform( lfo.WAVE_RAMP );
			randLfoEnabled = false;
			break;
		case 4:
			lfo.SetWaveform( lfo.WAVE_SQUARE );
			randLfoEnabled = false;
			break;
		case 5:
			lfo.SetWaveform( lfo.WAVE_SQUARE );
			randLfoEnabled = true;
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
	adcConfig[ ADC_FA ].InitSingle( PIN_FT_IN );
	adcConfig[ ADC_FS ].InitSingle( PIN_FA_IN );
	adcConfig[ ADC_FR ].InitSingle( PIN_FM_IN );
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
		clawsSmartKnob.Activate();
		fangsModSmartKnob.Deactivate();
		slitherFrequencySmartKnob.Activate();
		slitherTypeSmartKnob.Deactivate();
	} else {
		subMixSmartKnob.Deactivate();
		subTypeSmartKnob.Activate();
		filterCutoffSmartKnob.Deactivate();
		filterTypeSmartKnob.Activate();
		clawsSmartKnob.Deactivate();
		fangsModSmartKnob.Activate();
		slitherFrequencySmartKnob.Deactivate();
		slitherTypeSmartKnob.Activate();
	}
}
void updateFilters(){
	currentFilterMode = filterTypeSmartKnob.GetValue() * FILTER_MODES_COUNT;
	for( int i = 0; i < NUM_VOICES; i++ ){
		filters[ i ].SetRes( resValue );
		combFilters[ i ].SetRevTime( fmap( resValue, 0.f, 1.0 ) );
	}
}
void updatePounces(){
	for( int i = 0; i < NUM_VOICES; i++ ){
		pounces[ i ].SetAttackTime( fmap( pounceAttackValue, 0.005f, 4.f ) );
		pounces[ i ].SetSustainLevel( pounceSustainValue );
		pounces[ i ].SetDecayTime( fmap( pounceDecayValue, 0.005f, 4.f ) );
		pounces[ i ].SetReleaseTime( fmap( pounceDecayValue, 0.005f, 4.f ) );
	}
}
void updateFangs(){
	for( int i = 0; i < NUM_VOICES; i++ ){
		ampEnvs[ i ].SetAttackTime( fmap( fangsAttackValue, 0.005f, 4.f ) );
		ampEnvs[ i ].SetSustainLevel( fangsSustainValue );
		ampEnvs[ i ].SetDecayTime( fmap( fangsDecayValue, 0.005f, 4.f ) );
		ampEnvs[ i ].SetReleaseTime( fmap( fangsDecayValue, 0.005f, 4.f ) );
	}
}
void initSmartKnobs(){
	subMixSmartKnob.Init( true, 0.f );
	subTypeSmartKnob.Init( false, 0.f );
	filterCutoffSmartKnob.Init( true, 0.5f );
	filterTypeSmartKnob.Init( false, 0.f );
	clawsSmartKnob.Init( true, 0.f );
	fangsModSmartKnob.Init( false, 1.f );
	slitherFrequencySmartKnob.Init( true, 0.5f );
	slitherTypeSmartKnob.Init( false, 0.f );
	pounceAttackSmartKnob.Init( true, 0.f );
	ampEnvAttackSmartKnob.Init( false, 0.f );
	pounceSustainSmartKnob.Init( true, 0.5f);
	ampEnvSustainSmartKnob.Init( false, 0.5f);
	pounceReleaseSmartKnob.Init( true, 0.5f);
	ampEnvReleaseSmartKnob.Init( false, 0.5f );
}
void initDsp(){
	for( int i = 0; i < NUM_VOICES; i++ ){
		superSaws[ i ].Init( SAMPLE_RATE );
		subOscs[ i ].Init( SAMPLE_RATE );
		filters[ i ].Init( SAMPLE_RATE );
		int combfilterBufferLength = sizeof( combFilterBuffers[ i ] ) / sizeof( combFilterBuffers[ i ][ 0 ] );
		for( int j = 0; j < combfilterBufferLength; j++ ) combFilterBuffers[ i ][ j ] = 0.f;
		combFilters[ i ].Init( SAMPLE_RATE, combFilterBuffers[ i ], combfilterBufferLength );
		// TODO MAP COMB FILTER PERIOD TO SOMETHING?
		combFilters[ i ].SetPeriod( 2.0 ); // FOR NOW, THIS IS A FIXED VALUE = THE MAX BUFFER SIZE
		pounces[ i ].Init( SAMPLE_RATE, 32 );
		ampEnvs[ i ].Init( SAMPLE_RATE, 32 );
	}
	lfo.Init( SAMPLE_RATE / 32.f );
	lfo.SetWaveform( lfo.WAVE_SIN );
}
void updateLfo(){
	updateLfoWave();
	lfo.SetFreq( fmap( slitherFrequencySmartKnob.GetValue(), 0.02f, 20.f ) );
}
int main(){	
	hw.Init();
	hw.SetAudioBlockSize( sampleBlockSize ); // SET THE NUMBER OF SAMPLES HANDLED PER BLOCK
	hw.SetAudioSampleRate( SaiHandle::Config::SampleRate::SAI_48KHZ );		
	MidiUsbHandler::Config midiConfig;
	midiConfig.transport_config.periph = MidiUsbTransport::Config::INTERNAL;
	midi.Init( midiConfig );	
	initSmartKnobs();
	initDsp();
	initAdc();
	modeSwitch.Init( hw.GetPin( MODE_SWITCH_PIN ), 100 );
	hw.StartAudio( AudioCallback );
	while( true ){
		handleMidi();
		for( int i = 0; i < NUM_VOICES; i++ ){
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
		// updateFangs();		
		updateSubOscWave();
		updateFilters();
		System::Delay( 1 );
	}
}