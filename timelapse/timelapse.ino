#include <GNSS.h>
#include <LowPower.h>
#include <RTC.h>

typedef struct { 
 int start_h;
 int end_h;
 int m[3];
} schedule;

schedule s = {17, 7, {0, 20, 40}}; // 毎時何分に起動するか。複数書く場合は小さい順。

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

void printClock(RtcTime &rtc)
{
  printf("%04d/%02d/%02d %02d:%02d:%02d\n",
         rtc.year(), rtc.month(), rtc.day(),
         rtc.hour(), rtc.minute(), rtc.second());
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
  Serial.print("The next boot time is ");
  printClock(rtc_to_alarm);  
  int sleep_sec = rtc_to_alarm.unixtime() - rtc.unixtime();
  return sleep_sec;
}

void setup()
{
  Serial.begin(115200);
  while (!Serial);
  RtcTime  test = RtcTime(2022, 12, 14, 6, 49);
  getNextAlarm(s, test);
  Serial.println("Starting!");
  setRTC();
  setLowPower();
}

void loop()
{
  int sleep_sec;
  // Display the current time every a second
  RtcTime now = RTC.getTime();
  printClock(now);
  sleep_sec = getNextAlarm(s, now);
  Serial.print("Go to deep sleep...");
  LowPower.deepSleep(sleep_sec);
}

