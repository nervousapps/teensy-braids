#include "WProgram.h"

#include "macro_oscillator.h"

#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>
#include <ResponsiveAnalogRead.h>

using namespace braids;

MacroOscillator osc;
IntervalTimer myTimer;

const uint32_t kSampleRate = 96000;
const uint16_t kAudioBlockSize = 28;

uint8_t sync_buffer[kAudioBlockSize];
int16_t bufferA[kAudioBlockSize];
int16_t bufferB[kAudioBlockSize];
uint8_t buffer_sel;

volatile uint8_t buffer_index;
volatile uint8_t wait;

// Timer interruption to put the following sample
void putSample(void){
    uint16_t val;

    if(buffer_sel)
        val = ((uint16_t)(bufferB[buffer_index]+0x7FFF))>>4;
    else
        val = ((uint16_t)(bufferA[buffer_index]+0x7FFF))>>4;

    buffer_index++;
    // // FOR teensy 3.5
    // analogWrite(A21, val);
    // analogWrite(A22, val);
    // FOR teensy 3.2
    analogWrite(A14, val);
    if(buffer_index>=kAudioBlockSize) {
        wait = 0;
        buffer_index = 0;
        buffer_sel = ~buffer_sel;
    }

}

// Globals that define the parameters of the oscillator
static volatile int16_t pitch,pre_pitch;
static volatile int16_t timbre;
static volatile int16_t color;
static volatile int16_t shape;


// Handles the ontrol change
// void OnControlChange(byte channel, byte control, byte value){
//     if(control==32){
//         shape = value;
//     }
//     if(control==33){
//         color = value << 7;
//     }
//     if(control==34){
//         timbre = value << 7;
//     }
// }

// Handles note on events
void OnNoteOn(byte channel, byte note, byte velocity){
    // If the velocity is larger than zero, means that is turning on
    if(velocity){
        // Sets the 7 bit midi value as pitch
        pitch = note << 7;
        // triggers a note
        osc.Strike();
    }
}

// // for teensy 3.5
// const int ANALOG_PINS[2] = {A12,A13}; //{A0,A1,A2};
// const int CCID[2] = {33,34}; //{32,33,34};
// ResponsiveAnalogRead analog[]{
//   {ANALOG_PINS[0],true},
//   {ANALOG_PINS[1],true}
// };
// Encoder knobRightS(8, 9);
// long positionRightS = -999;
// /////////////////////////

//for teensy 3.2
const int ANALOG_PINS[3] = {A0,A1,A2};
const int CCID[3] = {32,33,34};
ResponsiveAnalogRead analog[]{
  {ANALOG_PINS[0],true},
  {ANALOG_PINS[1],true},
  {ANALOG_PINS[2],true}
};
/////////////////////

int val;

extern "C" int main(void)
{

    // Initalizes the buffers to zero
    memset(bufferA, 0, kAudioBlockSize);
    memset(bufferB, 0, kAudioBlockSize);

    // Global used to trigger the next buffer to render
    wait = 0;

    // Initializes the objects
    osc.Init();
    osc.set_shape(MACRO_OSC_SHAPE_GRANULAR_CLOUD);
    osc.set_parameters(0, 0);
    myTimer.begin(putSample,1e6/96000.0);

    //pinMode(13, OUTPUT);
    //pinMode(23, OUTPUT);
    analogWriteResolution(12);
    analogReadResolution(7);
    analogReadAveraging(4);
    analogReference(EXTERNAL);


    // Defines the handlers of midi events
    //usbMIDI.setHandleControlChange(OnControlChange);
    usbMIDI.setHandleNoteOn(OnNoteOn);

    pitch = 32 << 7;


    // Loop
    while (1) {
        memset(sync_buffer, 0, sizeof(sync_buffer));
        // Set the pin to 1 to mark the begining of the render cycle
        //digitalWriteFast(13,HIGH);
        // If the pitch changes update it
        if(pre_pitch!=pitch){
            osc.set_pitch(pitch);
            pre_pitch = pitch;
        }
        // Get the timbre and color parameters from the ui and set them
        osc.set_parameters(timbre,color);

        // Trims the shape to the valid values
        shape = shape >= MACRO_OSC_SHAPE_DIGITAL_MODULATION ? MACRO_OSC_SHAPE_DIGITAL_MODULATION : shape<0 ? 0 : shape;

        // Sets the shape
        MacroOscillatorShape osc_shape = static_cast<MacroOscillatorShape>(shape);//
        osc.set_shape(osc_shape);

        if(buffer_sel){
            osc.Render(sync_buffer, bufferA, kAudioBlockSize);
        }
        else{
            osc.Render(sync_buffer, bufferB, kAudioBlockSize);
        }
        // Reads the midi data
        usbMIDI.read();

        // // for teensy 3.5
        // long newRightS;
        // newRightS = knobRightS.read()/2;
        // if (newRightS != positionRightS){
        //   if (newRightS < 0){
        //     newRightS = 40;
        //     knobRightS.write(newRightS*2);
        //   }
        //   if (newRightS > 40){
        //     newRightS = 0;
        //     knobRightS.write(0);
        //   }
        //   positionRightS = newRightS;
        //   shape = positionRightS;
        // }
        // for (int i=0;i<2;i++){
        //   // update the ResponsiveAnalogRead object every loop
        //   analog[i].update();
        //   if (analog[i].hasChanged()) {
        //     val = analog[i].getValue();
        //     if(i == 0){
        //       color = val << 7;
        //     }
        //     if(i == 1){
        //       timbre = val << 7;
        //     }
        //   }
        // }
        //////////////////////////////////

        //// for teensy 3.2
        for (int i=0;i<3;i++){
          // update the ResponsiveAnalogRead object every loop
          analog[i].update();
          if (analog[i].hasChanged()) {
            val = analog[i].getValue();
            if(i == 0){
              shape = val/3;
            }
            if(i == 1){
              color = val << 7;
            }
            if(i == 2){
              timbre = val << 7;
            }
          }
        }
        //////////////////////////////////

        // Set the pin low to mark the end of rendering and processing
        //digitalWriteFast(13,LOW);
        // Waits until the buffer is ready to render again
        wait = 1;
        while(wait);
    }
}
