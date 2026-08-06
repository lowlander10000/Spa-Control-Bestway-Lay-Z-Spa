let state = {};
let socket;
let selectedNetwork = "";
let toastTimer;
let preferredTemperatureUnit = localStorage.getItem("layzspaTemperatureUnit") || "Celsius";

const minTemp = 20;
const maxTemp = 40;
const circumference = 603;


function isFahrenheit() {
  return preferredTemperatureUnit === "Fahrenheit";
}

function temperatureSymbol() {
  return isFahrenheit() ? "°F" : "°C";
}

function displayTemperature(value) {
  if (value === null || value === undefined || value === "--") return "--";
  const celsius = Number(value);
  if (!Number.isFinite(celsius)) return value;
  const converted = isFahrenheit() ? (celsius * 9 / 5) + 32 : celsius;
  return Number.isInteger(converted) ? String(converted) : converted.toFixed(1);
}

function applyTemperatureUnit() {
  document.documentElement.classList.toggle("fahrenheit-mode", isFahrenheit());
  document.querySelectorAll(".unit").forEach(el => el.innerText = temperatureSymbol());
  const select = document.getElementById("temperatureUnit");
  if (select && select.value !== preferredTemperatureUnit) select.value = preferredTemperatureUnit;
  setText("temperature", displayTemperature(state.temperature ?? "--"));
  setText("tempSmall", displayTemperature(state.temperature ?? "--"));
  setText("target", displayTemperature(state.target ?? "--"));
  setText("targetBig", displayTemperature(state.target ?? "--"));
}

async function loadTemperaturePreference() {
  try {
    const response = await fetch("/api/settings/region", { cache: "no-store" });
    if (!response.ok) return;
    const settings = await response.json();
    preferredTemperatureUnit = localStorage.getItem("layzspaTemperatureUnit") || settings.temperatureUnit || "Celsius";
    applyTemperatureUnit();
    render();
  } catch (_) {}
}

function connectWebSocket() {
  socket = new WebSocket("ws://" + location.hostname + ":81/");

  socket.onopen = () => {
    const el = document.getElementById("connectionStatus");
    if (el) { el.className = "ptw-connection live"; el.title = tr("Live verbonden"); el.setAttribute("aria-label", tr("Live verbinding actief")); }
  };

  socket.onmessage = (event) => {
    state = JSON.parse(event.data);

    // Keep the dashboard temperature meter synchronized with the original
    // Celsius/Fahrenheit button on the spa control panel.
    if (typeof state.fahrenheit === "boolean") {
      const unitFromSpa = state.fahrenheit ? "Fahrenheit" : "Celsius";
      if (preferredTemperatureUnit !== unitFromSpa) {
        preferredTemperatureUnit = unitFromSpa;
        localStorage.setItem("layzspaTemperatureUnit", preferredTemperatureUnit);
      }
    }

    render();
  };

  socket.onclose = () => {
    resetLiveControls();
    const el = document.getElementById("connectionStatus");
    if (el) { el.className = "ptw-connection offline"; el.title = tr("Offline"); el.setAttribute("aria-label", tr("Geen live verbinding")); }
    setTimeout(connectWebSocket, 2000);
  };
}

function setText(id, value) {
  const el = document.getElementById(id);
  if (el) el.innerText = value;
}

function setActive(id, active) {
  const el = document.getElementById(id);
  if (!el) return;

  el.classList.toggle("active", active);

  const label = el.querySelector("small");
  if (label) label.innerText = active ? tr("AAN") : tr("UIT");
}

function setDot(id, ok, goodText, badText) {
  const el = document.getElementById(id);
  if (!el) return;

  el.innerText = ok ? goodText : badText;
  el.className = "dot " + (ok ? "green" : "red");
}

function renderGauge() {
  const currentCard = document.getElementById("currentTemperatureCard");
  const targetCard = document.getElementById("targetTemperatureCard");
  const temp = Number(state.temperature);
  const target = Number(state.target);
  const activelyHeating = !!state.heaterActive;
  const targetReached = Number.isFinite(temp) && Number.isFinite(target) && target > 0 && temp >= target;

  [currentCard, targetCard].forEach(card => {
    if (!card) return;
    card.classList.toggle("is-heating", activelyHeating);
    card.classList.toggle("is-ready", !activelyHeating && targetReached);
    card.classList.toggle("is-cold", !activelyHeating && !targetReached);
  });

  const mode = activelyHeating
    ? tr("Verwarmen")
    : (state.heater && targetReached)
      ? tr("Water is op temperatuur")
      : state.heater
        ? tr("Heater stand-by")
        : tr("Stand-by");
  const modeEl = document.getElementById("spaMode");
  if (modeEl) {
    const label = modeEl.querySelector("span:first-child");
    if (label) label.innerText = mode;
    else modeEl.innerText = mode;
    const check = modeEl.querySelector(".status-check");
    if (check) check.style.display = targetReached ? "inline-grid" : "none";
  }

  setText("targetHeatStatus", state.heater ? tr("Verwarming: Aan") : tr("Verwarming: Uit"));
}

function resetLiveControls() {
  ["heater", "filter", "bubbles", "jets", "power"].forEach(id => setActive(id, false));
  setControlActive("controlPower", false, tr("Uit"));
  setControlActive("controlLock", false, tr("Vrij"));
  setControlActive("controlUnit", false, "°C");
  setControlActive("controlTimer", false, tr("Uit"));
  setControlActive("controlHeater", false, tr("Uit"));
  setControlActive("controlFilter", false, tr("Uit"));
  setControlActive("controlBubbles", false, tr("Uit"));
  setControlActive("controlJets", false, tr("Uit"));
  document.getElementById("controlHeater")?.classList.remove("heating-now");
}

function render() {
  setText("temperature", displayTemperature(state.temperature ?? "--"));
  setText("tempSmall", displayTemperature(state.temperature ?? "--"));
  setText("target", displayTemperature(state.target ?? "--"));
  setText("targetBig", displayTemperature(state.target ?? "--"));
  applyTemperatureUnit();
  setText("ip", state.ip ?? "--");
  setText("uptime", state.uptime ?? "--");

  if (!state.dataValid) {
    resetLiveControls();
    renderGauge();
    return;
  }

  setActive("heater", state.heater);
  const heaterButton = document.getElementById("heater");
  if (heaterButton) {
    heaterButton.classList.toggle("heating-now", !!state.heaterActive);
    const label = heaterButton.querySelector("small");
    if (label) label.innerText = state.heaterActive ? tr("VERWARMT") : (state.heater ? tr("AAN") : tr("UIT"));
  }
  setActive("filter", state.filter);
  setActive("bubbles", state.bubbles);
  setActive("jets", state.jets);
  setActive("power", state.power);
  setControlActive("controlPower", state.power, state.power ? tr("Aan") : tr("Uit"));
  setControlActive("controlLock", state.locked, state.locked ? tr("Vergrendeld") : tr("Vrij"));
  setControlActive("controlUnit", state.fahrenheit, state.fahrenheit ? "°F" : "°C");
  setControlActive("controlTimer", state.timerActive, state.timerActive ? tr("Actief") : tr("Uit"));
  setControlActive("controlHeater", state.heater, state.heaterActive ? tr("Verwarmt") : (state.heater ? tr("Aan") : tr("Uit")));
  document.getElementById("controlHeater")?.classList.toggle("heating-now", !!state.heaterActive);
  setControlActive("controlFilter", state.filter, state.filter ? tr("Aan") : tr("Uit"));
  setControlActive("controlBubbles", state.bubbles, state.bubbles ? tr("Aan") : tr("Uit"));
  setControlActive("controlJets", state.jets, state.jets ? tr("Aan") : tr("Uit"));

  setDot("wifiStatus", !!state.wifi, tr("Verbonden"), tr("Offline"));
  setDot("mqttStatus", !!state.mqtt, tr("Online"), tr("Offline"));

  setDot(
    "settingsWifiStatus",
    !!state.wifi,
    "Verbonden",
    "Offline"
  );

  setText("settingsIp", state.ip ?? "--");
  setText("settingsUptime", state.uptime ?? "--");
  setText(
    "settingsHeap",
    state.heap ? formatBytes(state.heap) : "--"
  );

  setText("infoWifi", state.wifi ? tr("Verbonden") : tr("Offline"));
  setText("infoRssi", state.wifi && Number.isFinite(Number(state.rssi)) ? `${state.rssi} dBm` : "--");
  setText("infoMqtt", state.mqtt ? tr("Verbonden") : tr("Offline"));
  setText("infoIp", state.ip ?? "--");
  setText("infoUptime", state.uptime ?? "--");
  setText("infoHeap", state.heap ? formatBytes(state.heap) : "--");
  setText("infoChip", state.chipId ? String(state.chipId).toUpperCase() : "--");

  renderGauge();
}

function setControlActive(id, active, text) {
  const el = document.getElementById(id);
  if (!el) return;
  el.classList.toggle("active", !!active);
  const small = el.querySelector("small");
  if (small) small.innerText = text;
}

function pressSpaButton(button) {
  sendCommand("press:" + button);
}


const navDefaults = {dashboard:true,control:true,planner:true,history:true,energy:true,logs:true,maintenance:true,settings:true,info:true};
const dashboardSectionDefaults = {status:true,currentTemperature:true,targetTemperature:true,weather:true,controls:true,summary:true,insight:true,system:true};
const dashboardOrderDefaults = ["status","currentTemperature","targetTemperature","weather","controls","summary","insight","system"];
function getDashboardOrder(){
  try{
    const stored=JSON.parse(localStorage.getItem("layzspaDashboardOrder")||"[]");
    const valid=stored.filter(k=>dashboardOrderDefaults.includes(k));
    dashboardOrderDefaults.forEach(k=>{if(!valid.includes(k))valid.push(k);});
    return valid;
  }catch(_){return [...dashboardOrderDefaults];}
}
function applyDashboardOrder(){
  const dashboard=document.getElementById("dashboardView");
  if(!dashboard)return;
  getDashboardOrder().forEach(key=>{
    const card=dashboard.querySelector(`[data-dashboard-card="${key}"]`);
    if(card)dashboard.appendChild(card);
  });
}
function loadDashboardOrderEditor(){
  const list=document.getElementById("dashboardOrderList");
  if(!list)return;
  getDashboardOrder().forEach(key=>{
    const item=list.querySelector(`[data-dashboard-order="${key}"]`);
    if(item)list.appendChild(item);
  });
  initializeDashboardOrderDrag();
}
function initializePointerSortable(list, itemSelector, handleSelector){
  if(!list||list.dataset.pointerSortReady==="true")return;
  list.dataset.pointerSortReady="true";
  let dragged=null;
  let pointerId=null;
  let startY=0;
  let active=false;

  list.querySelectorAll(itemSelector).forEach(item=>item.setAttribute("draggable","false"));

  list.addEventListener("pointerdown",event=>{
    const handle=event.target.closest(handleSelector);
    const item=event.target.closest(itemSelector);
    if(!handle||!item||!list.contains(item))return;
    dragged=item;
    pointerId=event.pointerId;
    startY=event.clientY;
    active=false;
    handle.setPointerCapture?.(pointerId);
    event.preventDefault();
  });

  list.addEventListener("pointermove",event=>{
    if(!dragged||event.pointerId!==pointerId)return;
    if(!active&&Math.abs(event.clientY-startY)<4)return;
    active=true;
    dragged.classList.add("dragging");
    const siblings=[...list.querySelectorAll(`${itemSelector}:not(.dragging)` )];
    let placed=false;
    for(const sibling of siblings){
      const box=sibling.getBoundingClientRect();
      if(event.clientY<box.top+box.height/2){
        list.insertBefore(dragged,sibling);
        placed=true;
        break;
      }
    }
    if(!placed)list.appendChild(dragged);
    event.preventDefault();
  });

  const finish=event=>{
    if(!dragged||event.pointerId!==pointerId)return;
    dragged.classList.remove("dragging");
    dragged=null;
    pointerId=null;
    active=false;
    event.preventDefault();
  };
  list.addEventListener("pointerup",finish);
  list.addEventListener("pointercancel",finish);
}

function initializeDashboardOrderDrag(){
  initializePointerSortable(document.getElementById("dashboardOrderList"), ".dashboard-order-item", ".drag-handle");
}
function saveDashboardOrder(){
  const list=document.getElementById("dashboardOrderList");
  if(!list)return;
  const order=[...list.querySelectorAll("[data-dashboard-order]")].map(el=>el.dataset.dashboardOrder);
  localStorage.setItem("layzspaDashboardOrder",JSON.stringify(order));
  applyDashboardOrder();
  showToast(tr("Dashboardvolgorde opgeslagen"));
}
function resetDashboardOrder(){
  localStorage.removeItem("layzspaDashboardOrder");
  loadDashboardOrderEditor();
  applyDashboardOrder();
  showToast(tr("Standaardvolgorde hersteld"));
}

