#include "defines.h"

#include <math.h>
#include <iomanip>
#include <sys/types.h>
#include <sstream> // std::stringstream
#include "esp_log.h"

#include "ClassFlowCNNGeneral.h"
#include "MainFlowControl.h"
#include "ClassControllCamera.h"

#include "CTfLiteClass.h"
#include "ClassLogFile.h"

static const char *TAG = "CNN";

ClassFlowCNNGeneral::ClassFlowCNNGeneral(ClassFlowAlignment *_flowalign, t_CNNType _cnntype) : ClassFlowImage(NULL, TAG)
{
    cnn_model_file = "";
    model_x_size = 1;
    model_y_size = 1;

    CNNGoodThreshold = 0.0;
    ListFlowControll = NULL;
    previousElement = NULL;

    isLogImageSelect = false;
    CNNType = _cnntype;
    flowpostalignment = _flowalign;
    imagesRetention = 5;

    disabled = false;
}

std::vector<double> ClassFlowCNNGeneral::getMeterValues(int _number = 0)
{
    std::vector<double> meterValues;

    if (GENERAL[_number]->ROI.size() == 0)
    {
        return meterValues;
    }

    for (int i = 0; i < GENERAL[_number]->ROI.size(); ++i)
    {
        if (CNNType == Digit)
        {
            meterValues.push_back(GENERAL[_number]->ROI[i]->result_klasse);
        }
        else
        {
            meterValues.push_back(GENERAL[_number]->ROI[i]->result_float);
        }
    }

    return meterValues;
}

std::string ClassFlowCNNGeneral::getReadout(int _number = 0, bool _extendedResolution, int prev, float _before_narrow_Analog, float AnalogToDigitTransitionStart)
{
    std::string result = "";

    if (GENERAL[_number]->ROI.size() == 0)
    {
        return result;
    }

    LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "getReadout _number=" + std::to_string(_number) + ", _extendedResolution=" + std::to_string(_extendedResolution) + ", prev=" + std::to_string(prev));

    if (CNNType == Analogue || CNNType == Analogue100)
    {
        float number = GENERAL[_number]->ROI[GENERAL[_number]->ROI.size() - 1]->result_float;
        int result_after_decimal_point = ((int)floor(number * 10.0f) + 10) % 10;

        prev = PointerEvalAnalog(GENERAL[_number]->ROI[GENERAL[_number]->ROI.size() - 1]->result_float, prev);
        result = std::to_string(prev);

        if (_extendedResolution)
        {
            result = result + std::to_string(result_after_decimal_point);
        }

        for (int i = GENERAL[_number]->ROI.size() - 2; i >= 0; --i)
        {
            prev = PointerEvalAnalog(GENERAL[_number]->ROI[i]->result_float, prev);
            result = std::to_string(prev) + result;
        }
        return result;
    }

    if (CNNType == Digit)
    {
        for (int i = 0; i < GENERAL[_number]->ROI.size(); ++i)
        {
            if ((GENERAL[_number]->ROI[i]->result_klasse >= 0) && (GENERAL[_number]->ROI[i]->result_klasse < 10))
            {
                result = result + std::to_string(GENERAL[_number]->ROI[i]->result_klasse);
            }
            else
            {
                result = result + "N";
            }
        }
        return result;
    }

    if ((CNNType == DoubleHyprid10) || (CNNType == Digit100))
    {
        float number = GENERAL[_number]->ROI[GENERAL[_number]->ROI.size() - 1]->result_float;
        // NaN?
        if ((number >= 0.0f) && (number < 10.0f))
        {
            // is only set if it is the first digit (no analogue before!)
            if (_extendedResolution)
            {
                int result_after_decimal_point = ((int)floor(number * 10.0f)) % 10;
                int result_before_decimal_point = ((int)floor(number)) % 10;

                result = std::to_string(result_before_decimal_point) + std::to_string(result_after_decimal_point);
                prev = result_before_decimal_point;
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "getReadout(dig100-ext) result_before_decimal_point=" + std::to_string(result_before_decimal_point) + ", result_after_decimal_point=" + std::to_string(result_after_decimal_point) + ", prev=" + std::to_string(prev));
            }
            else
            {
                if (_before_narrow_Analog >= 0.0f)
                {
                    prev = PointerEvalHybrid(GENERAL[_number]->ROI[GENERAL[_number]->ROI.size() - 1]->result_float, _before_narrow_Analog, prev, true, AnalogToDigitTransitionStart);
                }
                else
                {
                    prev = PointerEvalHybrid(GENERAL[_number]->ROI[GENERAL[_number]->ROI.size() - 1]->result_float, prev, prev);
                }

                // is necessary because a number greater than 9.994999 returns a 10! (for further details see check in PointerEvalHybrid)
                if ((prev >= 0) && (prev < 10))
                {
                    result = std::to_string(prev);
                    LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "getReadout(dig100) prev=" + std::to_string(prev));
                }
                else
                {
                    result = "N";
                }
            }
        }
        else
        {
            result = "N";
            if (_extendedResolution && (CNNType != Digit))
            {
                result = "NN";
            }
        }

        for (int i = GENERAL[_number]->ROI.size() - 2; i >= 0; --i)
        {
            if ((GENERAL[_number]->ROI[i]->result_float >= 0.0f) && (GENERAL[_number]->ROI[i]->result_float < 10.0f))
            {
                prev = PointerEvalHybrid(GENERAL[_number]->ROI[i]->result_float, GENERAL[_number]->ROI[i + 1]->result_float, prev);
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "getReadout(DoubleHyprid10) - roi_" + std::to_string(i) + "prev= " + std::to_string(prev));
                result = std::to_string(prev) + result;
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "getReadout(DoubleHyprid10) - roi_" + std::to_string(i) + "result= " + result);
            }
            else
            {
                prev = -1;
                result = "N" + result;
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "getReadout(result_float < 0 /'N') - roi_" + std::to_string(i) + "result_float=" + std::to_string(GENERAL[_number]->ROI[i]->result_float));
            }
        }
        return result;
    }
    return result;
}

