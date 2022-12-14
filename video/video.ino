#include <Camera.h>
#include <SDHCI.h>
#include "AviLibrary.h"

/*
---------------------
Name: video.ino
Author: R. Okamoto
Started: 2022/12/9
Purpose: To capture a video with a set of arameters
---------------------
Description:
This program is for make your SonySpresense a video camera that is fully customizable.
You can set video capturing parameters using videoParams structure.
The obtained video will be saved as an AVI file.
It depends on the AviLibrary. Please install it before staring.
https://github.com/YoshinoTaro/AviLibrary_Arduino
See below for the library description.
It depends on the AviLibrary. Please install it before staring.
Also you should carefully read the official document about video streaming.
https://developer.sony.com/develop/spresense/docs/arduino_developer_guide_ja.html#_camera%E3%81%AEpreview%E3%82%92%E5%8F%96%E5%BE%97%E3%81%99%E3%82%8Bvideostream%E6%A9%9F%E8%83%BD
I recommend you to set the memory size (Tools->Memory in ArduinoIDE) larger than 1024 kB.
*/

SDClass theSD;
File aviFile;
AviLibrary theAvi;

typedef struct { 
  String filename;
  int recording_time; // recodring time in sec.
  CAM_VIDEO_FPS fps; // Streaming FPS. Note: real FPS will be smaller than this value.
  int h; // Video width
  int v; // Video height
  int buff_num; // Buffer number 1 or 2.
  int jpg_buf_devisor; // JPEG buffer devisor, default = 7 
  int jpg_quality; // 0 ~ 100, default = 80
  int hdr; // HDR mode. CAM_HDR_MODE_ON or CAM_HDR_MODE_OFF
  bool autowb; // Auto White Balance?
  bool autoexp; // Auto Exposure?
  bool autoiso; // Auto ISO?
  CAM_WHITE_BALANCE wb; // Auto white balance mode. CAM_WHITE_BALANCE_{AUTO/INCANDESCENT/FLUORESCENT/DAYLIGHT/FLASH/CLOUDY/SHADE}
  int exp_time; // Exposure time, 30 means 1/30 sec.
  int iso; // ISO sensitivity
} videoParams;

// Setting camera parameters
videoParams p {
  String("movie.avi"),
  10,
  CAM_VIDEO_FPS_15,
  CAM_IMGSIZE_QUADVGA_H,
  CAM_IMGSIZE_QUADVGA_V,
  2,
  5,
  80,
  CAM_HDR_MODE_ON,
  true,
  true,
  true,
  CAM_WHITE_BALANCE_DAYLIGHT,
  100,
  100
};

void printError(enum CamErr err)
{
  Serial.print("Error: ");
  switch (err)
    {
      case CAM_ERR_NO_DEVICE:
        Serial.println("No Device");
        break;
      case CAM_ERR_ILLEGAL_DEVERR:
        Serial.println("Illegal device error");
        break;
      case CAM_ERR_ALREADY_INITIALIZED:
        Serial.println("Already initialized");
        break;
      case CAM_ERR_NOT_INITIALIZED:
        Serial.println("Not initialized");
        break;
      case CAM_ERR_NOT_STILL_INITIALIZED:
        Serial.println("Still picture not initialized");
        break;
      case CAM_ERR_CANT_CREATE_THREAD:
        Serial.println("Failed to create thread");
        break;
      case CAM_ERR_INVALID_PARAM:
        Serial.println("Invalid parameter");
        break;
      case CAM_ERR_NO_MEMORY:
        Serial.println("No memory");
        break;
      case CAM_ERR_USR_INUSED:
        Serial.println("Buffer already in use");
        break;
      case CAM_ERR_NOT_PERMITTED:
        Serial.println("Operation not permitted");
        break;
      default:
        break;
    }
}

