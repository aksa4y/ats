. (Join-Path $PSScriptRoot '../scripts/Common.ps1')
function Check($Condition, $Message) { if (!$Condition) { throw $Message } }
function MustFail([scriptblock]$Action) {
    $failed = $false
    try { & $Action } catch { $failed = $true }
    Check $failed 'Expected failure did not occur'
}
$temp = Join-Path ([IO.Path]::GetTempPath()) ('ATS tests ' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($temp) | Out-Null
try {
    Check ((Get-Field $null 'absent' 'fallback') -eq 'fallback') 'Null property fallback'
    $game = Join-Path $temp 'ATS with spaces'
    $bin = Join-Path $game 'bin/win_x64'
    [IO.Directory]::CreateDirectory($bin) | Out-Null
    Set-Content (Join-Path $bin 'amtrucks.exe') 'synthetic'
    Check (Test-GamePath $game) 'Game discovery with spaces'
    Check ((Find-Game $game) -eq $game) 'Explicit game path'
    MustFail { Find-Game (Join-Path $temp 'missing') }
    $manifest = [pscustomobject]@{schema=1; platform='windows-x64'; visual_smoke=$false; game_sha256=@()}
    Assert-Compatible $manifest 'unknown'
    $manifest.visual_smoke = $true
    MustFail { Assert-Compatible $manifest 'unknown' }
    $manifest.game_sha256 = @('known')
    Assert-Compatible $manifest 'known'
    MustFail { Get-VerifiedAsset ([pscustomobject]@{browser_download_url='https://evil.example/a';digest='bad'}) 'unused' }
    MustFail { Get-VerifiedAsset ([pscustomobject]@{browser_download_url='https://github.com/a';digest='bad'}) 'unused' }
    $source = Join-Path $temp 'source'; Set-Content $source 'new'
    $target = Join-Path $temp 'target'; Set-Content $target 'old'
    $neighbor = Join-Path $temp 'other-plugin'; Set-Content $neighbor 'keep'
    $files = @([pscustomobject]@{Source=$source;Target=$target}, [pscustomobject]@{Source=(Join-Path $temp 'absent');Target=(Join-Path $temp 'new-target')})
    MustFail { Install-FileSet $files (Join-Path $temp 'backup') }
    Check ((Get-Content $target) -eq 'old') 'Rollback must restore old file'
    Check ((Get-Content $neighbor) -eq 'keep') 'Unrelated plugin changed'
    Install-FileSet @($files[0]) (Join-Path $temp 'backup2')
    Check ((Get-Content $target) -eq 'new') 'Install failed'
    $archive = Join-Path $temp 'evil.zip'
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::Open($archive, [IO.Compression.ZipArchiveMode]::Create)
    $null = $zip.CreateEntry('../escape.txt'); $zip.Dispose()
    MustFail { Expand-SafeZip $archive (Join-Path $temp 'extract') }
    Check (!(Test-Path (Join-Path $temp 'escape.txt'))) 'ZIP escaped destination'
    Write-Host 'Installer tests passed.'
} finally { Remove-Item -LiteralPath $temp -Recurse -Force }
