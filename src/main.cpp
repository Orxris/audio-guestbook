/*
 *            Audio Guestbook
 *        
 *        Author: Jonathan Pennell
 *         Board: Teensy 4.0
 *
 */


/* This can be removed when converting the file to an Arduino IDE .ino file. */
#include <Arduino.h>

/* These imports need to stay, even after converting to a .ino file */
#include <Bounce.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <TimeLib.h>


/* Define pins used by the teensy audio shield.
 * Define the SD Card Pins as per: 
 * https://www.pjrc.com/teensy/card10a_rev1_web.png
 */
#define SDCARD_CS_PIN 10
#define SDCARD_MOSI_PIN 7
#define SDCARD_SCK_PIN 14

/* Define the LED Pins
 * These will differ based on your own setup, I have chosen to use pins 2 and 3
 * for absolutely no reason. Change at your own will
 */
#define SETUP_LED 3
#define RECORDING_LED 2

/* Define the required Input/Playback pins
 * These will differ based on your own setup, I have chosen to use pins 2 and 3
 * for absolutely no reason. Change at your own will
 */
#define HOOK_PIN 0
#define PLAYBACK_BUTTON_PIN 1

/* Use these to easily swap between RISING edge and FALLING edge.
 * For example, if your phone is recording audio when the handset
 * is down, and stops recording when you lift it then you'll need
 * to swap the values for the macros HANDSET_UP and HANDSET_DOWN.
 */
#define HANDSET_UP     buttonRecord.fallingEdge()
#define HANDSET_DOWN   buttonRecord.risingEdge()


/* AUDIO GLOBALS */
AudioSynthWaveform    sine_wave;
AudioInputI2S         I2S_input;

AudioRecordQueue      mic_audio_queue;
AudioMixer4           mixer;
AudioOutputI2S        I2S_output;

AudioConnection       patchCord1(sine_wave, 0, mixer, 0);
AudioConnection       patchCord4(mixer, 0, I2S_output, 0);
AudioConnection       patchCord5(I2S_input, 0, mic_audio_queue,0);
AudioControlSGTL5000  sgt15000;

/* I've got no idea what the stuff above is doing
 * so it's best to not touch it unless you do...
 */ 


File *file;
File root_dir = SD.open("/");

/* Bounce helps us handle 'chatter' or 'bounce', where a single button press
 * produces multiple 'presses' milliseconds apart.
 * https://www.pjrc.com/teensy/td_libs_Bounce.html
 */
Bounce buttonRecord = Bounce(HOOK_PIN, 80);
Bounce buttonPlay = Bounce(PLAYBACK_BUTTON_PIN, 40);

enum Mode
{
  Initialising,
  Ready,
  Prompting,
  Recording,
  Playing,
  Error
};
Mode mode = Mode::Initialising;


void
play_error_tone(int pattern[4])
{
  /* pattern is an array of 0s and 1s, where a 1 represents a long beep
   * and 0 a short beep.
   */
  AudioSynthWaveform wave_form;
  for (int i=0; i<4;i++)
  {
    wave_form.amplitude(0.9);
    delay(250 * (pattern[i] + 1));
    wave_form.amplitude(0);
    delay(250);
  }
}


void
error(int code)
{
  /*
   * 100: General Errors
   * 200: 
   * 300: 
   * 400: SD Card Errors
   */

  /* Full SD Card */
  if (code == 401)
  {
    int beep_sequence[] = {1,0,0,1};
    play_error_tone(beep_sequence);
  }
}


/*
 * Prints the root directory of the SD card to the serial output.
 * For reasons unknown, adding this function in numerous places also
 * fixed an issue where the final audio file was being truncated to 0 seconds.
 */
void
printDirectory(File dir)
{
  char filename[15];
  File current_file;
  for (uint8_t i=0; i<9999;i++) {
    snprintf(filename, 11, " %05d.RAW", i);
    if (SD.exists(filename)) {
      current_file = SD.open(filename);
      Serial.printf("File: %s, Size: %d\n", filename, current_file.size());
    }
    else {
      break;
    }
  }
}