/**
 * @brief Determines the number of an ROI in connection with previous ROI results
 *
 * @param number: is the current ROI as float value from recognition
 * @param number_of_predecessors: is the last (lower) ROI as float from recognition
 * @param eval_predecessors: is the evaluated number. Sometimes a much lower value can change higer values
 *                          example: 9.8, 9.9, 0.1
 *                          0.1 => 0 (eval_predecessors)
 *                          The 0 makes a 9.9 to 0 (eval_predecessors)
 *                          The 0 makes a 9.8 to 0
 * @param Analog_Predecessors false/true if the last ROI is an analog or digit ROI (default=false)
 *                              runs in special handling because analog is much less precise
 * @param digitAnalogTransitionStart start of the transitionlogic begins on number_of_predecessor (default=9.2)
 *
 * @return int the determined number of the current ROI
 */
int ClassFlowCNNGeneral::PointerEvalHybrid(float number, float number_of_predecessors, int eval_predecessors, bool Analog_Predecessors, float digitAnalogTransitionStart)
{
    int result = -1;
    int result_after_decimal_point = ((int)floor(number * 10.0f)) % 10;
    int result_before_decimal_point = ((int)floor(number) + 10) % 10;

    if (eval_predecessors < 0)
    {
        // on first digit is no spezial logic for transition needed
        // we use the recognition as given. The result is the int value of the recognition
        // add precisition of 2 digits and round before trunc
        // a number greater than 9.994999 is returned as 10, this leads to an error during the decimal shift because the NUMBERS[j]->ReturnRawValue is one digit longer.
        // To avoid this, an additional test must be carried out, see "if ((CNNType == DoubleHyprid10) || (CNNType == Digit100))" check in getReadout()
        result = (int) (trunc(round((float)((int)(number + 10.0f) % 10) * 100.0f)) / 100.0f);

        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "PointerEvalHybrid - No predecessor - Result = " + std::to_string(result) + " number: " + std::to_string(number) + " number_of_predecessors = " + std::to_string(number_of_predecessors) + " eval_predecessors = " + std::to_string(eval_predecessors) + " Digit_Uncertainty = " + std::to_string(Digit_Uncertainty));
        return result;
    }

    if (Analog_Predecessors)
    {
        result = PointerEvalAnalogToDigit(number, number_of_predecessors, eval_predecessors, digitAnalogTransitionStart);
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "PointerEvalHybrid - Analog predecessor, evaluation over PointerEvalAnalog = " + std::to_string(result) + " number: " + std::to_string(number) + " number_of_predecessors = " + std::to_string(number_of_predecessors) + " eval_predecessors = " + std::to_string(eval_predecessors) + " Digit_Uncertainty = " + std::to_string(Digit_Uncertainty));
        return result;
    }

    if ((number_of_predecessors >= Digit_Transition_Area_Predecessor) && (number_of_predecessors <= (10.0 - Digit_Transition_Area_Predecessor)))
    {
        // no digit change, because predecessor is far enough away (0+/-DigitTransitionRangePredecessor) --> number is rounded
        // Band around the digit --> Round off, as digit reaches inaccuracy in the frame
        if ((result_after_decimal_point <= DigitBand) || (result_after_decimal_point >= (10 - DigitBand)))
        {
            result = ((int)round(number) + 10) % 10;
        }
        else
        {
            result = ((int)trunc(number) + 10) % 10;
        }

        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "PointerEvalHybrid - NO analogue predecessor, no change of digits, as pre-decimal point far enough away = " + std::to_string(result) + " number: " + std::to_string(number) + " number_of_predecessors = " + std::to_string(number_of_predecessors) + " eval_predecessors = " + std::to_string(eval_predecessors) + " Digit_Uncertainty = " + std::to_string(Digit_Uncertainty));
        return result;
    }

    // Zero crossing at the predecessor has taken place (! evaluation via Prev_value and not number!) --> round up here (2.8 --> 3, but also 3.1 --> 3)
    if (eval_predecessors <= 1)
    {
        // We simply assume that the current digit after the zero crossing of the predecessor
        // has passed through at least half (x.5)
        if (result_after_decimal_point > 5)
        {
            // The current digit does not yet have a zero crossing, but the predecessor does..
            result = (result_before_decimal_point + 1) % 10;
        }
        else
        {
            // Act. digit and predecessor have zero crossing
            result = result_before_decimal_point % 10;
        }
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "PointerEvalHybrid - NO analogue predecessor, zero crossing has taken placen = " + std::to_string(result) + " number: " + std::to_string(number) + " number_of_predecessors = " + std::to_string(number_of_predecessors) + " eval_predecessors = " + std::to_string(eval_predecessors) + " Digit_Uncertainty = " + std::to_string(Digit_Uncertainty));
        return result;
    }

    // remains only >= 9.x --> no zero crossing yet --> 2.8 --> 2,
    // and from 9.7(DigitTransitionRangeLead) 3.1 --> 2
    // everything >=x.4 can be considered as current number in transition. With 9.x predecessor the current
    // number can still be x.6 - x.7.
    // Preceding (else - branch) does not already happen from 9.
    if (Digit_Transition_Area_Forward >= number_of_predecessors || result_after_decimal_point >= 4)
    {
        // The current digit, like the previous digit, does not yet have a zero crossing.
        result = result_before_decimal_point % 10;
    }
    else
    {
        // current digit precedes the smaller digit (9.x). So already >=x.0 while the previous digit has not yet
        // has no zero crossing. Therefore, it is reduced by 1.
        result = (result_before_decimal_point + 9) % 10;
    }

    LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "PointerEvalHybrid - O analogue predecessor, >= 9.5 --> no zero crossing yet = " + std::to_string(result) + " number: " + std::to_string(number) + " number_of_predecessors = " + std::to_string(number_of_predecessors) + " eval_predecessors = " + std::to_string(eval_predecessors) + " Digit_Uncertainty = " + std::to_string(Digit_Uncertainty) + " result_after_decimal_point = " + std::to_string(result_after_decimal_point));
    return result;
}

