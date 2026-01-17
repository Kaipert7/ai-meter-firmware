#pragma once

#ifndef CLASSFLOWCNNGENERAL_H
#define CLASSFLOWCNNGENERAL_H

#include "ClassFlowDefineTypes.h"
#include "ClassFlowAlignment.h"

enum t_CNNType
{
    AutoDetect,
    Analogue,
    Analogue100,
    Digit,
    DigitHyprid10,
    DoubleHyprid10,
    Digit100,
    None
};

class ClassFlowCNNGeneral : public ClassFlowImage
{
protected:
    t_CNNType CNNType;
    std::vector<general *> GENERAL;

    float CNNGoodThreshold;

    std::string cnn_model_file;

    int model_x_size;
    int model_y_size;
    int model_channel;

    bool isLogImageSelect;
    std::string LogImageSelect;

    ClassFlowAlignment *flowpostalignment;

    int PointerEvalAnalog(float number, int numeral_preceder);
    int PointerEvalAnalogToDigit(float number, float numeral_preceder, int eval_predecessors, float AnalogToDigitTransitionStart);
    int PointerEvalHybrid(float number, float number_of_predecessors, int eval_predecessors, bool Analog_Predecessors = false, float AnalogToDigitTransitionStart = 9.2);

    bool doNeuralNetwork(std::string time_value);

    bool doAlignAndCut(std::string time_value);

    bool getNetworkParameter();

public:
    ClassFlowCNNGeneral(ClassFlowAlignment *_flowalign, t_CNNType _cnntype = AutoDetect);

    bool ReadParameter(FILE *pfile, std::string &aktparamgraph);
    bool doFlow(std::string time_value);

    std::string getHTMLSingleStep(std::string host);

    std::vector<double> getMeterValues(int _number);

    std::string getReadout(int _number, bool _extendedResolution = false, int prev = -1, float _before_narrow_Analog = -1, float AnalogToDigitTransitionStart = 9.2);

    std::string getReadoutRawString(int _number);

    void DrawROI(CImageBasis *Image);

    std::vector<HTMLInfo *> GetHTMLInfo(void);

    int getNumberGENERAL(void);
    general *GetGENERAL(int _number);
    general *GetGENERAL(std::string _name, bool _create);
    general *FindGENERAL(std::string _name_number);
    std::string getNameGENERAL(int _number);

    bool isExtendedResolution(int _number = 0);

    void UpdateNameNumbers(std::vector<std::string> *_name_numbers);

    t_CNNType getCNNType() { return CNNType; };

    std::string name() { return "ClassFlowCNNGeneral"; };
};

#endif
