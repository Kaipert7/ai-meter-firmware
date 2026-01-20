#include "defines.h"

#include "ClassFlowImage.h"
#include <string>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#include "time_sntp.h"
#include "ClassLogFile.h"
#include "CImageBasis.h"
#include "esp_log.h"

static const char *TAG = "FLOWIMAGE";

ClassFlowImage::ClassFlowImage(const char *_logTag)
{
    logTag = _logTag;
    isLogImage = false;
    disabled = false;
    imagesRetention = 5;
}

ClassFlowImage::ClassFlowImage(std::vector<ClassFlow *> *_lfc, const char *_logTag) : ClassFlow(_lfc)
{
    logTag = _logTag;
    isLogImage = false;
    disabled = false;
    imagesRetention = 5;
}

ClassFlowImage::ClassFlowImage(std::vector<ClassFlow *> *_lfc, ClassFlow *_prev, const char *_logTag) : ClassFlow(_lfc, _prev)
{
    logTag = _logTag;
    isLogImage = false;
    disabled = false;
    imagesRetention = 5;
}

string ClassFlowImage::CreateLogFolder(string _time)
{
    if (!isLogImage)
    {
        return "";
    }

    string logPath = imagesLocation + "/" + _time.LOGFILE_TIME_FORMAT_DATE_EXTR + "/" + _time.LOGFILE_TIME_FORMAT_HOUR_EXTR;

    isLogImage = mkdir_r(logPath.c_str(), S_IRWXU) == 0;
    if (!isLogImage)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Can't create log folder for analog images. Path " + logPath);
    }

    return logPath;
}

void ClassFlowImage::LogImage(string _logPath, string _name, float *_resultFloat, int *_resultInt, string _time, CImageBasis *_img)
{
    if (!isLogImage)
    {
        return;
    }

    char buf[10];

    if (_resultFloat != NULL)
    {
        if (*_resultFloat < 0)
        {
            sprintf(buf, "N.N_");
        }
        else
        {
            sprintf(buf, "%.1f_", *_resultFloat);
            if (strcmp(buf, "10.0_") == 0)
            {
                sprintf(buf, "N.N_");
            }
        }
    }
    else if (_resultInt != NULL)
    {
        sprintf(buf, "%d_", *_resultInt);
    }
    else
    {
        buf[0] = '\0';
    }

    string nm = _logPath + "/" + buf + _name + "_" + _time + ".jpg";
    nm = format_filename(nm);

    string output = "/sdcard/img_tmp/" + _name + ".jpg";
    output = format_filename(output);

    ESP_LOGD(logTag, "save to file: %s", nm.c_str());
    _img->SaveToFile(nm);
}

void ClassFlowImage::RemoveOldLogs(void)
{
    if (!isLogImage)
    {
        return;
    }

    ESP_LOGD(TAG, "remove old images");
    if (imagesRetention == 0)
    {
        return;
    }

    time_t rawtime;
    struct tm *timeinfo;
    char cmpfilename[30];

    time(&rawtime);
    rawtime = add_days(rawtime, -1 * imagesRetention + 1);
    timeinfo = localtime(&rawtime);

    strftime(cmpfilename, 30, LOGFILE_TIME_FORMAT, timeinfo);
    string folderName = string(cmpfilename).LOGFILE_TIME_FORMAT_DATE_EXTR;

    DIR *dir = opendir(imagesLocation.c_str());
    if (!dir)
    {
        ESP_LOGE(TAG, "Failed to stat dir: %s", imagesLocation.c_str());
        return;
    }

    struct dirent *entry;
    int deleted = 0;
    int notDeleted = 0;

    while ((entry = readdir(dir)) != NULL)
    {
        string folderPath = imagesLocation + "/" + entry->d_name;
        if (entry->d_type == DT_DIR)
        {
            if ((strlen(entry->d_name) == folderName.length()) && (strcmp(entry->d_name, folderName.c_str()) < 0))
            {
                remove_folder(folderPath.c_str(), logTag);
                deleted++;
            }
            else
            {
                notDeleted++;
            }
        }
    }

    ESP_LOGD(TAG, "Image folder deleted: %d | Image folder not deleted: %d", deleted, notDeleted);
    closedir(dir);
}
