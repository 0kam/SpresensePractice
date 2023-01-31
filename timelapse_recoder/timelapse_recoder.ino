/*
---------------------
Name: timelapse_recoder.ino
Author: R. Okamoto
Started: 2023/1/30
Purpose: To perform time-lapse audio recordings using Sony Spresense
---------------------
Description:
This script is for making time-lapse audio recoder with Sony Spresense.
You can record a given length of audio and write it to .wav or .mp3 file.
This script performs an interval recording with a given schedule (the starting time, ending time, and interval).

The overview of the recording process is:
1. Recieving GNSS signals and set RTC time (Only at the first boot).
2. Recording and saving an audio.
3. Calculate the next booting time.
4. Going to deep sleep.

You must install DSP files in your SD card before starting. Follow the instruction below.
https://developer.sony.com/develop/spresense/docs/arduino_tutorials_ja.html#_dsp_%E3%83%95%E3%82%A1%E3%82%A4%E3%83%AB%E3%81%AE%E3%82%A4%E3%83%B3%E3%82%B9%E3%83%88%E3%83%BC%E3%83%AB

Also you can learn how to attach a microphone to your Spresense with the document below.
https://developer.sony.com/develop/spresense/docs/hw_docs_ja.html

Please make sure that the memory size is set to be 768 kb (Tools -> Memory in ArduinoIDE)

Sometime the recording preocess fails for some reason (e.g., audio library error, file writing wrror).
This script utilizes the Watchdog function of Spresense to avoid the whole process freezing after such errors.
The Watchdog monitors the recording loop and if something wrong happened (e.g., file writing error), it will kill the process and reset the hardware.
*/

// For timelapse operation
#include <GNSS.h>
#include <LowPower.h>
#include <RTC.h>
typedef struct { 
 int start_h;
 int end_h;
 int m[3];
} schedule;

// For audio recording
#include <SDHCI.h>
#include <Audio.h>
#include <Watchdog.h>
#define BAUDRATE 115200

/* Time-lapse Parameters
-----------------------------------------------------------------------------------------------*/
schedule s = {1, 24, {0, 30}}; // {開始時刻, 終了時刻, {毎時何分に起動するか(複数書く場合は小さい順)}}
/*---------------------------------------------------------------------------------------------*/


/* Audio Parameters
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
static const int32_t sr = AS_SAMPLINGRATE_16000;
/* The output directory to save audio files */
String out_dir = "AUDIO";
/*---------------------------------------------------------------------------------------------*/


/* Parameters for WAV recording
-----------------------------------------------------------------------------------------------*/
static const uint8_t  recoding_bit_length = 16;
/*---------------------------------------------------------------------------------------------*/


/* For timelapse operation
-----------------------------------------------------------------------------------------------*/
const char* boot_cause_strings[] = {
  "Power On Reset with Power Supplied",
  "System WDT expired or Self Reboot",
  "Chip WDT expired",
  "WKUPL signal detected in deep sleep",
  "WKUPS signal detected in deep sleep",
  "RTC Alarm expired in deep sleep",
  "USB Connected in deep sleep",
  "Others in deep sleep",
  "SCU Interrupt detected in cold sleep",
  "RTC Alarm0 expired in cold sleep",
  "RTC Alarm1 expired in cold sleep",
  "RTC Alarm2 expired in cold sleep",
  "RTC Alarm Error occurred in cold sleep",
  "Unknown(13)",
  "Unknown(14)",
  "Unknown(15)",
  "GPIO detected in cold sleep",
  "GPIO detected in cold sleep",
  "GPIO detected in cold sleep",
  "GPIO detected in cold sleep",
  "GPIO detected in cold sleep",
  "GPIO detected in cold sleep",
  "GPIO detected in cold sleep",
  "GPIO detected in cold sleep",
  "GPIO detected in cold sleep",
  "GPIO detected in cold sleep",
  "GPIO detected in cold sleep",
  "GPIO detected in cold sleep",
  "SEN_INT signal detected in cold sleep",
  "PMIC signal detected in cold sleep",
  "USB Disconnected in cold sleep",
  "USB Connected in cold sleep",
  "Power On Reset",
};

void printBootCause(bootcause_e bc)
{
  Serial.println("--------------------------------------------------");
  Serial.print("Boot Cause: ");
  Serial.print(boot_cause_strings[bc]);
  if ((COLD_GPIO_IRQ36 <= bc) && (bc <= COLD_GPIO_IRQ47)) {
    // Wakeup by GPIO
    int pin = LowPower.getWakeupPin(bc);
    Serial.print(" <- pin ");
    Serial.print(pin);
  }
  Serial.println();
  Serial.println("--------------------------------------------------");
}

SpGnss Gnss;
#define MY_TIMEZONE_IN_SECONDS (9 * 60 * 60) // JST

String printClock(RtcTime &rtc)
{
  char buf [13];
  sprintf(buf,
          "%04d%02d%02d_%02d%02d",
          rtc.year(), rtc.month(), rtc.day(),
          rtc.hour(), rtc.minute()
        );
  String str = String(buf);
  return str;
}

