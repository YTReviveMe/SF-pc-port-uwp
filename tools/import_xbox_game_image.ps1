#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$DeviceAddress,

    [Parameter(Mandatory)]
    [string]$PackageFullName,

    [Parameter(Mandatory)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$CuePath,

    [Parameter(Mandatory)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$BinPath,

    [PSCredential]$Credential = (Get-Credential -Message 'Xbox Device Portal credentials')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$cueText = [System.IO.File]::ReadAllText((Resolve-Path -LiteralPath $CuePath))
$filePattern = [regex]'(?im)^\s*FILE\s+"([^"]+)"\s+BINARY\s*$'
$cueMatches = $filePattern.Matches($cueText)
if ($cueMatches.Count -ne 1) {
    throw 'The CUE must contain exactly one quoted FILE ... BINARY line.'
}

$referencedBinary = $cueMatches[0].Groups[1].Value
if ([System.IO.Path]::GetFileName($referencedBinary) -ne $referencedBinary) {
    throw 'The CUE references a BIN outside its own folder. Copy a single-folder BIN/CUE pair instead.'
}

# The app always reads this canonical LocalState layout. Keep the original
# track directives intact while making the FILE reference agree with the name
# uploaded to Device Portal.
$canonicalCue = $filePattern.Replace($cueText, 'FILE "game.bin" BINARY', 1)
$temporaryCue = Join-Path ([System.IO.Path]::GetTempPath()) "syphon-filter-xbox-$([guid]::NewGuid()).cue"
[System.IO.File]::WriteAllText(
    $temporaryCue,
    $canonicalCue,
    [System.Text.UTF8Encoding]::new($false))

function Send-DevicePortalFile {
    param(
        [Parameter(Mandatory)][string]$SourcePath,
        [Parameter(Mandatory)][string]$DestinationName
    )

    $encodedPackage = [uri]::EscapeDataString($PackageFullName)
    $encodedPath = [uri]::EscapeDataString("Game/$DestinationName")
    $uri = "https://$DeviceAddress`:11443/api/filesystem/apps/file?knownfolderid=LocalAppData&packagefullname=$encodedPackage&path=$encodedPath"
    Invoke-WebRequest -Uri $uri -Method Post -Credential $Credential -SkipCertificateCheck `
        -InFile $SourcePath -ContentType 'application/octet-stream' `
        -Headers @{ 'Content-Disposition' = "form-data; name=`"file`"; filename=`"$DestinationName`"" } |
        Out-Null
}

try {
    Write-Host 'Uploading game.bin to the app LocalState folder…'
    Send-DevicePortalFile -SourcePath (Resolve-Path -LiteralPath $BinPath) -DestinationName 'game.bin'
    Write-Host 'Uploading game.cue after the BIN completed…'
    Send-DevicePortalFile -SourcePath $temporaryCue -DestinationName 'game.cue'
    Write-Host 'Import complete. Start or restart the Xbox app.'
} finally {
    Remove-Item -LiteralPath $temporaryCue -Force -ErrorAction SilentlyContinue
}
