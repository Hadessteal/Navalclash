$ErrorActionPreference = 'Stop'
$LuantiCommit = '5ebd9b57984d5854e0a37fd0125da48e9e59e190'
$EngineVersion = '0.9.0'
$ProtocolVersion = '16'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Work = Join-Path $Root '_native-build'
$Luanti = Join-Path $Work 'luanti'
$Build = Join-Path $Work 'build'
$PrototypeBuild = Join-Path $Work 'prototype-build'
$PackageName = "NavyCraft-Native-Windows-Base-$EngineVersion-p$ProtocolVersion"
$UpdateName = "NavyCraft-Native-Windows-Update-$EngineVersion-p$ProtocolVersion"
$Package = Join-Path $Work $PackageName
$Update = Join-Path $Work $UpdateName

function Resolve-Tool([string]$Name, [string[]]$Fallbacks) {
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    foreach ($path in $Fallbacks) {
        if (Test-Path -LiteralPath $path) { return $path }
    }
    throw "$Name was not found on PATH or in known Visual Studio locations"
}

function Copy-DirectoryIfPresent([string]$Source, [string]$Destination) {
    if (Test-Path $Source) {
        Copy-Item -Recurse -Force $Source $Destination
    }
}

function Find-BuiltBinary([string]$Name) {
    $candidateDirectories = @(
        (Join-Path $Luanti 'bin\Release'),
        (Join-Path $Luanti 'bin'),
        (Join-Path $Build 'bin\Release'),
        (Join-Path $Build 'bin'),
        (Join-Path $Build 'Release')
    )
    foreach ($directory in $candidateDirectories) {
        if (-not (Test-Path -LiteralPath $directory)) { continue }
        $result = Get-ChildItem -LiteralPath $directory -Filter $Name -File -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($result) { return $result }
    }
    foreach ($root in (@($Luanti, $Build) | Select-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $root)) { continue }
        $result = Get-ChildItem -LiteralPath $root -Recurse -Filter $Name -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.FullName -match '[\\/]bin[\\/](Release|RelWithDebInfo|MinSizeRel|Debug)?[\\/]?[^\\/]+$' -or
                $_.DirectoryName -match '[\\/]Release$'
            } |
            Select-Object -First 1
        if ($result) { return $result }
    }
    return $null
}

$CMake = Resolve-Tool 'cmake' @(
    'C:\Program Files\CMake\bin\cmake.exe',
    'C:\Program Files (x86)\CMake\bin\cmake.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
)
$CTestCandidate = Join-Path (Split-Path -Parent $CMake) 'ctest.exe'
if (Test-Path -LiteralPath $CTestCandidate) {
    $CTest = $CTestCandidate
} else {
    $CTest = Resolve-Tool 'ctest' @()
}

Write-Host 'NavyCraft native Gate 9 build: protocol 16, no Lua ship fallback.' -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $Work | Out-Null

if (-not (Test-Path $Luanti)) {
    git -c core.autocrlf=false clone --recursive https://github.com/luanti-org/luanti.git $Luanti
}
git -C $Luanti config core.autocrlf false
git -C $Luanti fetch origin $LuantiCommit --depth 1
git -C $Luanti checkout --force $LuantiCommit
git -C $Luanti clean -ffd
git -C $Luanti submodule update --init --recursive
python (Join-Path $Root 'scripts/verify-upstream.py') $Luanti

Write-Host '1/7 Building permanent native regression suite...' -ForegroundColor Cyan
Remove-Item -Recurse -Force $PrototypeBuild -ErrorAction SilentlyContinue
& $CMake -S (Join-Path $Root 'prototype') -B $PrototypeBuild -A x64 -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'Prototype CMake configuration failed' }
& $CMake --build $PrototypeBuild --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw 'Prototype native regression build failed' }
& $CTest --test-dir $PrototypeBuild -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Prototype native regression tests failed' }
& (Join-Path $PrototypeBuild 'Release/navycraft_construct_benchmark.exe')
if ($LASTEXITCODE -ne 0) { throw 'Prototype native benchmark failed' }

