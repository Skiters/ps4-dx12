param([string]$OutputRoot='F:\PS4Dev\handoff\vulkan-reference-profiler-20261011')
$ErrorActionPreference='Stop'
$active=Get-Content -LiteralPath (Join-Path $OutputRoot 'active-session.json') -Raw | ConvertFrom-Json
$stop=Join-Path $active.session 'STOP'
[IO.File]::WriteAllText($stop,'Stop requested '+[DateTime]::UtcNow.ToString('o'))
Write-Output ('Graceful stop requested. Reports: '+$active.session)
