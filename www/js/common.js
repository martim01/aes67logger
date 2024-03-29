var g_access_token;
var g_ajax = new XMLHttpRequest();
g_ajax.timeout = 300;
var g_cookie_array = [];

const zeroPad = (num,places)=>String(num).padStart(places,'0');

function getCookies()
{
	let decodedCookie = decodeURIComponent(document.cookie);
	decodedCookie.split(';').forEach(function(el){
	let pos = el.indexOf('=');
	if(pos != -1)
	{
		var key = el.substring(0,pos);
		var value = el.substring(pos+1);
		g_cookie_array[key] = value;
		console.log("Cookie: "+key+"="+value);
		if(key.trim() == 'access_token')
		{
			g_access_token = value;
			console.log("Got cookie"+g_access_token);
		}
	}
	});

}


function login()
{
	var play = { "username" : document.getElementById('username').value,
                 "password" : document.getElementById('password').value};

    ajaxPostPutPatch(location.host, "POST", "x-api/login", JSON.stringify(play), handleLogin);
}
function logout()
{
	ajaxDelete(location.host, "x-api/login", handleLogout);
	window.location.href  = "/";
}

function handleLogin(status, jsonObj)
{
    if(status !== 200 || ('access_token' in jsonObj) == false)
    {
        UIkit.notification({message: jsonObj["reason"], status: 'danger', timeout: 3000})
    }
    else 
    {
		g_access_token = jsonObj.access_token;
		

		document.cookie = "access_token="+g_access_token+"; path=/";
		window.location.pathname = "dashboard";
    }
}

function handleLogout(status, jsonObj)
{

}


function checkPermissions()
{
	getCookies();
	ajaxGet(location.host,"x-api/permissions", handlePermissions);
}

function handlePermissions(status, jsonObj)
{
    if(status !== 200)
    {
        UIkit.notification({message: jsonObj["reason"], status: 'danger', timeout: 3000})
    }
    else 
    {
		console.log(jsonObj);
		if(jsonObj.loggerserver === true)
		{
			console.log("Logger");
			document.getElementById('card_logging').style.display = 'block';
		}
		if(jsonObj.encoderserver === true)
		{
			console.log("Encoder");
			document.getElementById('card_encoding').style.display = 'block';
		}
		if(jsonObj.playbackserver === true)
		{
			console.log("Playback");
			document.getElementById('card_playback').style.display = 'block';
		}
		if(jsonObj.admin === true)
		{
			console.log("Admin");
			document.getElementById('card_admin').style.display = 'block';
		}	
    }
}

function ajaxGet(host, endpoint, callback, bJson=true)
{
	var ajax = new XMLHttpRequest();
	ajax.timeout = 2000;
	
	ajax.onload = function() 
	{
		if(this.readyState == 4)
		{
			if(bJson)
            {
				console.log(this.responseText);
				callback(this.status, JSON.parse(this.responseText));
			}
			else
			{
				callback(this.status, this.responseText);
			}
		}
	}
	ajax.onerror = function(e)
	{
		callback(this.status, null);
	}
	ajax.ontimeout = function(e)
	{
		callback(this.status, null);
	}
	ajax.open("GET", location.protocol+"//"+host+"/"+endpoint, true);
	ajax.setRequestHeader('Authorization', 'Bearer ' + g_access_token);
	ajax.send();
}

function ajaxPostPutPatch(host, method, endpoint, jsonData, callback)
{
	var ajax = new XMLHttpRequest();
	ajax.onreadystatechange = function()
	{
		
		if(this.readyState == 4)
		{
			var jsonObj = JSON.parse(this.responseText);
			callback(this.status, jsonObj);
		}
	}
	
	ajax.open(method, location.protocol+"//"+host+"/"+endpoint, true);
	ajax.setRequestHeader("Content-type", "application/json");	
	ajax.setRequestHeader('Authorization', 'Bearer ' + g_access_token);
	ajax.send(jsonData);
}

function ajaxDelete(host, endpoint, callback)
{
	var ajax = new XMLHttpRequest();
	ajax.onreadystatechange = function()
	{
		
		if(this.readyState == 4)
		{
			var jsonObj = null;
			if(this.responseText != "")
			{
				jsonObj = JSON.parse(this.responseText);
			}
			callback(this.status, jsonObj);
		}
	}
	
	ajax.open("DELETE", location.protocol+"//"+host+"/"+endpoint, true);
	ajax.setRequestHeader('Authorization', 'Bearer ' + g_access_token);
	ajax.send(null);
}


function millisecondsToTime(milliseconds)
{
	var seconds = Math.floor(milliseconds/1000)%60;
	var minutes = Math.floor(milliseconds/(1000*60))%60;
	var hours = Math.floor(milliseconds/(1000*3600));
	
	return toString(hours)+":"+toString(minutes)+":"+toString(seconds);
}

function toString(value)
{
	if(value == 0)
	{
		return '00';
	}
	else if(value < 10)
	{
		return '0'+value;
	}
	return value;
}
