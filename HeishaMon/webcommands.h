// "Commands" tab on the root page: send set topics from the browser.
// Included only by webfunctions.cpp (after htmlcode.h and commands.h), which provides the shared page parts.
// The pane sends the command in the background with fetch('/command?Name=Value'), the same handler as
// MQTT commands/Name, and shows the plain-text reply. The command names come from GET /commands.
#pragma once

static const char webCommandsTab[] FLASHPROG = R"====(
<button class='tabnav-btn' data-tab='Commands' onclick="openTable('Commands');cmdLoad()">Commands</button>
)====";

static const char webCommandsPane[] FLASHPROG = R"====(
<style>#Commands .setting-row:hover{background:none}</style>
<div id='Commands' class='tab-pane'>
<div class='panel'>
  <div class='panel-header'><h3>Send command</h3></div>
  <form id='cmdForm'>
    <div class='setting-row'><label class='setting-label' for='cmdName'>Command</label>
      <select id='cmdName' class='setting-input' required></select></div>
    <div class='setting-row'><label class='setting-label' for='cmdValue'>Value</label>
      <input id='cmdValue' class='setting-input' type='text' autocomplete='off' required></div>
    <div class='setting-row'><span></span><button class='btn btn-primary' type='submit' style='justify-self:start'>Send</button></div>
  </form>
</div>
<div class='panel' style='margin-top:16px'>
  <div class='panel-header'><h3>Responses</h3><span class='panel-meta'><button class='btn btn-ghost' type='button' id='cmdClear'>Clear</button></span></div>
  <pre id='cmdLog' role='status' aria-live='polite' style='padding:12px 16px;white-space:pre-wrap;min-height:48px'></pre>
</div></div>
<dialog id='cmdDialog' style='margin:auto;padding:20px;border-radius:8px;border:1px solid var(--border);background:var(--bg-surface);color:var(--text-primary)'>
  <p id='cmdQuestion' style='margin-bottom:16px'></p>
  <div style='display:flex;gap:8px;justify-content:flex-end'>
    <button class='btn btn-ghost' type='button' id='cmdCancel'>Cancel</button>
    <button class='btn btn-primary' type='button' id='cmdConfirm'>Send</button>
  </div>
</dialog>
<script>
var cmdLoaded=false;
function cmdLoad(){
  if(cmdLoaded)return;
  fetch('/commands').then(function(r){return r.text();}).then(function(t){
    var sel=document.getElementById('cmdName');
    t.split('\n').forEach(function(l){
      if(!l)return;
      var p=l.split(' ');
      var o=document.createElement('option');
      o.value=p[0];
      o.textContent=p[1]?p[0]+' ('+p[1]+')':p[0];
      sel.appendChild(o);
    });
    cmdLoaded=sel.options.length>0;
  });
}
(function(){
  var name=document.getElementById('cmdName');
  var value=document.getElementById('cmdValue');
  var log=document.getElementById('cmdLog');
  var dialog=document.getElementById('cmdDialog');
  function add(line){
    log.textContent=new Date().toLocaleTimeString()+'  '+line+'\n'+log.textContent;
  }
  function send(){
    var n=name.value,v=value.value;
    fetch('/command?'+encodeURIComponent(n)+'='+encodeURIComponent(v))
      .then(function(r){return r.text();})
      .then(function(t){
        t=t.trim();
        add(t?t.split('\n').join('\n          '):n+': no response (unknown command or optional PCB disabled)');
      })
      .catch(function(e){add(n+': request failed ('+e+')');});
  }
  name.addEventListener('change',function(){value.value='';});
  document.getElementById('cmdForm').addEventListener('submit',function(e){
    e.preventDefault();
    document.getElementById('cmdQuestion').textContent='Send '+name.value+' = '+value.value+' ?';
    dialog.showModal();
  });
  document.getElementById('cmdCancel').addEventListener('click',function(){dialog.close();});
  document.getElementById('cmdConfirm').addEventListener('click',function(){dialog.close();send();});
  document.getElementById('cmdClear').addEventListener('click',function(){log.textContent='';});
})();
</script>
)====";

/*
 * GET /commands: the settable topics as plain text, one per line ("Name" or "Name pcb|relay"), streamed from the commands
 * tables (one per callback). Relays are not in the tables (ESP32 only); optional PCB commands only
 * exist when it is enabled. Not available in listen only mode, where nothing can be sent.
 */
int handleCommands(struct webserver_t *client, settingsStruct *heishamonSettings) {
  if (heishamonSettings->listenonly) {
    if (client->content == 0) {
      webserver_send(client, 403, (char *)"text/plain", 9);
      webserver_send_content_P(client, PSTR("Forbidden"), 9);
    }
    return 0;
  }

  const unsigned int numCommands = sizeof(commands) / sizeof(commands[0]);
  const unsigned int numOptional = heishamonSettings->optionalPCB ? sizeof(optionalCommands) / sizeof(optionalCommands[0]) : 0;
#ifdef ESP32
  const unsigned int numRelays = 2;
#else
  const unsigned int numRelays = 0;
#endif
  const unsigned int total = numCommands + numOptional + numRelays;
  const unsigned int i = client->content;

  if (i == 0) {
    webserver_send(client, 200, (char *)"text/plain", 0);
  }
  if (i < total) {
    char name[29];
    const char *kind = "";  // shown after the name: pcb / relay
    if (i < numCommands) {
      cmdStruct tmp;
      memcpy_P(&tmp, &commands[i], sizeof(tmp));
      strlcpy(name, tmp.name, sizeof(name));
    } else if (i < numCommands + numOptional) {
      optCmdStruct tmp;
      memcpy_P(&tmp, &optionalCommands[i - numCommands], sizeof(tmp));
      strlcpy(name, tmp.name, sizeof(name));
      kind = " pcb";
    } else {
      strlcpy(name, (i == numCommands + numOptional) ? "SetRelayOne" : "SetRelayTwo", sizeof(name));
      kind = " relay";
    }
    char row[40];
    int len = snprintf(row, sizeof(row), "%s%s\n", name, kind);
    webserver_send_content(client, row, len);
  }
  return 0;
}
