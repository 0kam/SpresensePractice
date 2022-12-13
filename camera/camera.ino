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
Purpose: To capture images with a set of camera parameters
---------------------
Description:
This program is for make your SonySpresense a camera that is fully customizable.
You can set capturing parameters using cameraParams structure.
You can also specify the number of pictures using TOTAL_PICTURE_COUNT.
The obtained images will be named as IMG_{number}.jpg.
*/

typedef struct { 
  String filename;
  int h;
  int v;
  int jpg_buf_devisor; // JPEG buffer devisor, default = 7 
  int jpg_quality; // 0 ~ 100, default = 80
  int hdr; // HDR mode. CAM_HDR_MODE_ON or CAM_HDR_MODE_OFF
  bool autowb; // Auto White Balance?
  bool autoexp; // Auto Exposure?
  bool autoiso; // Auto ISO?
  CAM_WHITE_BALANCE wb; // Auto white balance mode. CAM_WHITE_BALANCE_{AUTO/INCANDESCENT/FLUORESCENT/DAYLIGHT/FLASH/CLOUDY/SHADE}
  int exp_time; // Exposure time, 100 u sec units. The max value is 2740 (NO HDR) and 317(HDR)
  int iso; // ISO sensitivity
} cameraParams;

// Setting camera parameters
cameraParams p {
  String("IMG.jpg"),
  CAM_IMGSIZE_QUADVGA_H,
  CAM_IMGSIZE_QUADVGA_V,
  5,
  80,
  CAM_HDR_MODE_OFF,
  true,
  false,
  false,
  CAM_WHITE_BALANCE_DAYLIGHT,
  50,
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

/**
 * Callback from Camera library when video frame is captured.
 */

void CamCB(CamImage img)
{
  /* Check the img instance is available or not. */

  if (img.isAvailable())
    {

      /* If you want RGB565 data, convert image data format to RGB565 */

      img.convertPixFormat(CAM_IMAGE_PIX_FMT_RGB565);

      /* You can use image data directly by using getImgSize() and getImgBuff().
       * for displaying image to a display, etc. */

      Serial.print("Image data size = ");
      Serial.print(img.getImgSize(), DEC);
      Serial.print(" , ");

      Serial.print("buff addr = ");
      Serial.print((unsigned long)img.getImgBuff(), HEX);
      Serial.println("");
    }
  else
    {
      Serial.println("Failed to get video stream image");
    }
}

// Initializing the camera
void init_camera(cameraParams p)
{
  CamErr err;

  /* begin() without parameters means that
   * number of buffers = 1, 30FPS, QVGA, YUV 4:2:2 format */

  Serial.println("Prepare camera");
  err = theCamera.begin(0); // begin(0) stands for no preview
  if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }

  /* Start video stream.
   * If received video stream data from camera device,
   *  camera library call CamCB.
   */
  
  /*
  Serial.println("Start streaming");
  err = theCamera.startStreaming(true, CamCB);
  if (err != CAM_ERR_SUCCESS)
    {
      printError(err);
    }
  */

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
      Serial.println(String("Exposure: ") + theCamera.getAbsoluteExposure());
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
