#include "defines.h"

#include "ClassFlowAlignment.h"
#include "ClassControllCamera.h"
#include "ClassFlowTakeImage.h"
#include "ClassFlow.h"
#include "MainFlowControl.h"

#include "CRotateImage.h"
#include "esp_log.h"

#include "ClassLogFile.h"
#include "psram.h"

static const char *TAG = "ALIGN";

void ClassFlowAlignment::SetInitialParameter(void)
{
    anz_ref = 0;
    Camera.ImageInitialRotate = 0;
    Camera.ImageAntialiasing = false;
    Camera.ImageInitialFlip = false;
    namerawimage = "/sdcard/img_tmp/raw.jpg";
    FileStoreRefAlignment = "/sdcard/config/align.txt";
    ListFlowControll = NULL;
    AlignAndCutImage = NULL;
    ImageBasis = NULL;
    ImageTMP = NULL;
#ifdef ALGROI_LOAD_FROM_MEM_AS_JPG
    AlgROI = (ImageData *)malloc_psram_heap(std::string(TAG) + "->AlgROI", sizeof(ImageData), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
#endif
    previousElement = NULL;
    disabled = false;
    SAD_criteria = 0.05;
}

ClassFlowAlignment::ClassFlowAlignment(std::vector<ClassFlow *> *lfc)
{
    SetInitialParameter();
    ListFlowControll = lfc;

    for (int i = 0; i < ListFlowControll->size(); ++i)
    {
        if (((*ListFlowControll)[i])->name().compare("ClassFlowTakeImage") == 0)
        {
            ImageBasis = ((ClassFlowTakeImage *)(*ListFlowControll)[i])->rawImage;
        }
    }

    // the function take pictures does not exist --> must be created first ONLY FOR TEST PURPOSES
    if (!ImageBasis)
    {
        ESP_LOGD(TAG, "CImageBasis had to be created");
        ImageBasis = new CImageBasis("ImageBasis", namerawimage);
    }
}

bool ClassFlowAlignment::ReadParameter(FILE *pfile, std::string &aktparamgraph)
{
    aktparamgraph = trim_string_left_right(aktparamgraph);
    if (aktparamgraph.size() == 0)
    {
        if (!GetNextParagraph(pfile, aktparamgraph))
        {
            return false;
        }
    }

    if ((to_upper(aktparamgraph).compare("[ALIGNMENT]") != 0) && (to_upper(aktparamgraph).compare(";[ALIGNMENT]") != 0))
    {
        // Paragraph does not fit Alignment
        return false;
    }

    int suchex = 20;
    int suchey = 20;
    int maxangle = 45;
    int alg_algo = 0; // default=0; 1 =HIGHACCURACY; 2= FAST; 3= OFF //add disable aligment algo |01.2023

    std::vector<std::string> splitted;

    while (getNextLine(pfile, &aktparamgraph) && !isNewParagraph(aktparamgraph))
    {
        splitted = split_line(aktparamgraph);

        if (splitted.size() > 1)
        {
            std::string _param = to_upper(splitted[0]);

            if (_param == "FLIPIMAGESIZE")
            {
                Camera.ImageInitialFlip = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "INITIALROTATE")
            {
                if (is_string_numeric(splitted[1]))
                {
                    Camera.ImageInitialRotate = std::stod(splitted[1]);
                }
            }
            else if (_param == "SEARCHFIELDX")
            {
                if (is_string_numeric(splitted[1]))
                {
                    suchex = clip_int(std::stoi(splitted[1]), 320, 0);
                }
            }
            else if (_param == "SEARCHFIELDY")
            {
                if (is_string_numeric(splitted[1]))
                {
                    suchey = clip_int(std::stoi(splitted[1]), 240, 0);
                }
            }
            else if (_param == "SEARCHMAXANGLE")
            {
                if (is_string_numeric(splitted[1]))
                {
                    maxangle = clip_int(std::stoi(splitted[1]), 180, 0);
                }
            }
            else if (_param == "ANTIALIASING")
            {
                Camera.ImageAntialiasing = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "ALIGNMENTALGO")
            {
                if (to_upper(splitted[1]) == "HIGHACCURACY")
                {
                    alg_algo = 1;
                }
                else if (to_upper(splitted[1]) == "FAST")
                {
                    alg_algo = 2;
                }
                else if (to_upper(splitted[1]) == "OFF")
                {
                    // no align algo if set to 3 = off => no draw ref //add disable aligment algo |01.2023
                    alg_algo = 3;
                }
            }
            else if ((splitted.size() == 3) && (anz_ref < 2))
            {
                if (is_string_numeric(splitted[1]) && is_string_numeric(splitted[2]))
                {
                    References[anz_ref].image_file = format_filename("/sdcard" + splitted[0]);
                    References[anz_ref].target_x = std::stod(splitted[1]);
                    References[anz_ref].target_y = std::stod(splitted[2]);
                    anz_ref++;
                }
                else
                {
                    References[anz_ref].image_file = format_filename("/sdcard" + splitted[0]);
                    References[anz_ref].target_x = 10;
                    References[anz_ref].target_y = 10;
                    anz_ref++;
                }
            }
        }
    }

    for (int i = 0; i < anz_ref; ++i)
    {
        References[i].search_x = suchex;
        References[i].search_y = suchey;
        References[i].search_max_angle = (float)maxangle;
        References[i].fastalg_SAD_criteria = SAD_criteria;
        References[i].alignment_algo = alg_algo;
    }

    // no align algo if set to 3 = off => no draw ref //add disable aligment algo |01.2023
    if (References[0].alignment_algo != 3)
    {
        return LoadReferenceAlignmentValues();
    }

    return true;
}

std::string ClassFlowAlignment::getHTMLSingleStep(std::string host)
{
    std::string result;

    result = "<p>Rotated Image: </p> <p><img src=\"" + host + "/img_tmp/rot.jpg\"></p>\n";
    result = result + "<p>Found Alignment: </p> <p><img src=\"" + host + "/img_tmp/rot_roi.jpg\"></p>\n";
    result = result + "<p>Aligned Image: </p> <p><img src=\"" + host + "/img_tmp/alg.jpg\"></p>\n";
    return result;
}

bool ClassFlowAlignment::doFlow(std::string time)
{
#ifdef ALGROI_LOAD_FROM_MEM_AS_JPG
    // AlgROI needs to be allocated before ImageTMP to avoid heap fragmentation
    if (!AlgROI)
    {
        AlgROI = (ImageData *)heap_caps_realloc(AlgROI, sizeof(ImageData), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

        if (!AlgROI)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Can't allocate AlgROI");
            LogFile.WriteHeapInfo("ClassFlowAlignment-doFlow");
        }
    }

    if (AlgROI)
    {
        ImageBasis->writeToMemoryAsJPG((ImageData *)AlgROI, 90);
    }
#endif

    if (!ImageTMP)
    {
        ImageTMP = new CImageBasis("TempImage", ImageBasis); // Make sure the name does not get change, it is relevant for the PSRAM allocation!

        if (!ImageTMP)
        {
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Can't allocate TempImage -> Exec this round aborted!");
            LogFile.WriteHeapInfo("ClassFlowAlignment-doFlow");
            return false;
        }
    }

    delete AlignAndCutImage;
    AlignAndCutImage = new CAlignAndCutImage("AlignAndCutImage", ImageBasis, ImageTMP);

    if (!AlignAndCutImage)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Can't allocate AlignAndCutImage -> Exec this round aborted!");
        LogFile.WriteHeapInfo("ClassFlowAlignment-doFlow");
        return false;
    }

    CRotateImage rt("rawImage", AlignAndCutImage, ImageTMP, Camera.ImageInitialFlip);

    if (Camera.ImageInitialFlip)
    {
        int temp_value = ImageBasis->height;
        ImageBasis->height = ImageBasis->width;
        ImageBasis->width = temp_value;

        temp_value = ImageTMP->width;
        ImageTMP->width = ImageTMP->height;
        ImageTMP->height = temp_value;
    }

    if (((Camera.ImageInitialRotate > 0) && (Camera.ImageInitialRotate < 360)) || Camera.ImageInitialFlip)
    {
        if (Camera.ImageAntialiasing)
        {
            rt.RotateAntiAliasing(Camera.ImageInitialRotate);
        }
        else
        {
            rt.Rotate(Camera.ImageInitialRotate);
        }

        if (Camera.SaveAllFiles)
        {
            AlignAndCutImage->SaveToFile(format_filename("/sdcard/img_tmp/rot.jpg"));
        }
    }

    // no align algo if set to 3 = off //add disable aligment algo |01.2023
    if (References[0].alignment_algo != 3)
    {
        int res = AlignAndCutImage->Align(&References[0], &References[1]);
        if (res >= Alignment_OK)
        {
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Alignment OK");
            if (res == Fast_Alignment_OK)
            {
                SaveReferenceAlignmentValues();
            }
            flowctrl.AlignmentOk = true;
        }
        else
        {
            // Alignment failed
            LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Alignment failed");
            flowctrl.AlignmentOk = false;
        }
    }
    else
    {
        flowctrl.AlignmentOk = true;
    }

#ifdef ALGROI_LOAD_FROM_MEM_AS_JPG
    if (AlgROI)
    {
        // no align algo if set to 3 = off => no draw ref //add disable aligment algo |01.2023
        if (References[0].alignment_algo != 3)
        {
            DrawRef(ImageTMP);
        }

        flowctrl.DigitDrawROI(ImageTMP);
        flowctrl.AnalogDrawROI(ImageTMP);
        ImageTMP->writeToMemoryAsJPG((ImageData *)AlgROI, 90);
    }
#endif

    if (Camera.SaveAllFiles)
    {
        AlignAndCutImage->SaveToFile(format_filename("/sdcard/img_tmp/alg.jpg"));
        ImageTMP->SaveToFile(format_filename("/sdcard/img_tmp/alg_roi.jpg"));
    }

    // must be deleted to have memory space for loading tflite
    delete ImageTMP;
    ImageTMP = NULL;

    return true;
}

