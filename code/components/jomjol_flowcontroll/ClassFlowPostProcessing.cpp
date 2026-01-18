#include "defines.h"

#include <iomanip>
#include <sstream>
#include <time.h>
#include <esp_log.h>

#include "ClassFlowPostProcessing.h"
#include "MainFlowControl.h"
#include "ClassFlowTakeImage.h"
#include "ClassLogFile.h"

#include "Helper.h"
#include "time_sntp.h"

static const char *TAG = "POSTPROC";

std::string ClassFlowPostProcessing::getNumbersName()
{
    std::string ret = "";

    for (int i = 0; i < NUMBERS.size(); ++i)
    {
        ret += NUMBERS[i]->name;

        if (i < NUMBERS.size() - 1)
        {
            ret = ret + "\t";
        }
    }

    return ret;
}

std::string ClassFlowPostProcessing::GetJSON(std::string _lineend)
{
    std::string json = "{" + _lineend;

    for (int i = 0; i < NUMBERS.size(); ++i)
    {
        json += "\"" + NUMBERS[i]->name + "\":" + _lineend;
        json += getJsonFromNumber(i, _lineend) + _lineend;

        if ((i + 1) < NUMBERS.size())
        {
            json += "," + _lineend;
        }
    }

    json += "}";

    return json;
}

std::string ClassFlowPostProcessing::getJsonFromNumber(int i, std::string _lineend)
{
    std::string json = "";

    json += "  {" + _lineend;

    if (NUMBERS[i]->ReturnValue.length() > 0)
    {
        json += "    \"value\": \"" + NUMBERS[i]->ReturnValue + "\"," + _lineend;
    }
    else
    {
        json += "    \"value\": \"\"," + _lineend;
    }

    json += "    \"raw\": \"" + NUMBERS[i]->ReturnRawValue + "\"," + _lineend;
    json += "    \"pre\": \"" + NUMBERS[i]->ReturnPreValue + "\"," + _lineend;
    json += "    \"error\": \"" + NUMBERS[i]->ErrorMessageText + "\"," + _lineend;

    if (NUMBERS[i]->ReturnRateValue.length() > 0)
    {
        json += "    \"rate\": \"" + NUMBERS[i]->ReturnRateValue + "\"," + _lineend;
    }
    else
    {
        json += "    \"rate\": \"\"," + _lineend;
    }

    json += "    \"timestamp\": \"" + NUMBERS[i]->timeStamp + "\"" + _lineend;
    json += "  }" + _lineend;

    return json;
}

std::string ClassFlowPostProcessing::GetPreValue(std::string _number)
{
    std::string result;
    int index = -1;

    if (_number == "")
    {
        _number = "default";
    }

    for (int i = 0; i < NUMBERS.size(); ++i)
    {
        if (NUMBERS[i]->name == _number)
        {
            index = i;
        }
    }

    if (index == -1)
    {
        return std::string("");
    }

    result = round_output(NUMBERS[index]->PreValue, NUMBERS[index]->Nachkomma);

    return result;
}

bool ClassFlowPostProcessing::SetPreValue(double _newvalue, std::string _numbers, bool _extern)
{
    for (int j = 0; j < NUMBERS.size(); ++j)
    {
        if (NUMBERS[j]->name == _numbers)
        {
            if (_newvalue >= 0)
            {
                // if new value posivive, use provided value to preset PreValue
                NUMBERS[j]->PreValue = _newvalue;
            }
            else
            {
                // if new value negative, use last raw value to preset PreValue
                char *p;
                double ReturnRawValueAsDouble = strtod(NUMBERS[j]->ReturnRawValue.c_str(), &p);

                if (ReturnRawValueAsDouble == 0)
                {
                    LogFile.WriteToFile(ESP_LOG_WARN, TAG, "SetPreValue: RawValue not a valid value for further processing: " + NUMBERS[j]->ReturnRawValue);
                    return false;
                }

                NUMBERS[j]->PreValue = ReturnRawValueAsDouble;
            }

            NUMBERS[j]->ReturnPreValue = std::to_string(NUMBERS[j]->PreValue);
            NUMBERS[j]->PreValueValid = true;

            if (_extern)
            {
                time(&(NUMBERS[j]->timeStampLastPreValue));
                localtime(&(NUMBERS[j]->timeStampLastPreValue));
            }

            UpdatePreValueINI = true; // Only update prevalue file if a new value is set
            SavePreValue();

            LogFile.WriteToFile(ESP_LOG_INFO, TAG, "SetPreValue: PreValue for " + NUMBERS[j]->name + " set to " + std::to_string(NUMBERS[j]->PreValue));
            return true;
        }
    }

    LogFile.WriteToFile(ESP_LOG_WARN, TAG, "SetPreValue: Numbersname not found or not valid");
    return false; // No new value was set (e.g. wrong numbersname, no numbers at all)
}