int ClassFlowCNNGeneral::PointerEvalAnalogToDigit(float number, float numeral_preceder, int eval_predecessors, float AnalogToDigitTransitionStart)
{
    int result = -1;
    int result_after_decimal_point = ((int)floor(number * 10.0f)) % 10;
    int result_before_decimal_point = ((int)floor(number) + 10) % 10;
    bool roundedUp = false;

    // Within the digit inequalities
    // Band around the digit --> Round off, as digit reaches inaccuracy in the frame
    if ((result_after_decimal_point >= (10 - (int)(Digit_Uncertainty * 10.0f))) || (eval_predecessors <= 4 && result_after_decimal_point >= 6))
    {
        // or digit runs after (analogue =0..4, digit >=6)
        result = (int)(round(number) + 10) % 10;
        roundedUp = true;
        // before/ after decimal point, because we adjust the number based on the uncertainty.
        result_after_decimal_point = ((int)floor(result * 10)) % 10;
        result_before_decimal_point = ((int)floor(result) + 10) % 10;
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "PointerEvalAnalogToDigit - Digit Uncertainty - Result = " + std::to_string(result) + " number: " + std::to_string(number) + " numeral_preceder: " + std::to_string(numeral_preceder) + " erg before comma: " + std::to_string(result_before_decimal_point) + " erg after comma: " + std::to_string(result_after_decimal_point));
    }
    else
    {
        result = (int)((int)trunc(number) + 10) % 10;
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "PointerEvalAnalogToDigit - NO digit Uncertainty - Result = " + std::to_string(result) + " number: " + std::to_string(number) + " numeral_preceder = " + std::to_string(numeral_preceder));
    }

    // No zero crossing has taken place.
    // Only eval_predecessors used because numeral_preceder could be wrong here.
    // numeral_preceder<=0.1 & eval_predecessors=9 corresponds to analogue was reset because of previous analogue that are not yet at 0.
    if ((eval_predecessors >= 6 && (numeral_preceder > AnalogToDigitTransitionStart || numeral_preceder <= 0.2) && roundedUp))
    {
        result = ((result_before_decimal_point + 10) - 1) % 10;
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "PointerEvalAnalogToDigit - Nulldurchgang noch nicht stattgefunden = " + std::to_string(result) + " number: " + std::to_string(number) + " numeral_preceder = " + std::to_string(numeral_preceder) + " eerg after comma = " + std::to_string(result_after_decimal_point));
    }

    return result;
}

int ClassFlowCNNGeneral::PointerEvalAnalog(float number, int numeral_preceder)
{
    int result = -1;

    if (numeral_preceder == -1)
    {
        result = (int)floor(number);
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "PointerEvalAnalog - No predecessor - Result = " + std::to_string(result) + " number: " + std::to_string(number) + " numeral_preceder = " + std::to_string(numeral_preceder) + " Analog_error = " + std::to_string(Analog_error));
        return result;
    }

    float number_min = number - (float)Analog_error / 10.0f;
    float number_max = number + (float)Analog_error / 10.0f;

    if ((int)floor(number_max) - (int)floor(number_min) != 0)
    {
        if (numeral_preceder <= Analog_error)
        {
            result = ((int)floor(number_max) + 10) % 10;
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "PointerEvalAnalog - number ambiguous, correction upwards - result = " + std::to_string(result) + " number: " + std::to_string(number) + " numeral_preceder = " + std::to_string(numeral_preceder) + " Analog_error = " + std::to_string(Analog_error));
            return result;
        }
        if (numeral_preceder >= (10 - Analog_error))
        {
            result = ((int)floor(number_min) + 10) % 10;
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "PointerEvalAnalog - number ambiguous, downward correction - result = " + std::to_string(result) + " number: " + std::to_string(number) + " numeral_preceder = " + std::to_string(numeral_preceder) + " Analog_error = " + std::to_string(Analog_error));
            return result;
        }
    }

    result = ((int)floor(number) + 10) % 10;
    LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "PointerEvalAnalog - number unambiguous, no correction necessary - result = " + std::to_string(result) + " number: " + std::to_string(number) + " numeral_preceder = " + std::to_string(numeral_preceder) + " Analog_error = " + std::to_string(Analog_error));

    return result;
}

bool ClassFlowCNNGeneral::ReadParameter(FILE *pfile, std::string &aktparamgraph)
{
    aktparamgraph = trim_string_left_right(aktparamgraph);
    if (aktparamgraph.size() == 0)
    {
        if (!GetNextParagraph(pfile, aktparamgraph))
        {
            return false;
        }
    }

    if ((to_upper(aktparamgraph).compare("[ANALOG]") != 0) && (to_upper(aktparamgraph).compare(";[ANALOG]") != 0) &&
        (to_upper(aktparamgraph).compare("[DIGIT]") != 0) && (to_upper(aktparamgraph).compare(";[DIGIT]") != 0) &&
        (to_upper(aktparamgraph).compare("[DIGITS]") != 0) && (to_upper(aktparamgraph).compare(";[DIGITS]") != 0))
    {
        // Paragraph passt nicht
        return false;
    }

    if (aktparamgraph[0] == ';')
    {
        disabled = true;
        while (getNextLine(pfile, &aktparamgraph) && !isNewParagraph(aktparamgraph))
            ;
        ESP_LOGD(TAG, "[Analog/Digit] is disabled!");

        return true;
    }

    std::vector<std::string> splitted;

    while (getNextLine(pfile, &aktparamgraph) && !isNewParagraph(aktparamgraph))
    {
        splitted = split_line(aktparamgraph);

        if (splitted.size() > 1)
        {
            std::string _param = to_upper(splitted[0]);

            if (_param == "ROIIMAGESLOCATION")
            {
                imagesLocation = "/sdcard" + splitted[1];
                isLogImage = true;
            }
            else if (_param == "LOGIMAGESELECT")
            {
                LogImageSelect = splitted[1];
                isLogImageSelect = true;
            }
            else if (_param == "ROIIMAGESRETENTION")
            {
                if (is_string_numeric(splitted[1]))
                {
                    imagesRetention = std::stoi(splitted[1]);
                }
            }
            else if (_param == "MODEL")
            {
                cnn_model_file = splitted[1];
            }
            else if (_param == "CNNGOODTHRESHOLD")
            {
                if (is_string_numeric(splitted[1]))
                {
                    CNNGoodThreshold = std::stof(splitted[1]);
                }
            }
            else if (splitted.size() >= 5)
            {
                general *_general = GetGENERAL(splitted[0], true);
                roi *new_roi = _general->ROI[_general->ROI.size() - 1];

                if (is_string_numeric(splitted[1]) && is_string_numeric(splitted[2]) && is_string_numeric(splitted[3]) && is_string_numeric(splitted[4]))
                {
                    new_roi->pos_x = std::stoi(splitted[1]);
                    new_roi->pos_y = std::stoi(splitted[2]);

                    new_roi->delta_x = std::stoi(splitted[3]);
                    new_roi->delta_y = std::stoi(splitted[4]);
                }
                else
                {
                    new_roi->pos_x = 0;
                    new_roi->pos_y = 0;

                    new_roi->delta_x = 30;
                    new_roi->delta_y = 30;
                }

                new_roi->ccw = false;
                if (splitted.size() >= 6)
                {
                    new_roi->ccw = alphanumeric_to_boolean(splitted[5]);
                }

                new_roi->result_float = -1;
                new_roi->image = NULL;
                new_roi->image_org = NULL;
            }
        }
    }

    if (!getNetworkParameter())
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "An error occured on setting up the Network -> Disabling it!");
        disabled = true; // An error occured, disable this CNN!
        return false;
    }

    for (int i = 0; i < GENERAL.size(); ++i)
    {
        for (int j = 0; j < GENERAL[i]->ROI.size(); ++j)
        {
            GENERAL[i]->ROI[j]->image = new CImageBasis("ROI " + GENERAL[i]->ROI[j]->name, model_x_size, model_y_size, model_channel);
            GENERAL[i]->ROI[j]->image_org = new CImageBasis("ROI " + GENERAL[i]->ROI[j]->name + " original", GENERAL[i]->ROI[j]->delta_x, GENERAL[i]->ROI[j]->delta_y, 3);
        }
    }

    return true;
}

