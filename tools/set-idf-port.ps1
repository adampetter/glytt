param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^COM\d+$')]
    [string]$Port
)

$settingsPath = Join-Path $PSScriptRoot '..\.vscode\settings.json'
$settingsPath = [System.IO.Path]::GetFullPath($settingsPath)

if (-not (Test-Path $settingsPath)) {
    Write-Error "Settings file not found: $settingsPath"
    exit 1
}

$raw = Get-Content -Path $settingsPath -Raw
$json = $raw | ConvertFrom-Json

$json.'idf.port' = $Port
$json.'idf.portWin' = $Port

$json | ConvertTo-Json -Depth 20 | Set-Content -Path $settingsPath -Encoding UTF8
Write-Output "ESP-IDF port updated to $Port in $settingsPath"