void ClassFlowAlignment::SaveReferenceAlignmentValues(void)
{
    FILE *pFile = fopen(FileStoreRefAlignment.c_str(), "w");

    std::string temp_time;
    if (strlen(temp_time.c_str()) == 0)
    {
        time_t rawtime;
        struct tm *timeinfo;
        char buffer[80];

        time(&rawtime);
        timeinfo = localtime(&rawtime);

        strftime(buffer, 80, "%Y-%m-%dT%H:%M:%S", timeinfo);
        temp_time = std::string(buffer);
    }

    fputs(temp_time.c_str(), pFile);
    fputs("\n", pFile);

    std::string temp_value = std::to_string(References[0].fastalg_x) + "\t" + std::to_string(References[0].fastalg_y);
    temp_value = temp_value + "\t" + std::to_string(References[0].fastalg_SAD) + "\t" + std::to_string(References[0].fastalg_min);
    temp_value = temp_value + "\t" + std::to_string(References[0].fastalg_max) + "\t" + std::to_string(References[0].fastalg_avg);
    fputs(temp_value.c_str(), pFile);
    fputs("\n", pFile);

    temp_value = std::to_string(References[1].fastalg_x) + "\t" + std::to_string(References[1].fastalg_y);
    temp_value = temp_value + "\t" + std::to_string(References[1].fastalg_SAD) + "\t" + std::to_string(References[1].fastalg_min);
    temp_value = temp_value + "\t" + std::to_string(References[1].fastalg_max) + "\t" + std::to_string(References[1].fastalg_avg);
    fputs(temp_value.c_str(), pFile);
    fputs("\n", pFile);

    fclose(pFile);
}

