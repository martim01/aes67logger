var g_jsonUsers = null;
var g_user = null;

function findUser(name)
{
    for(let x in g_jsonUsers)
    {
        if(g_jsonUsers[x]["username"] == name)
        {
            return g_jsonUsers[x];
        }
    }
    return null;
}

function getUsers()
{
    getCookies();
    ajaxGet(location.host, "x-api/users", showUsers);
}


function showUsers(result, jsonObj)
{
console.log(jsonObj);
	if(result == 200)
	{
        g_jsonUsers = jsonObj;

        var tbody = document.getElementById('users');
        if(tbody)
		{
            while(tbody.hasChildNodes())
            {
                tbody.removeChild(tbody.lastChild);
            }

            for(let x in jsonObj)
            {
                var tr = document.createElement('tr');
                tr.username = jsonObj[x].username;
                tr.addEventListener("click", showUserModal, false);
                createCell('username', jsonObj[x], tr);
                createCell('loggerserver', jsonObj[x], tr);
                createCell('encoderserver', jsonObj[x], tr);
                createCell('playbackserver', jsonObj[x], tr);
                createCell('admin', jsonObj[x], tr);

                tbody.append(tr);
            }
        }
	}
    else
    {
        UIkit.notification({message: jsonObj.reason, status: 'danger', timeout: 3000});	
    }
}

function createCell(name, jsonObj, tr)
{
    var td = document.createElement('td');
    if(jsonObj[name] === true)
    {
        var img = document.createElement('img');
        img.src = '../images/tick.png'
        td.appendChild(img);
    }
    else if(jsonObj[name] === false)
    {
        td.innerHTML = '&nbsp;';
    }
    else
    {
        td.innerHTML = jsonObj[name];
    }
    td.username = tr.username;
    tr.appendChild(td);
}


function showUserModal(evt)
{
    if(evt.currentTarget && evt.currentTarget.username)
    {
        g_user = evt.currentTarget.username;
        document.getElementById('user_modal_title').innerHTML = "Edit User "+evt.currentTarget.username;
        document.getElementById('add_user_button').innerHTML = "Update Permissions";

        document.getElementById('user_name').style.display = 'none';
        document.getElementById('password').value = '';
        document.getElementById('update_password_button').style.display='inline';

        var jsonObj = findUser(evt.currentTarget.username);
        if(jsonObj !== null)
        {    
            document.getElementById('perm_logging').checked = jsonObj["loggerserver"];
            document.getElementById('perm_encoding').checked = jsonObj["encoderserver"];
            document.getElementById('perm_playback').checked = jsonObj["playbackserver"];
            document.getElementById('perm_admin').checked = jsonObj["admin"];
        }
        else
        {
            document.getElementById('perm_logging').checked = false;
            document.getElementById('perm_encoding').checked = false;
            document.getElementById('perm_playback').checked = false;
            document.getElementById('perm_admin').checked = false;
        }
    }
    else
    {
        g_user = null;


        document.getElementById('user_modal_title').innerHTML = "Add User";
        document.getElementById('add_user_button').innerHTML = "Add User";
       
        document.getElementById('user_name').style.display = '';
        document.getElementById('password').value = '';
        document.getElementById('update_password_button').style.display='none';
        document.getElementById('perm_logging').checked = false;
        document.getElementById('perm_encoding').checked = false;
        document.getElementById('perm_playback').checked = false;
        document.getElementById('perm_admin').checked = false;

    }
    UIkit.modal(document.getElementById('user_modal')).show();	
}

function updateUser()
{
    if(g_user !== null)
    {
        patchUser();
    }
    else
    {
        addUser();
    }
}

function patchUser()
{	
    var jsonObj = {	'username' : g_user,
					'webserver' : true,
					'admin' : document.getElementById('perm_admin').checked, 
					'loggerserver' : document.getElementById('perm_logging').checked,
					'encoderserver' : document.getElementById('perm_encoding').checked,
					'playbackserver' : document.getElementById('perm_playback').checked
                };

    ajaxPostPutPatch(location.host, 'PATCH', 'x-api/users', JSON.stringify(jsonObj), handleUpdateUser);
}

function updatePassword()
{	
    var password = document.getElementById('password').value;
	if(password == "")
	{
		UIkit.notification({message: "Password cannot be empty", status: 'danger', timeout: 3000});
		return;
	}

    var jsonObj = {	'username' : g_user,
					'password' : password
                };

    ajaxPostPutPatch(location.host, 'PATCH', 'x-api/users', JSON.stringify(jsonObj), handleUpdateUser);
}

function addUser()
{
    var username = document.getElementById('user_name').value;
	if(username == "")
	{
		UIkit.notification({message: "Username cannot be empty", status: 'danger', timeout: 3000});
		return;
	}
    var password = document.getElementById('password').value;
	if(password == "")
	{
		UIkit.notification({message: "Password cannot be empty", status: 'danger', timeout: 3000});
		return;
	}

    var jsonObj = {	'username' : username,
                    'password' : password,
					'webserver' : true,
					'admin' : document.getElementById('perm_admin').checked, 
					'loggerserver' : document.getElementById('perm_logging').checked,
					'encoderserver' : document.getElementById('perm_encoding').checked,
					'playbackserver' : document.getElementById('perm_playback').checked
                };

    ajaxPostPutPatch(location.host, 'POST', 'x-api/users', JSON.stringify(jsonObj), handleUpdateUser);
}


function handleUpdateUser(result, jsonObj)
{
	console.log(jsonObj);
    UIkit.modal(document.getElementById('user_modal')).hide();

	if(result == 200 || result == 201)
	{
        getUsers();
	}
    else
    {
        UIkit.notification({message: jsonObj.reason, status: 'danger', timeout: 3000});	
    }
}
