param(
    [Parameter(Mandatory = $true)]
    [string]$PackagePath,

    [string]$CertificatePath
)

$ErrorActionPreference = 'Stop'

$package = Get-Item -LiteralPath $PackagePath -ErrorAction Stop
if ($package.Extension -notin @('.appx', '.msix')) {
    throw "PackagePath must point to an .appx or .msix package: $PackagePath"
}

if (-not $CertificatePath) {
    $CertificatePath = Join-Path $package.DirectoryName 'SyphonFilterUWP.cer'
}

$certificate = Get-ChildItem Cert:\CurrentUser\My |
    Where-Object {
        $_.Subject -eq 'CN=SyphonFilterPC' -and
        $_.FriendlyName -eq 'Syphon Filter Xbox Dev Mode' -and
        $_.HasPrivateKey -and $_.NotAfter -gt (Get-Date).AddDays(30)
    } |
    Sort-Object NotAfter -Descending |
    Select-Object -First 1

if (-not $certificate) {
    $certificate = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject 'CN=SyphonFilterPC' `
        -FriendlyName 'Syphon Filter Xbox Dev Mode' `
        -CertStoreLocation Cert:\CurrentUser\My `
        -KeyExportPolicy Exportable `
        -HashAlgorithm SHA256 `
        -NotAfter (Get-Date).AddYears(2)
}

$signTool = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64\signtool.exe'
if (-not (Test-Path -LiteralPath $signTool)) {
    throw "SignTool was not found at $signTool. Install the Windows 10 SDK."
}

& $signTool sign /fd SHA256 /sha1 $certificate.Thumbprint /s My /v $package.FullName
if ($LASTEXITCODE -ne 0) {
    throw "SignTool failed for $($package.FullName) with exit code $LASTEXITCODE"
}

Export-Certificate -Cert $certificate -FilePath $CertificatePath -Force | Out-Null
Write-Output "Signed package: $($package.FullName)"
Write-Output "Signing certificate: $CertificatePath"
Write-Output "Certificate thumbprint: $($certificate.Thumbprint)"
