var g_loggerArray = new Array();
var g_ajax = new XMLHttpRequest();
var g_ws = null;
g_ajax.timeout = 300;

const zeroPad = (num,places)=>String(num).padStart(places,'0');

const CLR_PLAYING = "#92d14f";
const CLR_IDLE = "#8db4e2";
const CLR_ERROR =  "#ff7777";
const CLR_ORPHANED =  "#ffa000";
const CLR_NO_FILE = "#a0a0a0"
const CLR_UNKNOWN = "#c8c8c8"
const CLR_WARNING = "#ffa000"
const CLR_CONNECTING = "#ffff00";
const CLR_SYNC = "#008000";


function showLogger(logger)
{
    var grid = document.getElementById('logger_grid');

    var divMain = document.createElement('div');
    divMain.classList.add('uk-width-1-3@s', 'uk-width-1-6@xl');
    
    var aLogger = document.createElement('a');
    aLogger.classList.add('uk-link-reset', 'uk-display-block', 'uk-width-large@l', 'uk-width-medium@m', 'uk-card', 'uk-card-default', 'uk-card-body',
                            'uk-card-hover', 'uk-card-small');
    aLogger.id = logger;
    aLogger.href = 'status/?logger='+logger;
    
    


    var divHeader = document.createElement('div');
    divHeader.className='uk-card-header';
    var titleH3 = document.createElement('h3');
    titleH3.className='uk-card-title';
    var titleSpan = document.createElement('span');
    titleSpan.id = "host_"+logger;
    titleSpan.innerHTML = logger;
    titleH3.appendChild(titleSpan);
    divHeader.appendChild(titleH3);
    
    var divBadge = document.createElement('div');
    divBadge.classList.add('uk-card-badge', 'uk-label', 'uk-label-danger');
    divBadge.innerHTML = "Not Running";
    divBadge.id = 'running_'+logger; 
    divHeader.appendChild(divBadge);
    
    
    aLogger.appendChild(divHeader);
    

    var divBody = document.createElement('div');
    divBody.classList.add('uk-card-body', 'uk-card-small');
    
    var divUpTimeGrid = document.createElement('div');
    divUpTimeGrid.classList.add('uk-child-width-expand', 'uk-grid-small', 'uk-text-left', 'uk-grid');
    var divUpTimeTitle = document.createElement('div');
    divUpTimeTitle.classList.add('uk-width-1-2', 'uk-text-primary')
    divUpTimeTitle.innerHTML = 'Up Time:'
    var divUpTime  = document.createElement('div');
    //divUpTime.classList.add('uk-label');
    divUpTime.id = 'up_time_'+logger; 
    divUpTimeGrid.appendChild(divUpTimeTitle);
    divUpTimeGrid.appendChild(divUpTime);
    divBody.appendChild(divUpTimeGrid);

    
    var divSessionGrid = document.createElement('div');
    divSessionGrid.classList.add('uk-child-width-expand', 'uk-grid-small', 'uk-text-left', 'uk-grid');
    var divSessionTitle = document.createElement('div');
    divSessionTitle.classList.add('uk-width-1-2', 'uk-text-primary')
    divSessionTitle.innerHTML = 'Session:'
    var divSession  = document.createElement('div');
    //divSession.classList.add('uk-label');
    divSession.id = 'session_'+logger; 
    divSessionGrid.appendChild(divSessionTitle);
    divSessionGrid.appendChild(divSession);
    divBody.appendChild(divSessionGrid);

    var divFileGrid = document.createElement('div');
    divFileGrid.classList.add('uk-child-width-expand', 'uk-grid-small', 'uk-text-left', 'uk-grid');
    var divFileTitle = document.createElement('div');
    divFileTitle.classList.add('uk-width-1-2', 'uk-text-primary')
    divFileTitle.innerHTML = 'File:'
    var divFile  = document.createElement('div');
    divFile.id = 'file_'+logger; 
    divFileGrid.appendChild(divFileTitle);
    divFileGrid.appendChild(divFile);
    divBody.appendChild(divFileGrid);

    //session
    //filename
    
    //timestamp
    

    
    //divBody.appendChild(divBodySub);
    aLogger.appendChild(divBody);

    var divFooter = document.createElement('div');
    divFooter.className = 'uk-card-footer';
    var spanTimestamp  = document.createElement('span');
    spanTimestamp.id = 'timestamp_'+logger; 
    divFooter.appendChild(spanTimestamp);
    aLogger.appendChild(divFooter);

    divMain.appendChild(aLogger);
    
    grid.append(divMain);
}

function dashboard()
{
    getLoggers(handleLoggers);
    //g_loggerArray.forEach(showLogger);
}

function handleLoggers(status, jsData)
{
    if(status == 200)
    {
        g_loggerArray = jsData;
        if(g_loggerArray !== null)
        {
            g_loggerArray.forEach(showLogger);
        }

        ws_connect('status',statusUpdate)
    }
    else
    {
        console.log(status);
    }
}