general *ClassFlowCNNGeneral::FindGENERAL(std::string _name_number)
{
    for (int i = 0; i < GENERAL.size(); ++i)
    {
        if (GENERAL[i]->name == _name_number)
        {
            return GENERAL[i];
        }
    }

    return NULL;
}

general *ClassFlowCNNGeneral::GetGENERAL(std::string _name, bool _create = true)
{
    std::string _number_sequence, _roi_name;
    int _pospunkt = _name.find_first_of(".");

    if (_pospunkt > -1)
    {
        _number_sequence = _name.substr(0, _pospunkt);
        _roi_name = _name.substr(_pospunkt + 1, _name.length() - _pospunkt - 1);
    }
    else
    {
        _number_sequence = "default";
        _roi_name = _name;
    }

    general *_ret = NULL;

    for (int i = 0; i < GENERAL.size(); ++i)
    {
        if (GENERAL[i]->name == _number_sequence)
        {
            _ret = GENERAL[i];
        }
    }

    // not found and should not be created
    if (!_create)
    {
        return _ret;
    }

    if (_ret == NULL)
    {
        _ret = new general;
        _ret->name = _number_sequence;
        GENERAL.push_back(_ret);
    }

    roi *new_roi = new roi;
    new_roi->name = _roi_name;

    _ret->ROI.push_back(new_roi);

    ESP_LOGD(TAG, "GetGENERAL - GENERAL %s - roi %s - ccw: %d", _number_sequence.c_str(), _roi_name.c_str(), new_roi->ccw);

    return _ret;
}

std::string ClassFlowCNNGeneral::getHTMLSingleStep(std::string host)
{
    std::vector<HTMLInfo *> html_info;

    std::string result = "<p>Found ROIs: </p> <p><img src=\"" + host + "/img_tmp/alg_roi.jpg\"></p>\n";
    result = result + "Analog Pointers: <p> ";

    html_info = GetHTMLInfo();

    for (int i = 0; i < html_info.size(); ++i)
    {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(1) << html_info[i]->val;
        std::string temp_string = stream.str();

        result = result + "<img src=\"" + host + "/img_tmp/" + html_info[i]->filename + "\"> " + temp_string;
        delete html_info[i];
    }

    html_info.clear();

    return result;
}

bool ClassFlowCNNGeneral::doFlow(std::string time_value)
{
    if (disabled)
    {
        return true;
    }

    if (!doAlignAndCut(time_value))
    {
        return false;
    }

    LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "doFlow after alignment");
    doNeuralNetwork(time_value);
    RemoveOldLogs();

    return true;
}

bool ClassFlowCNNGeneral::doAlignAndCut(std::string time_value)
{
    if (disabled)
    {
        return true;
    }

    CAlignAndCutImage *caic = flowpostalignment->GetAlignAndCutImage();

    for (int _number = 0; _number < GENERAL.size(); ++_number)
    {
        for (int _roi = 0; _roi < GENERAL[_number]->ROI.size(); ++_roi)
        {
            ESP_LOGD(TAG, "General %d - Align&Cut", _roi);

            caic->CutAndSave(GENERAL[_number]->ROI[_roi]->pos_x, GENERAL[_number]->ROI[_roi]->pos_y, GENERAL[_number]->ROI[_roi]->delta_x, GENERAL[_number]->ROI[_roi]->delta_y, GENERAL[_number]->ROI[_roi]->image_org);
            if (Camera.SaveAllFiles)
            {
                if (GENERAL[_number]->name == "default")
                {
                    GENERAL[_number]->ROI[_roi]->image_org->SaveToFile(format_filename("/sdcard/img_tmp/" + GENERAL[_number]->ROI[_roi]->name + ".jpg"));
                }
                else
                {
                    GENERAL[_number]->ROI[_roi]->image_org->SaveToFile(format_filename("/sdcard/img_tmp/" + GENERAL[_number]->name + "_" + GENERAL[_number]->ROI[_roi]->name + ".jpg"));
                }
            }

            GENERAL[_number]->ROI[_roi]->image_org->Resize(model_x_size, model_y_size, GENERAL[_number]->ROI[_roi]->image);
            if (Camera.SaveAllFiles)
            {
                if (GENERAL[_number]->name == "default")
                {
                    GENERAL[_number]->ROI[_roi]->image->SaveToFile(format_filename("/sdcard/img_tmp/" + GENERAL[_number]->ROI[_roi]->name + ".jpg"));
                }
                else
                {
                    GENERAL[_number]->ROI[_roi]->image->SaveToFile(format_filename("/sdcard/img_tmp/" + GENERAL[_number]->name + "_" + GENERAL[_number]->ROI[_roi]->name + ".jpg"));
                }
            }
        }
    }

    return true;
}

