/*************************************************** 
 Trellome Midi Sequencer
  George Kuetemeyer
  gk@pobox.com
  May 7, 2014
  
  Trellome was inspired by a variety of monome-like devices.
  Check out: monome.org for more info.
  
  Trellome is laid out as an eight by eight matrix. The cells in the
  top six rows provide six note options for an eight step sequencer.
  The first row buttons turn on random octave shifts for each step.
  The second row buttons turn on an echo effect.

  Designed specifically to work with the Adafruit Trellis w/HT16K33
  ----> https://www.adafruit.com/products/1616
  ----> https://www.adafruit.com/products/1611

  These displays use I2C to communicate, 2 pins are required to  
  interface
  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Trellis library Written by Limor Fried/Ladyada for Adafruit Industries.  
  MIT license, all text above must be included in any redistribution
 ****************************************************/

#include <Wire.h>
#include <Adafruit_Trellis.h>
#include <Adafruit_Trellis_XY.h>
#include <MIDI.h>
#include <Button.h>
#include <StackArray.h>

/***************************************************/

Adafruit_Trellis matrix0 = Adafruit_Trellis();
Adafruit_Trellis matrix1 = Adafruit_Trellis();
Adafruit_Trellis matrix2 = Adafruit_Trellis();
Adafruit_Trellis matrix3 = Adafruit_Trellis();

Adafruit_TrellisSet trellis =  Adafruit_TrellisSet(&matrix0, &matrix1, &matrix2, &matrix3);

// set to however many you're working with here, up to 8
#define NUMTRELLIS 4
#define NKEYS (NUMTRELLIS * 16)
#define INTPIN 2

//-----------------------------------------------------------
Adafruit_Trellis_XY trellisXY = Adafruit_Trellis_XY();

byte xVal;
byte yVal;
byte xyTrellisID;

// store x/y array states -- eight bytes
byte EightByEight[8];

// sequencer step array
unsigned int seqStep[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };

// C Pentatonic Scale
// 5 notes plus octave fits trellis size pretty well.
// Obviously there are lots of options here!!
byte Scale[6] = { 48, 50, 53, 55, 57, 60 };

// non-Trellis button on Midi Shield
// using this button to start/stop sequencer
Button start_stop = Button(2, PULLUP);
byte start_stop_state = 0;

// midi channel
byte midi_channel = 1;

// stack for turning off played notes
StackArray <byte> playedNotes;

//-----------------------------------------------------------
void setup() { 
  MIDI.begin();
  //Serial.begin(9600);

  // define number of keys
  trellisXY.begin(NKEYS); 
  // set up x/y offsets
  trellisXY.setOffsets(0, 0, 0);
  trellisXY.setOffsets(1, 4, 0);
  trellisXY.setOffsets(2, 0, 4);
  trellisXY.setOffsets(3, 4, 4);
  
  // INT pin requires a pullup
  pinMode(INTPIN, INPUT);
  digitalWrite(INTPIN, HIGH);

  // initialize trellis.
  // NB: Your tile address order may differ, depending on how you've arranged
  // your tiles  
  trellis.begin(0x72, 0x73, 0x70, 0x71);
  // turn off Trellis LEDs
  for (byte i = 0; i < trellisXY.numKeys; i++) {
    trellis.clrLED(i);
  }
  // you can adjust brightness according to your needs/taste
  trellis.setBrightness(1);
  trellis.writeDisplay();  
}

//-----------------------------------------------------------
void loop() {
  if (trellis.readSwitches()) {
    // go through every button
    for (byte i=0; i < trellisXY.numKeys; i++) {
      if (trellis.justPressed(i)) {
        // button debounce
        delay(10);
        // get x/y coordinates
        xVal = trellisXY.getTrellisX(i);
        yVal = trellisXY.getTrellisY(i);
	// Alternate LED
	if (trellis.isLED(i)) {
	  trellis.clrLED(i);
          // update X/Y matrix
          bitWrite(EightByEight[yVal], xVal, 0);          
        }
	else {
	  trellis.setLED(i);
          // update X/Y matrix
          bitWrite(EightByEight[yVal], xVal, 1);
        }
         
      }
    }
    // display LED matrix
    trellis.writeDisplay();
  }
  else {
    // adjust tempo  
    delay(map(analogRead(0), 0, 1023, 50, 1000));
    // check start/stop
    if(start_stop.isPressed() == 1) {
      if(start_stop_state == 0) {
        start_stop_state = 1;
      }
      else {
        // stop playing
        start_stop_state = 0;
        // turn off all notes
        turn_off_notes(midi_channel);
      }
    }
    // play next notes
    if(start_stop_state == 1) {
      play_notes();
    }
  }
}

//-------------------------------------------------------------------------------
// play notes
void play_notes() {
  byte octave = 0;
  // update eight sequence step counters
  // (seqStep[0] used for current step
  // other counters support echo feature
  seqStep[0] = (seqStep[0] + 1) % 8;   // modulo operator rolls over variable
  seqStep[1] = (seqStep[0] - 1) % 8;
  seqStep[2] = (seqStep[1] - 1) % 8;
  seqStep[3] = (seqStep[2] - 1) % 8;
  seqStep[4] = (seqStep[3] - 1) % 8;
  seqStep[5] = (seqStep[4] - 1) % 8;
  seqStep[6] = (seqStep[5] - 1) % 8;
  seqStep[7] = (seqStep[6] - 1) % 8;
  
  // turn off previous notes in channel
  turn_off_notes(midi_channel);
  
  // get random octave for step note
  if(bitRead(EightByEight[0], seqStep[0]) == 1) {
    octave = 12 * random(0, 4);
  }
  
  // play current step notes
  for(byte y = 2; y < 8; y++) {  
    if(bitRead(EightByEight[y], seqStep[0]) == 1) {
      play_note(midi_channel, Scale[y - 2] + octave, 64);
    }
  }
  
  // play echo notes
  for(byte e = 0; e < 8; e++) {  
    if(bitRead(EightByEight[1], e) == 1) {
      for(byte y = 2; y < 8; y++) {
        if(bitRead(EightByEight[y], seqStep[e]) == 1) {          
          play_note(midi_channel, Scale[y - 2] + octave, 64);
        }
      }
    }
  }
  // display cursor for current step column
  display_cursor();
}

//-------------------------------------------------------------------------------
void play_note(byte channel, byte pitch, byte volume) {
  playedNotes.push(volume);
  playedNotes.push(pitch);
  MIDI.sendNoteOn(pitch, volume, channel);
}

//-------------------------------------------------------------------------------
void turn_off_notes(byte channel) {
  byte pitch;
  byte volume;
  while(!playedNotes.isEmpty()) {
    pitch  = playedNotes.pop();
    volume = playedNotes.pop();
    MIDI.sendNoteOff(pitch, volume, channel);
  }
}

//-------------------------------------------------------------------------------
void display_cursor() {
  for(byte c = 0; c < 2; c++) {
    for(byte y = 0; y < 8; y++) {
      if(bitRead(EightByEight[y], seqStep[0]) == 0) {
        trellis.setLED(trellisXY.getTrellisId(seqStep[0], y));
        bitWrite(EightByEight[y], seqStep[0], 1);
      }
      else {
        trellis.clrLED(trellisXY.getTrellisId(seqStep[0], y));
        bitWrite(EightByEight[y], seqStep[0], 0);
      }
    }
    trellis.writeDisplay();
  }
}
