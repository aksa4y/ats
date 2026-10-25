param([string]$Dll, [string]$Version = '0.1.0', [string]$OutputDirectory = 'dist')
$ErrorActionPreference = 'Stop'
if (!(Test-Path -LiteralPath $Dll)) { throw 'Build the DLL first.' }
$stage = Join-Path ([IO.Path]::GetTempPath()) ([guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $stage | Out-Null
try {
    Copy-Item -LiteralPath $Dll -Destination (Join-Path $stage 'ATSExhaust.dll')
    @{schema=1; version=$Version; platform='windows-x64'; minimum_spf='1.2.6'; visual_smoke=$false;
      game_sha256=@(); dll_sha256=(Get-FileHash -LiteralPath $Dll -Algorithm SHA256).Hash.ToLowerInvariant()} |
      ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $stage 'manifest.json') -Encoding UTF8
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath (Join-Path $OutputDirectory 'ATSExhaust-windows-x64.zip') -Force
} finally { Remove-Item -LiteralPath $stage -Recurse -Force }