const dashboardButtonOrderDefaults = ["heater","filter","bubbles","jets","power"];
function getDashboardButtonOrder(){
  try{
    const stored=JSON.parse(localStorage.getItem("layzspaDashboardButtonOrder")||"[]");
    const valid=stored.filter(k=>dashboardButtonOrderDefaults.includes(k));
    dashboardButtonOrderDefaults.forEach(k=>{if(!valid.includes(k))valid.push(k);});
    return valid;
  }catch(_){return [...dashboardButtonOrderDefaults];}
}
function applyDashboardButtonOrder(){
  const grid=document.querySelector(".premium-device-grid");
  if(!grid)return;
  getDashboardButtonOrder().forEach(key=>{
    const button=grid.querySelector(`[data-dashboard-button-order="${key}"]`);
    if(button)grid.appendChild(button);
  });
}
function loadDashboardButtonOrderEditor(){
  const list=document.getElementById("dashboardButtonOrderList");
  if(!list)return;
  getDashboardButtonOrder().forEach(key=>{
    const item=list.querySelector(`[data-dashboard-button-order-item="${key}"]`);
    if(item)list.appendChild(item);
  });
  initializeDashboardButtonOrderDrag();
}
function initializeDashboardButtonOrderDrag(){
  initializePointerSortable(document.getElementById("dashboardButtonOrderList"), ".dashboard-order-item", ".drag-handle");
}
function saveDashboardButtonOrder(){
  const list=document.getElementById("dashboardButtonOrderList");
  if(!list)return;
  const order=[...list.querySelectorAll("[data-dashboard-button-order-item]")].map(el=>el.dataset.dashboardButtonOrderItem);
  localStorage.setItem("layzspaDashboardButtonOrder",JSON.stringify(order));
  applyDashboardButtonOrder();
  showToast(tr("Knoppenvolgorde opgeslagen"));
}
function resetDashboardButtonOrder(){
  localStorage.removeItem("layzspaDashboardButtonOrder");
  loadDashboardButtonOrderEditor();
  applyDashboardButtonOrder();
  showToast(tr("Standaardvolgorde hersteld"));
}

function readLocalConfig(key, defaults){ try{return {...defaults,...JSON.parse(localStorage.getItem(key)||"{}")};}catch(_){return {...defaults};} }
const navigationOrderDefaults=["dashboard","control","planner","history","energy","logs","maintenance","settings","info"];
function getNavigationOrder(){
  try{
    const stored=JSON.parse(localStorage.getItem("layzspaNavigationOrder")||"[]");
    const valid=stored.filter(name=>navigationOrderDefaults.includes(name));
    navigationOrderDefaults.forEach(name=>{if(!valid.includes(name))valid.push(name);});
    return valid;
  }catch(_){return [...navigationOrderDefaults];}
}
function applyNavigationOrder(){
  const nav=document.getElementById("bottomNav");
  if(nav)getNavigationOrder().forEach(name=>{const button=document.getElementById(`nav${name.charAt(0).toUpperCase()+name.slice(1)}`);if(button)nav.appendChild(button);});
}
function loadNavigationOrderEditor(){
  const list=document.getElementById("navigationOrderList");
  if(!list)return;
  getNavigationOrder().forEach(name=>{const item=list.querySelector(`[data-nav-order="${name}"]`);if(item)list.appendChild(item);});
  initializeNavigationOrderDrag();
}
function initializeNavigationOrderDrag(){
  initializePointerSortable(document.getElementById("navigationOrderList"), ".navigation-order-item", ".drag-handle");
}
function resetNavigationOrder(){
  localStorage.removeItem("layzspaNavigationOrder");
  loadNavigationOrderEditor();
  applyNavigationOrder();
  showToast(tr("Standaardvolgorde hersteld"));
}
function applyNavigationSettings(){
  const cfg=readLocalConfig("layzspaNavTabs",navDefaults);
  const enabled=localStorage.getItem("layzspaBottomNavEnabled")!=="false";
  applyNavigationOrder();
  document.getElementById("bottomNav")?.classList.toggle("bottom-nav-disabled",!enabled);
  document.body.classList.toggle("without-bottom-nav",!enabled);
  const toggle=document.getElementById("bottomNavEnabled"); if(toggle) toggle.checked=enabled;
  Object.keys(navDefaults).forEach(name=>{const el=document.getElementById(`nav${name.charAt(0).toUpperCase()+name.slice(1)}`);if(el)el.classList.toggle("nav-user-hidden",cfg[name]===false);});
  document.querySelectorAll("[data-nav-setting]").forEach(i=>i.checked=cfg[i.dataset.navSetting]!==false);
}
function saveNavigationSettings(){
  const cfg={...navDefaults};
  document.querySelectorAll("[data-nav-setting]").forEach(i=>cfg[i.dataset.navSetting]=i.checked);
  const list=document.getElementById("navigationOrderList");
  const order=list?[...list.querySelectorAll("[data-nav-order]")].map(el=>el.dataset.navOrder):getNavigationOrder();
  localStorage.setItem("layzspaNavTabs",JSON.stringify(cfg));
  localStorage.setItem("layzspaNavigationOrder",JSON.stringify(order));
  localStorage.setItem("layzspaBottomNavEnabled",document.getElementById("bottomNavEnabled")?.checked!==false?"true":"false");
  applyNavigationSettings(); showToast(tr("Navigatie opgeslagen"));
}
function getDashboardSections(){
  const cfg=readLocalConfig("layzspaDashboardSections",dashboardSectionDefaults);
  // Oude instelling uit eerdere versies blijven respecteren.
  if(cfg.gauge===false){cfg.currentTemperature=false;cfg.targetTemperature=false;}
  return cfg;
}
function applyDashboardSections(){
  const cfg=getDashboardSections();
  const map={
    status:".dashboard-status-strip",
    currentTemperature:"#currentTemperatureCard",
    targetTemperature:"#targetTemperatureCard",
    weather:"#weatherDashboardCard",
    controls:".premium-device-grid",
    summary:"#smartSummary",
    insight:"#smartInsightCard",
    system:".premium-system-list"
  };
  Object.entries(map).forEach(([k,sel])=>document.querySelector(sel)?.classList.toggle("dashboard-section-hidden",cfg[k]===false));
  document.querySelectorAll("[data-dashboard-section-setting]").forEach(i=>i.checked=cfg[i.dataset.dashboardSectionSetting]!==false);
}
function saveDashboardLayout(){
  saveDashboardButtons(false);
  const cfg={...dashboardSectionDefaults};
  document.querySelectorAll("[data-dashboard-section-setting]").forEach(i=>cfg[i.dataset.dashboardSectionSetting]=i.checked);
  localStorage.setItem("layzspaDashboardSections",JSON.stringify(cfg));
  applyDashboardSections();
  showToast(tr("Dashboardindeling opgeslagen"));
}
function setAllDashboardVisibility(enabled){
  document.querySelectorAll("[data-dashboard-setting],[data-dashboard-section-setting]").forEach(i=>i.checked=enabled);
}
function resetDashboardVisibility(){
  document.querySelectorAll("[data-dashboard-setting],[data-dashboard-section-setting]").forEach(i=>i.checked=true);
  localStorage.removeItem("layzspaDashboardButtons");
  localStorage.removeItem("layzspaDashboardSections");
  applyDashboardButtons();
  applyDashboardSections();
  showToast(tr("Standaardindeling hersteld"));
}
function toggleAppMenu(){const open=!document.getElementById("appMenu")?.classList.contains("open");document.getElementById("appMenu")?.classList.toggle("open",open);document.getElementById("appMenuBackdrop")?.classList.toggle("open",open);document.getElementById("appMenu")?.setAttribute("aria-hidden",open?"false":"true");}
function closeAppMenu(){document.getElementById("appMenu")?.classList.remove("open");document.getElementById("appMenuBackdrop")?.classList.remove("open");document.getElementById("appMenu")?.setAttribute("aria-hidden","true");}
function openMenuView(name){closeAppMenu();showView(name);}

const dashboardButtonDefaults = { power: true, heater: true, filter: true, bubbles: true, jets: true };

function getDashboardButtons() {
  try {
    return { ...dashboardButtonDefaults, ...JSON.parse(localStorage.getItem("layzspaDashboardButtons") || "{}") };
  } catch (_) {
    return { ...dashboardButtonDefaults };
  }
}

function applyDashboardButtons() {
  const cfg = getDashboardButtons();
  document.querySelectorAll("[data-dashboard-button]").forEach(el => {
    el.classList.toggle("dashboard-button-hidden", cfg[el.dataset.dashboardButton] === false);
  });
}

function loadDashboardButtons() {
  const cfg = getDashboardButtons();
  document.querySelectorAll("[data-dashboard-setting]").forEach(input => {
    input.checked = cfg[input.dataset.dashboardSetting] !== false;
  });
  applyDashboardButtons();
}

function saveDashboardButtons(showMessage = true) {
  const cfg = {};
  document.querySelectorAll("[data-dashboard-setting]").forEach(input => {
    cfg[input.dataset.dashboardSetting] = input.checked;
  });
  localStorage.setItem("layzspaDashboardButtons", JSON.stringify(cfg));
  applyDashboardButtons();
  if (showMessage) showToast(tr("Dashboardknoppen opgeslagen"));
}

function sendCommand(command) {
  if (socket && socket.readyState === WebSocket.OPEN) {
    socket.send(command);
  }
}

function toggleDevice(device) {
  sendCommand("toggle:" + device);
}

function setTarget(change) {
  sendCommand(change > 0 ? "target:+" : "target:-");
}

function showView(viewName) {
  const titles = {
    dashboard: "Bestway Lay-Z-Spa",
    planner: "Planner",
    history: "Historie",
    energy: "Energie",
    logs: "Diagnostiek",
    settings: "Instellingen",
    hardware: "Hardware",
    control: "Bedieningspaneel",
    maintenance: "Onderhoud",
    personalization: "Interface aanpassen",
    info: "Informatie"
  };

  document.querySelectorAll(".view").forEach((view) => {
    view.classList.toggle("active", view.id === `${viewName}View`);
  });

  document.querySelectorAll(".bottom-nav button").forEach((button) => {
    button.classList.toggle("selected", button.id === `nav${viewName.charAt(0).toUpperCase()}${viewName.slice(1)}`);
  });

  setText("pageTitle", titles[viewName] || "Bestway Lay-Z-Spa");

  if (viewName === "settings") {
    loadWifiStatus();
    loadMqttSettings();
    loadRegionalSettings();
    loadWeatherSettings();
    loadSystemStatus();
  } else if (viewName === "info") {
    // De Info-pagina gebruikt systeemvelden die door /api/wifi/status worden gevuld.
    // Haal ze bij het openen altijd opnieuw op, ook als Instellingen nog niet is bezocht.
    loadWifiStatus();
    render();
  } else if (viewName === "hardware") {
    loadHardwareSettings();
  } else if (viewName === "maintenance") {
    loadMaintenance();
  } else if (viewName === "control") {
    // Bedieningspaneel blijft een zuiver bedieningsscherm.
  } else if (viewName === "personalization") {
    loadDashboardButtons();
    applyDashboardButtonOrder();
    loadDashboardButtonOrderEditor();
    applyDashboardSections();
    applyNavigationSettings();
    loadNavigationOrderEditor();
  } else if (viewName === "planner") {
    loadSchedules();
  } else if (viewName === "history") {
    loadHistory();
  } else if (viewName === "energy") {
    loadEnergy();
  } else if (viewName === "logs") {
    loadDiagnostics();
    loadLogs();
  }

  window.scrollTo({ top: 0, behavior: "smooth" });
}


async function loadWifiStatus() {
  try {
    const response = await fetch("/api/wifi/status", {
      cache: "no-store"
    });

    if (!response.ok) {
      throw new Error("WiFi-status niet beschikbaar");
    }

    const wifi = await response.json();

    setDot(
      "settingsWifiStatus",
      !!wifi.connected,
      tr("Verbonden"),
      tr("Offline")
    );

    setText("settingsSsid", wifi.ssid || "--");
    setText("settingsIp", wifi.ip || "--");
    setText(
      "settingsRssi",
      wifi.connected ? `${wifi.rssi} dBm` : "--"
    );

    setText("settingsHostname", wifi.hostname || "--");
    setText("settingsMac", wifi.mac || "--");
    setText("settingsApActive", wifi.apMode ? tr("Ja") : tr("Nee"));
    setText("settingsApSsid", wifi.apSsid || "--");
    setText("settingsApIp", wifi.apIp || "--");

    setText("settingsFirmware", wifi.firmware || "--");
    setText("settingsCpu", wifi.cpu ? `${wifi.cpu} MHz` : "--");
    setText("settingsFlash", wifi.flash || "--");
    setText("settingsFs", wifi.fs || "--");
    setText("settingsResetReason", wifi.resetReason || "--");
  } catch (error) {
    showToast(error.message, true);
  }
}

