#include "defines.h"

#include <iostream>
#include <string>
#include <vector>
#include <regex>

#include "ClassFlowTakeImage.h"
#include "Helper.h"
#include "ClassLogFile.h"

#include "CImageBasis.h"
#include "ClassControllCamera.h"
#include "MainFlowControl.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "psram.h"

#include <time.h>

static const char *TAG = "TAKEIMAGE";

esp_err_t ClassFlowTakeImage::camera_capture(void)
{
    std::string file_name = NameRawImage;
    Camera.capture_to_file(file_name);
    time(&TimeImageTaken);
    localtime(&TimeImageTaken);

    return ESP_OK;
}

void ClassFlowTakeImage::takePictureWithFlash(int flash_duration)
{
    // in case the image is flipped, it must be reset here //
    rawImage->width = CCstatus.ImageWidth;
    rawImage->height = CCstatus.ImageHeight;
    if (Camera.CamTempImage)
    {
        rawImage->width = CFstatus.ImageWidth;
        rawImage->height = CFstatus.ImageHeight;
    }

    ESP_LOGD(TAG, "flash_duration: %d", flash_duration);

    Camera.capture_to_basis_image(rawImage, flash_duration);

    time(&TimeImageTaken);
    localtime(&TimeImageTaken);

    if (Camera.SaveAllFiles)
    {
        rawImage->SaveToFile(NameRawImage);
    }
}

void ClassFlowTakeImage::SetInitialParameter(void)
{
    TimeImageTaken = 0;
    rawImage = NULL;
    NameRawImage = "/sdcard/img_tmp/raw.jpg";

    disabled = false;
}

