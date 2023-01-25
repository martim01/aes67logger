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


function login()
{
	var play = { "username" : document.getElementById('username').value,
                 "password" : document.getElementById('password').value};

    ajaxPostPutPatch("POST", "/x-api/login", JSON.stringify(play), handleLogin);
}

function handleLogin(status, jsonObj)
{
    if(status !== 200)
    {
        UIkit.notification({message: jsonObj["reason"], status: 'danger', timeout: 3000})
    }
    else
    {
        console.log(jsonObj);   
    }
}

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

function ajaxPostPutPatch(method, endpoint, jsonData, callback)
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
	
	ajax.open(method, "http://127.0.0.1:8080/"+endpoint, true);
	ajax.setRequestHeader("Content-type", "application/json");
	ajax.send(jsonData);
}

function ajaxDelete(endpoint, callback)
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
	
	ajax.open("DELETE", "http://127.0.0.1:8080/"+endpoint, true);
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

function system()
{
    getUpdate(handleSystemGetUpdate);
}

function handleGetUpdate(status, jsonObj)
{
    if(status == 200)
    {
        if('server' in jsonObj)
        {
            if('version' in jsonObj.server)
            {
                document.getElementById('loggermanager-version').innerHTML = jsonObj.server.version;
            }
            if('date' in jsonObj.server)
            {
                document.getElementById('loggermanager-date').innerHTML = jsonObj.server.date;
            }
        }
        if('logger' in jsonObj)
        {
            if('version' in jsonObj.server)
            {
                document.getElementById('logger-version').innerHTML = jsonObj.server.version;
            }
            if('date' in jsonObj.server)
            {
                document.getElementById('logger-date').innerHTML = jsonObj.server.date;
            }
        }
    }
}

function handleSystemGetUpdate(status, jsonObj)
{
    handleGetUpdate(status, jsonObj);

    ws_connect('info', updateInfo_System);
}

function updateInfo_System(jsonObj)
{
    console.log(jsonObj);
	document.getElementById('application-start_time').innerHTML = jsonObj.application.start_time;
	document.getElementById('application-up_time').innerHTML = millisecondsToTime(jsonObj.application.up_time*1000);
	
	document.getElementById('system-uptime').innerHTML = millisecondsToTime(jsonObj.system.uptime*1000);
	document.getElementById('system-procs').innerHTML = jsonObj.system.procs;
	document.getElementById('temperature-cpu').innerHTML = jsonObj.temperature.cpu;
	
	document.getElementById('cpu-cpu').innerHTML = jsonObj.cpu.cpu;
	document.getElementById('cpu-cpu0').innerHTML = jsonObj.cpu.cpu0;
	document.getElementById('cpu-cpu1').innerHTML = jsonObj.cpu.cpu1;
	document.getElementById('cpu-cpu2').innerHTML = jsonObj.cpu.cpu2;
	document.getElementById('cpu-cpu3').innerHTML = jsonObj.cpu.cpu3;
	
	document.getElementById('disk-bytes-total').innerHTML = Math.round(jsonObj.disk.bytes.total/1073741824);
	document.getElementById('disk-bytes-free').innerHTML = Math.round(jsonObj.disk.bytes.free/1073741824);
	document.getElementById('disk-bytes-available').innerHTML = Math.round(jsonObj.disk.bytes.available/1073741824);
	
	document.getElementById('disk-inodes-total').innerHTML = jsonObj.disk.inodes.total;
	document.getElementById('disk-inodes-free').innerHTML = jsonObj.disk.inodes.free;
	document.getElementById('disk-inodes-available').innerHTML = jsonObj.disk.inodes.available;
	
	document.getElementById('system-loads-1').innerHTML = Math.round(jsonObj.system.loads["1"]*100)/100;
	document.getElementById('system-loads-5').innerHTML = Math.round(jsonObj.system.loads["5"]*100)/100;
	document.getElementById('system-loads-15').innerHTML = Math.round(jsonObj.system.loads["15"]*100)/100;
	
	document.getElementById('system-ram-total').innerHTML = Math.round(jsonObj.system.ram.total/1048576);
	document.getElementById('system-ram-buffered').innerHTML = Math.round(jsonObj.system.ram.buffered/1048576);
	document.getElementById('system-ram-shared').innerHTML = Math.round(jsonObj.system.ram.shared/1048576);
	document.getElementById('system-ram-free').innerHTML = Math.round(jsonObj.system.ram.free/1048576);	
	
	if(jsonObj.process !== undefined)
	{
		document.getElementById('process-rs').innerHTML = Math.round(jsonObj.process.rs/1024);
		document.getElementById('process-vm').innerHTML = Math.round(jsonObj.process.vm/1024);
	}
	
	
	if(jsonObj.ntp !== undefined)
	{
		if(jsonObj.ntp.error !== undefined)
		{
			document.getElementById("ntp-sync").innerHTML = jsonObj.ntp.error;
			document.getElementById("current_time").style.color = CLR_ERROR;
		}
		else
		{
			if(jsonObj.ntp.synchronised == true)
			{
				document.getElementById("current_time").style.color = CLR_SYNC;
			}
			else
			{
				document.getElementById("current_time").style.color = CLR_WARNING;
			}
			document.getElementById("ntp-sync").innerHTML = jsonObj.ntp.synchronised;
			document.getElementById("ntp-clock").innerHTML = jsonObj.ntp.source;
			document.getElementById("ntp-refid").innerHTML = jsonObj.ntp.refid;
			document.getElementById("ntp-stratum").innerHTML = jsonObj.ntp.stratum;
			document.getElementById("ntp-precision").innerHTML = Math.round(Math.pow(2, jsonObj.ntp.precision)*1000000)+"us";
			document.getElementById("ntp-offset").innerHTML = jsonObj.ntp.offset+"s";
			document.getElementById("ntp-jitter").innerHTML = jsonObj.ntp.sys_jitter+"s";
			document.getElementById("ntp-poll").innerHTML = Math.pow(2, jsonObj.ntp.tc)+"s";
		}
	}
	else
	{
		document.getElementById("current_time").style.color = CLR_UNKNOWN;
	}
}

