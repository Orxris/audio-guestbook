/*
 *
 *
 *
 */


#include <Bounce.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <TimeLib.h>


// Define pins used by the teensy audio shield.
// Define the SD Card Pins
#define SDCARD_CS_PIN 10
#define SDCARD_MOSI_PIN 7
#define SDCARD_SCK_PIN 14

// Define the LED Pins
#define SETUP_LED 3
#define RECORDING_LED 2

// Define the required Input/Playback pins
#define HOOK_PIN 0
#define PLAYBACK_BUTTON_PIN 1

/* Use these to easily swap between RISING edge and FALLING edge.
 * For example, if your phone is recording audio when the handset
 * is down, and stops recording when you lift it then you'll need
 * to swap the values for the macros HANDSET_UP and HANDSET_DOWN.
 */
#define HANDSET_UP     buttonRecord.fallingEdge()
#define HANDSET_DOWN   buttonRecord.risingEdge()


// GLOBALS
AudioSynthWaveform 	  waveform1;
AudioInputI2S 		    i2s2;
AudioPlaySdRaw		    playRaw1;
AudioPlaySdWav		    playWav1;

AudioRecordQueue	    queue1;
AudioMixer4		        mixer;
AudioOutputI2S		    i2s1;

AudioConnection       patchCord1(waveform1, 0, mixer, 0);
AudioConnection       patchCord2(playRaw1, 0, mixer, 1);
AudioConnection       patchCord3(playWav1, 0, mixer, 1);
AudioConnection       patchCord4(mixer, 0, i2s1, 0);
AudioConnection       patchCord5(i2s2, 0, queue1,0);
AudioControlSGTL5000  sgt15000_1;

/* I've got no idea what the stuff above is doing
 * so it's best to not touch it unless you do...
 */ 


// Moved char filename[15] to startRecording()
File frec;
File root_dir = SD.open("/");

Bounce buttonRecord = Bounce(HOOK_PIN, 80);
Bounce buttonPlay = Bounce(PLAYBACK_BUTTON_PIN, 40);

enum Mode {Initialising, Ready, Prompting, Recording, Playing};
Mode mode = Mode::Initialising;


/*
 *
 */
void
printDirectory(File dir, int numTabs)
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
  pinMode(SETUP_LED, OUTPUT);
  pinMode(RECORDING_LED, OUTPUT);
	pinMode(HOOK_PIN, INPUT_PULLUP);
	pinMode(PLAYBACK_BUTTON_PIN, INPUT_PULLUP);

  digitalWrite(SETUP_LED, LOW);
  digitalWrite(RECORDING_LED, LOW);
}


void
setup_sd_card ()
{
	SPI.setMOSI(SDCARD_MOSI_PIN);
	SPI.setSCK(SDCARD_SCK_PIN);
	while (!(SD.begin(SDCARD_CS_PIN))) 
  {
		Serial.println("Unable to access the SD Card");
    digitalWrite(SETUP_LED, HIGH);
		delay(500);
    digitalWrite(SETUP_LED, LOW);
    delay(500);
	}
}


void
setup_sgt15000 () 
{
	sgt15000_1.enable();
	sgt15000_1.inputSelect(AUDIO_INPUT_MIC);
	sgt15000_1.volume(0.5);
	sgt15000_1.micGain(5);
}


void
play_welcome_tone()
{
  for (int i=0;i<4;i++) {
    if (i == 3 || i == 7) {
      waveform1.frequency(880);
    }
    else {
      waveform1.frequency(440);
    }
    waveform1.amplitude(0.9);
    Serial.println("Waiting 250...");
    delay(250);
    Serial.println("Setting Amplitude 0...");
    waveform1.amplitude(0);
    Serial.println("Waiting 250...");
    delay(250);
  }
}

void
play_end_tone()
{
  for (int i=0;i<4;i++) {
    if (i == 3 || i == 7) {
      waveform1.frequency(440);
    }
    else {
      waveform1.frequency(880);
    }
    waveform1.amplitude(0.9);
    Serial.println("Waiting 250...");
    delay(250);
    Serial.println("Setting Amplitude 0...");
    waveform1.amplitude(0);
    Serial.println("Waiting 250...");
    delay(250);
  }
}


