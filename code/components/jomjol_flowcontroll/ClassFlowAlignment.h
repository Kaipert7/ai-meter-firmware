#pragma once

#ifndef CLASSFLOWALIGNMENT_H
#define CLASSFLOWALIGNMENT_H

#include <string>

#include "ClassFlow.h"
#include "Helper.h"
#include "CAlignAndCutImage.h"
#include "CFindTemplate.h"

using namespace std;

class ClassFlowAlignment : public ClassFlow
{
protected:
    RefInfo References[2];
    int anz_ref;
    std::string namerawimage;
    CAlignAndCutImage *AlignAndCutImage;
    std::string FileStoreRefAlignment;
    float SAD_criteria;

    void SetInitialParameter(void);
    bool LoadReferenceAlignmentValues(void);
    void SaveReferenceAlignmentValues();

public:
    CImageBasis *ImageBasis, *ImageTMP;
#ifdef ALGROI_LOAD_FROM_MEM_AS_JPG
    ImageData *AlgROI;
#endif

    ClassFlowAlignment(std::vector<ClassFlow *> *lfc);

    CAlignAndCutImage *GetAlignAndCutImage() { return AlignAndCutImage; };

    void DrawRef(CImageBasis *Image);

    bool ReadParameter(FILE *pfile, std::string &aktparamgraph);
    bool doFlow(std::string time);
    std::string getHTMLSingleStep(std::string host);
    std::string name() { return "ClassFlowAlignment"; };
};

#endif // CLASSFLOWALIGNMENT_H
