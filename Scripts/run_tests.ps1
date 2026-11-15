# Runs the three command-line checks on both maps and prints one line per check.
# Close the editor first (it holds the project's module). Logs go to Saved\Tests.
#   powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1
$proj = Split-Path -Parent $PSScriptRoot
$exe = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$logs = Join-Path $proj "Saved\Tests"
New-Item -ItemType Directory -Force $logs | Out-Null
$failed = 0
foreach ($map in "L_AnyRomance", "L_TestRoute") {
    foreach ($test in "RomanceVerifyRules", "RomanceVerifyMovement", "RomanceVerifyCourse") {
        $log = Join-Path $logs "$map-$test.log"
        & $exe "$proj\AnyRomanceUE.uproject" "/Game/Maps/$map" -game -nullrhi -unattended -nosplash -nosound "-$test" "-abslog=$log" 2>$null | Out-Null
        $line = Select-String -LiteralPath $log -Pattern "ANY_ROMANCE_\w+ (passed:|failed)" | Select-Object -Last 1
        $text = if ($line) { $line.Line -replace '^.*?(ANY_ROMANCE_)', '$1' } else { "no result line; see $log" }
        if ($LASTEXITCODE -ne 0 -or -not $line -or $text -match "failed") { $failed++; Write-Output "FAIL  $map  $text" }
        else { Write-Output "ok    $map  $text" }
    }
}
exit $failed
