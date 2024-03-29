var g_encoderArray = new Array();
var g_ws = null;
var g_encoder = null;
var g_action = '';
var g_encoder_host = location.host+":4430";


const CLR_PLAYING = "#92d14f";
const CLR_IDLE = "#8db4e2";
const CLR_ERROR =  "#ff7777";
const CLR_ORPHANED =  "#ffa000";
const CLR_NO_FILE = "#a0a0a0"
const CLR_UNKNOWN = "#c8c8c8"
const CLR_WARNING = "#ffa000"
const CLR_CONNECTING = "#ffff00";
const CLR_SYNC = "#008000";


function config()
{
	getConfig(handleConfig);
}

function showEncoder(encoder)
{
    var grid = document.getElementById('encoder_grid');

    var divMain = document.createElement('div');
    divMain.classList.add('uk-width-1-3@s', 'uk-width-1-6@xl');
    
    var aEncoder = document.createElement('a');
    aEncoder.classList.add('uk-link-reset', 'uk-display-block', 'uk-width-large@l', 'uk-width-medium@m', 'uk-card', 'uk-card-default', 'uk-card-body',
                            'uk-card-hover', 'uk-card-small');
    aEncoder.id = encoder;
    aEncoder.href = '';
    
    
    var divHeader = document.createElement('div');
    divHeader.className='uk-card-header';
    var titleH3 = document.createElement('h3');
    titleH3.className='uk-card-title';
    var titleSpan = document.createElement('span');
    titleSpan.id = "host_"+encoder;
    titleSpan.innerHTML = encoder;
    titleH3.appendChild(titleSpan);
    divHeader.appendChild(titleH3);
    
    var divBadge = document.createElement('div');
    divBadge.classList.add('uk-card-badge', 'uk-label', 'uk-label-warning');
    divBadge.innerHTML = "Unknown";
    divBadge.id = 'running_'+encoder; 
    divHeader.appendChild(divBadge);
    
    
    aEncoder.appendChild(divHeader);
    

    var divBody = document.createElement('div');
    divBody.classList.add('uk-card-body', 'uk-card-small');
    
    var divUpTimeGrid = document.createElement('div');
    divUpTimeGrid.classList.add('uk-child-width-expand', 'uk-grid-small', 'uk-text-left', 'uk-grid');
    var divUpTimeTitle = document.createElement('div');
    divUpTimeTitle.classList.add('uk-width-1-2', 'uk-text-primary')
    divUpTimeTitle.innerHTML = 'Up Time:'
    var divUpTime  = document.createElement('div');
    //divUpTime.classList.add('uk-label');
    divUpTime.id = 'up_time_'+encoder; 
    divUpTimeGrid.appendChild(divUpTimeTitle);
    divUpTimeGrid.appendChild(divUpTime);
    divBody.appendChild(divUpTimeGrid);

    
    var divSessionGrid = document.createElement('div');
    divSessionGrid.classList.add('uk-child-width-expand', 'uk-grid-small', 'uk-text-left', 'uk-grid');
    var divSessionTitle = document.createElement('div');
    divSessionTitle.classList.add('uk-width-1-2', 'uk-text-primary')
    divSessionTitle.innerHTML = 'Queue:'
	var divSession  = document.createElement('div');
    divSession.id = 'queue_'+encoder; 
    divSessionGrid.appendChild(divSessionTitle);
    divSessionGrid.appendChild(divSession);
    divBody.appendChild(divSessionGrid);

    var divFileGrid = document.createElement('div');
    divFileGrid.classList.add('uk-child-width-expand', 'uk-grid-small', 'uk-text-left', 'uk-grid');
    var divFileTitle = document.createElement('div');
    divFileTitle.classList.add('uk-width-1-3', 'uk-text-primary')
    divFileTitle.innerHTML = 'File:'
    var divFile  = document.createElement('div');
    divFile.id = 'file_'+encoder; 
	var divPercent  = document.createElement('div');
    divPercent.id = 'percent_'+encoder; 
    divFileGrid.appendChild(divFileTitle);
    divFileGrid.appendChild(divFile);
	divFileGrid.appendChild(divPercent);
    divBody.appendChild(divFileGrid);


	var divLastFileGrid = document.createElement('div');
    divLastFileGrid.classList.add('uk-child-width-expand', 'uk-grid-small', 'uk-text-left', 'uk-grid');
    var divLastFileTitle = document.createElement('div');
    divLastFileTitle.classList.add('uk-width-1-2', 'uk-text-primary')
    divLastFileTitle.innerHTML = 'Last File:'
    var divLastFile  = document.createElement('div');
    divLastFile.id = 'last_file_'+encoder; 
    divLastFileGrid.appendChild(divLastFileTitle);
    divLastFileGrid.appendChild(divLastFile);
    divBody.appendChild(divLastFileGrid);


	var divEncodedGrid = document.createElement('div');
    divEncodedGrid.classList.add('uk-child-width-expand', 'uk-grid-small', 'uk-text-left', 'uk-grid');
    var divEncodedTitle = document.createElement('div');
    divEncodedTitle.classList.add('uk-width-1-2', 'uk-text-primary')
    divEncodedTitle.innerHTML = 'Files Encoded:'
    var divEncoded  = document.createElement('div');
    divEncoded.id = 'files_encoded_'+encoder; 
    divEncodedGrid.appendChild(divEncodedTitle);
    divEncodedGrid.appendChild(divEncoded);
    divBody.appendChild(divEncodedGrid);


	aEncoder.appendChild(divBody);




	var divFooter = document.createElement('div');
    divFooter.className = 'uk-card-footer';
    var spanTimestamp  = document.createElement('span');
    spanTimestamp.id = 'timestamp_'+encoder; 
    divFooter.appendChild(spanTimestamp);
    aEncoder.appendChild(divFooter);

    divMain.appendChild(aEncoder);
    
    grid.appendChild(divMain);
}

