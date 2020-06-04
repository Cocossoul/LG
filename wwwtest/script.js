//links
//http://eloquentjavascript.net/09_regexp.html
//https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Regular_Expressions


var messages = [], //array that hold the record of each string in chat
  lastUserMessage = "", //keeps track of the most recent input string from the user
  botMessage = "", //var keeps track of what the chatbot is going to say
  botName = 'Chatbot', //name of the chatbot
  talking = true; //when false the speach function doesn't work
var chatbox;
var chatlog;
var logSize = 8;
var request;
var timer;
var playerName = "joueur";
var password = "none";
var logged = false;

function init()
{
    request = new XMLHttpRequest();
    request.onload = request_callback;
    request.responseType = "text";
    chatbox = document.getElementById("chatbox");
    chatlog = document.getElementById("chatlog");
    timer = window.setInterval(update, 1000);
}
function update() {
    if (!logged)
        return;
    send_to_server("update_"+playerName+"_"+password+"_");
}

function send_to_server(str) {
    try {
        request.open("get", str);
        request.send();
    }
    catch(err) {
        messages.push("<b>ERROR:</b> Impossible de communiquer avec le serveur, déconnection...");
        updateChatlog();
        logged = false;
    }
}

function get_next_char(str, c, n) {
    while (str[n] != c && n < str.length)
        n++;
    return n;
}

function process_resource(str) {
    var cmd, name, content;
    var sep = '_';
    var cmdE = get_next_char(str, sep, 0);
    var cmd = str.substring(0, cmdE);
    if (str[cmdE] != '_')
        return null;
    var nameE = get_next_char(str, sep, cmdE + 1);
    var name = str.substring(cmdE + 1, nameE);
    if (str[nameE] != '_')
        return null;
    var content = str.substring(nameE + 1, str.length);
    return [cmd, name, content];
}

function request_callback()
{
    var response = request.responseText.split("\n");
    for (line of response) {
        if (line == "")
            continue;
        var resource = process_resource(line);
        console.log("recieved " + resource[0]);
        switch (resource[0]) {
            case "send":
                messages.push("<b>" + resource[1] + ":</b> " + resource[2]);
                break;
            case "connect":
                logged = true;
                messages.push("<b>" + resource[1] + ":</b> " + resource[2]);
                break;
            case "notconnected":
                logged = false;
                messages.push("<b>" + resource[1] + ":</b> " + resource[2]);
                break;
        }
        updateChatlog();
    }
}
//
//
//****************************************************************
//****************************************************************
//****************************************************************
//****************************************************************
//****************************************************************
//****************************************************************
//****************************************************************
//edit this function to change what the chatbot says
/*function chatbotResponse() {
  talking = true;
  botMessage = "I'm confused"; //the default message

  if (lastUserMessage === 'hi' || lastUserMessage =='hello') {
    const hi = ['hi','howdy','hello']
    botMessage = hi[Math.floor(Math.random()*(hi.length))];;
  }

  if (lastUserMessage === 'name') {
    botMessage = 'My name is ' + botName;
  }
}*/
//****************************************************************
//****************************************************************
//****************************************************************
//****************************************************************
//****************************************************************
//****************************************************************
//****************************************************************
//
//
//

function updateChatlog() {
    for (var i = 1; i < 8; i++) {
        if (messages[messages.length - i])
            document.getElementById("chatlog" + i).innerHTML = messages[messages.length - i];
    }
}

function processPlayerInput(pInput) {
    if (pInput == "")
        return;
    if (pInput[0] == '/') {
        var cmdArgs = pInput.split(" ");
        switch(cmdArgs[0]) {
            case "/connect":
                if (cmdArgs.length < 3 && playerName == "joueur") {
                    messages.push("<b>ERROR:</b> /connect name password");
                    updateChatlog();
                    break;
                }
                if (playerName == "joueur" || cmdArgs.length > 2) {
                    playerName = cmdArgs[1];
                    password = cmdArgs[2];
                }
                send_to_server("connect_"+playerName+"_"+password+"_");
                break;
            case "/echo":
                send_to_server("echo_"+playerName+"_"+password+"_");
                break;
            case "/list":
                send_to_server("list___");
            case "/info":
                console.log("Name: "+ playerName);
                console.log("Password:" + password);
                break;
        }
        return;
    }
    send_to_server("send_"+playerName+"_"+password+"_"+pInput);
}
//this runs each time enter is pressed.
//It controls the overall input and output
function newEntry() {
    //if the message from the user isn't empty then run
    if (chatbox.value != "") {
        //pulls the value from the chatbox ands sets it to lastUserMessage
        lastUserMessage = chatbox.value;
        //sets the chat box to be clear
        chatbox.value = "";
        //adds the value of the chatbox to the array messages
        messages.push("<b>moi:</b> " + lastUserMessage);
        processPlayerInput(lastUserMessage);
        updateChatlog();
        /*
            //Speech(lastUserMessage);  //says what the user typed outloud
            //sets the variable botMessage in response to lastUserMessage
        chatbotResponse();
        //add the chatbot's name and message to the array messages
        messages.push("<b>" + botName + ":</b> " + botMessage);
// says the message using the text to speech function written below
        Speech(botMessage);
//outputs the last few array elements of messages to html
*/
}
}
/*
    //text to Speech
    //https://developers.google.com/web/updates/2014/01/Web-apps-that-talk-Introduction-to-the-Speech-Synthesis-API
function Speech(say) {
    if ('speechSynthesis' in window && talking) {
        var utterance = new SpeechSynthesisUtterance(say);
        //msg.voice = voices[10]; // Note: some voices don't support altering params
        //msg.voiceURI = 'native';
        //utterance.volume = 1; // 0 to 1
        //utterance.rate = 0.1; // 0.1 to 10
        //utterance.pitch = 1; //0 to 2
        //utterance.text = 'Hello World';
        //utterance.lang = 'en-US';
        speechSynthesis.speak(utterance);
    }
}
*/
        //runs the keypress() function when a key is pressed
        document.onkeypress = keyPress;
        //if the key pressed is 'enter' runs the function newEntry()
        function keyPress(e) {
            var x = e || window.event;
            var key = (x.keyCode || x.which);
            if (key == 13 || key == 3) {
                //runs this function when enter is pressed
                newEntry();
            }
            /*
    if (key == 38) {
        console.log('hi')
//document.getElementById("chatbox").value = lastUserMessage;
    }
    */
}

//clears the placeholder text ion the chatbox
//this function is set to run when the users brings focus to the chatbox, by clicking on it
function placeHolder() {
    chatbox.placeholder = "";
}
