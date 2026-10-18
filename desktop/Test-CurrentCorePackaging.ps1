$ErrorActionPreference='Stop'
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('ps4-current-package-test-'+[Guid]::NewGuid().ToString('N'))
$source=Join-Path $testRoot 'input'
$catalog=Join-Path $testRoot 'cores'
$current=Join-Path $catalog 'dx12-current'
$publisher=Join-Path $PSScriptRoot 'package-core.ps1'
function Require($condition,[string]$message){if(!$condition){throw $message}}
function Reject([scriptblock]$action){$failed=$false;try{& $action | Out-Null}catch{$failed=$true};Require $failed 'Invalid replacement accepted'}
function Sha([string]$path){(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()}
try{
 New-Item -ItemType Directory -Path $source -Force | Out-Null
 # Filesystem/identity fixture only; never execute these fake runtime inputs.
 'first core' | Set-Content -LiteralPath (Join-Path $source 'core.exe')
 'matching symbol identity' | Set-Content -LiteralPath (Join-Path $source 'core.pdb')
 '{}' | Set-Content -LiteralPath (Join-Path $source 'compiler-profile.json')
 '{}' | Set-Content -LiteralPath (Join-Path $source 'native-contract.json')
 @('core.exe','core.pdb','compiler-profile.json','native-contract.json') | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $testRoot 'files.json')
 @{Repository='https://github.com/Skiters/ps4-dx12';Revision=('a'*40);Dirty=$false;Submodules=@{}} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $testRoot 'source.json')
 @{Configuration='Release';Architecture='x64';Compiler='fixture';CompilerVersion='1';CompilerProfileSha256=(Sha (Join-Path $source 'compiler-profile.json'));CompilerProfileFile='compiler-profile.json';NativeCompilerContractFile='native-contract.json'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $testRoot 'build.json')
 $arguments=@{SourceDirectory=$source;DestinationRoot=$catalog;Id='dx12-current';Name='Current';Commit=('a'*40);Description='Fixture';RuntimeFileList=(Join-Path $testRoot 'files.json');SourceIdentityFile=(Join-Path $testRoot 'source.json');BuildIdentityFile=(Join-Path $testRoot 'build.json');Executable='core.exe';IsolatedProfile=$true}
 & $publisher @arguments -ProfileId 'previous-selected-profile' -Results 'old result' | Out-Null
 $oldHash=Sha (Join-Path $current 'core.exe')
 # A reader holding old symbols must stop replacement before mutation.
 $reader=[IO.File]::Open((Join-Path $current 'core.pdb'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
 try{Reject {& $publisher @arguments}}finally{$reader.Dispose()}
 Require ((Sha (Join-Path $current 'core.exe')) -eq $oldHash) 'Busy package changed'
 # Unknown content is not swept into previous-package deletion.
 $unknown=Join-Path $current 'unique-source.cpp'
 'unique source' | Set-Content -LiteralPath $unknown
 Reject {& $publisher @arguments}
 Require (Test-Path -LiteralPath $unknown) 'Unknown source deleted'
 Remove-Item -LiteralPath $unknown
 # Missing input during staging must preserve the old usable package.
 Remove-Item -LiteralPath (Join-Path $source 'native-contract.json')
 Reject {& $publisher @arguments}
 Require ((Sha (Join-Path $current 'core.exe')) -eq $oldHash) 'Failed staging replaced current'
 '{}' | Set-Content -LiteralPath (Join-Path $source 'native-contract.json')
 'second core' | Set-Content -LiteralPath (Join-Path $source 'core.exe')
 & $publisher @arguments | Out-Null
 $manifest=Get-Content -LiteralPath (Join-Path $current 'core-version.json') -Raw | ConvertFrom-Json
 Require ($manifest.Sha256 -eq (Sha (Join-Path $source 'core.exe')) -and $manifest.Sha256 -ne $oldHash) 'New core identity incorrect'
 Require ($manifest.Results -eq '' -and $manifest.ProfileId -eq 'previous-selected-profile') 'Old acceptance transferred or profile lost'
 Require ((Get-ChildItem -LiteralPath $catalog -Directory).Count -eq 1) 'Temporary/previous package retained'
 $wrong=@{}+$arguments;$wrong.Id='another-version';Reject {& $publisher @wrong}
 Write-Output 'PASS: one current package, busy-file protection, unknown-source preservation, staging rollback, exact new identity, acceptance reset, stable profile and no old runtime archive'
}finally{
 $absolute=[IO.Path]::GetFullPath($testRoot)
 $prefix=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\ps4-current-package-test-'
 if(!$absolute.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe fixture cleanup'}
 if(Test-Path -LiteralPath $absolute){
  if(Get-ChildItem -LiteralPath $absolute -Recurse -Force | Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}){throw 'Fixture cleanup reparse point'}
  Remove-Item -LiteralPath $absolute -Recurse -Force
 }
}
