var g_loggerArray = new Array();
var g_ajax = new XMLHttpRequest();
var g_ws = null;
g_ajax.timeout = 300;
var g_cookie_array = [];
var g_logger = null;
var g_access_token_loggingserver = null;
var g_action = '';

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
			if(key.trim() == 'access_token_loggingserver')
			{
				g_access_token_loggingserver = value;
			}
		}
	});

	console.log(g_cookie_array);
}


function login()
{
	var play = { "username" : document.getElementById('username').value,
                 "password" : document.getElementById('password').value};

    ajaxPostPutPatch("POST", "x-api/login", JSON.stringify(play), handleLogin);
}
function logout()
{
	ajaxDelete("x-api/login", handleLogout);
	window.location.href  = "/";
}

function handleLogin(status, jsonObj)
{
    if(status !== 200 || ('token' in jsonObj) == false)
    {
        UIkit.notification({message: jsonObj["reason"], status: 'danger', timeout: 3000})
    }
    else 
    {
		console.log(jsonObj);
		g_access_token_loggingserver = jsonObj.token;

        document.cookie = "access_token_loggingserver="+g_access_token_loggingserver+"; path=/";
		window.location.pathname = "/dashboard";
		console.log(window.location);
    }
}

function handleLogout(status, jsonObj)
{

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
    aLogger.href = '../loggers/index.html?logger='+logger;
    
    


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
    divBadge.classList.add('uk-card-badge', 'uk-label', 'uk-label-warning');
    divBadge.innerHTML = "Unknown";
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
    divBody.appendChild(divFileGrid)
    aLogger.appendChild(divBody);

    var divFooter = document.createElement('div');
    divFooter.className = 'uk-card-footer';
    var spanTimestamp  = document.createElement('span');
    spanTimestamp.id = 'timestamp_'+logger; 
    divFooter.appendChild(spanTimestamp);
    aLogger.appendChild(divFooter);

    divMain.appendChild(aLogger);
    
    grid.insertBefore(divMain, document.getElementById('logger_add'));
}

function dashboard()
{
	getLoggers(handleLoggers);
}

function logs()
{
	var now = new Date();
	now.setMinutes(now.getMinutes() - now.getTimezoneOffset());
	document.getElementById('end_time').value = now.toISOString().slice(0, 16);
	now.setMinutes(now.getMinutes() - 60);
	document.getElementById('start_time').value = now.toISOString().slice(0, 16);
	getLoggers(populateSelectLoggers);
}

function populateSelectLoggers(status, jsData)
{
    if(status == 200)
    {
        g_loggerArray = jsData;
        if(g_loggerArray !== null)
        {
			var select = document.getElementById('select_log');
            g_loggerArray.forEach(function(el){
				var opt = document.createElement('option');
				opt.id = el;
				opt.value = el;
				opt.innerHTML = el;
				select.appendChild(opt);
			});
        }
	}
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

		ajaxGet('x-api/status', handleLoggersStatus);
    }
    else
    {
        console.log(status);
    }
}

function handleLoggersStatus(status, jsonObj)
{
	if(status == 200)
	{
		statusUpdate(jsonObj)
	}
	ws_connect('status',statusUpdate)
}

