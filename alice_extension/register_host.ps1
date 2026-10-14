# register_host.ps1
# Sets up the Alice native messaging host for Chrome.
# Run once (or again after changes) from the alice_extension folder.

$ErrorActionPreference = "Stop"

$destDir  = "C:\Users\JASKA\AppData\Roaming\Alice"
$srcDir   = $PSScriptRoot
$jsonDest = "$destDir\alice_host.json"
$pyDest   = "$destDir\alice_host.py"
$batDest  = "$destDir\alice_host.bat"

# ── 1. Ensure destination directory exists ────────────────────────────────────
if (-not (Test-Path $destDir)) {
    New-Item -ItemType Directory -Path $destDir | Out-Null
    Write-Host "Created directory: $destDir"
}

# ── 2. Copy host files ────────────────────────────────────────────────────────
Copy-Item -Path "$srcDir\alice_host.py"   -Destination $pyDest   -Force
Write-Host "Copied alice_host.py  -> $pyDest"

Copy-Item -Path "$srcDir\alice_host.bat"  -Destination $batDest  -Force
Write-Host "Copied alice_host.bat -> $batDest"

Copy-Item -Path "$srcDir\alice_host.json" -Destination $jsonDest -Force
Write-Host "Copied alice_host.json -> $jsonDest"

# ── 3. Patch the path inside alice_host.json to point to the .bat file ───────
#    Chrome on Windows cannot execute .py files directly; the .bat is the entry point.
$json = Get-Content $jsonDest -Raw | ConvertFrom-Json
$json.path = $batDest
$json | ConvertTo-Json -Depth 5 | Set-Content $jsonDest -Encoding UTF8
Write-Host "Patched alice_host.json path -> $batDest"

# ── 4. Register in Windows registry for Chrome AND Opera ─────────────────────
$browsers = @{
    "Chrome" = "HKCU:\Software\Google\Chrome\NativeMessagingHosts\com.alice.bridge"
    "Opera"  = "HKCU:\Software\Opera Software\NativeMessagingHosts\com.alice.bridge"
}
foreach ($browser in $browsers.GetEnumerator()) {
    if (-not (Test-Path $browser.Value)) {
        New-Item -Path $browser.Value -Force | Out-Null
    }
    Set-ItemProperty -Path $browser.Value -Name "(Default)" -Value $jsonDest
    Write-Host "Registry key set ($($browser.Key)): $($browser.Value) -> $jsonDest"
}

# ── Done ──────────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "Setup complete."
Write-Host ""
Write-Host "IMPORTANT: Do NOT run alice_host.py manually."
Write-Host "Opera/Chrome launches it automatically when the extension calls connectNative()."
Write-Host "To see debug output, check: $destDir\bridge.log"
Write-Host ""
Write-Host "After running this script:"
Write-Host "  1. Open Opera -> opera://extensions -> Enable Developer mode"
Write-Host "  2. Click 'Load unpacked' and select the alice_extension folder"
Write-Host "  3. Copy the extension ID shown"
Write-Host "  4. Open $jsonDest and replace the extension ID in allowed_origins if needed"
Write-Host "  5. Re-run this script if you changed alice_host.json"
Write-Host "  6. Open https://chatgpt.com in Opera and send a message from Alice"
