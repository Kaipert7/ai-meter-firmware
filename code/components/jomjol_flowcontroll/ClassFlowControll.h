#pragma once

#ifndef CLASSFLOWCONTROLL_H
#define CLASSFLOWCONTROLL_H

#include <string>

#include "ClassFlow.h"
#include "ClassFlowTakeImage.h"
#include "ClassFlowAlignment.h"
#include "ClassFlowCNNGeneral.h"
#include "ClassFlowPostProcessing.h"
#include "ClassFlowMQTT.h"
#include "ClassFlowInfluxDB.h"
#include "ClassFlowInfluxDBv2.h"
#include "ClassFlowWebhook.h"
#include "ClassFlowCNNGeneral.h"

class ClassFlowControll : public ClassFlow
{
protected:
	std::vector<ClassFlow *> FlowControll;
	ClassFlowPostProcessing *flowpostprocessing;
	ClassFlowAlignment *flowalignment;
	ClassFlowCNNGeneral *flowanalog;
	ClassFlowCNNGeneral *flowdigit;
	//	ClassFlowDigit* flowdigit;
	ClassFlowTakeImage *flowtakeimage;
	ClassFlow *CreateClassFlow(std::string _type);

	void SetInitialParameter(void);
	std::string aktstatusWithTime;
	std::string aktstatus;
	int aktRunNr;

public:
	bool AutoStart = false;
	float AutoInterval = 5;

	bool SetupModeActive = false;
	bool AlignmentOk = false;

	void InitFlow(std::string config);
	bool doFlow(std::string time);
	void doFlowTakeImageOnly(std::string time);
	bool getStatusSetupModus() { return SetupModeActive; };
	std::string getReadout(bool _rawvalue, bool _noerror, int _number);
	std::string getReadoutAll(int _type);
	bool UpdatePrevalue(std::string _newvalue, std::string _numbers, bool _extern);
	std::string GetPrevalue(std::string _number = "");
	bool ReadParameter(FILE *pFile, string &aktparamgraph);
	std::string getJSON();
	const std::vector<NumberPost *> &getNumbers();
	std::string getNumbersName();

	std::string TranslateAktstatus(std::string _input);

#ifdef ALGROI_LOAD_FROM_MEM_AS_JPG
	void DigitDrawROI(CImageBasis *TempImage);
	void AnalogDrawROI(CImageBasis *TempImage);
#endif

	esp_err_t GetJPGStream(std::string file_name, httpd_req_t *req);
	esp_err_t SendRawJPG(httpd_req_t *req);

	std::string doSingleStep(std::string _stepname, std::string _host);

	bool getIsAutoStart();
	void setAutoStartInterval(long &_interval);

	std::string *getActStatusWithTime();
	std::string *getActStatus();
	void setActStatus(std::string _aktstatus);

	std::vector<HTMLInfo *> GetAllDigit();
	std::vector<HTMLInfo *> GetAllAnalog();

	t_CNNType GetTypeDigit();
	t_CNNType GetTypeAnalog();

	bool StartMQTTService();

	int CleanTempFolder();

	std::string name() { return "ClassFlowControll"; };
};

#endif
