#include <SDHCI.h>
#include <stdio.h>  /* for sprintf */
#include <Camera.h>

#define BAUDRATE                (115200)
#define TOTAL_PICTURE_COUNT     (10)

/*
---------------------
Name: camera.ino
Author: R. Okamoto
Started: 2022/12/9
Purpose: To capture images with a set of parameters
---------------------
Description:
This program is for make your SonySpresense a camera that is fully customizable.
You can set image capturing parameters using cameraParams structure.
You can also specify the number of pictures using TOTAL_PICTURE_COUNT.
The obtained images will be named as IMG_{number}.jpg.
I recommend you to set the memory size (Tools->Memory in ArduinoIDE) larger than 1024 kB.
*/

typedef struct { 
  String filename;
  int h; // Image width
  int v; // Image height
  int jpg_buf_devisor; // JPEG buffer devisor, default = 7 
  int jpg_quality; // 0 ~ 100, default = 80
  int hdr; // HDR mode. CAM_HDR_MODE_ON or CAM_HDR_MODE_OFF
  bool autowb; // Auto White Balance?
  bool autoexp; // Auto Exposure?
  bool autoiso; // Auto ISO?
  CAM_WHITE_BALANCE wb; // Auto white balance mode. CAM_WHITE_BALANCE_{AUTO/INCANDESCENT/FLUORESCENT/DAYLIGHT/FLASH/CLOUDY/SHADE}
  int exp_time; // Exposure time, 30 means 1/30 sec.
  int iso; // ISO sensitivity
} cameraParams;

// Setting camera parameters
cameraParams p {
  String("IMG.jpg"),
  CAM_IMGSIZE_QUADVGA_H,
  CAM_IMGSIZE_QUADVGA_V,
  5,
  80,
  CAM_HDR_MODE_ON,
  true,
  false,
  true,
  CAM_WHITE_BALANCE_DAYLIGHT,
  200,
  100
};

SDClass  theSD;

int take_picture_count = 0;

/**
 * Print error message
 */

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

// Initializing the camera
void init_camera(cameraParams p)
{
  CamErr err;

  Serial.println("Prepare camera");
  err = theCamera.begin(0); // begin(0) stands for no preview
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
  err = theCamera.setAutoExposure(p.autoexp);
  if (err != CAM_ERR_SUCCESS)
  {
      printError(err);
  }
  if (!p.autoexp)
  {
    int exp = int(1 / float(p.exp_time) * 10000);
    err = theCamera.setAbsoluteExposure(exp);
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
  /* Set parameters about still picture */
  Serial.println("Set still picture format"); 
  err = theCamera.setStillPictureImageFormat(
    p.h,
    p.v,
    CAM_IMAGE_PIX_FMT_JPG,
    p.jpg_buf_devisor
  );

  if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }
  Serial.println("Set HDR mode"); 
  err = theCamera.setHDR(p.hdr);
  if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }
}

/**
 * @brief Take picture with format JPEG per second
 */

void take_a_picture(cameraParams p)
{
  /* Take still picture.
  * Unlike video stream(startStreaming) , this API wait to receive image data
  *  from camera device.
  */
  Serial.println("call takePicture()");
  CamImage img = theCamera.takePicture();
  /* Check availability of the img instance. */
  /* If any errors occur, the img is not available. */
  if (img.isAvailable())
    {    
      Serial.println(String("ISO: ") + theCamera.getISOSensitivity());
      Serial.println(String("Exposure: 1/") + (1 / (float(theCamera.getAbsoluteExposure()) / 10000)));
      Serial.println(String("HDR mode: ") + theCamera.getHDR());
      Serial.print("Save taken picture as ");
      Serial.print(p.filename);
      Serial.println("");
        
      /* Remove the old file with the same file name as new created file,
       * and create new file.
       */

      theSD.remove(p.filename);
      File myFile = theSD.open(p.filename, FILE_WRITE);
      myFile.write(img.getImgBuff(), img.getImgSize());
      myFile.close();
    }
    else
    {
    /* The size of a picture may exceed the allocated memory size.
     * Then, allocate the larger memory size and/or decrease the size of a picture.
     * [How to allocate the larger memory]
     * - Decrease jpgbufsize_divisor specified by setStillPictureImageFormat()
     * - Increase the Memory size from Arduino IDE tools Menu
     * [How to decrease the size of a picture]
     * - Decrease the JPEG quality by setJPEGQuality()
     */
      Serial.println("Failed to take picture");
    }
}

void setup()
{
  /* Open serial communications and wait for port to open */
  Serial.begin(BAUDRATE);
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
  /* Initialize the camera */
  init_camera(p);
}

void loop()
{
  if (take_picture_count < TOTAL_PICTURE_COUNT)
    {
      p.filename = String("IMG") + take_picture_count + String(".jpg");
      take_a_picture(p);
    }
  else if (take_picture_count == TOTAL_PICTURE_COUNT)
    {
      Serial.println("End.");
      theCamera.end();
    }
  sleep(1); /* wait for one second to take still picture. */
  take_picture_count++;
}
