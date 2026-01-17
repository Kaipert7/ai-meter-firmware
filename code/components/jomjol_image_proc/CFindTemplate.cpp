#include "defines.h"

#include "CFindTemplate.h"

#include "ClassLogFile.h"
#include "Helper.h"

#include <esp_log.h>

static const char *TAG = "C FIND TEMPL";

bool CFindTemplate::FindTemplate(RefInfo *_ref)
{
    if (file_size(_ref->image_file.c_str()) == 0)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, _ref->image_file + " is empty!");
        return false;
    }

    uint8_t *rgb_template = stbi_load(_ref->image_file.c_str(), &tpl_width, &tpl_height, &tpl_bpp, channels);
    if (rgb_template == NULL)
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Failed to load " + _ref->image_file + "! Is it corrupted?");
        return false;
    }

    // ESP_LOGD(TAG, "FindTemplate 01");
    if (_ref->search_x == 0)
    {
        _ref->search_x = width;
        _ref->found_x = 0;
    }

    if (_ref->search_y == 0)
    {
        _ref->search_y = height;
        _ref->found_y = 0;
    }

    int x_search_area_start = std::max((_ref->target_x - _ref->search_x), 0);
    int x_search_area_stop = _ref->target_x + _ref->search_x;

    if ((x_search_area_stop + tpl_width) > width)
    {
        x_search_area_stop = width - tpl_width;
    }

    int x_search_area = x_search_area_stop - x_search_area_start + 1;

    int y_search_area_start = std::max((_ref->target_y - _ref->search_y), 0);
    int y_search_area_stop = _ref->target_y + _ref->search_y;

    if ((y_search_area_stop + tpl_height) > height)
    {
        y_search_area_stop = height - tpl_height;
    }

    int y_search_area = y_search_area_stop - y_search_area_start + 1;

    float avg = 0, SAD = 0;
    int min = 0, max = 0;
    bool isSimilar = false;

    // ESP_LOGD(TAG, "FindTemplate 02");
    if ((_ref->alignment_algo == 2) && (_ref->fastalg_x > -1) && (_ref->fastalg_y > -1))
    {
        isSimilar = CalculateSimularities(rgb_template, _ref->fastalg_x, _ref->fastalg_y, x_search_area, y_search_area, min, avg, max, SAD, _ref->fastalg_SAD, _ref->fastalg_SAD_criteria);
    }

    // ESP_LOGD(TAG, "FindTemplate 03");
    if (isSimilar)
    {
        _ref->found_x = _ref->fastalg_x;
        _ref->found_y = _ref->fastalg_y;

        stbi_image_free(rgb_template);

        return true;
    }

    // ESP_LOGD(TAG, "FindTemplate 04");
    double minSAD = pow(tpl_width * tpl_height * 255, 2);

    RGBImageLock();

    // ESP_LOGD(TAG, "FindTemplate 05");
    int _anzchannels = channels;

    if (_ref->alignment_algo == 0)
    {
        // 0 = "Default" (nur R-Kanal)
        _anzchannels = 1;
    }

    for (int x_outer = x_search_area_start; x_outer <= x_search_area_stop; x_outer++)
    {
        for (int y_outer = y_search_area_start; y_outer <= y_search_area_stop; ++y_outer)
        {
            double aktSAD = 0;

            for (int tpl_x = 0; tpl_x < tpl_width; tpl_x++)
            {
                for (int tpl_y = 0; tpl_y < tpl_height; tpl_y++)
                {
                    stbi_uc *p_org = rgb_image + (channels * ((y_outer + tpl_y) * width + (x_outer + tpl_x)));
                    stbi_uc *p_tpl = rgb_template + (channels * (tpl_y * tpl_width + tpl_x));

                    for (int tpl_ch = 0; tpl_ch < _anzchannels; ++tpl_ch)
                    {
                        aktSAD += pow(p_tpl[tpl_ch] - p_org[tpl_ch], 2);
                    }
                }
            }

            if (aktSAD < minSAD)
            {
                minSAD = aktSAD;

                _ref->found_x = x_outer;
                _ref->found_y = y_outer;
            }
        }
    }

    // ESP_LOGD(TAG, "FindTemplate 06");
    if (_ref->alignment_algo == 2)
    {
        isSimilar = CalculateSimularities(rgb_template, _ref->found_x, _ref->found_y, x_search_area, y_search_area, min, avg, max, SAD, _ref->fastalg_SAD, _ref->fastalg_SAD_criteria);
    }

    // ESP_LOGD(TAG, "FindTemplate 07");
    if (isSimilar)
    {
        _ref->fastalg_x = _ref->found_x;
        _ref->fastalg_y = _ref->found_y;

        _ref->fastalg_min = min;
        _ref->fastalg_max = max;

        _ref->fastalg_avg = avg;
        _ref->fastalg_SAD = SAD;

        RGBImageRelease();
        stbi_image_free(rgb_template);

        return true;
    }

    RGBImageRelease();
    stbi_image_free(rgb_template);

    // ESP_LOGD(TAG, "FindTemplate 09");
    return false;
}

bool CFindTemplate::CalculateSimularities(uint8_t *_rgb_tmpl, int _startx, int _starty, int _sizex, int _sizey, int &min, float &avg, int &max, float &SAD, float _SADold, float _SADcrit)
{
    int dif = 0;
    int minDif = 255;
    int maxDif = -255;
    double avgDifSum = 0;
    long int anz = 0;
    double aktSAD = 0;

    for (int x_outer = 0; x_outer <= _sizex; x_outer++)
    {
        for (int y_outer = 0; y_outer <= _sizey; ++y_outer)
        {
            stbi_uc *p_org = rgb_image + (channels * ((y_outer + _starty) * width + (x_outer + _startx)));
            stbi_uc *p_tpl = _rgb_tmpl + (channels * (y_outer * tpl_width + x_outer));

            for (int _ch = 0; _ch < channels; ++_ch)
            {
                dif = p_tpl[_ch] - p_org[_ch];
                aktSAD += pow(p_tpl[_ch] - p_org[_ch], 2);

                if (dif < minDif)
                {
                    minDif = dif;
                }

                if (dif > maxDif)
                {
                    maxDif = dif;
                }

                avgDifSum += dif;
                anz++;
            }
        }
    }

    avg = avgDifSum / anz;
    min = minDif;
    max = maxDif;
    SAD = sqrt(aktSAD) / anz;

    float _SADdif = abs(SAD - _SADold);

    ESP_LOGD(TAG, "Anzahl %ld, avgDifSum %fd, avg %f, SAD_neu: %fd, _SAD_old: %f, _SAD_crit:%f", anz, avgDifSum, avg, SAD, _SADold, _SADdif);

    if (_SADdif <= _SADcrit)
    {
        return true;
    }

    return false;
}
