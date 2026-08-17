# Desktop scheduling

The app schedules its daily update in three layers:

| Layer | Platform | Works when the app is closed |
|-------|----------|------------------------------|
| WorkManager periodic job | Android | yes |
| In-app timer + catch-up on launch and resume | all | no |
| OS task calling `--headless-sync` | Windows, Linux, macOS | yes |

Desktop has no equivalent of WorkManager, so layer 3 is what makes the daily
promise real there. `--headless-sync` runs exactly the same `SyncRunner` the
app uses, writes the result into shared preferences and exits: no window is
created, so the task is invisible to the user.

* Linux — `vibestack-atlas-sync.service` + `.timer` (systemd user timer,
  `Persistent=true` catches up after the machine was off).
* Windows — `register_task_windows.ps1` (Task Scheduler, `-StartWhenAvailable`
  is the same catch-up rule). No administrator rights required.
* macOS — a launchd agent with `StartCalendarInterval` follows the same shape;
  see DESKTOP.md.

Exit codes: `0` the catalog was updated, `2` the run was skipped because the
data was still fresh or auto-update is off.