void ClassFlowCNNGeneral::DrawROI(CImageBasis *Image)
{
    if (Image->ImageOkay())
    {
        if (CNNType == Analogue || CNNType == Analogue100)
        {
            int r = 0;
            int g = 255;
            int b = 0;

            for (int _ana = 0; _ana < GENERAL.size(); ++_ana)
            {
                for (int _roi = 0; _roi < GENERAL[_ana]->ROI.size(); ++_roi)
                {
                    Image->drawRect(GENERAL[_ana]->ROI[_roi]->pos_x, GENERAL[_ana]->ROI[_roi]->pos_y, GENERAL[_ana]->ROI[_roi]->delta_x, GENERAL[_ana]->ROI[_roi]->delta_y, r, g, b, 1);
                    Image->drawEllipse((int)(GENERAL[_ana]->ROI[_roi]->pos_x + GENERAL[_ana]->ROI[_roi]->delta_x / 2), (int)(GENERAL[_ana]->ROI[_roi]->pos_y + GENERAL[_ana]->ROI[_roi]->delta_y / 2), (int)(GENERAL[_ana]->ROI[_roi]->delta_x / 2), (int)(GENERAL[_ana]->ROI[_roi]->delta_y / 2), r, g, b, 2);
                    Image->drawLine((int)(GENERAL[_ana]->ROI[_roi]->pos_x + GENERAL[_ana]->ROI[_roi]->delta_x / 2), (int)GENERAL[_ana]->ROI[_roi]->pos_y, (int)(GENERAL[_ana]->ROI[_roi]->pos_x + GENERAL[_ana]->ROI[_roi]->delta_x / 2), (int)(GENERAL[_ana]->ROI[_roi]->pos_y + GENERAL[_ana]->ROI[_roi]->delta_y), r, g, b, 2);
                    Image->drawLine((int)GENERAL[_ana]->ROI[_roi]->pos_x, (int)(GENERAL[_ana]->ROI[_roi]->pos_y + GENERAL[_ana]->ROI[_roi]->delta_y / 2), (int)GENERAL[_ana]->ROI[_roi]->pos_x + GENERAL[_ana]->ROI[_roi]->delta_x, (int)(GENERAL[_ana]->ROI[_roi]->pos_y + GENERAL[_ana]->ROI[_roi]->delta_y / 2), r, g, b, 2);
                }
            }
        }
        else
        {
            for (int _dig = 0; _dig < GENERAL.size(); ++_dig)
            {
                for (int _roi = 0; _roi < GENERAL[_dig]->ROI.size(); ++_roi)
                {
                    Image->drawRect(GENERAL[_dig]->ROI[_roi]->pos_x, GENERAL[_dig]->ROI[_roi]->pos_y, GENERAL[_dig]->ROI[_roi]->delta_x, GENERAL[_dig]->ROI[_roi]->delta_y, 0, 0, (255 - _dig * 100), 2);
                }
            }
        }
    }
}

bool ClassFlowCNNGeneral::getNetworkParameter(void)
{
    if (disabled)
    {
        return true;
    }

    std::string temp_cnn = "/sdcard" + cnn_model_file;
    temp_cnn = format_filename(temp_cnn);
    ESP_LOGD(TAG, "%s", temp_cnn.c_str());

    CTfLiteClass *tflite = new CTfLiteClass;

    if (!tflite->LoadModel(temp_cnn))
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Can't load tflite model " + cnn_model_file + " -> Init aborted!");
        LogFile.WriteHeapInfo("getNetworkParameter-LoadModel");
        delete tflite;
        return false;
    }

    if (!tflite->MakeAllocate())
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Can't allocate tflite model -> Init aborted!");
        LogFile.WriteHeapInfo("getNetworkParameter-MakeAllocate");
        delete tflite;
        return false;
    }

    if (CNNType == AutoDetect)
    {
        tflite->GetInputDimension(false);

        model_x_size = tflite->ReadInputDimenstion(0);
        model_y_size = tflite->ReadInputDimenstion(1);
        model_channel = tflite->ReadInputDimenstion(2);

        int anz_output_dimensions = tflite->GetAnzOutPut();
        switch (anz_output_dimensions)
        {
        case 2:
            CNNType = Analogue;
            ESP_LOGD(TAG, "TFlite-Type set to Analogue");
            break;
        case 10:
            CNNType = DoubleHyprid10;
            ESP_LOGD(TAG, "TFlite-Type set to DoubleHyprid10");
            break;
        case 11:
            CNNType = Digit;
            ESP_LOGD(TAG, "TFlite-Type set to Digit");
            break;
        // case 20:
        //    CNNType = DigitHyprid10;
        //    ESP_LOGD(TAG, "TFlite-Type set to DigitHyprid10");
        //    break;
        // case 22:
        //    CNNType = DigitHyprid;
        //    ESP_LOGD(TAG, "TFlite-Type set to DigitHyprid");
        //    break;
        case 100:
            if (model_x_size == 32 && model_y_size == 32)
            {
                CNNType = Analogue100;
                ESP_LOGD(TAG, "TFlite-Type set to Analogue100");
            }
            else
            {
                CNNType = Digit100;
                ESP_LOGD(TAG, "TFlite-Type set to Digit");
            }
            break;
        default:
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "tflite does not fit the firmware (outout_dimension=" + std::to_string(anz_output_dimensions) + ")");
        }
    }

    delete tflite;
    return true;
}