function statusUpdate(jsonObj)
{
    jsonObj.forEach(statusUpdateLogger)
}
function statusUpdateLogger(jsonObj)
{
    if('name' in jsonObj)
    {
        console.log(jsonObj);
        if('running' in jsonObj)
        {
            var span = document.getElementById('running_'+jsonObj['name']);
            span.className = 'uk-label';
            span.classList.add('uk-card-badge');
            if(jsonObj['running'] === true)
            {
                span.classList.add('uk-label-success');
                span.innerHTML = 'running';
            }
            else
            {
                span.classList.add('uk-label-error');
                span.innerHTML = 'not running';
            }
            
        }
        if('session' in jsonObj)
        {
            var span = document.getElementById('session_'+jsonObj['name']);
            span.innerHTML = jsonObj['session'];
        }
        else
        {
            var span = document.getElementById('session_'+jsonObj['name']);
            span.innerHTML = "----";
        }

        if('timestamp' in jsonObj)
        {
            var span = document.getElementById('timestamp_'+jsonObj['name']);
            var dt = new Date(jsonObj['timestamp']*1000);
            span.innerHTML = dt.toISOString();
        }
        if('up_time' in jsonObj)
        {
            var up_time = jsonObj['up_time'];
            var span = document.getElementById('up_time_'+jsonObj['name']);
            
            var days = Math.floor(up_time / 86400);
            up_time = up_time % 86400;
            var hours = Math.floor(up_time / 3600);
            up_time = up_time % 3600;
            var minutes = Math.floor(up_time / 60);
            up_time = up_time % 60;

            span.innerHTML = zeroPad(days,4)+" "+zeroPad(hours,2)+":"+zeroPad(minutes,2)+":"+zeroPad(up_time,2);
            
        }
        if('filename' in jsonObj)
        {
            var span = document.getElementById('file_'+jsonObj['name']);
            span.innerHTML = jsonObj['filename'];
        }
        else
        {
            var span = document.getElementById('file_'+jsonObj['name']);
            span.innerHTML = "Not Recording";
        }
    }
}


function serverOffline()
{
    var elm = document.getElementById('current_time');
    elm.className = 'uk-h3';
    elm.classList.add('uk-text-danger');
}

function ws_connect(endpoint, callbackMessage)
{
	g_ws = new WebSocket("ws://127.0.0.1:8080/x-api/ws/"+endpoint);
    g_ws.timeout = true;
	g_ws.onopen = function(ev)  { this.tm = setTimeout(serverOffline, 4000) };
	g_ws.onerror = function(ev) { serverOffline(); };
	g_ws.onclose = function(ev) { serverOffline(); };
	
	
	g_ws.onmessage = function(ev) 
	{
		clearTimeout(this.tm);
		if(this.timeout)
		{
			this.tm = setTimeout(serverOffline, 4000);
		}
        var dt = new Date();
        var elm = document.getElementById('current_time');
        elm.innerHTML = dt.toISOString();
        elm.className = 'uk-h3';
        elm.classList.add('uk-text-success');
        
		
		var jsonObj = JSON.parse(ev.data);
        callbackMessage(jsonObj);
	}	
}



function getLoggers(callback)
{
	ajaxGet("/x-api/loggers",callback);
}


function getStatus(callback)
{
	ajaxGet("/x-api/status",callback);
}

function getPower(callback)
{
	ajaxGet("/x-api/power",callback);
}

function getConfig(callback)
{
	ajaxGet("/x-api/config", callback);
}

function getInfo(callback)
{
	ajaxGet("/x-api/info", callback);
}


function getUpdate(callback)
{
	ajaxGet("/x-api/update",callback);
}

function ajaxGet(endpoint, callback)
{
	console.log("ajaxGet "+endpoint);
	var ajax = new XMLHttpRequest();
	ajax.timeout = 2000;
	
	ajax.onload = function() 
	{
		if(this.readyState == 4)
		{
			console.log(this.responseText);

            callback(this.status, JSON.parse(this.responseText));
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
	ajax.open("GET", "http://127.0.0.1:8080/"+endpoint, true);
	ajax.send();
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

/*
function createBodyGrid(name, id)
{
	var body_grid = document.createElement('div');
	body_grid.classList.add('uk-child-width-expand');
	body_grid.classList.add('uk-grid-small');
	body_grid.classList.add('uk-text-left');
	body_grid.classList.add('uk-link-reset');
	body_grid.classList.add('uk-display-block');
	body_grid.setAttribute('uk-grid',true);
	
	
	
	var div_title = document.createElement('div');
	div_title.className = 'uk-width-1-4';
	
	var span = document.createElement('span');
	span.className = 'uk-text-bold';
	span.innerHTML = name+':';
	
	div_title.appendChild(span);
	body_grid.appendChild(div_title);
	
	var div_value = document.createElement('div');
	div_value.id = id+'_'+g_loopi_array.length;
	div_value.innerHTML = '?';
	
	body_grid.appendChild(div_value);
	
	return body_grid;
}
*/	
	