Write-Host '2/7 Applying Gate 9 overlay to exact Luanti source...' -ForegroundColor Cyan
python (Join-Path $Root 'scripts/apply-engine-overlay.py') $Luanti
python (Join-Path $Root 'scripts/test-overlay.py') $Luanti

$Vcpkg = $env:VCPKG_ROOT
if (-not $Vcpkg) { $Vcpkg = $env:VCPKG_INSTALLATION_ROOT }
if (-not $Vcpkg) {
    $LocalVcpkg = Join-Path $Root '_tools\vcpkg'
    if (Test-Path (Join-Path $LocalVcpkg 'vcpkg.exe')) {
        $Vcpkg = $LocalVcpkg
    } else {
        $Vcpkg = 'C:\vcpkg'
    }
}
$VcpkgExe = Join-Path $Vcpkg 'vcpkg.exe'
$Toolchain = Join-Path $Vcpkg 'scripts/buildsystems/vcpkg.cmake'
if (-not (Test-Path $VcpkgExe)) {
    throw "vcpkg.exe not found. Set VCPKG_ROOT or install vcpkg at C:\vcpkg"
}
if (-not (Test-Path $Toolchain)) {
    throw "vcpkg CMake toolchain not found: $Toolchain"
}

Write-Host '3/7 Preparing Luanti vcpkg manifest dependencies...' -ForegroundColor Cyan
$env:VCPKG_ROOT = $Vcpkg
$env:VCPKG_INSTALLATION_ROOT = $Vcpkg
Write-Host "Using vcpkg at $Vcpkg. Luanti's CMake configure will install its pinned manifest packages."

Write-Host '4/7 Configuring complete patched client and server...' -ForegroundColor Cyan
Remove-Item -Recurse -Force $Build -ErrorAction SilentlyContinue
& $CMake -S $Luanti -B $Build -A x64 `
    "-DCMAKE_TOOLCHAIN_FILE=$Toolchain" `
    -DBUILD_CLIENT=TRUE -DBUILD_SERVER=TRUE -DRUN_IN_PLACE=TRUE `
    -DBUILD_UNITTESTS=TRUE -DBUILD_BENCHMARKS=TRUE `
    -DVCPKG_APPLOCAL_DEPS=ON -DENABLE_CURSES=OFF -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'Luanti CMake configuration failed' }

Write-Host '5/7 Compiling patched luanti.exe and luantiserver.exe...' -ForegroundColor Cyan
$BuildLog = Join-Path $Work 'native-build.log'
& $CMake --build $Build --config Release --parallel 2 2>&1 | Tee-Object $BuildLog
if ($LASTEXITCODE -ne 0) {
    throw "Native compile failed. Compiler output: $BuildLog"
}

Write-Host '6/7 Running mandatory complete Luanti tests...' -ForegroundColor Cyan
$TestLog = Join-Path $Work 'native-tests.log'
$TestInventory = & $CTest --test-dir $Build -C Release -N 2>&1
$TestInventory | Set-Content -Encoding UTF8 (Join-Path $Work 'native-test-inventory.log')
$match = [regex]::Match(($TestInventory -join "`n"), 'Total Tests:\s+(\d+)')
if (-not $match.Success -or [int]$match.Groups[1].Value -lt 1) {
    throw 'Luanti configured without any runnable tests; refusing to package binaries'
}
& $CTest --test-dir $Build -C Release --output-on-failure 2>&1 | Tee-Object $TestLog
if ($LASTEXITCODE -ne 0) {
    throw "Complete Luanti tests failed. Test output: $TestLog"
}

Write-Host '7/7 Assembling permanent base and drag-and-drop binary update...' -ForegroundColor Cyan
$Client = Find-BuiltBinary 'luanti.exe'
$Server = Find-BuiltBinary 'luantiserver.exe'
if (-not $Client) { throw 'Compiled luanti.exe was not found' }
if (-not $Server) { throw 'Compiled luantiserver.exe was not found' }

