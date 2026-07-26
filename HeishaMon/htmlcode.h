#if defined(ESP8266)
  #define FLASHPROG PROGMEM
#else
  #define FLASHPROG  // ESP32 ignores FLASHPROG, makes sure compiler uses the .rodata instead of .data for consts when confusing for PROGMEM
#endif

// ─────────────────────────────────────────────────────────────────────────────
// SHARED CSS
// ─────────────────────────────────────────────────────────────────────────────
static const char webCSS[] FLASHPROG = R"====(
<style>

/* ═══════════════════════════════════════════════════════════════════════
   LIGHT MODE (DEFAULT)
   ═══════════════════════════════════════════════════════════════════════ */
:root {
  --bg-base:#f8f9fa;
  --bg-surface:#ffffff;
  --bg-elevated:#f1f3f5;
  --bg-hover:#e9ecef;
  --border:#dee2e6;
  --border-focus:#3a7bd5;
  --text-primary:#212529;
  --text-secondary:#495057;
  --text-muted:#6c757d;
  --accent:#3a7bd5;
  --accent-glow:rgba(58,123,213,0.15);
  --accent-hover:#5a9be8;
  --red:#dc3545;
  --red-glow:rgba(220,53,69,0.15);
  --green:#28a745;
  --green-glow:rgba(40,167,69,0.15);
  --orange:#fd7e14;
  --radius:8px;
  --radius-sm:5px;
  --radius-lg:12px;
}

/* ═══════════════════════════════════════════════════════════════════════
   DARK MODE
   ═══════════════════════════════════════════════════════════════════════ */
html.dark-mode {
  --bg-base:#0f1117;
  --bg-surface:#161922;
  --bg-elevated:#1e2230;
  --bg-hover:#262b3a;
  --border:#2a3040;
  --border-focus:#3a7bd5;
  --text-primary:#eef0f4;
  --text-secondary:#7b8597;
  --text-muted:#7b8597;
  --accent:#3a7bd5;
  --accent-glow:rgba(58,123,213,0.25);
  --accent-hover:#5a9be8;
  --red:#e74c5e;
  --red-glow:rgba(231,76,94,0.25);
  --green:#2ecc94;
  --green-glow:rgba(46,204,148,0.2);
  --orange:#f0a500;
}

/* ═══════════════════════════════════════════════════════════════════════
   BASE STYLES (remain unchanged)
   ═══════════════════════════════════════════════════════════════════════ */
*{box-sizing:border-box;margin:0;padding:0}
html{font-size:15px;-webkit-text-size-adjust:100%}
body{
  font-family:'Sora',sans-serif;
  background:var(--bg-base);
  color:var(--text-primary);
  min-height:100vh;
  line-height:1.5;
  transition:background 0.3s, color 0.3s;
}

