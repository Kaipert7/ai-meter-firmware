#include "defines.h"

#include "ClassFlow.h"
#include <fstream>
#include <string>
#include <iostream>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "CLASS";

void ClassFlow::SetInitialParameter(void)
{
	ListFlowControll = NULL;
	previousElement = NULL;

	disabled = false;
}

ClassFlow::ClassFlow(void)
{
	SetInitialParameter();
}

ClassFlow::ClassFlow(std::vector<ClassFlow *> *lfc)
{
	SetInitialParameter();
	ListFlowControll = lfc;
}

ClassFlow::ClassFlow(std::vector<ClassFlow *> *lfc, ClassFlow *_prev)
{
	SetInitialParameter();
	ListFlowControll = lfc;
	previousElement = _prev;
}

bool ClassFlow::ReadParameter(FILE *pFile, std::string &aktparamgraph)
{
	return false;
}

bool ClassFlow::doFlow(std::string time)
{
	return false;
}

std::string ClassFlow::getHTMLSingleStep(std::string host)
{
	return "";
}

std::string ClassFlow::GetParameterName(std::string _input)
{
	std::string _param;
	int _pospunkt = _input.find_first_of(".");
	if (_pospunkt > -1)
	{
		_param = _input.substr(_pospunkt + 1, _input.length() - _pospunkt - 1);
	}
	else
	{
		_param = _input;
	}

	return _param;
}

bool ClassFlow::isNewParagraph(std::string input)
{
	if ((input[0] == '[') || ((input[0] == ';') && (input[1] == '[')))
	{
		return true;
	}

	return false;
}

bool ClassFlow::GetNextParagraph(FILE *pFile, std::string &aktparamgraph)
{
	while (getNextLine(pFile, &aktparamgraph) && !isNewParagraph(aktparamgraph));

	if (isNewParagraph(aktparamgraph))
	{
		return true;
	}

	return false;
}

/*
bool ClassFlow::GetNextParagraph(FILE *pFile, std::string &aktparamgraph, bool &disabled, bool &eof)
{
	while (getNextLine_new(pFile, &aktparamgraph, disabled, eof) && !isNewParagraph(aktparamgraph));

	if (isNewParagraph(aktparamgraph))
	{
		return true;
	}

	return false;
}
*/

bool ClassFlow::getNextLine(FILE *pFile, std::string *rt)
{
	char temp_char[1024];
	if (pFile == NULL)
	{
		*rt = "";
		return false;
	}

	if (!fgets(temp_char, 1024, pFile))
	{
		*rt = "";
		ESP_LOGD(TAG, "END OF FILE");
		return false;
	}

	ESP_LOGD(TAG, "%s", temp_char);
	*rt = temp_char;
	*rt = trim_string_left_right(*rt);

	while ((temp_char[0] == ';' || temp_char[0] == '#' || (rt->size() == 0)) && !(temp_char[1] == '['))
	{
		*rt = "";
		if (!fgets(temp_char, 1024, pFile))
		{
			return false;
		}
		ESP_LOGD(TAG, "%s", temp_char);
		*rt = temp_char;
		*rt = trim_string_left_right(*rt);
	}

	return true;
}

/*
bool ClassFlow::getNextLine(FILE *pFile, std::string *rt, bool &disabled, bool &eof)
{
	eof = false;
	char zw[1024] = "";

	if (pFile == NULL)
	{
		*rt = "";
		return false;
	}

	if (fgets(zw, 1024, pFile))
	{
		ESP_LOGD(TAG, "%s", zw);

		if ((strlen(zw) == 0) && feof(pFile))
		{
			*rt = "";
			eof = true;
			return false;
		}
	}
	else
	{
		*rt = "";
		eof = true;
		return false;
	}

	*rt = zw;
	*rt = trim_string_left_right(*rt);

	while ((zw[0] == ';' || zw[0] == '#' || (rt->size() == 0)) && !(zw[1] == '['))
	{
		fgets(zw, 1024, pFile);
		ESP_LOGD(TAG, "%s", zw);

		if (feof(pFile))
		{
			*rt = "";
			eof = true;
			return false;
		}

		*rt = zw;
		*rt = trim_string_left_right(*rt);
	}

	disabled = ((*rt)[0] == ';');
	return true;
}
*/
