#pragma once

#ifndef CLASSCONTROLLCAMERA_H
#define CLASSCONTROLLCAMERA_H

#include <string>
#include <esp_http_server.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>

#include "defines.h"
#include "esp_camera.h"
#include "CImageBasis.h"

typedef struct
{
    int CamXclkFreqMhz;

    framesize_t ImageFrameSize = FRAMESIZE_VGA; // 0 - 10
    gainceiling_t ImageGainceiling;             // Image gain (GAINCEILING_x2, x4, x8, x16, x32, x64 or x128)

    int ImageQuality;    // 0 - 63
    int ImageBrightness; // (-2 to 2) - set brightness
    int ImageContrast;   //-2 - 2
    int ImageSaturation; //-2 - 2
    int ImageSharpness;  //-2 - 2
    bool ImageAutoSharpness;
    int ImageSpecialEffect; // 0 - 6
    int ImageWbMode;        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
    int ImageAwb;           // white balance enable (0 or 1)
    int ImageAwbGain;       // Auto White Balance enable (0 or 1)
    int ImageAec;           // auto exposure off (1 or 0)
    int ImageAec2;          // automatic exposure sensor  (0 or 1)
    int ImageAeLevel;       // auto exposure levels (-2 to 2)
    int ImageAecValue;      // set exposure manually  (0-1200)
    int ImageAgc;           // auto gain off (1 or 0)
    int ImageAgcGain;       // set gain manually (0 - 30)
    int ImageBpc;           // black pixel correction
    int ImageWpc;           // white pixel correction
    int ImageRawGma;        // (1 or 0)
    int ImageLenc;          // lens correction (1 or 0)
    int ImageHmirror;       // (0 or 1) flip horizontally
    int ImageVflip;         // Invert image (0 or 1)
    int ImageDcw;           // downsize enable (1 or 0)

    int ImageDenoiseLevel; // The OV2640 does not support it, OV3660 and OV5640 (0 to 8)

    int ImageWidth;
    int ImageHeight;

    int ImageLedIntensity;

    bool ImageZoomEnabled;
    int ImageZoomOffsetX;
    int ImageZoomOffsetY;
    int ImageZoomSize;

    int WaitBeforePicture;
} camera_controll_config_temp_t;

extern camera_controll_config_temp_t CCstatus;
extern camera_controll_config_temp_t CFstatus;

class CCamera
{
protected:
    void ledc_init(void);
    bool load_next_demo_image(camera_fb_t *fb);
    long get_file_size(std::string filename);
    void set_camera_window(sensor_t *cam_sensor, int frameSizeX, int frameSizeY, int xOffset, int yOffset, int xTotal, int yTotal, int xOutput, int yOutput, int imageVflip);
    void set_image_width_height_from_resolution(framesize_t resol);
    void sanitize_zoom_params(camera_controll_config_temp_t *camConfig, int imageSize, int frameSizeX, int frameSizeY, int &imageWidth, int &imageHeight, int &zoomOffsetX, int &zoomOffsetY);

public:
    uint16_t CamSensorId = OV2640_PID;

    int LedIntensity = 4096;
    bool CaptureToBasisImageLed = false;
    bool CaptureToFileLed = false;
    bool CaptureToHTTPLed = false;
    bool CaptureToStreamLed = false;

    bool DemoMode = false;
    bool SaveAllFiles = false;
    bool ImageAntialiasing = false;
    float ImageInitialRotate = 0.0;
    bool ImageInitialFlip = false;

    bool CamDeepSleepEnable = false;
    bool CameraInitSuccessful = false;
    bool changedCameraSettings = false;
    bool CamTempImage = false;

    CCamera(void);
    esp_err_t init_camera(void);
    void power_reset_camera(void);

    void set_flash_light_on_off(bool status);
    void set_blink_led_on_off(bool status);

    esp_err_t set_sensor_controll_config(camera_controll_config_temp_t *camConfig);
    esp_err_t get_sensor_controll_config(camera_controll_config_temp_t *camConfig);
    esp_err_t set_camera_config_from_to(camera_controll_config_temp_t *camConfigFrom, camera_controll_config_temp_t *camConfigTo);

    int check_camera_settings_changed(void);
    int set_camera_deep_sleep(bool enable);

    int set_camera_gainceiling(sensor_t *cam_sensor, gainceiling_t gainceilingLevel);
    void set_camera_sharpness(bool autoSharpnessEnabled, int sharpnessLevel);
    void set_camera_special_effect(sensor_t *cam_sensor, int specialEffect);
    void set_camera_contrast_brightness(sensor_t *cam_sensor, int _contrast, int _brightness);

    esp_err_t capture_to_http(httpd_req_t *req, int flash_duration = 0);
    esp_err_t capture_to_stream(httpd_req_t *req, bool FlashlightOn);

    void set_quality_zoom_size(camera_controll_config_temp_t *camConfig);
    void set_zoom_size(camera_controll_config_temp_t *camConfig);

    int set_led_intensity(int _intrel);
    bool get_camera_init_successful(void);
    void use_demo_mode(void);

    framesize_t text_to_framesize(const char *text);

    esp_err_t capture_to_file(std::string file_name, int flash_duration = 0);
    esp_err_t capture_to_basis_image(CImageBasis *_Image, int flash_duration = 0);
};

extern CCamera Camera;
#endif