bool ClassFlowPostProcessing::LoadPreValue(void)
{
    UpdatePreValueINI = false; // Conversion to the new format

    FILE *pFile = fopen(FilePreValue.c_str(), "r");
    if (pFile == NULL)
    {
        return false;
    }

    // Makes sure that an empty file is treated as such.
    char temp_char[1024];
    temp_char[0] = '\0';

    fgets(temp_char, 1024, pFile);
    ESP_LOGD(TAG, "Read line Prevalue.ini: %s", temp_char);
    std::string temp_time = trim_string_left_right(std::string(temp_char));

    if (temp_time.length() == 0)
    {
        return false;
    }

    std::string temp_value, temp_name;
    bool _done = false;
    std::vector<std::string> splitted = split_line(temp_time, "\t");

    //  Conversion to the new format
    if (splitted.size() > 1)
    {
        while ((splitted.size() > 1) && !_done)
        {
            temp_name = trim_string_left_right(splitted[0]);
            temp_time = trim_string_left_right(splitted[1]);
            temp_value = trim_string_left_right(splitted[2]);

            for (int j = 0; j < NUMBERS.size(); ++j)
            {
                if (NUMBERS[j]->name == temp_name)
                {
                    NUMBERS[j]->PreValue = stod(temp_value.c_str());
                    NUMBERS[j]->ReturnPreValue = round_output(NUMBERS[j]->PreValue, NUMBERS[j]->Nachkomma + 1); // To be on the safe side, 1 digit more, as Exgtended Resolution may be on (will only be set during the first run).

                    time_t tStart;
                    int yy, month, dd, hh, mm, ss;
                    struct tm whenStart;

                    sscanf(temp_time.c_str(), PREVALUE_TIME_FORMAT_INPUT, &yy, &month, &dd, &hh, &mm, &ss);
                    whenStart.tm_year = yy - 1900;
                    whenStart.tm_mon = month - 1;
                    whenStart.tm_mday = dd;
                    whenStart.tm_hour = hh;
                    whenStart.tm_min = mm;
                    whenStart.tm_sec = ss;
                    whenStart.tm_isdst = -1;

                    NUMBERS[j]->timeStampLastPreValue = mktime(&whenStart);

                    time(&tStart);
                    localtime(&tStart);
                    double difference = difftime(tStart, NUMBERS[j]->timeStampLastPreValue);
                    difference /= 60;

                    if (difference > PreValueAgeStartup)
                    {
                        NUMBERS[j]->PreValueValid = false;
                    }
                    else
                    {
                        NUMBERS[j]->PreValueValid = true;
                    }
                }
            }

            if (!fgets(temp_char, 1024, pFile))
            {
                _done = true;
            }
            else
            {
                ESP_LOGD(TAG, "Read line Prevalue.ini: %s", temp_char);
                splitted = split_line(trim_string_left_right(std::string(temp_char)), "\t");

                if (splitted.size() > 1)
                {
                    temp_name = trim_string_left_right(splitted[0]);
                    temp_time = trim_string_left_right(splitted[1]);
                    temp_value = trim_string_left_right(splitted[2]);
                }
            }
        }
        fclose(pFile);
    }
    else
    {
        // Old Format
        fgets(temp_char, 1024, pFile);
        fclose(pFile);
        ESP_LOGD(TAG, "%s", temp_char);
        temp_value = trim_string_left_right(std::string(temp_char));
        NUMBERS[0]->PreValue = stod(temp_value.c_str());

        time_t tStart;
        int yy, month, dd, hh, mm, ss;
        struct tm whenStart;

        sscanf(temp_time.c_str(), PREVALUE_TIME_FORMAT_INPUT, &yy, &month, &dd, &hh, &mm, &ss);
        whenStart.tm_year = yy - 1900;
        whenStart.tm_mon = month - 1;
        whenStart.tm_mday = dd;
        whenStart.tm_hour = hh;
        whenStart.tm_min = mm;
        whenStart.tm_sec = ss;
        whenStart.tm_isdst = -1;

        ESP_LOGD(TAG, "TIME: %d, %d, %d, %d, %d, %d", whenStart.tm_year, whenStart.tm_mon, whenStart.tm_wday, whenStart.tm_hour, whenStart.tm_min, whenStart.tm_sec);

        NUMBERS[0]->timeStampLastPreValue = mktime(&whenStart);

        time(&tStart);
        localtime(&tStart);
        double difference = difftime(tStart, NUMBERS[0]->timeStampLastPreValue);
        difference /= 60;

        if (difference > PreValueAgeStartup)
        {
            return false;
        }

        NUMBERS[0]->Value = NUMBERS[0]->PreValue;
        NUMBERS[0]->ReturnValue = std::to_string(NUMBERS[0]->Value);

        if (NUMBERS[0]->digit_roi || NUMBERS[0]->analog_roi)
        {
            NUMBERS[0]->ReturnValue = round_output(NUMBERS[0]->Value, NUMBERS[0]->Nachkomma);
        }

        UpdatePreValueINI = true; // Conversion to the new format
        SavePreValue();
    }

    return true;
}

void ClassFlowPostProcessing::SavePreValue()
{
    // PreValues unchanged --> File does not have to be rewritten
    if (!UpdatePreValueINI)
    {
        return;
    }

    FILE *pFile = fopen(FilePreValue.c_str(), "w");

    for (int j = 0; j < NUMBERS.size(); ++j)
    {
        char buffer[80];
        struct tm *timeinfo = localtime(&NUMBERS[j]->timeStampLastPreValue);
        strftime(buffer, 80, PREVALUE_TIME_FORMAT_OUTPUT, timeinfo);
        NUMBERS[j]->timeStamp = std::string(buffer);
        NUMBERS[j]->timeStampTimeUTC = NUMBERS[j]->timeStampLastPreValue;
        // ESP_LOGD(TAG, "SaverPreValue %d, Value: %f, Nachkomma %d", j, NUMBERS[j]->PreValue, NUMBERS[j]->Nachkomma);

        std::string temp_string = NUMBERS[j]->name + "\t" + NUMBERS[j]->timeStamp + "\t" + round_output(NUMBERS[j]->PreValue, NUMBERS[j]->Nachkomma) + "\n";
        ESP_LOGD(TAG, "Write PreValue line: %s", temp_string.c_str());

        if (pFile)
        {
            fputs(temp_string.c_str(), pFile);
        }
    }

    UpdatePreValueINI = false;

    fclose(pFile);
}

