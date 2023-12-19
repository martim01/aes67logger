var g_loggerArray = new Array();
var g_sourceArray = new Array();
var g_count = 0;
var g_ws = null;
var g_logger = null;
var g_action = '';
var g_sourceRouting = {};
var g_logger_host = location.hostname+":4431";
var g_loggerDetails = {};
var g_loadedSources = {};

const CLR_PLAYING = "#92d14f";
const CLR_IDLE = "#8db4e2";
const CLR_ERROR =  "#ff7777";
const CLR_ORPHANED =  "#ffa000";
const CLR_NO_FILE = "#a0a0a0"
const CLR_UNKNOWN = "#c8c8c8"
const CLR_WARNING = "#ffa000"
const CLR_CONNECTING = "#ffff00";
const CLR_SYNC = "#008000";

function msToTime(duration) {
  var milliseconds = Math.floor((duration % 1000) / 100),
    seconds = Math.floor((duration / 1000) % 60),
    minutes = Math.floor((duration / (1000 * 60)) % 60),
    hours = Math.floor((duration / (1000 * 60 * 60)) % 24);

  hours = (hours < 10) ? "0" + hours : hours;
  minutes = (minutes < 10) ? "0" + minutes : minutes;
  seconds = (seconds < 10) ? "0" + seconds : seconds;

  return hours + ":" + minutes + ":" + seconds + "." + milliseconds;
}

function showLogger(jsonObj)
{
    var grid = document.getElementById('logger-grid');

    var divMain = document.createElement('div');
    divMain.classList.add('uk-width-1-3@s', 'uk-width-1-6@xl');
    
    var aLogger = document.createElement('a');
    aLogger.classList.add('uk-link-reset', 'uk-display-block', 'uk-width-large@l', 'uk-width-medium@m', 'uk-card', 'uk-card-default', 'uk-card-body',
                            'uk-card-hover', 'uk-card-small');
    aLogger.id = jsonObj.name;
    aLogger.href = 'loggers/index.html?logger='+jsonObj.name;
    
    
    var divHeader = document.createElement('div');
    divHeader.className='uk-card-header';
    var titleH3 = document.createElement('h3');
    titleH3.className='uk-card-title';
    var titleSpan = document.createElement('span');
    titleSpan.id = "host_"+jsonObj.name;
    titleSpan.innerHTML = jsonObj.name;
    titleH3.appendChild(titleSpan);
    divHeader.appendChild(titleH3);
    
    var divBadge = document.createElement('div');
    
    if(jsonObj.settings && jsonObj.settings.enable && jsonObj.settings.enable.current !== undefined)
    {
	if(jsonObj.settings.enable.current === true)
        {
           divBadge.innerHTML = "Enabled";
           divBadge.classList.add('uk-card-badge', 'uk-label', 'uk-label-success');
        }
        else
        {
             divBadge.innerHTML = "Disabled";
             divBadge.classList.add('uk-card-badge', 'uk-label', 'uk-label-warning');
        }
    }
    else
    {
    	divBadge.innerHTML = "Unknown";
        divBadge.classList.add('uk-card-badge', 'uk-label', 'uk-label-danger');
    }
    divBadge.id = 'running_'+jsonObj.name; 
    divHeader.appendChild(divBadge);
    
    
    aLogger.appendChild(divHeader);
    

    var divBody = document.createElement('div');
    divBody.classList.add('uk-card-body', 'uk-card-small');
    
    var divSourceGrid = document.createElement('div');
    divSourceGrid.classList.add('uk-child-width-expand', 'uk-grid-small', 'uk-text-left', 'uk-grid');
    var divSourceTitle = document.createElement('div');
    divSourceTitle.classList.add('uk-width-1-2', 'uk-text-primary')
    divSourceTitle.innerHTML = 'Source:'
    divSourceTitle.id = 'sourceTitle_'+jsonObj.name;
    var divSource  = document.createElement('div');
    if(jsonObj.mixer)
    {
	divSourceTitle.channels = jsonObj.mixer.length;
        for(var i = 0; i < jsonObj.mixer.length; i++)
	{
	  var divSourceBadge = document.createElement('div');
          divSourceBadge.classList.add('uk-label', 'uk-label-danger');
	  divSourceBadge.innerHTML = jsonObj.mixer[i].source.name+ "-"+jsonObj.mixer[i].source.channel;
          divSourceBadge.id = 'source_'+jsonObj.name+'_'+i;
          divSource.appendChild(divSourceBadge);

	  if(g_sourceRouting[jsonObj.mixer[i].source.name] === undefined)
	  {
	      g_sourceRouting[jsonObj.mixer[i].source.name] = new Array();
	  }
	  g_sourceRouting[jsonObj.mixer[i].source.name].push(jsonObj.name);
	}
    }
    //divUpTime.classList.add('uk-label');
    divSource.id = 'up_time_'+jsonObj.name; 
    divSourceGrid.appendChild(divSourceTitle);
    divSourceGrid.appendChild(divSource);
    divBody.appendChild(divSourceGrid);

    

    var divFileGrid = document.createElement('div');
    divFileGrid.classList.add('uk-child-width-expand', 'uk-grid-small', 'uk-text-left', 'uk-grid');

    var divFileTitle = document.createElement('div');
    divFileTitle.classList.add('uk-width-1-2', 'uk-text-primary')
    divFileTitle.innerHTML = 'File:'
    var divFile  = document.createElement('div');
    divFile.id = 'file_'+jsonObj.name; 
    if(jsonObj.advanced && jsonObj.advanced.filename)
    {
        divFile.innerHTML = jsonObj.advanced.filename;
    }


    var divFileLengthTitle = document.createElement('div');
    divFileLengthTitle.classList.add('uk-width-1-2', 'uk-text-primary')
    divFileLengthTitle.innerHTML = 'Length:'
    var divFileLength  = document.createElement('div');
    divFileLength.id = 'filelength_'+jsonObj.name;
    if(jsonObj.advanced && jsonObj.advanced.length)
    {
	var length = new Date(jsonObj.advanced.length)
    }

    divFileGrid.appendChild(divFileTitle);
    divFileGrid.appendChild(divFile);
    divFileGrid.appendChild(divFileLengthTitle);
    divFileGrid.appendChild(divFileLength);



    divBody.appendChild(divFileGrid)
    aLogger.appendChild(divBody);

    var divFooter = document.createElement('div');
    divFooter.className = 'uk-card-footer';
    var spanTimestamp  = document.createElement('span');
    spanTimestamp.id = 'timestamp_'+jsonObj.name; 
    divFooter.appendChild(spanTimestamp);
    aLogger.appendChild(divFooter);

    divMain.appendChild(aLogger);
    
    var elmB = document.getElementById('logger-add');
    if(elmB)
    {
        grid.insertBefore(divMain,elmB);
    }
}

