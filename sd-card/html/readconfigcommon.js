function createReader(file) {
     var image = new Image();
	 
     reader.onload = function(evt) {
         var image = new Image();
		 
         image.onload = function(evt) {
             var width = this.width;
             var height = this.height;
         };
		 
         image.src = evt.target.result; 
     };
	 
     reader.readAsDataURL(file);
 }

function split_line(input, delimiter = " =\t\r") {
    var Output = Array(0);
	var upper_input = input.toUpperCase();
	
    // if (input.includes("password") || input.includes("EapId") || input.includes("Token") || input.includes("ApiKey") || input.includes("http_password")) {
    if (upper_input.includes("PASSWORD") || upper_input.includes("EAPID") || upper_input.includes("TOKEN") || upper_input.includes("APIKEY") || upper_input.includes("HTTP_PASSWORD")) {
        var pos = input.indexOf("=");
        delimiter = " \t\r";
        Output.push(trim(input.substr(0, pos), delimiter));
		
		var is_pw_encrypted = input.substr(pos + 2, 6);
		
		if (is_pw_encrypted == "**##**") {
			Output.push(encryptDecrypt(input.substr(pos + 8, input.length)));
		}
		else {
			Output.push(trim(input.substr(pos + 1, input.length), delimiter));
		}
    }
    else { 
        input = trim(input, delimiter);
        var pos = findDelimiterPos(input, delimiter);
        var token;
        
		while (pos > -1) {
            token = input.substr(0, pos);
            token = trim(token, delimiter);
            Output.push(token);
			
            input = input.substr(pos + 1, input.length);
            input = trim(input, delimiter);
            pos = findDelimiterPos(input, delimiter);
        }
        
		Output.push(input);
    }
     
    return Output;
}  

function findDelimiterPos(input, delimiter) {
    var pos = -1;
    var input_temp;
    var akt_del;
     
    for (var anz = 0; anz < delimiter.length; ++anz) {
        akt_del = delimiter[anz];
        input_temp = input.indexOf(akt_del);
        
		if (input_temp > -1) {
            if (pos > -1) {
                if (input_temp < pos) {
                    pos = input_temp;
				}
            }
            else {
                pos = input_temp;
			}
        }
    }
    
	return pos;
}

function trim(istring, adddelimiter) {
    while ((istring.length > 0) && (adddelimiter.indexOf(istring[0]) >= 0)) {
        istring = istring.substr(1, istring.length-1);
    }
          
    while ((istring.length > 0) && (adddelimiter.indexOf(istring[istring.length-1]) >= 0)) {
        istring = istring.substr(0, istring.length-1);
    }

    return istring;
}
     
function dataURLtoBlob(dataurl) {
    var arr = dataurl.split(','), mime = arr[0].match(/:(.*?);/)[1],
          bstr = atob(arr[1]), n = bstr.length, u8arr = new Uint8Array(n);
    
	while(n--){
        u8arr[n] = bstr.charCodeAt(n);
    }
    
	return new Blob([u8arr], {type:mime});
}	

function FileCopyOnServer(_source, _target, _domainname = "") {
    url = _domainname + "/editflow?task=copy&in=" + _source + "&out=" + _target;
    var xhttp = new XMLHttpRequest();  
    
	try {
        xhttp.open("GET", url, false);
        xhttp.send();
	} catch (error) { console.log(error); }
}

function FileDeleteOnServer(_filename, _domainname = "") {
    var xhttp = new XMLHttpRequest();
    var okay = false;

    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                okay = true;
            } 
        }
    };
     
	try {
        var url = _domainname + "/delete" + _filename;
        xhttp.open("POST", url, false);
        xhttp.send();
    } catch (error) { console.log(error); }

    return okay;
}

function FileSendContent(_content, _filename, _domainname = "") {
    var xhttp = new XMLHttpRequest();  
    var okay = false;

    xhttp.onreadystatechange = function() {
        if (xhttp.readyState == 4) {
            if (xhttp.status == 200) {
                okay = true;
            } 
			else if (xhttp.status == 0) {
				firework.launch('Server closed the connection abruptly!', 'danger', 30000);
            } 
			else {
				firework.launch('An error occured: ' + xhttp.responseText, 'danger', 30000);
            }
        }
    };

    try {
        upload_path = _domainname + "/upload" + _filename;
        xhttp.open("POST", upload_path, false);
        xhttp.send(_content);
    } catch (error) { console.log(error); }
	
    return okay;        
}

function CopyReferenceToImgTmp(_domainname) {
    for (index = 0; index < 2; ++index) {
        _filenamevon = REFERENCES[index]["name"];
        _filenamenach = _filenamevon.replace("/config/", "/img_tmp/");
        FileDeleteOnServer(_filenamenach, _domainname);
        FileCopyOnServer(_filenamevon, _filenamenach, _domainname);
     
        _filenamevon = _filenamevon.replace(".jpg", "_org.jpg");
        _filenamenach = _filenamenach.replace(".jpg", "_org.jpg");
        FileDeleteOnServer(_filenamenach, _domainname);
        FileCopyOnServer(_filenamevon, _filenamenach, _domainname);
    }
}

