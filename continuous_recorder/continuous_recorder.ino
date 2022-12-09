#include <SDHCI.h>
#include <Audio.h>
#include <Watchdog.h>
#define BAUDRATE 115200
/*
---------------------
Name: continuous_recoder.ino
Author: R. Okamoto
Started: 2022/12/9
Purpose: To continuously record audio using Sony Spresense
---------------------
Description:
This script is for making continuous audio recoder with Sony Spresense.
You can record a given length of audio and write it to .wav or .mp3 file.

You must install DSP files in your SD card before starting. Follow the instruction below.
https://developer.sony.com/develop/spresense/docs/arduino_tutorials_ja.html#_dsp_%E3%83%95%E3%82%A1%E3%82%A4%E3%83%AB%E3%81%AE%E3%82%A4%E3%83%B3%E3%82%B9%E3%83%88%E3%83%BC%E3%83%AB

Also you can learn how to attach a microphone to your Spresense with the document below.
https://developer.sony.com/develop/spresense/docs/hw_docs_ja.html

Sometime the recording preocess fails for some reason (e.g., audio library error, file writing wrror).
This script utilizes the Watchdog function of Spresense to avoid the whole process freezing after such errors.
The Watchdog monitors the recording loop and if something wrong happened (e.g., file writing error), it will kill the process and reset the hardware.
*/


/* Parameters
-----------------------------------------------------------------------------------------------*/
/* The audio codec. AS_CODECTYPE_MP3 or AS_CODECTYPE_WAV */
static const int32_t codec =  AS_CODECTYPE_WAV;
/* The recording time in sec. */
static const int32_t recording_time = 300;
/*
The gain of the microphone.
The range is 0 to 21 [dB] for analog microphones and -78.5 to 0 [dB] for digital microphones.
For an analog microphone, specify an integer value multiplied by 10 for input_gain, for example, 100 when setting 10 [dB].
For a digital microphone, specify an integer value multiplied by 100 for input_gain, for example -5 when setting -0.05 [dB].
*/
static const int32_t gain = 210;
/* Using digital mic? */
static const bool is_digital = false;
/* Mono or Stereo?. AS_CHANNEL_STEREO or AS_CHANNEL_STEREO */
static const int32_t channel = AS_CHANNEL_MONO;
/* 
The sumpling rate. MP3 supports 32kHz ( AS_SAMPLINGRATE_32000 ), 44.1kHz ( AS_SAMPLINGRATE_44100 ), 48kHz ( AS_SAMPLINGRATE_48000 ) 
WAV (maybe) supports 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 64000, 88200, 96000, 128000, 176400, 192000.
*/
static const int32_t sr = AS_SAMPLINGRATE_48000;
/* The output directory to save audio files */
String out_dir = "AUDIO";
/*---------------------------------------------------------------------------------------------*/


/* Parameters for WAV recording
-----------------------------------------------------------------------------------------------*/
static const uint8_t  recoding_bit_length = 16;
/*---------------------------------------------------------------------------------------------*/

int32_t recoding_size;
int buff_size;
String ext;
const int wd_time = (recording_time + 10) * 1000; // Watchdog time in ms. if the main loop took more than wd_time, the watchdog will reset the process.

SDClass theSD;
AudioClass *theAudio;

File myFile;

bool ErrEnd = false;

/**
 * @brief Audio attention callback
 *
 * When audio internal error occurc, this function will be called back.
 */

static void audio_attention_cb(const ErrorAttentionParam *atprm)
{
  puts("Attention!");
  
  if (atprm->error_code >= AS_ATTENTION_CODE_WARNING)
    {
      ErrEnd = true;
   }
}