function statusUpdate(jsonObj)
{
    jsonObj.forEach(statusUpdateLogger)
}
function statusUpdateLogger(jsonObj)
{
	if(jsonObj === null)
		return;

    if('name' in jsonObj)
    {
		var card = document.getElementById(jsonObj['name']);
		if(card === null)
		{	//card not created yet
			return;
		}

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
	getCookies();
	
	ws_protocol = "ws:";
	if(location.protocol == 'https:')
	{
		ws_protocol = "wss:";
	}

	g_ws = new WebSocket(ws_protocol+"//"+location.host+"/x-api/ws/"+endpoint+"?access_token="+g_access_token_loggingserver);
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
	ajaxGet("x-api/loggers",callback);
}


function getStatus(callback)
{
	ajaxGet("x-api/status",callback);
}

function getPower(callback)
{
	ajaxGet("x-api/power",callback);
}

function getConfig(callback)
{
	ajaxGet("x-api/config", callback);
}

function getInfo(callback)
{
	ajaxGet("x-api/info", callback);
}


function getUpdate(callback)
{
	ajaxGet("x-api/update",callback);
}

function ajaxGet(endpoint, callback, bJson=true)
{
	console.log("ajaxGet "+endpoint);
	var ajax = new XMLHttpRequest();
	ajax.timeout = 2000;
	
	ajax.onload = function() 
	{
		if(this.readyState == 4)
		{
			console.log(this.responseText);
			if(bJson)
            {
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
	ajax.open("GET", location.protocol+"//"+location.host+"/"+endpoint, true);
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
	
	ajax.open(method, location.protocol+"//"+location.host+"/"+endpoint, true);
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
			var jsonObj = null;
			if(this.responseText != "")
			{
				jsonObj = JSON.parse(this.responseText);
			}
			callback(this.status, jsonObj);
		}
	}
	
	ajax.open("DELETE", location.protocol+"//"+location.host+"/"+endpoint, true);
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
	if(status == 200)
    {
		handleGetUpdate(status, jsonObj);
		getInfo(handleGetSystemInfo);
	}
	else
	{
		UIkit.notification({message: jsonObj["reason"], status: 'danger', timeout: 3000})
	}
}

function handleGetSystemInfo(status, jsonObj)
{
	if(status == 200)
	{
		updateInfo_System(jsonObj);
		ws_connect('info', updateInfo_System);
	}
	else if(jsonObj)
	{
		UIkit.notification({message: jsonObj["reason"], status: 'danger', timeout: 3000})
	}
	else
	{
		console.log(status);
	}
}

function updateInfo_System(jsonObj)
{
	if(jsonObj.application !== undefined)
	{
		document.getElementById('application-start_time').innerHTML = jsonObj.application.start_time;
		document.getElementById('application-up_time').innerHTML = millisecondsToTime(jsonObj.application.up_time*1000);
	}
	if(jsonObj.system !== undefined)
	{
		document.getElementById('system-uptime').innerHTML = millisecondsToTime(jsonObj.system.uptime*1000);
		document.getElementById('system-procs').innerHTML = jsonObj.system.procs;
		document.getElementById('temperature-cpu').innerHTML = jsonObj.temperature.cpu;
	}
	if(jsonObj.cpu != undefined)
	{
		if(jsonObj.cpu.error != undefined)
		{

		}
		else
		{
			jsonObj.cpu.forEach(function(el){
				if(el.id != 'cpu')
				{
					var cpuElm = document.getElementById(el.id);
					if(cpuElm === null)
					{
						var divId = document.createElement('div');
						divId.className = "uk-width-1-4";
						divId.id = 'div_'+el.id;
						var spanId = document.createElement('span');
						spanId.className = 'uk-text-bold'
						spanId.innerHTML = el.id+':';
						divId.appendChild(spanId);

						var divUsage = document.createElement('div');
						divUsage.className = "uk-width-1-4";
						cpuElm = document.createElement('span');
						cpuElm.id = el.id;
						divUsage.appendChild(cpuElm);

						var grid = document.getElementById('grid_cpu');
						grid.appendChild(divId);
						grid.appendChild(divUsage);
					}
					cpuElm.innerHTML = el.usage;
				}
				else
				{
					document.getElementById('cpu-cpu').innerHTML = el.usage;
				}
			});
		}				
		
		
	}
	if(jsonObj.disk !== undefined)
	{
		if(jsonObj.disk.error != undefined)
		{
			document.getElementById('disk-path').innerHTML = jsonObj.disk.error;
		}
		else
		{
			document.getElementById('disk-path').innerHTML = jsonObj.disk.disk;

			document.getElementById('disk-bytes-total').innerHTML = Math.round(jsonObj.disk.bytes.total/1073741824);
			document.getElementById('disk-bytes-free').innerHTML = Math.round(jsonObj.disk.bytes.free/1073741824);
			document.getElementById('disk-bytes-available').innerHTML = Math.round(jsonObj.disk.bytes.available/1073741824);
			
			document.getElementById('disk-inodes-total').innerHTML = jsonObj.disk.inodes.total;
			document.getElementById('disk-inodes-free').innerHTML = jsonObj.disk.inodes.free;
			document.getElementById('disk-inodes-available').innerHTML = jsonObj.disk.inodes.available;
		}
	}
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
		ajaxPostPutPatch("PUT", "x-api/power", JSON.stringify(play), handleRestartPut);
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
	
	
	ajax.open('PUT',location.protocol+"//"+g_loopi_array[0].url+"/x-api/update");
	ajax.send(fd);
}


function getLogs()
{
	var dtStart = new Date(document.getElementById('start_time').value);
	var dtEnd = new Date(document.getElementById('end_time').value);

	var endpoint = "x-api/logs?logger="+document.getElementById('select_log').value+"&start_time="+dtStart.getTime()/1000+
	"&end_time="+dtEnd.getTime()/1000;

	ajaxGet(endpoint, handleGetLogs);
}

function handleGetLogs(status, log)
{
	if(status == 200)
	{
		document.getElementById('log').innerHTML = log;
	}
	else
	{
		UIkit.notification({message: log["reason"], status: 'danger', timeout: 3000})
	}
}

function loggers()
{
	const params = new Proxy(new URLSearchParams(window.location.search), { get: (searchParams, prop) => searchParams.get(prop)});
	
	g_logger = params.logger;

	ajaxGet("x-api/loggers/"+g_logger+"/status", connectToLogger)
}

function connectToLogger(status, jsonObj)
{
	if(status == 200)
	{
		handleLoggerInfo(jsonObj);
		ws_connect("loggers/"+g_logger, handleLoggerInfo);
	}
	else
	{
		UIkit.notification({message: jsonObj["reason"], status: 'danger', timeout: 3000});
	}
}


function handleLoggerInfo(jsonObj)
{
	if("id" in jsonObj)
	{
		document.getElementById('logger').innerHTML = jsonObj.id;
	}
	if("qos" in jsonObj)
	{
		document.getElementById('qos-bitrate').innerHTML = jsonObj.qos.bitrate.toFixed(2) + " kbps";
		document.getElementById('qos-received').innerHTML = jsonObj.qos.packets.received;
		document.getElementById('qos-lost').innerHTML = jsonObj.qos.packets.lost;
		document.getElementById('qos-errors').innerHTML = jsonObj.qos.timestamp_errors;
		document.getElementById('qos-interpacket').innerHTML = (jsonObj.qos.packet_gap).toFixed(4)+" ms";
		document.getElementById('qos-jitter').innerHTML = (jsonObj.qos.jitter.toFixed(4))+" ms";
		document.getElementById('qos-tsdf').innerHTML = (jsonObj.qos['ts-df'].toFixed(4))+" ms";
		document.getElementById('qos-duration').innerHTML = jsonObj.qos['duration']+" &mu;s";
	}

	if("session" in jsonObj)
	{
		document.getElementById('session-sdp').innerHTML = jsonObj.session.sdp;
		document.getElementById('session-name').innerHTML = jsonObj.session.name;
		document.getElementById('session-type').innerHTML = jsonObj.session.type;
		document.getElementById('session-description').innerHTML = jsonObj.session.description;
		document.getElementById('session-groups').innerHTML = jsonObj.session.groups;
		ShowClock(jsonObj.session);

		var elm = document.getElementById('session-audio');
		elm.className = 'uk-card-badge';
		elm.classList.add('uk-label');

		if(jsonObj.session.audio)
		{
			elm.classList.add('uk-label-success');
			elm.innerHTML = "Audio Incoming";	
		}
		else
		{
			elm.classList.add('uk-label-danger');
			elm.innerHTML = "No Audio";	
		}

		if('subsessions' in jsonObj.session && jsonObj.session.subsessions.length > 0)
		{
			document.getElementById('subsession-id').innerHTML = jsonObj.session.subsessions[0].id;
			document.getElementById('subsession-source_address').innerHTML = jsonObj.session.subsessions[0].source_address;
			document.getElementById('subsession-medium').innerHTML = jsonObj.session.subsessions[0].medium;
			document.getElementById('subsession-codec').innerHTML = jsonObj.session.subsessions[0].codec;
			document.getElementById('subsession-protocol').innerHTML = jsonObj.session.subsessions[0].protocol;
			document.getElementById('subsession-port').innerHTML = jsonObj.session.subsessions[0].port;
			document.getElementById('subsession-sample_rate').innerHTML = jsonObj.session.subsessions[0].sample_rate;
			document.getElementById('subsession-channels').innerHTML = jsonObj.session.subsessions[0].channels;
			document.getElementById('subsession-sync_timestamp').innerHTML = jsonObj.session.subsessions[0].sync_timestamp;

			ShowClock(jsonObj.session.subsessions[0]);
		}
	}

	if('heartbeat' in jsonObj)
	{
		var dtT = new Date(jsonObj.heartbeat.timestamp*1000);
		document.getElementById('application-timestamp').innerHTML = dtT.toISOString();
        
		var dtS = new Date(jsonObj.heartbeat.start_time*1000);
		document.getElementById('application-start_time').innerHTML = dtS.toISOString();

		var up_time = jsonObj.heartbeat.up_time;
		var days = Math.floor(up_time / 86400);
		up_time = up_time % 86400;
		var hours = Math.floor(up_time / 3600);
		up_time = up_time % 3600;
		var minutes = Math.floor(up_time / 60);
		up_time = up_time % 60;

		document.getElementById('application-up_time').innerHTML =  zeroPad(days,4)+" "+zeroPad(hours,2)+":"+zeroPad(minutes,2)+":"+zeroPad(up_time,2);
            
	}
	if('file' in jsonObj)
	{
		var elm = document.getElementById('file-filename');
		elm.innerHTML = jsonObj.file.filename;
		if(jsonObj.file.open === true)
		{
			elm.className = 'uk-text-success';
		}
		else
		{
			elm.className = 'uk-text-danger';
		}
	}
	if('streaming' in jsonObj)
	{
		document.getElementById('source-name').innerHTML = jsonObj.streaming.name;
		document.getElementById('source-type').innerHTML = jsonObj.streaming.type;
		if(jsonObj.streaming.type == 'RTSP')
		{
			document.getElementById('div_source-sdp').style.display = 'none';
			document.getElementById('div_source-rtsp').style.display= 'block';
			document.getElementById('source-rtsp').innerHTML = jsonObj.streaming.source;
		}
		else
		{
			document.getElementById('div_source-rtsp').style.display = 'none';
			document.getElementById('div_source-sdp').style.display = 'block';
			document.getElementById('source-sdp').innerHTML = jsonObj.streaming.source;
		}
		var elm = document.getElementById('source-connection');
		elm.className = 'uk-card-badge';
		elm.classList.add('uk-label');

		if(jsonObj.streaming.streaming)
		{
			elm.classList.add('uk-label-success');
			elm.innerHTML = "Streaming";	
		}
		else
		{
			elm.classList.add('uk-label-danger');
			elm.innerHTML = "No Connection";	
		}
	}
}

function ShowClock(jsonObj)
{
	if('ref_clock' in jsonObj)
	{
		document.getElementById('ref-domain').innerHTML = jsonObj.ref_clock.domain;
		document.getElementById('ref-id').innerHTML = jsonObj.ref_clock.id;
		document.getElementById('ref-type').innerHTML = jsonObj.ref_clock.type;
		document.getElementById('ref-version').innerHTML = jsonObj.ref_clock.version;
	}
}

function loggerAdmin()
{
	if(g_action == 'restart')
	{
		var play = { "command" : "restart", "password" : document.getElementById('admin_password').value};
		ajaxPostPutPatch("PUT", "x-api/loggers/"+g_logger, JSON.stringify(play), handleRestartLogger);
	}
	else if(g_action == 'remove')
	{
		var play = {"password" : document.getElementById('admin_password').value};
		ajaxPostPutPatch("DELETE", "x-api/loggers/"+g_logger, JSON.stringify(play), handleDeleteLogger);
	}
}


function handleRestartLogger(status, jsonObj)
{
	if(status != 200)
	{
		UIkit.notification({message: jsonObj["reason"], status: 'danger', timeout: 3000})
	}
	UIkit.modal(document.getElementById('password_modal')).hide();
}

function handleDeleteLogger(status, jsonObj)
{
	UIkit.modal(document.getElementById('password_modal')).hide();
	
	if(status != 200)
	{
		UIkit.notification({message: jsonObj["reason"], status: 'danger', timeout: 3000})
	}
	else
	{
		//console.log(status);
		//console.log(jsonObj);
		window.location.pathname = "../dashboard";
	}
}


function changeSession()
{
	ajaxGet("x-api/sources", handleSources)
}

function addSources(which, jsonObj)
{
	var sel = document.getElementById('select_'+which);
	while(sel.options.length) sel.remove(0);
	
	if(which in jsonObj)
	{
		jsonObj[which].sort();
		jsonObj[which].forEach(function(el){
			var opt = document.createElement('option');
			opt.value=el;
			opt.innerHTML = el;
			sel.appendChild(opt);
		});
	}
}

function handleSources(status, jsonObj)
{
	if(status == 200)
	{
		addSources('rtsp', jsonObj);
		addSources('sdp', jsonObj);
		UIkit.modal(document.getElementById('update_session_modal')).show();	
	}
}



function showAdminPassword(action)
{
	g_action = action;
	if(action == 'restart')
	{
		document.getElementById('logger_admin').innerHTML = "Restart Logger";
	}
	else if(action == 'remove')
	{
		document.getElementById('logger_admin').innerHTML = "Remove Logger";
	}
	UIkit.modal(document.getElementById('password_modal')).show();	
}



var g_method = 'rtsp';
function showrtsp()
{
	g_method = 'rtsp';
	document.getElementById('div_rtsp').style.display = 'block';
	document.getElementById('div_sdp').style.display = 'none';
}

function showsdp()
{
	g_method = 'sdp';
	document.getElementById('div_rtsp').style.display = 'none';
	document.getElementById('div_sdp').style.display = 'block';
}

function updateLoggerSession()
{
	var name = document.getElementById('config_name').value;
	if(name == '')
	{
		UIkit.notification({message: 'Source name may not be empty', status: 'warning', timeout: 2000});
		return;
	}
	var rtsp = '';
	var sdp = '';
	console.log(g_method);
	if(g_method == 'rtsp')
	{
		rtsp = document.getElementById('select_rtsp').value;
		if(rtsp == '')
		{
			UIkit.notification({message: 'RTSP may not be empty', status: 'warning', timeout: 2000});
			return;
		}
	}
	else
	{
		sdp = document.getElementById('select_sdp').value;
		if(sdp == '')
		{
			UIkit.notification({message: 'SDP may not be empty', status: 'warning', timeout: 2000});
			return;
		}
	}
	var jsonObj = [ { "section" : "source", "key" : "name", "value" : name},
					{ "section" : "source", "key" : "rtsp", "value" : rtsp},
					{ "section" : "source", "key" : "sdp",  "value" : sdp} ];				
	
	ajaxPostPutPatch("PATCH", "x-api/loggers/"+g_logger+"/config", JSON.stringify(jsonObj), handleUpdateSession);

}

function handleUpdateSession(status, jsonObj)
{
	if(status != 200)
	{
		UIkit.notification({message: jsonObj["reason"], status: 'danger', timeout: 3000})
	}	
	UIkit.modal(document.getElementById('update_session_modal')).hide();	
}

function showAddLogger()
{
	ajaxGet("x-api/sources", handleSourcesAddLogger)
}

function handleSourcesAddLogger(status, jsonObj)
{
	if(status == 200)
	{
		addSources('rtsp', jsonObj);
		addSources('sdp', jsonObj);
		UIkit.modal(document.getElementById('add_logger_modal')).show();	
	}
}


function addLogger()
{
	var name = document.getElementById('logger_name').value;
	if(name == "")
	{
		UIkit.notification({message: "Logger name cannot be empty", status: 'danger', timeout: 3000});
		return;
	}
	if(name.search(' ') != -1)
	{
		UIkit.notification({message: "Logger name cannot contain spaces", status: 'danger', timeout: 3000});
		return;
	}
	if(g_loggerArray.find(element => element==name))
	{
		UIkit.notification({message: "Logger already exits", status: 'danger', timeout: 3000});
		return;
	}
	
	var rtsp = '';
	var sdp = '';
	if(g_method == 'rtsp')
	{
		rtsp = document.getElementById('select_rtsp').value;
		if(rtsp == '')
		{
			UIkit.notification({message: 'RTSP may not be empty', status: 'warning', timeout: 2000});
			return;
		}
	}
	else
	{
		sdp = document.getElementById('select_sdp').value;
		if(sdp == '')
		{
			UIkit.notification({message: 'RTSP may not be empty', status: 'warning', timeout: 2000});
			return;
		}
	}
		
	var jsonObj = {	'name' : name,
					'source' : document.getElementById('config_name').value,
					'rtsp' : rtsp, 
					'sdp' : sdp,
					'wav' : document.getElementById('keep_wav').value,
					'opus' : document.getElementById('keep_opus').value,
					'flac' : document.getElementById('keep_flac').value};
		
	ajaxPostPutPatch('POST', 'x-api/loggers', JSON.stringify(jsonObj), handleAddLogger);

}

function handleAddLogger(status, jsonObj)
{
	UIkit.modal(document.getElementById('add_logger_modal')).hide();

	if(status != 200 || !('logger' in jsonObj))
	{
		UIkit.notification({message: jsonObj.reason, status: 'danger', timeout: 3000});	
	}
	else
	{
		g_loggerArray.push(jsonObj.logger);
		showLogger(jsonObj.logger);
	}
}

