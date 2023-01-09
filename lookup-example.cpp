#include "daisy_seed.h"
#include "daisysp.h"
#define CURVE_POINTS 17
#define DETUNE_CURVE { 0.0, 0.00967268, 0.0220363, 0.0339636, 0.0467636, 0.0591273, 0.0714909, 0.0838545, 0.0967273, 0.121527, 0.147127, 0.193455, 0.243418, 0.2933815, 0.343345, 0.3928, 1.0 }
using namespace daisy;
DaisySeed hw;
float driftValue,
    lookupResult,
	detuneLookupTable[ CURVE_POINTS ] = DETUNE_CURVE;
int detuneKnobCurveIndex;
enum AdcChannel { driftKnob, ADC_CHANNELS_COUNT };
float lookup( float knobAmount ){
    // GET THE MIN POSSIBLE AMOUNT ON THE LOOKUP TABLE, WHICH IS THE VALUE AT THE CURVE INDEX
	float minLookupValue = detuneLookupTable[ detuneKnobCurveIndex ];
    // GET THE MAX POSSIBLE AMOUNT ON THE LOOKUP TABLE, WHICH IS THE NEXT VALUE ON THE CURVE
	float maxLookupValue = detuneLookupTable[ detuneKnobCurveIndex + 1 ];
    // GET THE MIN POSSIBLE AMOUNT OF THE KNOB VALUE, WHICH IS THE INDEX DIVIDED BY THE NUMBER OF CURVE POINTS
	float minKnobPosition = ( float )detuneKnobCurveIndex / CURVE_POINTS;
	// GET THE MAX POSSIBLE AMOUNT OF THE KNOB VALUE, WHICH IS THE INDEX DIVIDED BY THE NUMBER OF CURVE POINTS
    float maxKnobPosition = ( ( float )detuneKnobCurveIndex + 1 ) / CURVE_POINTS;
    // FIGURE OUT HOW FAR BETWEEN THE MIN AND MAX POINTS ON THE KNOB THAT THE CURRENT VALUE IS
    // FIGURE OUT THE VALUE ON THE LOOKUP TABLE THAT CORRESPONDS WITH THIS SAME POINT
    // RETURN THAT VALUE ON THE LOOKUP TABLE
	return minLookupValue + ( ( maxLookupValue - minLookupValue ) * ( ( knobAmount - minKnobPosition ) / ( maxKnobPosition / minKnobPosition ) ) );
}
int main(){
	hw.Init();
	AdcChannelConfig adcConfig[ ADC_CHANNELS_COUNT ];
    adcConfig[ driftKnob ].InitSingle( daisy::seed::A0 );
	hw.adc.Init( adcConfig, ADC_CHANNELS_COUNT );
    hw.adc.Start();
	while( true ){
		driftValue = hw.adc.GetFloat( driftKnob );
		detuneKnobCurveIndex = driftValue * CURVE_POINTS;
        lookupResult = lookup( driftValue );
		System::Delay( 1 );
	}
}