.topbar{
  display:flex;
  align-items:center;
  justify-content:space-between;
  background:var(--bg-surface);
  border-bottom:1px solid var(--border);
  padding:0 24px;
  height:56px;
  position:sticky;
  top:0;z-index:100;
}
.topbar-left{display:flex;align-items:center;gap:16px}
.topbar-logo{
  font-family:'JetBrains Mono',monospace;
  font-size:18px;font-weight:500;
  color:var(--accent);
  letter-spacing:-0.5px;
  text-decoration:none;
}
.topbar-logo span{color:var(--text-secondary);font-weight:400}
.hamburger{
  background:none;border:none;
  color:var(--text-secondary);
  font-size:20px;cursor:pointer;
  padding:6px;border-radius:var(--radius-sm);
  transition:background .2s,color .2s;
}
.hamburger:hover{background:var(--bg-elevated);color:var(--text-primary)}
.sidemenu{
  position:fixed;top:0;left:-240px;width:240px;height:100%;
  background:var(--bg-surface);
  border-right:1px solid var(--border);
  z-index:200;
  transition:left .3s cubic-bezier(.4,0,.2,1);
  display:flex;flex-direction:column;
  overflow-y:auto;
}
.sidemenu.open{left:0}
.sidemenu-overlay{
  display:none;position:fixed;inset:0;
  background:rgba(0,0,0,.45);z-index:199;
}
.sidemenu-overlay.open{display:block}
.sidemenu-header{
  padding:24px 20px 16px;
  border-bottom:1px solid var(--border);
}
.sidemenu-header h2{
  font-family:'JetBrains Mono',monospace;
  font-size:15px;color:var(--accent);font-weight:500;
}
.sidemenu-header p{font-size:11px;color:var(--text-muted);margin-top:2px}
.sidemenu-nav{padding:8px 12px;flex:1}
.sidemenu-nav a{
  display:flex;align-items:center;gap:10px;
  padding:10px 12px;
  color:var(--text-secondary);
  text-decoration:none;border-radius:var(--radius);
  font-size:13px;font-weight:400;
  transition:background .18s,color .18s;
  margin-bottom:2px;
}
.sidemenu-nav a:hover{background:var(--bg-elevated);color:var(--text-primary)}
.sidemenu-nav a.danger{color:var(--red)}
.sidemenu-nav a.danger:hover{background:var(--red-glow)}
.sidemenu-nav .nav-icon{width:16px;text-align:center;opacity:.7}
.sidemenu-footer{
  padding:16px 20px;
  border-top:1px solid var(--border);
  font-size:11px;color:var(--text-muted);
}
.sidemenu-footer a{color:var(--accent);text-decoration:none}
.sidemenu-footer a:hover{text-decoration:underline}
.tabnav{
  display:flex;gap:4px;
  padding:12px 24px 0;
  background:var(--bg-base);
  flex-wrap:wrap;
}
.tabnav button{
  background:none;border:none;
  color:var(--text-muted);
  font-family:'Sora',sans-serif;
  font-size:14px;font-weight:500;
  padding:10px 20px;
  border-radius:var(--radius-sm) var(--radius-sm) 0 0;
  cursor:pointer;
  transition:color .2s,background .2s;
  letter-spacing:.3px;text-transform:uppercase;
  position:relative;
}
.tabnav button::after{
  content:'';position:absolute;
  bottom:0;left:16px;right:16px;height:2px;
  background:transparent;
  border-radius:1px;
  transition:background .25s;
}
.tabnav button:hover{color:var(--text-secondary);background:var(--bg-elevated)}
.tabnav button.active{color:var(--accent)}
.tabnav button.active::after{background:var(--accent)}
.main-content{padding:20px 24px 40px}
.statusbar{
  display:flex;flex-wrap:wrap;gap:12px;
  margin-bottom:20px;
}
.status-chip{
  display:flex;align-items:center;gap:8px;
  background:var(--bg-surface);
  border:1px solid var(--border);
  border-radius:20px;
  padding:6px 14px;
  font-size:12px;color:var(--text-secondary);
}
.status-chip .chip-label{color:var(--text-muted);font-size:10.5px;text-transform:uppercase;letter-spacing:.5px}
.status-chip .chip-value{color:var(--text-primary);font-family:'JetBrains Mono',monospace;font-size:12px;font-weight:500}
.status-chip.listen-only{border-color:var(--orange);background:rgba(240,165,0,.08)}
.status-chip.listen-only .chip-value{color:var(--orange)}
.status-chip.rules-active {background:linear-gradient(135deg,rgba(33,150,243,0.08),rgba(33,150,243,0.12));border-color:rgba(33,150,243,0.2);}
.status-chip.rules-active .chip-value {color:#2196F3;font-weight:600;}
.status-chip.rules-inactive {background:linear-gradient(135deg,rgba(158,158,158,0.05),rgba(158,158,158,0.08));border-color:rgba(158,158,158,0.15);}
.status-chip.rules-inactive .chip-value {color:#9e9e9e;font-weight:500;}
.status-dot{width:8px;height:8px;border-radius:50%;margin-right:8px;transition:background 0.3s;}
.status-dot.excellent{background:#2ecc94;}
.status-dot.good{background:#3a7bd5;}
.status-dot.fair{background:#f0a500;}
.status-dot.poor{background:#e74c5e;}
.status-dot.disconnected{background:#6b7280;}
.panel{
  background:var(--bg-surface);
  border:1px solid var(--border);
  border-radius:var(--radius-lg);
  overflow:hidden;
}
.panel-header{
  padding:14px 20px;
  border-bottom:1px solid var(--border);
  display:flex;align-items:center;justify-content:space-between;
}
.panel-header h3{font-size:13px;font-weight:500;color:var(--text-primary);letter-spacing:.2px}
.panel-header .panel-meta{font-size:11px;color:var(--text-muted)}
table{
  width:100%;border-collapse:collapse;
  font-size:12.5px;
}
thead th{
  text-align:left;
  padding:10px 16px;
  color:var(--text-muted);
  font-size:10.5px;font-weight:600;
  text-transform:uppercase;letter-spacing:.7px;
  background:var(--bg-base);
  border-bottom:1px solid var(--border);
  position:sticky;top:56px;
}
tbody tr{
  border-bottom:1px solid rgba(42,48,64,.5);
  transition:background .15s;
}
tbody tr:hover{background:var(--bg-elevated)}
tbody tr:last-child{border-bottom:none}
tbody td{
  padding:9px 16px;
  color:var(--text-secondary);
}
tbody td:first-child{color:var(--text-primary);font-family:'JetBrains Mono',monospace;font-size:11.5px}
.update-effect{
  animation:flash-update 1.2s ease-out;
}
@keyframes flash-update {
  0% {color: var(--accent);text-shadow: 0 0 10px var(--accent-glow), 0 0 24px var(--accent-glow);transform: scale(1.08);}
  50% {transform: scale(1);}:
  100% {color: var(--text-secondary);text-shadow: none;}
}
#cli{
  width:100%;
  background:#0a0c0f;
  color:#6ee7b7;
  border:1px solid var(--border);
  border-radius:var(--radius);
  padding:14px 16px;
  font-family:'JetBrains Mono',monospace;
  font-size:11.5px;
  resize:none;  /* Changed from vertical to none since we're auto-sizing */
  outline:none;
  line-height:1.6;
}

#cli:focus{border-color:var(--border-focus);box-shadow:0 0 0 3px var(--accent-glow)}

.console-toggle-compact {
  display:flex;
  align-items:center;
  gap:6px;
}
.console-toggle-label-compact {
  font-size:11px;
  color:var(--text-muted);
  font-weight:400;
  text-transform:uppercase;
  letter-spacing:0.3px;
}
.theme-switch-compact {
  position:relative;
  width:32px;
  height:18px;
}
.theme-switch-compact input {
  opacity:0;
  width:0;
  height:0;
}
.theme-slider-compact {
  position:absolute;
  cursor:pointer;
  top:0;
  left:0;
  right:0;
  bottom:0;
  background:var(--border);
  transition:0.3s;
  border-radius:18px;
}
.theme-slider-compact:before {
  position:absolute;
  content:"";
  height:14px;
  width:14px;
  left:2px;
  bottom:2px;
  background:white;
  transition:0.3s;
  border-radius:50%;
}
input:checked + .theme-slider-compact {
  background:var(--accent);
}
input:checked + .theme-slider-compact:before {
  transform:translateX(14px);
}
input:disabled + .theme-slider-compact {
  opacity:0.4;
  cursor:not-allowed;
}

.alias-edit{
  outline:none;
  border:1px solid transparent;
  border-radius:var(--radius-sm);
  padding:3px 6px;
  min-width:80px;
  font-size:12.5px;
  color:var(--text-secondary);
  background:transparent;
  transition:border-color .2s,background .2s;
}
.alias-edit:hover,.alias-edit:focus{
  border-color:var(--border);
  background:var(--bg-elevated);
}
.alias-edit:focus{border-color:var(--border-focus);box-shadow:0 0 0 2px var(--accent-glow)}
.tab-pane{display:none}
.tab-pane.active{display:block}
.settings-grid{
  display:grid;
  gap:0;
}
.setting-row{
  display:grid;
  grid-template-columns:280px 1fr;
  align-items:center;
  padding:12px 20px;
  border-bottom:1px solid rgba(42,48,64,.4);
  gap:16px;
}
@media(max-width:480px){
  .setting-row{grid-template-columns:1fr}
  .setting-label{text-align:left}
}
.setting-row:last-child{border-bottom:none}
.setting-row:hover{background:rgba(30,34,48,.4)}
.setting-label{
  font-size:12.5px;
  color:var(--text-secondary);
  text-align:right;
  font-weight:400;
}
.setting-input{
  background:var(--bg-base);
  border:1px solid var(--border);
  color:var(--text-primary);
  font-family:'Sora',sans-serif;
  font-size:12.5px;
  padding:7px 10px;
  border-radius:var(--radius-sm);
  outline:none;
  transition:border-color .2s,box-shadow .2s;
  width:100%;max-width:320px;
}
.setting-input:focus{border-color:var(--border-focus);box-shadow:0 0 0 3px var(--accent-glow)}
.setting-input[type=password]{letter-spacing:2px}
select.setting-input{appearance:auto}
.setting-hint{font-size:11px;color:var(--text-muted);margin-left:8px}
.setting-row .checkbox-wrap{display:flex;align-items:center;gap:8px}
.checkbox-wrap input[type=checkbox]{width:17px;height:17px;accent-color:var(--accent);cursor:pointer}
.radio-group{display:flex;gap:16px;flex-wrap:wrap}
.radio-group label{display:flex;align-items:center;gap:6px;font-size:12px;color:var(--text-secondary);cursor:pointer}
.radio-group input[type=radio]{accent-color:var(--accent)}
.section-divider{
  padding:10px 20px 4px;
  font-size:10.5px;font-weight:600;
  color:var(--text-muted);
  text-transform:uppercase;letter-spacing:.8px;
  background:var(--bg-base);
}
.btn{
  display:inline-flex;align-items:center;justify-content:center;
  padding:8px 20px;
  border-radius:var(--radius-sm);
  font-family:'Sora',sans-serif;
  font-size:12.5px;font-weight:500;
  cursor:pointer;border:none;
  transition:background .2s,box-shadow .2s,transform .1s;
  text-decoration:none;
  letter-spacing:.3px;
}
.btn:active{transform:scale(.97)}
.btn-primary{background:var(--accent);color:#fff}
.btn-primary:hover{background:var(--accent-hover);box-shadow:0 4px 14px var(--accent-glow)}
.btn-danger{background:var(--red);color:#fff}
.btn-danger:hover{background:#d63a4d;box-shadow:0 4px 14px var(--red-glow)}
.btn-ghost{
  background:transparent;
  color:var(--text-secondary);
  border:1px solid var(--border);
}
.btn-ghost:hover{background:var(--bg-elevated);color:var(--text-primary)}
.btn:disabled{opacity:.4;cursor:not-allowed;transform:none}
.form-actions{padding:20px;display:flex;gap:12px;align-items:center}
.loading-overlay{
  display:flex;align-items:center;justify-content:center;
  padding:60px 20px;
  color:var(--text-muted);
  font-size:13px;
}
.spinner{
  width:20px;height:20px;
  border:2px solid var(--border);
  border-top-color:var(--accent);
  border-radius:50%;
  animation:spin .7s linear infinite;
  margin-right:10px;
}
@keyframes spin{to{transform:rotate(360deg)}}
.firmware-container{max-width:620px;margin:0 auto;padding:40px 24px}
.firmware-warning{
  background:rgba(231,76,94,.08);
  border:1px solid rgba(231,76,94,.25);
  border-radius:var(--radius);
  padding:14px 18px;
  font-size:12px;
  color:var(--red);
  margin-top:20px;line-height:1.6;
}
.firmware-warning strong{color:var(--text-primary)}
.firmware-info{
  background:rgba(58,123,213,.08);
  border:1px solid rgba(58,123,213,.25);
  border-radius:var(--radius);
  padding:16px 20px;
  font-size:12.5px;
  color:var(--text-secondary);
  margin-bottom:20px;line-height:1.8;
}
.firmware-info strong{color:var(--text-primary)}
.firmware-info a{color:var(--accent)}
progress{
  width:100%;height:6px;
  appearance:none;
  border-radius:3px;
  background:var(--bg-elevated);
  margin-top:12px;
  overflow:hidden;
}
progress::-webkit-progress-bar{background:var(--bg-elevated);border-radius:3px}
progress::-webkit-progress-value{background:var(--accent);border-radius:3px;transition:width .2s}
#status{font-size:12px;color:var(--text-muted);margin-top:8px;min-height:18px}
.file-input-wrap{position:relative;margin-top:12px}
.file-input-wrap input[type=file]{
  font-size:12px;color:var(--text-secondary);
  font-family:'Sora',sans-serif;
}
.rules-editor{background:#0f1117;color:#e4e7eb;padding:12px;border:1px solid #2d3748;border-radius:6px;font-family:'JetBrains Mono',monospace;font-size:14px;line-height:1.5;min-height:400px;white-space:pre;overflow:auto;}
.rules-editor:focus{outline:none;border-color:#3a7bd5;}
.keyword{color:#c792ea;}
.operator{color:#89ddff;}
.number{color:#f78c6c;}
.string{color:#c3e88d;}
.comment{color:#546e7a;font-style:italic;}
.variable{color:#82aaff;}
.function{color:#ffcb6b;}
.at-param{color:#f07178;}
.percent-param{color:#c3e88d;}
.question-param{color:#89ddff;}
.ds18b20{color:#ff5370;}
.rules-error{border:2px solid #e74c5e!important;box-shadow:0 0 0 3px rgba(231,76,94,0.2)!important;}
.rules-valid{border:2px solid #2ecc94!important;box-shadow:0 0 0 3px rgba(46,204,148,0.2)!important;}
.validation-feedback{margin-top:8px;padding:10px 14px;border-radius:6px;font-size:13px;line-height:1.4;}
.validation-feedback.error{background:rgba(231,76,94,0.1);border:1px solid #e74c5e;color:#ff9999;}
.validation-feedback.success{background:rgba(46,204,148,0.1);border:1px solid #2ecc94;color:#6ee7b7;}
.validation-feedback.warning{background:rgba(240,165,0,0.1);border:1px solid #f0a500;color:#ffd54f;}
.line-numbers{background:#0a0c10;color:#4a5568;padding:12px 8px;border:1px solid #2d3748;border-right:none;border-radius:6px 0 0 6px;font-family:'JetBrains Mono',monospace;font-size:14px;line-height:1.5;text-align:right;user-select:none;min-width:40px;white-space:pre;}
.rules-editor{flex:1;background:#0f1117;color:#e4e7eb;padding:12px;border:1px solid #2d3748;border-radius:0 6px 6px 0;font-family:'JetBrains Mono',monospace;font-size:14px;line-height:1.5;min-height:400px;white-space:pre;overflow:auto;}
.msg-box{
  max-width:520px;margin:80px auto;text-align:center;
  padding:48px 32px;
  background:var(--bg-surface);
  border:1px solid var(--border);
  border-radius:var(--radius-lg);
}
.msg-box h2{font-size:18px;color:var(--text-primary);margin-bottom:10px;font-weight:500}
.msg-box p{font-size:13px;color:var(--text-muted);line-height:1.7}
.msg-box.warning h2{color:var(--orange)}
.msg-box.danger h2{color:var(--red)}
.msg-box.success h2{color:var(--green)}
@media(max-width:600px){
  .setting-row{grid-template-columns:1fr;gap:4px}
  .setting-label{text-align:left}
  .tabnav{padding:10px 12px 0;gap:2px}
  .tabnav button{padding:9px 14px;font-size:13px}
  .main-content{padding:16px 12px 32px}
  .topbar{padding:0 12px}
  thead th,tbody td{padding:8px 10px;font-size:11.5px}
  .panel-header {
    flex-direction:column;
    align-items:flex-start;
    gap:8px;
  }
  .panel-header > div {
    flex-wrap:wrap;
  }  
}
select#wifi_ssid_select{
  display:none;
  background:var(--bg-base);
  border:1px solid var(--border);
  color:var(--text-primary);
  font-family:'Sora',sans-serif;
  font-size:12px;
  padding:7px 10px;
  border-radius:var(--radius-sm);
  max-width:320px;width:100%;
}

/* ═══════════════════════════════════════════════════════════════════════
   DARK MODE TOGGLE SWITCH
   ═══════════════════════════════════════════════════════════════════════ */
.theme-toggle {
  display:flex;
  align-items:center;
  justify-content:space-between;
  padding:12px 12px;
  margin:8px 12px;
  background:var(--bg-elevated);
  border-radius:var(--radius);
  border:1px solid var(--border);
}
.theme-toggle-label {
  font-size:13px;
  color:var(--text-secondary);
  font-weight:400;
  display:flex;
  align-items:center;
  gap:8px;
}
.theme-toggle-label .nav-icon {
  width:16px;
  text-align:center;
  opacity:0.7;
}
.theme-switch {
  position:relative;
  width:44px;
  height:24px;
}
.theme-switch input {
  opacity:0;
  width:0;
  height:0;
}
.theme-slider {
  position:absolute;
  cursor:pointer;
  top:0;
  left:0;
  right:0;
  bottom:0;
  background:var(--border);
  transition:0.3s;
  border-radius:24px;
}
.theme-slider:before {
  position:absolute;
  content:"";
  height:18px;
  width:18px;
  left:3px;
  bottom:3px;
  background:white;
  transition:0.3s;
  border-radius:50%;
}
input:checked + .theme-slider {
  background:var(--accent);
}
input:checked + .theme-slider:before {
  transform:translateX(20px);
}



/* ═══════════════════════════════════════════════════════════════════════
   DARK MODE SPECIFIC OVERRIDES
   ═══════════════════════════════════════════════════════════════════════ */
html.dark-mode #cli {
  background:#0a0c0f;
  color:#6ee7b7;
}

html.dark-mode .rules-editor {
  background:#0f1117;
  color:#e4e7eb;
  border-color:#2d3748;
}

html.dark-mode .line-numbers {
  background:#0a0c10;
  color:#4a5568;
  border-color:#2d3748;
}

</style>
)====";


// ─────────────────────────────────────────────────────────────────────────────
// HTML HEAD
// ─────────────────────────────────────────────────────────────────────────────
static const char webHeader[] FLASHPROG = R"====(
<!DOCTYPE html>
<html lang='en'>
<head>
<meta charset='utf-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Heisha Monitor</title>
<script>
(function(){
  function getCookie(name){
    var nameEQ=name+"=";
    var ca=document.cookie.split(';');
    for(var i=0;i<ca.length;i++){
      var c=ca[i];
      while(c.charAt(0)==' ')c=c.substring(1,c.length);
      if(c.indexOf(nameEQ)==0)return c.substring(nameEQ.length,c.length);
    }
    return null;
  }
  var darkMode=getCookie('darkMode');
  var wantDark = darkMode==='true' || (darkMode===null && window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches);
  if(wantDark){
    document.documentElement.classList.add('dark-mode-loading');
  }
})();
</script>
<style>
html.dark-mode-loading {
  background:#0f1117 !important;
  color:#eef0f4 !important;
}
html.dark-mode-loading body {
  background:#0f1117 !important;
  color:#eef0f4 !important;
  transition:none !important;
}
html.dark-mode-loading .topbar,
html.dark-mode-loading .panel,
html.dark-mode-loading .sidemenu,
html.dark-mode-loading .status-chip,
html.dark-mode-loading .msg-box {
  background:#161922 !important;
  border-color:#2a3040 !important;
}
html.dark-mode-loading .theme-toggle {
  background:#1e2230 !important;
  border-color:#2a3040 !important;
}
html.dark-mode-loading .tabnav {
  background:#0f1117 !important;
}
html.dark-mode-loading .setting-input,
html.dark-mode-loading input[type=text],
html.dark-mode-loading input[type=number],
html.dark-mode-loading input[type=password],
html.dark-mode-loading select {
  background:#0f1117 !important;
  color:#eef0f4 !important;
  border-color:#2a3040 !important;
}
html.dark-mode-loading thead th {
  background:#0f1117 !important;
  color:#7b8597 !important;
  border-color:#2a3040 !important;
}
html.dark-mode-loading progress {
  background:#1e2230 !important;
}
html.dark-mode-loading progress::-webkit-progress-bar {
  background:#1e2230 !important;
}
html.dark-mode-loading progress::-webkit-progress-value {
  background:#3a7bd5 !important;
}
html.dark-mode-loading .panel-header {
  border-color:#2a3040 !important;
}
</style>
)====";

static const char refreshMeta[] FLASHPROG = R"====(
<meta http-equiv='refresh' content='5; url=/' />
)====";

// ─────────────────────────────────────────────────────────────────────────────
// BODY START (shared top bar structure — version injected server-side)
// ─────────────────────────────────────────────────────────────────────────────
static const char webBodyStart[] FLASHPROG = R"====(
</head>
<body>
<div class='sidemenu-overlay' id='menuOverlay' onclick='closeMenu()'></div>
<aside class='sidemenu' id='sideMenu'>
  <div class='sidemenu-header'>
    <h2>HeishaMon</h2>
    <p id='sideVersion'>)====" HEISHAMON_VERSION R"====(</p>
  </div>
  
  <!-- DARK MODE TOGGLE -->
  <div class='theme-toggle'>
    <label class='theme-toggle-label'>
      <span class='nav-icon'>☀</span>
      Dark Mode
    </label>
    <label class='theme-switch'>
      <input type='checkbox' id='darkModeToggle' onchange='toggleDarkMode()'>
      <span class='theme-slider'></span>
    </label>
  </div>
  
  <nav class='sidemenu-nav' id='sideNav'></nav>
  <div class='sidemenu-footer'>
    <a href='https://github.com/heishamon/HeishaMon' target='_blank'>GitHub</a>
  </div>
</aside>
<header class='topbar'>
  <div class='topbar-left'>
    <button class='hamburger' onclick='toggleMenu()'>&#9776;</button>
    <a class='topbar-logo' href='/'>Heisha<span>Mon</span></a>
  </div>
  <div class='topbar-right'></div>
</header>
)====";

static const char webFooter[] FLASHPROG = "</body></html>";

// ─────────────────────────────────────────────────────────────────────────────
// MENU & WEBSOCKET JS (shared across pages)
// ─────────────────────────────────────────────────────────────────────────────
static const char menuJS[] FLASHPROG = R"====(
<script>
function toggleMenu(){
  var m=document.getElementById('sideMenu');
  var o=document.getElementById('menuOverlay');
  m.classList.toggle('open');
  o.classList.toggle('open');
}
function closeMenu(){
  document.getElementById('sideMenu').classList.remove('open');
  document.getElementById('menuOverlay').classList.remove('open');
}

function setCookie(name, value, days) {
  var expires = "";
  if (days) {
    var date = new Date();
    date.setTime(date.getTime() + (days * 24 * 60 * 60 * 1000));
    expires = "; expires=" + date.toUTCString();
  }
  document.cookie = name + "=" + (value || "") + expires + "; path=/";
}

function getCookie(name) {
  var nameEQ = name + "=";
  var ca = document.cookie.split(';');
  for(var i = 0; i < ca.length; i++) {
    var c = ca[i];
    while (c.charAt(0) == ' ') c = c.substring(1, c.length);
    if (c.indexOf(nameEQ) == 0) return c.substring(nameEQ.length, c.length);
  }
  return null;
}

function toggleDarkMode() {
  var toggle = document.getElementById('darkModeToggle');
  var html = document.documentElement;
  
  if (toggle.checked) {
    html.classList.add('dark-mode');
    setCookie('darkMode', 'true', 365);
  } else {
    html.classList.remove('dark-mode');
    setCookie('darkMode', 'false', 365);
  }
}

function initDarkMode() {
  var darkMode = getCookie('darkMode');
  var toggle = document.getElementById('darkModeToggle');
  var html = document.documentElement;

  // Remove the temporary loading class
  html.classList.remove('dark-mode-loading');

  // No explicit preference saved yet: follow the OS/browser setting
  var useDark;
  if (darkMode === 'true') {
    useDark = true;
  } else if (darkMode === 'false') {
    useDark = false;
  } else {
    useDark = !!(window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches);
  }

  if (useDark) {
    html.classList.add('dark-mode');
    if (toggle) toggle.checked = true;
  } else {
    html.classList.remove('dark-mode');
    if (toggle) toggle.checked = false;
  }
}

// Initialize immediately when DOM is ready
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', initDarkMode);
} else {
  initDarkMode();
}
</script>
)====";

static const char websocketJS[] FLASHPROG = R"====(
<script>
var bConnected=false;
var inactivityTimeout=5000;
var lastActivityTime=Date.now();
function monitorWebSocket(){
  setInterval(function(){
    if(Date.now()-lastActivityTime>inactivityTimeout&&oWebsocket.readyState===WebSocket.OPEN){
      console.log('Inactivity detected, reconnecting...');
      oWebsocket.close();
    }
  },inactivityTimeout);
}
function attemptReconnect(){
  if(!bConnected){console.log('Reconnecting...');startWebsockets();}
}
function startWebsockets(){
  if(typeof MozWebSocket!='undefined'){
    oWebsocket=new MozWebSocket('ws://'+location.host);
  } else if(typeof WebSocket!='undefined'){
    oWebsocket=new WebSocket('ws://'+location.host+'/ws');
  }
  if(oWebsocket){
    oWebsocket.onopen=function(){bConnected=true;};
    oWebsocket.onclose=function(){bConnected=false;attemptReconnect();};
    oWebsocket.onerror=function(e){console.log('WS error:',e);};
    oWebsocket.onmessage=function(evt){
      lastActivityTime=Date.now();
      if(evt.data.startsWith('{')){
        var j=JSON.parse(evt.data);
        if(j.logMsg!=null){
          var obj=document.getElementById('cli');
          if(!obj)return;
          var chk=document.getElementById('autoscroll');
          obj.value+=j.logMsg+'\n';
          if(chk&&chk.checked)obj.scrollTop=obj.scrollHeight;
        } else if(j.data){
          if(j.data.stats){
            updStat('wifi',j.data.stats.wifi);
            updStat('ethernet',j.data.stats.ethernet);
            updStat('memory',j.data.stats.memory);
            updStat('correct',j.data.stats.correct);
            updStat('mqtt',j.data.stats.mqtt);
            updStat('uptime',j.data.stats.uptime);
            updStat('rules',j.data.stats.rules);
          } else if(j.data.heishavalues){
            updCell(j.data.heishavalues.topic+'-Value',j.data.heishavalues.value);
            updCell(j.data.heishavalues.topic+'-Description',j.data.heishavalues.description);
          } else if(j.data.dallasvalues){
            var dID=j.data.dallasvalues.sensorID;
            if(j.data.dallasvalues.value!==undefined)updCell('SensorID-'+dID+'-Temperature',j.data.dallasvalues.value);
            if(j.data.dallasvalues.present!==undefined)updDallasPresence(dID,j.data.dallasvalues.present);
          } else if(j.data.s0values){
            updCell('s0port-'+j.data.s0values.s0port+'-Watt',j.data.s0values.Watt);
            updCell('s0port-'+j.data.s0values.s0port+'-Watthour',j.data.s0values.Watthour);
            updCell('s0port-'+j.data.s0values.s0port+'-WatthourTotal',j.data.s0values.WatthourTotal);
          } else if(j.data.opentherm){
            updCell(j.data.opentherm.name+'-value',j.data.opentherm.value);
          } else if(j.data.dallasRescan){
            refreshDallasTable();
          }
        }
      } else {
        var obj=document.getElementById('cli');
        if(!obj)return;
        var chk=document.getElementById('autoscroll');
        obj.value+=evt.data+'\n';
        if(chk&&chk.checked)obj.scrollTop=obj.scrollHeight;
      }
    };
  }
}
function updStat(id,val){
  var el=document.getElementById(id);
  if(el){
    // Special handling for WiFi disconnected state
    if(id === 'wifi' && (val === -1 || val === '-1' || parseInt(val) < 0)){
      el.textContent = 'not connected';
      // Remove the % symbol that follows
      var percentSpan = el.nextElementSibling;
      if(percentSpan && percentSpan.textContent === '%'){
        percentSpan.style.display = 'none';
      }
    } else {
      el.textContent = val != null ? val : '';
      // Show % symbol again if it was hidden
      var percentSpan = el.nextElementSibling;
      if(percentSpan && percentSpan.textContent === '%'){
        percentSpan.style.display = '';
      }
    }
  }
  
  if ((el) && (id == 'wifi') && (val!==undefined)){
    var w=parseInt(val);
    var label=el.previousElementSibling;
    var dot=label.previousElementSibling;
    if(dot&&dot.classList.contains('status-dot')){
      dot.className='status-dot';
      if(w===-1||w<0)dot.classList.add('disconnected');
      else if(w>=75)dot.classList.add('excellent');
      else if(w>=50)dot.classList.add('good');
      else if(w>=25)dot.classList.add('fair');
      else dot.classList.add('poor');
    }
  }

  if (id === 'rules') {
    var chip = document.getElementById('rulesChip');
    if (chip) {
      var count = parseInt(val);
      var valueEl = chip.querySelector('.chip-value');
      if (count > 0) {
        chip.className = 'status-chip rules-active';
        valueEl.textContent = 'ACTIVE';
      } else {
        chip.className = 'status-chip rules-inactive';
        valueEl.textContent = 'INACTIVE';
      }
    }
    return;
  }
}

function updCell(id,val){
  var el=document.getElementById(id);
  if(el&&el.textContent!==val){
    el.classList.remove('update-effect');
    void el.offsetWidth;
    el.textContent=val;
    el.classList.add('update-effect');
  }
}

function updDallasPresence(sID,present){
  var statusCell=document.getElementById('SensorID-'+sID+'-Status');
  if(!statusCell)return;
  var row=statusCell.parentElement;
  if(row)row.style.opacity=present?'':'0.6';
  statusCell.style.color=present?'':'var(--danger,#f44336)';
  if(present){
    delete statusCell.dataset.offlineSince;
    statusCell.classList.remove('offline-duration');
    statusCell.textContent='OK';
  } else {
    statusCell.dataset.offlineSince=Date.now();
    statusCell.classList.add('offline-duration');
    statusCell.textContent=formatOfflineDuration(false,0);
  }
}

function formatOfflineDuration(present,lastSeenSeconds){
  if(present)return'OK';
  if(lastSeenSeconds==null||lastSeenSeconds<0)return'Offline (never seen)';
  var s=lastSeenSeconds;
  if(s<60)return'Offline for '+s+'s';
  if(s<3600)return'Offline for '+Math.floor(s/60)+'m';
  if(s<86400)return'Offline for '+Math.floor(s/3600)+'h';
  return'Offline for '+Math.floor(s/86400)+'d';
}

setInterval(function(){
  document.querySelectorAll('.offline-duration[data-offline-since]').forEach(function(cell){
    var seconds=Math.floor((Date.now()-cell.dataset.offlineSince)/1000);
    cell.textContent=formatOfflineDuration(false,seconds);
  });
},15000);
</script>
)====";

// ─────────────────────────────────────────────────────────────────────────────
// ROOT PAGE: tab switching + data refresh JS
// ─────────────────────────────────────────────────────────────────────────────
static const char selectJS[] FLASHPROG = R"====(
<script>
function openTable(name){
  var panes=document.getElementsByClassName('tab-pane');
  for(var i=0;i<panes.length;i++)panes[i].classList.remove('active');
  var target=document.getElementById(name);
  if(target)target.classList.add('active');
  var tabs=document.querySelectorAll('.tabnav button');
  tabs.forEach(function(b){b.classList.toggle('active',b.dataset.tab===name);});
}
</script>
)====";

static const char refreshJS[] FLASHPROG = R"====(
<script>
var isEditing=false;
document.body.onload=function(){
  openTable('Heatpump');
  document.getElementById('cli').value='';
  startWebsockets();
  monitorWebSocket();
  refreshTable();
};
var dallasAliasEdit=function(){
  isEditing=false;
  var addr=this.getAttribute('data-address');
  var alias=this.innerText.substring(0,30);
  var xhr=new XMLHttpRequest();
  xhr.open('GET','/dallasalias?'+addr+'='+alias,true);
  xhr.send();
};
function rescanDallas(){
  var xhr=new XMLHttpRequest();
  xhr.open('GET','/scandallas',true);
  xhr.onload=function(){refreshTable();};
  xhr.send();
}
function removeDallas(addr){
  if(!confirm('Remove sensor '+addr+'? This also clears its retained mqtt value.'))return;
  var xhr=new XMLHttpRequest();
  xhr.open('GET','/removedallas?'+addr+'=1',true);
  xhr.onload=function(){refreshTable();};
  xhr.send();
}
function renderDallasTable(d){
  if(!(d&&d['1wire']&&Array.isArray(d['1wire'])))return;
  var tb=document.getElementById('dallasvalues');tb.innerHTML='';
  d['1wire'].forEach(function(item){
    var row=document.createElement('tr');
    var sID=item['Sensor'];
    if(!item.Present)row.style.opacity='0.6';
    ['Sensor','Temperature','Alias'].forEach(function(k){
      var cell=document.createElement('td');
      cell.id='SensorID-'+sID+'-'+k;
      if(k==='Alias'){
        var div=document.createElement('div');
        div.textContent=item[k];
        div.classList.add('alias-edit');
        div.contentEditable='true';
        div.setAttribute('data-address',item.Sensor);
        div.addEventListener('focus',function(){isEditing=true;});
        div.addEventListener('blur',dallasAliasEdit);
        cell.appendChild(div);
      } else {cell.textContent=item[k];}
      row.appendChild(cell);
    });
    var statusCell=document.createElement('td');
    statusCell.id='SensorID-'+sID+'-Status';
    if(!item.Present){
      statusCell.style.color='var(--danger,#f44336)';
      statusCell.classList.add('offline-duration');
      if(item.LastSeenSeconds>=0)statusCell.dataset.offlineSince=Date.now()-item.LastSeenSeconds*1000;
    }
    statusCell.textContent=formatOfflineDuration(item.Present,item.LastSeenSeconds);
    row.appendChild(statusCell);
    var actionCell=document.createElement('td');
    var removeBtn=document.createElement('button');
    removeBtn.textContent='Remove';
    removeBtn.className='btn btn-ghost';
    removeBtn.style.cssText='padding:2px 8px;font-size:11px;height:22px;';
    removeBtn.onclick=function(){removeDallas(sID);};
    actionCell.appendChild(removeBtn);
    row.appendChild(actionCell);
    tb.appendChild(row);
  });
}
async function refreshDallasTable(){
  try{
    if(isEditing)return;
    var res=await fetch('/json');
    var d=await res.json();
    renderDallasTable(d);
  }catch(e){}
}
async function refreshTable(){
  try{
    if(isEditing)return;
    var res=await fetch('/json');
    var d=await res.json();
    if(d&&d.heatpump&&Array.isArray(d.heatpump)){
      var tb=document.getElementById('heishavalues');tb.innerHTML='';
      d.heatpump.forEach(function(item){tb.appendChild(buildRow(item,'Topic'));});
    }
    if(d&&d['heatpump extra']&&Array.isArray(d['heatpump extra'])){
      var tb=document.getElementById('heishavalues');
      d['heatpump extra'].forEach(function(item){tb.appendChild(buildRow(item,'Topic'));});
    }
    if(d&&d['heatpump optional']&&Array.isArray(d['heatpump optional'])){
      var tb=document.getElementById('heishavalues');
      d['heatpump optional'].forEach(function(item){tb.appendChild(buildRow(item,'Topic'));});
    }
    renderDallasTable(d);
    if(d&&d.s0&&Array.isArray(d.s0)){
      var tb=document.getElementById('s0values');tb.innerHTML='';
      d.s0.forEach(function(item){
        var row=document.createElement('tr');
        var port=item['S0 port'];
        for(var k in item){if(Object.hasOwn(item,k)){
          var cell=document.createElement('td');
          cell.id='s0port-'+port+'-'+k;
          cell.textContent=item[k];
          row.appendChild(cell);
        }}
        tb.appendChild(row);
      });
    }
    if(d&&d.opentherm&&typeof d.opentherm==='object'){
      var tb=document.getElementById('openthermvalues');tb.innerHTML='';
      for(var key in d.opentherm){if(Object.hasOwn(d.opentherm,key)){
        var row=document.createElement('tr');
        var nc=document.createElement('td');nc.id=key;nc.textContent=key;
        var tc=document.createElement('td');tc.id=key+'-type';tc.textContent=d.opentherm[key].type;
        var vc=document.createElement('td');vc.id=key+'-value';vc.textContent=d.opentherm[key].value;
        row.appendChild(nc);row.appendChild(tc);row.appendChild(vc);
        tb.appendChild(row);
      }}
    }
  } catch(e){console.error(e);}
}
function buildRow(item,idKey){
  var row=document.createElement('tr');
  var topic=item[idKey];
  for(var k in item){if(Object.hasOwn(item,k)){
    var cell=document.createElement('td');
    cell.id=topic+'-'+k;
    cell.textContent=item[k];
    row.appendChild(cell);
  }}
  return row;
}
</script>
)====";

static const char consoleTogglesJS[] FLASHPROG = R"====(
<script>
// Refresh both console toggles from the live device state.
// The device's runtime flags are the source of truth: they reset on reboot to
// the "from start" settings and are flipped by /togglelog and /togglehexdump.
// Reading /getsettings after every change keeps the switches in sync with the
// device instead of with a (possibly stale) browser-side cache.
function refreshConsoleToggles() {
  return fetch('/getsettings')
    .then(function(response) { return response.json(); })
    .then(function(data) {
      var mqttEnabled = (data.logMqtt === 'enabled' || data.logMqtt === 1);
      var hexdumpEnabled = (data.logHexdump === 'enabled' || data.logHexdump === 1);
      document.getElementById('mqttLogToggle').checked = mqttEnabled;
      document.getElementById('hexdumpToggle').checked = hexdumpEnabled;
    })
    .catch(function(err) {
      console.error('Failed to fetch settings:', err);
    });
}

function initConsoleToggles() {
  refreshConsoleToggles();
}

// Toggle MQTT log, then re-read the device state so the switch reflects the
// actual resulting position (the endpoint flips the flag server-side).
function toggleMqttLog() {
  fetch('/togglelog')
    .then(function() { refreshConsoleToggles(); })
    .catch(function(err) {
      console.error('Error toggling MQTT log:', err);
      refreshConsoleToggles();
    });
}

// Toggle Hexdump log, then re-read the device state (see toggleMqttLog).
function toggleHexdump() {
  fetch('/togglehexdump')
    .then(function() { refreshConsoleToggles(); })
    .catch(function(err) {
      console.error('Error toggling hexdump:', err);
      refreshConsoleToggles();
    });
}

function downloadConsole() {
  var text = document.getElementById('cli').value;
  if (!text) return;
  var blob = new Blob([text], { type: 'text/plain' });
  var url = URL.createObjectURL(blob);
  var a = document.createElement('a');
  var now = new Date();
  var ts = now.getFullYear()
    + '-' + String(now.getMonth()+1).padStart(2,'0')
    + '-' + String(now.getDate()).padStart(2,'0')
    + '_' + String(now.getHours()).padStart(2,'0')
    + String(now.getMinutes()).padStart(2,'0')
    + String(now.getSeconds()).padStart(2,'0');
  a.href = url;
  a.download = 'heishamon-console-' + ts + '.txt';
  a.click();
  URL.revokeObjectURL(url);
}

// Initialize on page load
document.addEventListener('DOMContentLoaded', function() {
  if (document.getElementById('mqttLogToggle')) {
    initConsoleToggles();
  }
});
</script>
)====";

// ─────────────────────────────────────────────────────────────────────────────
// ROOT PAGE BODY FRAGMENTS
// ─────────────────────────────────────────────────────────────────────────────

// Side nav links for root page (injected via JS on load below)
// We build the nav + status bar in one block, then the tab panes.

// Side nav links and status bar for root page
static const char webBodyRoot1[] FLASHPROG = R"====(
<script>
document.addEventListener('DOMContentLoaded',function(){
  var nav=document.getElementById('sideNav');
  nav.innerHTML=`
<a href="/firmware"><span class="nav-icon">&#8679;</span> Firmware</a>
<a href="/reboot" onclick="return confirm('Reboot the device?')"><span class="nav-icon">&#8635;</span> Reboot</a>
<a href="/rules"><span class="nav-icon">&#8881;</span> Rules</a>
<a href="/settings"><span class="nav-icon">&#9881;</span> Settings</a>
`;
});
</script>
<div class='main-content'>
<div class='statusbar' id='statusBar'>
  <div class='status-chip'><span class='status-dot'></span><span class='chip-label'>WiFi</span><span class='chip-value' id='wifi'>—</span><span style='color:var(--text-muted);font-size:11px'>%</span></div>
  <div class='status-chip'><span class='chip-label'>Memory</span><span class='chip-value' id='memory'>—</span><span style='color:var(--text-muted);font-size:11px'>%</span></div>
)===="
#ifdef ESP32
R"====(
  <div class='status-chip'><span class='chip-label'>Ethernet</span><span class='chip-value' id='ethernet'>—</span></div>
)===="
#endif
R"====(
  <div class='status-chip'><span class='chip-label'>Correct</span><span class='chip-value' id='correct'>—</span><span style='color:var(--text-muted);font-size:11px'>%</span></div>
  <div class='status-chip'><span class='chip-label'>MQTT reconnects</span><span class='chip-value' id='mqtt'>—</span></div>
  <div class='status-chip'><span class='chip-label'>Uptime</span><span class='chip-value' id='uptime'>—</span></div>
  <div class='status-chip rules-inactive' id='rulesChip'><span class='chip-label'>Rules</span><span class='chip-value' id='rules_value'>—</span></div>
</div>
)====";

// listen-only badge (conditionally appended server-side before tabs)
static const char webBodyRootStatusListenOnly[] FLASHPROG = R"====(
<script>document.addEventListener('DOMContentLoaded',function(){
  var bar=document.getElementById('statusBar');
  var chip=document.createElement('div');
  chip.className='status-chip listen-only';
  chip.innerHTML='<span class="chip-label">Listen Only</span><span class="chip-value">ACTIVE</span>';
  bar.appendChild(chip);
});</script>
)====";

// Tab navigation bar
static const char webTabnavOpen[] FLASHPROG = R"====(
<nav class='tabnav'>
  <button class='tabnav-btn active' data-tab='Heatpump' onclick="openTable('Heatpump')">Heatpump</button>
)====";
static const char webBodyRootDallasTab[] FLASHPROG = R"====(
<button class='tabnav-btn' data-tab='Dallas' onclick="openTable('Dallas')">Dallas 1-Wire</button>
)====";
static const char webBodyRootS0Tab[] FLASHPROG = R"====(
<button class='tabnav-btn' data-tab='S0' onclick="openTable('S0')">S0 kWh</button>
)====";
static const char webBodyRootOpenthermTab[] FLASHPROG = R"====(
<button class='tabnav-btn' data-tab='Opentherm' onclick="openTable('Opentherm')">Opentherm</button>
)====";
static const char webTabnavClose[] FLASHPROG = R"====(
  <button class='tabnav-btn' data-tab='Console' onclick="openTable('Console')">Console</button>
</nav>
)====";
// These are injected by the server between tabnav wrappers:
static const char webBodyEndDiv[] FLASHPROG = "</div>";

// ─── TAB PANES ───
static const char webBodyRootHeatpumpValues[] FLASHPROG = R"====(
<div id='Heatpump' class='tab-pane active'>
<div class='panel'>
  <div class='panel-header'><h3>Heatpump Values</h3><span class='panel-meta' id='hpMeta'>Live</span></div>
  <table><thead><tr>
    <th>Topic</th><th>Name</th><th>Value</th><th>Description</th>
  </tr></thead><tbody id='heishavalues'>
    <tr><td colspan='4' style='color:var(--text-muted);padding:24px;text-align:center'>Loading…</td></tr>
  </tbody></table>
</div></div>
)====";

static const char webBodyRootDallasValues[] FLASHPROG = R"====(
<div id='Dallas' class='tab-pane'>
<div class='panel'>
  <div class='panel-header'><h3>Dallas 1-Wire Sensors</h3><button onclick='rescanDallas()' class='btn btn-ghost' style='padding:4px 10px;font-size:11px;height:24px;'>&#8635; Rescan bus</button></div>
  <table><thead><tr>
    <th>Sensor</th><th>Temperature</th><th>Alias</th><th>Status</th><th></th>
  </tr></thead><tbody id='dallasvalues'>
    <tr><td colspan='5' style='color:var(--text-muted);padding:24px;text-align:center'>Loading…</td></tr>
  </tbody></table>
</div></div>
)====";

static const char webBodyRootS0Values[] FLASHPROG = R"====(
<div id='S0' class='tab-pane'>
<div class='panel'>
  <div class='panel-header'><h3>S0 kWh Meters</h3><span class='panel-meta'>Live</span></div>
  <table><thead><tr>
    <th>Port</th><th>Watt</th><th>Wh</th><th>Wh Total</th><th>Pulse Quality</th><th>Avg Pulse Width</th>
  </tr></thead><tbody id='s0values'>
    <tr><td colspan='6' style='color:var(--text-muted);padding:24px;text-align:center'>Loading…</td></tr>
  </tbody></table>
</div></div>
)====";

static const char webBodyRootOpenthermValues[] FLASHPROG = R"====(
<div id='Opentherm' class='tab-pane'>
<div class='panel'>
  <div class='panel-header'><h3>Opentherm Values</h3><span class='panel-meta'>Live</span></div>
  <table><thead><tr>
    <th>Name</th><th>Type</th><th>Value</th>
  </tr></thead><tbody id='openthermvalues'>
    <tr><td colspan='3' style='color:var(--text-muted);padding:24px;text-align:center'>Loading…</td></tr>
  </tbody></table>
</div></div>
)====";

static const char webBodyRootConsole[] FLASHPROG = R"====(
<div id='Console' class='tab-pane'>
<div class='panel' style='display:flex;flex-direction:column;height:calc(100vh - 180px)'>
  <div class='panel-header'>
    <h3>Console Output</h3>
    <div style='display:flex;gap:16px;align-items:center'>
      <div class='console-toggle-compact'>
        <span class='console-toggle-label-compact'>Log to MQTT</span>
        <label class='theme-switch-compact'>
          <input type='checkbox' id='mqttLogToggle' onchange='toggleMqttLog()'>
          <span class='theme-slider-compact'></span>
        </label>
      </div>
      <div class='console-toggle-compact'>
        <span class='console-toggle-label-compact'>Hexdump</span>
        <label class='theme-switch-compact'>
          <input type='checkbox' id='hexdumpToggle' onchange='toggleHexdump()'>
          <span class='theme-slider-compact'></span>
        </label>
      </div>
      <div class='console-toggle-compact'>
        <span class='console-toggle-label-compact'>Autoscroll</span>
        <label class='theme-switch-compact'>
          <input type='checkbox' id='autoscroll' checked>
          <span class='theme-slider-compact'></span>
        </label>
      </div>
      <button onclick='downloadConsole()' class='btn btn-ghost' style='padding:4px 10px;font-size:11px;height:24px;'>&#8681; Download</button>
    </div>
  </div>
  <div style='flex:1;padding:16px;display:flex;flex-direction:column'>
    <textarea id='cli' disabled style='flex:1;height:auto;min-height:300px'></textarea>
  </div>
</div></div>
)====";

// ─────────────────────────────────────────────────────────────────────────────
// SETTINGS PAGE
// ─────────────────────────────────────────────────────────────────────────────
static const char settingsJS[] FLASHPROG = R"====(
<script>
function ShowHideDallasTable(cb){
  document.getElementById('dallassettings').style.display=cb.checked?'block':'none';
}
function ShowHideS0Table(cb){
  document.getElementById('s0settings').style.display=cb.checked?'block':'none';
}
function changeMinWatt(port){
  var ppkwh=document.getElementById('s0_ppkwh_'+port).value;
  var interval=document.getElementById('s0_interval_'+port).value;
  document.getElementById('s0_minwatt_'+port).innerHTML=Math.round((3600*1000/ppkwh)/interval);
}
</script>
)====";

#ifdef TLS_SUPPORT 
static const char caUploadJS[] PROGMEM = R"====(
  <script>
    (function(){
      function uploadCA(){
        var f = document.getElementById('mqtt_ca_cert_file').files[0];
        var btn = document.getElementById('ca_upload_btn');
        var st  = document.getElementById('ca_status');
        if (!f) { st.innerText = 'Please choose a file first.'; return; }
        btn.disabled = true;
        st.innerText = 'Uploading...';
        var fd = new FormData();
        fd.append('cacert', f, f.name);
        var xhr = new XMLHttpRequest();
        xhr.onreadystatechange = function(){
          if (xhr.readyState === 4) {
            st.innerText = xhr.responseText || 'No response';
            btn.disabled = false;
            if (xhr.status === 200 && /success/i.test(st.innerText)) {
              if (typeof getSettings === 'function') { getSettings(); }
            }
          }
        };
        xhr.open('POST', '/cacert');
        xhr.send(fd);
      }
      window.uploadCA = uploadCA;
    })();
  </script>;
)====";
#endif

static const char webBodySettings1[] FLASHPROG = R"====(
<script>
document.addEventListener('DOMContentLoaded',function(){
  var nav=document.getElementById('sideNav');
  nav.innerHTML=`
<a href="/"><span class="nav-icon">&#8634;</span> Home</a>
<a href="/firmware"><span class="nav-icon">&#8679;</span> Firmware</a>
<a href="/reboot" onclick="return confirm('Reboot the device?')"><span class="nav-icon">&#8635;</span> Reboot</a>
<a href="/rules"><span class="nav-icon">&#8881;</span> Rules</a>
`;
});
</script>
)====";