ClassFlowPostProcessing::ClassFlowPostProcessing(std::vector<ClassFlow *> *lfc, ClassFlowCNNGeneral *_analog, ClassFlowCNNGeneral *_digit)
{
    PreValueUse = false;
    PreValueAgeStartup = 30;
    SkipErrorMessage = false;
    ListFlowControll = NULL;
    FilePreValue = format_filename("/sdcard/config/prevalue.ini");
    ListFlowControll = lfc;
    flowTakeImage = NULL;
    UpdatePreValueINI = false;
    flowAnalog = _analog;
    flowDigit = _digit;

    for (int i = 0; i < ListFlowControll->size(); ++i)
    {
        if (((*ListFlowControll)[i])->name().compare("ClassFlowTakeImage") == 0)
        {
            flowTakeImage = (ClassFlowTakeImage *)(*ListFlowControll)[i];
        }
    }
}

void ClassFlowPostProcessing::handleDecimalExtendedResolution(std::string _decsep, std::string _value)
{
    std::string _digit;
    int _pospunkt = _decsep.find_first_of(".");

    if (_pospunkt > -1)
    {
        _digit = _decsep.substr(0, _pospunkt);
    }
    else
    {
        _digit = "default";
    }

    for (int j = 0; j < NUMBERS.size(); ++j)
    {
        bool temp_value = alphanumeric_to_boolean(_value);

        // Set to default first (if nothing else is set)
        if ((_digit == "default") || (NUMBERS[j]->name == _digit))
        {
            NUMBERS[j]->isExtendedResolution = temp_value;
        }
    }
}

void ClassFlowPostProcessing::handleDecimalSeparator(std::string _decsep, std::string _value)
{
    std::string _digit;
    int _pospunkt = _decsep.find_first_of(".");

    if (_pospunkt > -1)
    {
        _digit = _decsep.substr(0, _pospunkt);
    }
    else
    {
        _digit = "default";
    }

    for (int j = 0; j < NUMBERS.size(); ++j)
    {
        int temp_value = 0;

        if (is_string_numeric(_value))
        {
            temp_value = std::stoi(_value);
        }

        //  Set to default first (if nothing else is set)
        if ((_digit == "default") || (NUMBERS[j]->name == _digit))
        {
            NUMBERS[j]->DecimalShift = temp_value;
            NUMBERS[j]->DecimalShiftInitial = temp_value;
        }

        NUMBERS[j]->Nachkomma = NUMBERS[j]->AnzahlAnalog - NUMBERS[j]->DecimalShift;
    }
}

void ClassFlowPostProcessing::handleAnalogToDigitTransitionStart(std::string _decsep, std::string _value)
{
    std::string _digit;
    int _pospunkt = _decsep.find_first_of(".");

    if (_pospunkt > -1)
    {
        _digit = _decsep.substr(0, _pospunkt);
    }
    else
    {
        _digit = "default";
    }

    for (int j = 0; j < NUMBERS.size(); ++j)
    {
        float temp_value = 9.2;

        if (is_string_numeric(_value))
        {
            temp_value = std::stof(_value);
        }

        // Set to default first (if nothing else is set)
        if ((_digit == "default") || (NUMBERS[j]->name == _digit))
        {
            NUMBERS[j]->AnalogToDigitTransitionStart = temp_value;
        }
    }
}

void ClassFlowPostProcessing::handleAllowNegativeRate(std::string _decsep, std::string _value)
{
    std::string _digit;
    int _pospunkt = _decsep.find_first_of(".");

    if (_pospunkt > -1)
    {
        _digit = _decsep.substr(0, _pospunkt);
    }
    else
    {
        _digit = "default";
    }

    for (int j = 0; j < NUMBERS.size(); ++j)
    {
        bool temp_value = alphanumeric_to_boolean(_value);

        // Set to default first (if nothing else is set)
        if ((_digit == "default") || (NUMBERS[j]->name == _digit))
        {
            NUMBERS[j]->AllowNegativeRates = temp_value;
        }
    }
}

void ClassFlowPostProcessing::handleIgnoreLeadingNaN(std::string _decsep, std::string _value)
{
    std::string _digit;
    int _pospunkt = _decsep.find_first_of(".");

    if (_pospunkt > -1)
    {
        _digit = _decsep.substr(0, _pospunkt);
    }
    else
    {
        _digit = "default";
    }

    for (int j = 0; j < NUMBERS.size(); ++j)
    {
        bool temp_value = alphanumeric_to_boolean(_value);

        // Set to default first (if nothing else is set)
        if ((_digit == "default") || (NUMBERS[j]->name == _digit))
        {
            NUMBERS[j]->IgnoreLeadingNaN = temp_value;
        }
    }
}

void ClassFlowPostProcessing::handleMaxFlowRate(std::string _decsep, std::string _value)
{
    std::string _digit;
    int _pospunkt = _decsep.find_first_of(".");

    if (_pospunkt > -1)
    {
        _digit = _decsep.substr(0, _pospunkt);
    }
    else
    {
        _digit = "default";
    }

    for (int j = 0; j < NUMBERS.size(); ++j)
    {
        float temp_value = 4.0;

        if (is_string_numeric(_value))
        {
            temp_value = std::stof(_value);
        }

        // Set to default first (if nothing else is set)
        if ((_digit == "default") || (NUMBERS[j]->name == _digit))
        {
            NUMBERS[j]->useMaxFlowRate = true;
            NUMBERS[j]->MaxFlowRate = temp_value;
        }
    }
}

