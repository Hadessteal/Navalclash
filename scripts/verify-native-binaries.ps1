param(
    [Parameter(Mandatory=$true)][string]$LuantiSourceDirectory,
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [Parameter(Mandatory=$true)][string]$ExpectedVersion,
    [Parameter(Mandatory=$true)][int]$ExpectedProtocol
)
$ErrorActionPreference = 'Stop'

$SourceRoot = (Resolve-Path -LiteralPath $LuantiSourceDirectory).Path
$BuildRoot = (Resolve-Path -LiteralPath $BuildDirectory).Path

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

function Invoke-Probe([System.IO.FileInfo]$Binary, [string]$Argument) {
    $stdout = [IO.Path]::GetTempFileName()
    $stderr = [IO.Path]::GetTempFileName()
    try {
        $process = Start-Process -FilePath $Binary.FullName -ArgumentList @($Argument) `
            -PassThru -Wait -WindowStyle Hidden `
            -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        $output = @()
        if (Test-Path -LiteralPath $stdout) {
            $output += Get-Content -LiteralPath $stdout
        }
        if (Test-Path -LiteralPath $stderr) {
            $output += Get-Content -LiteralPath $stderr
        }
        if ($process.ExitCode -ne 0) {
            throw "$($Binary.Name) $Argument failed with exit code $($process.ExitCode)`: $($output -join ' ')"
        }
        return (($output -join "`n").Trim())
    }
    finally {
        Remove-Item -LiteralPath $stdout,$stderr -Force -ErrorAction SilentlyContinue
    }
}

$client = Find-Binary 'luanti.exe'
$server = Find-Binary 'luantiserver.exe'
if (-not $client) { throw 'luanti.exe missing from complete native build' }
if (-not $server) { throw 'luantiserver.exe missing from complete native build' }
if ($client.Length -lt 1000000) { throw "luanti.exe is unexpectedly small: $($client.Length) bytes" }
if ($server.Length -lt 500000) { throw "luantiserver.exe is unexpectedly small: $($server.Length) bytes" }
$clientVersion = Invoke-Probe $client '--version'
$serverVersion = Invoke-Probe $server '--version'
$clientNativeVersion = Invoke-Probe $client '--navycraft-version'
$serverNativeVersion = Invoke-Probe $server '--navycraft-version'
$clientProtocol = Invoke-Probe $client '--navycraft-protocol'
$serverProtocol = Invoke-Probe $server '--navycraft-protocol'

if ($clientNativeVersion -ne $ExpectedVersion) {
    throw "luanti.exe reports NavyCraft version $clientNativeVersion, expected $ExpectedVersion"
}
if ($serverNativeVersion -ne $ExpectedVersion) {
    throw "luantiserver.exe reports NavyCraft version $serverNativeVersion, expected $ExpectedVersion"
}
if ($clientProtocol -ne [string]$ExpectedProtocol) {
    throw "luanti.exe reports NavyCraft protocol $clientProtocol, expected $ExpectedProtocol"
}
if ($serverProtocol -ne [string]$ExpectedProtocol) {
    throw "luantiserver.exe reports NavyCraft protocol $serverProtocol, expected $ExpectedProtocol"
}

$report = @(
    "NavyCraft Native $ExpectedVersion protocol $ExpectedProtocol",
    "client=$($client.FullName)",
    "client_sha256=$((Get-FileHash $client.FullName -Algorithm SHA256).Hash.ToLowerInvariant())",
    "server=$($server.FullName)",
    "server_sha256=$((Get-FileHash $server.FullName -Algorithm SHA256).Hash.ToLowerInvariant())",
    "client_version=$clientVersion",
    "server_version=$serverVersion",
    "client_navycraft_version=$clientNativeVersion",
    "server_navycraft_version=$serverNativeVersion",
    "client_navycraft_protocol=$clientProtocol",
    "server_navycraft_protocol=$serverProtocol"
)
$report | Set-Content -Encoding UTF8 (Join-Path $BuildRoot 'navycraft-native-binary-acceptance.txt')
$report | ForEach-Object { Write-Host $_ }