/* To initialize
-----------------------------------------------------------------------------------------------*/
void init()
{
  /* Initialize the SD card
  ---------------------------------------------*/
  Serial.begin(BAUDRATE);
  while (!theSD.begin())
  {
    /* wait until SD card is mounted. */
    Serial.println("Insert SD card.");
  }

  /* Create outout directory if needed.
  ---------------------------------------------*/  
  if (theSD.exists(out_dir) == false)
  {
    Serial.println(String("Creating " + out_dir));
    theSD.mkdir(out_dir);    
  }

    /* Setting audio parameters according to the audio codec
  ---------------------------------------------*/
  if (codec == AS_CODECTYPE_MP3) 
  {
    int32_t recoding_bitrate = 96000; // recording bit rate. 96kbps fixed.
    /* Bytes per second */
    int32_t recoding_byte_per_second = (recoding_bitrate / 8);
    /* Total recording size */
    recoding_size = recoding_byte_per_second * recording_time;
    buff_size = 8000; // Buffer size, default is 160000, for HiRes WAV recording
    ext = ".mp3";
  }
  else if (codec == AS_CODECTYPE_WAV) 
  {
    /* Bytes per second */
    static const int32_t recoding_byte_per_second = sr *
                                                    channel *
                                                    recoding_bit_length / 8;
    /* Total recording size */
    recoding_size = recoding_byte_per_second * recording_time;
    buff_size = 80000; // Buffer size, default is 160000, for HiRes WAV recording  
    ext = ".wav";
  }
  else 
  {
    Serial.println("Uknown codec detected! Use 'AS_CODECTYPE_MP3' or 'AS_CODECTYPE_WAV' instead!");
  }

    /* Initialize the audio library
  -----------------------------------------------------------------------------------------------*/
  theAudio = AudioClass::getInstance();
  theAudio->begin(audio_attention_cb);
  Serial.println("Initializing the Audio Library");

  theAudio->setRecorderMode(
    AS_SETRECDR_STS_INPUTDEVICE_MIC,
    gain,
    buff_size,
    is_digital
  );

  theAudio->initRecorder(
    codec, 
    "/mnt/sd0/BIN", // The location of the DSP files. Use /mnt/spif/BIN instead if you save DSP files in the SPI-Flash.
    sr,
    channel
  );
  Serial.println("Initialized the Recorder!");
}

/* To exit recording 
-----------------------------------------------------------------------------------------------*/
void exit_recording()
{
  theAudio->closeOutputFile(myFile);
  myFile.close();
  
  theAudio->setReadyMode();
  theAudio->end();

  Serial.println("Something wrong happened. End Recording.");
}

/* To record an audio with a given file name
-----------------------------------------------------------------------------------------------*/
bool rec(String file_name)
{
  /* Open file for data write on SD card
  ---------------------------------------------*/
  /*
  if (theSD.exists(file_name))
  {
    Serial.println(String("Remove existing file ") + file_name);
    theSD.remove(file_name);
  }
  */  
  myFile = theSD.open(file_name, FILE_WRITE);
  
  /* Verify file open
  ---------------------------------------------*/
  if (!myFile)
    {
      Serial.println("File open error");
      return false;
    }

  Serial.println(String("Open! ") + file_name);

  /* Start recording
  ---------------------------------------------*/
  if (codec == AS_CODECTYPE_WAV) // If the audio codec is WAV, write header before starting.
  {
    theAudio->writeWavHeader(myFile);    
  }
  theAudio->startRecorder();
  Serial.println("Recording Start!");

  /* Record until the audio capture reaches recording_size
  ---------------------------------------------*/  
  err_t err;
  while (theAudio->getRecordingSize() < recoding_size)
  {
    err = theAudio->readFrames(myFile);
    
    if (err != AUDIOLIB_ECODE_OK) // If something wrong happened, stop recording
    {
      printf("File End! =%d\n",err);
      theAudio->stopRecorder();
      exit_recording();
      return false;
    }

    if (ErrEnd)  // If audio_attention_cb returns errors, stop recording
    {
      printf("Error End\n");
      theAudio->stopRecorder();
      exit_recording();
      return false;
    }
  }

  /* Stop recording
  ---------------------------------------------*/  
  Serial.println("Recording finished!");
  theAudio->stopRecorder();
  sleep(1);
  theAudio->closeOutputFile(myFile);
  myFile.close();
  return true;
}

void(* resetFunc) (void) = 0; //declare reset function @ address 0

/* Main functions
-----------------------------------------------------------------------------------------------*/
void setup()
{
  Watchdog.begin();
  init();
  Watchdog.start(wd_time);
}

int i = 1;
bool rec_ok = true;
String file_name;

void loop()
{
  file_name = out_dir + String("/audio_") + String(i) + ext;
  i ++;  
  if (theSD.exists(file_name))
  {
    Serial.println(String("Skipping ") + file_name + String(" because it exists"));
    return;
  }
  rec_ok = rec(file_name);
  if (!rec_ok)
  {
    Serial.println("Failed recording! Restarting...");
    delay(wd_time);
  }  
  Watchdog.kick();
}