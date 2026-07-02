# LeeOCR packaging — assembles a portable dist\LeeOCR folder with every DLL it
# needs (resolved transitively), so it runs on a machine without MSYS2.
# A LeeGStudios.com project.
param(
    [string]$UcrtBin = "C:\msys64\ucrt64\bin",
    [string]$Root = (Split-Path $PSScriptRoot -Parent)
)
$ErrorActionPreference = "Stop"

$exe = Join-Path $Root "build\LeeOCR.exe"
if (-not (Test-Path $exe)) {
    throw "LeeOCR.exe not found. Build first:  cmake --build build"
}

$dist = Join-Path $Root "dist\LeeOCR"
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Force -Path $dist | Out-Null

# Recursively resolve DLL dependencies that live in the UCRT64 toolchain.
$objdump = Join-Path $UcrtBin "objdump.exe"
$seen = @{}; $processed = @{}
$queue = New-Object System.Collections.Queue
$queue.Enqueue($exe)
while ($queue.Count -gt 0) {
    $f = $queue.Dequeue()
    if ($processed.ContainsKey($f)) { continue }
    $processed[$f] = $true
    $deps = & $objdump -p $f 2>$null | Select-String "DLL Name:" |
            ForEach-Object { ($_ -replace '.*DLL Name:\s*', '').Trim() }
    foreach ($d in $deps) {
        $p = Join-Path $UcrtBin $d
        if ((Test-Path $p) -and -not $seen.ContainsKey($d.ToLower())) {
            $seen[$d.ToLower()] = $p
            $queue.Enqueue($p)
        }
    }
}

Copy-Item $exe (Join-Path $dist "LeeOCR.exe")
foreach ($p in $seen.Values) { Copy-Item $p $dist }
Copy-Item (Join-Path $Root "tessdata") (Join-Path $dist "tessdata") -Recurse

Write-Host "Packaged LeeOCR.exe + $($seen.Count) DLLs + tessdata -> $dist"