void ClassFlowPostProcessing::handleMaxRateType(std::string _decsep, std::string _value)
{
    std::string _digit, _decpos;
    int _pospunkt = _decsep.find_first_of(".");

    if (_pospunkt > -1)
    {
        _digit = _decsep.substr(0, _pospunkt);
    }
    else
    {
        _digit = "default";
    }

    for (int j = 0; j < NUMBERS.size(); ++j)
    {
        t_RateType temp_value = AbsoluteChange;

        if (to_upper(_value) == "RATECHANGE")
        {
            temp_value = RateChange;
        }

        // Set to default first (if nothing else is set)
        if ((_digit == "default") || (NUMBERS[j]->name == _digit))
        {
            NUMBERS[j]->MaxRateType = temp_value;
        }
    }
}

void ClassFlowPostProcessing::handleMaxRateValue(std::string _decsep, std::string _value)
{
    std::string _digit;
    int _pospunkt = _decsep.find_first_of(".");

    if (_pospunkt > -1)
    {
        _digit = _decsep.substr(0, _pospunkt);
    }
    else
    {
        _digit = "default";
    }

    for (int j = 0; j < NUMBERS.size(); ++j)
    {
        float temp_value = 1;

        if (is_string_numeric(_value))
        {
            temp_value = std::stof(_value);
        }

        // Set to default first (if nothing else is set)
        if ((_digit == "default") || (NUMBERS[j]->name == _digit))
        {
            NUMBERS[j]->useMaxRateValue = true;
            NUMBERS[j]->MaxRateValue = temp_value;
        }
    }
}

void ClassFlowPostProcessing::handleChangeRateThreshold(std::string _decsep, std::string _value)
{
    std::string _digit;
    int _pospunkt = _decsep.find_first_of(".");

    if (_pospunkt > -1)
    {
        _digit = _decsep.substr(0, _pospunkt);
    }
    else
    {
        _digit = "default";
    }

    for (int j = 0; j < NUMBERS.size(); ++j)
    {
        int temp_value = 2;

        if (is_string_numeric(_value))
        {
            temp_value = std::stof(_value);
        }

        // Set to default first (if nothing else is set)
        if ((_digit == "default") || (NUMBERS[j]->name == _digit))
        {
            NUMBERS[j]->ChangeRateThreshold = temp_value;
        }
    }
}

bool ClassFlowPostProcessing::ReadParameter(FILE *pFile, std::string &aktparamgraph)
{
    aktparamgraph = trim_string_left_right(aktparamgraph);
    if (aktparamgraph.size() == 0)
    {
        if (!GetNextParagraph(pFile, aktparamgraph))
        {
            return false;
        }
    }

    // Paragraph does not fit PostProcessing
    if ((to_upper(aktparamgraph).compare("[POSTPROCESSING]") != 0) && (to_upper(aktparamgraph).compare(";[POSTPROCESSING]") != 0))
    {
        return false;
    }

    InitNUMBERS();
    std::vector<std::string> splitted;

    while (getNextLine(pFile, &aktparamgraph) && !isNewParagraph(aktparamgraph))
    {
        splitted = split_line(aktparamgraph);

        if (splitted.size() > 1)
        {
            std::string _param = to_upper(GetParameterName(splitted[0]));

            if (_param == "PREVALUEUSE")
            {
                PreValueUse = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "PREVALUEAGESTARTUP")
            {
                if (is_string_numeric(splitted[1]))
                {
                    PreValueAgeStartup = std::stoi(splitted[1]);
                }
            }
            else if (_param == "SKIPERRORMESSAGE")
            {
                SkipErrorMessage = alphanumeric_to_boolean(splitted[1]);
            }
            else if (_param == "ALLOWNEGATIVERATES")
            {
                handleAllowNegativeRate(splitted[0], splitted[1]);
            }
            else if (_param == "DECIMALSHIFT")
            {
                handleDecimalSeparator(splitted[0], splitted[1]);
            }
            else if (_param == "ANALOGTODIGITTRANSITIONSTART")
            {
                handleAnalogToDigitTransitionStart(splitted[0], splitted[1]);
            }
            else if (_param == "MAXFLOWRATE")
            {
                handleMaxFlowRate(splitted[0], splitted[1]);
            }
            else if (_param == "MAXRATEVALUE")
            {
                handleMaxRateValue(splitted[0], splitted[1]);
            }
            else if (_param == "MAXRATETYPE")
            {
                handleMaxRateType(splitted[0], splitted[1]);
            }
            else if (_param == "CHANGERATETHRESHOLD")
            {
                handleChangeRateThreshold(splitted[0], splitted[1]);
            }
            else if (_param == "EXTENDEDRESOLUTION")
            {
                handleDecimalExtendedResolution(splitted[0], splitted[1]);
            }
            else if (_param == "IGNORELEADINGNAN")
            {
                handleIgnoreLeadingNaN(splitted[0], splitted[1]);
            }
        }
    }

    if (PreValueUse)
    {
        return LoadPreValue();
    }

    return true;
}

