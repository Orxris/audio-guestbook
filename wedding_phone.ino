


#include <Bounce.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <TimeLib.h>


// Define pins used by the teensy audio shield.

#define SDCARD_CS_PIN 10
#define SDCARD_MOSI_PIN 7
#define SDCARD_SCK_PIN 14

#define SETUP_LED 5
#define RECORDING_LED 6

// INPUTS
#define HOOK_PIN 0
#define PLAYBACK_BUTTON_PIN 1

// GLOBALS
AudioSynthWaveform 	waveform1;
AudioInputI2S 		i2s2;
AudioPlaySdRaw		playRaw1;
AudioPlaySdWav		playWav1;

AudioRecordQueue	queue1;
AudioMixer4		mixer;
AudioOutputI2S		i2s1;

AudioConnection patchCord1(waveform1, 0, mixer, 0);
AudioConnection patchCord2(playRaw1, 0, mixer, 1);
AudioConnection patchCord3(playWav1, 0, mixer, 1);
AudioConnection patchCord4(mixer, 0, i2s1, 0);
AudioConnection patchCord5(i2s2, 0, queue1,0);


//AudioConnection  patchCord10(i2s2, 0, i2s1, 0);
//AudioConnection  patchCord20(i2s2, 0, i2s1, 1);

AudioControlSGTL5000 sgt15000_1;

char filename[15];
File frec;
File root_dir = SD.open("/");

Bounce buttonRecord = Bounce(HOOK_PIN, 80);
Bounce buttonPlay = Bounce(PLAYBACK_BUTTON_PIN, 40);

enum Mode {Initialising, Ready, Prompting, Recording, Playing};
Mode mode = Mode::Initialising;


void printDirectory(File dir, int numTabs) {
  char existing_file[15];
  File current_file;
  for (uint8_t i=0; i<9999;i++) {
		snprintf(existing_file, 11, " %05d.RAW", i);
		if (SD.exists(existing_file)) {
			current_file = SD.open(existing_file);
      Serial.printf("File: %s, Size: %d\n", existing_file, current_file.size());
		}
    else {
      break;
    }
	}
}


void setup () {

  digitalWrite(SETUP_LED, LOW);

  Serial.begin(9600);
	Serial.println(__FILE__ __DATE__);

  Serial.println("Setting pinModes...");
	pinMode(HOOK_PIN, INPUT_PULLUP);
	pinMode(PLAYBACK_BUTTON_PIN, INPUT_PULLUP);
  Serial.println("Setting pinModes...");

  Serial.println("Setting AudioMemory...");
	AudioMemory(60);
  Serial.println("Setting AudioMemory Finished.");

  Serial.println("Setting sgt15000...");
	sgt15000_1.enable();
	sgt15000_1.inputSelect(AUDIO_INPUT_MIC);
	sgt15000_1.volume(0.5);
  Serial.println("Setting sgt15000 finished.");

  Serial.println("Playing Startup Waveform...");
  Serial.println("Playing SINEWAVE...");
	waveform1.begin(WAVEFORM_SINE);
  Serial.println("Setting Frequency...");

  Serial.println("Setting Amplitude...");
	for (int i=0; i<8;i++) {
    if (i == 3 || i == 7) {
      waveform1.frequency(880);
    }
    else {
      waveform1.frequency(440);
    }
    waveform1.amplitude(0.9);
    // Serial.println("Waiting 250...");
	  delay(250);
    // Serial.println("Setting Amplitude 0...");
	  waveform1.amplitude(0);
    // Serial.println("Waiting 250...");
	  delay(250);
  }
  
  Serial.println("Playing Waveform finished.");

  Serial.println("Setting SD Card stuff...");
	SPI.setMOSI(SDCARD_MOSI_PIN);
	SPI.setSCK(SDCARD_SCK_PIN);
	if (!(SD.begin(SDCARD_CS_PIN))) {
			while(1) {
			Serial.println("Unable to access the SD Card");
      digitalWrite(LED_PIN, HIGH);
			delay(500);
      digitalWrite(LED_PIN, LOW);
      delay(500);
			}
	}
  Serial.println("Setting SD Card stuff Finished.");

  Serial.println("Setting micGain to 15");
  // set to 5 from 15 as per github code
	sgt15000_1.micGain(5);
  Serial.println("Setting micGain Finished.");

	setSyncProvider(getTeensy3Time);

	FsDateTime::setCallback(dateTime);

  Serial.println("Setting Mode to Ready...");
	mode = Mode::Ready;
  Serial.println("Finished Setup.");
}