& (Join-Path $Root 'scripts/verify-native-binaries.ps1') `
    -LuantiSourceDirectory $Luanti `
    -BuildDirectory $Build `
    -ExpectedVersion $EngineVersion `
    -ExpectedProtocol ([int]$ProtocolVersion)
if ($LASTEXITCODE -ne 0) { throw 'NavyCraft binary identity verification failed' }

& (Join-Path $Root 'scripts/run-native-startup-acceptance.ps1') `
    -LuantiSourceDirectory $Luanti `
    -BuildDirectory $Build `
    -GameDirectory (Join-Path $Root 'game/navycraft') `
    -ExpectedProtocol ([int]$ProtocolVersion)
if ($LASTEXITCODE -ne 0) { throw 'NavyCraft native startup acceptance failed' }

Remove-Item -Recurse -Force $Package,$Update -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path (Join-Path $Package 'bin') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Package 'games') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Update 'bin') | Out-Null

Copy-Item $Client.FullName (Join-Path $Package 'bin/luanti.exe')
Copy-Item $Server.FullName (Join-Path $Package 'bin/luantiserver.exe')
Copy-Item $Client.FullName (Join-Path $Update 'bin/luanti.exe')
Copy-Item $Server.FullName (Join-Path $Update 'bin/luantiserver.exe')

foreach ($directory in @('builtin','client','fonts','textures','locale')) {
    Copy-DirectoryIfPresent (Join-Path $Luanti $directory) (Join-Path $Package $directory)
}
Copy-Item -Recurse -Force (Join-Path $Root 'game/navycraft') (Join-Path $Package 'games/navycraft')
Copy-Item (Join-Path $Root 'native-engine-version.json') $Package
Copy-Item (Join-Path $Root 'native-engine-version.json') $Update
Copy-Item $BuildLog,$TestLog $Package

$RuntimeBin = Join-Path $Vcpkg 'installed/x64-windows/bin'
if (Test-Path $RuntimeBin) {
    Copy-Item (Join-Path $RuntimeBin '*.dll') (Join-Path $Package 'bin') -ErrorAction SilentlyContinue
    Copy-Item (Join-Path $RuntimeBin '*.dll') (Join-Path $Update 'bin') -ErrorAction SilentlyContinue
}

$BaseReadme = @"
NavyCraft Native Windows base $EngineVersion, protocol $ProtocolVersion

Install this base once. Start the client with bin\luanti.exe.
Future engine iterations are supplied as versioned update archives that are
extracted over this folder. Keep worlds outside the replaced bin directory.
"@
$BaseReadme | Set-Content -Encoding UTF8 (Join-Path $Package 'INSTALL.txt')

$UpdateReadme = @"
NavyCraft native drag-and-drop update $EngineVersion, protocol $ProtocolVersion

Close both client and server. Extract this archive over an existing NavyCraft
Native Windows base and allow replacement of files in bin. Do not apply this
update to stock Luanti or to a base with an incompatible protocol version.
"@
$UpdateReadme | Set-Content -Encoding UTF8 (Join-Path $Update 'UPDATE.txt')

$BaseZip = Join-Path $Work "$PackageName.zip"
$UpdateZip = Join-Path $Work "$UpdateName.zip"
Remove-Item $BaseZip,$UpdateZip -Force -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $Package '*') -DestinationPath $BaseZip
Compress-Archive -Path (Join-Path $Update '*') -DestinationPath $UpdateZip

$BaseHash = (Get-FileHash -Algorithm SHA256 $BaseZip).Hash.ToLowerInvariant()
$UpdateHash = (Get-FileHash -Algorithm SHA256 $UpdateZip).Hash.ToLowerInvariant()
"$BaseHash  $(Split-Path -Leaf $BaseZip)" | Set-Content -Encoding ASCII "$BaseZip.sha256"
"$UpdateHash  $(Split-Path -Leaf $UpdateZip)" | Set-Content -Encoding ASCII "$UpdateZip.sha256"

Write-Host "Native base created: $BaseZip" -ForegroundColor Green
Write-Host "Drag-and-drop update created: $UpdateZip" -ForegroundColor Green
