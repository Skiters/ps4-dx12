param(
 [Parameter(Mandatory)][string]$BuildDir,
 [string]$CoreRoot=(Join-Path $env:LOCALAPPDATA 'PS4Desktop/cores'),
 [string]$Results='Release compile/link verified; exact core runtime/game NOT_RUN.',
 [switch]$SelectCurrent
)
$ErrorActionPreference='Stop'
$build=[IO.Path]::GetFullPath($BuildDir).TrimEnd('\')
$identity=Get-Content -LiteralPath (Join-Path $build 'release-identity.json') -Raw | ConvertFrom-Json -AsHashtable
if($identity.configuration -ne 'Release' -or $identity.commit -notmatch '^[a-f0-9]{40}$'){throw 'Expected a pinned Release identity'}
$runtime=@('shadPS4-DX12-Core.exe','shadPS4-DX12-Core.pdb','release-identity.json','compiler-profile.json')
foreach($file in $identity.files){
 $path=[IO.Path]::GetFullPath($file.path)
 if($path.StartsWith($build+'\',[StringComparison]::OrdinalIgnoreCase)){
  if((Get-FileHash -LiteralPath $path).Hash -ine $file.sha256){throw "Build output changed since release identity: $path"}
 }
}
foreach($name in @('native-compiler','licenses')){
 $directory=Join-Path $build $name
 if(!(Test-Path -LiteralPath $directory -PathType Container)){throw "Required package directory missing: $name"}
 foreach($file in Get-ChildItem -LiteralPath $directory -File -Recurse){$runtime+=[IO.Path]::GetRelativePath($build,$file.FullName).Replace('\','/')}
}
$profile=@{Compiler='clang-cl / MSVC ABI';Configuration=$identity.configuration;Configure=$identity.configure;BuildCommand=$identity.build_command;Tools=@($identity.files | Where-Object {!([IO.Path]::GetFullPath($_.path).StartsWith($build+'\',[StringComparison]::OrdinalIgnoreCase))})}
$profile | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $build 'compiler-profile.json') -Encoding utf8NoBOM
$source=@{Repository='https://github.com/Skiters/ps4-dx12';Revision=$identity.commit;Dirty=$false;Submodules=@{};ReferenceRepository='https://github.com/shadps4-emu/shadPS4';ReferenceRevision=$identity.dependencies.reference_sha}
foreach($entry in $identity.dependencies.submodules){$source.Submodules[$entry.path]=$entry.sha}
$metadata=@{Configuration='Release';Architecture='x64';Compiler='clang-cl / MSVC ABI';CompilerVersion='22.1.3';CompilerProfileFile='compiler-profile.json';CompilerProfileSha256=(Get-FileHash -LiteralPath (Join-Path $build 'compiler-profile.json')).Hash.ToLowerInvariant();NativeCompilerContractFile='native-compiler/mesa-native.json'}
$runtime | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $build 'current-runtime-list.json') -Encoding utf8NoBOM
$source | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $build 'current-source-identity.json') -Encoding utf8NoBOM
$metadata | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $build 'current-build-identity.json') -Encoding utf8NoBOM
$settingsPath=Join-Path $env:LOCALAPPDATA 'PS4Desktop/settings.json'
$settings=$null;$profileId=''
if($SelectCurrent){
 $settings=Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json -AsHashtable
 if($settings.ExternalCore -and $settings.ExternalCore.Backend -eq 'dx12'){
  $profileId=if($settings.ExternalCore.ProfileId){$settings.ExternalCore.ProfileId}else{$settings.ExternalCore.Id}
 }
}
$arguments=@{SourceDirectory=$build;DestinationRoot=$CoreRoot;Id='dx12-current';Name='DX12 — актуальное ядро';Commit=$identity.commit;Description='Текущий перенос закреплённого оригинального shadPS4; полная игровая приёмка ещё не завершена.';RuntimeFileList=(Join-Path $build 'current-runtime-list.json');SourceIdentityFile=(Join-Path $build 'current-source-identity.json');BuildIdentityFile=(Join-Path $build 'current-build-identity.json');ProfileId=$profileId;IsolatedProfile=$true;Results=$Results}
& (Join-Path $PSScriptRoot '../../desktop/package-core.ps1') @arguments
if($SelectCurrent){
 $settings.SelectedCoreId='dx12-current';$settings.ExternalCoreDirectory='';$settings.ExternalCore=$null;$settings.Backend='dx12'
 $temporary=$settingsPath+'.publishing'
 try{
  $settings | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $temporary -Encoding utf8NoBOM
  Move-Item -LiteralPath $temporary -Destination $settingsPath -Force
 }finally{if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary -Force}}
}