function dashboard()
{
	loggers();
}

function config()
{
	getConfig(handleConfig);
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

		ajaxGet(g_logger_host,  'x-api/status', handleLoggersStatus);
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

	 
	console.log(ws_protocol+"//"+g_logger_host+"/x-api/ws/"+endpoint);
	g_ws = new WebSocket(ws_protocol+"//"+g_logger_host+"/x-api/ws/"+endpoint+"?access_token="+g_access_token);
    g_ws.timeout = true;
	g_ws.onopen = function(ev)  { console.log("Ws_open"); this.tm = setTimeout(serverOffline, 4000) };
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
        var elm = document.getElementById('current-time');
        elm.innerHTML = dt.toISOString();
        elm.className = 'uk-h3';
        elm.classList.add('uk-text-success');
        
		
		var jsonObj = JSON.parse(ev.data);
        callbackMessage(jsonObj);
	}	
}



function getLoggers(callback)
{
	ajaxGet(g_logger_host, "x-api/loggers",callback);
}


function getStatus(callback)
{
	ajaxGet(g_logger_host, "x-api/status",callback);
}

function getPower(callback)
{
	ajaxGet(g_logger_host, "x-api/power",callback);
}

function getConfig(callback)
{
	ajaxGet(g_logger_host, "x-api/config", callback);
}

function getInfo(callback)
{
	ajaxGet(g_logger_host, "x-api/info", callback);
}


