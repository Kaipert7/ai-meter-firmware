var config_gesamt = "";
var config_gesamt_temp = "";

var config_split = "";
var config_split_temp = "";

var param = [];
var param_temp = [];

var namenumberslist = "";
var datalist = "";
var tflitelist = "";

var category = [];
var category_temp = [];

var NUMBERS = new Array(0);
var NUMBERS_temp = new Array(0);

var REFERENCES = new Array(0);
var REFERENCES_temp = new Array(0);

var domainname_for_testing = "";

/* Returns the domainname with prepended protocol.
Eg. http://watermeter.fritz.box or http://192.168.1.5 */
function getDomainname(){
    var host = window.location.hostname;
    if (domainname_for_testing != "") {
        console.log("Using pre-defined domainname for testing: " + domainname_for_testing);
        domainname = "http://" + domainname_for_testing;
    }
    else
    {
        domainname = window.location.protocol + "//" + host;
        if (window.location.port != "") {
            domainname = domainname + ":" + window.location.port;
        }
    }

    return domainname;
}

function getConfig() {
    return config_gesamt;
}

function getConfigCategory() {
    return category;
}

function loadConfig(_domainname) {
	config_gesamt = "";

	var url = _domainname + "/fileserver/config/config.ini";
	
    var xhttp = new XMLHttpRequest();
    xhttp.addEventListener('load', function(event) {
        if (xhttp.status >= 200 && xhttp.status < 300) {
            config_gesamt = xhttp.responseText;
        } 
        else {
            console.warn('Response status: ${response.status}');
        }
    });
    try {     
        xhttp.open("GET", url, false);
        xhttp.send();
    } catch (error) { console.log(error); }
}

function SaveConfigToServer(_domainname){
    // leere Zeilen am Ende löschen
    var _config_split_length = config_split.length - 1;
	 
    while (config_split[_config_split_length] == "") {
        config_split.pop();
    }

    var _config_gesamt = "";
	 
    for (var i = 0; i < config_split.length; ++i) {
        _config_gesamt = _config_gesamt + config_split[i] + "\n";
    } 

    FileDeleteOnServer("/config/config.ini", _domainname);
    FileSendContent(_config_gesamt, "/config/config.ini", _domainname);          
}

function getNUMBERSList() {
    _domainname = getDomainname(); 
    namenumberslist = "";

    var xhttp = new XMLHttpRequest();
	
    xhttp.addEventListener('load', function(event) {
        if (xhttp.status >= 200 && xhttp.status < 300) {
            namenumberslist = xhttp.responseText;
        } 
        else {
            console.warn(request.statusText, request.responseText);
        }
    });

    try {
        url = _domainname + '/editflow?task=namenumbers';     
        xhttp.open("GET", url, false);
        xhttp.send();
    } catch (error) { console.log(error); }

    namenumberslist = namenumberslist.split("\t");

    return namenumberslist;
}

function getDATAList() {
    _domainname = getDomainname(); 
    datalist = "";

    var xhttp = new XMLHttpRequest();
	
    xhttp.addEventListener('load', function(event) {
        if (xhttp.status >= 200 && xhttp.status < 300) {
            datalist = xhttp.responseText;
        } 
        else {
            console.warn(request.statusText, request.responseText);
        }
    });

    try {
        url = _domainname + '/editflow?task=data';     
        xhttp.open("GET", url, false);
        xhttp.send();
    } catch (error) { console.log(error); }

    datalist = datalist.split("\t");
    datalist.pop();
    datalist.sort();

    return datalist;
}

function getTFLITEList() {
    _domainname = getDomainname();
	
    tflitelist = "";

    var xhttp = new XMLHttpRequest();
	
    xhttp.addEventListener('load', function(event) {
        if (xhttp.status >= 200 && xhttp.status < 300) {
            tflitelist = xhttp.responseText;
        } 
        else {
            console.warn(request.statusText, request.responseText);
        }
    });

    try {
        url = _domainname + '/editflow?task=tflite';
        xhttp.open("GET", url, false);
        xhttp.send();
    } catch (error) { console.log(error); }

    tflitelist = tflitelist.split("\t");
    tflitelist.sort();

    return tflitelist;
}