void ClassFlowPostProcessing::InitNUMBERS()
{
    std::vector<std::string> name_numbers;

    if (flowDigit)
    {
        int anzDIGIT = flowDigit->getNumberGENERAL();
        flowDigit->UpdateNameNumbers(&name_numbers);
        ESP_LOGD(TAG, "Anzahl NUMBERS: %d - DIGITS: %d", name_numbers.size(), anzDIGIT);
    }

    if (flowAnalog)
    {
        int anzANALOG = flowAnalog->getNumberGENERAL();
        flowAnalog->UpdateNameNumbers(&name_numbers);
        ESP_LOGD(TAG, "Anzahl NUMBERS: %d - ANALOG: %d", name_numbers.size(), anzANALOG);
    }

    for (int _num = 0; _num < name_numbers.size(); ++_num)
    {
        NumberPost *_number = new NumberPost;
        _number->name = name_numbers[_num];

        _number->digit_roi = NULL;

        if (flowDigit)
        {
            _number->digit_roi = flowDigit->FindGENERAL(name_numbers[_num]);
        }

        if (_number->digit_roi)
        {
            _number->AnzahlDigit = _number->digit_roi->ROI.size();
        }
        else
        {
            _number->AnzahlDigit = 0;
        }

        _number->analog_roi = NULL;

        if (flowAnalog)
        {
            _number->analog_roi = flowAnalog->FindGENERAL(name_numbers[_num]);
        }

        if (_number->analog_roi)
        {
            _number->AnzahlAnalog = _number->analog_roi->ROI.size();
        }
        else
        {
            _number->AnzahlAnalog = 0;
        }

        _number->PreValue = 0.0f; // last value read out well
        _number->ReturnPreValue = "";
        _number->PreValueValid = false;
        _number->ErrorMessage = false;
        _number->ErrorMessageText = ""; // Error message for consistency check
        _number->AllowNegativeRates = false;
        _number->DecimalShift = 0;
        _number->DecimalShiftInitial = 0;
        _number->AnalogToDigitTransitionStart = 9.2f;
        _number->MaxFlowRate = 4.0f;
        _number->useMaxFlowRate = false;
        _number->MaxRateValue = 0.1f;
        _number->MaxRateType = AbsoluteChange;
        _number->useMaxRateValue = false;
        _number->ChangeRateThreshold = 2;
        _number->isExtendedResolution = false;
        _number->IgnoreLeadingNaN = false;

        _number->Value = 0.0f;           // last value read out, incl. corrections
        _number->ReturnValue = "";    // corrected return value, possibly with error message
        _number->ReturnRawValue = ""; // raw value (with N & leading 0)
        _number->FlowRateAct = 0.0f;     // m3 / min

        _number->Nachkomma = _number->AnzahlAnalog;

        NUMBERS.push_back(_number);
    }

    for (int i = 0; i < NUMBERS.size(); ++i)
    {
        ESP_LOGD(TAG, "Number %s, Anz DIG: %d, Anz ANA %d", NUMBERS[i]->name.c_str(), NUMBERS[i]->AnzahlDigit, NUMBERS[i]->AnzahlAnalog);
    }
}

std::string ClassFlowPostProcessing::ShiftDecimal(std::string in, int _decShift)
{
    if (_decShift == 0)
    {
        return in;
    }

    int _pos_dec_org = find_delimiter_pos(in, ".");
    if (_pos_dec_org == std::string::npos)
    {
        _pos_dec_org = in.length();
    }
    else
    {
        in = in.erase(_pos_dec_org, 1);
    }

    int _pos_dec_neu = _pos_dec_org + _decShift;
    // comma is before the first digit
    if (_pos_dec_neu <= 0)
    {
        for (int i = 0; i > _pos_dec_neu; --i)
        {
            in = in.insert(0, "0");
        }

        in = "0." + in;
        return in;
    }

    // Comma should be after string (123 --> 1230)
    if (_pos_dec_neu > in.length())
    {
        for (int i = in.length(); i < _pos_dec_neu; ++i)
        {
            in = in.insert(in.length(), "0");
        }
        return in;
    }

    std::string temp_string = in.substr(0, _pos_dec_neu);
    temp_string = temp_string + ".";
    temp_string = temp_string + in.substr(_pos_dec_neu, in.length() - _pos_dec_neu);

    return temp_string;
}