// wird von "bool ClassFlowCNNGeneral::doFlow(std::string time_value)" aufgerufen
bool ClassFlowCNNGeneral::doNeuralNetwork(std::string time_value)
{
    if (disabled)
    {
        return false;
    }

    std::string cnn_model = "/sdcard" + cnn_model_file;
    cnn_model = format_filename(cnn_model);
    ESP_LOGD(TAG, "%s", cnn_model.c_str());

    CTfLiteClass *tflite = new CTfLiteClass;

    if (!tflite->LoadModel(cnn_model))
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Can't load tflite model " + cnn_model_file + " -> Exec aborted this round!");
        LogFile.WriteHeapInfo("doNeuralNetwork-LoadModel");
        delete tflite;
        return false;
    }

    if (!tflite->MakeAllocate())
    {
        LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Can't allocate tfilte model -> Exec aborted this round!");
        LogFile.WriteHeapInfo("doNeuralNetwork-MakeAllocate");
        delete tflite;
        return false;
    }

    std::string logPath = CreateLogFolder(time_value);

    std::vector<NumberPost *> numbers = flowctrl.getNumbers();

    time_t _imagetime; // in seconds
    time(&_imagetime);
    localtime(&_imagetime);
    LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "doNeuralNetwork, _imagetime: " + std::to_string(_imagetime));

    // For each NUMBER
    for (int j = 0; j < GENERAL.size(); ++j)
    {
        LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Processing Number '" + GENERAL[j]->name + "'");

        int start_roi = 0;

        if ((numbers[j]->useMaxFlowRate) && (numbers[j]->PreValueValid) && (numbers[j]->timeStampLastValue == numbers[j]->timeStampLastPreValue))
        {
            int _AnzahlDigit = numbers[j]->AnzahlDigit;
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "doNeuralNetwork, _AnzahlDigit: " + std::to_string(_AnzahlDigit));

            int _AnzahlAnalog = numbers[j]->AnzahlAnalog;
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "doNeuralNetwork, _AnzahlAnalog: " + std::to_string(_AnzahlAnalog));

            float _MaxFlowRate = (numbers[j]->MaxFlowRate / 60); // in unit/minutes
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "doNeuralNetwork, _MaxFlowRate: " + std::to_string(_MaxFlowRate));

            float _LastPreValueTimeDifference = (float)((difftime(_imagetime, numbers[j]->timeStampLastPreValue)) / 60); // in minutes
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "doNeuralNetwork, _LastPreValueTimeDifference: " + std::to_string(_LastPreValueTimeDifference) + " minutes");

            std::string _PreValue_old = std::to_string((float)numbers[j]->PreValue);
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "doNeuralNetwork, _PreValue_old: " + _PreValue_old);

            ////////////////////////////////////////////////////
            std::string _PreValue_new1 = std::to_string((float)numbers[j]->PreValue + (_MaxFlowRate * _LastPreValueTimeDifference));
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "doNeuralNetwork, _PreValue_new1: " + _PreValue_new1);

            std::string _PreValue_new2 = std::to_string((float)numbers[j]->PreValue - (_MaxFlowRate * _LastPreValueTimeDifference));
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "doNeuralNetwork, _PreValue_new2: " + _PreValue_new2);

            ////////////////////////////////////////////////////
            // is necessary because there are always 6 numbers after the DecimalPoint due to float
            int _CorrectionValue = 0;
            int _pospunkt = _PreValue_old.find_first_of(".");
            if (_pospunkt > -1)
            {
                _CorrectionValue = _PreValue_old.length() - _pospunkt;
                _CorrectionValue = _CorrectionValue - numbers[j]->Nachkomma;
            }
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "doNeuralNetwork, _CorrectionValue: " + std::to_string(_CorrectionValue));

            int _PreValue_len = ((int)_PreValue_old.length() - _CorrectionValue);
            if (numbers[j]->isExtendedResolution)
            {
                _PreValue_len = _PreValue_len - 1;
            }
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "doNeuralNetwork, _PreValue_len(without DecimalPoint and ExtendedResolution): " + std::to_string(_PreValue_len));

            ////////////////////////////////////////////////////
            // (+) Find out which Numbers should not change
            int _DecimalPoint1 = 0;
            int _NumbersNotChanged1 = 0;
            while ((_PreValue_old.length() > _NumbersNotChanged1) && (_PreValue_old[_NumbersNotChanged1] == _PreValue_new1[_NumbersNotChanged1]))
            {
                if (_PreValue_old[_NumbersNotChanged1] == '.')
                {
                    _DecimalPoint1 = 1;
                }
                _NumbersNotChanged1++;
            }
            _NumbersNotChanged1 = _NumbersNotChanged1 - _DecimalPoint1;
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Number of ROIs that should not change: " + std::to_string(_NumbersNotChanged1));

            if ((_AnzahlDigit + _AnzahlAnalog) > _PreValue_len)
            {
                _NumbersNotChanged1 = _NumbersNotChanged1 + ((_AnzahlDigit + _AnzahlAnalog) - _PreValue_len);
            }
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Number of ROIs that should not change(corrected): " + std::to_string(_NumbersNotChanged1));

            ////////////////////////////////////////////////////
            // (-) Find out which Numbers should not change
            int _NumbersNotChanged2 = _NumbersNotChanged1;
            if (numbers[j]->AllowNegativeRates)
            {
                int _DecimalPoint2 = 0;
                while ((_PreValue_old.length() > _NumbersNotChanged2) && (_PreValue_old[_NumbersNotChanged2] == _PreValue_new2[_NumbersNotChanged2]))
                {
                    if (_PreValue_old[_NumbersNotChanged2] == '.')
                    {
                        _DecimalPoint2 = 1;
                    }
                    _NumbersNotChanged2++;
                }
                _NumbersNotChanged2 = _NumbersNotChanged2 - _DecimalPoint2;
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Number of ROIs that should not change: " + std::to_string(_NumbersNotChanged2));

                if ((_AnzahlDigit + _AnzahlAnalog) > _PreValue_len)
                {
                    _NumbersNotChanged2 = _NumbersNotChanged2 + ((_AnzahlDigit + _AnzahlAnalog) - _PreValue_len);
                }
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Number of ROIs that should not change(corrected): " + std::to_string(_NumbersNotChanged2));
            }

            ////////////////////////////////////////////////////
            int start_digit_new = 0;
            int start_analog_new = 0;
            int _NumbersNotChanged = min(_NumbersNotChanged1, _NumbersNotChanged2);
            if (_NumbersNotChanged <= _AnzahlDigit)
            {
                // The change already takes place at the digit ROIs
                start_digit_new = (_AnzahlDigit - _NumbersNotChanged);
                start_digit_new = (_AnzahlDigit - start_digit_new);
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "From the " + std::to_string(start_digit_new) + " th digit ROI is evaluated");
            }
            else
            {
                // The change only takes place at the analog ROIs
                start_digit_new = _AnzahlDigit;
                start_analog_new = (_AnzahlAnalog - (_NumbersNotChanged - _AnzahlDigit));
                start_analog_new = (_AnzahlAnalog - start_analog_new);
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "From the " + std::to_string(start_analog_new) + " th analog ROI is evaluated");
            }

            ////////////////////////////////////////////////////
            if (CNNType == Digit || CNNType == Digit100 || CNNType == DoubleHyprid10)
            {
                start_roi = start_digit_new;
            }
            else if (CNNType == Analogue || CNNType == Analogue100)
            {
                start_roi = start_analog_new;
            }
        }

        // For each ROI
        for (int i = 0; i < GENERAL[j]->ROI.size(); ++i)
        {
            LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "ROI #" + std::to_string(i) + " - TfLite");

            switch (CNNType)
            {
            case Analogue:
            {
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "CNN Type: Analogue");

                tflite->LoadInputImageBasis(GENERAL[j]->ROI[i]->image);

                tflite->Invoke();
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Analogue - After Invoke");

                float _value1 = tflite->GetOutputValue(0);
                float _value2 = tflite->GetOutputValue(1);

                float _result = fmod((atan2(_value1, _value2) / (M_PI * 2.0f) + 2.0f), 1.0f);

                if (GENERAL[j]->ROI[i]->ccw)
                {
                    _result = 10.0f - (_result * 10.0f);
                }
                else
                {
                    _result = _result * 10.0f;
                }

                if (i >= start_roi)
                {
                    GENERAL[j]->ROI[i]->result_float = _result;
                }

                GENERAL[j]->ROI[i]->raw_result_float = _result;

                if ((_result < 0.0f) || (_result >= 10.0f))
                {
                    GENERAL[j]->ROI[i]->isReject = true;
                }
                else
                {
                    GENERAL[j]->ROI[i]->isReject = false;
                }

                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "General result (Analog) - roi_" + std::to_string(i) + ": " + std::to_string(GENERAL[j]->ROI[i]->raw_result_float));
                ESP_LOGD(TAG, "General result (Analog) - roi_%i - ccw: %d -  %f", i, GENERAL[j]->ROI[i]->ccw, GENERAL[j]->ROI[i]->raw_result_float);

                if (isLogImage)
                {
                    std::string _image_name = GENERAL[j]->name + "_" + GENERAL[j]->ROI[i]->name;

                    if (isLogImageSelect)
                    {
                        if (LogImageSelect.find(GENERAL[j]->ROI[i]->name) != std::string::npos)
                        {
                            LogImage(logPath, _image_name, &GENERAL[j]->ROI[i]->raw_result_float, NULL, time_value, GENERAL[j]->ROI[i]->image_org);
                        }
                    }
                }
            }
            break;

            case Digit:
            {
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "CNN Type: Digit");

                int _result = tflite->GetClassFromImageBasis(GENERAL[j]->ROI[i]->image);

                if (i >= start_roi)
                {
                    GENERAL[j]->ROI[i]->result_klasse = _result;
                }

                GENERAL[j]->ROI[i]->raw_result_klasse = _result;

                if ((_result < 0) || (_result >= 10))
                {
                    GENERAL[j]->ROI[i]->isReject = true;
                }
                else
                {
                    GENERAL[j]->ROI[i]->isReject = false;
                }

                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "General result (Digit) - roi_" + std::to_string(i) + ": " + std::to_string(GENERAL[j]->ROI[i]->raw_result_klasse));
                ESP_LOGD(TAG, "General result (Digit) - roi_%i: %d", i, GENERAL[j]->ROI[i]->raw_result_klasse);

                if (isLogImage)
                {
                    std::string _image_name = GENERAL[j]->name + "_" + GENERAL[j]->ROI[i]->name;

                    if (isLogImageSelect)
                    {
                        if (LogImageSelect.find(GENERAL[j]->ROI[i]->name) != std::string::npos)
                        {
                            LogImage(logPath, _image_name, NULL, &GENERAL[j]->ROI[i]->raw_result_klasse, time_value, GENERAL[j]->ROI[i]->image_org);
                        }
                    }
                }
            }
            break;

            case DoubleHyprid10:
            {
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "CNN Type: DoubleHyprid10");

                tflite->LoadInputImageBasis(GENERAL[j]->ROI[i]->image);

                tflite->Invoke();
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "DoubleHyprid10 - After Invoke");

                int _num = tflite->GetOutClassification(0, 9);
                int _numplus = (_num + 1) % 10;
                int _numminus = (_num - 1 + 10) % 10;

                float _value = tflite->GetOutputValue(_num);
                float _valueplus = tflite->GetOutputValue(_numplus);
                float _valueminus = tflite->GetOutputValue(_numminus);

                float _result = (float)_num - _valueminus / (_value + _valueminus);
                float _fit = _value + _valueminus;

                if (_valueplus > _valueminus)
                {
                    _result = (float)_num + _valueplus / (_valueplus + _value);
                    _fit = _value + _valueplus;
                }

                std::string temp_bufer = "DoubleHyprid10 - _num (plus, minus): " + std::to_string(_num) + " (" + std::to_string(_numplus) + ", " + std::to_string(_numminus) + ")";
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, temp_bufer);

                temp_bufer = "DoubleHyprid10 - _val (plus, minus): " + std::to_string(_value) + " (" + std::to_string(_valueplus) + ", " + std::to_string(_valueminus) + ")";
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, temp_bufer);

                temp_bufer = "DoubleHyprid10 - _result: " + std::to_string(_result) + ", _fit: " + std::to_string(_fit);
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, temp_bufer);

                float _result_save_file = _result;

                if (_fit < CNNGoodThreshold)
                {
                    GENERAL[j]->ROI[i]->isReject = true;
                    _result = -1;
                    temp_bufer = "DoubleHyprid10 - Value Rejected due to Threshold (Fit: " + std::to_string(_fit) + ", Threshold: " + std::to_string(CNNGoodThreshold) + ")";
                    LogFile.WriteToFile(ESP_LOG_WARN, TAG, temp_bufer);
                }
                else
                {
                    GENERAL[j]->ROI[i]->isReject = false;
                }

                if (GENERAL[j]->ROI[i]->ccw)
                {
                }

                if (i >= start_roi)
                {
                    GENERAL[j]->ROI[i]->result_float = _result;
                }

                GENERAL[j]->ROI[i]->raw_result_float = _result;

                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Result General(DoubleHyprid10) - roi_" + std::to_string(i) + ": " + std::to_string(GENERAL[j]->ROI[i]->raw_result_float));
                ESP_LOGD(TAG, "Result General(DoubleHyprid10) - roi_%i: %f", i, GENERAL[j]->ROI[i]->raw_result_float);

                if (isLogImage)
                {
                    std::string _image_name = GENERAL[j]->name + "_" + GENERAL[j]->ROI[i]->name;

                    if (isLogImageSelect)
                    {
                        if (LogImageSelect.find(GENERAL[j]->ROI[i]->name) != std::string::npos)
                        {
                            LogImage(logPath, _image_name, &_result_save_file, NULL, time_value, GENERAL[j]->ROI[i]->image_org);
                        }
                    }
                }
            }
            break;

            case Digit100:
            case Analogue100:
            {
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "CNN Type: Digit100 or Analogue100");

                tflite->LoadInputImageBasis(GENERAL[j]->ROI[i]->image);
                tflite->Invoke();

                int _num = tflite->GetOutClassification();
                float _result = 0.0f;

                if (GENERAL[j]->ROI[i]->ccw)
                {
                    _result = 10.0f - ((float)_num / 10.0f);
                }
                else
                {
                    _result = (float)_num / 10.0f;
                }

                if (i >= start_roi)
                {
                    GENERAL[j]->ROI[i]->result_float = _result;
                }

                GENERAL[j]->ROI[i]->raw_result_float = _result;

                if ((_result < 0.0f) || (_result >= 10.0f))
                {
                    GENERAL[j]->ROI[i]->isReject = true;
                }
                else
                {
                    GENERAL[j]->ROI[i]->isReject = false;
                }

                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "Result General(Digit100 or Analogue100) - roi_" + std::to_string(i) + ": " + std::to_string(GENERAL[j]->ROI[i]->raw_result_float));
                ESP_LOGD(TAG, "Result General(Digit100 or Analogue100) - roi_%i - ccw: %d - %f", i, GENERAL[j]->ROI[i]->ccw, GENERAL[j]->ROI[i]->raw_result_float);

                if (isLogImage)
                {
                    std::string _image_name = GENERAL[j]->name + "_" + GENERAL[j]->ROI[i]->name;

                    if (isLogImageSelect)
                    {
                        if (LogImageSelect.find(GENERAL[j]->ROI[i]->name) != std::string::npos)
                        {
                            LogImage(logPath, _image_name, &GENERAL[j]->ROI[i]->raw_result_float, NULL, time_value, GENERAL[j]->ROI[i]->image_org);
                        }
                    }
                }
            }
            break;

            default:
            {
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "CNN Type: unknown");
            }
            break;
            }
        }
    }

    delete tflite;
    return true;
}