void
setup_pins ()
{
  /* SETUP_LED pin is on when phone is on and setup succeeds */
  pinMode(SETUP_LED, OUTPUT);

  /* RECORDING_LED is on when phone is recording */
  pinMode(RECORDING_LED, OUTPUT);

  /* HOOK_PIN is the headset up/down */
  pinMode(HOOK_PIN, INPUT_PULLUP);

  /* Setting LED pins to off */
  digitalWrite(SETUP_LED, LOW);
  digitalWrite(RECORDING_LED, LOW);
}


void
setup_sd_card ()
{
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);

  /* Keep attempting to setup SD card until power goes out or it is inserted. */
  while (!(SD.begin(SDCARD_CS_PIN))) 
  {
    Serial.println("Unable to access the SD Card");

    /* Flash the Setup LED to indicate there's an issue with the phone. */
    digitalWrite(SETUP_LED, HIGH);
    delay(500);
    digitalWrite(SETUP_LED, LOW);
    delay(500);
  }
}


void
setup_sgt15000 (AudioControlSGTL5000 *audio_codec) 
{
  audio_codec->enable();
  audio_codec->inputSelect(AUDIO_INPUT_MIC);

  /* Increase this if the audio as a whole is too quiet */
  audio_codec->volume(0.5);

  /* Lower this if you're struggling to hear over background noise */
  audio_codec->micGain(5);
}


void
beep (AudioSynthWaveform *wave_form)
{
  wave_form->amplitude(0.9);
  Serial.println("Waiting 250...");
  delay(250);
  Serial.println("Setting Amplitude 0...");
  wave_form->amplitude(0);
  Serial.println("Waiting 250...");
  delay(250);
}


void
play_start_tone(AudioSynthWaveform *wave_form)
{
  for (int i=0;i<4;i++) {
    if (i == 3 || i == 7) {
      wave_form->frequency(880);
    }
    else {
      wave_form->frequency(440);
    }
    beep(wave_form);
  }
}

void
play_end_tone(AudioSynthWaveform *wave_form)
{
  for (int i=0;i<4;i++) {
    if (i == 3 || i == 7) {
      wave_form->frequency(440);
    }
    else {
      wave_form->frequency(880);
    }
    beep(wave_form);
  }
}


File
*get_next_file()
{
  char filename[15];
  
  Serial.println("Root Dir at time of starting recording.");
  printDirectory(root_dir);

  for (uint8_t i=0; i<9999;i++) {
    snprintf(filename, 11, " %05d.RAW", i);
    if (!SD.exists(filename)) {
      break;
    }
  }

  File *next_file = &SD.open(filename);

  if (file == NULL) {
    Serial.println("Couldn't open a file to record!");
    return;
  }

  return next_file;
}


void
wait(unsigned int milliseconds)
{
  elapsedMillis msec = 0;

  while (msec <= milliseconds) {
    buttonRecord.update();
    buttonPlay.update();
    if (HANDSET_DOWN) {
      Serial.println("Button (pin 0) Press");
      return;
    }
    /*if (buttonPlay.fallingEdge()) {
     *  Serial.println("Button (pin 1) Press");
     * }
     */    
    if (HANDSET_UP) {
      Serial.println("Button (pin 0) Release");
      return;
    }
    /*if (buttonPlay.risingEdge()) {
     *  Serial.println("Button (pin 1) Release");
     *}
     */
    }
}


File
*start_recording(File *file)
{
  Serial.printf("Recording to %s.\n", file->name());
  Serial.println("Changing Mode to Recording...");
  mode = Mode::Recording;
  digitalWrite(RECORDING_LED, HIGH);
  mic_audio_queue.begin();
}


void
continue_recording(File *file)
{
  if (mic_audio_queue.available() >= 16) {
    /* Serial.println("mic_audio_queue.Available >= 2"); */
    byte buffer[512];

    /* TODO: Find out why this is split into two 256 chunks instead of one
     * 512 chunk.
     */
    memcpy(buffer, mic_audio_queue.readBuffer(), 256);
    mic_audio_queue.freeBuffer();
    memcpy(buffer+256, mic_audio_queue.readBuffer(), 256);
    mic_audio_queue.freeBuffer();

    file->write(buffer, sizeof(buffer));
  }
}