function ParseConfig() {
    config_split = config_gesamt.split("\n");
    var aktline = 0;

    param = new Object();
    category = new Object(); 

    var catname = "TakeImage";
    category[catname] = new Object(); 
    category[catname]["enabled"] = false;
    category[catname]["found"] = false;
    param[catname] = new Object();
    ParamAddValue(param, catname, "RawImagesLocation", 1, false, "/log/source");
    ParamAddValue(param, catname, "RawImagesRetention", 1, false, "15");
    ParamAddValue(param, catname, "SaveAllFiles", 1, false, "false");
    ParamAddValue(param, catname, "WaitBeforeTakingPicture", 1, false, "2");
    ParamAddValue(param, catname, "CamXclkFreqMhz", 1, false, "20");
    ParamAddValue(param, catname, "CamGainceiling", 1, false, "x8");            // Image gain (GAINCEILING_x2, x4, x8, x16, x32, x64 or x128)
    ParamAddValue(param, catname, "CamQuality", 1, false, "10");                // 0 - 63
    ParamAddValue(param, catname, "CamBrightness", 1, false, "0");              // (-2 to 2) - set brightness
    ParamAddValue(param, catname, "CamContrast", 1, false, "0");                //-2 - 2
    ParamAddValue(param, catname, "CamSaturation", 1, false, "0");              //-2 - 2
    ParamAddValue(param, catname, "CamSharpness", 1, false, "0");               //-2 - 2
    ParamAddValue(param, catname, "CamAutoSharpness", 1, false, "false");       // (1 or 0)	
    ParamAddValue(param, catname, "CamSpecialEffect", 1, false, "no_effect");   // 0 - 6
    ParamAddValue(param, catname, "CamWbMode", 1, false, "auto");               // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
    ParamAddValue(param, catname, "CamAwb", 1, false, "true");                  // white balance enable (0 or 1)
    ParamAddValue(param, catname, "CamAwbGain", 1, false, "true");              // Auto White Balance enable (0 or 1)
    ParamAddValue(param, catname, "CamAec", 1, false, "true");                  // auto exposure off (1 or 0)
    ParamAddValue(param, catname, "CamAec2", 1, false, "true");                 // automatic exposure sensor  (0 or 1)
    ParamAddValue(param, catname, "CamAeLevel", 1, false, "2");                 // auto exposure levels (-2 to 2)
    ParamAddValue(param, catname, "CamAecValue", 1, false, "600");              // set exposure manually  (0-1200)
    ParamAddValue(param, catname, "CamAgc", 1, false, "true");                  // auto gain off (1 or 0)
    ParamAddValue(param, catname, "CamAgcGain", 1, false, "8");                 // set gain manually (0 - 30)
    ParamAddValue(param, catname, "CamBpc", 1, false, "true");                  // black pixel correction
    ParamAddValue(param, catname, "CamWpc", 1, false, "true");                  // white pixel correction
    ParamAddValue(param, catname, "CamRawGma", 1, false, "true");               // (1 or 0)
    ParamAddValue(param, catname, "CamLenc", 1, false, "true");                 // lens correction (1 or 0)
    ParamAddValue(param, catname, "CamHmirror", 1, false, "false");             // (0 or 1) flip horizontally
    ParamAddValue(param, catname, "CamVflip", 1, false, "false");               // Invert image (0 or 1)
    ParamAddValue(param, catname, "CamDcw", 1, false, "true");                  // downsize enable (1 or 0)
    ParamAddValue(param, catname, "CamDenoise", 1, false, "0");                 // The OV2640 does not support it, OV3660 and OV5640 (0 to 8)
    ParamAddValue(param, catname, "CamZoom", 1, false, "false");
    ParamAddValue(param, catname, "CamZoomSize", 1, false, "0");
    ParamAddValue(param, catname, "CamZoomOffsetX", 1, false, "0");
    ParamAddValue(param, catname, "CamZoomOffsetY", 1, false, "0");
    ParamAddValue(param, catname, "LEDIntensity", 1, false, "50");
    ParamAddValue(param, catname, "Demo", 1, false, "false");

    var catname = "Alignment";
    category[catname] = new Object();
    category[catname]["enabled"] = false;
    category[catname]["found"] = false;
    param[catname] = new Object();
    ParamAddValue(param, catname, "SearchFieldX", 1, false, "20");
    ParamAddValue(param, catname, "SearchFieldY", 1, false, "20");
    ParamAddValue(param, catname, "SearchMaxAngle", 1, false, "15");
    ParamAddValue(param, catname, "Antialiasing", 1, false, "true");
    ParamAddValue(param, catname, "AlignmentAlgo", 1, false, "default");
    ParamAddValue(param, catname, "InitialRotate", 1, false, "0");

    var catname = "Digits";
    category[catname] = new Object();
    category[catname]["enabled"] = false;
    category[catname]["found"] = false;
    param[catname] = new Object();
    ParamAddValue(param, catname, "Model", 1, false, "/config/dig-cont_0712_s3_q.tflite");
    ParamAddValue(param, catname, "CNNGoodThreshold", 1, false, "0.5");
    ParamAddValue(param, catname, "ROIImagesLocation", 1, false, "/log/digit");
    ParamAddValue(param, catname, "ROIImagesRetention", 1, false, "3");

    var catname = "Analog";
    category[catname] = new Object();
    category[catname]["enabled"] = false;
    category[catname]["found"] = false;
    param[catname] = new Object();
    ParamAddValue(param, catname, "Model", 1, false, "/config/ana-cont_1300_s2.tflite");
    ParamAddValue(param, catname, "ROIImagesLocation", 1, false, "/log/analog");
    ParamAddValue(param, catname, "ROIImagesRetention", 1, false, "3");

    var catname = "PostProcessing";
    category[catname] = new Object();
    category[catname]["enabled"] = false;
    category[catname]["found"] = false;
    param[catname] = new Object();
    // ParamAddValue(param, catname, "PreValueUse", 1, true, "true");
    ParamAddValue(param, catname, "PreValueUse", 1, false, "true");
    ParamAddValue(param, catname, "PreValueAgeStartup", 1, false, "720");
    ParamAddValue(param, catname, "SkipErrorMessage", 1, true, "false");
    ParamAddValue(param, catname, "AllowNegativeRates", 1, true, "false");
    ParamAddValue(param, catname, "DecimalShift", 1, true, "0");
    ParamAddValue(param, catname, "AnalogToDigitTransitionStart", 1, true, "9.2");
    ParamAddValue(param, catname, "MaxFlowRate", 1, true, "4.0");
    ParamAddValue(param, catname, "MaxRateValue", 1, true, "0.05");
    ParamAddValue(param, catname, "MaxRateType", 1, true);
    ParamAddValue(param, catname, "ChangeRateThreshold", 1, true, "2");
    ParamAddValue(param, catname, "ExtendedResolution", 1, true, "false");
    ParamAddValue(param, catname, "IgnoreLeadingNaN", 1, true, "false");

    var catname = "MQTT";
    category[catname] = new Object();
    category[catname]["enabled"] = false;
    category[catname]["found"] = false;
    param[catname] = new Object();
    ParamAddValue(param, catname, "Uri", 1, false, "mqtt://example.com:1883");
    ParamAddValue(param, catname, "MainTopic", 1, false, "watermeter");
    ParamAddValue(param, catname, "ClientID", 1, false, "watermeter");
    ParamAddValue(param, catname, "user", 1, false, "USERNAME");
    ParamAddValue(param, catname, "password", 1, false, "PASSWORD");
    ParamAddValue(param, catname, "CACert", 1, false, "/config/certs/RootCA.pem");
    ParamAddValue(param, catname, "ClientCert", 1, false, "/config/certs/client.pem.crt");
    ParamAddValue(param, catname, "ClientKey", 1, false, "/config/certs/client.pem.key");
    ParamAddValue(param, catname, "ValidateServerCert", 1, false, "true");
    ParamAddValue(param, catname, "RetainMessages", 1, false, "true");
    ParamAddValue(param, catname, "HomeassistantDiscovery", 1, false, "true");
    ParamAddValue(param, catname, "DiscoveryPrefix", 1, false, "homeassistant");
    ParamAddValue(param, catname, "MeterType", 1, false, "other");
    ParamAddValue(param, catname, "DomoticzTopicIn", 1, false, "domoticz/in");
    ParamAddValue(param, catname, "DomoticzIDX", 1, true, "0");

    var catname = "InfluxDB";
    category[catname] = new Object();
    category[catname]["enabled"] = false;
    category[catname]["found"] = false;
    param[catname] = new Object();
    ParamAddValue(param, catname, "Uri", 1, false, "undefined");
    ParamAddValue(param, catname, "Database", 1, false, "undefined");
    ParamAddValue(param, catname, "user", 1, false, "undefined");
    ParamAddValue(param, catname, "password", 1, false, "undefined");
    ParamAddValue(param, catname, "Measurement", 1, true, "undefined");
    ParamAddValue(param, catname, "Field", 1, true, "undefined");

    var catname = "InfluxDBv2";
    category[catname] = new Object();
    category[catname]["enabled"] = false;
    category[catname]["found"] = false;
    param[catname] = new Object();
    ParamAddValue(param, catname, "Uri", 1, false, "undefined");
    ParamAddValue(param, catname, "Bucket", 1, false, "undefined");
    ParamAddValue(param, catname, "Org", 1, false, "undefined");
    ParamAddValue(param, catname, "Token", 1, false, "undefined");
    ParamAddValue(param, catname, "Measurement", 1, true, "undefined");
    ParamAddValue(param, catname, "Field", 1, true, "undefined");

    var catname = "Webhook";
    category[catname] = new Object();
    category[catname]["enabled"] = false;
    category[catname]["found"] = false;
    param[catname] = new Object();
    ParamAddValue(param, catname, "Uri", 1, false, "undefined");
    ParamAddValue(param, catname, "ApiKey", 1, false, "undefined");
    ParamAddValue(param, catname, "UploadImg", 1, false, "0");

    var catname = "GPIO";
    category[catname] = new Object();
    category[catname]["enabled"] = false;
    category[catname]["found"] = false;
    param[catname] = new Object();
    ParamAddValue(param, catname, "IO0", 6, false, "", [null, null, /^[0-9]*$/, null, null, /^[a-zA-Z0-9_-]*$/]);
    ParamAddValue(param, catname, "IO1", 6, false, "",  [null, null, /^[0-9]*$/, null, null, /^[a-zA-Z0-9_-]*$/]);
    ParamAddValue(param, catname, "IO3", 6, false, "",  [null, null, /^[0-9]*$/, null, null, /^[a-zA-Z0-9_-]*$/]);
    ParamAddValue(param, catname, "IO4", 6, false, "",  [null, null, /^[0-9]*$/, null, null, /^[a-zA-Z0-9_-]*$/]);
    ParamAddValue(param, catname, "IO12", 6, false, "",  [null, null, /^[0-9]*$/, null, null, /^[a-zA-Z0-9_-]*$/]);
    ParamAddValue(param, catname, "IO13", 6, false, "",  [null, null, /^[0-9]*$/, null, null, /^[a-zA-Z0-9_-]*$/]);
    ParamAddValue(param, catname, "LEDType");
    ParamAddValue(param, catname, "LEDNumbers");
    ParamAddValue(param, catname, "LEDColor", 3);
     // Default Values, um abwärtskompatiblität zu gewährleisten
    param[catname]["LEDType"]["value1"] = "WS2812";
    param[catname]["LEDNumbers"]["value1"] = "2";
    param[catname]["LEDColor"]["value1"] = "50";
    param[catname]["LEDColor"]["value2"] = "50";
    param[catname]["LEDColor"]["value3"] = "50";

    var catname = "AutoTimer";
    category[catname] = new Object();
    category[catname]["enabled"] = false;
    category[catname]["found"] = false;
    param[catname] = new Object();
    //ParamAddValue(param, catname, "AutoStart", 1, false, "true");
    ParamAddValue(param, catname, "Interval", 1, false, "5");    

    var catname = "DataLogging";
    category[catname] = new Object();
    category[catname]["enabled"] = false;
    category[catname]["found"] = false;
    param[catname] = new Object();
    ParamAddValue(param, catname, "DataLogActive", 1, false, "true");
    ParamAddValue(param, catname, "DataFilesRetention", 1, false, "3");     

    var catname = "Debug";
    category[catname] = new Object();
    category[catname]["enabled"] = false;
    category[catname]["found"] = false;
    param[catname] = new Object();
    ParamAddValue(param, catname, "LogLevel", 1, false, "1");
    ParamAddValue(param, catname, "LogfilesRetention", 1, false, "3");

    var catname = "System";
    category[catname] = new Object();
    category[catname]["enabled"] = false;
    category[catname]["found"] = false;
    param[catname] = new Object();
    ParamAddValue(param, catname, "TimeZone", 1, false, "default");
    ParamAddValue(param, catname, "TimeServer", 1, false, "pool.ntp.org");
    ParamAddValue(param, catname, "Hostname", 1, false, "watermeter");   
    ParamAddValue(param, catname, "RSSIThreshold", 1, false, "0"); 
    ParamAddValue(param, catname, "CPUFrequency", 1, false, "160");
    ParamAddValue(param, catname, "Tooltip", 1, false, "true");
    ParamAddValue(param, catname, "SetupMode", 1, false, "false");
     
    while (aktline < config_split.length){
        for (var cat in category) {
            var cat_temp = cat.toUpperCase();
            var cat_aktive = "[" + cat_temp + "]";
            var cat_inaktive = ";[" + cat_temp + "]";
            
            if ((config_split[aktline].trim().toUpperCase() == cat_aktive) || (config_split[aktline].trim().toUpperCase() == cat_inaktive)) {
                if (config_split[aktline].trim().toUpperCase() == cat_aktive) {
                    category[cat]["enabled"] = true;
                }
                
                category[cat]["found"] = true;
                category[cat]["line"] = aktline;
                aktline = ParseConfigParamAll(aktline, cat);
                continue;
            }
        }
        
        aktline++;
    }
}

