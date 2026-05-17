cls
Write-Host "=============================================" -ForegroundColor Green
Write-Host "        IndustrialMonitor CPU %         " -ForegroundColor Green
Write-Host "=============================================" -ForegroundColor Green

while ($true) {
    $process = Get-Process "IndustrialMonitor" -ErrorAction SilentlyContinue
    if ($process) {
        $procId = $process.Id
        $counter = Get-Counter "\Process(IndustrialMonitor)\% Processor Time" -ErrorAction SilentlyContinue
        if ($counter) {
            $cpuPercent = [math]::Round($counter.CounterSamples.CookedValue, 1)
        } else {
            $cpuPercent = 0
        }

        Write-Host "running~"
        Write-Host "PID: $procId"
        Write-Host "CPU: $cpuPercent %"
    } else {
        Write-Host "please run IndustrialMonitor.exe" -ForegroundColor Red
    }
    Start-Sleep 1
    cls
}