async function openWifiModal() {
  const modal = document.getElementById("wifiModal");

  modal.classList.add("open");
  modal.setAttribute("aria-hidden", "false");
  document.body.style.overflow = "hidden";

  await scanWifiNetworks();
}

function closeWifiModal() {
  const modal = document.getElementById("wifiModal");

  modal.classList.remove("open");
  modal.setAttribute("aria-hidden", "true");
  document.body.style.overflow = "";

  resetWifiModal();
}

function resetWifiModal() {
  selectedNetwork = "";

  document
    .getElementById("wifiForm")
    .classList.add("hidden");

  document
    .getElementById("wifiNetworks")
    .classList.remove("hidden");

  document.getElementById("selectedSsid").value = "";
  document.getElementById("wifiPassword").value = "";
  document.getElementById("wifiIpMode").value = "dhcp";
  ["wifiStaticIp","wifiGateway","wifiDns1","wifiDns2"].forEach(id => document.getElementById(id).value = "");
  document.getElementById("wifiSubnet").value = "255.255.255.0";
  toggleWifiStaticFields();
}
async function scanWifiNetworks() {
  const scanState = document.getElementById("wifiScanState");
  const list = document.getElementById("wifiNetworks");
  const form = document.getElementById("wifiForm");

  form.classList.add("hidden");
  list.classList.remove("hidden");
  list.innerHTML = "";

  scanState.classList.remove("hidden");
  scanState.innerText = tr("Netwerken zoeken...");

  const startedAt = Date.now();
  const timeoutMs = 20000;

  try {
    while (Date.now() - startedAt < timeoutMs) {
      const response = await fetch("/api/wifi/scan", {
        cache: "no-store"
      });

      const result = await response.json();

      if (
        response.status === 202 &&
        result.status === "scanning"
      ) {
        await sleep(400);
        continue;
      }

      if (!response.ok) {
        throw new Error(
          result.error || tr("Scannen mislukt")
        );
      }

      const networks = result;

      scanState.classList.add("hidden");

      if (
        !Array.isArray(networks) ||
        networks.length === 0
      ) {
        scanState.classList.remove("hidden");
        scanState.innerText = tr("Geen netwerken gevonden.");
        return;
      }

      networks
        .sort(
          (a, b) =>
            Number(b.rssi) - Number(a.rssi)
        )
        .forEach((network) => {
          const button =
            document.createElement("button");

          button.type = "button";
          button.className = "network-item";

          const security = network.encrypted
            ? tr("Beveiligd")
            : tr("Open netwerk");

          button.innerHTML = `
            <span class="network-name">
              <b>${escapeHtml(
                network.ssid || tr("Verborgen netwerk")
              )}</b>
              <small>${security}</small>
            </span>
            <span class="signal">
              ${signalBars(Number(network.rssi))}
              ${Number(network.rssi)} dBm
            </span>
          `;

          button.addEventListener("click", () => {
            selectWifiNetwork(network);
          });

          list.appendChild(button);
        });

      return;
    }

    throw new Error(tr("WiFi-scan duurde te lang"));
  } catch (error) {
    scanState.classList.remove("hidden");
    scanState.innerText = error.message;

    showToast(error.message, true);
  }
}

function selectWifiNetwork(network) {
  selectedNetwork = network.ssid || "";

  document.getElementById("selectedSsid").value =
    selectedNetwork;

  document
    .getElementById("wifiNetworks")
    .classList.add("hidden");

  document
    .getElementById("wifiScanState")
    .classList.add("hidden");

  document
    .getElementById("wifiForm")
    .classList.remove("hidden");

  const password =
    document.getElementById("wifiPassword");

  password.required = !!network.encrypted;

  password.placeholder = network.encrypted
    ? tr("WiFi-wachtwoord")
    : tr("Niet nodig voor open netwerk");

  password.focus();
}

function backToNetworks() {
  document
    .getElementById("wifiForm")
    .classList.add("hidden");

  document
    .getElementById("wifiNetworks")
    .classList.remove("hidden");
}


function toggleWifiStaticFields() {
  const isStatic = document.getElementById("wifiIpMode")?.value === "static";
  const fields = document.getElementById("wifiStaticFields");
  if (!fields) return;
  fields.classList.toggle("hidden", !isStatic);
  ["wifiStaticIp", "wifiGateway", "wifiSubnet"].forEach(id => {
    const input = document.getElementById(id);
    if (input) input.required = isStatic;
  });
}

function isValidIpv4(value) {
  const parts = String(value || "").trim().split(".");
  return parts.length === 4 && parts.every(part => /^\d{1,3}$/.test(part) && Number(part) >= 0 && Number(part) <= 255);
}

async function connectSelectedWifi(event) {
  event.preventDefault();

  const password =
    document.getElementById("wifiPassword").value;

  const button =
    document.getElementById("wifiConnectButton");

  if (!selectedNetwork) {
    showToast(tr("Kies eerst een netwerk."), true);
    return;
  }

  button.disabled = true;
  button.innerText = tr("Verbinden...");

  const mode = document.getElementById("wifiIpMode").value;
  const ip = document.getElementById("wifiStaticIp").value.trim();
  const gateway = document.getElementById("wifiGateway").value.trim();
  const subnet = document.getElementById("wifiSubnet").value.trim();
  const dns1 = document.getElementById("wifiDns1").value.trim();
  const dns2 = document.getElementById("wifiDns2").value.trim();

  if (mode === "static" && (![ip, gateway, subnet].every(isValidIpv4) || (dns1 && !isValidIpv4(dns1)) || (dns2 && !isValidIpv4(dns2)))) {
    showToast(tr("Controleer de vaste IP-instellingen."), true);
    return;
  }

  const body = new URLSearchParams({
    ssid: selectedNetwork,
    password,
    mode,
    ip,
    gateway,
    subnet,
    dns1,
    dns2
  });

  try {
    const response = await fetch(
      "/api/wifi/connect",
      {
        method: "POST",
        headers: {
          "Content-Type":
            "application/x-www-form-urlencoded;charset=UTF-8"
        },
        body: body.toString()
      }
    );

    const result = await response.json();

    if (!response.ok || !result.ok) {
      throw new Error(
        result.error ||
          tr("Verbinden kon niet starten")
      );
    }

    const finalStatus =
      await waitForWifiConnection(20000);

    if (!finalStatus.connected) {
      throw new Error(
        finalStatus.error ||
          tr("Verbinden mislukt. Controleer het wachtwoord.")
      );
    }

    showToast(
      `Verbonden. Nieuw IP: ${
        finalStatus.ip || tr("onbekend")
      }`
    );

    setTimeout(() => {
      closeWifiModal();
      loadWifiStatus();
    }, 700);
  } catch (error) {
    showToast(error.message, true);
  } finally {
    button.disabled = false;
    button.innerText = tr("Verbinden");
  }
}

async function forgetWifi() {
  const confirmed = confirm(
    tr("Weet je zeker dat je de opgeslagen WiFi-instellingen wilt wissen?") + "\n\n" +
      tr("De ESP schakelt daarna terug naar de setupmodus.")
  );

  if (!confirmed) {
    return;
  }

  try {
    const response = await fetch(
      "/api/wifi/forget",
      {
        method: "POST"
      }
    );

    const result = await response.json();

    if (!response.ok || !result.ok) {
      throw new Error(
        result.error ||
          "WiFi-instellingen konden niet worden gewist."
      );
    }

    showToast(tr("WiFi-instellingen gewist."));

    setDot(
      "settingsWifiStatus",
      false,
      tr("Verbonden"),
      tr("Offline")
    );

    setText("settingsSsid", "--");
    setText("settingsIp", "--");
    setText("settingsRssi", "--");

    setTimeout(() => {
      loadWifiStatus();
    }, 1000);
  } catch (error) {
    showToast(error.message, true);
  }
}
async function waitForWifiConnection(timeoutMs) {
  const startedAt = Date.now();

  while (Date.now() - startedAt < timeoutMs) {
    await sleep(500);

    const response = await fetch(
      "/api/wifi/status",
      {
        cache: "no-store"
      }
    );

    if (!response.ok) {
      continue;
    }

    const status = await response.json();

    if (
      status.connected ||
      status.status === "connected"
    ) {
      return status;
    }

    if (status.status === "failed") {
      return status;
    }
  }

  return {
    connected: false,
    status: "failed",
    error: "Verbindingstime-out"
  };
}


async function restartEsp() {
  if (!confirm(tr("Weet je zeker dat je de ESP opnieuw wilt opstarten?"))) return;

  try {
    await fetch("/api/restart", { method: "POST" });
    showToast(tr("ESP wordt opnieuw opgestart..."));

    setTimeout(() => {
      location.reload();
    }, 8000);
  } catch (error) {
    showToast(tr("Herstart mislukt."), true);
  }
}


function startOtaUpload() {
  const file=document.getElementById("otaFile").files[0];
  if(!file){showToast(tr("Kies eerst een firmwarebestand."),true);return;}

  const xhr=new XMLHttpRequest();
  const progress=document.getElementById("otaProgress");
  const percent=document.getElementById("otaPercent");

  xhr.upload.onprogress=(e)=>{
    if(!e.lengthComputable) return;
    const p=Math.round(e.loaded*100/e.total);
    progress.value=p;
    percent.innerText=p+"%";
  };

  xhr.onload=()=>{
    if(xhr.status===200){
      progress.value=100;
      percent.innerText="100%";
      showToast(tr("Firmware geüpload. ESP start opnieuw..."));
      setTimeout(()=>location.reload(),12000);
    }else{
      showToast(tr("OTA-update mislukt."),true);
    }
  };

  xhr.onerror=()=>showToast(tr("Uploadfout."),true);

  const form=new FormData();
  form.append("update",file);
  xhr.open("POST","/api/ota");
  xhr.send(form);
}


function startFilesystemUpload() {
  const file=document.getElementById("fsOtaFile").files[0];
  if(!file){showToast(tr("Kies eerst littlefs.bin."),true);return;}
  if(!file.name.toLowerCase().endsWith(".bin")){showToast(tr("Kies een geldig .bin-bestand."),true);return;}

  const xhr=new XMLHttpRequest();
  const progress=document.getElementById("fsOtaProgress");
  const percent=document.getElementById("fsOtaPercent");

  xhr.upload.onprogress=(e)=>{
    if(!e.lengthComputable) return;
    const p=Math.round(e.loaded*100/e.total);
    progress.value=p;
    percent.innerText=p+"%";
  };

  xhr.onload=()=>{
    console.log("LittleFS OTA HTTP status:", xhr.status);
    console.log("LittleFS OTA response:", xhr.responseText);
    if(xhr.status===200){
      progress.value=100;
      percent.innerText="100%";
      showToast(tr("LittleFS geüpload. ESP start opnieuw..."));
      setTimeout(()=>location.reload(),15000);
    }else{
      let message=tr("LittleFS-update mislukt.");
      try { message=JSON.parse(xhr.responseText).error || message; } catch (_) {}
      showToast(message,true);
    }
  };

  xhr.onerror=()=>{
    console.error("LittleFS OTA netwerkfout", {status:xhr.status, response:xhr.responseText});
    showToast(tr("Uploadfout tijdens LittleFS-update."),true);
  };
  xhr.ontimeout=()=>{
    console.error("LittleFS OTA timeout", {status:xhr.status, response:xhr.responseText});
    showToast(tr("LittleFS-upload timeout na 180 seconden."),true);
  };
  xhr.onabort=()=>{
    console.warn("LittleFS OTA door browser afgebroken");
    showToast(tr("LittleFS-upload afgebroken."),true);
  };

  const form=new FormData();
  form.append("update",file);
  xhr.open("POST","/api/ota/filesystem");
  xhr.timeout=180000;
  xhr.send(form);
}


function sleep(milliseconds) {
  return new Promise((resolve) => {
    setTimeout(resolve, milliseconds);
  });
}