char[15]
get_next_filename()
{
  char filename[15];
  Serial.println("Root Dir at time of starting recording.");
  printDirectory(root_dir, 0);
	for (uint8_t i=0; i<9999;i++) {
		snprintf(filename, 11, " %05d.RAW", i);
		if (!SD.exists(filename)) {
			break;
		}
	}
  return filename;
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
  setup_sgt15000();
  Serial.println("Setting sgt15000 finished.");

  Serial.println("Setting SD Card stuff...");
  setup_sd_card();
  Serial.println("Setting SD Card stuff Finished.");

  /* From the TimeLib Library,
   */
	setSyncProvider(getTeensy3Time);

	FsDateTime::setCallback(dateTime);

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
      Serial.println("Something unexpected has happened.
      break;

		case Mode::Ready:
			if (HANDSET_UP) {
        Serial.println("Root Dir at time of printing.");
        printDirectory(root_dir, 0);
				Serial.println("Handset Lifted");
				Serial.println("Changing Mode to Prompting...");
        mode = Mode::Prompting;
			}
			else if (buttonPlay.fallingEdge()) {
				playAllRecordings();
			}
			break;

		case Mode::Prompting:
			wait(100);
			Serial.println("Starting Recording");
      			startRecording();
			break;

		case Mode::Recording:
      // Swapped to rising edge
			if (HANDSET_DOWN) {
				Serial.println("Stopped Recording");
				stopRecording();
			  play_end_tone();
      }
      else {
				continueRecording();
			}
			break;

		case Mode::Playing:
			break;
	}
}

void
startRecording()
{
  char[15] filename = get_next_filename();

	frec = SD.open(filename, FILE_WRITE);
	
  if (!frec) {
		Serial.println("Couldn't open a file to record!");
    return;
  }

  Serial.printf("Recording to %s.\n", filename);
  Serial.println("Changing Mode to Recording...");
  mode = Mode::Recording;
  digitalWrite(RECORDING_LED, HIGH);
  queue1.begin();
}


void
continueRecording()
{
	if (queue1.available() >= 16) {
    // Serial.println("Queue1.Available >= 2");
		byte buffer[512];
		memcpy(buffer, queue1.readBuffer(), 256);
		queue1.freeBuffer();
		memcpy(buffer+256, queue1.readBuffer(), 256);
		queue1.freeBuffer();
		frec.write(buffer, sizeof(buffer));
    // Serial.printf("\rFile Size: %d\r", frec.size());
	}
}

void
stopRecording()
{
  Serial.println("Root Dir at time of stopping recording");
  printDirectory(root_dir, 0);
	queue1.end();
	while (queue1.available() > 0) {
		frec.write((byte*)queue1.readBuffer(), 256);
		queue1.freeBuffer();
	}
  Serial.println("Writing Finished.");
	frec.close();
  Serial.println("File Closed.");
  Serial.println("Changing Mode to Ready...");
	mode = Mode::Ready;
  Serial.println("Setting Recording Light Off.");
  digitalWrite(RECORDING_LED, LOW);
  Serial.println("Root Dir at time of file closed.");
  printDirectory(root_dir, 0);
}



time_t
getTeensy3Time()
{
	return Teensy3Clock.get();
}

void
dateTime(uint16_t* date, uint16_t* time, uint8_t* ms10)
{
	*date = FS_DATE(year(), month(), day());
	*time = FS_TIME(hour(), minute(), second());
	*ms10 = second() & 1 ? 100 : 0;
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
		}
		//if (buttonPlay.fallingEdge()) {
		//	Serial.println("Button (pin 1) Press");
		//}
		if (HANDSET_UP) {
			Serial.println("Button (pin 0) Release");
		}
		//if (buttonPlay.risingEdge()) {
		//	Serial.println("Button (pin 1) Release");
		//}
	}
}

