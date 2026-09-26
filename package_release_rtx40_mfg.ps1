$ErrorActionPreference = 'Stop'

# Always resolve relative to this script, not the caller's working directory.
$root = $PSScriptRoot
$releaseDir = Join-Path $root 'release'

# package_release.ps1 refuses to overwrite existing outputs, so drop the whole
# staging folder first (zips included - back up anything you want to keep).
if (Test-Path -LiteralPath $releaseDir) {
    Remove-Item -LiteralPath $releaseDir -Recurse -Force
}

& (Join-Path $root 'package_release.ps1') -Version nr-rtx40-mfg -EnableRtx40Mfg