static const char settingsForm1[] FLASHPROG = R"====(
<div class='main-content' style='max-width:780px;margin:0 auto'>
<div id='loading_settings' class='loading-overlay'>
  <div class='spinner'></div> Loading settings…
</div>
<div id='settings_form' style='display:none'>
  <form accept-charset='UTF-8' action='/savesettings' method='POST'>
  <div class='panel' style='margin-bottom:16px'>
  <div class='panel-header'><h3>Network</h3></div>
  <div class='settings-grid'>
    <div class='setting-row'>
      <label class='setting-label'>Hostname</label>
      <input type='text' name='wifi_hostname' maxlength='39' class='setting-input' value=''>
    </div>
    <div class='setting-row'>
      <label class='setting-label'>WiFi SSID</label>
      <div>
        <input type='text' name='wifi_ssid' id='wifi_ssid_id' class='setting-input' value=''>
        <select id='wifi_ssid_select' onchange='changewifissid()'>
          <option hidden selected value=''>Select SSID</option>
        </select>
      </div>
    </div>
    <div class='setting-row'>
      <label class='setting-label'>WiFi Password</label>
      <input type='password' name='wifi_password' maxlength='64' class='setting-input' value=''>
    </div>
  </div></div>
  <div class='panel' style='margin-bottom:16px'>
  <div class='panel-header'><h3>Update Authentication</h3></div>
  <div class='settings-grid'>
    <div class='setting-row'>
      <label class='setting-label'>Update Username</label>
      <span style='font-size:12.5px;color:var(--text-muted)'>admin</span>
    </div>
    <div class='setting-row'>
      <label class='setting-label'>Current Password</label>
      <div style='display:flex;align-items:center;gap:10px'>
        <input type='password' name='current_ota_password' maxlength='39' class='setting-input' value=''>
        <span class='setting-hint'>default: "heisha"</span>
      </div>
    </div>
    <div class='setting-row'>
      <label class='setting-label'>New Password</label>
      <input type='password' name='new_ota_password' maxlength='39' class='setting-input' value=''>
    </div>
  </div></div>
  <div class='panel' style='margin-bottom:16px'>
  <div class='panel-header'><h3>MQTT</h3></div>
  <div class='settings-grid'>
    <div class='setting-row'>
      <label class='setting-label'>Topic Base</label>
      <input type='text' name='mqtt_topic_base' maxlength='127' class='setting-input' value=''>
    </div>
    <div class='setting-row'>
      <label class='setting-label'>Server</label>
      <input type='text' name='mqtt_server' maxlength='64' class='setting-input' value=''>
    </div>
    <div class='setting-row'>
      <label class='setting-label'>Port</label>
      <input type='number' name='mqtt_port' maxlength='5' class='setting-input' value=''>
    </div>
    <div class='setting-row'>
      <label class='setting-label'>Username</label>
      <input type='text' name='mqtt_username' maxlength='64' class='setting-input' value=''>
    </div>
    <div class='setting-row'>
      <label class='setting-label'>Password</label>
      <input type='password' name='mqtt_password' maxlength='64' class='setting-input' value=''>
    </div>
  )===="  
  
  #ifdef TLS_SUPPORT
  R"====(