void CamCB(CamImage img)
{

  /* Check the img instance is available or not. */

  if (img.isAvailable())
  {
    theAvi.addFrame(img.getImgBuff(), img.getImgSize());
  }
  else
  {
    Serial.println("Failed to get video stream image");
  }
}

// Initializing the camera
void init_camera(videoParams p)
{
  CamErr err;

  Serial.println("Prepare camera");
  err = theCamera.begin(
    p.buff_num, 
    p.fps,
    p.h,
    p.v,
    CAM_IMAGE_PIX_FMT_JPG,
    p.jpg_buf_devisor
  );
  if (err != CAM_ERR_SUCCESS)
  {
    printError(err);
  }
  if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }

  Serial.println("Set ISO Sensitivity");
  err = theCamera.setAutoISOSensitivity(p.autoiso);
  if (err != CAM_ERR_SUCCESS)
  {
    printError(err);
  }
  if (!p.autoiso)
  {
    err = theCamera.setISOSensitivity(p.iso);
    if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }
  }
  Serial.println("Set exposure");
  int exp = int(1 / float(p.exp_time) * 10000);
  err = theCamera.setAutoExposure(exp);
  if (err != CAM_ERR_SUCCESS)
  {
      printError(err);
  }
  if (!p.autoexp)
  {
    err = theCamera.setAbsoluteExposure(p.exp_time);
    if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }
  }

  /* Auto white balance configuration */
  Serial.println("Set white balance parameter");
  err = theCamera.setAutoWhiteBalance(p.autowb);
  if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }
  if (p.autowb)
  {
    err = theCamera.setAutoWhiteBalanceMode(p.wb);
    if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }
  }
  
  /* JPEG quality configuration */  
  Serial.println("Set JPEG quality");
  err = theCamera.setJPEGQuality(p.jpg_quality);
  if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }
  /* HDR configuration */
  Serial.println("Set HDR mode"); 
  err = theCamera.setHDR(p.hdr);
  if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }
    err = theCamera.startStreaming(true, CamCB);
  if (err != CAM_ERR_SUCCESS)
  {
    printError(err);
  }
}

void setup() {
  CamErr err;
  Serial.begin(115200);
  while (!Serial)
  {
    ; /* wait for serial port to connect. Needed for native USB port only */
  }

  /* Initialize SD */
  while (!theSD.begin())
  {
    /* wait until SD card is mounted. */
    Serial.println("Insert SD card.");
  }

  theSD.remove(p.filename);
  aviFile = theSD.open(p.filename, FILE_WRITE);
  theAvi.begin(aviFile, p.h, p.v);
  init_camera(p);
  Serial.println("Start recording...");
  theAvi.startRecording();
}

void loop() {
  static uint32_t start_time = millis();  
  uint32_t duration = millis() - start_time;
  if (duration > (p.recording_time * 1000)) {
    theCamera.startStreaming(false);
    theAvi.endRecording();
    theAvi.end();
    Serial.println("Movie saved");
    Serial.println("| Movie width:    " + String(theAvi.getWidth()));
    Serial.println("| Movie height:   " + String(theAvi.getHeight()));
    Serial.println("| File size (kB): " + String(theAvi.getFileSize()));
    Serial.println("| Captured Frame: " + String(theAvi.getTotalFrame())); 
    Serial.println("| Duration (sec): " + String(theAvi.getDuration()));
    Serial.println("| Frame per sec : " + String(theAvi.getFps()));
    Serial.println("| Max data rate : " + String(theAvi.getMaxDataRate()));
    Serial.println("| ISO: " + String(theCamera.getISOSensitivity()));
    Serial.println(String("| Exposure: 1/") + (1 / (float(theCamera.getAbsoluteExposure()) / 10000)));
    Serial.println("| HDR mode: " + String(theCamera.getHDR()));
    theCamera.end();
    while (true) {
      digitalWrite(LED0, HIGH);
      delay(100);
      digitalWrite(LED0, LOW);
      delay(100);
    }
  }
  delay(100);
}

