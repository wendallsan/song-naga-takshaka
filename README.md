# Takshaka 

### Supersaw desktop synthesizer by Song Naga

This project is powered by the [Electro-smith Daisy Seed](https://www.electro-smith.com/daisy/daisy) and was developed using the the [Simple synthesizer design platform](https://www.synthux.academy/simple).

<img width="1001" alt="image" src="https://github.com/user-attachments/assets/ee726df8-112d-4a92-88f9-ab15783c1485" />


Current Design Concept  

![TAKSHAKA Supersaw Synthesizer Flowchart drawio](https://github.com/user-attachments/assets/ab366624-8888-4e36-8fee-69feb5ac0f69)


Functional Diagram  

## Features

- **4 Note Polyphony**  
- **SupaSaw Oscillator** is modelled on the famous Roland JP-8000 preset.
- **DRIFT** and **SHIFT** knobs adjust supersaw detune and volume levels of the oscillator stack that produces the SupaSaw effect.
- **Suboscillator** that can be mixed in with the main main signal via the **SUB** knob.  Waveform and octave can be set via the **SUB OCT** and the **SUB WAVE** switches.
- **VENOM** knob distorts the combined SupaSaw and Sub oscillator signals before passing it on to the filters.
- **LOW PASS FILTER** provides Frequency Control via the **GROWL** knob and resonance via the **REZ** knob.  Keyboard tracking can be enabled using the **GROWL KEYTRACK** switch.
- **COMB FILTER** provides delay time control via the **HOWL** knob and feedback amount via the **FDBK** knob.  Keyboard tracking can be enabled using the **HOWL KEYTRACK** switch.
- **FILTER ORDER** can be switched using the **FILTER ORDER** switch.
- **POUNCE** section provides an ADSR envelope that can be routed to multiple destinations on the synth.
- **SLITHER** section provides a LFO that can be routed to multiple destinations on the synth.  Waveshape of the LFO can be set using the **SLITHER SELECT** switches.
- **CLAWS** section provides a dedicated ADSR envelope and VCA for the output signal.   
- **Full MIDI Control** of all parameters via USB-over-MIDI input.  USB also powers the instrument.
- **Stereo Output**  

### About the Supersaw Oscillator  
The Supersaw Oscillator used in **Takshaka** is based on the thesis ["How to Emulate the Super Saw"](https://forum.orthogonaldevices.com/uploads/short-url/rLjREzRcZvvK2527rFnTGvuwY1b.pdf) written by Adam Szabo in 2010.