<div class='setting-row'>
  <label class='setting-label'>Use TLS (port 8883)</label>
  <div class='checkbox-wrap'>
    <input type='checkbox' id='mqtt_tls_enabled' name='mqtt_tls_enabled' value='enabled'>
  </div>
</div>

<div class='setting-row'>
  <label class='setting-label'>Root CA Certificate (PEM)</label>
  <div>
    <div class='file-input-wrap'>
      <input type='file' id='mqtt_ca_cert_file' accept='.pem,.crt,.cer'
             onchange="document.getElementById('ca_file_name').innerText=this.files&&this.files[0]?this.files[0].name:'No file selected'">
    </div>
    <div id='ca_file_name' style='margin-top:4px;font-size:11px;color:var(--text-muted)'>No file selected</div>
  </div>
</div>

<div class='setting-row'>
  <label class='setting-label'>CA Certificate Actions</label>
  <div style='display:flex;align-items:center;gap:8px;flex-wrap:wrap'>
    <button type='button' id='ca_upload_btn' onclick='uploadCA()' class='btn btn-primary'>Upload CA</button>
    <button type='button' onclick="window.open('/cacert','_blank')" class='btn btn-ghost'>View / Download CA</button>
    <span id='ca_status' style='font-size:11px;color:var(--text-muted);font-style:italic'>No upload yet</span>
  </div>
</div>
)===="
#endif

R"====(
  </div></div>
  <div class='panel' style='margin-bottom:16px'>
  <div class='panel-header'><h3>Time</h3></div>
  <div class='settings-grid'>
    <div class='setting-row'>
      <label class='setting-label'>NTP Servers</label>
      <input type='text' name='ntp_servers' maxlength='253' class='setting-input' value='' placeholder='Comma separated'>
    </div>
    <div class='setting-row'>
      <label class='setting-label'>Timezone</label>
      <select name='timezone' class='setting-input'>
)====";

// server inserts tzDataOptions here, then settingsForm2 continues

#ifdef ESP8266
static const char settingsForm2[] FLASHPROG = R"====(
      </select>
    </div>
  </div></div>
  <div class='panel' style='margin-bottom:16px'>
  <div class='panel-header'><h3>Polling</h3></div>
  <div class='settings-grid'>
    <div class='setting-row'>
      <label class='setting-label'>Heatpump poll interval</label>
      <div style='display:flex;align-items:center;gap:8px'>
        <input type='number' name='waitTime' class='setting-input' value='' style='width:80px'>
        <span class='setting-hint'>seconds (min 5)</span>
      </div>
    </div>
    <div class='setting-row'>
      <label class='setting-label'>MQTT retransmit interval</label>
      <div style='display:flex;align-items:center;gap:8px'>
        <input type='number' name='updateAllTime' class='setting-input' value='' style='width:80px'>
        <span class='setting-hint'>seconds</span>
      </div>
    </div>
  </div></div>
  <div class='panel' style='margin-bottom:16px'>
  <div class='panel-header'><h3>Behavior</h3></div>
  <div class='settings-grid'>
    <div class='setting-row'><label class='setting-label'>WiFi hotspot when disconnected</label><div class='checkbox-wrap'><input type='checkbox' name='hotspot' value='enabled'></div></div>
    <div class='setting-row'><label class='setting-label'>Debug log to MQTT from start</label><div class='checkbox-wrap'><input type='checkbox' name='logMqtt' value='enabled'></div></div>
    <div class='setting-row'><label class='setting-label'>Debug hexdump from start</label><div class='checkbox-wrap'><input type='checkbox' name='logHexdump' value='enabled'></div></div>
    <div class='setting-row'><label class='setting-label'>Debug log to serial1 (GPIO2)</label><div class='checkbox-wrap'><input type='checkbox' name='logSerial1' value='enabled'></div></div>
    <div class='setting-row'><label class='setting-label'>Emulate optional PCB</label><div class='checkbox-wrap'><input type='checkbox' name='optionalPCB' value='enabled'></div></div>
    <div class='setting-row'><label class='setting-label'>Enable Opentherm processing</label><div class='checkbox-wrap'><input type='checkbox' name='opentherm' value='enabled'></div></div>
    <div class='setting-row'><label class='setting-label'>Force load rules on boot</label><div style='display:flex;align-items:center;gap:10px'><div class='checkbox-wrap'><input type='checkbox' name='force_rules' value='enabled'></div><span class='setting-hint' style='display:block;margin-top:4px'>Rules load normally, but skip after crashes to prevent boot loops. Enable to override.</span></div></div>
  </div></div>
  <div class='panel' style='margin-bottom:16px'>
  <div class='panel-header'><h3>Listen Only</h3></div>
  <div class='settings-grid'>
    <div class='setting-row'><label class='setting-label'>Listen only (parallel CZ-TAW1)</label><div class='checkbox-wrap'><input type='checkbox' name='listenonly' value='enabled'></div></div>
  </div></div>
  <div class='panel' style='margin-bottom:16px'>
  <div class='panel-header'><h3>1-Wire DS18B20</h3></div>
  <div class='settings-grid'>
    <div class='setting-row'><label class='setting-label'>Use 1-Wire DS18B20</label><div class='checkbox-wrap'><input type='checkbox' onclick='ShowHideDallasTable(this)' name='use_1wire' value='enabled'></div></div>
  </div>
  <div id='dallassettings' style='display:none'>
  <div class='settings-grid'>
    <div class='setting-row'>
      <label class='setting-label'>1-Wire poll interval</label>
      <div style='display:flex;align-items:center;gap:8px'>
        <input type='number' name='waitDallasTime' class='setting-input' value='' style='width:80px'>
        <span class='setting-hint'>seconds (min 5)</span>
      </div>
    </div>
    <div class='setting-row'>
      <label class='setting-label'>1-Wire MQTT retransmit</label>
      <div style='display:flex;align-items:center;gap:8px'>
        <input type='number' name='updataAllDallasTime' class='setting-input' value='' style='width:80px'>
        <span class='setting-hint'>seconds</span>
      </div>
    </div>
    <div class='setting-row'>
      <label class='setting-label'>Temperature resolution</label>
      <div class='radio-group'>
        <label><input type='radio' id='9-bit' name='dallasResolution' value='9'> 9-bit</label>
        <label><input type='radio' id='10-bit' name='dallasResolution' value='10'> 10-bit</label>
        <label><input type='radio' id='11-bit' name='dallasResolution' value='11'> 11-bit</label>
        <label><input type='radio' id='12-bit' name='dallasResolution' value='12'> 12-bit</label>
      </div>
    </div>
  </div></div>
  </div>
  <div class='panel' style='margin-bottom:16px'>
  <div class='panel-header'><h3>S0 kWh Metering</h3></div>
  <div class='settings-grid'>
    <div class='setting-row'><label class='setting-label'>Use S0 kWh metering</label><div class='checkbox-wrap'><input type='checkbox' onclick='ShowHideS0Table(this)' name='use_s0' value='enabled'></div></div>
  </div>
  <div id='s0settings' style='display:none'>
  <div class='settings-grid'>
    <div class='setting-row'><label class='setting-label'>Port 1 imp/kWh</label><input type='number' id='s0_ppkwh_1' onchange='changeMinWatt(1)' name='s0_1_ppkwh' class='setting-input' value=''></div>
    <div class='setting-row'><label class='setting-label'>Port 1 standby interval</label><div style='display:flex;align-items:center;gap:8px'><input type='number' id='s0_interval_1' onchange='changeMinWatt(1)' name='s0_1_interval' class='setting-input' value='' style='width:80px'><span class='setting-hint'>seconds</span></div></div>
    <div class='setting-row'><label class='setting-label'>Port 1 min pulse width</label><div style='display:flex;align-items:center;gap:8px'><input type='number' id='s0_minpulsewidth_1' name='s0_1_minpulsewidth' class='setting-input' value='' style='width:80px'><span class='setting-hint'>ms</span></div></div>
    <div class='setting-row'><label class='setting-label'>Port 1 max pulse width</label><div style='display:flex;align-items:center;gap:8px'><input type='number' id='s0_maxpulsewidth_1' name='s0_1_maxpulsewidth' class='setting-input' value='' style='width:80px'><span class='setting-hint'>ms</span></div></div>
    <div class='setting-row'><label class='setting-label'>Port 1 standby threshold</label><span style='font-size:12px;color:var(--text-muted)'><span id='s0_minwatt_1'>—</span> W</span></div>
    <div class='setting-row'><label class='setting-label'>Port 2 imp/kWh</label><input type='number' id='s0_ppkwh_2' onchange='changeMinWatt(2)' name='s0_2_ppkwh' class='setting-input' value=''></div>
    <div class='setting-row'><label class='setting-label'>Port 2 standby interval</label><div style='display:flex;align-items:center;gap:8px'><input type='number' id='s0_interval_2' onchange='changeMinWatt(2)' name='s0_2_interval' class='setting-input' value='' style='width:80px'><span class='setting-hint'>seconds</span></div></div>
    <div class='setting-row'><label class='setting-label'>Port 2 min pulse width</label><div style='display:flex;align-items:center;gap:8px'><input type='number' id='s0_minpulsewidth_2' name='s0_2_minpulsewidth' class='setting-input' value='' style='width:80px'><span class='setting-hint'>ms</span></div></div>
    <div class='setting-row'><label class='setting-label'>Port 2 max pulse width</label><div style='display:flex;align-items:center;gap:8px'><input type='number' id='s0_maxpulsewidth_2' name='s0_2_maxpulsewidth' class='setting-input' value='' style='width:80px'><span class='setting-hint'>ms</span></div></div>
    <div class='setting-row'><label class='setting-label'>Port 2 standby threshold</label><span style='font-size:12px;color:var(--text-muted)'><span id='s0_minwatt_2'>—</span> W</span></div>
  </div></div>
  </div>
  <div class='form-actions'>
    <button type='submit' class='btn btn-primary'>Save Settings</button>
  </div>
  </form>
  <div style='padding:0 20px 24px'>
    <a href='/factoryreset' class='btn btn-danger' onclick="return confirm('Are you sure? This will erase all configuration.')">Factory Reset</a>
  </div>
</div></div>
)====";

#else
// ESP32 version of settingsForm2
static const char settingsForm2[] FLASHPROG = R"====(
      </select>
    </div>
  </div></div>
  <div class='panel' style='margin-bottom:16px'>
  <div class='panel-header'><h3>Polling</h3></div>
  <div class='settings-grid'>
    <div class='setting-row'>
      <label class='setting-label'>Heatpump poll interval</label>
      <div style='display:flex;align-items:center;gap:8px'>
        <input type='number' name='waitTime' class='setting-input' value='' style='width:80px'>
        <span class='setting-hint'>seconds (min 5)</span>
      </div>
    </div>
    <div class='setting-row'>
      <label class='setting-label'>MQTT retransmit interval</label>
      <div style='display:flex;align-items:center;gap:8px'>
        <input type='number' name='updateAllTime' class='setting-input' value='' style='width:80px'>
        <span class='setting-hint'>seconds</span>
      </div>
    </div>
  </div></div>
  <div class='panel' style='margin-bottom:16px'>
  <div class='panel-header'><h3>Behavior</h3></div>
  <div class='settings-grid'>
    <div class='setting-row'><label class='setting-label'>WiFi hotspot when disconnected</label><div class='checkbox-wrap'><input type='checkbox' name='hotspot' value='enabled'></div></div>
    <div class='setting-row'><label class='setting-label'>Debug log to MQTT from start</label><div class='checkbox-wrap'><input type='checkbox' name='logMqtt' value='enabled'></div></div>
    <div class='setting-row'><label class='setting-label'>Debug hexdump from start</label><div class='checkbox-wrap'><input type='checkbox' name='logHexdump' value='enabled'></div></div>
    <div class='setting-row'><label class='setting-label'>Debug log USB</label><div class='checkbox-wrap'><input type='checkbox' name='logSerial1' value='enabled'></div></div>
    <div class='setting-row'><label class='setting-label'>Emulate optional PCB</label><div class='checkbox-wrap'><input type='checkbox' name='optionalPCB' value='enabled'></div></div>
    <div class='setting-row'><label class='setting-label'>Enable Opentherm processing</label><div class='checkbox-wrap'><input type='checkbox' name='opentherm' value='enabled'></div></div>
    <div class='setting-row'><label class='setting-label'>Enable CZ-TAW1 proxy port</label><div class='checkbox-wrap'><input type='checkbox' name='proxy' value='enabled'></div></div>
    <div class='setting-row'><label class='setting-label'>Force rules on boot</label><div class='checkbox-wrap'><input type='checkbox' name='force_rules' value='enabled'></div></div>
  </div></div>
  <div class='panel' style='margin-bottom:16px'>
  <div class='panel-header'><h3>Listen Only</h3></div>
  <div class='settings-grid'>
    <div class='setting-row'><label class='setting-label'>Listen only (parallel CZ-TAW1)</label><div class='checkbox-wrap'><input type='checkbox' name='listenonly' value='enabled'></div></div>
  </div></div>
  <div class='panel' style='margin-bottom:16px'>
  <div class='panel-header'><h3>1-Wire DS18B20</h3></div>
  <div class='settings-grid'>
    <div class='setting-row'><label class='setting-label'>Use 1-Wire DS18B20</label><div class='checkbox-wrap'><input type='checkbox' onclick='ShowHideDallasTable(this)' name='use_1wire' value='enabled'></div></div>
  </div>
  <div id='dallassettings' style='display:none'>
  <div class='settings-grid'>
    <div class='setting-row'>
      <label class='setting-label'>1-Wire poll interval</label>
      <div style='display:flex;align-items:center;gap:8px'>
        <input type='number' name='waitDallasTime' class='setting-input' value='' style='width:80px'>
        <span class='setting-hint'>seconds (min 5)</span>
      </div>
    </div>
    <div class='setting-row'>
      <label class='setting-label'>1-Wire MQTT retransmit</label>
      <div style='display:flex;align-items:center;gap:8px'>
        <input type='number' name='updataAllDallasTime' class='setting-input' value='' style='width:80px'>
        <span class='setting-hint'>seconds</span>
      </div>
    </div>
    <div class='setting-row'>
      <label class='setting-label'>Temperature resolution</label>
      <div class='radio-group'>
        <label><input type='radio' id='9-bit' name='dallasResolution' value='9'> 9-bit</label>
        <label><input type='radio' id='10-bit' name='dallasResolution' value='10'> 10-bit</label>
        <label><input type='radio' id='11-bit' name='dallasResolution' value='11'> 11-bit</label>
        <label><input type='radio' id='12-bit' name='dallasResolution' value='12'> 12-bit</label>
      </div>
    </div>
  </div></div>
  </div>
  <div class='panel' style='margin-bottom:16px'>
  <div class='panel-header'><h3>S0 kWh Metering</h3></div>
  <div class='settings-grid'>
    <div class='setting-row'><label class='setting-label'>Use S0 kWh metering</label><div class='checkbox-wrap'><input type='checkbox' onclick='ShowHideS0Table(this)' name='use_s0' value='enabled'></div></div>
  </div>
  <div id='s0settings' style='display:none'>
  <div class='settings-grid'>
    <div class='setting-row'><label class='setting-label'>Port 1 imp/kWh</label><input type='number' id='s0_ppkwh_1' onchange='changeMinWatt(1)' name='s0_1_ppkwh' class='setting-input' value=''></div>
    <div class='setting-row'><label class='setting-label'>Port 1 standby interval</label><div style='display:flex;align-items:center;gap:8px'><input type='number' id='s0_interval_1' onchange='changeMinWatt(1)' name='s0_1_interval' class='setting-input' value='' style='width:80px'><span class='setting-hint'>seconds</span></div></div>
    <div class='setting-row'><label class='setting-label'>Port 1 min pulse width</label><div style='display:flex;align-items:center;gap:8px'><input type='number' id='s0_minpulsewidth_1' name='s0_1_minpulsewidth' class='setting-input' value='' style='width:80px'><span class='setting-hint'>ms</span></div></div>
    <div class='setting-row'><label class='setting-label'>Port 1 max pulse width</label><div style='display:flex;align-items:center;gap:8px'><input type='number' id='s0_maxpulsewidth_1' name='s0_1_maxpulsewidth' class='setting-input' value='' style='width:80px'><span class='setting-hint'>ms</span></div></div>
    <div class='setting-row'><label class='setting-label'>Port 1 standby threshold</label><span style='font-size:12px;color:var(--text-muted)'><span id='s0_minwatt_1'>—</span> W</span></div>
    <div class='setting-row'><label class='setting-label'>Port 2 imp/kWh</label><input type='number' id='s0_ppkwh_2' onchange='changeMinWatt(2)' name='s0_2_ppkwh' class='setting-input' value=''></div>
    <div class='setting-row'><label class='setting-label'>Port 2 standby interval</label><div style='display:flex;align-items:center;gap:8px'><input type='number' id='s0_interval_2' onchange='changeMinWatt(2)' name='s0_2_interval' class='setting-input' value='' style='width:80px'><span class='setting-hint'>seconds</span></div></div>
    <div class='setting-row'><label class='setting-label'>Port 2 min pulse width</label><div style='display:flex;align-items:center;gap:8px'><input type='number' id='s0_minpulsewidth_2' name='s0_2_minpulsewidth' class='setting-input' value='' style='width:80px'><span class='setting-hint'>ms</span></div></div>
    <div class='setting-row'><label class='setting-label'>Port 2 max pulse width</label><div style='display:flex;align-items:center;gap:8px'><input type='number' id='s0_maxpulsewidth_2' name='s0_2_maxpulsewidth' class='setting-input' value='' style='width:80px'><span class='setting-hint'>ms</span></div></div>
    <div class='setting-row'><label class='setting-label'>Port 2 standby threshold</label><span style='font-size:12px;color:var(--text-muted)'><span id='s0_minwatt_2'>—</span> W</span></div>
  </div></div>
  </div>
  <div class='form-actions'>
    <button type='submit' class='btn btn-primary'>Save Settings</button>
  </div>
  </form>
  <div style='padding:0 20px 24px'>
    <a href='/factoryreset' class='btn btn-danger' onclick="return confirm('Are you sure? This will erase all configuration.')">Factory Reset</a>
  </div>