bool ClassFlowCNNGeneral::isExtendedResolution(int _number)
{
    if (CNNType == Digit)
    {
        return false;
    }

    return true;
}

std::vector<HTMLInfo *> ClassFlowCNNGeneral::GetHTMLInfo(void)
{
    std::vector<HTMLInfo *> result;

    for (int _number = 0; _number < GENERAL.size(); ++_number)
    {
        for (int _roi = 0; _roi < GENERAL[_number]->ROI.size(); ++_roi)
        {
            ESP_LOGD(TAG, "Image: %d", (int)GENERAL[_number]->ROI[_roi]->image);
            if (GENERAL[_number]->ROI[_roi]->image)
            {
                if (GENERAL[_number]->name == "default")
                {
                    GENERAL[_number]->ROI[_roi]->image->SaveToFile(format_filename("/sdcard/img_tmp/" + GENERAL[_number]->ROI[_roi]->name + ".jpg"));
                }
                else
                {
                    GENERAL[_number]->ROI[_roi]->image->SaveToFile(format_filename("/sdcard/img_tmp/" + GENERAL[_number]->name + "_" + GENERAL[_number]->ROI[_roi]->name + ".jpg"));
                }
            }

            HTMLInfo *temp_info = new HTMLInfo;
            if (GENERAL[_number]->name == "default")
            {
                temp_info->filename = GENERAL[_number]->ROI[_roi]->name + ".jpg";
                temp_info->filename_org = GENERAL[_number]->ROI[_roi]->name + ".jpg";
            }
            else
            {
                temp_info->filename = GENERAL[_number]->name + "_" + GENERAL[_number]->ROI[_roi]->name + ".jpg";
                temp_info->filename_org = GENERAL[_number]->name + "_" + GENERAL[_number]->ROI[_roi]->name + ".jpg";
            }

            if (CNNType == Digit)
            {
                temp_info->val = (float)GENERAL[_number]->ROI[_roi]->raw_result_klasse;
            }
            else
            {
                temp_info->val = GENERAL[_number]->ROI[_roi]->raw_result_float;
            }

            temp_info->image = GENERAL[_number]->ROI[_roi]->image;
            temp_info->image_org = GENERAL[_number]->ROI[_roi]->image_org;

            result.push_back(temp_info);
        }
    }

    return result;
}