function ParamAddValue(param, _cat, _param, _anzParam = 1, _isNUMBER = false, _defaultValue = "", _checkRegExList = null) {
    param[_cat][_param] = new Object(); 
    param[_cat][_param]["found"] = false;
    param[_cat][_param]["enabled"] = false;
    param[_cat][_param]["line"] = -1; 
    param[_cat][_param]["anzParam"] = _anzParam;
    param[_cat][_param]["defaultValue"] = _defaultValue;
    param[_cat][_param]["Numbers"] = _isNUMBER;
    param[_cat][_param].checkRegExList = _checkRegExList;
	
    if (_isNUMBER) {
        for (var _num in NUMBERS) {
            for (var j = 1; j <= param[_cat][_param]["anzParam"]; ++j) {
                NUMBERS[_num][_cat][_param]["value"+j] = _defaultValue;
            }
        }
    }
    else {
        for (var j = 1; j <= param[_cat][_param]["anzParam"]; ++j) {
            param[_cat][_param]["value"+j] = _defaultValue;
        }
    }
};

function ParseConfigParamAll(_aktline, _catname) {
    ++_aktline;

    while ((_aktline < config_split.length) && !(config_split[_aktline][0] == "[") && !((config_split[_aktline][0] == ";") && (config_split[_aktline][1] == "["))) {
        var _input = config_split[_aktline];
        let [isCom, input] = isCommented(_input);
        var linesplit = split_line(input);
        ParamExtractValueAll(param, linesplit, _catname, _aktline, isCom);
        
        if (!isCom && (linesplit.length >= 5) && (_catname == 'Digits')) {
            ExtractROIs(input, "digit");
        }
        
        if (!isCom && (linesplit.length >= 5) && (_catname == 'Analog')) {
            ExtractROIs(input, "analog");
        }
        
        if (!isCom && (linesplit.length == 3) && (_catname == 'Alignment')) {
            _newref = new Object();
            _newref["name"] = linesplit[0];
            _newref["x"] = linesplit[1];
            _newref["y"] = linesplit[2];
            REFERENCES.push(_newref);
        }

        ++_aktline;
    }
	
    return _aktline; 
}