// auslesen der Kameraeinstellungen aus der config.ini
// wird beim Start aufgerufen
bool ClassFlowTakeImage::ReadParameter(FILE *pFile, std::string &aktparamgraph)
{
    aktparamgraph = trim_string_left_right(aktparamgraph);
    if (aktparamgraph.size() == 0)
    {
        if (!GetNextParagraph(pFile, aktparamgraph))
        {
            return false;
        }
    }

    if ((to_upper(aktparamgraph).compare("[TAKEIMAGE]") != 0) && (to_upper(aktparamgraph).compare(";[TAKEIMAGE]") != 0))
    {
        // Paragraph does not fit TakeImage
        return false;
    }

    Camera.get_sensor_controll_config(&CCstatus); // Kamera >>> CCstatus
    std::vector<std::string> splitted;

    while (getNextLine(pFile, &aktparamgraph) && !isNewParagraph(aktparamgraph))
    {
        splitted = split_line(aktparamgraph);

        if (splitted.size() > 1)
        {
            std::string _param = to_upper(splitted[0]);

            if (_param == "RAWIMAGESLOCATION")
            {
                imagesLocation = "/sdcard" + splitted[1];
                isLogImage = true;
            }
            else if (_param == "RAWIMAGESRETENTION")
            {
                if (is_string_numeric(splitted[1]))
                {
                    imagesRetention = std::stod(splitted[1]);
                }
            }
            else if (_param == "SAVEALLFILES")
            {
                Camera.SaveAllFiles = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "WAITBEFORETAKINGPICTURE")
            {
                if (is_string_numeric(splitted[1]))
                {
                    int _WaitBeforePicture = std::stoi(splitted[1]);
                    if (_WaitBeforePicture != 0)
                    {
                        CCstatus.WaitBeforePicture = _WaitBeforePicture;
                    }
                    else
                    {
                        CCstatus.WaitBeforePicture = 2;
                    }
                }
            }
            else if (_param == "CAMXCLKFREQMHZ")
            {
                if (is_string_numeric(splitted[1]))
                {
                    int _CamXclkFreqMhz = std::stoi(splitted[1]);
                    CCstatus.CamXclkFreqMhz = clip_int(_CamXclkFreqMhz, 20, 1);
                }
            }
            else if (_param == "CAMGAINCEILING")
            {
                std::string _ImageGainceiling = to_upper(splitted[1]);

                if (is_string_numeric(_ImageGainceiling))
                {
                    int _ImageGainceiling_ = std::stoi(_ImageGainceiling);
                    switch (_ImageGainceiling_)
                    {
                    case 1:
                        CCstatus.ImageGainceiling = GAINCEILING_4X;
                        break;
                    case 2:
                        CCstatus.ImageGainceiling = GAINCEILING_8X;
                        break;
                    case 3:
                        CCstatus.ImageGainceiling = GAINCEILING_16X;
                        break;
                    case 4:
                        CCstatus.ImageGainceiling = GAINCEILING_32X;
                        break;
                    case 5:
                        CCstatus.ImageGainceiling = GAINCEILING_64X;
                        break;
                    case 6:
                        CCstatus.ImageGainceiling = GAINCEILING_128X;
                        break;
                    default:
                        CCstatus.ImageGainceiling = GAINCEILING_2X;
                    }
                }
                else
                {
                    if (_ImageGainceiling == "X4")
                    {
                        CCstatus.ImageGainceiling = GAINCEILING_4X;
                    }
                    else if (_ImageGainceiling == "X8")
                    {
                        CCstatus.ImageGainceiling = GAINCEILING_8X;
                    }
                    else if (_ImageGainceiling == "X16")
                    {
                        CCstatus.ImageGainceiling = GAINCEILING_16X;
                    }
                    else if (_ImageGainceiling == "X32")
                    {
                        CCstatus.ImageGainceiling = GAINCEILING_32X;
                    }
                    else if (_ImageGainceiling == "X64")
                    {
                        CCstatus.ImageGainceiling = GAINCEILING_64X;
                    }
                    else if (_ImageGainceiling == "X128")
                    {
                        CCstatus.ImageGainceiling = GAINCEILING_128X;
                    }
                    else
                    {
                        CCstatus.ImageGainceiling = GAINCEILING_2X;
                    }
                }
            }
            else if (_param == "CAMQUALITY")
            {
                if (is_string_numeric(splitted[1]))
                {
                    int _ImageQuality = std::stoi(splitted[1]);
                    CCstatus.ImageQuality = clip_int(_ImageQuality, 63, 6);
                }
            }
            else if (_param == "CAMBRIGHTNESS")
            {
                if (is_string_numeric(splitted[1]))
                {
                    int _ImageBrightness = std::stoi(splitted[1]);
                    CCstatus.ImageBrightness = clip_int(_ImageBrightness, 2, -2);
                }
            }
            else if (_param == "CAMCONTRAST")
            {
                if (is_string_numeric(splitted[1]))
                {
                    int _ImageContrast = std::stoi(splitted[1]);
                    CCstatus.ImageContrast = clip_int(_ImageContrast, 2, -2);
                }
            }
            else if (_param == "CAMSATURATION")
            {
                if (is_string_numeric(splitted[1]))
                {
                    int _ImageSaturation = std::stoi(splitted[1]);
                    CCstatus.ImageSaturation = clip_int(_ImageSaturation, 2, -2);
                }
            }
            else if (_param == "CAMSHARPNESS")
            {
                if (is_string_numeric(splitted[1]))
                {
                    int _ImageSharpness = std::stoi(splitted[1]);
                    if (Camera.CamSensorId == OV2640_PID)
                    {
                        CCstatus.ImageSharpness = clip_int(_ImageSharpness, 2, -2);
                    }
                    else
                    {
                        CCstatus.ImageSharpness = clip_int(_ImageSharpness, 3, -3);
                    }
                }
            }
            else if (_param == "CAMAUTOSHARPNESS")
            {
                CCstatus.ImageAutoSharpness = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "CAMSPECIALEFFECT")
            {
                std::string _ImageSpecialEffect = to_upper(splitted[1]);

                if (is_string_numeric(_ImageSpecialEffect))
                {
                    int _ImageSpecialEffect_ = std::stoi(_ImageSpecialEffect);
                    CCstatus.ImageSpecialEffect = clip_int(_ImageSpecialEffect_, 6, 0);
                }
                else
                {
                    if (_ImageSpecialEffect == "NEGATIVE")
                    {
                        CCstatus.ImageSpecialEffect = 1;
                    }
                    else if (_ImageSpecialEffect == "GRAYSCALE")
                    {
                        CCstatus.ImageSpecialEffect = 2;
                    }
                    else if (_ImageSpecialEffect == "RED")
                    {
                        CCstatus.ImageSpecialEffect = 3;
                    }
                    else if (_ImageSpecialEffect == "GREEN")
                    {
                        CCstatus.ImageSpecialEffect = 4;
                    }
                    else if (_ImageSpecialEffect == "BLUE")
                    {
                        CCstatus.ImageSpecialEffect = 5;
                    }
                    else if (_ImageSpecialEffect == "RETRO")
                    {
                        CCstatus.ImageSpecialEffect = 6;
                    }
                    else
                    {
                        CCstatus.ImageSpecialEffect = 0;
                    }
                }
            }
            else if (_param == "CAMWBMODE")
            {
                std::string _ImageWbMode = to_upper(splitted[1]);

                if (is_string_numeric(_ImageWbMode))
                {
                    int _ImageWbMode_ = std::stoi(_ImageWbMode);
                    CCstatus.ImageWbMode = clip_int(_ImageWbMode_, 4, 0);
                }
                else
                {
                    if (_ImageWbMode == "SUNNY")
                    {
                        CCstatus.ImageWbMode = 1;
                    }
                    else if (_ImageWbMode == "CLOUDY")
                    {
                        CCstatus.ImageWbMode = 2;
                    }
                    else if (_ImageWbMode == "OFFICE")
                    {
                        CCstatus.ImageWbMode = 3;
                    }
                    else if (_ImageWbMode == "HOME")
                    {
                        CCstatus.ImageWbMode = 4;
                    }
                    else
                    {
                        CCstatus.ImageWbMode = 0;
                    }
                }
            }
            else if (_param == "CAMAWB")
            {
                CCstatus.ImageAwb = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "CAMAWBGAIN")
            {
                CCstatus.ImageAwbGain = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "CAMAEC")
            {
                CCstatus.ImageAec = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "CAMAEC2")
            {
                CCstatus.ImageAec2 = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "CAMAELEVEL")
            {
                if (is_string_numeric(splitted[1]))
                {
                    int _ImageAeLevel = std::stoi(splitted[1]);
                    if (Camera.CamSensorId == OV2640_PID)
                    {
                        CCstatus.ImageAeLevel = clip_int(_ImageAeLevel, 2, -2);
                    }
                    else
                    {
                        CCstatus.ImageAeLevel = clip_int(_ImageAeLevel, 5, -5);
                    }
                }
            }
            else if (_param == "CAMAECVALUE")
            {
                if (is_string_numeric(splitted[1]))
                {
                    int _ImageAecValue = std::stoi(splitted[1]);
                    CCstatus.ImageAecValue = clip_int(_ImageAecValue, 1200, 0);
                }
            }
            else if (_param == "CAMAGC")
            {
                CCstatus.ImageAgc = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "CAMAGCGAIN")
            {
                if (is_string_numeric(splitted[1]))
                {
                    int _ImageAgcGain = std::stoi(splitted[1]);
                    CCstatus.ImageAgcGain = clip_int(_ImageAgcGain, 30, 0);
                }
            }
            else if (_param == "CAMBPC")
            {
                CCstatus.ImageBpc = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "CAMWPC")
            {
                CCstatus.ImageWpc = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "CAMRAWGMA")
            {
                CCstatus.ImageRawGma = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "CAMLENC")
            {
                CCstatus.ImageLenc = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "CAMHMIRROR")
            {
                CCstatus.ImageHmirror = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "CAMVFLIP")
            {
                CCstatus.ImageVflip = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "CAMDCW")
            {
                CCstatus.ImageDcw = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "CAMDENOISE")
            {
                if (is_string_numeric(splitted[1]))
                {
                    int _ImageDenoiseLevel = std::stoi(splitted[1]);
                    if (Camera.CamSensorId == OV2640_PID)
                    {
                        CCstatus.ImageDenoiseLevel = 0;
                    }
                    else
                    {
                        CCstatus.ImageDenoiseLevel = clip_int(_ImageDenoiseLevel, 8, 0);
                    }
                }
            }
            else if (_param == "CAMZOOM")
            {
                CCstatus.ImageZoomEnabled = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "CAMZOOMOFFSETX")
            {
                if (is_string_numeric(splitted[1]))
                {
                    int _ImageZoomOffsetX = std::stoi(splitted[1]);
                    if (Camera.CamSensorId == OV2640_PID)
                    {
                        CCstatus.ImageZoomOffsetX = clip_int(_ImageZoomOffsetX, 480, -480);
                    }
                    else if (Camera.CamSensorId == OV3660_PID)
                    {
                        CCstatus.ImageZoomOffsetX = clip_int(_ImageZoomOffsetX, 704, -704);
                    }
                    else if (Camera.CamSensorId == OV5640_PID)
                    {
                        CCstatus.ImageZoomOffsetX = clip_int(_ImageZoomOffsetX, 960, -960);
                    }
                }
            }
            else if (_param == "CAMZOOMOFFSETY")
            {
                if (is_string_numeric(splitted[1]))
                {
                    int _ImageZoomOffsetY = std::stoi(splitted[1]);
                    if (Camera.CamSensorId == OV2640_PID)
                    {
                        CCstatus.ImageZoomOffsetY = clip_int(_ImageZoomOffsetY, 360, -360);
                    }
                    else if (Camera.CamSensorId == OV3660_PID)
                    {
                        CCstatus.ImageZoomOffsetY = clip_int(_ImageZoomOffsetY, 528, -528);
                    }
                    else if (Camera.CamSensorId == OV5640_PID)
                    {
                        CCstatus.ImageZoomOffsetY = clip_int(_ImageZoomOffsetY, 720, -720);
                    }
                }
            }
            else if (_param == "CAMZOOMSIZE")
            {
                if (is_string_numeric(splitted[1]))
                {
                    int _ImageZoomSize = std::stoi(splitted[1]);
                    if (Camera.CamSensorId == OV2640_PID)
                    {
                        CCstatus.ImageZoomSize = clip_int(_ImageZoomSize, 29, 0);
                    }
                    else if (Camera.CamSensorId == OV3660_PID)
                    {
                        CCstatus.ImageZoomSize = clip_int(_ImageZoomSize, 43, 0);
                    }
                    else if (Camera.CamSensorId == OV5640_PID)
                    {
                        CCstatus.ImageZoomSize = clip_int(_ImageZoomSize, 59, 0);
                    }
                }
            }
            else if (_param == "LEDINTENSITY")
            {
                if (is_string_numeric(splitted[1]))
                {
                    int ledintensity = std::stoi(splitted[1]);
                    CCstatus.ImageLedIntensity = Camera.set_led_intensity(ledintensity);
                }
            }
            else if (_param == "DEMO")
            {
                Camera.DemoMode = alphanumeric_to_boolean(splitted[1]);
                if (Camera.DemoMode == true)
                {
                    Camera.use_demo_mode();
                }
            }
        }
    }

    Camera.set_sensor_controll_config(&CCstatus); // CCstatus >>> Kamera
    Camera.set_quality_zoom_size(&CCstatus);

    Camera.changedCameraSettings = false;
    Camera.CamTempImage = false;

    rawImage = new CImageBasis("rawImage");
    rawImage->CreateEmptyImage(CCstatus.ImageWidth, CCstatus.ImageHeight, 3);

    return true;
}

