# End-user acquisition of the pinned ExifTool (session 33, decisions P3/P9).
# Windows PowerShell 5.1+ (not pwsh-only). Never redistributes ExifTool (S1c).
# Writes only to the install prefix and cache directory; never PATH, profiles,
# or system directories.
#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$Prefix = "",
    [string]$PinFile = "",
    [string]$CacheDir = "",
    [switch]$Check
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Die([string]$Message) {
    [Console]::Error.WriteLine("install.ps1: $Message")
    exit 1
}

function Resolve-PinFile([string]$Given, [string]$ScriptDir) {
    if (-not [string]::IsNullOrWhiteSpace($Given)) {
        return $Given
    }
    $beside = Join-Path $ScriptDir "backends.env"
    if (Test-Path -LiteralPath $beside -PathType Leaf) {
        return $beside
    }
    $repo = Join-Path $ScriptDir "..\build\backends.env"
    if (Test-Path -LiteralPath $repo -PathType Leaf) {
        return (Resolve-Path -LiteralPath $repo).Path
    }
    Die "missing pin file (looked next to this script and in ..\build\backends.env)"
}

function Get-FileSha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Test-ExpectedHash([string]$Path, [string]$Expected) {
    $got = Get-FileSha256 $Path
    return ($got -eq $Expected.ToLowerInvariant())
}

function Find-Perl {
    $cmd = Get-Command perl -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source) {
        return $cmd.Source
    }
    $cmd = Get-Command perl.exe -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source) {
        return $cmd.Source
    }
    return $null
}

function Write-Wiring([string]$Version, [string]$ScriptPath) {
    Write-Output "ExifTool $Version installed at:"
    Write-Output "  $ScriptPath"
    Write-Output ""
    Write-Output "Wire discovery (this script does not modify PATH, shell profiles, or system directories):"
    Write-Output ""
    Write-Output "  `$env:UMM_EXIFTOOL = '$ScriptPath'"
    Write-Output ""
    Write-Output "Alternatively set ExifToolConfig.exiftool_script to that path (explicit config)."
    Write-Output "Discovery order: explicit config, then UMM_EXIFTOOL, then PATH."
}

function Write-PerlGuidance([string]$StrawberryVersion) {
    [Console]::Error.WriteLine("install.ps1: Perl was not found on PATH. The ExifTool backend needs a Perl interpreter.")
    if (-not [string]::IsNullOrWhiteSpace($StrawberryVersion)) {
        [Console]::Error.WriteLine("install.ps1: Tested pin (decision M4b): Strawberry Perl $StrawberryVersion")
    }
    [Console]::Error.WriteLine("install.ps1: Install Perl, then re-run. This script does not install Perl and does not pretend success.")
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$pinPath = Resolve-PinFile $PinFile $scriptDir
if (-not (Test-Path -LiteralPath $pinPath -PathType Leaf)) {
    Die "missing pin file: $pinPath"
}

$version = ""
$sha256 = ""
$strawberry = ""
$sourceUrl = ""

foreach ($raw in Get-Content -LiteralPath $pinPath) {
    $line = $raw -replace "`r", ""
    if ([string]::IsNullOrWhiteSpace($line)) {
        continue
    }
    if ($line.StartsWith("#")) {
        if ($line -match '(?i)ExifTool source:\s*(\S+)') {
            $sourceUrl = $Matches[1]
        }
        continue
    }
    $eq = $line.IndexOf("=")
    if ($eq -lt 1) {
        Die "malformed line (expected KEY=VALUE): $line"
    }
    $key = $line.Substring(0, $eq)
    $value = $line.Substring($eq + 1)
    switch ($key) {
        "UMM_EXIFTOOL_VERSION" { $version = $value }
        "UMM_EXIFTOOL_SHA256" { $sha256 = $value }
        "UMM_STRAWBERRY_PERL_VERSION" { $strawberry = $value }
    }
}

if ([string]::IsNullOrWhiteSpace($version)) {
    Die "missing key: UMM_EXIFTOOL_VERSION"
}
if ([string]::IsNullOrWhiteSpace($sha256)) {
    Die "missing key: UMM_EXIFTOOL_SHA256"
}
if ($sha256 -notmatch '^[0-9a-fA-F]{64}$') {
    Die "UMM_EXIFTOOL_SHA256 must be 64 hex characters"
}
if ([string]::IsNullOrWhiteSpace($sourceUrl)) {
    Die "pin file has no ExifTool source URL comment"
}
$sourceUrl = $sourceUrl.Replace('${UMM_EXIFTOOL_VERSION}', $version)

if ([string]::IsNullOrWhiteSpace($Prefix)) {
    $local = $env:LOCALAPPDATA
    if ([string]::IsNullOrWhiteSpace($local)) {
        Die "LOCALAPPDATA is not set"
    }
    $Prefix = Join-Path $local "umm\exiftool-$version"
}
if ([string]::IsNullOrWhiteSpace($CacheDir)) {
    $local = $env:LOCALAPPDATA
    if ([string]::IsNullOrWhiteSpace($local)) {
        Die "LOCALAPPDATA is not set"
    }
    $CacheDir = Join-Path $local "umm\cache\exiftool"
}

$installedScript = Join-Path $Prefix "exiftool"
$archiveName = "exiftool-$version.tar.gz"
$archivePath = Join-Path $CacheDir $archiveName

if ($Check) {
    if (Test-Path -LiteralPath $archivePath -PathType Leaf) {
        if (-not (Test-ExpectedHash $archivePath $sha256)) {
            Remove-Item -LiteralPath $archivePath -Force
            Die "checksum mismatch for $archivePath (deleted)"
        }
    }
    if (-not (Test-Path -LiteralPath $installedScript -PathType Leaf)) {
        Die "ExifTool script not found at $installedScript"
    }
    $perl = Find-Perl
    if (-not $perl) {
        Write-PerlGuidance $strawberry
        exit 1
    }
    Write-Output "install.ps1: ExifTool $version ok at $installedScript"
    exit 0
}

function Require-Tool([string]$Name) {
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $cmd) {
        return $false
    }
    return $true
}

