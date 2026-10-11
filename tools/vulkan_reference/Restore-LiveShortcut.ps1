param([string]$Manifest='F:\PS4Dev\handoff\vulkan-live-monitor\integration.json')
$ErrorActionPreference='Stop'
$cfg=Get-Content -LiteralPath $Manifest -Raw|ConvertFrom-Json
if((Get-FileHash -LiteralPath $cfg.original_shortcut).Hash.ToLowerInvariant() -ne $cfg.original_shortcut_sha256){throw 'Original backup checksum mismatch.'}
Copy-Item -LiteralPath $cfg.original_shortcut -Destination $cfg.shortcut -Force
if((Get-FileHash -LiteralPath $cfg.shortcut).Hash.ToLowerInvariant() -ne $cfg.original_shortcut_sha256){throw 'Restoration verification failed.'}
Write-Output 'Original shortcut restored byte-for-byte. Game/profile are untouched.'