</div></div>
)====";
#endif

// Populate settings form via /getsettings (unchanged logic)
static const char populategetsettingsJS[] FLASHPROG = R"====(
<script>
var getSettings=function(){
  var req=new XMLHttpRequest();
  req.onreadystatechange=function(){
    if(req.readyState===4&&req.status===200){
      var j=JSON.parse(req.responseText);
)===="
#ifdef TLS_SUPPORT
R"====(
      if (j.mqtt_ca_cert) {
        document.getElementById('ca_status').innerText = j.mqtt_ca_cert;
      } else {
        document.getElementById('ca_status').innerText = 'Unknown CA status';
      }
)===="
#endif
R"====(
      for(var k in j){
        var el=document.getElementsByName(k);
        if(el.length>0){
          if(['text','number','password'].indexOf(el[0].type)>-1){el[0].value=j[k];}
          if(el[0].type=='checkbox'&&(j[k] === 'enabled' || j[k] === 1)){
            el[0].checked=true;
            if(k.indexOf('1wire')>-1)ShowHideDallasTable(el[0]);
            if(k.indexOf('s0')>-1)ShowHideS0Table(el[0]);
          }
          if(el[0].type=='radio'){for(var x=0;x<el.length;x++){if(el[x].value==j[k])el[x].checked=true;}}
          if(el[0].type.indexOf('select')>-1){var ch=el[0].childNodes;for(var x=0;x<ch.length;x++){if(ch[x].value==j[k])ch[x].selected=true;}}
        }
      }
      document.getElementById('loading_settings').style.display='none';
      document.getElementById('settings_form').style.display='block';
      changeMinWatt(1);changeMinWatt(2);
    }
  };
  req.open('GET','/getsettings',true);
  req.send();
};
getSettings();
</script>
)====";

static const char changewifissidJS[] FLASHPROG = R"====(
<script>
function changewifissid(){
  document.getElementById('wifi_ssid_id').value=document.getElementById('wifi_ssid_select').value;
}
</script>
)====";

static const char populatescanwifiJS[] FLASHPROG = R"====(
<script>
var refreshWifiScan=function(){
  var sel=document.getElementById('wifi_ssid_select');
  var req=new XMLHttpRequest();
  req.onreadystatechange=function(){
    if(req.readyState===4&&req.status===200){
      var list=JSON.parse(req.responseText);
      sel.innerHTML='';
      var def=document.createElement('option');
      def.value='';def.text='Select SSID';def.selected=true;def.hidden=true;
      sel.appendChild(def);
      list.forEach(function(item){
        var opt=document.createElement('option');
        opt.value=item.ssid;
        opt.text=item.ssid+' ('+item.rssi+' dBm)';
        sel.appendChild(opt);
      });
      sel.style.display='block';
    }
  };
  req.open('GET','/wifiscan',true);
  req.send();
  setTimeout(refreshWifiScan,30000);
};
setTimeout(refreshWifiScan,500);
</script>
)====";

// ─────────────────────────────────────────────────────────────────────────────
// RULES PAGE
// ─────────────────────────────────────────────────────────────────────────────
static const char showRulesPage1[] FLASHPROG = R"====(
<script>
document.addEventListener('DOMContentLoaded',function(){
  var nav=document.getElementById('sideNav');
  nav.innerHTML=`
<a href="/"><span class="nav-icon">&#8634;</span> Home</a>
<a href="/firmware"><span class="nav-icon">&#8679;</span> Firmware</a>
<a href="/reboot" onclick="return confirm('Reboot the device?')"><span class="nav-icon">&#8635;</span> Reboot</a>
<a href="/settings"><span class="nav-icon">&#9881;</span> Settings</a>
`;
});
</script>
  <div style='display:flex;'>
    <div id='line-numbers' class='line-numbers'></div>
    <div id='rules' contenteditable='true' spellcheck='false' class='rules-editor'>)====";



static const char showRulesPage2[] FLASHPROG = R"====(</div>
  </div>
  <div style='margin-top:12px;'>
    <button type='button' onclick='validateRules()' style='background:#3a7bd5;color:white;border:none;padding:10px 20px;border-radius:6px;cursor:pointer;margin-right:8px;'>Validate</button>
    <button type='button' onclick='saveRules()' style='background:#2ecc94;color:white;border:none;padding:10px 20px;border-radius:6px;cursor:pointer;'>Save Rules</button>
    <button type='button' onclick="clearRules()" style='background:#f44336;color:white;border:none;padding:10px 20px;border-radius:6px;cursor:pointer;'>Erase Rules</button>
  </div>
  <div id='validation-result'></div>
)====";

static const char rulesJS[] FLASHPROG = R"=====(
<script>
const KEYWORDS = ['on','then','end','if','else','elseif','NULL'];
const OPERATORS = ['&&','||','==','>=','<=','!=','>','<','+','-','*','/','%','^','='];
const FUNCTIONS = ['coalesce','max','min','isset','round','floor','ceil','setTimer','print','concat','gpio'];

function highlightRules() {
  const editor = document.getElementById('rules');
  const cursorPos = saveCursorPosition(editor);
  
  let text = editor.textContent;
  let html = '';
  let i = 0;
  
  while(i < text.length) {
    let matched = false;
    
    // Comments
    if(text.substr(i,2) === '//') {
      let end = text.indexOf('\n', i);
      if(end === -1) end = text.length;
      html += '<span class="comment">' + escapeHtml(text.substring(i,end)) + '</span>';
      i = end;
      matched = true;
    }
    
    // Strings
    if(!matched && (text[i] === "'" || text[i] === '"')) {
      const quote = text[i];
      let end = i + 1;
      while(end < text.length && text[end] !== quote) {
        if(text[end] === '\\') end++;
        end++;
      }
      if(end < text.length) end++;
      html += '<span class="string">' + escapeHtml(text.substring(i,end)) + '</span>';
      i = end;
      matched = true;
    }
    
    // Numbers
    if(!matched && /\d/.test(text[i])) {
      let end = i;
      while(end < text.length && /[\d.]/.test(text[end])) end++;
      html += '<span class="number">' + text.substring(i,end) + '</span>';
      i = end;
      matched = true;
    }
    
    // Keywords
    if(!matched && /[a-zA-Z]/.test(text[i])) {
      for(let k of KEYWORDS) {
        if(text.substr(i,k.length) === k && (i===0 || !/\w/.test(text[i-1])) && (i+k.length>=text.length || !/\w/.test(text[i+k.length]))) {
          html += '<span class="keyword">' + k + '</span>';
          i += k.length;
          matched = true;
          break;
        }
      }
    }
    
    // Functions
    if(!matched && /[a-zA-Z]/.test(text[i])) {
      for(let f of FUNCTIONS) {
        if(text.substr(i,f.length+1) === f+'(' && (i===0 || !/\w/.test(text[i-1]))) {
          html += '<span class="function">' + f + '</span>(';
          i += f.length + 1;
          matched = true;
          break;
        }
      }
    }
    
    // Heatpump params (@)
    if(!matched && text[i] === '@') {
      let end = i + 1;
      while(end < text.length && /\w/.test(text[end])) end++;
      html += '<span class="at-param">' + text.substring(i,end) + '</span>';
      i = end;
      matched = true;
    }
    
    // DateTime params (%)
    if(!matched && text[i] === '%') {
      let end = i + 1;
      while(end < text.length && /\w/.test(text[end])) end++;
      html += '<span class="percent-param">' + text.substring(i,end) + '</span>';
      i = end;
      matched = true;
    }
    
    // Thermostat params (?)
    if(!matched && text[i] === '?') {
      let end = i + 1;
      while(end < text.length && /\w/.test(text[end])) end++;
      html += '<span class="question-param">' + text.substring(i,end) + '</span>';
      i = end;
      matched = true;
    }
    
    // Dallas sensors
    if(!matched && text.substr(i,8) === 'ds18b20#') {
      let end = i + 8;
      while(end < text.length && /[0-9A-Fa-f]/.test(text[end])) end++;
      html += '<span class="ds18b20">' + text.substring(i,end) + '</span>';
      i = end;
      matched = true;
    }
    
    // Variables (# and $)
    if(!matched && (text[i] === '#' || text[i] === '$')) {
      let end = i + 1;
      while(end < text.length && /\w/.test(text[end])) end++;
      html += '<span class="variable">' + text.substring(i,end) + '</span>';
      i = end;
      matched = true;
    }
    
    // Operators (check longer ones first)
    if(!matched) {
      const sortedOps = OPERATORS.slice().sort((a,b) => b.length - a.length);
      for(let o of sortedOps) {
        if(text.substr(i,o.length) === o) {
          html += '<span class="operator">' + escapeHtml(o) + '</span>';
          i += o.length;
          matched = true;
          break;
        }
      }
    }
    
    // Default character
    if(!matched) {
      html += escapeHtml(text[i]);
      i++;
    }
  }
  
  editor.innerHTML = html;
  restoreCursorPosition(editor, cursorPos);
}

function escapeHtml(text) {
  return text.replace(/[&<>]/g, m => ({'&':'&amp;','<':'&lt;','>':'&gt;'}[m]));
}

function saveCursorPosition(el) {
  const sel = window.getSelection();
  if(sel.rangeCount === 0) return null;
  const range = sel.getRangeAt(0);
  const preCaretRange = range.cloneRange();
  preCaretRange.selectNodeContents(el);
  preCaretRange.setEnd(range.endContainer, range.endOffset);
  return preCaretRange.toString().length;
}

function restoreCursorPosition(el, pos) {
  if(pos === null) return;
  const sel = window.getSelection();
  let charCount = 0;
  const nodeStack = [el];
  let node, foundStart = false;
  const range = document.createRange();
  range.setStart(el, 0);
  range.collapse(true);
  
  while(!foundStart && (node = nodeStack.pop())) {
    if(node.nodeType === 3) {
      const nextCharCount = charCount + node.length;
      if(pos <= nextCharCount) {
        range.setStart(node, pos - charCount);
        foundStart = true;
      }
      charCount = nextCharCount;
    } else {
      let i = node.childNodes.length;
      while(i--) {
        nodeStack.push(node.childNodes[i]);
      }
    }
  }
  
  sel.removeAllRanges();
  sel.addRange(range);
}

function updateLineNumbers() {
  const editor = document.getElementById('rules');
  const lineNumbers = document.getElementById('line-numbers');
  const lines = editor.textContent.split('\n');
  const lineCount = lines.length;
  
  let html = '';
  for(let i = 1; i <= lineCount; i++) {
    html += i + '\n';
  }
  
  lineNumbers.textContent = html;
}

function validateRules() {
  const editor = document.getElementById('rules');
  const result = document.getElementById('validation-result');
  const code = editor.textContent.trim();
  
  if(!code) {
    result.innerHTML = '<div class="validation-feedback warning">Rules are empty.</div>';
    return;
  }
  
  const errors = [];
  const warnings = [];
  const blockStack = [];
  
  let cleanCode = code.replace(/\/\/.*$/gm, '');
  cleanCode = cleanCode.replace(/'[^']*'/g, '""').replace(/"[^"]*"/g, '""');
  
  const lines = cleanCode.split('\n');
  
  for(let lineNum = 0; lineNum < lines.length; lineNum++) {
    const line = lines[lineNum];
    const trimmed = line.trim();
    
    if(!trimmed) continue;
    
    const statements = trimmed.split(/;|\bthen\b/).map(s => s.trim()).filter(s => s);
    
    for(let stmt of statements) {
      if(/^on\s+/.test(stmt)) {
        blockStack.push({type: 'on', line: lineNum + 1});
      }
      else if(/^if\s+/.test(stmt)) {
        blockStack.push({type: 'if', line: lineNum + 1, hasElse: false});
      }
      else if(/^elseif\s+/.test(stmt)) {
        if(blockStack.length === 0 || blockStack[blockStack.length-1].type !== 'if') {
          errors.push('Line ' + (lineNum+1) + ': "elseif" without matching "if"');
        } else if(blockStack[blockStack.length-1].hasElse) {
          errors.push('Line ' + (lineNum+1) + ': "elseif" after "else"');
        }
      }
      else if(/^else$/.test(stmt)) {
        if(blockStack.length === 0 || blockStack[blockStack.length-1].type !== 'if') {
          errors.push('Line ' + (lineNum+1) + ': "else" without matching "if"');
        } else if(blockStack[blockStack.length-1].hasElse) {
          errors.push('Line ' + (lineNum+1) + ': Multiple "else" for same "if"');
        } else {
          blockStack[blockStack.length-1].hasElse = true;
        }
      }
      else if(/^end$/.test(stmt)) {
        if(blockStack.length === 0) {
          errors.push('Line ' + (lineNum+1) + ': "end" without matching block');
        } else {
          blockStack.pop();
        }
      }
    }
    
    // Check for missing semicolons - CORRECTED VERSION
    const origTrimmed = code.split('\n')[lineNum].trim();
    if(origTrimmed && !origTrimmed.startsWith('//')) {
      const needsSemicolon = !origTrimmed.endsWith(';') && 
                             !origTrimmed.endsWith('then') && 
                             !origTrimmed.endsWith('end') &&
                             origTrimmed !== 'else' &&
                             origTrimmed !== 'end' &&
                             !/^(on|if|elseif)\s/.test(origTrimmed);
      
      if(needsSemicolon) {
        warnings.push('Line ' + (lineNum+1) + ': Missing semicolon');
      }
    }
  }
  
  if(blockStack.length > 0) {
    const unclosed = blockStack.map(b => b.type + ' (line ' + b.line + ')');
    errors.push('Unclosed block(s): ' + unclosed.join(', '));
  }
  
  editor.classList.remove('rules-error', 'rules-valid');
  
  if(errors.length > 0) {
    editor.classList.add('rules-error');
    result.innerHTML = '<div class="validation-feedback error"><strong>Errors:</strong><br>' + errors.join('<br>') + '</div>';
  } else if(warnings.length > 0) {
    editor.classList.add('rules-valid');
    result.innerHTML = '<div class="validation-feedback warning"><strong>Warnings:</strong><br>' + warnings.join('<br>') + '</div>';
  } else {
    editor.classList.add('rules-valid');
    result.innerHTML = '<div class="validation-feedback success"><strong>✓ Rules are valid!</strong><br>No syntax errors detected.</div>';
  }
}

function saveRules() {
  const editor = document.getElementById('rules');
  const rulesText = editor.textContent.trim();
  
  // Use fetch with explicit body control
  fetch('/saverules', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded'
    },
    body: 'rules=' + encodeURIComponent(rulesText)
  })
  .then(function(response) {
    if (response.ok) {
      // Reload the page to show saved rules
      window.location.href = '/rules';
    } else {
      alert('Failed to save rules');
    }
  })
  .catch(function(err) {
    console.error('Error saving rules:', err);
    alert('Error saving rules');
  });
}

function clearRules() {
  if(confirm('Are you sure you want to clear all rules? This cannot be undone.')) {
    const editor = document.getElementById('rules');
    editor.textContent = '';
    
    // Save the empty rules
    const formData = new FormData();
    formData.append('rules', '');
    
    fetch('/saverules', {
      method: 'POST',
      body: formData
    })
    .then(function(response) {
      if (response.ok) {
        window.location.href = '/rules';
      } else {
        alert('Failed to clear rules');
      }
    })
    .catch(function(err) {
      console.error('Error clearing rules:', err);
      alert('Error clearing rules');
    });
  }
}

