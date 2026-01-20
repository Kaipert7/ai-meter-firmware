#pragma once

#ifndef CLASSFLOWIMAGE_H
#define CLASSFLOWIMAGE_H

#include "ClassFlow.h"

using namespace std;

class ClassFlowImage : public ClassFlow
{
protected:
	string imagesLocation;
	bool isLogImage;
	unsigned short imagesRetention;
	const char *logTag;

	string CreateLogFolder(string _time);
	void LogImage(string _logPath, string _name, float *_resultFloat, int *_resultInt, string _time, CImageBasis *_img);

public:
	ClassFlowImage(const char *_logTag);
	ClassFlowImage(std::vector<ClassFlow *> *_lfc, const char *_logTag);
	ClassFlowImage(std::vector<ClassFlow *> *_lfc, ClassFlow *_prev, const char *_logTag);

	void RemoveOldLogs(void);
};

#endif // CLASSFLOWIMAGE_H
