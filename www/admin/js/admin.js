var g_jsonUsers = null;


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
    ajaxGet(location.host, "x-api/users", showUsers);
}


function showUsers(result, jsonObj)
{
	console.log("showUsers");
	console.log(jsonObj);
	if(result == 200)
	{
        g_jsonUsers = jsonObj;

        var tbody = document.getElementById('users');
        if(tbody)
		{
            for(let x in jsonObj)
            {
                var tr = document.createElement('tr');
                tr.username = jsonObj[x].username;
                tr.addEventListener("click", showUserModal, false);
                createCell('username', jsonObj[x], tr);
                createCell('webserver', jsonObj[x], tr);
                createCell('admin', jsonObj[x], tr);
                createCell('loggerserver', jsonObj[x], tr);
                createCell('encoderserver', jsonObj[x], tr);
                createCell('playbackserver', jsonObj[x], tr);

                tbody.append(tr);
            }
        }
	}
}

function createCell(name, jsonObj, tr)
{
    var td = document.createElement('td');
    td.innerHTML = jsonObj[name];
    td.username = tr.username;
    tr.appendChild(td);
}


function showUserModal(evt)
{
    if(evt.currentTarget && evt.currentTarget.username)
    {
        console.log("Edit user");
        document.getElementById('user_modal_title').innerHTML = "Edit User "+evt.currentTarget.username;
        document.getElementById('add_user_button').innerHTML = "Update User";

        document.getElementById('user_name').style.display = 'none';
        document.getElementById('password').value = '';

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
        document.getElementById('user_modal_title').innerHTML = "Add User";
        document.getElementById('add_user_button').innerHTML = "Add User";
       
        document.getElementById('user_name').style.display = '';
        document.getElementById('password').value = '';
        document.getElementById('perm_logging').checked = false;
        document.getElementById('perm_encoding').checked = false;
        document.getElementById('perm_playback').checked = false;
        document.getElementById('perm_admin').checked = false;

    }
    UIkit.modal(document.getElementById('user_modal')).show();	
}