void
empty_queue(File *file)
{
  while (mic_audio_queue.available() > 0) {
    file->write(mic_audio_queue.readBuffer(), 256);
    mic_audio_queue.freeBuffer();
  }
}


/* This function does a few things:
  1. Ends the audio recording queue.
  2. Saves the remaining data in the queue to the file.
  3. Turns off the recording light.
*/
void
stop_recording(File *file)
{
  Serial.println("Root Dir at time of stopping recording");
  printDirectory(root_dir);

  mic_audio_queue.end();
  
  Serial.printf("Bytes remaining in Queue: %d", mic_audio_queue.available());
  empty_queue(file);
  Serial.println("Writing Finished.");
  
  file->close();
  Serial.println("File Closed.");
  
  Serial.println("Deleting File Object.");
  delete &file;

  Serial.println("Setting Recording Light Off.");
  digitalWrite(RECORDING_LED, LOW);

  Serial.println("Changing Mode to Ready...");
  mode = Mode::Ready;
  
  Serial.println("Root Dir at time of file closed.");
  printDirectory(root_dir);

}


/* Unused function, a rewrite of the three seperate recording functions.
 * This implementation stays 'in' the record_audio function until the handset
 * is down, rather than going back to the main loop.
 */
void
record_audio(File *file, AudioRecordQueue *audio_queue)
{

  byte buffer[512];

  audio_queue->begin();

  while (!HANDSET_DOWN)
  {
    if (audio_queue->available() >= 16)
    {
      memcpy(buffer, audio_queue->readBuffer(), 512);
      audio_queue->freeBuffer();

      file->write(buffer, sizeof(buffer));
    }

    buttonRecord.update();

  }

  audio_queue->end();

  while (audio_queue->available() > 0)
  {
    memcpy(buffer, audio_queue->readBuffer(), 256);
    mic_audio_queue.freeBuffer();

    file->write(buffer, sizeof(buffer));
  }

}


void
setup ()
{

  Serial.begin(9600);
  Serial.println(__FILE__ __DATE__);
 
  Serial.println("Setting up Pins...");
  setup_pins();
  Serial.println("Set up Pins.");

  Serial.println("Setting AudioMemory...");
  AudioMemory(60);
  Serial.println("Setting AudioMemory Finished.");

  Serial.println("Setting sgt15000...");
  setup_sgt15000(&sgt15000);
  Serial.println("Setting sgt15000 finished.");

  Serial.println("Setting SD Card stuff...");
  setup_sd_card();
  Serial.println("Setting SD Card stuff Finished.");

  Serial.println("Setting Mode to Ready...");
  mode = Mode::Ready;
  
  digitalWrite(SETUP_LED, HIGH);
  Serial.println("Finished Setup.");
}


void
loop()
{

  buttonRecord.update();
  buttonPlay.update();

  switch(mode) {

    case Mode::Initialising:
      Serial.println("Current Mode: Initialising...");
      Serial.println("Teensy is in main loop, initialisation should have finished.");
      Serial.println("Something unexpected has happened.");
      break;

    case Mode::Ready:
      Serial.println("Waiting for handset to be lifted...");

      if (HANDSET_UP) {
        Serial.println("Root Dir at time of printing.");
        printDirectory(root_dir);
        
        Serial.println("Handset Lifted");
        Serial.println("Changing Mode to Prompting...");
        mode = Mode::Prompting;
      }
      break;

    case Mode::Prompting:
      wait(100);
      Serial.println("Starting Recording");
      file = get_next_file();
      play_start_tone(&sine_wave);
      start_recording(file);
      break;

    case Mode::Recording:
      /* Swapped to rising edge */
      if (HANDSET_DOWN) {
        Serial.println("Stopped Recording");
        stop_recording(file);
        play_end_tone(&sine_wave);
      }
      else {
        continue_recording(file);
      }
      break;

    case Mode::Playing:
      break;
  }
}