bool ClassFlowPostProcessing::doFlow(std::string temp_time)
{
    time_t imagetime = flowTakeImage->getTimeImageTaken();
    if (imagetime == 0)
    {
        time(&imagetime);
    }

    struct tm *timeinfo = localtime(&imagetime);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%dT%H:%M:%S", timeinfo);
    temp_time = std::string(strftime_buf);

    for (int j = 0; j < NUMBERS.size(); ++j)
    {
        NUMBERS[j]->ErrorMessage = false;
        NUMBERS[j]->ErrorMessageText = "";

        NUMBERS[j]->Value = -1;

        if (SkipErrorMessage)
        {
            NUMBERS[j]->ReturnValue = std::to_string(NUMBERS[j]->PreValue);
            NUMBERS[j]->ReturnRawValue = NUMBERS[j]->ReturnValue;
        }
        else
        {
            NUMBERS[j]->ReturnValue = "";
            NUMBERS[j]->ReturnRawValue = "";
        }

        NUMBERS[j]->FlowRateAct = 0.0f;
        NUMBERS[j]->ReturnRateValue = round_output(0.0f, NUMBERS[j]->Nachkomma);
        NUMBERS[j]->ReturnChangeAbsolute = NUMBERS[j]->ReturnRateValue;

        // calculate time difference
        double LastValueTimeDifference = difftime(imagetime, NUMBERS[j]->timeStampLastValue) / 60;       // in minutes
        double LastPreValueTimeDifference = difftime(imagetime, NUMBERS[j]->timeStampLastPreValue) / 60; // in minutes

        if (!flowctrl.AlignmentOk)
        {
            NUMBERS[j]->Value = NUMBERS[j]->PreValue;
            NUMBERS[j]->timeStampLastValue = imagetime;

            NUMBERS[j]->ErrorMessage = true;
            NUMBERS[j]->ErrorMessageText = NUMBERS[j]->ErrorMessageText + "Alignment failed - Read: " + round_output(NUMBERS[j]->Value, NUMBERS[j]->Nachkomma) + " - Raw: " + NUMBERS[j]->ReturnRawValue + " - Pre: " + round_output(NUMBERS[j]->PreValue, NUMBERS[j]->Nachkomma) + " - Rate: " + NUMBERS[j]->ReturnRateValue;

            std::string temp_string = NUMBERS[j]->name + ": Raw: " + NUMBERS[j]->ReturnRawValue + ", Value: " + NUMBERS[j]->ReturnValue + ", Status: " + NUMBERS[j]->ErrorMessageText;
            LogFile.WriteToFile(ESP_LOG_ERROR, TAG, temp_string);
            WriteDataLog(j);

            continue;
        }

        UpdateNachkommaDecimalShift();

        int previous_value = -1;
        if (NUMBERS[j]->analog_roi)
        {
            NUMBERS[j]->ReturnRawValue = flowAnalog->getReadout(j, NUMBERS[j]->isExtendedResolution);

            if (NUMBERS[j]->ReturnRawValue.length() > 0)
            {
                char temp_char = NUMBERS[j]->ReturnRawValue[0];
                if (temp_char >= 48 && temp_char <= 57)
                {
                    previous_value = temp_char - 48;
                }
            }
        }

        if (NUMBERS[j]->digit_roi && NUMBERS[j]->analog_roi)
        {
            NUMBERS[j]->ReturnRawValue = "." + NUMBERS[j]->ReturnRawValue;
        }

        if (NUMBERS[j]->digit_roi)
        {
            if (NUMBERS[j]->analog_roi)
            {
                NUMBERS[j]->ReturnRawValue = flowDigit->getReadout(j, false, previous_value, NUMBERS[j]->analog_roi->ROI[0]->result_float, NUMBERS[j]->AnalogToDigitTransitionStart) + NUMBERS[j]->ReturnRawValue;
            }
            else
            {
                NUMBERS[j]->ReturnRawValue = flowDigit->getReadout(j, NUMBERS[j]->isExtendedResolution, previous_value); // Extended Resolution only if there are no analogue digits
            }
        }

        NUMBERS[j]->ReturnRawValue = ShiftDecimal(NUMBERS[j]->ReturnRawValue, NUMBERS[j]->DecimalShift);

        if (NUMBERS[j]->IgnoreLeadingNaN)
        {
            while ((NUMBERS[j]->ReturnRawValue.length() > 1) && (NUMBERS[j]->ReturnRawValue[0] == 'N'))
            {
                NUMBERS[j]->ReturnRawValue.erase(0, 1);
            }
        }

        std::string TempValue = NUMBERS[j]->ReturnRawValue;

        if (find_delimiter_pos(TempValue, "N") != std::string::npos)
        {
            if (PreValueUse && NUMBERS[j]->PreValueValid)
            {
                TempValue = ErsetzteN(TempValue, NUMBERS[j]->PreValue);
            }
            else
            {
                NUMBERS[j]->Value = NUMBERS[j]->PreValue;
                NUMBERS[j]->timeStampLastValue = imagetime;

                NUMBERS[j]->ErrorMessage = true;
                NUMBERS[j]->ErrorMessageText = NUMBERS[j]->ErrorMessageText + "PreValue not valid - Read: " + round_output(NUMBERS[j]->Value, NUMBERS[j]->Nachkomma) + " - Raw: " + NUMBERS[j]->ReturnRawValue + " - Pre: " + round_output(NUMBERS[j]->PreValue, NUMBERS[j]->Nachkomma) + " - Rate: " + NUMBERS[j]->ReturnRateValue;

                std::string temp_string = NUMBERS[j]->name + ": Raw: " + NUMBERS[j]->ReturnRawValue + ", Value: " + NUMBERS[j]->ReturnValue + ", Status: " + NUMBERS[j]->ErrorMessageText;
                LogFile.WriteToFile(ESP_LOG_INFO, TAG, temp_string);
                WriteDataLog(j);

                continue; // there is no number because there is still an N.
            }
        }

        // Delete leading zeros (unless there is only one 0 left)
        while ((TempValue.length() > 1) && (TempValue[0] == '0'))
        {
            TempValue.erase(0, 1);
        }

        NUMBERS[j]->Value = std::stod(TempValue);

        NUMBERS[j]->ReturnChangeAbsolute = round_output(NUMBERS[j]->Value - NUMBERS[j]->PreValue, NUMBERS[j]->Nachkomma);
        NUMBERS[j]->FlowRateAct = std::stod(round_output(((NUMBERS[j]->Value - NUMBERS[j]->PreValue) / LastPreValueTimeDifference), NUMBERS[j]->Nachkomma));

        if (NUMBERS[j]->MaxRateType == RateChange)
        {
            NUMBERS[j]->ReturnRateValue = std::to_string(NUMBERS[j]->FlowRateAct);
        }
        else
        {
            // Difference per round, as a safeguard in case a reading error(Neg. Rate - Read: or Rate too high - Read:) occurs in the meantime
            NUMBERS[j]->ReturnRateValue = round_output((NUMBERS[j]->Value - NUMBERS[j]->PreValue) / ((int)(round(LastPreValueTimeDifference / LastValueTimeDifference))), NUMBERS[j]->Nachkomma);
        }

        if (PreValueUse && NUMBERS[j]->PreValueValid)
        {
            if ((NUMBERS[j]->Nachkomma > 0) && (NUMBERS[j]->ChangeRateThreshold > 0))
            {
                double _difference1 = (NUMBERS[j]->PreValue - (NUMBERS[j]->ChangeRateThreshold / pow(10, NUMBERS[j]->Nachkomma)));
                double _difference2 = (NUMBERS[j]->PreValue + (NUMBERS[j]->ChangeRateThreshold / pow(10, NUMBERS[j]->Nachkomma)));

                if ((NUMBERS[j]->Value >= _difference1) && (NUMBERS[j]->Value <= _difference2))
                {
                    NUMBERS[j]->Value = NUMBERS[j]->PreValue;
                }
            }

            if ((!NUMBERS[j]->AllowNegativeRates) && (NUMBERS[j]->Value < NUMBERS[j]->PreValue))
            {
                LogFile.WriteToFile(ESP_LOG_DEBUG, TAG, "handleAllowNegativeRate for device: " + NUMBERS[j]->name);

                if ((NUMBERS[j]->Value < NUMBERS[j]->PreValue))
                {
                    NUMBERS[j]->Value = NUMBERS[j]->PreValue;
                    NUMBERS[j]->timeStampLastValue = imagetime;

                    NUMBERS[j]->ErrorMessage = true;
                    NUMBERS[j]->ErrorMessageText = NUMBERS[j]->ErrorMessageText + "Neg. Rate - Read: " + round_output(NUMBERS[j]->Value, NUMBERS[j]->Nachkomma) + " - Raw: " + NUMBERS[j]->ReturnRawValue + " - Pre: " + round_output(NUMBERS[j]->PreValue, NUMBERS[j]->Nachkomma) + " - Rate: " + NUMBERS[j]->ReturnRateValue;

                    std::string temp_string = NUMBERS[j]->name + ": Raw: " + NUMBERS[j]->ReturnRawValue + ", Value: " + NUMBERS[j]->ReturnValue + ", Status: " + NUMBERS[j]->ErrorMessageText;
                    LogFile.WriteToFile(ESP_LOG_ERROR, TAG, temp_string);
                    WriteDataLog(j);

                    continue;
                }
            }

            if ((NUMBERS[j]->useMaxRateValue) && (NUMBERS[j]->Value != NUMBERS[j]->PreValue))
            {
                if (abs(std::stod(NUMBERS[j]->ReturnRateValue)) > abs(NUMBERS[j]->MaxRateValue))
                {
                    NUMBERS[j]->Value = NUMBERS[j]->PreValue;
                    NUMBERS[j]->timeStampLastValue = imagetime;

                    NUMBERS[j]->ErrorMessage = true;
                    NUMBERS[j]->ErrorMessageText = NUMBERS[j]->ErrorMessageText + "Rate too high - Read: " + round_output(NUMBERS[j]->Value, NUMBERS[j]->Nachkomma) + " - Raw: " + NUMBERS[j]->ReturnRawValue + " - Pre: " + round_output(NUMBERS[j]->PreValue, NUMBERS[j]->Nachkomma) + " - Rate: " + NUMBERS[j]->ReturnRateValue;

                    std::string temp_string = NUMBERS[j]->name + ": Raw: " + NUMBERS[j]->ReturnRawValue + ", Value: " + NUMBERS[j]->ReturnValue + ", Status: " + NUMBERS[j]->ErrorMessageText;
                    LogFile.WriteToFile(ESP_LOG_ERROR, TAG, temp_string);
                    WriteDataLog(j);

                    continue;
                }
            }
        }

        NUMBERS[j]->ReturnChangeAbsolute = round_output(NUMBERS[j]->Value - NUMBERS[j]->PreValue, NUMBERS[j]->Nachkomma);

        NUMBERS[j]->PreValue = NUMBERS[j]->Value;
        NUMBERS[j]->PreValueValid = true;
        UpdatePreValueINI = true;

        NUMBERS[j]->ReturnValue = round_output(NUMBERS[j]->Value, NUMBERS[j]->Nachkomma);
        NUMBERS[j]->ReturnPreValue = round_output(NUMBERS[j]->PreValue, NUMBERS[j]->Nachkomma);

        NUMBERS[j]->timeStampLastValue = imagetime;
        NUMBERS[j]->timeStampLastPreValue = imagetime;

        NUMBERS[j]->ErrorMessage = false;
        NUMBERS[j]->ErrorMessageText = NUMBERS[j]->ErrorMessageText + "no error - Read: " + round_output(NUMBERS[j]->Value, NUMBERS[j]->Nachkomma) + " - Raw: " + NUMBERS[j]->ReturnRawValue + " - Pre: " + round_output(NUMBERS[j]->PreValue, NUMBERS[j]->Nachkomma) + " - Rate: " + NUMBERS[j]->ReturnRateValue;

        std::string temp_string = NUMBERS[j]->name + ": Raw: " + NUMBERS[j]->ReturnRawValue + ", Value: " + NUMBERS[j]->ReturnValue + ", Status: " + NUMBERS[j]->ErrorMessageText;
        LogFile.WriteToFile(ESP_LOG_INFO, TAG, temp_string);
        WriteDataLog(j);
    }

    SavePreValue();

    return true;
}