int ClassFlowCNNGeneral::getNumberGENERAL(void)
{
    return GENERAL.size();
}

std::string ClassFlowCNNGeneral::getNameGENERAL(int _number)
{
    if (_number < GENERAL.size())
    {
        return GENERAL[_number]->name;
    }

    return "GENERAL DOES NOT EXIST";
}

general *ClassFlowCNNGeneral::GetGENERAL(int _number)
{
    if (_number < GENERAL.size())
    {
        return GENERAL[_number];
    }

    return NULL;
}

void ClassFlowCNNGeneral::UpdateNameNumbers(std::vector<std::string> *_name_numbers)
{
    for (int _number = 0; _number < GENERAL.size(); ++_number)
    {
        std::string _name = GENERAL[_number]->name;
        bool found = false;

        for (int _roi = 0; _roi < (*_name_numbers).size(); ++_roi)
        {
            if ((*_name_numbers)[_roi] == _name)
            {
                found = true;
            }
        }
        if (!found)
        {
            (*_name_numbers).push_back(_name);
        }
    }
}

std::string ClassFlowCNNGeneral::getReadoutRawString(int _number)
{
    std::string temp_string = "";

    if (_number >= GENERAL.size() || GENERAL[_number] == NULL || GENERAL[_number]->ROI.size() == 0)
    {
        return temp_string;
    }

    for (int _roi = 0; _roi < GENERAL[_number]->ROI.size(); ++_roi)
    {
        if (CNNType == Analogue || CNNType == Analogue100)
        {
            if ((GENERAL[_number]->ROI[_roi]->raw_result_float < 0.0f) || (GENERAL[_number]->ROI[_roi]->raw_result_float >= 10.0f))
            {
                temp_string = temp_string + ",N";
            }
            else
            {
                temp_string = temp_string + "," + round_output(GENERAL[_number]->ROI[_roi]->raw_result_float, 1);
            }
        }

        else if (CNNType == Digit)
        {
            if ((GENERAL[_number]->ROI[_roi]->raw_result_klasse < 0) || (GENERAL[_number]->ROI[_roi]->raw_result_klasse >= 10))
            {
                temp_string = temp_string + ",N";
            }
            else
            {
                temp_string = temp_string + "," + round_output(GENERAL[_number]->ROI[_roi]->raw_result_klasse, 0);
            }
        }

        else if ((CNNType == DoubleHyprid10) || (CNNType == Digit100))
        {
            if ((GENERAL[_number]->ROI[_roi]->raw_result_float < 0.0f) || (GENERAL[_number]->ROI[_roi]->raw_result_float >= 10.0f))
            {
                temp_string = temp_string + ",N";
            }
            else
            {
                temp_string = temp_string + "," + round_output(GENERAL[_number]->ROI[_roi]->raw_result_float, 1);
            }
        }
    }

    return temp_string;
}