function ParamExtractValue(_param, _linesplit, _catname, _paramname, _aktline, _iscom, _anzvalue = 1) {
    if ((_linesplit[0].toUpperCase() == _paramname.toUpperCase()) && (_linesplit.length > _anzvalue)) {
        _param[_catname][_paramname]["found"] = true;
        _param[_catname][_paramname]["enabled"] = !_iscom;
        _param[_catname][_paramname]["line"] = _aktline;
        _param[_catname][_paramname]["anzpara"] = _anzvalue;
        
        for (var j = 1; j <= _anzvalue; ++j) {
            _param[_catname][_paramname]["value"+j] = _linesplit[j];
        }
    }
}

function ParamExtractValueAll(_param, _linesplit, _catname, _aktline, _iscom) {
    for (var paramname in _param[_catname]) {
        _AktROI = "default";
        _AktPara = _linesplit[0];
        _pospunkt = _AktPara.indexOf (".");
        
        if (_pospunkt > -1) {
            _AktROI = _AktPara.substring(0, _pospunkt);
            _AktPara = _AktPara.substring(_pospunkt+1);
        }
        
        if (_AktPara.toUpperCase() == paramname.toUpperCase()) {
            while (_linesplit.length <= _param[_catname][paramname]["anzParam"]) {
                // line contains no value, so the default value is loaded
                _linesplit.push(_param[_catname][paramname]["defaultValue"]);
            }

            _param[_catname][paramname]["found"] = true;
            _param[_catname][paramname]["enabled"] = !_iscom;
            _param[_catname][paramname]["line"] = _aktline;
            
            if (_param[_catname][paramname]["Numbers"] == true) {
                // möglicher Multiusage
                var _numbers = getNUMBERS(_linesplit[0]);
                _numbers[_catname][paramname] = new Object;
                _numbers[_catname][paramname]["found"] = true;
                _numbers[_catname][paramname]["enabled"] = !_iscom;
     
                for (var j = 1; j <= _param[_catname][paramname]["anzParam"]; ++j) {
                    _numbers[_catname][paramname]["value"+j] = _linesplit[j];
                }
				
                if (_numbers["name"] == "default") {
                    for (var _num in NUMBERS) {
                        // Assign value to default
                        if (NUMBERS[_num][_catname][paramname]["found"] == false) {
                            NUMBERS[_num][_catname][paramname]["found"] = true;
                            NUMBERS[_num][_catname][paramname]["enabled"] = !_iscom;
                            NUMBERS[_num][_catname][paramname]["line"] = _aktline;
                                
                            for (var j = 1; j <= _param[_catname][paramname]["anzParam"]; ++j) {
                                NUMBERS[_num][_catname][paramname]["value"+j] = _linesplit[j];
                            }
                        }
                    }
                }
            }
            else {  
                for (var j = 1; j <= _param[_catname][paramname]["anzParam"]; ++j) {
                    _param[_catname][paramname]["value"+j] = _linesplit[j];
                }
            }
        }
    }
}

