$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
try {
    if (![Environment]::Is64BitOperatingSystem) { throw 'Windows x64 is required.' }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs = if (Test-Path -LiteralPath $vswhere) { & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath }
    if (!$vs) {
        if (!(Get-Command winget -ErrorAction SilentlyContinue)) { throw 'Install Microsoft App Installer (winget), then run build.bat again.' }
        & winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --source winget --accept-package-agreements --accept-source-agreements --override '--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
        if ($LASTEXITCODE -notin @(0,3010)) { throw "Build Tools installation failed: $LASTEXITCODE" }
        $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    }
    $cmake = if (Get-Command cmake -ErrorAction SilentlyContinue) { (Get-Command cmake).Source } else { Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' }
    if (!(Test-Path -LiteralPath $cmake)) { throw 'CMake is missing. Add the C++ CMake tools component in Visual Studio Installer.' }
    & $cmake -S $root -B (Join-Path $root 'build') -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
    & $cmake --build (Join-Path $root 'build') --config Release --parallel
    if ($LASTEXITCODE) { throw 'Build failed.' }
    $ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
    & $ctest --test-dir (Join-Path $root 'build') -C Release --output-on-failure
    if ($LASTEXITCODE) { throw 'C++ tests failed.' }
    & (Join-Path $root 'tests\installer_test.ps1')
    & (Join-Path $PSScriptRoot 'Package.ps1') -Dll (Join-Path $root 'build\Release\ATSExhaust.dll') -OutputDirectory (Join-Path $root 'dist')
    Write-Host 'Built dist\ATSExhaust-windows-x64.zip. Experimental smoke controller for the pinned ATS 1.61.3.18 build.'
    exit 0
} catch { Write-Host $_ -ForegroundColor Red; exit 1 }
