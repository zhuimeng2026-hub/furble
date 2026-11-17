<#
.SYNOPSIS
    One-click flash helper for furble M5StickS3 (ESP32-S3).
.DESCRIPTION
    Detects the serial port automatically (handles port drift), lets you
    confirm/choose when ambiguous, then flashes with PlatformIO.
    Uses the project-local .venv Python which has pyserial installed.
#>

$ErrorActionPreference = "Stop"
$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$EnvName = "m5stick-s3"
$VenvPython = Join-Path $ProjectDir ".venv\Scripts\python.exe"
$PlatformIO = Join-Path $ProjectDir ".venv\Scripts\platformio.exe"

function Wait-Key {
    # Console.ReadKey throws when there is no interactive console (e.g. piped
    # output / scheduled run). Fail soft so the script never crashes on it.
    try { [void][System.Console]::ReadKey($true) } catch { }
}

function Write-Head($msg) {
    Write-Host ""
    Write-Host "==== $msg ====" -ForegroundColor Cyan
}

# --- sanity checks -------------------------------------------------------
if (-not (Test-Path $VenvPython)) {
    Write-Host "ERROR: venv python not found: $VenvPython" -ForegroundColor Red
    Write-Host "Create it first:  python -m venv .venv ; .venv\Scripts\pip install -r requirements.txt"
    exit 1
}
if (-not (Test-Path $PlatformIO)) {
    Write-Host "ERROR: PlatformIO not found: $PlatformIO" -ForegroundColor Red
    exit 1
}

Write-Head "furble M5StickS3 one-click flash"

# --- enumerate COM ports via pyserial (project venv) ---------------------
# PowerShell parses CSV lines emitted by a tiny inline Python snippet.
$pyCode = @'
import json, serial.tools.list_ports as lp
# ESP32-S3 USB Serial/JTAG and common USB-UART bridges score higher.
def score(vid, pid, desc):
    d = (desc or "").lower()
    if vid == 0x303A:            # Espressif
        return 100
    if vid == 0x1A86:            # WCH CH340/CH9102 (common on M5 boards)
        return 90
    if vid == 0x10C4:            # Silicon Labs CP210x
        return 80
    if vid == 0x0403:            # FTDI
        return 70
    if "jtag" in d or "esp" in d:
        return 60
    return 10
out = []
for p in lp.comports():
    out.append({
        "dev": p.device,
        "vid": p.vid or 0,
        "pid": p.pid or 0,
        "desc": p.description or "",
        "score": score(p.vid or 0, p.pid or 0, p.description or ""),
    })
out.sort(key=lambda x: -x["score"])
print("JSON_START")
print(json.dumps(out))
'@
$pyFile = Join-Path $ProjectDir ".flash_ports_tmp.py"
Set-Content -Path $pyFile -Value $pyCode -Encoding UTF8
try {
    $raw = & $VenvPython $pyFile 2>&1
} finally {
    Remove-Item -Path $pyFile -Force -ErrorAction SilentlyContinue
}
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: failed to enumerate serial ports:" -ForegroundColor Red
    $raw | Write-Host
    exit 1
}
$jsonLine = ($raw | Where-Object { $_ -like '{"dev"*' } | Select-Object -First 1)
$ports = @()
if ($jsonLine) {
    $ports = $jsonLine | ConvertFrom-Json
}

# --- choose port ---------------------------------------------------------
$selected = $null
if ($ports.Count -eq 0) {
    Write-Host "No COM port detected." -ForegroundColor Yellow
    Write-Host "  1) Plug in the M5StickS3 (USB cable, data-capable)." -ForegroundColor Yellow
    Write-Host "  2) If Windows shows 'USB Serial/JTAG' under Other devices, install the driver." -ForegroundColor Yellow
    Write-Host "  3) Run this script again."
    Write-Host ""
    Write-Host "Press any key to exit..."
    Wait-Key
    exit 1
} elseif ($ports.Count -eq 1) {
    $selected = $ports[0]
    Write-Host "Auto-detected one device:" -ForegroundColor Green
    Write-Host ("  {0}  VID:PID={1:X4}:{2:X4}  {3}" -f $selected.dev, $selected.vid, $selected.pid, $selected.desc)
    Write-Host "Using this port. (Pass a COM number as argument to override.)" -ForegroundColor DarkGray
} else {
    Write-Host "Multiple COM ports detected. Select the M5StickS3:" -ForegroundColor Yellow
    for ($i = 0; $i -lt $ports.Count; $i++) {
        $p = $ports[$i]
        $tag = ""
        if ($p.score -ge 80) { $tag = "  <-- likely ESP32" }
        Write-Host ("  [{0}] {1}  VID:PID={2:X4}:{3:X4}  {4}{5}" -f $i, $p.dev, $p.vid, $p.pid, $p.desc, $tag)
    }
    $answer = Read-Host "Enter number [0-$($ports.Count-1)] (or 'q' to quit)"
    if ($answer -eq 'q' -or $answer -eq 'Q') { exit 0 }
    $idx = 0
    if (-not [int]::TryParse($answer, [ref]$idx) -or $idx -lt 0 -or $idx -ge $ports.Count) {
        Write-Host "Invalid selection. Aborted." -ForegroundColor Red
        exit 1
    }
    $selected = $ports[$idx]
}

$port = $selected.dev
Write-Head "Flashing"
Write-Host ("  Target : {0}" -f $EnvName)
Write-Host ("  Port   : {0}" -f $port)
Write-Host ""

# --- build + flash -------------------------------------------------------
Push-Location $ProjectDir
try {
    & $PlatformIO run -e $EnvName -t upload --upload-port $port
    $code = $LASTEXITCODE
} finally {
    Pop-Location
}

Write-Host ""
if ($code -eq 0) {
    Write-Head "DONE - flash succeeded"
    Write-Host ("Flashed to {0}." -f $port) -ForegroundColor Green
    Write-Host ""
    Write-Host "Next (manual verification):" -ForegroundColor DarkGray
    Write-Host ("  Serial console @115200 on {0}:" -f $port) -ForegroundColor DarkGray
    Write-Host "    .venv\Scripts\platformio.exe device monitor -e m5stick-s3 -p $port -b 115200" -ForegroundColor DarkGray
    Write-Host "    type HELP / PING / STATUS ; replies are prefixed with '>'"
} else {
    Write-Head "FAILED"
    Write-Host ("PlatformIO exited with code {0}." -f $code) -ForegroundColor Red
    Write-Host "Common causes: wrong port, another app holding the port, cable/driver, bootloader mode."
}

Write-Host ""
Write-Host "Press any key to close..."
Wait-Key
exit $code