function restart(command)
{
	UIkit.modal.confirm('Are you sure?').then(function() 
	{
		var play = { "command" : command};
		ajaxPostPutPatch("PUT", "/x-api/power", JSON.stringify(play), handleRestartPut);
	}, function () {});	
}


function handleRestartPut(status, jsonObj)
{
	if(status != 200)
	{
		UIkit.notification({message: jsonObj["reason"], status: 'danger', timeout: 3000})
	}

}


function showUpdate(application)
{
	document.getElementById('update_application_title').innerHTML = application;
	document.getElementById('update_application').value = application;
	UIkit.modal(document.getElementById('update_modal')).show();
}

function updateApp()
{
	if(document.getElementById('filename').value != document.getElementById('update_application').value)
	{
		UIkit.modal.confirm('File chosen has different name to application. Continue?').then(function() 
		{
			doUpdate();
		}, function () {
		});
	}
	else
	{
		doUpdate();
	}
}

function doUpdate()
{
	var fd = new FormData(document.getElementById('update_form'));
	
	var ajax = new XMLHttpRequest();

		
	ajax.onreadystatechange = function()
	{
		
		if(this.readyState == 4)
		{
			var jsonObj = JSON.parse(this.responseText);
			
			
			if(this.status != 200)
			{
				UIkit.notification({message: jsonObj["reason"], status: 'danger', timeout: 4000});
			}
			else if(jsonObj["restart"] == true)
			{
				UIkit.notification({message: document.getElementById('update_application').value+" has been updated but needs restarting.", status: 'danger', timeout: 3000});
				
				getUpdate(0, showVersion);
			}
			else
			{
				UIkit.notification({message: document.getElementById('update_application').value+" has been updated", status: 'danger', timeout: 3000});
				getUpdate(0, showVersion);
			}
		}
	}

	UIkit.modal(document.getElementById('update_modal')).hide();
	
	
	ajax.open('PUT',"http://"+g_loopi_array[0].url+"/x-epi/update");
	ajax.send(fd);
}
