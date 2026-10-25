param(
    [ValidateSet('Run','Update','Diagnose')][string]$Mode = 'Run',
    [string]$GamePath,
    [switch]$NoLaunch
)
. (Join-Path $PSScriptRoot 'Common.ps1')
$root = Split-Path $PSScriptRoot -Parent
$state = Join-Path $root '.state'
$stage = Join-Path $state ([guid]::NewGuid().ToString('N'))
$exitCode = 0
try {
    if (![Environment]::Is64BitOperatingSystem -or ![Environment]::Is64BitProcess) { throw 'Run this installer with 64-bit Windows PowerShell on Windows 10/11 x64.' }
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    [IO.Directory]::CreateDirectory($stage) | Out-Null
    $game = Find-Game $GamePath
    $exe = Join-Path $game 'bin\win_x64\amtrucks.exe'
    $hash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
    $version = (Get-Item -LiteralPath $exe).VersionInfo.ProductVersion
    $plugins = Join-Path $game 'bin\win_x64\plugins'
    $framework = Join-Path $plugins 'spf-framework.dll'
    $report = [ordered]@{game_path=$game; product_version=$version; game_sha256=$hash; spf_installed=(Test-Path -LiteralPath $framework); visual_smoke=$false; status='Windows particle backend not implemented'; checked_utc=[DateTime]::UtcNow.ToString('o')}
    $report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $state 'diagnostics.json') -Encoding UTF8
    Write-Host "ATS: $game`nVersion: $version`nSHA256: $hash"
    Write-Warning 'This is a telemetry preview. It does not control visible smoke.'
    if ($Mode -ne 'Diagnose') {
        Assert-GameStopped
        $release = Get-Release 'aksa4y/ats'
        $assets = @($release.assets | Where-Object { $_.name -eq 'ATSExhaust-windows-x64.zip' })
        if ($assets.Count -ne 1) { throw 'The latest release has no Windows package. See README for source build instructions.' }
        $archive = Join-Path $stage 'plugin.zip'
        Get-VerifiedAsset $assets[0] $archive
        $package = Join-Path $stage 'plugin'
        Expand-SafeZip $archive $package
        $manifest = Get-Content -LiteralPath (Join-Path $package 'manifest.json') -Raw | ConvertFrom-Json
        Assert-Compatible $manifest $hash
        $dll = Join-Path $package 'ATSExhaust.dll'
        if (!(Test-Path -LiteralPath $dll)) { throw 'Plugin DLL missing in release package.' }
        if ((Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash -ne $manifest.dll_sha256) { throw 'Plugin manifest hash mismatch.' }
        $files = @([pscustomobject]@{Source=$dll; Target=(Join-Path $plugins 'spfPlugins\ATSExhaust\ATSExhaust.dll')},
                   [pscustomobject]@{Source=(Join-Path $package 'manifest.json'); Target=(Join-Path $plugins 'spfPlugins\ATSExhaust\manifest.json')})
        $existingVersion = if (Test-Path -LiteralPath $framework) { (Get-Item -LiteralPath $framework).VersionInfo.FileVersion } else { '0.0.0' }
        $parsedVersion = [version]'0.0.0'
        [void][version]::TryParse(($existingVersion -replace '[^0-9.].*$',''), [ref]$parsedVersion)
        $needsFramework = $Mode -eq 'Update' -or !$parsedVersion -or $parsedVersion -lt [version]$manifest.minimum_spf
        if ($needsFramework) {
            $spf = Get-Release 'TrackAndTruckDevs/SPF-Framework'
            if ([version]($spf.tag_name.TrimStart('v')) -lt [version]$manifest.minimum_spf) { throw 'Required SPF release is not available.' }
            $spfAssets = @($spf.assets | Where-Object { $_.name -match '^spf-framework_v[0-9.]+\.zip$' })
            if ($spfAssets.Count -ne 1) { throw 'Ambiguous SPF runtime archive.' }
            $spfZip = Join-Path $stage 'spf.zip'
            Get-VerifiedAsset $spfAssets[0] $spfZip
            $spfDir = Join-Path $stage 'spf'
            Expand-SafeZip $spfZip $spfDir
            if (!(Test-Path -LiteralPath (Join-Path $spfDir 'spf-framework.dll'))) { throw 'Unexpected SPF package structure.' }
            # Install runtime and bundled localization only. Never copy spfPlugins or user configuration.
            $files += [pscustomobject]@{Source=(Join-Path $spfDir 'spf-framework.dll'); Target=$framework}
            $locales = Join-Path $spfDir 'spfAssets\localization'
            if (Test-Path -LiteralPath $locales) {
                foreach ($file in Get-ChildItem -LiteralPath $locales -File -Filter '*.json') {
                    $files += [pscustomobject]@{Source=$file.FullName; Target=(Join-Path $plugins ('spfAssets\localization\' + $file.Name))}
                }
            }
        }
        Ensure-VCRuntime $stage
        Assert-GameStopped
        $backup = Join-Path $state ('backups\' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-ffff'))
        Install-FileSet $files $backup
        Write-Host "Installed release $($release.tag_name). Backup: $backup"
        if ($Mode -eq 'Run' -and !$NoLaunch) {
            Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe)
        }
    }
    Write-Host "Diagnostics: $(Join-Path $state 'diagnostics.json')"
} catch {
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
    if (Test-Path -LiteralPath $state) { $_ | Out-String | Set-Content -LiteralPath (Join-Path $state 'last-error.txt') -Encoding UTF8 }
    $exitCode = 1
} finally {
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
}
exit $exitCode