document.addEventListener('DOMContentLoaded', function() {
  const editor = document.getElementById('rules');
  if(editor) {
    editor.textContent = editor.textContent.trim();
    editor.addEventListener('keydown', function(e) {
      if(e.key === 'Enter') {
        e.preventDefault();
        const sel = window.getSelection();
        const range = sel.getRangeAt(0);
        const lines = editor.textContent.substring(0, getCursorPosition()).split('\n');
        const currentLine = lines[lines.length - 1];
        const indentMatch = currentLine.match(/^[\t ]*/);
        let indent = indentMatch ? indentMatch[0] : '';
        const trimmedLine = currentLine.trim();
        if(trimmedLine.endsWith('then')) {
          indent += '\t';
        }
        range.deleteContents();
        const textNode = document.createTextNode('\n' + indent);
        range.insertNode(textNode);
        range.setStartAfter(textNode);
        range.setEndAfter(textNode);
        sel.removeAllRanges();
        sel.addRange(range);
        highlightRules();
        updateLineNumbers();
      }
      if(e.key === 'Tab') {
        e.preventDefault();
        const sel = window.getSelection();
        const range = sel.getRangeAt(0);
        range.deleteContents();
        const textNode = document.createTextNode('\t');
        range.insertNode(textNode);
        range.setStartAfter(textNode);
        range.setEndAfter(textNode);
        sel.removeAllRanges();
        sel.addRange(range);
        highlightRules();
        updateLineNumbers();
      }
      if(e.key === 'Backspace') {
        const sel = window.getSelection();
        if(sel.rangeCount === 0) return;
        const range = sel.getRangeAt(0);
        const textBeforeCursor = editor.textContent.substring(0, getCursorPosition());
        const lines = textBeforeCursor.split('\n');
        const currentLine = lines[lines.length - 1];
        if(currentLine.match(/^[\t ]+$/) && range.collapsed) {
          e.preventDefault();
          const newIndent = currentLine.substring(0, currentLine.length - 1);
          const lineStart = textBeforeCursor.length - currentLine.length;
          const beforeLine = editor.textContent.substring(0, lineStart);
          const afterLine = editor.textContent.substring(textBeforeCursor.length);
          editor.textContent = beforeLine + newIndent + afterLine;
          restoreCursorPosition(editor, lineStart + newIndent.length);
          highlightRules();
          updateLineNumbers();
        }
      }
    });
    
    editor.addEventListener('input', highlightRules);
    highlightRules();
    updateLineNumbers();
  }
});

function getCursorPosition() {
  const sel = window.getSelection();
  if(sel.rangeCount === 0) return 0;
  const range = sel.getRangeAt(0);
  const preCaretRange = range.cloneRange();
  preCaretRange.selectNodeContents(document.getElementById('rules'));
  preCaretRange.setEnd(range.endContainer, range.endOffset);
  return preCaretRange.toString().length;
}

</script>
)=====";

// ─────────────────────────────────────────────────────────────────────────────
// FIRMWARE PAGE
// ─────────────────────────────────────────────────────────────────────────────
static const char showFirmwarePage[] FLASHPROG = R"====(
<script>
document.addEventListener('DOMContentLoaded',function(){
  var nav=document.getElementById('sideNav');
  nav.innerHTML=`
<a href="/"><span class="nav-icon">&#8634;</span> Home</a>
<a href="/reboot" onclick="return confirm('Reboot the device?')"><span class="nav-icon">&#8635;</span> Reboot</a>
<a href="/rules"><span class="nav-icon">&#8881;</span> Rules</a>
<a href="/settings"><span class="nav-icon">&#9881;</span> Settings</a>
`;
});
</script>
<script>
function getMD5(){
  var fn=document.getElementById('firmware').value;
  var sp=fn.split('-')[2];
  if(sp){var md5=sp.split('.')[0];if(md5.length==32)document.getElementById('md5').value=md5;}
}
function reloadHome(){setTimeout(function(){window.location.href='/';},15000);}
function uploadFile(){
  document.getElementById('updatebutton').disabled=true;
  document.getElementById('status').innerText='';
  var file=document.getElementById('firmware').files[0];
  var fd=new FormData();
  fd.append('firmware',file);
  fd.append('md5',document.getElementById('md5').value);
  var req=new XMLHttpRequest();
  req.upload.addEventListener('progress',function(e){document.getElementById('progressBar').value=Math.round((e.loaded/e.total)*100);},false);
  req.onreadystatechange=function(){
    if(req.readyState===4){
      document.getElementById('status').innerText=req.responseText;
      if(req.responseText.includes('success'))reloadHome();
      else document.getElementById('updatebutton').disabled=false;
    }
  };
  req.open('POST','/firmware');
  req.send(fd);
}
</script>
<div class='firmware-container'>
  <div class='firmware-info'>
)===="
#ifdef ESP32
R"====(    <strong>Board:</strong> HeishaMon Large (ESP32)<br>
    Download the <strong>HeishaMon large</strong> binary from the
    <a href='https://github.com/heishamon/HeishaMon/tree/main/binaries' target='_blank'>releases page</a>.
)===="
#else
R"====(    <strong>Board:</strong> HeishaMon Small (ESP8266)<br>
    Download the <strong>HeishaMon small</strong> binary from the
    <a href='https://github.com/heishamon/HeishaMon/tree/main/binaries' target='_blank'>releases page</a>.
)===="
#endif
R"====(  </div>
  <div class='panel'>
    <div class='panel-header'><h3>Firmware Update</h3></div>
    <div style='padding:24px'>
      <div class='file-input-wrap'>
        <input type='file' accept='.bin,.bin.gz' id='firmware' name='firmware' onchange='getMD5();'>
      </div>
      <div style='margin-top:20px'>
        <label style='font-size:12px;color:var(--text-muted);display:block;margin-bottom:6px'>MD5 Checksum</label>
        <input type='text' id='md5' name='md5' value='' size='32' minlength='32' maxlength='32' class='setting-input' placeholder='Auto-detected from filename'>
      </div>
      <div class='firmware-warning'>
        <strong>Warning:</strong> Leaving the MD5 checksum empty disables integrity verification, which could brick your HeishaMon. A TTL cable will be required to recover in that case.
      </div>
      <div style='margin-top:24px'>
        <button id='updatebutton' class='btn btn-primary' onclick='uploadFile()'>Upload &amp; Update</button>
      </div>
      <progress id='progressBar' value='0' max='100'></progress>
      <p id='status'></p>
    </div>
  </div>
</div>
)====";

static const char firmwareSuccessResponse[] FLASHPROG = R"====(
Update success! Rebooting. This page will refresh afterwards.
)====";

static const char firmwareFailResponse[] FLASHPROG = R"====(
Update failed! Please try again...
)====";

// ─────────────────────────────────────────────────────────────────────────────
// MESSAGE PAGES (reboot, factory reset, wifi change, password error, save ok)
// ─────────────────────────────────────────────────────────────────────────────
static const char webBodyFactoryResetWarning[] FLASHPROG = R"====(
<div class='msg-box danger'>
  <h2>Factory Reset</h2>
  <p>All configuration has been erased.<br>Connect to the <strong style='color:var(--text-primary)'>Heishamon-Setup</strong> WiFi hotspot to reconfigure.</p>
</div>
)====";

static const char webBodyRebootWarning[] FLASHPROG = R"====(
<div class='msg-box'>
  <h2>Rebooting…</h2>
  <p>The device is restarting. Please wait.</p>
</div>
)====";

static const char webBodySettingsNewWifiWarning[] FLASHPROG = R"====(
<div class='msg-box'>
  <h2>Reconfiguring WiFi</h2>
  <p>Attempting to connect to the new access point.<br><br>
  The <strong style='color:var(--text-primary)'>Heishamon-Setup</strong> hotspot will be brought down automatically on success.<br><br>
  This page will redirect to home shortly.</p>
</div>
)====";

static const char webBodySettingsResetPasswordWarning[] FLASHPROG = R"====(
<div class='msg-box warning'>
  <h2>Wrong Password</h2>
  <p>The current password you entered is incorrect.<br><br>
  Perform a factory reset to restore the default password: <strong style='color:var(--text-primary)'>heisha</strong></p>
</div>
)====";

static const char webBodySettingsSaveMessage[] FLASHPROG = R"====(
<div class='msg-box success'>
  <h2>Settings Saved</h2>
  <p>Configuration has been saved. The device is rebooting…</p>
</div>
)====";

