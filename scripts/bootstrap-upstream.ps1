$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Destination = Join-Path $Root "upstream\luanti"
New-Item -ItemType Directory -Force -Path (Join-Path $Root "upstream") | Out-Null
if (Test-Path (Join-Path $Destination ".git")) {
    Write-Host "Luanti checkout already exists: $Destination"
    exit 0
}
git clone --depth 1 --branch 5.16.1 https://github.com/luanti-org/luanti.git $Destination
Write-Host "Pinned Luanti 5.16.1 at $Destination"