function dashboard()
{
	getEncoders(handleEncoders);
}

function logs()
{
	var now = new Date();
	now.setMinutes(now.getMinutes() - now.getTimezoneOffset());
	document.getElementById('end_time').value = now.toISOString().slice(0, 16);
	now.setMinutes(now.getMinutes() - 60);
	document.getElementById('start_time').value = now.toISOString().slice(0, 16);
	getEncoders(populateSelectEncoders);
}

function populateSelectEncoders(status, jsData)
{
    if(status == 200)
    {
        g_encoderArray = jsData;
        if(g_encoderArray !== null)
        {
			var select = document.getElementById('select_log');
            g_encoderArray.forEach(function(el){
				var opt = document.createElement('option');
				opt.id = el;
				opt.value = el;
				opt.innerHTML = el;
				select.appendChild(opt);
			});
        }
	}
}

function handleEncoders(status, jsData)
{
    if(status == 200)
    {
        g_encoderArray = jsData;
        if(g_encoderArray !== null)
        {
            g_encoderArray.forEach(showEncoder);
        }

		ajaxGet(g_encoder_host, 'x-api/status', handleEncodersStatus);
    }
    else
    {
        console.log(status);
    }
}

function handleEncodersStatus(status, jsonObj)
{
	if(status == 200)
	{
		statusUpdate(jsonObj)
	}
	ws_connect('status',statusUpdate)
}