bool ClassFlowAlignment::LoadReferenceAlignmentValues(void)
{
    FILE *pFile = fopen(FileStoreRefAlignment.c_str(), "r");
    if (pFile == NULL)
    {
        return false;
    }

    char temp_bufer[1024];
    // erste Zeile: 2025-03-16T18:50:22
    if (!fgets(temp_bufer, 1024, pFile))
    {
        fclose(pFile);
        ESP_LOGE(TAG, "/sdcard/config/align.txt empty!");
        return false;
    }

    // zweite Zeile: 177	342	-0.000000	6144	1611659784	0.000000
    if (!fgets(temp_bufer, 1024, pFile))
    {
        fclose(pFile);
        ESP_LOGE(TAG, "/sdcard/config/align.txt empty!");
        return false;
    }

    std::vector<std::string> splitted;
    splitted = split_line(temp_bufer, "\t");

    if (splitted.size() < 6)
    {
        fclose(pFile);
        ESP_LOGE(TAG, "/sdcard/config/align.txt wrong format!");
        return false;
    }

    if (is_string_numeric(splitted[0]) && is_string_numeric(splitted[1]) && is_string_numeric(splitted[2]) &&
        is_string_numeric(splitted[3]) && is_string_numeric(splitted[4]) && is_string_numeric(splitted[5]))
    {
        References[0].fastalg_x = stoi(splitted[0]);
        References[0].fastalg_y = stoi(splitted[1]);
        References[0].fastalg_SAD = stof(splitted[2]);
        References[0].fastalg_min = stoi(splitted[3]);
        References[0].fastalg_max = stoi(splitted[4]);
        References[0].fastalg_avg = stof(splitted[5]);
    }
    else
    {
        fclose(pFile);
        ESP_LOGE(TAG, "/sdcard/config/align.txt wrong format!");
        return false;
    }

    // dritte Zeile: 398	145	-0.000000	6144	1611659784	0.000000
    if (!fgets(temp_bufer, 1024, pFile))
    {
        fclose(pFile);
        ESP_LOGE(TAG, "/sdcard/config/align.txt empty!");
        return false;
    }

    splitted = split_line(temp_bufer, "\t");

    if (splitted.size() < 6)
    {
        fclose(pFile);
        ESP_LOGE(TAG, "/sdcard/config/align.txt wrong format!");
        return false;
    }

    if (is_string_numeric(splitted[0]) && is_string_numeric(splitted[1]) && is_string_numeric(splitted[2]) &&
        is_string_numeric(splitted[3]) && is_string_numeric(splitted[4]) && is_string_numeric(splitted[5]))
    {
        References[1].fastalg_x = stoi(splitted[0]);
        References[1].fastalg_y = stoi(splitted[1]);
        References[1].fastalg_SAD = stof(splitted[2]);
        References[1].fastalg_min = stoi(splitted[3]);
        References[1].fastalg_max = stoi(splitted[4]);
        References[1].fastalg_avg = stof(splitted[5]);
    }
    else
    {
        fclose(pFile);
        ESP_LOGE(TAG, "/sdcard/config/align.txt wrong format!");
        return false;
    }

    fclose(pFile);

    return true;
}

void ClassFlowAlignment::DrawRef(CImageBasis *Image)
{
    if (Image->ImageOkay())
    {
        Image->drawRect(References[0].target_x, References[0].target_y, References[0].width, References[0].height, 255, 0, 0, 2);
        Image->drawRect(References[1].target_x, References[1].target_y, References[1].width, References[1].height, 255, 0, 0, 2);
    }
}
