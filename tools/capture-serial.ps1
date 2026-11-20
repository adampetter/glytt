param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^COM\d+$')]
    [string]$Port,

    [int]$Baud = 115200,

    [int]$Seconds = 30
)

$py = 'C:\Users\Alex\.espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe'
if (-not (Test-Path $py)) {
    Write-Error "Python runtime not found: $py"
    exit 1
}

$script = @"
import serial
import time

port = r'$Port'
baud = $Baud
duration = $Seconds

end_time = time.time() + duration
print(f'[SERIAL] capture start port={port} baud={baud} seconds={duration}')

with serial.Serial(port, baud, timeout=0.2) as s:
    while time.time() < end_time:
        data = s.readline()
        if data:
            try:
                print(data.decode('utf-8', errors='replace').rstrip())
            except Exception:
                print(str(data))

print('[SERIAL] capture end')
"@

& $py -c $script