std::vector<double> ClassFlowPostProcessing::addNumbersTogether(std::vector<double> DigitValues, std::vector<double> AnalogValues)
{
    std::vector<double> meterValues;

    for (int i = 0; i < DigitValues.size(); ++i)
    {
        meterValues.push_back(DigitValues[i]);
    }

    for (int i = 0; i < AnalogValues.size(); ++i)
    {
        meterValues.push_back(AnalogValues[i]);
    }

    return meterValues;
}

void ClassFlowPostProcessing::WriteDataLog(int _index)
{
    if (!LogFile.GetDataLogToSD())
    {
        return;
    }

    std::string analog = "";
    std::string digit = "";

    char buffer[80];
    struct tm *timeinfo = localtime(&NUMBERS[_index]->timeStampLastValue);
    strftime(buffer, 80, PREVALUE_TIME_FORMAT_OUTPUT, timeinfo);
    std::string temp_time = std::string(buffer);

    if (flowAnalog)
    {
        analog = flowAnalog->getReadoutRawString(_index);
    }

    if (flowDigit)
    {
        digit = flowDigit->getReadoutRawString(_index);
    }

    LogFile.WriteToData(temp_time, NUMBERS[_index]->name, NUMBERS[_index]->ReturnRawValue, NUMBERS[_index]->ReturnValue, NUMBERS[_index]->ReturnPreValue,
                        NUMBERS[_index]->ReturnRateValue, NUMBERS[_index]->ReturnChangeAbsolute, NUMBERS[_index]->ErrorMessageText, digit, analog);

    ESP_LOGD(TAG, "WriteDataLog: %s, %s, %s, %s, %s", NUMBERS[_index]->ReturnRawValue.c_str(), NUMBERS[_index]->ReturnValue.c_str(), NUMBERS[_index]->ErrorMessageText.c_str(), digit.c_str(), analog.c_str());
}

