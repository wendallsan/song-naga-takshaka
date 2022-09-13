# Takshaka 

### Supersaw desktop synthesizer by Song Naga

This project is powered by the [Electro-smith Daisy Seed](https://www.electro-smith.com/daisy/daisy) and the [Simple synthesizer design platform](https://www.synthux.academy/simple).
![Design Concept](https://user-images.githubusercontent.com/1865305/189991192-ca00ab04-4045-4d62-9642-9f78877c540c.png)
Design Concept Illustration

## Planned Features

- **Supersaw Oscillator** modelled on the Roland JP-8000 with **Range** (±3 octaves, stepped by octave), **Coarse** (±6 semitones), and **Fine** (±1 semitone) frequency controls, as well as a CV input.
- **Drift** and **Intensity** controls adjust supersaw detune and amount of effect, with a CV inputs for each control.
- **Suboscillator** that can be mixed in with the main main signal and is output on its own dedicated **Sub Out**.  The Sub Out bypasses the Filter and Amp sections, and provides the raw sub output signal.  
- **Filter** with **Cutoff**, **Resonance**, and **Drive** controls, including a CV input for Cutoff.
- **Amplifier** with its own **dedicated envelope** and CV input.
- **Mod Envelope** can modulate Drift, Intensity and Cutoff.
- Both the Amp and Mod Envelopes are controlled by a shared Gate Input, and can be retriggered using the Trigger Input.
- **LFO** with **Rate** control can modulate Drift, Intensity, Cutoff, or Amp
- **MIDI Control** of all parameters via USB or MIDI input.  As an added bonus, Takshaka can operate as a polyphonic 4-voice synthesizer when controlled by MIDI. 
- **Mode Button** provides access to additional controls:  
  -  **Suboscillator Waveform Select** (press MODE + adjust Super Sub knob.  Options are sine, square, tri.)
  -  **Filter Type Select** (pres MODE + adjust Filter Cutoff knob.  Options are 24db LPF, 18db LPF, 12db LPF, 12db HPF, and notch.)
  -  **Drive Type Select** (press MODE + adjust Filter Drive knob.  Options are ___.)
  -  **LFO Waveform Select** (press MODE + adjust LFO Rate knob.  Options are sine, tri, ramp, saw, square and random.)

### About the Supersaw Oscillator  
The Supersaw Oscillator used in **Takshaka** is based on the thesis ["How to Emulate the Super Saw"](https://forum.orthogonaldevices.com/uploads/short-url/rLjREzRcZvvK2527rFnTGvuwY1b.pdf) written by Adam Szabo in 2010.