if (-not (Require-Tool "tar.exe") -and -not (Require-Tool "tar")) {
    Die "tar.exe is required. Install Windows tar (Windows 10+) and re-run."
}

New-Item -ItemType Directory -Force -Path $CacheDir | Out-Null

function Invoke-Download([string]$Url, [string]$Dest) {
    $part = "$Dest.part"
    if (Test-Path -LiteralPath $part) {
        Remove-Item -LiteralPath $part -Force
    }
    $curl = Get-Command curl.exe -ErrorAction SilentlyContinue
    if ($curl) {
        & curl.exe -fL --retry 3 -o $part $Url
        if ($LASTEXITCODE -ne 0) {
            if (Test-Path -LiteralPath $part) {
                Remove-Item -LiteralPath $part -Force
            }
            Die "download failed: $Url"
        }
    } else {
        try {
            Invoke-WebRequest -Uri $Url -OutFile $part -UseBasicParsing
        } catch {
            if (Test-Path -LiteralPath $part) {
                Remove-Item -LiteralPath $part -Force
            }
            Die "download failed: $Url. Install curl.exe or fix TLS. $($_.Exception.Message)"
        }
    }
    Move-Item -LiteralPath $part -Destination $Dest -Force
}

if (Test-Path -LiteralPath $archivePath -PathType Leaf) {
    if (-not (Test-ExpectedHash $archivePath $sha256)) {
        Remove-Item -LiteralPath $archivePath -Force
        Die "checksum mismatch for $archivePath (deleted; not retried)"
    }
} else {
    Invoke-Download $sourceUrl $archivePath
    if (-not (Test-ExpectedHash $archivePath $sha256)) {
        Remove-Item -LiteralPath $archivePath -Force
        Die "checksum mismatch for $archivePath (deleted; not retried)"
    }
}

$staging = "$Prefix.staging.$PID"
if (Test-Path -LiteralPath $staging) {
    Remove-Item -LiteralPath $staging -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $staging | Out-Null
& tar.exe -xf $archivePath -C $staging
if ($LASTEXITCODE -ne 0) {
    Remove-Item -LiteralPath $staging -Recurse -Force -ErrorAction SilentlyContinue
    Die "failed to extract $archivePath"
}

$found = Get-ChildItem -LiteralPath $staging -Filter "exiftool" -Recurse -File -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $found) {
    Remove-Item -LiteralPath $staging -Recurse -Force -ErrorAction SilentlyContinue
    Die "archive does not contain an exiftool script"
}

$srcRoot = $found.DirectoryName
$parent = Split-Path -Parent $Prefix
if (-not [string]::IsNullOrWhiteSpace($parent)) {
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
}
if (Test-Path -LiteralPath $Prefix) {
    Remove-Item -LiteralPath $Prefix -Recurse -Force
}
Move-Item -LiteralPath $srcRoot -Destination $Prefix
Remove-Item -LiteralPath $staging -Recurse -Force -ErrorAction SilentlyContinue

if (-not (Test-Path -LiteralPath $installedScript -PathType Leaf)) {
    Die "extract did not produce $installedScript"
}

Write-Wiring $version $installedScript

$perl = Find-Perl
if (-not $perl) {
    Write-PerlGuidance $strawberry
    exit 1
}