function getConfigParameters() {
	loadConfig(getDomainname());
	ParseConfig(getDomainname());
    return param;
}

function WriteConfigININew() {
    // Cleanup empty NUMBERS
    for (var j = 0; j < NUMBERS.length; ++j) {
        if ((NUMBERS[j]["digit"].length + NUMBERS[j]["analog"].length) == 0) {
            NUMBERS.splice(j, 1);
        }
    }

    config_split = new Array(0);

    for (var cat in param) {
        var text_cat = "[" + cat + "]";
		  
        if (!category[cat]["enabled"]) {
            text_cat = ";" + text_cat;
        }
        
        config_split.push(text_cat);

        for (var name in param[cat]) {
            if (param[cat][name]["Numbers"]) {
                for (_num in NUMBERS) {
                    var text_numbers = NUMBERS[_num]["name"] + "." + name;

                    text_numbers = text_numbers + " =" 
                         
                    for (var j = 1; j <= param[cat][name]["anzParam"]; ++j) {
                        if (!(typeof NUMBERS[_num][cat][name]["value"+j] == 'undefined')) {
                            text_numbers = text_numbers + " " + NUMBERS[_num][cat][name]["value"+j];
                        }
                        else {
                            text_numbers = text_numbers + " " + NUMBERS[_num][cat][name]["defaultValue"];
                        }
                    }
						 
                    if ((!category[cat]["enabled"]) || (!NUMBERS[_num][cat][name]["enabled"])) {
                        text_numbers = ";" + text_numbers;
                    }
						 
                    config_split.push(text_numbers);
                }
            }
            else {
                var text_name = name + " =" 
                    
                for (var j = 1; j <= param[cat][name]["anzParam"]; ++j) {
                    if (!(typeof param[cat][name]["value"+j] == 'undefined')) {
                        text_name = text_name + " " + param[cat][name]["value"+j];
                    }
                    else {
                        text_name = text_name + " " + param[cat][name]["defaultValue"];
                    }
                }
					
                if ((!category[cat]["enabled"]) || (!param[cat][name]["enabled"])) {
                    text_name = ";" + text_name;
                }
					
                config_split.push(text_name);
            }
        }
		  
        if (cat == "Digits") {
            for (var _roi in NUMBERS) {
                if (NUMBERS[_roi]["digit"].length > 0) {
                    for (var _roiddet in NUMBERS[_roi]["digit"]) {
                        var text_digital = NUMBERS[_roi]["name"] + "." + NUMBERS[_roi]["digit"][_roiddet]["name"];
                        text_digital = text_digital + " " + NUMBERS[_roi]["digit"][_roiddet]["x"];
                        text_digital = text_digital + " " + NUMBERS[_roi]["digit"][_roiddet]["y"];
                        text_digital = text_digital + " " + NUMBERS[_roi]["digit"][_roiddet]["dx"];
                        text_digital = text_digital + " " + NUMBERS[_roi]["digit"][_roiddet]["dy"];
                        text_digital = text_digital + " " + NUMBERS[_roi]["digit"][_roiddet]["CCW"];
                        config_split.push(text_digital);
                    }
                }
            }
        }
		  
        if (cat == "Analog") {
            for (var _roi in NUMBERS) {
                if (NUMBERS[_roi]["analog"].length > 0) {
                    for (var _roiddet in NUMBERS[_roi]["analog"]) {
                        var text_analog = NUMBERS[_roi]["name"] + "." + NUMBERS[_roi]["analog"][_roiddet]["name"];
                        text_analog = text_analog + " " + NUMBERS[_roi]["analog"][_roiddet]["x"];
                        text_analog = text_analog + " " + NUMBERS[_roi]["analog"][_roiddet]["y"];
                        text_analog = text_analog + " " + NUMBERS[_roi]["analog"][_roiddet]["dx"];
                        text_analog = text_analog + " " + NUMBERS[_roi]["analog"][_roiddet]["dy"];
                        text_analog = text_analog + " " + NUMBERS[_roi]["analog"][_roiddet]["CCW"];
                        config_split.push(text_analog);
                    }
                }
            }
        }
		  
        if (cat == "Alignment") {
            for (var _roi in REFERENCES) {
                var text_alignment = REFERENCES[_roi]["name"];
                text_alignment = text_alignment + " " + REFERENCES[_roi]["x"];
                text_alignment = text_alignment + " " + REFERENCES[_roi]["y"];
                config_split.push(text_alignment);
            }
        }

        config_split.push("");
    }
}