function statusUpdate(jsonObj)
{
    if(Array.isArray(jsonObj))
    {
        jsonObj.forEach(statusUpdateEncoder);
    }
    else
    {
      statusUpdateEncoder(jsonObj);
    }
}
function statusUpdateEncoder(jsonObj)
{
	
    if(jsonObj === null)
        return;
    console.log(jsonObj);
    if('id' in jsonObj)
    {
        var card = document.getElementById(jsonObj['id']);
	if(card === null)
	{	//card not created yet
	    return;
	}

        if('heartbeat' in jsonObj)
        {
            var span = document.getElementById('running_'+jsonObj['id']);
            span.className = 'uk-label';
            span.classList.add('uk-card-badge');
            span.classList.add('uk-label-success');
            span.innerHTML = 'running';

   	    if('timestamp' in jsonObj['heartbeat'])
            {
                var span = document.getElementById('timestamp_'+jsonObj['id']);
                var dt = new Date(jsonObj['heartbeat']['timestamp']*1000);
                span.innerHTML = dt.toISOString();
            }
            if('up_time' in jsonObj.heartbeat)
            {
                var up_time = jsonObj.heartbeat['up_time'];
                var span = document.getElementById('up_time_'+jsonObj['id']);
                var days = Math.floor(up_time / 86400);
                up_time = up_time % 86400;
                var hours = Math.floor(up_time / 3600);
                up_time = up_time % 3600;
                var minutes = Math.floor(up_time / 60);
                up_time = up_time % 60;
                span.innerHTML = zeroPad(days,4)+" "+zeroPad(hours,2)+":"+zeroPad(minutes,2)+":"+zeroPad(up_time,2);
           }
     	   
           if('queue' in jsonObj)
	   {
               var span = document.getElementById('queue_'+jsonObj['id']);
               span.innerHTML = jsonObj['queue'];
	   }
	   if('last_encoded' in jsonObj)
	   {
               var span = document.getElementById('last_file_'+jsonObj['id']);
               span.innerHTML = jsonObj['last_encoded'];
	   }
           if('files_encoded' in jsonObj)
           {
               var span = document.getElementById('files_encoded_'+jsonObj['id']);
               span.innerHTML = jsonObj['files_encoded'];
           }

	   if('encoded' in jsonObj)
	   {
               var span = document.getElementById('percent_'+jsonObj['id']);
	       if(span !== null)
               {
	           span.innerHTML = Math.round(jsonObj['encoded']*100.0)+"%";
	       }
	   }

           if('filename' in jsonObj)
           {
               var span = document.getElementById('file_'+jsonObj['id']);
               span.innerHTML = jsonObj['filename'];
           }
           else
           {
               var span = document.getElementById('file_'+jsonObj['id']);
               span.innerHTML = "Not Encoding";
           }
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

	g_ws = new WebSocket(ws_protocol+"//"+g_encoder_host+"/x-api/ws/"+endpoint+"?access_token="+g_access_token);
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



function getEncoders(callback)
{
	ajaxGet(g_encoder_host, "x-api/encoders",callback);
}


function getStatus(callback)
{
	ajaxGet(g_encoder_host, "x-api/status",callback);
}

function getPower(callback)
{
	ajaxGet(g_encoder_host, "x-api/power",callback);
}

function getConfig(callback)
{
	ajaxGet(g_encoder_host, "x-api/config", callback);
}

function getInfo(callback)
{
	ajaxGet(g_encoder_host, "x-api/info", callback);
}


function getUpdate(callback)
{
	ajaxGet(g_encoder_host, "x-api/update",callback);
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
                document.getElementById('encodingserver-version').innerHTML = jsonObj.server.version;
            }
            if('date' in jsonObj.server)
            {
                document.getElementById('encodingserver-date').innerHTML = jsonObj.server.date;
            }
        }
        if('encoder' in jsonObj)
        {
            if('version' in jsonObj.server)
            {
                document.getElementById('encoder-version').innerHTML = jsonObj.server.version;
            }
            if('date' in jsonObj.server)
            {
                document.getElementById('encoder-date').innerHTML = jsonObj.server.date;
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
		ajaxPostPutPatch(g_encoder_host, "PUT", "x-api/power", JSON.stringify(play), handleRestartPut);
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

	ajaxGet(g_encoder_host, endpoint, handleGetLogs);
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

function encoders()
{
	const params = new Proxy(new URLSearchParams(window.location.search), { get: (searchParams, prop) => searchParams.get(prop)});
	
	g_encoder = params.encoder;

	ajaxGetg_encoder_host, ("x-api/encoders/"+g_encoder+"/status", connectToEncoder)
}

function connectToEncoder(status, jsonObj)
{
	if(status == 200)
	{
		handleEncoderInfo(jsonObj);
		ws_connect("encoders/"+g_encoder, handleEncoderInfo);
	}
	else
	{
		UIkit.notification({message: jsonObj["reason"], status: 'danger', timeout: 3000});
	}
}


function handleEncoderInfo(jsonObj)
{
	console.log(jsonObj);
	if("id" in jsonObj)
	{
		document.getElementById('encoder').innerHTML = jsonObj.id;
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
	if('filename' in jsonObj)
	{
var elm = document.getElementById('file-filename');
                elm.innerHTML = jsonObj.filename;
                elm.className = 'uk-text-success';
	}
	if('queue' in jsonObj)
	{
		var elm = document.getElementById('queue');
                elm.innerHTML = jsonObj.queue;
                elm.className = 'uk-text-success';
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

function encoderAdmin()
{
	if(g_action == 'restart')
	{
		var play = { "command" : "restart", "password" : document.getElementById('admin_password').value};
		ajaxPostPutPatch(g_encoder_host, "PUT", "x-api/encoders/"+g_encoder, JSON.stringify(play), handleRestartEncoder);
	}
	else if(g_action == 'remove')
	{
		var play = {"password" : document.getElementById('admin_password').value};
		ajaxPostPutPatch(g_encoder_host, "DELETE", "x-api/encoders/"+g_encoder, JSON.stringify(play), handleDeleteEncoder);
	}
}


function handleRestartEncoder(status, jsonObj)
{
	if(status != 200)
	{
		UIkit.notification({message: jsonObj["reason"], status: 'danger', timeout: 3000})
	}
	UIkit.modal(document.getElementById('password_modal')).hide();
}

function handleDeleteEncoder(status, jsonObj)
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
	ajaxGetg_encoder_host, ("x-api/sources", handleSources)
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
		document.getElementById('encoder_admin').innerHTML = "Restart Encoder";
	}
	UIkit.modal(document.getElementById('password_modal')).show();	
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