ClassFlowTakeImage::ClassFlowTakeImage(std::vector<ClassFlow *> *lfc) : ClassFlowImage(lfc, TAG)
{
    imagesLocation = "/log/source";
    imagesRetention = 5;
    SetInitialParameter();
}

std::string ClassFlowTakeImage::getHTMLSingleStep(std::string host)
{
    std::string result;
    result = "Raw Image: <br>\n<img src=\"" + host + "/img_tmp/raw.jpg\">\n";
    return result;
}

// wird bei jeder Auswertrunde aufgerufen
bool ClassFlowTakeImage::doFlow(std::string zwtime)
{
    psram_init_shared_memory_for_take_image_step();

    std::string logPath = CreateLogFolder(zwtime);

    int flash_duration = (int)(CCstatus.WaitBeforePicture * 1000);
    if (Camera.CamTempImage)
    {
        flash_duration = (int)(CFstatus.WaitBeforePicture * 1000);
    }

#ifdef WIFITURNOFF
    esp_wifi_stop(); // to save power usage and
#endif

    takePictureWithFlash(flash_duration);

#ifdef WIFITURNOFF
    esp_wifi_start();
#endif

    LogImage(logPath, "raw", NULL, NULL, zwtime, rawImage);

    RemoveOldLogs();
    psram_deinit_shared_memory_for_take_image_step();

    return true;
}

esp_err_t ClassFlowTakeImage::SendRawJPG(httpd_req_t *req)
{
    int flash_duration = (int)(CCstatus.WaitBeforePicture * 1000);
    if (Camera.CamTempImage)
    {
        flash_duration = (int)(CFstatus.WaitBeforePicture * 1000);
    }

    time(&TimeImageTaken);
    localtime(&TimeImageTaken);

    return Camera.capture_to_http(req, flash_duration);
}

ImageData *ClassFlowTakeImage::SendRawImage(void)
{
    CImageBasis *zw = new CImageBasis("SendRawImage", rawImage);
    ImageData *id;

    int flash_duration = (int)(CCstatus.WaitBeforePicture * 1000);
    if (Camera.CamTempImage)
    {
        flash_duration = (int)(CFstatus.WaitBeforePicture * 1000);
    }

    Camera.capture_to_basis_image(zw, flash_duration);
    time(&TimeImageTaken);
    localtime(&TimeImageTaken);

    id = zw->writeToMemoryAsJPG();
    delete zw;
    return id;
}

time_t ClassFlowTakeImage::getTimeImageTaken(void)
{
    return TimeImageTaken;
}

ClassFlowTakeImage::~ClassFlowTakeImage(void)
{
    delete rawImage;
}