function getUpdate(callback)
{
	ajaxGet(g_logger_host, "x-api/update",callback);
}







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
		ajaxPostPutPatch(g_logger_host, "PUT", "x-api/power", JSON.stringify(play), handleRestartPut);
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

	ajaxGet(g_logger_host, endpoint, handleGetLogs);
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
	console.log("loggers");
	getCookies();
	ajaxGet(g_logger_host, "x-api/plugins/destinations", handleDestinations);
}

function handleDestinations(status, jsonObj)
{
	if(status == 200)
	{
		g_loggerArray = jsonObj;
		g_count = 0;
		if(g_loggerArray.length > 0)
		{
			getLogger();
		}
		else
		{
			ajaxGet(g_logger_host, "x-api/plugins/sources", handleSources);
                }
	}
}

function getLogger()
{
	ajaxGet(g_logger_host, "x-api/plugins/destinations/"+g_loggerArray[g_count], handleGotLogger);
}

function handleGotLogger(status, jsonObj)
{
	if(status == 200)
	{
		if(jsonObj.plugin && jsonObj.plugin == "Recorder" && jsonObj.name)
		{
			showLogger(jsonObj);
		}

		++g_count;
		if(g_count < g_loggerArray.length)
		{
			getLogger();
		}
		 else
                {
                        ajaxGet(g_logger_host, "x-api/plugins/sources", handleSources);
                }
         }
}

function handleSources(status, jsonObj)
{
	 if(status == 200)
        {
                g_sourceArray = jsonObj;
                g_count = 0;
                if(g_sourceArray.length > 0)
                {
                        getSource();
                }
        }

}

function getSource()
{
        ajaxGet(g_logger_host, "x-api/plugins/sources/"+g_sourceArray[g_count], handleGotSource);
}


function handleGotSource(status, jsonObj)
{
    if(status == 200)
    {
        if(jsonObj.plugin && jsonObj.plugin == "AoIp")
        {
	    handleGotAoIp(jsonObj);
        }
        ++g_count;
        if(g_count < g_sourceArray.length)
        {
            getSource();
        }
	else
	{
	    ws_connect('plugins', handleRecorderInfo);
	}
    }
}

function handleGotAoIp(jsonObj)
{
    if(g_sourceRouting[jsonObj.name] !== undefined)
    {
        for(var i = 0; i < g_sourceRouting[jsonObj.name].length; i++)
        {
            var elmChannels = document.getElementById('sourceTitle_'+g_sourceRouting[jsonObj.name][i]);
            if(elmChannels)
            {
               for(var n = 0; n < elmChannels.channels; n++)
               {
                   var elm = document.getElementById('source_'+g_sourceRouting[jsonObj.name][i]+'_'+n);
                   if(elm && elm.innerHTML.split('-')[0] == jsonObj.name)
                   {
                        elm.classList.remove('uk-label-warning');
                        elm.classList.remove('uk-label-danger');
                        elm.classList.remove('uk-label-success');
                        if(jsonObj.settings.enable.current === undefined)
                        {
                            elm.classList.add('uk-label-danger');
                        }
                        else if(jsonObj.settings.enable.current === true)
                        {
                             elm.classList.add('uk-label-success');
                        }
                        else
                        {
                            elm.classList.add('uk-label-warning');
                        }
                    }
               }
            }
        }
    }
}