void loop() {

	buttonRecord.update();
	buttonPlay.update();

	switch(mode) {

    case Mode::Initialising:
      Serial.println("Current Mode: Initialising");
      break;

		case Mode::Ready:
      
      //Serial.print("\rReady...");
			// if the handset button is off when handset it up, swap this RisignEdge for
			// the Falling Edge

      // swapped to Falling Edge
			if (buttonRecord.fallingEdge()) {
        Serial.println("Root Dir at time of printing.");
        printDirectory(root_dir, 0);
				Serial.println("Handset Lifted");
				Serial.println("Changing Mode to Prompting...");
        mode = Mode::Prompting;
			}
      // swaped to rising edge
			else if (buttonPlay.fallingEdge()) {
				playAllRecordings();
			}
			break;

		case Mode::Prompting:

			wait(1000);
			/* 
      // I don't need this section, as I am not playing a greeting.
      playWav1.play("greeting.wav");
			while (playWav1.isPlaying()) {
				buttonRecord.update();
        // Swapped to rising edge
				if (buttonRecord.risingEdge()) {
					playWav1.stop();
          Serial.println("Changing Mode to Ready...");
					mode = Mode::Ready;
					return;
				}
			}
      */
			Serial.println("Starting Recording");
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
			startRecording();
			break;

		case Mode::Recording:

      // Swapped to rising edge
			if (buttonRecord.risingEdge()) {
				Serial.println("\nStopped Recording");
				stopRecording();
				for (int i=0;i<4;i++) {
          if (i == 3 || i == 7) {
            waveform1.frequency(440);
          }
          else {
            waveform1.frequency(880);
          }
          waveform1.amplitude(0.9);
          delay(250);
          waveform1.amplitude(0);
          delay(250);
        }
			}
			else {
				continueRecording();
			}
			break;

		case Mode::Playing:
      
			break;
	}
}

void startRecording() {
  Serial.println("Root Dir at time of starting recording.");
  printDirectory(root_dir, 0);
	for (uint8_t i=0; i<9999;i++) {
		snprintf(filename, 11, " %05d.RAW", i);
		if (!SD.exists(filename)) {
			break;
		}
	}

	frec = SD.open(filename, FILE_WRITE);
	if (frec) {
		Serial.println("Recording to");
		Serial.println(filename);
		queue1.begin();
    Serial.println("Changing Mode to Recording...");
		mode = Mode::Recording;
	}
	else {
		Serial.println("Couldn't open a file to record!");
	}
}

void continueRecording() {
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

void stopRecording() {
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
  Serial.println("Root Dir at time of file closed.");
  printDirectory(root_dir, 0);
}

void playAllRecordings() {
	File dir = SD.open("/");
	File entry = dir.openNextFile();

  Serial.println("Playing All Recordings...");

  while (true) {
		
		if (!entry) {
			entry.close();
			break;
		}

		int8_t len = strlen(entry.name());
    const char* uppercase_filename_without_ext = entry.name() + (len-4);
    const char* filename_without_ext = strlwr((char*)uppercase_filename_without_ext);
		if (strstr(filename_without_ext, (const char*)".wav")) {
			Serial.print("Now Playing ");
			Serial.println(entry.name());
		}

		waveform1.amplitude(0.5);
		wait(250);
		waveform1.amplitude(0);
		playRaw1.play(entry.name());
    Serial.println("Changing Mode to Playing...");
		mode = Mode::Playing;
	}
	entry.close();

	while (playRaw1.isPlaying()) {
		buttonPlay.update();
		buttonRecord.update();
    // Swappng record button to rising edge
		if (buttonPlay.risingEdge() || buttonRecord.risingEdge()) {
				playRaw1.stop();
        Serial.println("Changing Mode to Ready...");
				mode = Mode::Ready;
				return;
				}
	}
  Serial.println("Changing Mode to Ready...");
	mode = Mode::Ready;
}


time_t getTeensy3Time() {
	return Teensy3Clock.get();
}

void dateTime(uint16_t* date, uint16_t* time, uint8_t* ms10) {
	*date = FS_DATE(year(), month(), day());
	*time = FS_TIME(hour(), minute(), second());
	*ms10 = second() & 1 ? 100 : 0;
}


void wait(unsigned int milliseconds) {
	elapsedMillis msec = 0;

	while (msec <= milliseconds) {
		buttonRecord.update();
		buttonPlay.update();
		if (buttonRecord.risingEdge()) {
			Serial.println("Button (pin 0) Press");
		}
		//if (buttonPlay.fallingEdge()) {
		//	Serial.println("Button (pin 1) Press");
		//}
		if (buttonRecord.fallingEdge()) {
			Serial.println("Button (pin 0) Release");
		}
		//if (buttonPlay.risingEdge()) {
		//	Serial.println("Button (pin 1) Release");
		//}
	}
}