function signalBars(rssi) {
  if (rssi >= -55) return "▮▮▮▮";
  if (rssi >= -67) return "▮▮▮▯";
  if (rssi >= -75) return "▮▮▯▯";

  return "▮▯▯▯";
}

function formatBytes(bytes) {
  const value = Number(bytes);

  if (!Number.isFinite(value)) {
    return "--";
  }

  if (value < 1024) {
    return `${value} B`;
  }

  return `${(value / 1024).toFixed(1)} KB`;
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function showToast(message, isError = false) {
  const toast =
    document.getElementById("toast");

  toast.innerText = tr(message);

  toast.className =
    "toast show" +
    (isError ? " error" : "");

  clearTimeout(toastTimer);

  toastTimer = setTimeout(() => {
    toast.className = "toast";
  }, 3500);
}

document
  .getElementById("wifiModal")
  .addEventListener("click", (event) => {
    if (event.target.id === "wifiModal") {
      closeWifiModal();
    }
  });

if ("serviceWorker" in navigator) {
  navigator.serviceWorker.register("/sw.js");
}

loadTemperaturePreference();
connectWebSocket();



/* v3.2.0 RC3 - MQTT instellingen standaard ingeklapt */
function toggleMqttSettings(forceOpen){
  const panel=document.getElementById("mqttSettingsPanel");
  const button=document.getElementById("mqttSettingsToggle");
  if(!panel||!button)return;
  const open=typeof forceOpen==="boolean"?forceOpen:panel.hasAttribute("hidden");
  panel.toggleAttribute("hidden",!open);
  button.classList.toggle("open",open);
  button.setAttribute("aria-expanded",open?"true":"false");
}

const mqttPublicationDefaults={Temperature:"temperature",Target:"target/state",Power:"power/state",Heater:"heater/state",HeatingActive:"heater/active",Filter:"filter/state",Bubbles:"bubbles/state",Jets:"jets/state",Locked:"lock/state",Connected:"spa/connected",Ready:"spa/ready",Rssi:"system/rssi",Heap:"system/free_heap",Uptime:"system/uptime",Firmware:"system/firmware",Ip:"system/ip",Json:"state",Maintenance:"maintenance"};
async function loadMqttSettings(){try{const r=await fetch("/api/settings/mqtt",{cache:"no-store"});if(!r.ok)throw new Error("MQTT-instellingen niet beschikbaar");const s=await r.json();const v=(i,x)=>{const e=document.getElementById(i);if(e)e.value=x??""};const c=(i,x)=>{const e=document.getElementById(i);if(e)e.checked=!!x};c("mqttEnabled",s.enabled);v("mqttHost",s.host);v("mqttPort",s.port);v("mqttUser",s.username);v("mqttClientId",s.clientId);v("mqttBaseTopic",s.baseTopic);c("mqttDiscovery",s.homeAssistantDiscovery);Object.entries(mqttPublicationDefaults).forEach(([key,def])=>{c(`publish${key}`,s[`publish${key}`]);v(`topic${key}`,s[`topic${key}`]||def);});updateMqttPublicationCards();}catch(e){showToast(e.message,true);}}
function updateMqttPublicationCards(){Object.keys(mqttPublicationDefaults).forEach(key=>{const input=document.getElementById(`publish${key}`);input?.closest('.mqtt-publication-card')?.classList.toggle('disabled',!input.checked);});}
function resetMqttPublicationDefaults(){Object.entries(mqttPublicationDefaults).forEach(([key,value])=>{const input=document.getElementById(`topic${key}`);if(input)input.value=value;});showToast(tr("Standaardtopics hersteld"));}
async function saveMqttSettings(){const values={enabled:document.getElementById("mqttEnabled")?.checked?"true":"false",host:document.getElementById("mqttHost")?.value||"",port:document.getElementById("mqttPort")?.value||"1883",username:document.getElementById("mqttUser")?.value||"",password:document.getElementById("mqttPassword")?.value||"",clientId:document.getElementById("mqttClientId")?.value||"LayZSpaController",baseTopic:document.getElementById("mqttBaseTopic")?.value||"layzspa",homeAssistantDiscovery:document.getElementById("mqttDiscovery")?.checked?"true":"false"};Object.keys(mqttPublicationDefaults).forEach(key=>{values[`publish${key}`]=document.getElementById(`publish${key}`)?.checked?"true":"false";values[`topic${key}`]=document.getElementById(`topic${key}`)?.value||mqttPublicationDefaults[key];});const b=new URLSearchParams(values);try{const r=await fetch("/api/settings/mqtt",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"},body:b.toString()});if(!r.ok)throw new Error(tr("Opslaan mislukt"));updateMqttPublicationCards();showToast(tr("MQTT-instellingen opgeslagen"));}catch(e){showToast(e.message,true);}}
document.addEventListener("change",e=>{if(e.target?.id?.startsWith("publish"))updateMqttPublicationCards();});

let maintenanceData=null;
function maintenanceLabel(item){if(!item.enabled)return tr("Uit");if(item.due)return item.daysRemaining<0?`${Math.abs(item.daysRemaining)} ${tr("dagen te laat")}`:tr("Vandaag nodig");if(item.daysRemaining<=2)return `${tr("Over")} ${item.daysRemaining} ${tr(item.daysRemaining===1?"dag":"dagen")}`;return `${item.daysRemaining} ${tr("dagen")}`;}
function renderMaintenance(data){maintenanceData=data;const alertTexts={filterReplace:{due:"Filter vervangen nodig",soon:"Filter vervangen binnenkort"},filterClean:{due:"Filter schoonmaken nodig",soon:"Filter schoonmaken binnenkort"},chlorine:{due:"Chloor toevoegen nodig",soon:"Chloor toevoegen binnenkort"}};let alerts=[];Object.entries(alertTexts).forEach(([key,texts])=>{const item=data[key];const card=document.querySelector(`[data-maintenance-card="${key}"]`);if(!card||!item)return;card.classList.toggle("due",item.enabled&&item.due);card.classList.toggle("soon",item.enabled&&!item.due&&item.daysRemaining<=2);card.classList.toggle("disabled",!item.enabled);card.querySelector('[data-role="last"]').textContent=item.lastDate||tr("Nog niet geregistreerd");card.querySelector('[data-role="next"]').textContent=item.nextDate||tr("Na eerste registratie");card.querySelector('.maintenance-badge').textContent=maintenanceLabel(item);const en=document.getElementById(`${key}Enabled`),days=document.getElementById(`${key}Days`);if(en)en.checked=item.enabled;if(days)days.value=item.intervalDays;if(item.enabled&&item.due)alerts.push(`🔴 ${tr(texts.due)}`);else if(item.enabled&&item.daysRemaining<=2)alerts.push(`🟠 ${tr(texts.soon)}`);});const overall=document.getElementById("maintenanceOverall");if(overall){overall.textContent=data.status==="ALARM"?tr("Onderhoud nodig"):data.status==="WARNING"?tr("Binnenkort onderhoud"):tr("Alles in orde");overall.dataset.status=data.status;}const alert=document.getElementById("maintenanceAlert");if(alert){alert.classList.toggle("hidden",alerts.length===0);alert.innerHTML=alerts.length?`<strong>${tr("Onderhoudsmelding")}</strong><span>${alerts.join("<br>")}</span>`:"";}renderDashboardMaintenanceAlert(data);}
function renderDashboardMaintenanceAlert(data){let box=document.getElementById("dashboardMaintenanceAlert");if(!box){const dashboard=document.getElementById("dashboardView");const head=dashboard?.querySelector('.page-head');if(dashboard&&head){box=document.createElement('button');box.id="dashboardMaintenanceAlert";box.className="dashboard-maintenance-alert hidden";box.onclick=()=>showView('maintenance');head.insertAdjacentElement('afterend',box);}}if(!box)return;const needed=data.status!=="OK";box.classList.toggle("hidden",!needed);box.textContent=data.status==="ALARM"?tr("🧰 Onderhoud nodig – tik om te bekijken"):tr("🧰 Onderhoud binnenkort – tik om te bekijken");}
async function loadMaintenance(){try{const r=await fetch('/api/maintenance',{cache:'no-store'});if(!r.ok)throw new Error(tr('Onderhoud kon niet worden geladen'));renderMaintenance(await r.json());}catch(e){showToast(e.message,true);}}
async function saveMaintenanceSettings(){const b=new URLSearchParams({filterReplaceEnabled:document.getElementById('filterReplaceEnabled')?.checked?'true':'false',filterReplaceDays:document.getElementById('filterReplaceDays')?.value||'30',filterCleanEnabled:document.getElementById('filterCleanEnabled')?.checked?'true':'false',filterCleanDays:document.getElementById('filterCleanDays')?.value||'7',chlorineEnabled:document.getElementById('chlorineEnabled')?.checked?'true':'false',chlorineDays:document.getElementById('chlorineDays')?.value||'3'});try{const r=await fetch('/api/maintenance',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded;charset=UTF-8'},body:b.toString()});if(!r.ok)throw new Error(tr('Onderhoudsinstellingen opslaan mislukt'));renderMaintenance(await r.json());showToast(tr('Onderhoudsinstellingen opgeslagen'));}catch(e){showToast(e.message,true);}}
async function markMaintenanceDone(item){try{const r=await fetch('/api/maintenance/done',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded;charset=UTF-8'},body:new URLSearchParams({item}).toString()});const result=await r.json();if(!r.ok)throw new Error(tr(result.error||'Registreren mislukt'));renderMaintenance(result);showToast(tr('Datum bijgewerkt naar vandaag'));}catch(e){showToast(e.message,true);}}

async function loadRegionalSettings(){
  try{
    const r=await fetch("/api/settings/region",{cache:"no-store"});
    if(!r.ok) throw new Error("Regionale instellingen niet beschikbaar");
    const s=await r.json();
    const set=(id,val)=>{const e=document.getElementById(id); if(e) e.value=val??"";};
    const check=(id,val)=>{const e=document.getElementById(id); if(e) e.checked=!!val;};
    set("timeZone",s.timeZone||"Europe/Amsterdam");
    set("dateFormat",s.dateFormat||"DD-MM-YYYY");
    check("clock24",s.use24HourClock!==false);
  }catch(e){showToast(e.message,true);}
}

async function saveRegionalSettings(){
  saveWeatherSettings();
  const body=new URLSearchParams({
    timeZone:document.getElementById("timeZone")?.value||"Europe/Amsterdam",
    dateFormat:document.getElementById("dateFormat")?.value||"DD-MM-YYYY",
    use24HourClock:document.getElementById("clock24")?.checked?"true":"false"
  });
  try{
    const r=await fetch("/api/settings/region",{
      method:"POST",
      headers:{"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"},
      body:body.toString()
    });
    if(!r.ok) throw new Error(tr("Opslaan mislukt"));
    render();
    showToast(tr("Regionale instellingen opgeslagen"));
  }catch(e){showToast(e.message,true);}
}


let historyData = [];
let logData = [];

function applyTheme(theme) {
  document.documentElement.dataset.theme = theme;
  localStorage.setItem("spaTheme", theme);
  const button = document.getElementById("themeToggle");
  if (button) button.textContent = theme === "light" ? "🌙" : "☀️";
}

function toggleTheme() {
  applyTheme(document.documentElement.dataset.theme === "light" ? "dark" : "light");
}

function initTheme() {
  const stored = localStorage.getItem("spaTheme");
  const preferred = window.matchMedia && window.matchMedia("(prefers-color-scheme: light)").matches ? "light" : "dark";
  applyTheme(stored || preferred);
}

async function loadSystemStatus() {
  try {
    const response = await fetch("/api/settings/system", { cache: "no-store" });
    if (!response.ok) return;
    const data = await response.json();
    setText("dashboardClock", data.time || "--:--");
    setText("dashboardDate", data.date || "--");
    setText("dashboardTimeSync", tr(data.timeSynchronized ? "Gesynchroniseerd" : "Wachten"));
    setText("dashboardTimeZone", data.timeZone || "--");
  } catch (_) {}
}

async function loadHistory() {
  const limit = document.getElementById("historyLimit")?.value || "288";
  try {
    const response = await fetch(`/api/history?limit=${encodeURIComponent(limit)}`, { cache: "no-store" });
    if (!response.ok) throw new Error("Historie niet beschikbaar");
    historyData = await response.json();
    renderHistory();
  } catch (error) {
    showToast(error.message, true);
  }
}

function renderHistory() {
  const empty = document.getElementById("historyEmpty");
  const canvas = document.getElementById("temperatureChart");
  if (!canvas) return;
  const values = historyData.map(item => Number(item.temperature)).filter(Number.isFinite);
  empty?.classList.toggle("hidden", values.length > 0);
  canvas.classList.toggle("hidden", values.length === 0);
  setText("historyCount", historyData.length);
  setText("historyMin", values.length ? `${displayTemperature(Math.min(...values))} ${temperatureSymbol()}` : "--");
  setText("historyAvg", values.length ? `${displayTemperature(values.reduce((a,b)=>a+b,0)/values.length)} ${temperatureSymbol()}` : "--");
  setText("historyMax", values.length ? `${displayTemperature(Math.max(...values))} ${temperatureSymbol()}` : "--");
  drawTemperatureChart(canvas, historyData);
  renderActivityTimeline();
}

function drawTemperatureChart(canvas, data) {
  if (!data.length) return;
  const ratio = window.devicePixelRatio || 1;
  const width = canvas.clientWidth || 600;
  const height = 260;
  canvas.width = width * ratio;
  canvas.height = height * ratio;
  const ctx = canvas.getContext("2d");
  ctx.scale(ratio, ratio);
  ctx.clearRect(0, 0, width, height);
  const pad = { left: 42, right: 14, top: 18, bottom: 32 };
  const all = data.flatMap(item => [Number(item.temperature), Number(item.target)]).filter(Number.isFinite);
  let min = Math.floor(Math.min(...all) - 1), max = Math.ceil(Math.max(...all) + 1);
  if (max - min < 4) max = min + 4;
  const x = i => pad.left + (data.length === 1 ? 0 : i * (width-pad.left-pad.right)/(data.length-1));
  const y = value => pad.top + (max-value)*(height-pad.top-pad.bottom)/(max-min);
  ctx.font = "12px system-ui";
  ctx.strokeStyle = getComputedStyle(document.documentElement).getPropertyValue("--line").trim() || "rgba(255,255,255,.15)";
  ctx.fillStyle = getComputedStyle(document.documentElement).getPropertyValue("--muted").trim() || "#8fa8bf";
  for (let i=0;i<=4;i++) { const v=min+(max-min)*i/4; const yy=y(v); ctx.beginPath(); ctx.moveTo(pad.left,yy); ctx.lineTo(width-pad.right,yy); ctx.stroke(); ctx.fillText(`${v.toFixed(0)}°`,4,yy+4); }
  const drawLine=(key,style)=>{ ctx.strokeStyle=style; ctx.lineWidth=2.5; ctx.beginPath(); data.forEach((item,i)=>{ const value=Number(item[key]); if(!Number.isFinite(value)) return; const xx=x(i), yy=y(value); i===0?ctx.moveTo(xx,yy):ctx.lineTo(xx,yy); }); ctx.stroke(); };
  drawLine("target", "#ff9f43");
  drawLine("temperature", "#19a8ff");
  const first = new Date(Number(data[0].timestamp)*1000); const last = new Date(Number(data[data.length-1].timestamp)*1000);
  ctx.fillText(first.toLocaleTimeString([], {hour:"2-digit",minute:"2-digit"}), pad.left, height-8);
  const label=last.toLocaleTimeString([], {hour:"2-digit",minute:"2-digit"}); ctx.fillText(label, width-pad.right-ctx.measureText(label).width, height-8);
}

function renderActivityTimeline() {
  const container = document.getElementById("activityTimeline");
  if (!container) return;
  if (!historyData.length) { container.innerHTML = '<div class="empty-state">Nog geen activiteit.</div>'; return; }
  const keys=[['heater','🔥'],['filter','💧'],['bubbles','🫧'],['jets','🌊']];
  container.innerHTML = keys.map(([key,icon]) => `<div class="activity-row"><span>${icon} ${key}</span><div class="activity-bar">${historyData.map(item=>`<i class="${item[key]?'on':''}"></i>`).join('')}</div></div>`).join('');
}

async function clearHistory() {
  if (!confirm(tr("Historie volledig wissen?"))) return;
  const response = await fetch("/api/history", { method: "DELETE" });
  if (response.ok) { historyData=[]; renderHistory(); showToast(tr("Historie gewist")); }
}

async function loadEnergy() {
  try {
    const energyResponse = await fetch("/api/energy", { cache: "no-store" });
    if (!energyResponse.ok) throw new Error("Energiegegevens niet beschikbaar");
    const data = await energyResponse.json();
    setText("energyTotalKwh", `${Number(data.totalKwh || 0).toFixed(3)} kWh`);
    setText("energyTotalCost", `${data.currency || "€"} ${Number(data.estimatedCost || 0).toFixed(2)}`);
    setText("dashboardKwh", `${Number(data.totalKwh || 0).toFixed(2)} kWh`);
    setText("dashboardCost", `${data.currency || "€"} ${Number(data.estimatedCost || 0).toFixed(2)}`);
    setText("energyHeaterHours", `${Number(data.heaterHours || 0).toFixed(2)} u`);
    setText("energyFilterHours", `${Number(data.filterHours || 0).toFixed(2)} u`);
    setText("energyBubblesHours", `${Number(data.bubblesHours || 0).toFixed(2)} u`);
    setText("energyJetsHours", `${Number(data.jetsHours || 0).toFixed(2)} u`);
    [["energyHeaterWatts",data.heaterWatts],["energyFilterWatts",data.filterWatts],["energyBubblesWatts",data.bubblesWatts],["energyJetsWatts",data.jetsWatts],["energyPrice",data.pricePerKwh],["energyCurrency",data.currency]].forEach(([id,v])=>{const e=document.getElementById(id);if(e)e.value=v??"";});
  } catch (error) { showToast(error.message, true); }
}

async function saveEnergySettings() {
  const body = new URLSearchParams({
    heaterWatts: document.getElementById("energyHeaterWatts")?.value || "2200",
    filterWatts: document.getElementById("energyFilterWatts")?.value || "60",
    bubblesWatts: document.getElementById("energyBubblesWatts")?.value || "800",
    jetsWatts: document.getElementById("energyJetsWatts")?.value || "800",
    pricePerKwh: document.getElementById("energyPrice")?.value || "0.30",
    currency: document.getElementById("energyCurrency")?.value || "€"
  });
  const response = await fetch("/api/energy", {method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"},body:body.toString()});
  if (response.ok) { showToast(tr("Energie-instellingen opgeslagen")); loadEnergy(); } else showToast(tr("Opslaan mislukt"), true);
}

async function resetEnergy() {
  if (!confirm(tr("Alle energietellers resetten?"))) return;
  const response=await fetch("/api/energy",{method:"DELETE"});
  if(response.ok){showToast(tr("Energietellers gereset"));loadEnergy();}
}



let schedulesData=[];
const plannerDayDefinitions=[
  {mask:2,id:"maandag",label:"Maandag"},
  {mask:4,id:"dinsdag",label:"Dinsdag"},
  {mask:8,id:"woensdag",label:"Woensdag"},
  {mask:16,id:"donderdag",label:"Donderdag"},
  {mask:32,id:"vrijdag",label:"Vrijdag"},
  {mask:64,id:"zaterdag",label:"Zaterdag"},
  {mask:1,id:"zondag",label:"Zondag"}
];
const plannerActionLabels=["Heater aan","Heater uit","Filter aan","Filter uit","Bubbels aan","Bubbels uit","Jets aan","Jets uit","Doeltemperatuur"];

function openScheduleModal(){
  const modal=document.getElementById("scheduleModal");
  if(!modal){showToast(tr("Planner-venster ontbreekt"),true);return;}
  document.getElementById("schTime").value="08:00";
  document.getElementById("schAction").value="0";
  document.getElementById("schTemp").value="38";
  document.querySelectorAll('#schDays input[type="checkbox"]').forEach(input=>{input.checked=false;});
  modal.classList.add("open");
  modal.setAttribute("aria-hidden","false");
}
function closeScheduleModal(){
  const modal=document.getElementById("scheduleModal");
  if(!modal)return;
  modal.classList.remove("open");
  modal.setAttribute("aria-hidden","true");
}
function plannerActionText(item){
  const label=tr(plannerActionLabels[Number(item.action)]||"Onbekend");
  return Number(item.action)===8?`${label}: ${Number(item.value)||0}°`:label;
}
function renderSchedules(){
  for(const day of plannerDayDefinitions){
    const list=document.getElementById(`day-${day.id}`);
    if(!list)continue;
    const rows=schedulesData.filter(item=>(Number(item.daysMask)&day.mask)!==0);
    if(!rows.length){list.innerHTML=`<em>${escapeHtml(tr("Nog geen schema's"))}</em>`;continue;}
    list.innerHTML=rows.map(item=>{
      const time=`${String(item.hour).padStart(2,"0")}:${String(item.minute).padStart(2,"0")}`;
      return `<article class="schedule-row" data-schedule-id="${Number(item.id)}">
        <label class="schedule-select"><input type="checkbox" data-schedule-select value="${Number(item.id)}" aria-label="${escapeHtml(tr("Schema selecteren"))}"></label>
        <strong>${time}</strong>
        <span>${escapeHtml(plannerActionText(item))}</span>
        <label class="ui-switch" title="${escapeHtml(tr("Ingeschakeld"))}"><input type="checkbox" ${item.enabled!==false?"checked":""} onchange="setScheduleEnabled(${Number(item.id)},this.checked)"><i></i></label>
        <button class="icon-action schedule-delete" type="button" onclick="deleteSchedule(${Number(item.id)})" title="${escapeHtml(tr("Verwijderen"))}">🗑️</button>
      </article>`;
    }).join("");
  }
  translateDom(document.getElementById("plannerView"));
}
async function loadSchedules(){
  try{
    const response=await fetch("/api/schedules",{cache:"no-store"});
    if(!response.ok)throw new Error(tr("Planner kon niet worden geladen"));
    schedulesData=await response.json();
    if(!Array.isArray(schedulesData))schedulesData=[];
    renderSchedules();
  }catch(error){showToast(error.message||tr("Planner kon niet worden geladen"),true);}
}
async function saveSchedule(){
  const time=(document.getElementById("schTime")?.value||"").split(":");
  const checked=[...document.querySelectorAll('#schDays input[type="checkbox"]:checked')];
  const daysMask=checked.reduce((sum,input)=>sum+Number(input.value||0),0);
  if(time.length!==2||daysMask===0){showToast(tr("Kies een tijd en minimaal één dag"),true);return;}
  const action=Number(document.getElementById("schAction")?.value||0);
  const values={enabled:"true",daysMask:String(daysMask),hour:String(Number(time[0])),minute:String(Number(time[1])),action:String(action),value:String(Number(document.getElementById("schTemp")?.value||38))};
  try{
    const response=await fetch("/api/schedules",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"},body:new URLSearchParams(values).toString()});
    const result=await response.json().catch(()=>({}));
    if(!response.ok)throw new Error(tr(result.error||"Schema niet opgeslagen"));
    closeScheduleModal();
    showToast(tr("Schema opgeslagen"));
    await loadSchedules();
  }catch(error){showToast(error.message||tr("Schema niet opgeslagen"),true);}
}
async function setScheduleEnabled(id,enabled){
  const current=schedulesData.find(item=>Number(item.id)===Number(id));
  if(!current)return;
  const values={id:String(id),enabled:enabled?"true":"false",daysMask:String(current.daysMask),hour:String(current.hour),minute:String(current.minute),action:String(current.action),value:String(current.value||0)};
  try{
    const response=await fetch("/api/schedules",{method:"PUT",headers:{"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"},body:new URLSearchParams(values).toString()});
    if(!response.ok)throw new Error(tr("Schema niet bijgewerkt"));
    current.enabled=enabled;
  }catch(error){showToast(error.message,true);await loadSchedules();}
}
async function deleteSchedule(id,ask=true){
  if(ask&&!confirm(tr("Schema verwijderen?")))return false;
  const response=await fetch("/api/schedules",{method:"DELETE",headers:{"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"},body:new URLSearchParams({id:String(id)}).toString()});
  if(!response.ok){showToast(tr("Schema verwijderen mislukt"),true);return false;}
  return true;
}
async function deleteSelectedSchedules(){
  const ids=[...document.querySelectorAll('[data-schedule-select]:checked')].map(input=>Number(input.value));
  if(!ids.length){showToast(tr("Selecteer minimaal één schema"),true);return;}
  if(!confirm(tr("Aangevinkte schema's verwijderen?")))return;
  for(const id of ids)await deleteSchedule(id,false);
  showToast(tr("Schema's verwijderd"));
  await loadSchedules();
}

function translateLogMessage(message){
  const raw=String(message||"");
  const exact=tr(raw);
  if(exact!==raw) return exact;
  const patterns=[
    [/Scheduler geladen: (\d+) schema's/i,(_,n)=>`${tr("Planner geladen")}: ${n} ${tr("schema's")}`],
    [/MQTT verbindingspoging mislukt.*code[=: ](-?\d+)/i,(_,n)=>`${tr("MQTT connection attempt failed")} (${tr("status")} ${n})`],
    [/Hardwareconfiguratie opgeslagen; herstart vereist/i,()=>tr("Hardware configuration saved; restart required")]
  ];
  for(const [pattern,replacement] of patterns)if(pattern.test(raw))return raw.replace(pattern,replacement);
  return raw;
}
let liveLogTimer = null;

function updateLiveLogState() {
  const enabled = document.getElementById("liveLogEnabled")?.checked !== false;
  localStorage.setItem("spaLiveLog", enabled ? "1" : "0");
  if (liveLogTimer) clearInterval(liveLogTimer);
  liveLogTimer = null;
  if (enabled) {
    liveLogTimer = setInterval(() => {
      if (document.getElementById("logsView")?.classList.contains("active")) {
        loadDiagnostics();
        loadLogs(true);
      }
    }, 2000);
  }
}

async function loadLogs(silent = false) {
  try {
    const response = await fetch("/api/logs?limit=50", {cache:"no-store"});
    if (!response.ok) throw new Error(tr("Logboek niet beschikbaar"));
    logData = await response.json();
    renderLogs();
  } catch (error) {
    if (!silent) showToast(error.message, true);
  }
}

function renderLogs() {
  const list=document.getElementById("logList"); if(!list)return;
  const filter=document.getElementById("logFilter")?.value||"ALL";
  const rows=logData.filter(item=>filter==="ALL"||item.level===filter).reverse();
  if(!rows.length){list.innerHTML=`<div class="empty-state">${tr("Geen logregels.")}</div>`;return;}
  list.innerHTML=rows.map(item=>{const date=new Date(Number(item.timestamp)*1000);return `<article class="log-entry level-${String(item.level).toLowerCase()}"><time>${date.toLocaleString()}</time><b>${escapeHtml(tr(item.level))}</b><p>${escapeHtml(translateLogMessage(item.message))}</p></article>`;}).join('');
}

async function clearLogs(){if(!confirm(tr("Logboek wissen?")))return;const r=await fetch("/api/logs",{method:"DELETE"});if(r.ok){logData=[];renderLogs();showToast(tr("Logboek gewist"));}}

window.addEventListener("resize", () => { if (document.getElementById("historyView")?.classList.contains("active")) renderHistory(); });
initTheme();
const liveLogCheckbox = document.getElementById("liveLogEnabled");
if (liveLogCheckbox) liveLogCheckbox.checked = localStorage.getItem("spaLiveLog") !== "0";
updateLiveLogState();
loadSystemStatus();
loadEnergy();
setInterval(loadSystemStatus, 30000);
setInterval(loadEnergy, 60000);


let diagnosticData = {};

function diagnosticState(value) {
  return value ? "OK" : tr("Aandacht");
}

async function loadDiagnostics() {
  try {
    const response = await fetch("/api/diagnostics", { cache: "no-store" });
    if (!response.ok) throw new Error(tr("Diagnostiek niet beschikbaar"));
    diagnosticData = await response.json();
    setText("diagWifi", diagnosticState(diagnosticData.wifiConnected));
    setText("diagRssi", diagnosticData.wifiConnected ? `${diagnosticData.rssi} dBm` : "Offline");
    setText("diagMqtt", diagnosticState(diagnosticData.mqttConnected));
    setText("diagTime", diagnosticState(diagnosticData.timeSynchronized));
    setText("diagSpa", diagnosticState(diagnosticData.spaConnected));
    const otaError = String(diagnosticData.otaLastError || "");
    const mqttError = Number(diagnosticData.mqttLastError || 0);
    setText("diagOta", otaError ? tr("Aandacht") : "OK");
    setText("diagOtaError", otaError || tr("Geen fout gemeld"));
    setText("diagMqttReconnects", String(diagnosticData.mqttReconnectCount || 0));
    setText("diagMqttError", mqttError ? `MQTT status ${mqttError}` : tr("Geen fout gemeld"));
    setText("diagSpaData", diagnosticData.spaDataValid ? tr("Data geldig") : tr("Geen geldige data"));
    setText("diagHeap", formatBytes(diagnosticData.freeHeap || 0));
    setText("diagFragmentation", `${diagnosticData.heapFragmentation || 0}% ${tr("fragmentatie")}`);
    setText("diagFs", diagnosticState(diagnosticData.filesystemOk));
    const total=Number(diagnosticData.filesystemTotal||0), used=Number(diagnosticData.filesystemUsed||0);
    setText("diagFsUsage", total ? `${formatBytes(used)} / ${formatBytes(total)}` : "--");
  } catch (error) { showToast(error.message, true); }
}

async function runSelfTest() {
  try {
    const response = await fetch("/api/selftest", { method: "POST" });
    const result = await response.json();
    const labels = {filesystem:"LittleFS",memory:"Geheugen",wifi:"WiFi",time:"Tijd",mqtt:"MQTT",spa:"Spa-data"};
    const summary = Object.entries(labels).map(([key,label])=>`${result[key]?"✓":"⚠"} ${label}`).join(" · ");
    showToast(summary, !result.ok);
    await loadDiagnostics();
    await loadLogs();
  } catch (error) { showToast(tr("Zelftest mislukt"), true); }
}

function downloadTextFile(filename, text, type="text/plain") {
  const blob = new Blob([text], { type });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url; link.download = filename; link.click();
  setTimeout(()=>URL.revokeObjectURL(url), 1000);
}

async function downloadDiagnosticReport() {
  try {
    const [diagResponse, logsResponse] = await Promise.all([
      fetch("/api/diagnostics", {cache:"no-store"}),
      fetch("/api/logs?limit=50", {cache:"no-store"})
    ]);
    const diag = await diagResponse.json();
    const logs = await logsResponse.json();
    const report = [
      "Bestway Lay-Z-Spa diagnostisch rapport v0.2",
      `Gegenereerd: ${new Date().toLocaleString()}`,
      "",
      JSON.stringify(diag, null, 2),
      "",
      "Laatste logregels:",
      ...logs.map(item=>`${item.timestamp} [${item.level}] ${item.message}`)
    ].join("\n");
    downloadTextFile(`layzspa-diagnostics-${Date.now()}.txt`, report);
  } catch (error) { showToast(tr("Rapport maken mislukt"), true); }
}

async function fetchJsonOrNull(url) {
  const response = await fetch(url, {cache:"no-store"});
  if (response.status === 404) return null;
  if (!response.ok) throw new Error(url);
  return response.json();
}

async function fetchBackupFile(name) {
  const response = await fetch(`/api/backup/file?name=${encodeURIComponent(name)}`, {cache:"no-store"});
  if (response.status === 404) return null;
  if (!response.ok) throw new Error(`backup:${name}`);
  return response.json();
}

function removeLegacyAutomationReferences(key, value) {
  if (typeof value !== "string" || !value.includes("automation")) return value;
  try {
    const parsed = JSON.parse(value);
    if (Array.isArray(parsed)) return JSON.stringify(parsed.filter(item => item !== "automation"));
    if (parsed && typeof parsed === "object") {
      delete parsed.automation;
      return JSON.stringify(parsed);
    }
  } catch (_) {}
  return value;
}

function exportBrowserSettings() {
  const keys = [
    "layzspaTemperatureUnit","layzspaDashboardOrder","layzspaDashboardButtonOrder",
    "layzspaNavigationOrder","layzspaBottomNavEnabled","layzspaNavTabs",
    "layzspaDashboardSections","layzspaDashboardButtons","spaTheme",
    "layzspaWeatherSettings","layzspaLanguageMode","layzspaLanguage",
    "layzspaAccent","layzspaMotion","layzspaCompact","layzspaSmartInsight"
  ];
  const values = {};
  keys.forEach(key => {
    const value = localStorage.getItem(key);
    if (value !== null) values[key] = removeLegacyAutomationReferences(key, value);
  });
  return values;
}

async function exportBackup() {
  try {
    const [settings, hardware, maintenance, schedules, energy] = await Promise.all([
      fetchBackupFile("settings"), fetchBackupFile("hardware"),
      fetchBackupFile("maintenance"), fetchJsonOrNull("/api/schedules"), fetchJsonOrNull("/api/energy")
    ]);
    const backup = {
      format:"SpaControlBackup", version:"2.0", backupVersion:2,
      createdAt:new Date().toISOString(), wifiIncluded:false,
      files:{settings,hardware,maintenance},
      schedules:schedules||[], energy:energy||null,
      browser:exportBrowserSettings()
    };
    downloadTextFile(`spa-control-backup-${Date.now()}.json`, JSON.stringify(backup,null,2), "application/json");
    showToast(tr("Back-up gedownload"));
  } catch (error) { showToast(tr("Back-up maken mislukt"), true); }
}

async function postForm(url, values, method="POST") {
  const body = new URLSearchParams();
  Object.entries(values || {}).forEach(([key,value])=>{
    if (value !== undefined && value !== null && typeof value !== "object") body.set(key,String(value));
  });
  const response = await fetch(url,{method,headers:{"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"},body:body.toString()});
  if(!response.ok) throw new Error(url);
  return response;
}

async function restoreBackupFile(name, value) {
  if (!value) return;
  const response = await fetch(`/api/backup/file?name=${encodeURIComponent(name)}`, {
    method:"POST", headers:{"Content-Type":"application/json"}, body:JSON.stringify(value)
  });
  if (!response.ok) { let message=""; try{message=(await response.json()).error||"";}catch(_){} throw new Error(message||`backup:${name}`); }
}

function restoreBrowserSettings(values) {
  Object.entries(values || {}).forEach(([key,value]) => {
    if (typeof value === "string") {
      localStorage.setItem(key, removeLegacyAutomationReferences(key, value));
    }
  });
}

function normalizeBackupPayload(rawBackup) {
  if (!rawBackup || typeof rawBackup !== "object" || Array.isArray(rawBackup)) return null;
  const files = rawBackup.files && typeof rawBackup.files === "object"
    ? rawBackup.files
    : {
        settings: rawBackup.settings ?? null,
        hardware: rawBackup.hardware ?? rawBackup.bestway_hwcfg ?? null,
        maintenance: rawBackup.maintenance ?? null
      };
  const recognized = rawBackup.format === "SpaControlBackup" ||
    rawBackup.format === "Spa Control Backup" ||
    files.settings || files.hardware || files.maintenance ||
    Array.isArray(rawBackup.schedules) || rawBackup.energy || rawBackup.browser;
  if (!recognized) return null;
  return {
    format: "SpaControlBackup",
    version: String(rawBackup.version || rawBackup.backupVersion || "1.0"),
    files,
    schedules: Array.isArray(rawBackup.schedules) ? rawBackup.schedules : null,
    energy: rawBackup.energy && typeof rawBackup.energy === "object" ? rawBackup.energy : null,
    browser: rawBackup.browser && typeof rawBackup.browser === "object" ? rawBackup.browser : null
  };
}

async function importBackup(event) {
  const file = event.target.files?.[0];
  if (!file) return;
  if (!confirm(tr("Deze volledige back-up herstellen? Bestaande instellingen en planneritems worden vervangen."))) {
    event.target.value="";
    return;
  }

  const results = [];
  let restoredBackendSection = false;
  const runSection = async (label, action) => {
    try {
      await action();
      results.push(`✓ ${tr(label)}`);
      restoredBackendSection = true;
      return true;
    } catch (error) {
      results.push(`✗ ${tr(label)}: ${error.message || tr("Herstellen mislukt")}`);
      return false;
    }
  };

  try {
    const text = (await file.text()).replace(/^\uFEFF/, "").trim();
    let parsed;
    try { parsed = JSON.parse(text); }
    catch (_) { throw new Error(tr("Back-upbestand kan niet worden gelezen")); }

    const backup = normalizeBackupPayload(parsed);
    if (!backup) throw new Error(tr("Ongeldig back-upbestand: structuur niet herkend"));

    if (backup.files.settings) {
      await runSection("Instellingen hersteld", () => restoreBackupFile("settings", backup.files.settings));
    }
    if (backup.files.hardware) {
      await runSection("Hardware hersteld", () => restoreBackupFile("hardware", backup.files.hardware));
    }
    if (backup.files.maintenance) {
      await runSection("Onderhoud hersteld", () => restoreBackupFile("maintenance", backup.files.maintenance));
    }

    if (backup.schedules !== null) {
      await runSection("Planner hersteld", async () => {
        const existingResponse = await fetch("/api/schedules",{cache:"no-store"});
        const existing = existingResponse.ok ? await existingResponse.json() : [];
        for (const item of existing) await postForm("/api/schedules", {id:item.id}, "DELETE");
        for (const item of backup.schedules) await postForm("/api/schedules", item, "POST");
      });
    }

    if (backup.energy) {
      await runSection("Energie hersteld", () => postForm("/api/energy", backup.energy));
    }

    if (backup.browser) {
      restoreBrowserSettings(backup.browser);
      results.push(`✓ ${tr("Webinterface hersteld")}`);
    }

    results.push(`• ${tr("WiFi-instellingen behouden")}`);
    const failed = results.some(line => line.startsWith("✗"));
    showToast(results.join("\n"), failed);

    if (restoredBackendSection) {
      setTimeout(()=>fetch("/api/restart",{method:"POST"}),1800);
    } else {
      setTimeout(()=>location.reload(),800);
    }
  } catch (error) {
    showToast(error.message || tr("Herstellen mislukt"), true);
  } finally {
    event.target.value="";
  }
}


const hardwareModels = [
  "6-draads, vóór 2021 (AirJet)",
  "6-draads, 2021 (AirJet)",
  "6-draads, 2021 (AirJet + HydroJet)",
  "6-draads, 54149E (AirJet)",
  "4-draads, 54173 (AirJet + HydroJet)",
  "4-draads, 54154 (AirJet)",
  "4-draads, 54144 (HydroJet)",
  "4-draads, 54138 (AirJet + HydroJet)",
  "4-draads, 54123 (AirJet)"
];

function initHardwareSelectors() {
  ["hwCio", "hwDsp"].forEach(id => {
    const select = document.getElementById(id);
    if (!select || select.options.length) return;
    hardwareModels.forEach((name, value) => select.add(new Option(name, value)));
  });
}

function hardwarePinPreset(pcb, cio, dsp) {
  let pins = [0,0,0,0,0,0,0,0];
  const sixCio = cio <= 3;
  const sixDsp = dsp <= 3;
  if (pcb === "v1") {
    pins[0] = sixCio ? 7 : 3; pins[1] = 2; pins[2] = sixCio ? 1 : 0;
    pins[3] = sixDsp ? 5 : 6; pins[4] = sixDsp ? 4 : 7; pins[5] = sixDsp ? 3 : 0; pins[6] = sixDsp ? 6 : 0;
  } else if (pcb === "v2") {
    pins[0] = 1; pins[1] = 2; pins[2] = sixCio ? 3 : 0;
    pins[3] = 4; pins[4] = 5; pins[5] = sixDsp ? 6 : 0; pins[6] = sixDsp ? 7 : 0;
  } else if (pcb === "v2b") {
    pins[0] = sixCio ? 1 : 2; pins[1] = sixCio ? 2 : 5; pins[2] = sixCio ? 5 : 0;
    pins[3] = sixDsp ? 6 : 4; pins[4] = sixDsp ? 4 : 3; pins[5] = sixDsp ? 3 : 0; pins[6] = sixDsp ? 7 : 0;
  }
  return pins;
}

function applyHardwarePins() {
  const pcb = document.getElementById("hwPcb")?.value;
  if (!pcb || pcb === "custom") return;
  const pins = hardwarePinPreset(pcb, Number(document.getElementById("hwCio").value), Number(document.getElementById("hwDsp").value));
  pins.slice(0,7).forEach((pin, index) => document.getElementById(`hwPin${index+1}`).value = pin);
}

async function loadHardwareSettings() {
  initHardwareSelectors();
  try {
    const response = await fetch("/api/hardware", {cache:"no-store"});
    if (!response.ok) throw new Error("Hardwareconfiguratie niet beschikbaar");
    const cfg = await response.json();
    document.getElementById("hwCio").value = cfg.cio ?? 0;
    document.getElementById("hwDsp").value = cfg.dsp ?? 0;
    document.getElementById("hwPcb").value = cfg.pcb ?? "custom";
    document.getElementById("hwTempSensor").checked = String(cfg.hasTempSensor) === "1";
    (cfg.pins || []).forEach((pin,index) => { const el=document.getElementById(`hwPin${index+1}`); if(el) el.value=pin; });
  } catch (error) { showToast(error.message, "error"); }
}

async function saveHardwareSettings() {
  const value = id => Number(document.getElementById(id).value || 0);
  const cfg = {
    cio: value("hwCio"), dsp: value("hwDsp"), pcb: document.getElementById("hwPcb").value,
    hasTempSensor: document.getElementById("hwTempSensor").checked ? "1" : "0",
    pins: Array.from({length:8},(_,i)=>value(`hwPin${i+1}`))
  };
  try {
    const response = await fetch("/api/hardware", {method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(cfg)});
    const result = await response.json();
    if (!response.ok || !result.ok) throw new Error(result.error || "Opslaan mislukt");
    showToast(tr("Hardware opgeslagen. Herstart de ESP om dit actief te maken."), "success");
  } catch(error) { showToast(error.message, "error"); }
}


document.addEventListener("DOMContentLoaded", () => {
  loadDashboardButtons();
  applyDashboardSections();
  applyNavigationSettings();
  loadNavigationOrderEditor();
});




/* v3.2.0 RC7 - robuuste lokale weerinstellingen en weerkaart */
const WEATHER_SETTINGS_KEY = "layzspaWeatherSettings";
const WEATHER_CACHE_KEY = "layzspaWeatherCache";
let weatherRefreshTimer = null;

function getWeatherSettings(){
  try{
    return {enabled:true,city:"",country:"NL",interval:30,latitude:null,longitude:null,...JSON.parse(localStorage.getItem(WEATHER_SETTINGS_KEY)||"{}")};
  }catch(_){
    return {enabled:true,city:"",country:"NL",interval:30,latitude:null,longitude:null};
  }
}
function loadWeatherSettings(){
  const cfg=getWeatherSettings();
  const set=(id,value)=>{const el=document.getElementById(id);if(el)el.value=value??"";};
  set("weatherCity",cfg.city); set("weatherCountry",cfg.country||"NL"); set("weatherInterval",String(cfg.interval||30));
  const enabled=document.getElementById("weatherEnabled"); if(enabled) enabled.checked=cfg.enabled!==false;
  const result=document.getElementById("weatherLocationResult");
  if(result) result.textContent=cfg.latitude!=null&&cfg.longitude!=null?`${Number(cfg.latitude).toFixed(4)}, ${Number(cfg.longitude).toFixed(4)}`:"--";
}
async function saveWeatherSettings(){
  const previous=getWeatherSettings();
  const cfg={
    ...previous,
    enabled:document.getElementById("weatherEnabled")?.checked!==false,
    city:document.getElementById("weatherCity")?.value.trim()||"",
    country:(document.getElementById("weatherCountry")?.value.trim()||"NL").toUpperCase(),
    interval:Math.max(15,Number(document.getElementById("weatherInterval")?.value||30))
  };
  localStorage.setItem(WEATHER_SETTINGS_KEY,JSON.stringify(cfg));
  showToast(tr("Weerinstellingen opgeslagen"));
  scheduleWeatherRefresh();
  await fetchWeather(true);
  return true;
}
async function searchWeatherLocation(){
  const city=document.getElementById("weatherCity")?.value.trim()||"";
  const country=(document.getElementById("weatherCountry")?.value.trim()||"").toUpperCase();
  const result=document.getElementById("weatherLocationResult");
  if(!city){showToast(tr("Vul eerst een plaats in"),true);return;}
  if(result)result.textContent=tr("Zoeken...");
  try{
    const url=`https://geocoding-api.open-meteo.com/v1/search?name=${encodeURIComponent(city)}&count=10&language=${encodeURIComponent(activeLanguage||"nl")}&format=json`;
    const response=await fetch(url,{cache:"no-store"});
    if(!response.ok)throw new Error(tr("Locatie zoeken mislukt"));
    const data=await response.json();
    const candidates=Array.isArray(data.results)?data.results:[];
    const match=candidates.find(x=>!country||String(x.country_code||"").toUpperCase()===country)||candidates[0];
    if(!match)throw new Error(tr("Locatie niet gevonden"));
    const cfg={...getWeatherSettings(),city:match.name||city,country:String(match.country_code||country||"NL").toUpperCase(),latitude:match.latitude,longitude:match.longitude};
    localStorage.setItem(WEATHER_SETTINGS_KEY,JSON.stringify(cfg));
    loadWeatherSettings();
    if(result)result.textContent=`${cfg.city}, ${cfg.country} · ${Number(cfg.latitude).toFixed(4)}, ${Number(cfg.longitude).toFixed(4)}`;
    showToast(tr("Locatie gevonden"));
    await fetchWeather(true);
  }catch(error){if(result)result.textContent="--";showToast(error.message||tr("Locatie zoeken mislukt"),true);}
}
function weatherCodeInfo(code){
  const n=Number(code);
  if(n===0)return ["☀️",tr("Helder")];
  if([1,2].includes(n))return ["🌤️",tr("Licht bewolkt")];
  if(n===3)return ["☁️",tr("Bewolkt")];
  if([45,48].includes(n))return ["🌫️",tr("Mist")];
  if([51,53,55,56,57].includes(n))return ["🌦️",tr("Motregen")];
  if([61,63,65,66,67,80,81,82].includes(n))return ["🌧️",tr("Regen")];
  if([71,73,75,77,85,86].includes(n))return ["🌨️",tr("Sneeuw")];
  if([95,96,99].includes(n))return ["⛈️",tr("Onweer")];
  return ["🌤️",tr("Onbekend")];
}
function renderWeatherCard(record,stale=false){
  if(!record)return;
  const cfg=getWeatherSettings();
  document.getElementById("weatherDashboardCard")?.classList.toggle("dashboard-section-hidden",cfg.enabled===false);
  setText("weatherLocation",record.location||cfg.city||tr("Niet ingesteld"));
  setText("weatherTemperature",Number.isFinite(Number(record.temperature))?`${Math.round(Number(record.temperature))}°C`:"--°C");
  setText("weatherDescription",record.description||tr("Onbekend"));
  setText("weatherRain",Number.isFinite(Number(record.rain))?`${Math.round(Number(record.rain))}%`:"--%");
  setText("weatherWind",Number.isFinite(Number(record.wind))?`${Math.round(Number(record.wind))} km/u`:"-- km/u");
  setText("weatherIcon",record.icon||"🌤️");
  const stamp=record.timestamp?new Date(record.timestamp):null;
  setText("weatherUpdated",stamp&&!Number.isNaN(stamp.getTime())?`${stale?tr("Laatst bekend")+" · ":""}${stamp.toLocaleTimeString([], {hour:"2-digit",minute:"2-digit"})}`:"--:--");
}
async function fetchWeather(force=false){
  const cfg=getWeatherSettings();
  const cached=(()=>{try{return JSON.parse(localStorage.getItem(WEATHER_CACHE_KEY)||"null");}catch(_){return null;}})();
  if(cached)renderWeatherCard(cached,true);
  if(cfg.enabled===false){document.getElementById("weatherDashboardCard")?.classList.add("dashboard-section-hidden");return;}
  if(cfg.latitude==null||cfg.longitude==null){
    setText("weatherLocation",cfg.city||tr("Niet ingesteld"));
    setText("weatherDescription",tr("Locatie instellen"));
    return;
  }
  const maxAge=(Number(cfg.interval)||30)*60000;
  if(!force&&cached&&Date.now()-Number(cached.timestamp||0)<maxAge){renderWeatherCard(cached,false);return;}
  try{
    const url=`https://api.open-meteo.com/v1/forecast?latitude=${encodeURIComponent(cfg.latitude)}&longitude=${encodeURIComponent(cfg.longitude)}&current=temperature_2m,weather_code,wind_speed_10m&hourly=precipitation_probability&forecast_days=1&timezone=auto`;
    const response=await fetch(url,{cache:"no-store"});
    if(!response.ok)throw new Error("Weather HTTP "+response.status);
    const data=await response.json();
    const [icon,description]=weatherCodeInfo(data.current?.weather_code);
    const now=Date.now(); let rain=0;
    if(Array.isArray(data.hourly?.time)&&Array.isArray(data.hourly?.precipitation_probability)){
      let best=0,bestDiff=Infinity;
      data.hourly.time.forEach((t,i)=>{const diff=Math.abs(new Date(t).getTime()-now);if(diff<bestDiff){best=i;bestDiff=diff;}});
      rain=Number(data.hourly.precipitation_probability[best]||0);
    }
    const record={location:[cfg.city,cfg.country].filter(Boolean).join(", "),temperature:Number(data.current?.temperature_2m),description,icon,rain,wind:Number(data.current?.wind_speed_10m),timestamp:Date.now()};
    localStorage.setItem(WEATHER_CACHE_KEY,JSON.stringify(record)); renderWeatherCard(record,false);
  }catch(error){console.warn("Weather update failed",error);if(!cached)showToast(tr("Weer kon niet worden bijgewerkt"),true);}
}
function scheduleWeatherRefresh(){
  if(weatherRefreshTimer)clearInterval(weatherRefreshTimer);
  const minutes=Math.max(15,Number(getWeatherSettings().interval||30));
  weatherRefreshTimer=setInterval(()=>fetchWeather(true),minutes*60000);
}

/* ===== International, PWA and premium customization v2.0 ===== */
let activeLanguage = "nl";
let languagePack = {};
let deferredInstallPrompt = null;
const originalTextNodes = new Map();
const originalAttributes = new WeakMap();
function browserLanguage(){const l=(navigator.language||"nl").slice(0,2).toLowerCase();return ["nl","en","de","fr"].includes(l)?l:"en";}
function tr(source){
  if(source===null||source===undefined)return "";
  const text=String(source);
  if(activeLanguage==="nl") return text;
  return languagePack[text] || languagePack._phrases?.[text] || text;
}
function rememberTextNode(node){ if(!originalTextNodes.has(node)) originalTextNodes.set(node,node.nodeValue); }
function translateTextNode(node){
  rememberTextNode(node); const raw=originalTextNodes.get(node); const trimmed=raw.trim(); if(!trimmed)return;
  const translated=tr(trimmed); node.nodeValue=raw.replace(trimmed,translated);
}
function translateDom(root=document.body){
  if(!root)return;
  const walker=document.createTreeWalker(root,NodeFilter.SHOW_TEXT,{acceptNode(n){return n.parentElement&&!["SCRIPT","STYLE"].includes(n.parentElement.tagName)&&n.nodeValue.trim()?NodeFilter.FILTER_ACCEPT:NodeFilter.FILTER_REJECT;}});
  const nodes=[]; while(walker.nextNode())nodes.push(walker.currentNode); nodes.forEach(translateTextNode);
  root.querySelectorAll?.("[data-i18n]").forEach(el=>{const v=languagePack[el.dataset.i18n];if(v)el.textContent=v;});
  root.querySelectorAll?.("[title],[aria-label],[placeholder]").forEach(el=>{let saved=originalAttributes.get(el);if(!saved){saved={};originalAttributes.set(el,saved);}["title","aria-label","placeholder"].forEach(a=>{if(!el.hasAttribute(a))return;if(!(a in saved))saved[a]=el.getAttribute(a);el.setAttribute(a,tr(saved[a]));});});
}
async function applyLanguage(lang){
  activeLanguage=["nl","en","de","fr"].includes(lang)?lang:"en";
  document.documentElement.lang=activeLanguage;
  try{
    const response=await fetch(`/lang/${activeLanguage}.json?v=3216`,{cache:"no-store"});
    if(!response.ok) throw new Error(`Language file ${response.status}`);
    languagePack=await response.json();
  }catch(error){
    console.error("Language load failed",error);
    languagePack={_phrases:{}};
  }
  translateDom(document.body);
  updateLocalizedFileInputs();
  loadSchedules();
  render();
  updateSmartInsight();
  translateDom(document.body);
  fetchWeather(false);
}
function selectLanguage(lang){
  const chosen=["nl","en","de","fr"].includes(lang)?lang:"nl";
  localStorage.setItem("layzspaLanguageMode","manual");
  localStorage.setItem("layzspaLanguage",chosen);
  const mode=document.getElementById("languageMode");
  const select=document.getElementById("languageSelect");
  if(mode) mode.value="manual";
  if(select) select.value=chosen;
  document.getElementById("manualLanguageField")?.classList.remove("hidden");
  applyLanguage(chosen);
}
function languageModeChanged(){
  const mode=document.getElementById("languageMode")?.value||"manual";
  localStorage.setItem("layzspaLanguageMode",mode);
  document.getElementById("manualLanguageField")?.classList.toggle("hidden",mode==="browser");
  if(mode==="browser") applyLanguage(browserLanguage());
  else selectLanguage(document.getElementById("languageSelect")?.value||localStorage.getItem("layzspaLanguage")||"nl");
}
function saveLanguageSettings(){selectLanguage(document.getElementById("languageSelect")?.value||"nl");}
function loadLanguageSettings(){
  const mode=localStorage.getItem("layzspaLanguageMode")||"manual";
  const lang=localStorage.getItem("layzspaLanguage")||"nl";
  const m=document.getElementById("languageMode"),s=document.getElementById("languageSelect");
  if(m)m.value=mode;
  if(s)s.value=lang;
  document.getElementById("manualLanguageField")?.classList.toggle("hidden",mode==="browser");
  applyLanguage(mode==="browser"?browserLanguage():lang);
}
window.addEventListener("beforeinstallprompt",e=>{e.preventDefault();deferredInstallPrompt=e;document.getElementById("installAppButton")?.classList.add("available");});
async function installPwa(){if(deferredInstallPrompt){deferredInstallPrompt.prompt();await deferredInstallPrompt.userChoice;deferredInstallPrompt=null;}else showToast(activeLanguage==="de"?"Im Browsermenü 'App installieren' wählen":activeLanguage==="fr"?"Choisissez ‘Installer l’application’ dans le menu du navigateur":activeLanguage==="en"?"Choose ‘Install app’ in the browser menu":"Kies ‘App installeren’ in het browsermenu");}
const accents={blue:"#19a8ff",green:"#00df9a",orange:"#ff9f43",red:"#ff4d5e",purple:"#9b7bff"};
function loadAppearanceSettings(){const a=localStorage.getItem("layzspaAccent")||"blue";document.documentElement.style.setProperty("--blue",accents[a]);const sel=document.getElementById("accentTheme");if(sel)sel.value=a;const motion=localStorage.getItem("layzspaMotion")!=="false";const compact=localStorage.getItem("layzspaCompact")==="true";document.body.classList.toggle("reduce-motion",!motion);document.body.classList.toggle("compact-cards",compact);if(document.getElementById("motionEnabled"))document.getElementById("motionEnabled").checked=motion;if(document.getElementById("compactCards"))document.getElementById("compactCards").checked=compact;}
function saveAppearanceSettings(){const a=document.getElementById("accentTheme")?.value||"blue";localStorage.setItem("layzspaAccent",a);localStorage.setItem("layzspaMotion",document.getElementById("motionEnabled")?.checked!==false);localStorage.setItem("layzspaCompact",document.getElementById("compactCards")?.checked===true);loadAppearanceSettings();showToast(tr("Weergave opgeslagen"));}
function loadWidgetSettings(){const on=localStorage.getItem("layzspaSmartInsight")!=="false";document.getElementById("smartInsightCard")?.classList.toggle("dashboard-section-hidden",!on);if(document.getElementById("smartInsightEnabled"))document.getElementById("smartInsightEnabled").checked=on;}
function saveWidgetSettings(){localStorage.setItem("layzspaSmartInsight",document.getElementById("smartInsightEnabled")?.checked!==false);loadWidgetSettings();}
function updateSmartInsight(){const t=Number(state.temperature),g=Number(state.target),active=!!state.heaterActive;let title,text;if(active){title=tr("Water wordt verwarmd");text=tr("De heater levert nu actief warmte.");}else if(Number.isFinite(t)&&Number.isFinite(g)&&g>0&&t>=g){title=tr("Spa op temperatuur");text=tr("De gewenste watertemperatuur is bereikt.");}else if(Number.isFinite(t)&&Number.isFinite(g)&&g>t){title=tr("Water onder doeltemperatuur");text=tr("Schakel de heater in om het water op temperatuur te brengen.");}else{title=tr("Spa gereed");text=tr("Live gegevens worden geanalyseerd.");}setText("smartInsightTitle",title);setText("smartInsightText",text);}
// Dynamic values must use the same translation engine.
const baseRender = render;
render=function(){baseRender();
  const hb=document.getElementById("heater")?.querySelector("small"); if(hb)hb.innerText=state.heaterActive?tr("Verwarmt"):(state.heater?tr("Aan"):tr("Uit"));
  setControlActive("controlLock",state.locked,state.locked?tr("Vergrendeld"):tr("Vrij"));
  setControlActive("controlTimer",state.timerActive,state.timerActive?tr("Actief"):tr("Uit"));
  setControlActive("controlHeater",state.heater,state.heaterActive?tr("Verwarmt"):(state.heater?tr("Aan"):tr("Uit")));
  setDot("wifiStatus",!!state.wifi,tr("Verbonden"),tr("Offline")); setDot("mqttStatus",!!state.mqtt,tr("Online"),tr("Offline"));
  updateSmartInsight();
};
const baseShowView=showView;
showView=function(viewName){baseShowView(viewName);const titles={dashboard:"Bestway Lay-Z-Spa",planner:"Planner",history:"Historie",energy:"Energie",logs:"Diagnostiek",settings:"Instellingen",hardware:"Hardware",control:"Bedieningspaneel",personalization:"Interface aanpassen",maintenance:"Onderhoud",info:"Informatie"};setText("pageTitle",tr(titles[viewName]||"Bestway Lay-Z-Spa"));setTimeout(()=>translateDom(document.querySelector(`#${viewName}View`)||document.body),0);};
function updateLocalizedFileInputs(){
  document.querySelectorAll(".localized-file-input").forEach(wrapper=>{
    const input=wrapper.querySelector('input[type="file"]');
    const button=wrapper.querySelector(".file-select-button");
    const name=wrapper.querySelector(".file-selected-name");
    if(button) button.textContent=tr("Bestand kiezen");
    if(name) name.textContent=input?.files?.[0]?.name || tr("Geen bestand gekozen");
  });
}

function initializeLocalizedFileInputs(){
  document.querySelectorAll(".localized-file-input input[type=file]").forEach(input=>{
    if(input.dataset.localizedReady==="1") return;
    input.dataset.localizedReady="1";
    input.addEventListener("change",updateLocalizedFileInputs);
  });
  updateLocalizedFileInputs();
}


function initSettingsAccordions() {
  document.querySelectorAll("#settingsView > .settings-card, #personalizationView > .settings-card").forEach((card, index) => {
    if (card.dataset.accordionReady === "1") return;
    const title = card.querySelector(":scope > .settings-title");
    if (!title) return;
    card.dataset.accordionReady = "1";
    card.classList.add("settings-collapsible", "settings-collapsed");
    title.setAttribute("role", "button");
    title.setAttribute("tabindex", "0");
    title.setAttribute("aria-expanded", "false");
    const toggle = () => {
      const collapsed = card.classList.toggle("settings-collapsed");
      title.setAttribute("aria-expanded", collapsed ? "false" : "true");
    };
    title.addEventListener("click", event => {
      if (event.target.closest("button,input,select,label,a")) return;
      toggle();
    });
    title.addEventListener("keydown", event => {
      if (event.key === "Enter" || event.key === " ") { event.preventDefault(); toggle(); }
    });
  });
  const mqttPanel = document.getElementById("mqttSettingsPanel");
  const mqttToggle = document.getElementById("mqttSettingsToggle");
  if (mqttPanel) mqttPanel.hidden = true;
  if (mqttToggle) { mqttToggle.classList.remove("open"); mqttToggle.setAttribute("aria-expanded","false"); }
}

const observer=new MutationObserver(mutations=>{for(const m of mutations){m.addedNodes.forEach(n=>{if(n.nodeType===Node.TEXT_NODE)translateTextNode(n);else if(n.nodeType===Node.ELEMENT_NODE)translateDom(n);});}});
document.addEventListener("DOMContentLoaded",()=>{resetLiveControls();initSettingsAccordions();loadLanguageSettings();loadAppearanceSettings();loadWidgetSettings();applyDashboardOrder();loadDashboardOrderEditor();applyDashboardButtonOrder();loadDashboardButtonOrderEditor();initializeLocalizedFileInputs();loadWeatherSettings();loadMaintenance();fetchWeather();scheduleWeatherRefresh();const unitSelect=document.getElementById("temperatureUnit");if(unitSelect){unitSelect.addEventListener("change",()=>{preferredTemperatureUnit=unitSelect.value||"Celsius";localStorage.setItem("layzspaTemperatureUnit",preferredTemperatureUnit);applyTemperatureUnit();});}observer.observe(document.body,{childList:true,subtree:true});if("serviceWorker" in navigator)navigator.serviceWorker.register("/sw.js?v=3216").catch(()=>{});});