function handleRecorderInfo(jsonObj)
{
    if(jsonObj.plugin !== undefined && jsonObj.plugin === "Recorder" && jsonObj.name !== undefined)
    {
         if(jsonObj.filename !== undefined)
	 {
	     var file = document.getElementById('file_'+jsonObj.name);
	     if(file)
	     {
	         file.innerHTML = jsonObj.filename;
	     }
         }
         if(jsonObj.length !== undefined)
         {
	         
             var length= document.getElementById('filelength_'+jsonObj.name);
             if(length)
             {
                length.innerHTML = msToTime(jsonObj.length);
             }
        }
	
    }
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
		document.getElementById('qos-bitrate').innerHTML = jsonObj.qos["kbits/s"]["current"].toFixed(2) + " kbps";
		document.getElementById('qos-received').innerHTML = jsonObj.qos["packets"]["total"]["received"];
		document.getElementById('qos-lost').innerHTML = jsonObj.qos["buffer"]["packets"]["missing"];
		document.getElementById('qos-errors').innerHTML = jsonObj.qos["packets"]["out_of_order"];
		document.getElementById('qos-interpacket').innerHTML = (jsonObj.qos["interpacketGap"]["average"]).toFixed(4)+" ms";
		document.getElementById('qos-jitter').innerHTML = (jsonObj.qos['jitter'].toFixed(4))+" ms";
		document.getElementById('qos-tsdf').innerHTML = (jsonObj.qos['tsdf'].toFixed(4))+" ms";
		//document.getElementById('qos-duration').innerHTML = jsonObj.qos['duration']+" &mu;s";
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

//		if('subsessions' in jsonObj.session && jsonObj.session.subsessions.length > 0)
//		{
//			document.getElementById('subsession-id').innerHTML = jsonObj.session.subsessions[0].id;
//			document.getElementById('subsession-source_address').innerHTML = jsonObj.session.subsessions[0].source_address;
//			document.getElementById('subsession-medium').innerHTML = jsonObj.session.subsessions[0].medium;
//			document.getElementById('subsession-codec').innerHTML = jsonObj.session.subsessions[0].codec;
//			document.getElementById('subsession-protocol').innerHTML = jsonObj.session.subsessions[0].protocol;
//			document.getElementById('subsession-port').innerHTML = jsonObj.session.subsessions[0].port;
//			document.getElementById('subsession-sample_rate').innerHTML = jsonObj.session.subsessions[0].sample_rate;
//			document.getElementById('subsession-channels').innerHTML = jsonObj.session.subsessions[0].channels;
//			document.getElementById('subsession-sync_timestamp').innerHTML = jsonObj.session.subsessions[0].sync_timestamp;

//			ShowClock(jsonObj.session.subsessions[0]);
//		}
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
		document.getElementById('source-name').innerHTML = jsonObj.session;
		
		//document.getElementById('source-type').innerHTML = 'jsonObj.streaming.type';
		//if(jsonObj.streaming.type == 'RTSP')
		//{
	//		document.getElementById('div_source-sdp').style.display = 'none';
//			document.getElementById('div_source-rtsp').style.display= 'block';
			//document.getElementById('source-rtsp').innerHTML = jsonObj.streaming.source;
		//}
		//else
		//{
	//		document.getElementById('div_source-rtsp').style.display = 'none';
//			document.getElementById('div_source-sdp').style.display = 'block';
//			document.getElementById('source-sdp').innerHTML = jsonObj.streaming.source;
//		}
		var elm = document.getElementById('source-connection');
		elm.className = 'uk-card-badge';
		elm.classList.add('uk-label');

		if(jsonObj.streaming)
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
return;
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
		ajaxPostPutPatch(g_logger_host, "PUT", "x-api/loggers/"+g_logger, JSON.stringify(play), handleRestartLogger);
	}
	else if(g_action == 'remove')
	{
		var play = {"password" : document.getElementById('admin_password').value};
		ajaxPostPutPatch(g_logger_host, "DELETE", "x-api/loggers/"+g_logger, JSON.stringify(play), handleDeleteLogger);
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
	ajaxGet(g_logger_host, "x-api/sources", handleSources)
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
	
					ajaxPostPutPatch(g_logger_host, "PATCH", "x-api/loggers/"+g_logger+"/config", JSON.stringify(jsonObj), handleUpdateSession);

}

function handleUpdateSession(status, jsonObj)
{
	if(status != 200)
	{
		UIkit.notification({message: jsonObj["reason"], status: 'danger', timeout: 3000})
	}	
	UIkit.modal(document.getElementById('update_session_modal')).hide();	
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
					'wav' : parseInt(document.getElementById('keep_wav').value),
					'opus' : parseInt(document.getElementById('keep_opus').value),
					'flac' : parseInt(document.getElementById('keep_flac').value),
					'filelength' : parseInt(document.getElementById('filelength').value)};
		
					ajaxPostPutPatch(g_logger_host, 'POST', 'x-api/loggers', JSON.stringify(jsonObj), handleAddLogger);

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



