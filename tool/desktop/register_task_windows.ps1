<#
    Registers a daily VibeStack Atlas catalog sync in Windows Task Scheduler.

    Usage (normal user, no admin rights needed):
        powershell -ExecutionPolicy Bypass -File tool\desktop\register_task_windows.ps1 `
            -ExePath "C:\Apps\VibeStackAtlas\vibestack_atlas.exe" -At "09:00"

    Remove again:
        Unregister-ScheduledTask -TaskName "VibeStack Atlas daily sync" -Confirm:$false
#>
param(
    [Parameter(Mandatory = $true)][string]$ExePath,
    [string]$At = "09:00",
    [string]$TaskName = "VibeStack Atlas daily sync"
)

if (-not (Test-Path $ExePath)) {
    Write-Error "Executable not found: $ExePath"
    exit 1
}

$action   = New-ScheduledTaskAction -Execute $ExePath -Argument "--headless-sync"
$trigger  = New-ScheduledTaskTrigger -Daily -At $At
# StartWhenAvailable is the catch-up rule: if the PC was off at $At, the task
# runs at the next opportunity instead of being skipped for the day.
$settings = New-ScheduledTaskSettingsSet -StartWhenAvailable -DontStopIfGoingOnBatteries `
    -AllowStartIfOnBatteries -ExecutionTimeLimit (New-TimeSpan -Minutes 10)

Register-ScheduledTask -TaskName $TaskName -Action $action -Trigger $trigger `
    -Settings $settings -Description "Updates the offline catalog once a day." -Force

Write-Host "Registered '$TaskName' - runs $ExePath --headless-sync daily at $At."
