#pragma once

#ifndef CLASSFFLOWTAKEIMAGE_H
#define CLASSFFLOWTAKEIMAGE_H

#include <string>
#include "defines.h"

#include "ClassFlowImage.h"
#include "ClassControllCamera.h"

class ClassFlowTakeImage : public ClassFlowImage
{
protected:
    time_t TimeImageTaken;
    std::string NameRawImage;

    esp_err_t camera_capture(void);
    void takePictureWithFlash(int flash_duration);

    void SetInitialParameter(void);

public:
    CImageBasis *rawImage;

    ClassFlowTakeImage(std::vector<ClassFlow *> *lfc);

    bool ReadParameter(FILE *pFile, std::string &aktparamgraph);
    bool doFlow(std::string time);
    std::string getHTMLSingleStep(std::string host);
    time_t getTimeImageTaken(void);
    std::string name() { return "ClassFlowTakeImage"; };

    ImageData *SendRawImage(void);
    esp_err_t SendRawJPG(httpd_req_t *req);

    ~ClassFlowTakeImage(void);
};

#endif // CLASSFFLOWTAKEIMAGE_H
