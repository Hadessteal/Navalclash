param(
    [Parameter(Mandatory=$true)][string]$LuantiSourceDirectory,
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [Parameter(Mandatory=$true)][string]$GameDirectory,
    [Parameter(Mandatory=$true)][int]$ExpectedProtocol,
    [int]$TimeoutSeconds = 45
)

$ErrorActionPreference = 'Stop'

$SourceRoot = (Resolve-Path -LiteralPath $LuantiSourceDirectory).Path
$BuildRoot = (Resolve-Path -LiteralPath $BuildDirectory).Path
$GameRoot = (Resolve-Path -LiteralPath $GameDirectory).Path

function Find-Binary([string]$name) {
    $candidateDirectories = @(
        (Join-Path $SourceRoot 'bin\Release'),
        (Join-Path $SourceRoot 'bin'),
        (Join-Path $BuildRoot 'bin\Release'),
        (Join-Path $BuildRoot 'bin'),
        (Join-Path $BuildRoot 'Release')
    )
    foreach ($directory in $candidateDirectories) {
        if (-not (Test-Path -LiteralPath $directory)) { continue }
        $match = Get-ChildItem -LiteralPath $directory -Filter $name -File -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($match) { return $match }
    }
    foreach ($root in (@($SourceRoot, $BuildRoot) | Select-Object -Unique)) {
        $match = Get-ChildItem -LiteralPath $root -Recurse -Filter $name -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.FullName -match '[\\/]bin[\\/](Release|RelWithDebInfo|MinSizeRel|Debug)?[\\/]?[^\\/]+$' -or
                $_.DirectoryName -match '[\\/]Release$'
            } |
            Select-Object -First 1
        if ($match) { return $match }
    }
    return $null
}

$server = Find-Binary 'luantiserver.exe'
if (-not $server) { throw 'luantiserver.exe missing from complete native build' }

$gamesRoot = Join-Path $SourceRoot 'games'
$targetGame = Join-Path $gamesRoot 'navycraft'
New-Item -ItemType Directory -Force -Path $gamesRoot | Out-Null
if (-not [String]::Equals($GameRoot, $targetGame, [StringComparison]::OrdinalIgnoreCase)) {
    if (Test-Path -LiteralPath $targetGame) {
        $resolvedTarget = (Resolve-Path -LiteralPath $targetGame).Path
        if (-not $resolvedTarget.StartsWith($gamesRoot, [StringComparison]::OrdinalIgnoreCase)) {
            throw "refusing to replace game outside Luanti games directory: $resolvedTarget"
        }
        Remove-Item -LiteralPath $targetGame -Recurse -Force
    }
    Copy-Item -LiteralPath $GameRoot -Destination $targetGame -Recurse -Force
}

$runRoot = Join-Path ([IO.Path]::GetTempPath()) ("navycraft-native-startup-" + [Guid]::NewGuid().ToString("n"))
$world = Join-Path $runRoot 'world'
$config = Join-Path $runRoot 'minetest.conf'
$log = Join-Path $runRoot 'debug.txt'
$stdout = Join-Path $runRoot 'stdout.txt'
$stderr = Join-Path $runRoot 'stderr.txt'
New-Item -ItemType Directory -Force -Path $world | Out-Null

@"
gameid = navycraft
backend = sqlite3
player_backend = sqlite3
auth_backend = sqlite3
mod_storage_backend = sqlite3
"@ | Set-Content -Encoding ASCII (Join-Path $world 'world.mt')

@"
name = navycraft_acceptance
creative_mode = true
enable_damage = false
server_announce = false
secure.enable_security = false
"@ | Set-Content -Encoding ASCII $config

$port = Get-Random -Minimum 40100 -Maximum 49900
$arguments = @(
    '--world', $world,
    '--gameid', 'navycraft',
    '--config', $config,
    '--port', [string]$port,
    '--logfile', $log,
    '--info'
)

$process = Start-Process -FilePath $server.FullName -ArgumentList $arguments `
    -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr

$expectedLine = "[NavyCraft] native construct engine protocol $ExpectedProtocol active"
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$matched = $false
try {
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path -LiteralPath $log) {
            $content = Get-Content -Raw -LiteralPath $log
            if ($content.Contains($expectedLine)) {
                $matched = $true
                break
            }
            if ($content -match 'temporary Lua renderer|lua-\d+|LuaEntitySAO "nc_core:construct_node"') {
                throw 'server log contains removed Lua construct renderer markers'
            }
        }
        if ($process.HasExited) {
            break
        }
        Start-Sleep -Milliseconds 500
    }

    if (-not $matched) {
        $exit = if ($process.HasExited) { $process.ExitCode } else { 'still running' }
        $tail = if (Test-Path -LiteralPath $log) {
            ((Get-Content -LiteralPath $log -Tail 80) -join "`n")
        } else {
            '<debug log was not created>'
        }
        throw "native server startup did not report protocol $ExpectedProtocol within $TimeoutSeconds seconds; process=$exit`n$tail"
    }
}
finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit(5000)
    }
}

$report = @(
    "NavyCraft native server startup acceptance",
    "server=$($server.FullName)",
    "world=$world",
    "log=$log",
    "protocol_line=$expectedLine",
    "removed_lua_renderer_markers=absent"
)
$reportPath = Join-Path $BuildRoot 'navycraft-native-startup-acceptance.txt'
$report | Set-Content -Encoding UTF8 $reportPath
$report | ForEach-Object { Write-Host $_ }