function isCommented(input) {
    let isComment = false;
		  
    if (input.charAt(0) == ';') {
        isComment = true;
        input = input.substr(1, input.length-1);
    }
		  
    return [isComment, input];
}

function ExtractROIs(_aktline, _type){
    var linesplit = split_line(_aktline);
    abc = getNUMBERS(linesplit[0], _type);
    abc["pos_ref"] = _aktline;
    abc["x"] = linesplit[1];
    abc["y"] = linesplit[2];
    abc["dx"] = linesplit[3];
    abc["dy"] = linesplit[4];
    abc["ar"] = parseFloat(linesplit[3]) / parseFloat(linesplit[4]);
    abc["CCW"] = "false";
    
    if (linesplit.length >= 6) {
        abc["CCW"] = linesplit[5];
	}
}

function getNUMBERS(_name, _type, _create = true) {
    _pospunkt = _name.indexOf (".");
    
    if (_pospunkt > -1) {
        _digit = _name.substring(0, _pospunkt);
        _roi = _name.substring(_pospunkt+1);
    }
    else {
        _digit = "default";
        _roi = _name;
    }

    _ret = -1;

    for (i = 0; i < NUMBERS.length; ++i) {
        if (NUMBERS[i]["name"] == _digit) {
            _ret = NUMBERS[i];
        }
    }

    if (!_create) {         
        // nicht gefunden und soll auch nicht erzeugt werden, ggf. geht eine NULL zurück
        return _ret;
    }

    if (_ret == -1) {
        _ret = new Object();
        _ret["name"] = _digit;
        _ret['digit'] = new Array();
        _ret['analog'] = new Array();

        for (_cat in param) {
            for (_param in param[_cat]) {
                if (param[_cat][_param]["Numbers"] == true){
                    if (typeof  _ret[_cat] == 'undefined') {
                        _ret[_cat] = new Object();
                    }
					
                    _ret[_cat][_param] = new Object();
                    _ret[_cat][_param]["found"] = false;
                    _ret[_cat][_param]["enabled"] = false;
                    _ret[_cat][_param]["anzParam"] = param[_cat][_param]["anzParam"]; 
                }
            }
        }

        NUMBERS.push(_ret);
    }

    if (typeof _type == 'undefined') {
        // muss schon existieren !!! - also erst nach Digits / Analog aufrufen
        return _ret;
    }

    neuroi = new Object();
    neuroi["name"] = _roi;
    _ret[_type].push(neuroi);

    return neuroi;
}