// ─────────────────────────────────────────────────────────────────────────────
// TIMEZONE DATA (unchanged — referenced by server)
// ─────────────────────────────────────────────────────────────────────────────
// https://github.com/nayarsystems/posix_tz_db
struct tzStruct {
  char name[32];
  char value[46];
};
const tzStruct tzdata[] FLASHPROG = {

  { "ETC/GMT", "GMT0" },
  { "Africa/Abidjan", "GMT0" },
  { "Africa/Accra", "GMT0" },
  { "Africa/Addis_Ababa", "EAT-3" },
  { "Africa/Algiers", "CET-1" },
  { "Africa/Asmara", "EAT-3" },
  { "Africa/Bamako", "GMT0" },
  { "Africa/Bangui", "WAT-1" },
  { "Africa/Banjul", "GMT0" },
  { "Africa/Bissau", "GMT0" },
  { "Africa/Blantyre", "CAT-2" },
  { "Africa/Brazzaville", "WAT-1" },
  { "Africa/Bujumbura", "CAT-2" },
  { "Africa/Cairo", "EET-2" },
  { "Africa/Casablanca", "<+01>-1" },
  { "Africa/Ceuta", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Africa/Conakry", "GMT0" },
  { "Africa/Dakar", "GMT0" },
  { "Africa/Dar_es_Salaam", "EAT-3" },
  { "Africa/Djibouti", "EAT-3" },
  { "Africa/Douala", "WAT-1" },
  { "Africa/El_Aaiun", "<+01>-1" },
  { "Africa/Freetown", "GMT0" },
  { "Africa/Gaborone", "CAT-2" },
  { "Africa/Harare", "CAT-2" },
  { "Africa/Johannesburg", "SAST-2" },
  { "Africa/Juba", "CAT-2" },
  { "Africa/Kampala", "EAT-3" },
  { "Africa/Khartoum", "CAT-2" },
  { "Africa/Kigali", "CAT-2" },
  { "Africa/Kinshasa", "WAT-1" },
  { "Africa/Lagos", "WAT-1" },
  { "Africa/Libreville", "WAT-1" },
  { "Africa/Lome", "GMT0" },
  { "Africa/Luanda", "WAT-1" },
  { "Africa/Lubumbashi", "CAT-2" },
  { "Africa/Lusaka", "CAT-2" },
  { "Africa/Malabo", "WAT-1" },
  { "Africa/Maputo", "CAT-2" },
  { "Africa/Maseru", "SAST-2" },
  { "Africa/Mbabane", "SAST-2" },
  { "Africa/Mogadishu", "EAT-3" },
  { "Africa/Monrovia", "GMT0" },
  { "Africa/Nairobi", "EAT-3" },
  { "Africa/Ndjamena", "WAT-1" },
  { "Africa/Niamey", "WAT-1" },
  { "Africa/Nouakchott", "GMT0" },
  { "Africa/Ouagadougou", "GMT0" },
  { "Africa/Porto-Novo", "WAT-1" },
  { "Africa/Sao_Tome", "GMT0" },
  { "Africa/Tripoli", "EET-2" },
  { "Africa/Tunis", "CET-1" },
  { "Africa/Windhoek", "CAT-2" },
  { "America/Adak", "HST10HDT,M3.2.0,M11.1.0" },
  { "America/Anchorage", "AKST9AKDT,M3.2.0,M11.1.0" },
  { "America/Anguilla", "AST4" },
  { "America/Antigua", "AST4" },
  { "America/Araguaina", "<-03>3" },
  { "America/Argentina/Buenos_Aires", "<-03>3" },
  { "America/Argentina/Catamarca", "<-03>3" },
  { "America/Argentina/Cordoba", "<-03>3" },
  { "America/Argentina/Jujuy", "<-03>3" },
  { "America/Argentina/La_Rioja", "<-03>3" },
  { "America/Argentina/Mendoza", "<-03>3" },
  { "America/Argentina/Rio_Gallegos", "<-03>3" },
  { "America/Argentina/Salta", "<-03>3" },
  { "America/Argentina/San_Juan", "<-03>3" },
  { "America/Argentina/San_Luis", "<-03>3" },
  { "America/Argentina/Tucuman", "<-03>3" },
  { "America/Argentina/Ushuaia", "<-03>3" },
  { "America/Aruba", "AST4" },
  { "America/Asuncion", "<-04>4<-03>,M10.1.0/0,M3.4.0/0" },
  { "America/Atikokan", "EST5" },
  { "America/Bahia", "<-03>3" },
  { "America/Bahia_Banderas", "CST6CDT,M4.1.0,M10.5.0" },
  { "America/Barbados", "AST4" },
  { "America/Belem", "<-03>3" },
  { "America/Belize", "CST6" },
  { "America/Blanc-Sablon", "AST4" },
  { "America/Boa_Vista", "<-04>4" },
  { "America/Bogota", "<-05>5" },
  { "America/Boise", "MST7MDT,M3.2.0,M11.1.0" },
  { "America/Cambridge_Bay", "MST7MDT,M3.2.0,M11.1.0" },
  { "America/Campo_Grande", "<-04>4" },
  { "America/Cancun", "EST5" },
  { "America/Caracas", "<-04>4" },
  { "America/Cayenne", "<-03>3" },
  { "America/Cayman", "EST5" },
  { "America/Chicago", "CST6CDT,M3.2.0,M11.1.0" },
  { "America/Chihuahua", "MST7MDT,M4.1.0,M10.5.0" },
  { "America/Costa_Rica", "CST6" },
  { "America/Creston", "MST7" },
  { "America/Cuiaba", "<-04>4" },
  { "America/Curacao", "AST4" },
  { "America/Danmarkshavn", "GMT0" },
  { "America/Dawson", "MST7" },
  { "America/Dawson_Creek", "MST7" },
  { "America/Denver", "MST7MDT,M3.2.0,M11.1.0" },
  { "America/Detroit", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Dominica", "AST4" },
  { "America/Edmonton", "MST7MDT,M3.2.0,M11.1.0" },
  { "America/Eirunepe", "<-05>5" },
  { "America/El_Salvador", "CST6" },
  { "America/Fortaleza", "<-03>3" },
  { "America/Fort_Nelson", "MST7" },
  { "America/Glace_Bay", "AST4ADT,M3.2.0,M11.1.0" },
  { "America/Godthab", "<-03>3<-02>,M3.5.0/-2,M10.5.0/-1" },
  { "America/Goose_Bay", "AST4ADT,M3.2.0,M11.1.0" },
  { "America/Grand_Turk", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Grenada", "AST4" },
  { "America/Guadeloupe", "AST4" },
  { "America/Guatemala", "CST6" },
  { "America/Guayaquil", "<-05>5" },
  { "America/Guyana", "<-04>4" },
  { "America/Halifax", "AST4ADT,M3.2.0,M11.1.0" },
  { "America/Havana", "CST5CDT,M3.2.0/0,M11.1.0/1" },
  { "America/Hermosillo", "MST7" },
  { "America/Indiana/Indianapolis", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Indiana/Knox", "CST6CDT,M3.2.0,M11.1.0" },
  { "America/Indiana/Marengo", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Indiana/Petersburg", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Indiana/Tell_City", "CST6CDT,M3.2.0,M11.1.0" },
  { "America/Indiana/Vevay", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Indiana/Vincennes", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Indiana/Winamac", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Inuvik", "MST7MDT,M3.2.0,M11.1.0" },
  { "America/Iqaluit", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Jamaica", "EST5" },
  { "America/Juneau", "AKST9AKDT,M3.2.0,M11.1.0" },
  { "America/Kentucky/Louisville", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Kentucky/Monticello", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Kralendijk", "AST4" },
  { "America/La_Paz", "<-04>4" },
  { "America/Lima", "<-05>5" },
  { "America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0" },
  { "America/Lower_Princes", "AST4" },
  { "America/Maceio", "<-03>3" },
  { "America/Managua", "CST6" },
  { "America/Manaus", "<-04>4" },
  { "America/Marigot", "AST4" },
  { "America/Martinique", "AST4" },
  { "America/Matamoros", "CST6CDT,M3.2.0,M11.1.0" },
  { "America/Mazatlan", "MST7MDT,M4.1.0,M10.5.0" },
  { "America/Menominee", "CST6CDT,M3.2.0,M11.1.0" },
  { "America/Merida", "CST6CDT,M4.1.0,M10.5.0" },
  { "America/Metlakatla", "AKST9AKDT,M3.2.0,M11.1.0" },
  { "America/Mexico_City", "CST6CDT,M4.1.0,M10.5.0" },
  { "America/Miquelon", "<-03>3<-02>,M3.2.0,M11.1.0" },
  { "America/Moncton", "AST4ADT,M3.2.0,M11.1.0" },
  { "America/Monterrey", "CST6CDT,M4.1.0,M10.5.0" },
  { "America/Montevideo", "<-03>3" },
  { "America/Montreal", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Montserrat", "AST4" },
  { "America/Nassau", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/New_York", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Nipigon", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Nome", "AKST9AKDT,M3.2.0,M11.1.0" },
  { "America/Noronha", "<-02>2" },
  { "America/North_Dakota/Beulah", "CST6CDT,M3.2.0,M11.1.0" },
  { "America/North_Dakota/Center", "CST6CDT,M3.2.0,M11.1.0" },
  { "America/North_Dakota/New_Salem", "CST6CDT,M3.2.0,M11.1.0" },
  { "America/Nuuk", "<-03>3<-02>,M3.5.0/-2,M10.5.0/-1" },
  { "America/Ojinaga", "MST7MDT,M3.2.0,M11.1.0" },
  { "America/Panama", "EST5" },
  { "America/Pangnirtung", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Paramaribo", "<-03>3" },
  { "America/Phoenix", "MST7" },
  { "America/Port-au-Prince", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Port_of_Spain", "AST4" },
  { "America/Porto_Velho", "<-04>4" },
  { "America/Puerto_Rico", "AST4" },
  { "America/Punta_Arenas", "<-03>3" },
  { "America/Rainy_River", "CST6CDT,M3.2.0,M11.1.0" },
  { "America/Rankin_Inlet", "CST6CDT,M3.2.0,M11.1.0" },
  { "America/Recife", "<-03>3" },
  { "America/Regina", "CST6" },
  { "America/Resolute", "CST6CDT,M3.2.0,M11.1.0" },
  { "America/Rio_Branco", "<-05>5" },
  { "America/Santarem", "<-03>3" },
  { "America/Santiago", "<-04>4<-03>,M9.1.6/24,M4.1.6/24" },
  { "America/Santo_Domingo", "AST4" },
  { "America/Sao_Paulo", "<-03>3" },
  { "America/Scoresbysund", "<-01>1<+00>,M3.5.0/0,M10.5.0/1" },
  { "America/Sitka", "AKST9AKDT,M3.2.0,M11.1.0" },
  { "America/St_Barthelemy", "AST4" },
  { "America/St_Johns", "NST3:30NDT,M3.2.0,M11.1.0" },
  { "America/St_Kitts", "AST4" },
  { "America/St_Lucia", "AST4" },
  { "America/St_Thomas", "AST4" },
  { "America/St_Vincent", "AST4" },
  { "America/Swift_Current", "CST6" },
  { "America/Tegucigalpa", "CST6" },
  { "America/Thule", "AST4ADT,M3.2.0,M11.1.0" },
  { "America/Thunder_Bay", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Tijuana", "PST8PDT,M3.2.0,M11.1.0" },
  { "America/Toronto", "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Tortola", "AST4" },
  { "America/Vancouver", "PST8PDT,M3.2.0,M11.1.0" },
  { "America/Whitehorse", "MST7" },
  { "America/Winnipeg", "CST6CDT,M3.2.0,M11.1.0" },
  { "America/Yakutat", "AKST9AKDT,M3.2.0,M11.1.0" },
  { "America/Yellowknife", "MST7MDT,M3.2.0,M11.1.0" },
  { "Antarctica/Casey", "<+11>-11" },
  { "Antarctica/Davis", "<+07>-7" },
  { "Antarctica/DumontDUrville", "<+10>-10" },
  { "Antarctica/Macquarie", "AEST-10AEDT,M10.1.0,M4.1.0/3" },
  { "Antarctica/Mawson", "<+05>-5" },
  { "Antarctica/McMurdo", "NZST-12NZDT,M9.5.0,M4.1.0/3" },
  { "Antarctica/Palmer", "<-03>3" },
  { "Antarctica/Rothera", "<-03>3" },
  { "Antarctica/Syowa", "<+03>-3" },
  { "Antarctica/Troll", "<+00>0<+02>-2,M3.5.0/1,M10.5.0/3" },
  { "Antarctica/Vostok", "<+06>-6" },
  { "Arctic/Longyearbyen", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Asia/Aden", "<+03>-3" },
  { "Asia/Almaty", "<+06>-6" },
  { "Asia/Amman", "EET-2EEST,M2.5.4/24,M10.5.5/1" },
  { "Asia/Anadyr", "<+12>-12" },
  { "Asia/Aqtau", "<+05>-5" },
  { "Asia/Aqtobe", "<+05>-5" },
  { "Asia/Ashgabat", "<+05>-5" },
  { "Asia/Atyrau", "<+05>-5" },
  { "Asia/Baghdad", "<+03>-3" },
  { "Asia/Bahrain", "<+03>-3" },
  { "Asia/Baku", "<+04>-4" },
  { "Asia/Bangkok", "<+07>-7" },
  { "Asia/Barnaul", "<+07>-7" },
  { "Asia/Beirut", "EET-2EEST,M3.5.0/0,M10.5.0/0" },
  { "Asia/Bishkek", "<+06>-6" },
  { "Asia/Brunei", "<+08>-8" },
  { "Asia/Chita", "<+09>-9" },
  { "Asia/Choibalsan", "<+08>-8" },
  { "Asia/Colombo", "<+0530>-5:30" },
  { "Asia/Damascus", "EET-2EEST,M3.5.5/0,M10.5.5/0" },
  { "Asia/Dhaka", "<+06>-6" },
  { "Asia/Dili", "<+09>-9" },
  { "Asia/Dubai", "<+04>-4" },
  { "Asia/Dushanbe", "<+05>-5" },
  { "Asia/Famagusta", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Asia/Gaza", "EET-2EEST,M3.4.4/48,M10.5.5/1" },
  { "Asia/Hebron", "EET-2EEST,M3.4.4/48,M10.5.5/1" },
  { "Asia/Ho_Chi_Minh", "<+07>-7" },
  { "Asia/Hong_Kong", "HKT-8" },
  { "Asia/Hovd", "<+07>-7" },
  { "Asia/Irkutsk", "<+08>-8" },
  { "Asia/Jakarta", "WIB-7" },
  { "Asia/Jayapura", "WIT-9" },
  { "Asia/Jerusalem", "IST-2IDT,M3.4.4/26,M10.5.0" },
  { "Asia/Kabul", "<+0430>-4:30" },
  { "Asia/Kamchatka", "<+12>-12" },
  { "Asia/Karachi", "PKT-5" },
  { "Asia/Kathmandu", "<+0545>-5:45" },
  { "Asia/Khandyga", "<+09>-9" },
  { "Asia/Kolkata", "IST-5:30" },
  { "Asia/Krasnoyarsk", "<+07>-7" },
  { "Asia/Kuala_Lumpur", "<+08>-8" },
  { "Asia/Kuching", "<+08>-8" },
  { "Asia/Kuwait", "<+03>-3" },
  { "Asia/Macau", "CST-8" },
  { "Asia/Magadan", "<+11>-11" },
  { "Asia/Makassar", "WITA-8" },
  { "Asia/Manila", "PST-8" },
  { "Asia/Muscat", "<+04>-4" },
  { "Asia/Nicosia", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Asia/Novokuznetsk", "<+07>-7" },
  { "Asia/Novosibirsk", "<+07>-7" },
  { "Asia/Omsk", "<+06>-6" },
  { "Asia/Oral", "<+05>-5" },
  { "Asia/Phnom_Penh", "<+07>-7" },
  { "Asia/Pontianak", "WIB-7" },
  { "Asia/Pyongyang", "KST-9" },
  { "Asia/Qatar", "<+03>-3" },
  { "Asia/Qyzylorda", "<+05>-5" },
  { "Asia/Riyadh", "<+03>-3" },
  { "Asia/Sakhalin", "<+11>-11" },
  { "Asia/Samarkand", "<+05>-5" },
  { "Asia/Seoul", "KST-9" },
  { "Asia/Shanghai", "CST-8" },
  { "Asia/Singapore", "<+08>-8" },
  { "Asia/Srednekolymsk", "<+11>-11" },
  { "Asia/Taipei", "CST-8" },
  { "Asia/Tashkent", "<+05>-5" },
  { "Asia/Tbilisi", "<+04>-4" },
  { "Asia/Tehran", "<+0330>-3:30<+0430>,J79/24,J263/24" },
  { "Asia/Thimphu", "<+06>-6" },
  { "Asia/Tokyo", "JST-9" },
  { "Asia/Tomsk", "<+07>-7" },
  { "Asia/Ulaanbaatar", "<+08>-8" },
  { "Asia/Urumqi", "<+06>-6" },
  { "Asia/Ust-Nera", "<+10>-10" },
  { "Asia/Vientiane", "<+07>-7" },
  { "Asia/Vladivostok", "<+10>-10" },
  { "Asia/Yakutsk", "<+09>-9" },
  { "Asia/Yangon", "<+0630>-6:30" },
  { "Asia/Yekaterinburg", "<+05>-5" },
  { "Asia/Yerevan", "<+04>-4" },
  { "Atlantic/Azores", "<-01>1<+00>,M3.5.0/0,M10.5.0/1" },
  { "Atlantic/Bermuda", "AST4ADT,M3.2.0,M11.1.0" },
  { "Atlantic/Canary", "WET0WEST,M3.5.0/1,M10.5.0" },
  { "Atlantic/Cape_Verde", "<-01>1" },
  { "Atlantic/Faroe", "WET0WEST,M3.5.0/1,M10.5.0" },
  { "Atlantic/Madeira", "WET0WEST,M3.5.0/1,M10.5.0" },
  { "Atlantic/Reykjavik", "GMT0" },
  { "Atlantic/South_Georgia", "<-02>2" },
  { "Atlantic/Stanley", "<-03>3" },
  { "Atlantic/St_Helena", "GMT0" },
  { "Australia/Adelaide", "ACST-9:30ACDT,M10.1.0,M4.1.0/3" },
  { "Australia/Brisbane", "AEST-10" },
  { "Australia/Broken_Hill", "ACST-9:30ACDT,M10.1.0,M4.1.0/3" },
  { "Australia/Currie", "AEST-10AEDT,M10.1.0,M4.1.0/3" },
  { "Australia/Darwin", "ACST-9:30" },
  { "Australia/Eucla", "<+0845>-8:45" },
  { "Australia/Hobart", "AEST-10AEDT,M10.1.0,M4.1.0/3" },
  { "Australia/Lindeman", "AEST-10" },
  { "Australia/Lord_Howe", "<+1030>-10:30<+11>-11,M10.1.0,M4.1.0" },
  { "Australia/Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3" },
  { "Australia/Perth", "AWST-8" },
  { "Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3" },
  { "Europe/Amsterdam", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Andorra", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Astrakhan", "<+04>-4" },
  { "Europe/Athens", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Europe/Belgrade", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Bratislava", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Brussels", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Bucharest", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Europe/Budapest", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Busingen", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Chisinau", "EET-2EEST,M3.5.0,M10.5.0/3" },
  { "Europe/Copenhagen", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Dublin", "IST-1GMT0,M10.5.0,M3.5.0/1" },
  { "Europe/Gibraltar", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Guernsey", "GMT0BST,M3.5.0/1,M10.5.0" },
  { "Europe/Helsinki", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Europe/Isle_of_Man", "GMT0BST,M3.5.0/1,M10.5.0" },
  { "Europe/Istanbul", "<+03>-3" },
  { "Europe/Jersey", "GMT0BST,M3.5.0/1,M10.5.0" },
  { "Europe/Kaliningrad", "EET-2" },
  { "Europe/Kiev", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Europe/Kirov", "<+03>-3" },
  { "Europe/Lisbon", "WET0WEST,M3.5.0/1,M10.5.0" },
  { "Europe/Ljubljana", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/London", "GMT0BST,M3.5.0/1,M10.5.0" },
  { "Europe/Luxembourg", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Madrid", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Malta", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Mariehamn", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Europe/Minsk", "<+03>-3" },
  { "Europe/Monaco", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Moscow", "MSK-3" },
  { "Europe/Oslo", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Podgorica", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Prague", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Riga", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Europe/Rome", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Samara", "<+04>-4" },
  { "Europe/San_Marino", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Sarajevo", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Saratov", "<+04>-4" },
  { "Europe/Simferopol", "MSK-3" },
  { "Europe/Skopje", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Sofia", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Europe/Stockholm", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Tallinn", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Europe/Tirane", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Ulyanovsk", "<+04>-4" },
  { "Europe/Uzhgorod", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Europe/Vaduz", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Vatican", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Vienna", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Vilnius", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Europe/Volgograd", "<+03>-3" },
  { "Europe/Warsaw", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Zagreb", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Zaporozhye", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Europe/Zurich", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Indian/Antananarivo", "EAT-3" },
  { "Indian/Chagos", "<+06>-6" },
  { "Indian/Christmas", "<+07>-7" },
  { "Indian/Cocos", "<+0630>-6:30" },
  { "Indian/Comoro", "EAT-3" },
  { "Indian/Kerguelen", "<+05>-5" },
  { "Indian/Mahe", "<+04>-4" },
  { "Indian/Maldives", "<+05>-5" },
  { "Indian/Mauritius", "<+04>-4" },
  { "Indian/Mayotte", "EAT-3" },
  { "Indian/Reunion", "<+04>-4" },
  { "Pacific/Apia", "<+13>-13" },
  { "Pacific/Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3" },
  { "Pacific/Bougainville", "<+11>-11" },
  { "Pacific/Chatham", "<+1245>-12:45<+1345>,M9.5.0/2:45,M4.1.0/3:45" },
  { "Pacific/Chuuk", "<+10>-10" },
  { "Pacific/Easter", "<-06>6<-05>,M9.1.6/22,M4.1.6/22" },
  { "Pacific/Efate", "<+11>-11" },
  { "Pacific/Enderbury", "<+13>-13" },
  { "Pacific/Fakaofo", "<+13>-13" },
  { "Pacific/Fiji", "<+12>-12<+13>,M11.2.0,M1.2.3/99" },
  { "Pacific/Funafuti", "<+12>-12" },
  { "Pacific/Galapagos", "<-06>6" },
  { "Pacific/Gambier", "<-09>9" },
  { "Pacific/Guadalcanal", "<+11>-11" },
  { "Pacific/Guam", "ChST-10" },
  { "Pacific/Honolulu", "HST10" },
  { "Pacific/Kiritimati", "<+14>-14" },
  { "Pacific/Kosrae", "<+11>-11" },
  { "Pacific/Kwajalein", "<+12>-12" },
  { "Pacific/Majuro", "<+12>-12" },
  { "Pacific/Marquesas", "<-0930>9:30" },
  { "Pacific/Midway", "SST11" },
  { "Pacific/Nauru", "<+12>-12" },
  { "Pacific/Niue", "<-11>11" },
  { "Pacific/Norfolk", "<+11>-11<+12>,M10.1.0,M4.1.0/3" },
  { "Pacific/Noumea", "<+11>-11" },
  { "Pacific/Pago_Pago", "SST11" },
  { "Pacific/Palau", "<+09>-9" },
  { "Pacific/Pitcairn", "<-08>8" },
  { "Pacific/Pohnpei", "<+11>-11" },
  { "Pacific/Port_Moresby", "<+10>-10" },
  { "Pacific/Rarotonga", "<-10>10" },
  { "Pacific/Saipan", "ChST-10" },
  { "Pacific/Tahiti", "<-10>10" },
  { "Pacific/Tarawa", "<+12>-12" },
  { "Pacific/Tongatapu", "<+13>-13" },
  { "Pacific/Wake", "<+12>-12" },
  { "Pacific/Wallis", "<+12>-12" },
  { "Etc/GMT-0", "GMT0" },
  { "Etc/GMT-1", "<+01>-1" },
  { "Etc/GMT-2", "<+02>-2" },
  { "Etc/GMT-3", "<+03>-3" },
  { "Etc/GMT-4", "<+04>-4" },
  { "Etc/GMT-5", "<+05>-5" },
  { "Etc/GMT-6", "<+06>-6" },
  { "Etc/GMT-7", "<+07>-7" },
  { "Etc/GMT-8", "<+08>-8" },
  { "Etc/GMT-9", "<+09>-9" },
  { "Etc/GMT-10", "<+10>-10" },
  { "Etc/GMT-11", "<+11>-11" },
  { "Etc/GMT-12", "<+12>-12" },
  { "Etc/GMT-13", "<+13>-13" },
  { "Etc/GMT-14", "<+14>-14" },
  { "Etc/GMT0", "GMT0" },
  { "Etc/GMT+0", "GMT0" },
  { "Etc/GMT+1", "<-01>1" },
  { "Etc/GMT+2", "<-02>2" },
  { "Etc/GMT+3", "<-03>3" },
  { "Etc/GMT+4", "<-04>4" },
  { "Etc/GMT+5", "<-05>5" },
  { "Etc/GMT+6", "<-06>6" },
  { "Etc/GMT+7", "<-07>7" },
  { "Etc/GMT+8", "<-08>8" },
  { "Etc/GMT+9", "<-09>9" },
  { "Etc/GMT+10", "<-10>10" },
  { "Etc/GMT+11", "<-11>11" },
  { "Etc/GMT+12", "<-12>12" },
  { "Etc/UCT", "UTC0" },
  { "Etc/UTC", "UTC0" },
  { "Etc/Greenwich", "GMT0" },
  { "Etc/Universal", "UTC0" },
  { "Etc/Zulu", "UTC0" },
};

// tzDataOptions unchanged — large option block for <select>
static const char tzDataOptions[] FLASHPROG = R"====(
<option value="0">ETC/GMT</option>
<option value="1">Africa/Abidjan</option>
<option value="2">Africa/Accra</option>
<option value="3">Africa/Addis_Ababa</option>
<option value="4">Africa/Algiers</option>
<option value="5">Africa/Asmara</option>
<option value="6">Africa/Bamako</option>
<option value="7">Africa/Bangui</option>
<option value="8">Africa/Banjul</option>
<option value="9">Africa/Bissau</option>
<option value="10">Africa/Blantyre</option>
<option value="11">Africa/Brazzaville</option>
<option value="12">Africa/Bujumbura</option>
<option value="13">Africa/Cairo</option>
<option value="14">Africa/Casablanca</option>
<option value="15">Africa/Ceuta</option>
<option value="16">Africa/Conakry</option>
<option value="17">Africa/Dakar</option>
<option value="18">Africa/Dar_es_Salaam</option>
<option value="19">Africa/Djibouti</option>
<option value="20">Africa/Douala</option>
<option value="21">Africa/El_Aaiun</option>
<option value="22">Africa/Freetown</option>
<option value="23">Africa/Gaborone</option>
<option value="24">Africa/Harare</option>
<option value="25">Africa/Johannesburg</option>
<option value="26">Africa/Juba</option>
<option value="27">Africa/Kampala</option>
<option value="28">Africa/Khartoum</option>
<option value="29">Africa/Kigali</option>
<option value="30">Africa/Kinshasa</option>
<option value="31">Africa/Lagos</option>
<option value="32">Africa/Libreville</option>
<option value="33">Africa/Lome</option>
<option value="34">Africa/Luanda</option>
<option value="35">Africa/Lubumbashi</option>
<option value="36">Africa/Lusaka</option>
<option value="37">Africa/Malabo</option>
<option value="38">Africa/Maputo</option>
<option value="39">Africa/Maseru</option>
<option value="40">Africa/Mbabane</option>
<option value="41">Africa/Mogadishu</option>
<option value="42">Africa/Monrovia</option>
<option value="43">Africa/Nairobi</option>
<option value="44">Africa/Ndjamena</option>
<option value="45">Africa/Niamey</option>
<option value="46">Africa/Nouakchott</option>
<option value="47">Africa/Ouagadougou</option>
<option value="48">Africa/Porto-Novo</option>
<option value="49">Africa/Sao_Tome</option>
<option value="50">Africa/Tripoli</option>
<option value="51">Africa/Tunis</option>
<option value="52">Africa/Windhoek</option>
<option value="53">America/Adak</option>
<option value="54">America/Anchorage</option>
<option value="55">America/Anguilla</option>
<option value="56">America/Antigua</option>
<option value="57">America/Araguaina</option>
<option value="58">America/Argentina/Buenos_Aires</option>
<option value="59">America/Argentina/Catamarca</option>
<option value="60">America/Argentina/Cordoba</option>
<option value="61">America/Argentina/Jujuy</option>
<option value="62">America/Argentina/La_Rioja</option>
<option value="63">America/Argentina/Mendoza</option>
<option value="64">America/Argentina/Rio_Gallegos</option>
<option value="65">America/Argentina/Salta</option>
<option value="66">America/Argentina/San_Juan</option>
<option value="67">America/Argentina/San_Luis</option>
<option value="68">America/Argentina/Tucuman</option>
<option value="69">America/Argentina/Ushuaia</option>
<option value="70">America/Aruba</option>
<option value="71">America/Asuncion</option>
<option value="72">America/Atikokan</option>
<option value="73">America/Bahia</option>
<option value="74">America/Bahia_Banderas</option>
<option value="75">America/Barbados</option>
<option value="76">America/Belem</option>
<option value="77">America/Belize</option>
<option value="78">America/Blanc-Sablon</option>
<option value="79">America/Boa_Vista</option>
<option value="80">America/Bogota</option>
<option value="81">America/Boise</option>
<option value="82">America/Cambridge_Bay</option>
<option value="83">America/Campo_Grande</option>
<option value="84">America/Cancun</option>
<option value="85">America/Caracas</option>
<option value="86">America/Cayenne</option>
<option value="87">America/Cayman</option>
<option value="88">America/Chicago</option>
<option value="89">America/Chihuahua</option>
<option value="90">America/Costa_Rica</option>
<option value="91">America/Creston</option>
<option value="92">America/Cuiaba</option>
<option value="93">America/Curacao</option>
<option value="94">America/Danmarkshavn</option>
<option value="95">America/Dawson</option>
<option value="96">America/Dawson_Creek</option>
<option value="97">America/Denver</option>
<option value="98">America/Detroit</option>
<option value="99">America/Dominica</option>
<option value="100">America/Edmonton</option>
<option value="101">America/Eirunepe</option>
<option value="102">America/El_Salvador</option>
<option value="103">America/Fortaleza</option>
<option value="104">America/Fort_Nelson</option>
<option value="105">America/Glace_Bay</option>
<option value="106">America/Godthab</option>
<option value="107">America/Goose_Bay</option>
<option value="108">America/Grand_Turk</option>
<option value="109">America/Grenada</option>
<option value="110">America/Guadeloupe</option>
<option value="111">America/Guatemala</option>
<option value="112">America/Guayaquil</option>
<option value="113">America/Guyana</option>
<option value="114">America/Halifax</option>
<option value="115">America/Havana</option>
<option value="116">America/Hermosillo</option>
<option value="117">America/Indiana/Indianapolis</option>
<option value="118">America/Indiana/Knox</option>
<option value="119">America/Indiana/Marengo</option>
<option value="120">America/Indiana/Petersburg</option>
<option value="121">America/Indiana/Tell_City</option>
<option value="122">America/Indiana/Vevay</option>
<option value="123">America/Indiana/Vincennes</option>
<option value="124">America/Indiana/Winamac</option>
<option value="125">America/Inuvik</option>
<option value="126">America/Iqaluit</option>
<option value="127">America/Jamaica</option>
<option value="128">America/Juneau</option>
<option value="129">America/Kentucky/Louisville</option>
<option value="130">America/Kentucky/Monticello</option>
<option value="131">America/Kralendijk</option>
<option value="132">America/La_Paz</option>
<option value="133">America/Lima</option>
<option value="134">America/Los_Angeles</option>
<option value="135">America/Lower_Princes</option>
<option value="136">America/Maceio</option>
<option value="137">America/Managua</option>
<option value="138">America/Manaus</option>
<option value="139">America/Marigot</option>
<option value="140">America/Martinique</option>
<option value="141">America/Matamoros</option>
<option value="142">America/Mazatlan</option>
<option value="143">America/Menominee</option>
<option value="144">America/Merida</option>
<option value="145">America/Metlakatla</option>
<option value="146">America/Mexico_City</option>
<option value="147">America/Miquelon</option>
<option value="148">America/Moncton</option>
<option value="149">America/Monterrey</option>
<option value="150">America/Montevideo</option>
<option value="151">America/Montreal</option>
<option value="152">America/Montserrat</option>
<option value="153">America/Nassau</option>
<option value="154">America/New_York</option>
<option value="155">America/Nipigon</option>
<option value="156">America/Nome</option>
<option value="157">America/Noronha</option>
<option value="158">America/North_Dakota/Beulah</option>
<option value="159">America/North_Dakota/Center</option>
<option value="160">America/North_Dakota/New_Salem</option>
<option value="161">America/Nuuk</option>
<option value="162">America/Ojinaga</option>
<option value="163">America/Panama</option>
<option value="164">America/Pangnirtung</option>
<option value="165">America/Paramaribo</option>
<option value="166">America/Phoenix</option>
<option value="167">America/Port-au-Prince</option>
<option value="168">America/Port_of_Spain</option>
<option value="169">America/Porto_Velho</option>
<option value="170">America/Puerto_Rico</option>
<option value="171">America/Punta_Arenas</option>
<option value="172">America/Rainy_River</option>
<option value="173">America/Rankin_Inlet</option>
<option value="174">America/Recife</option>
<option value="175">America/Regina</option>
<option value="176">America/Resolute</option>
<option value="177">America/Rio_Branco</option>
<option value="178">America/Santarem</option>
<option value="179">America/Santiago</option>
<option value="180">America/Santo_Domingo</option>
<option value="181">America/Sao_Paulo</option>
<option value="182">America/Scoresbysund</option>
<option value="183">America/Sitka</option>
<option value="184">America/St_Barthelemy</option>
<option value="185">America/St_Johns</option>
<option value="186">America/St_Kitts</option>
<option value="187">America/St_Lucia</option>
<option value="188">America/St_Thomas</option>
<option value="189">America/St_Vincent</option>
<option value="190">America/Swift_Current</option>
<option value="191">America/Tegucigalpa</option>
<option value="192">America/Thule</option>
<option value="193">America/Thunder_Bay</option>
<option value="194">America/Tijuana</option>
<option value="195">America/Toronto</option>
<option value="196">America/Tortola</option>
<option value="197">America/Vancouver</option>
<option value="198">America/Whitehorse</option>
<option value="199">America/Winnipeg</option>
<option value="200">America/Yakutat</option>
<option value="201">America/Yellowknife</option>
<option value="202">Antarctica/Casey</option>
<option value="203">Antarctica/Davis</option>
<option value="204">Antarctica/DumontDUrville</option>
<option value="205">Antarctica/Macquarie</option>
<option value="206">Antarctica/Mawson</option>
<option value="207">Antarctica/McMurdo</option>
<option value="208">Antarctica/Palmer</option>
<option value="209">Antarctica/Rothera</option>
<option value="210">Antarctica/Syowa</option>
<option value="211">Antarctica/Troll</option>
<option value="212">Antarctica/Vostok</option>
<option value="213">Arctic/Longyearbyen</option>
<option value="214">Asia/Aden</option>
<option value="215">Asia/Almaty</option>
<option value="216">Asia/Amman</option>
<option value="217">Asia/Anadyr</option>
<option value="218">Asia/Aqtau</option>
<option value="219">Asia/Aqtobe</option>
<option value="220">Asia/Ashgabat</option>
<option value="221">Asia/Atyrau</option>
<option value="222">Asia/Baghdad</option>
<option value="223">Asia/Bahrain</option>
<option value="224">Asia/Baku</option>
<option value="225">Asia/Bangkok</option>
<option value="226">Asia/Barnaul</option>
<option value="227">Asia/Beirut</option>
<option value="228">Asia/Bishkek</option>
<option value="229">Asia/Brunei</option>
<option value="230">Asia/Chita</option>
<option value="231">Asia/Choibalsan</option>
<option value="232">Asia/Colombo</option>
<option value="233">Asia/Damascus</option>
<option value="234">Asia/Dhaka</option>
<option value="235">Asia/Dili</option>
<option value="236">Asia/Dubai</option>
<option value="237">Asia/Dushanbe</option>
<option value="238">Asia/Famagusta</option>
<option value="239">Asia/Gaza</option>
<option value="240">Asia/Hebron</option>
<option value="241">Asia/Ho_Chi_Minh</option>
<option value="242">Asia/Hong_Kong</option>
<option value="243">Asia/Hovd</option>
<option value="244">Asia/Irkutsk</option>
<option value="245">Asia/Jakarta</option>
<option value="246">Asia/Jayapura</option>
<option value="247">Asia/Jerusalem</option>
<option value="248">Asia/Kabul</option>
<option value="249">Asia/Kamchatka</option>
<option value="250">Asia/Karachi</option>
<option value="251">Asia/Kathmandu</option>
<option value="252">Asia/Khandyga</option>
<option value="253">Asia/Kolkata</option>
<option value="254">Asia/Krasnoyarsk</option>
<option value="255">Asia/Kuala_Lumpur</option>
<option value="256">Asia/Kuching</option>
<option value="257">Asia/Kuwait</option>
<option value="258">Asia/Macau</option>
<option value="259">Asia/Magadan</option>
<option value="260">Asia/Makassar</option>
<option value="261">Asia/Manila</option>
<option value="262">Asia/Muscat</option>
<option value="263">Asia/Nicosia</option>
<option value="264">Asia/Novokuznetsk</option>
<option value="265">Asia/Novosibirsk</option>
<option value="266">Asia/Omsk</option>
<option value="267">Asia/Oral</option>
<option value="268">Asia/Phnom_Penh</option>
<option value="269">Asia/Pontianak</option>
<option value="270">Asia/Pyongyang</option>
<option value="271">Asia/Qatar</option>
<option value="272">Asia/Qyzylorda</option>
<option value="273">Asia/Riyadh</option>
<option value="274">Asia/Sakhalin</option>
<option value="275">Asia/Samarkand</option>
<option value="276">Asia/Seoul</option>
<option value="277">Asia/Shanghai</option>
<option value="278">Asia/Singapore</option>
<option value="279">Asia/Srednekolymsk</option>
<option value="280">Asia/Taipei</option>
<option value="281">Asia/Tashkent</option>
<option value="282">Asia/Tbilisi</option>
<option value="283">Asia/Tehran</option>
<option value="284">Asia/Thimphu</option>
<option value="285">Asia/Tokyo</option>
<option value="286">Asia/Tomsk</option>
<option value="287">Asia/Ulaanbaatar</option>
<option value="288">Asia/Urumqi</option>
<option value="289">Asia/Ust-Nera</option>
<option value="290">Asia/Vientiane</option>
<option value="291">Asia/Vladivostok</option>
<option value="292">Asia/Yakutsk</option>
<option value="293">Asia/Yangon</option>
<option value="294">Asia/Yekaterinburg</option>
<option value="295">Asia/Yerevan</option>
<option value="296">Atlantic/Azores</option>
<option value="297">Atlantic/Bermuda</option>
<option value="298">Atlantic/Canary</option>
<option value="299">Atlantic/Cape_Verde</option>
<option value="300">Atlantic/Faroe</option>
<option value="301">Atlantic/Madeira</option>
<option value="302">Atlantic/Reykjavik</option>
<option value="303">Atlantic/South_Georgia</option>
<option value="304">Atlantic/Stanley</option>
<option value="305">Atlantic/St_Helena</option>
<option value="306">Australia/Adelaide</option>
<option value="307">Australia/Brisbane</option>
<option value="308">Australia/Broken_Hill</option>
<option value="309">Australia/Currie</option>
<option value="310">Australia/Darwin</option>
<option value="311">Australia/Eucla</option>
<option value="312">Australia/Hobart</option>
<option value="313">Australia/Lindeman</option>
<option value="314">Australia/Lord_Howe</option>
<option value="315">Australia/Melbourne</option>
<option value="316">Australia/Perth</option>
<option value="317">Australia/Sydney</option>
<option value="318">Europe/Amsterdam</option>
<option value="319">Europe/Andorra</option>
<option value="320">Europe/Astrakhan</option>
<option value="321">Europe/Athens</option>
<option value="322">Europe/Belgrade</option>
<option value="323">Europe/Berlin</option>
<option value="324">Europe/Bratislava</option>
<option value="325">Europe/Brussels</option>
<option value="326">Europe/Bucharest</option>
<option value="327">Europe/Budapest</option>
<option value="328">Europe/Busingen</option>
<option value="329">Europe/Chisinau</option>
<option value="330">Europe/Copenhagen</option>
<option value="331">Europe/Dublin</option>
<option value="332">Europe/Gibraltar</option>
<option value="333">Europe/Guernsey</option>
<option value="334">Europe/Helsinki</option>
<option value="335">Europe/Isle_of_Man</option>
<option value="336">Europe/Istanbul</option>
<option value="337">Europe/Jersey</option>
<option value="338">Europe/Kaliningrad</option>
<option value="339">Europe/Kiev</option>
<option value="340">Europe/Kirov</option>
<option value="341">Europe/Lisbon</option>
<option value="342">Europe/Ljubljana</option>
<option value="343">Europe/London</option>
<option value="344">Europe/Luxembourg</option>
<option value="345">Europe/Madrid</option>
<option value="346">Europe/Malta</option>
<option value="347">Europe/Mariehamn</option>
<option value="348">Europe/Minsk</option>
<option value="349">Europe/Monaco</option>
<option value="350">Europe/Moscow</option>
<option value="351">Europe/Oslo</option>
<option value="352">Europe/Paris</option>
<option value="353">Europe/Podgorica</option>
<option value="354">Europe/Prague</option>
<option value="355">Europe/Riga</option>
<option value="356">Europe/Rome</option>
<option value="357">Europe/Samara</option>
<option value="358">Europe/San_Marino</option>
<option value="359">Europe/Sarajevo</option>
<option value="360">Europe/Saratov</option>
<option value="361">Europe/Simferopol</option>
<option value="362">Europe/Skopje</option>
<option value="363">Europe/Sofia</option>
<option value="364">Europe/Stockholm</option>
<option value="365">Europe/Tallinn</option>
<option value="366">Europe/Tirane</option>
<option value="367">Europe/Ulyanovsk</option>
<option value="368">Europe/Uzhgorod</option>
<option value="369">Europe/Vaduz</option>
<option value="370">Europe/Vatican</option>
<option value="371">Europe/Vienna</option>
<option value="372">Europe/Vilnius</option>
<option value="373">Europe/Volgograd</option>
<option value="374">Europe/Warsaw</option>
<option value="375">Europe/Zagreb</option>
<option value="376">Europe/Zaporozhye</option>
<option value="377">Europe/Zurich</option>
<option value="378">Indian/Antananarivo</option>
<option value="379">Indian/Chagos</option>
<option value="380">Indian/Christmas</option>
<option value="381">Indian/Cocos</option>
<option value="382">Indian/Comoro</option>
<option value="383">Indian/Kerguelen</option>
<option value="384">Indian/Mahe</option>
<option value="385">Indian/Maldives</option>
<option value="386">Indian/Mauritius</option>
<option value="387">Indian/Mayotte</option>
<option value="388">Indian/Reunion</option>
<option value="389">Pacific/Apia</option>
<option value="390">Pacific/Auckland</option>
<option value="391">Pacific/Bougainville</option>
<option value="392">Pacific/Chatham</option>
<option value="393">Pacific/Chuuk</option>
<option value="394">Pacific/Easter</option>
<option value="395">Pacific/Efate</option>
<option value="396">Pacific/Enderbury</option>
<option value="397">Pacific/Fakaofo</option>
<option value="398">Pacific/Fiji</option>
<option value="399">Pacific/Funafuti</option>
<option value="400">Pacific/Galapagos</option>
<option value="401">Pacific/Gambier</option>
<option value="402">Pacific/Guadalcanal</option>
<option value="403">Pacific/Guam</option>
<option value="404">Pacific/Honolulu</option>
<option value="405">Pacific/Kiritimati</option>
<option value="406">Pacific/Kosrae</option>
<option value="407">Pacific/Kwajalein</option>
<option value="408">Pacific/Majuro</option>
<option value="409">Pacific/Marquesas</option>
<option value="410">Pacific/Midway</option>
<option value="411">Pacific/Nauru</option>
<option value="412">Pacific/Niue</option>
<option value="413">Pacific/Norfolk</option>
<option value="414">Pacific/Noumea</option>
<option value="415">Pacific/Pago_Pago</option>
<option value="416">Pacific/Palau</option>
<option value="417">Pacific/Pitcairn</option>
<option value="418">Pacific/Pohnpei</option>
<option value="419">Pacific/Port_Moresby</option>
<option value="420">Pacific/Rarotonga</option>
<option value="421">Pacific/Saipan</option>
<option value="422">Pacific/Tahiti</option>
<option value="423">Pacific/Tarawa</option>
<option value="424">Pacific/Tongatapu</option>
<option value="425">Pacific/Wake</option>
<option value="426">Pacific/Wallis</option>
<option value="427">Etc/GMT-0</option>
<option value="428">Etc/GMT-1</option>
<option value="429">Etc/GMT-2</option>
<option value="430">Etc/GMT-3</option>
<option value="431">Etc/GMT-4</option>
<option value="432">Etc/GMT-5</option>
<option value="433">Etc/GMT-6</option>
<option value="434">Etc/GMT-7</option>
<option value="435">Etc/GMT-8</option>
<option value="436">Etc/GMT-9</option>
<option value="437">Etc/GMT-10</option>
<option value="438">Etc/GMT-11</option>
<option value="439">Etc/GMT-12</option>
<option value="440">Etc/GMT-13</option>
<option value="441">Etc/GMT-14</option>
<option value="442">Etc/GMT0</option>
<option value="443">Etc/GMT+0</option>
<option value="444">Etc/GMT+1</option>
<option value="445">Etc/GMT+2</option>
<option value="446">Etc/GMT+3</option>
<option value="447">Etc/GMT+4</option>
<option value="448">Etc/GMT+5</option>
<option value="449">Etc/GMT+6</option>
<option value="450">Etc/GMT+7</option>
<option value="451">Etc/GMT+8</option>
<option value="452">Etc/GMT+9</option>
<option value="453">Etc/GMT+10</option>
<option value="454">Etc/GMT+11</option>
<option value="455">Etc/GMT+12</option>
<option value="456">Etc/UCT</option>
<option value="457">Etc/UTC</option>
<option value="458">Etc/Greenwich</option>
<option value="459">Etc/Universal</option>
<option value="460">Etc/Zulu</option>
)====";
