Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-Field($Object, [string]$Name, $Default = $null) {
    if ($null -ne $Object -and $Object.PSObject.Properties.Name -contains $Name) { return $Object.$Name }
    return $Default
}
function Assert-GameStopped {
    if (Get-Process amtrucks -ErrorAction SilentlyContinue) { throw 'Close American Truck Simulator before installation or update.' }
}
function Test-GamePath([string]$Path) {
    return $Path -and (Test-Path -LiteralPath (Join-Path $Path 'bin\win_x64\amtrucks.exe') -PathType Leaf)
}
function Find-Game([string]$ExplicitPath) {
    if ($ExplicitPath) {
        if (!(Test-GamePath $ExplicitPath)) { throw "ATS executable not found in: $ExplicitPath" }
        return (Resolve-Path -LiteralPath $ExplicitPath).Path
    }
    if (Test-GamePath $env:ATS_GAME_DIR) { return (Resolve-Path -LiteralPath $env:ATS_GAME_DIR).Path }
    $steamRoots = @()
    foreach ($key in @('HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'HKLM:\SOFTWARE\Valve\Steam')) {
        $item = Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue
        foreach ($field in @('SteamPath', 'InstallPath')) {
            $value = Get-Field $item $field
            if ($value) { $steamRoots += $value }
        }
    }
    $libraries = @($steamRoots)
    foreach ($root in $steamRoots) {
        $vdf = Join-Path $root 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $vdf) {
            foreach ($match in [regex]::Matches((Get-Content -LiteralPath $vdf -Raw), '"path"\s+"([^"]+)"')) {
                $libraries += $match.Groups[1].Value.Replace('\\', '\')
            }
        }
    }
    $found = @($libraries | Select-Object -Unique | ForEach-Object {
        $path = Join-Path $_ 'steamapps\common\American Truck Simulator'
        if (Test-GamePath $path) { (Resolve-Path -LiteralPath $path).Path }
    } | Select-Object -Unique)
    if ($found.Count -eq 1) { return $found[0] }
    if ($found.Count -gt 1) { Write-Host ($found -join "`n") }
    $entered = (Read-Host 'Enter the ATS installation folder').Trim('"')
    if (!(Test-GamePath $entered)) { throw 'Invalid ATS folder. Example: run.bat -GamePath "D:\SteamLibrary\steamapps\common\American Truck Simulator"' }
    return (Resolve-Path -LiteralPath $entered).Path
}
function Get-Release([string]$Repository) {
    try {
        return Invoke-RestMethod -Uri "https://api.github.com/repos/$Repository/releases/latest" -Headers @{'User-Agent'='ATSExhaust-Installer'; 'Accept'='application/vnd.github+json'} -TimeoutSec 45
    } catch { throw "Cannot retrieve release for $Repository. Check the network, GitHub rate limits, and Releases page. $($_.Exception.Message)" }
}
function Get-VerifiedAsset($Asset, [string]$Destination) {
    $url = [uri]$Asset.browser_download_url
    if ($url.Scheme -ne 'https' -or $url.Host -ne 'github.com') { throw 'Asset URL is not an HTTPS GitHub release URL.' }
    $digest = [string](Get-Field $Asset 'digest' '')
    if ($digest -notmatch '^sha256:([a-fA-F0-9]{64})$') { throw 'GitHub did not provide a SHA-256 digest. Download refused.' }
    $expected = $Matches[1]
    Invoke-WebRequest -UseBasicParsing -Uri $url.AbsoluteUri -OutFile $Destination -TimeoutSec 300
    if ((Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash -ne $expected) { throw 'SHA-256 mismatch. Download refused.' }
}
function Expand-SafeZip([string]$Archive, [string]$Destination) {
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $root = [IO.Path]::GetFullPath($Destination).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    [IO.Directory]::CreateDirectory($root) | Out-Null
    $zip = [IO.Compression.ZipFile]::OpenRead($Archive)
    try {
        $total = [long]0
        foreach ($entry in $zip.Entries) {
            $relative = $entry.FullName.Replace('\', '/').Replace('/', [IO.Path]::DirectorySeparatorChar)
            $target = [IO.Path]::GetFullPath((Join-Path $root $relative))
            if (!$target.StartsWith($root, [StringComparison]::OrdinalIgnoreCase) -or $relative.Contains(':')) { throw 'Unsafe ZIP path.' }
            $total += $entry.Length
            if ($total -gt 268435456) { throw 'Archive exceeds the 256 MB extraction limit.' }
            if ($entry.FullName.EndsWith('/') -or $entry.FullName.EndsWith('\')) { continue }
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target, $false)
        }
    } finally { $zip.Dispose() }
}
function Install-FileSet($Files, [string]$BackupDirectory) {
    # Backup each target before mutation. Roll back all touched files on failure.
    [IO.Directory]::CreateDirectory($BackupDirectory) | Out-Null
    $journal = [Collections.Generic.List[object]]::new()
    try {
        foreach ($file in $Files) {
            $target = [string]$file.Target
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
            $backup = Join-Path $BackupDirectory ("{0:D5}.bak" -f $journal.Count)
            $existed = Test-Path -LiteralPath $target
            if ($existed) { Copy-Item -LiteralPath $target -Destination $backup }
            $journal.Add([pscustomobject]@{Target=$target; Backup=$backup; Existed=$existed})
            Copy-Item -LiteralPath $file.Source -Destination $target -Force
        }
    } catch {
        $failure = $_
        for ($i = $journal.Count - 1; $i -ge 0; $i--) {
            $entry = $journal[$i]
            if ($entry.Existed) { Copy-Item -LiteralPath $entry.Backup -Destination $entry.Target -Force }
            else { Remove-Item -LiteralPath $entry.Target -Force -ErrorAction SilentlyContinue }
        }
        throw $failure
    } finally { ConvertTo-Json -InputObject @($journal.ToArray()) -Depth 4 | Set-Content -LiteralPath (Join-Path $BackupDirectory 'journal.json') -Encoding UTF8 }
}
function Ensure-VCRuntime([string]$Cache) {
    $runtime = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64' -ErrorAction SilentlyContinue
    if ((Get-Field $runtime 'Installed' 0) -eq 1 -and (Get-Field $runtime 'Minor' 0) -ge 40) { return }
    $installer = Join-Path $Cache 'vc_redist.x64.exe'
    Invoke-WebRequest -UseBasicParsing 'https://aka.ms/vs/17/release/vc_redist.x64.exe' -OutFile $installer -TimeoutSec 300
    $sig = Get-AuthenticodeSignature -LiteralPath $installer
    if ($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation(?:,|$)') { throw 'Microsoft runtime signature validation failed.' }
    $process = Start-Process -FilePath $installer -ArgumentList '/install','/passive','/norestart' -Wait -PassThru
    if ($process.ExitCode -eq 3010) { throw 'VC++ runtime installed. Restart Windows, then run run.bat again.' }
    if ($process.ExitCode -notin @(0,1638)) { throw "VC++ runtime installation failed: $($process.ExitCode)" }
}
function Assert-Compatible($Manifest, [string]$GameHash) {
    if ($Manifest.schema -ne 1 -or $Manifest.platform -ne 'windows-x64') { throw 'Unsupported package manifest.' }
    if ($Manifest.visual_smoke -isnot [bool]) { throw 'Invalid visual_smoke field.' }
    if ($Manifest.visual_smoke -and $GameHash -notin @($Manifest.game_sha256)) { throw 'No verified smoke backend for this game executable. Wait for a compatible release.' }
}

function Get-SmokeCompanion([string]$Destination, [string]$Existing) {
    # Immutable source revision from the repository requested by the user.
    $expected = '2cd77352ca267c992f7667f76f994b5cc06a3b32f14344cde36e37a9de4b81e2'
    if ((Test-Path -LiteralPath $Existing) -and (Get-FileHash -LiteralPath $Existing -Algorithm SHA256).Hash -eq $expected) {
        Copy-Item -LiteralPath $Existing -Destination $Destination
    } else {
        $url = 'https://media.githubusercontent.com/media/shortcord/modguard-linux-unoffical/7ae63b632ab56349b5f538b3aa9f3da1d66be3dd/reverse-enginnering/PM_389_Smoke.zip'
        Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $Destination -TimeoutSec 300
    }
    if ((Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash -ne $expected) { throw 'Smoke companion SHA-256 mismatch. Installation cancelled.' }
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($Destination)
    try {
        if (!$zip.GetEntry('unit/hookup/smoke_new.sii')) { throw 'Smoke particle definition missing.' }
        if (@($zip.Entries | Where-Object { $_.FullName -match '^vehicle/truck/.+\.pmg$' }).Count -eq 0) { throw 'Smoke exhaust models missing.' }
    } finally { $zip.Dispose() }
}
