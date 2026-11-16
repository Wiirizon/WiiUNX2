param([Parameter(Mandatory=$true)][string]$SDRoot)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'Update-SD.ps1') -SDRoot $SDRoot -Environment 'aroma'