function handleConfig(result, jsonObj)
{
	console.log("handleConfig");
	console.log(jsonObj);
	if(result == 200)
	{
		for(let section in jsonObj)
		{
			showConfigSection(section, jsonObj[section]);
		}
	}
}

function showConfigSection(section, jsonObj)
{
	console.log("showConfigSection "+section);
	console.log(jsonObj);

    var grid = document.getElementById('config_grid');

    var divMain = document.createElement('div');
    divMain.classList.add('uk-width-1-3@s', 'uk-width-1-5@xl');
    
	var aSection = document.createElement('a');
    aSection.classList.add('uk-link-reset', 'uk-display-block', 'uk-card', 'uk-card-default', 'uk-card-body');
	aSection.id = section;
    

    var divHeader = document.createElement('div');
    divHeader.className='uk-card-header';
    var titleH3 = document.createElement('h3');
    titleH3.className='uk-card-title';
    var titleSpan = document.createElement('span');
    titleSpan.id = "section_"+section;
    titleSpan.innerHTML = section;
    titleH3.appendChild(titleSpan);
    divHeader.appendChild(titleH3);
    
       
    aSection.appendChild(divHeader);
    

    var divBody = document.createElement('div');
    divBody.classList.add('uk-card-body', 'uk-card-small');
    
	
   
	for(let key in jsonObj)
	{
		
		if(typeof jsonObj[key] === 'object')
		{
			for(let subkey in jsonObj[key])
			{
				createKeyValue(key+"-"+subkey, jsonObj[key][subkey], divBody);	
			}
		}
		else
		{
			createKeyValue(key, jsonObj[key], divBody);
		}
	}

    aSection.appendChild(divBody);

    
    divMain.appendChild(aSection);
    
    grid.appendChild(divMain);
}

function createKeyValue(key, value, parentElement)
{
	var divSectionGrid = document.createElement('div');
    divSectionGrid.classList.add('uk-child-width-expand', 'uk-grid-small', 'uk-text-left', 'uk-grid');

	var divKey = document.createElement('div');
	divKey.classList.add('uk-width-1-3', 'uk-text-primary')
	divKey.innerHTML = key+":";
	var divValue  = document.createElement('div');
	divValue.classList.add('uk-width-2-3')
	divValue.innerHTML = value;
	divSectionGrid.appendChild(divKey);
	divSectionGrid.appendChild(divValue);
	parentElement.appendChild(divSectionGrid);
}


function logger()
{
	getCookies();
	const params = new Proxy(new URLSearchParams(window.location.search), { get: (searchParams, prop) => searchParams.get(prop)});
	g_logger = params.logger;

	ajaxGet(g_logger_host, "x-api/plugins/destinations/"+g_logger, handleRecorder);
}



function handleRecorder(status, jsonObj)
{
	if(status == 200)
	{
		g_loggerDetails = jsonObj;

		if(jsonObj.advanced !== undefined)
		{
			document.getElementById('file-name').innerHTML = jsonObj.advanced.filename;
			document.getElementById('file-length').innerHTML = msToTime(jsonObj.advanced.length);
			document.getElementById('file-channels').innerHTML = jsonObj.advanced.channels;
			document.getElementById('file-bits').innerHTML = jsonObj.advanced.bits;
			document.getElementById('file-samplerate').innerHTML = jsonObj.advanced.sample_rate;
		}
		g_count = 0;
		getRecorderSources();
	}
}

function getRecorderSources()
{
	if(g_loggerDetails.mixer !== undefined && g_count < g_loggerDetails.mixer.length)
	{
		if(g_loadedSources[g_loggerDetails.mixer[g_count].source.name] === undefined)
		{
			ajaxGet(g_logger_host, "x-api/plugins/sources/"+g_loggerDetails.mixer[g_count].source.name, handleRecorderSource);
		}
		++g_count;
	}
	else
	{

	}
}

function handleRecorderSource(status, jsonObj)
{
	if(status == 200)
	{
		g_loadedSources[jsonObj.name] = jsonObj;
	}
}