void setRTC()
{
  // Initialize RTC at first
  RTC.begin();

  // Initialize and start GNSS library
  int ret;
  ret = Gnss.begin();
  assert(ret == 0);
  
  Gnss.select(GPS); // GPS
  Gnss.select(GLONASS); // Glonass
  Gnss.select(QZ_L1CA); // QZSS  

  ret = Gnss.start();
  assert(ret == 0);

    // Wait for GNSS data
  int led_state = 0;
  while (true)
  {
    if (Gnss.waitUpdate()) {
      SpNavData  NavData;
  
      // Get the UTC time
      Gnss.getNavData(&NavData);
      SpGnssTime *time = &NavData.time;
  
      // Check if the acquired UTC time is accurate
      if (time->year >= 2000) {
        RtcTime now = RTC.getTime();
        // Convert SpGnssTime to RtcTime
        RtcTime gps(time->year, time->month, time->day,
                    time->hour, time->minute, time->sec, time->usec * 1000);
        // Set the time difference
        gps += MY_TIMEZONE_IN_SECONDS;
        int diff = now - gps;
        if (abs(diff) >= 1) {
          RTC.setTime(gps);
        }
        ledOff(LED0);
        break;        
      }      
    }
    Serial.println("Waiting for GNSS signals...");
    if (led_state == 0)
    {
      ledOn(LED0);
      led_state = 1;
    }
    else
    {
      ledOff(LED0);
      led_state = 0;
    }
  }
}

void setLowPower()
{
  bootcause_e bc = LowPower.bootCause();
  if ((bc == POR_SUPPLY) || (bc == POR_NORMAL)) {
    Serial.println("Example for RTC wakeup from deep sleep");
  } else {
    Serial.println("wakeup from deep sleep");
  }
  // Print the boot cause
  printBootCause(bc);
}

void findMinute(schedule s, int *next_minute, int *next_hour, RtcTime &rtc)
{
  bool find_m = false;
  int i;
  int m_len = sizeof(s.m) / sizeof(int);
  for (i = 0; i < m_len; i++)
  {
    Serial.println(s.m[i]); 
    if (s.m[i] > rtc.minute()){
      *next_minute = s.m[i];
      *next_hour = rtc.hour();
      find_m = true;
      break;
    }
    if (!find_m)
    {
      *next_minute = s.m[0];
      *next_hour = rtc.hour() + 1;
    }
  }
}

int getNextAlarm(schedule s, RtcTime &rtc)
{
  int next_hour;
  int next_minute = s.m[0];
  int next_date = rtc.day();
  if (s.start_h > s.end_h) // 日付をまたいで稼働する場合
  {
    if (rtc.hour() < s.start_h) // 開始時刻より前
    {
      if (rtc.hour() < s.end_h) // 開始時刻と終了時刻の間
      {
        findMinute(s, &next_minute, &next_hour, rtc);
      }
      else // 終了時刻は過ぎているが開始時刻より前
      {
        next_hour = s.start_h; // その日の開始時刻に起動        
      }      
    }
    else  // 開始時刻より後＝稼働時間内
    {
      findMinute(s, &next_minute, &next_hour, rtc);
    }
  }
  else // 日付をまたがずに稼働
  {
    if ((rtc.hour() >= s.start_h) & (rtc.hour() <= s.end_h)) // 稼働時間内
    {
      findMinute(s, &next_minute, &next_hour, rtc);
    } else if (rtc.hour() < s.start_h) //　開始時刻より前
    {
      next_hour = s.start_h;
    }
    else // 終了時刻より後
    {
      next_hour = s.start_h;
      next_date += 1;
    }
  }
  RtcTime rtc_to_alarm = RtcTime(rtc.year(), rtc.month(), next_date, next_hour, next_minute, 0);
  String str_now = printClock(rtc_to_alarm); 
  Serial.println("The next boot time is " + str_now);
  int sleep_sec = rtc_to_alarm.unixtime() - rtc.unixtime();
  return sleep_sec;
}
/*---------------------------------------------------------------------------------------------*/


/* For audio recording
-----------------------------------------------------------------------------------------------*/
int32_t recoding_size;
int buff_size;
String ext;
const int wd_time = 20000; // Watchdog time in ms. if the main loop took more than wd_time, the watchdog will reset the process.

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

/* To initialize the audio device
---------------------------------------------------------*/
void initAudio()
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

  /* To initialize the audio device
  -----------------------------------------------*/
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
------------------------------------------------------------*/
void exit_recording()
{
  theAudio->closeOutputFile(myFile);
  myFile.close();
  
  theAudio->setReadyMode();
  theAudio->end();

  Serial.println("Something wrong happened. End Recording.");
}

/* To record an audio with a given file name
-------------------------------------------------------------*/
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
    Watchdog.kick();
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

/*---------------------------------------------------------------------------------------------*/


/* Main functions
-----------------------------------------------------------------------------------------------*/
void setup()
{
  /* For timelapse operation
  ---------------------------------------------*/
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Starting!");
  setRTC();
  setLowPower();
  /* For audio recording
  ---------------------------------------------*/  
  Watchdog.begin();
  initAudio();
  Watchdog.start(wd_time);
}

bool rec_ok = true;
String file_name;
int sleep_sec;
RtcTime now;

void loop()
{
  now = RTC.getTime();
  // Display the current time every a second
  String str_now = printClock(now);
  file_name = out_dir + String("/") + str_now + ext;
  rec_ok = rec(file_name);
  if (!rec_ok)
  {
    Serial.println("Failed recording! Restarting...");
    while (true)
    {
      delay(1000);
      Serial.println(String("Watchdog timer ramains..." + (Watchdog.timeleft()/1000) + String("sec")));
    }
  }
  Watchdog.kick();
  now = RTC.getTime();
  sleep_sec = getNextAlarm(s, now);
  Serial.print("Go to deep sleep...");
  LowPower.deepSleep(sleep_sec);
}