void ClassFlowPostProcessing::UpdateNachkommaDecimalShift()
{
    for (int j = 0; j < NUMBERS.size(); ++j)
    {
        // There are only digits
        if (NUMBERS[j]->digit_roi && !NUMBERS[j]->analog_roi)
        {
            // ESP_LOGD(TAG, "Nurdigit");
            NUMBERS[j]->DecimalShift = NUMBERS[j]->DecimalShiftInitial;

            // Extended resolution is on and should also be used for this digit.
            if (NUMBERS[j]->isExtendedResolution && flowDigit->isExtendedResolution())
            {
                NUMBERS[j]->DecimalShift = NUMBERS[j]->DecimalShift - 1;
            }

            NUMBERS[j]->Nachkomma = -NUMBERS[j]->DecimalShift;
        }

        if (!NUMBERS[j]->digit_roi && NUMBERS[j]->analog_roi)
        {
            // ESP_LOGD(TAG, "Nur analog");
            NUMBERS[j]->DecimalShift = NUMBERS[j]->DecimalShiftInitial;

            if (NUMBERS[j]->isExtendedResolution && flowAnalog->isExtendedResolution())
            {
                NUMBERS[j]->DecimalShift = NUMBERS[j]->DecimalShift - 1;
            }

            NUMBERS[j]->Nachkomma = -NUMBERS[j]->DecimalShift;
        }

        // digit + analog
        if (NUMBERS[j]->digit_roi && NUMBERS[j]->analog_roi)
        {
            // ESP_LOGD(TAG, "Nur digit + analog");

            NUMBERS[j]->DecimalShift = NUMBERS[j]->DecimalShiftInitial;
            NUMBERS[j]->Nachkomma = NUMBERS[j]->analog_roi->ROI.size() - NUMBERS[j]->DecimalShift;

            // Extended resolution is on and should also be used for this digit.
            if (NUMBERS[j]->isExtendedResolution && flowAnalog->isExtendedResolution())
            {
                NUMBERS[j]->Nachkomma = NUMBERS[j]->Nachkomma + 1;
            }
        }

        ESP_LOGD(TAG, "UpdateNachkommaDecShift NUMBER%i: Nachkomma %i, DecShift %i", j, NUMBERS[j]->Nachkomma, NUMBERS[j]->DecimalShift);
    }
}

std::string ClassFlowPostProcessing::getReadout(int _number)
{
    return NUMBERS[_number]->ReturnValue;
}

std::string ClassFlowPostProcessing::getReadoutParam(bool _rawValue, bool _noerror, int _number)
{
    if (_rawValue)
    {
        return NUMBERS[_number]->ReturnRawValue;
    }

    if (_noerror)
    {
        return NUMBERS[_number]->ReturnValue;
    }

    return NUMBERS[_number]->ReturnValue;
}

std::string ClassFlowPostProcessing::ErsetzteN(std::string input, double _prevalue)
{
    int pot, ziffer;

    int posN = find_delimiter_pos(input, "N");
    int posPunkt = find_delimiter_pos(input, ".");

    if (posPunkt == std::string::npos)
    {
        posPunkt = input.length();
    }

    while (posN != std::string::npos)
    {
        if (posN < posPunkt)
        {
            pot = posPunkt - posN - 1;
        }
        else
        {
            pot = posPunkt - posN;
        }

        float temp_value = _prevalue / pow(10, pot);
        ziffer = ((int)temp_value) % 10;
        input[posN] = ziffer + 48;

        posN = find_delimiter_pos(input, "N");
    }

    return input;
}

std::string ClassFlowPostProcessing::getReadoutRate(int _number)
{
    return std::to_string(NUMBERS[_number]->FlowRateAct);
}

std::string ClassFlowPostProcessing::getReadoutTimeStamp(int _number)
{
    return NUMBERS[_number]->timeStamp;
}

std::string ClassFlowPostProcessing::getReadoutError(int _number)
{
    return NUMBERS[_number]->ErrorMessageText;
}