function UpdateConfigReferences(_domainname){
    for (var index = 0; index < 2; ++index) {
        _filenamenach = REFERENCES[index]["name"];
        _filenamevon = _filenamenach.replace("/config/", "/img_tmp/");
        FileDeleteOnServer(_filenamenach, _domainname);
        FileCopyOnServer(_filenamevon, _filenamenach, _domainname);
     
        _filenamenach = _filenamenach.replace(".jpg", "_org.jpg");
        _filenamevon = _filenamevon.replace(".jpg", "_org.jpg");
        FileDeleteOnServer(_filenamenach, _domainname);
        FileCopyOnServer(_filenamevon, _filenamenach, _domainname);
    }
}

function UpdateConfigReference(_anzneueref, _domainname){
    var index = 0;

    if (_anzneueref == 1) {	
        index = 0;
    }

    else if (_anzneueref == 2) {
        index = 1;
    }

    _filenamenach = REFERENCES[index]["name"];
    _filenamevon = _filenamenach.replace("/config/", "/img_tmp/");

    FileDeleteOnServer(_filenamenach, _domainname);
    FileCopyOnServer(_filenamevon, _filenamenach, _domainname);

    _filenamenach = _filenamenach.replace(".jpg", "_org.jpg");
    _filenamevon = _filenamevon.replace(".jpg", "_org.jpg");

    FileDeleteOnServer(_filenamenach, _domainname);
    FileCopyOnServer(_filenamevon, _filenamenach, _domainname);
}

function MakeTempRefImage(_filename, _enhance, _domainname){
    var filename = _filename["name"].replace("/config/", "/img_tmp/");
	 
    var url = _domainname + "/editflow?task=cutref&in=/config/reference.jpg&out=" + filename + "&x=" + _filename["x"] + "&y="  + _filename["y"] + "&dx=" + _filename["dx"] + "&dy=" + _filename["dy"];
     
    if (_enhance == true){
        url = url + "&enhance=true";
    }

    var xhttp = new XMLHttpRequest();  
     
    try {
        xhttp.open("GET", url, false);
        xhttp.send();
    } catch (error){ console.log(error); }

    if (xhttp.responseText == "CutImage Done") {
        if (_enhance == true) {
            firework.launch('Image Contrast got enhanced', 'success', 5000);
        }
        else {
            firework.launch('Alignment Marker have been updated', 'success', 5000);
        }
        return true;
    }
    else {
        return false;
    }
}

// Encrypt password
function EncryptPwString(pwToEncrypt) {
	var _pw_temp = "**##**";
	var pw_temp = "";

	if (isInString(pwToEncrypt, _pw_temp)) {
		pw_temp = pwToEncrypt;
	}
	else {
		pw_temp = _pw_temp + encryptDecrypt(pwToEncrypt);
	}

	return pw_temp;
}

// Decrypt password
function DecryptPwString(pwToDencrypt) {
	var _pw_temp = "**##**";
	var pw_temp = "";
	
    if (isInString(pwToDencrypt, _pw_temp))
    {
        var _temp = ReplaceString(pwToDencrypt, _pw_temp, "");
        pw_temp = encryptDecrypt(_temp);
    }
    else
    {
        pw_temp = pwToDencrypt;
    }

    return pw_temp;
}

function decryptConfigPwOnSD(_domainname = getDomainname()) {
    var url = _domainname + "/edit_flow?task=pw_decrypt&config_decrypt=true";
    var xhttp = new XMLHttpRequest();  
    
	try {
        xhttp.open("GET", url, false);
        xhttp.send();
	} catch (error) { console.log(error); }
	
	if (xhttp.responseText == "decrypted") {
		return true;
	}
	else {
        return false;
    }
}

function decryptWifiPwOnSD(_domainname = getDomainname()) {
    var url = _domainname + "/edit_flow?task=pw_decrypt&wifi_decrypt=true";
    var xhttp = new XMLHttpRequest();  
    
	try {
        xhttp.open("GET", url, false);
        xhttp.send();
	} catch (error) { console.log(error); }
	
	if (xhttp.responseText == "decrypted") {
		return true;
	}
	else {
        return false;
    }
}

function encryptDecrypt(input) {
	var key = ['K', 'C', 'Q']; //Can be any chars, and any size array
	var output = [];
	
	for (var i = 0; i < input.length; i++) {
		var charCode = input.charCodeAt(i) ^ key[i % key.length].charCodeAt(0);
		output.push(String.fromCharCode(charCode));
	}

	return output.join("");
}