function RenameNUMBER(_alt, _neu){
    if ((_neu.indexOf(".") >= 0) || (_neu.indexOf(",") >= 0) || (_neu.indexOf(" ") >= 0) || (_neu.indexOf("\"") >= 0)) {
        return "Number sequence name must not contain , . \" or a space";
    }

    index = -1;
    found = false;
    
	for (i = 0; i < NUMBERS.length; ++i) {
        if (NUMBERS[i]["name"] == _alt) {
            index = i;
        }
		
        if (NUMBERS[i]["name"] == _neu) {
            found = true;
        }
    }

    if (found) {
        return "Number sequence name is already existing, please choose another name";
    }

    NUMBERS[index]["name"] = _neu;
     
    return "";
}

function DeleteNUMBER(_delete){
    if (NUMBERS.length == 1) {
        return "One number sequence is mandatory. Therefore this cannot be deleted"
    }
     
    index = -1;
	 
    for (i = 0; i < NUMBERS.length; ++i) {
        if (NUMBERS[i]["name"] == _delete) {
            index = i;
        }
    }

    if (index > -1) {
        NUMBERS.splice(index, 1);
    }

    return "";
}

function CreateNUMBER(_numbernew){
    found = false;
    
    for (i = 0; i < NUMBERS.length; ++i) {
        if (NUMBERS[i]["name"] == _numbernew) {
            found = true;
        }
    }

    if (found) {
        return "Number sequence name is already existing, please choose another name";
    }

    _ret = new Object();
    _ret["name"] = _numbernew;
    _ret['digit'] = new Array();
    _ret['analog'] = new Array();

    for (_cat in param) {
        for (_param in param[_cat]) {
            if (param[_cat][_param]["Numbers"] == true) {
                if (typeof (_ret[_cat]) === "undefined") {
                    _ret[_cat] = new Object();
                }
					
                _ret[_cat][_param] = new Object();
					
                if (param[_cat][_param]["defaultValue"] === "") {
                    _ret[_cat][_param]["found"] = false;
                    _ret[_cat][_param]["enabled"] = false;
                }
                else {
                    _ret[_cat][_param]["found"] = true;
                    _ret[_cat][_param]["enabled"] = true;
                    _ret[_cat][_param]["value1"] = param[_cat][_param]["defaultValue"];

                }
					
                _ret[_cat][_param]["anzParam"] = param[_cat][_param]["anzParam"]; 
            }
        }
    }

    NUMBERS.push(_ret);               
    return "";
}

