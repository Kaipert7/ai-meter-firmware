#pragma once

#ifndef CLASSFFLOWPOSTPROCESSING_H
#define CLASSFFLOWPOSTPROCESSING_H

#include <string>

#include "ClassFlow.h"
#include "ClassFlowTakeImage.h"
#include "ClassFlowCNNGeneral.h"
#include "ClassFlowDefineTypes.h"

class ClassFlowPostProcessing : public ClassFlow
{
protected:
    bool UpdatePreValueINI;

    int PreValueAgeStartup;
    bool SkipErrorMessage;

    ClassFlowCNNGeneral *flowAnalog;
    ClassFlowCNNGeneral *flowDigit;

    std::string FilePreValue;

    ClassFlowTakeImage *flowTakeImage;

    bool LoadPreValue(void);
    std::string ShiftDecimal(std::string in, int _decShift);

    std::string ErsetzteN(std::string, double _prevalue);

    void InitNUMBERS();

    void handleDecimalSeparator(std::string _decsep, std::string _value);
    void handleMaxRateValue(std::string _decsep, std::string _value);
    void handleDecimalExtendedResolution(std::string _decsep, std::string _value);
    void handleMaxRateType(std::string _decsep, std::string _value);
    void handleAnalogToDigitTransitionStart(std::string _decsep, std::string _value);
    void handleAllowNegativeRate(std::string _decsep, std::string _value);
    void handleIgnoreLeadingNaN(std::string _decsep, std::string _value);
    void handleChangeRateThreshold(std::string _decsep, std::string _value);
    void handleMaxFlowRate(std::string _decsep, std::string _value);

    std::vector<double> addNumbersTogether(std::vector<double> DigitValues, std::vector<double> AnalogValues);

    void WriteDataLog(int _index);

public:
    bool PreValueUse;
    std::vector<NumberPost *> NUMBERS;

    ClassFlowPostProcessing(std::vector<ClassFlow *> *lfc, ClassFlowCNNGeneral *_analog, ClassFlowCNNGeneral *_digit);
    virtual ~ClassFlowPostProcessing() {};
    bool ReadParameter(FILE *pFile, std::string &aktparamgraph);
    bool doFlow(std::string time);
    std::string getReadout(int _number);
    std::string getReadoutParam(bool _rawValue, bool _noerror, int _number = 0);
    std::string getReadoutError(int _number = 0);
    std::string getReadoutRate(int _number = 0);
    std::string getReadoutTimeStamp(int _number = 0);
    void SavePreValue();
    std::string getJsonFromNumber(int i, std::string _lineend);
    std::string GetPreValue(std::string _number = "");
    bool SetPreValue(double _newvalue, string _numbers, bool _extern = false);

    std::string GetJSON(std::string _lineend = "\n");
    std::string getNumbersName();

    void UpdateNachkommaDecimalShift();

    std::vector<NumberPost *> *GetNumbers() { return &NUMBERS; };

    std::string name() { return "ClassFlowPostProcessing"; };
};

#endif // CLASSFFLOWPOSTPROCESSING_H