function DeleteNUMBER(_delte) {
    if (NUMBERS.length == 1) {
        return "The last number cannot be deleted"
    }
	
    index = -1;
    
    for (i = 0; i < NUMBERS.length; ++i) {
        if (NUMBERS[i]["name"] == _delte) {
            index = i;
        }
    }

    if (index > -1) {
        NUMBERS.splice(index, 1);
    }

    return "";
}

function GetReferencesInfo(){
    return REFERENCES;
}

function getNUMBERInfo(){
     return NUMBERS;
}

function getROIInfo(_typeROI, _number){
    index = -1;
    
    for (var i = 0; i < NUMBERS.length; ++i) {
        if (NUMBERS[i]["name"] == _number) {
            index = i;
        }
    }

    if (index != -1) {
        return NUMBERS[index][_typeROI];
    }
    else {
        return "";
    }
}

function RenameROI(_number, _type, _alt, _neu){
    if ((_neu.includes("=")) || (_neu.includes(".")) || (_neu.includes(":")) || (_neu.includes(",")) || (_neu.includes(";")) || (_neu.includes(" ")) || (_neu.includes("\""))) {
        return "ROI name must not contain . : , ; = \" or space";
    }

    index = -1;
    found = false;
    _indexnumber = -1;
    
    for (j = 0; j < NUMBERS.length; ++j) {
        if (NUMBERS[j]["name"] == _number) {
            _indexnumber = j;
        }
    }

    if (_indexnumber == -1) {
        return "Number sequence not existing. ROI cannot be renamed"
    }

    for (i = 0; i < NUMBERS[_indexnumber][_type].length; ++i) {
        if (NUMBERS[_indexnumber][_type][i]["name"] == _alt) {
            index = i;
        }
		
        if (NUMBERS[_indexnumber][_type][i]["name"] == _neu) {
            found = true;
        }
    }

    if (found) {
        return "ROI name is already existing, please choose another name";
    }

    NUMBERS[_indexnumber][_type][index]["name"] = _neu;
     
    return "";
}

function CreateROI(_number, _type, _pos, _roinew, _x, _y, _dx, _dy, _CCW){
    _indexnumber = -1;
    
    for (j = 0; j < NUMBERS.length; ++j) {
        if (NUMBERS[j]["name"] == _number) {
            _indexnumber = j;
        }
    }

    if (_indexnumber == -1) {
        return "Number sequence not existing. ROI cannot be created"
    }

    found = false;
    
    for (i = 0; i < NUMBERS[_indexnumber][_type].length; ++i) {
        if (NUMBERS[_indexnumber][_type][i]["name"] == _roinew) {
            found = true;
        }
    }

    if (found) {
        return "ROI name is already existing, please choose another name";
    }

    _ret = new Object();
    _ret["name"] = _roinew;
    _ret["x"] = _x;
    _ret["y"] = _y;
    _ret["dx"] = _dx;
    _ret["dy"] = _dy;
    _ret["ar"] = _dx / _dy;
    _ret["CCW"] = _CCW;

    NUMBERS[_indexnumber][_type].splice(_pos+1, 0, _ret);

    return "";
}