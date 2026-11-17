# ============================================================================
#  set_production.ps1 —— 切换 PRODUCTION_BUILD（等价于 VS「CMake 概述」里改那一项）
#  ---------------------------------------------------------------------------
#  用法：
#    .\tools\set_production.ps1                 # x64-release 打开（默认）
#    .\tools\set_production.ps1 -Value OFF      # 关掉日志过滤
#    .\tools\set_production.ps1 -Preset x64-debug -Value ON
#
#  原理：改的是 out/build/<preset>/CMakeCache.txt 里的 PRODUCTION_BUILD，
#        VS 的「CMake 概述」读的是同一份缓存，所以两边完全同步。
# ============================================================================
param(
    [ValidateSet('ON', 'OFF')]
    [string]$Value = 'ON',

    [ValidateSet('x64-release', 'x64-debug')]
    [string]$Preset = 'x64-release'
)

$ErrorActionPreference = 'Stop'

$repo = Split-Path $PSScriptRoot -Parent
Set-Location $repo

# CMake 不在 PATH 时用 VS 自带的那份
$cmake = 'D:\Visual Stdio community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path $cmake)) {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { $cmake = $cmd.Source }
    else { throw '找不到 cmake，请改脚本里的 $cmake 路径' }
}

Write-Host "==> 配置 $Preset，PRODUCTION_BUILD=$Value" -ForegroundColor Cyan
& $cmake --preset $Preset "-DPRODUCTION_BUILD=$Value"

if ($LASTEXITCODE -ne 0) { throw "cmake 配置失败（退出码 $LASTEXITCODE）" }

$cache = Join-Path $repo "out\build\$Preset\CMakeCache.txt"
$line = Select-String -Path $cache -Pattern '^PRODUCTION_BUILD:' | Select-Object -First 1
Write-Host "==> 缓存已更新：$($line.Line)" -ForegroundColor Green

if ($Value -eq 'ON') {
    Write-Host '==> 之后构建出来的程序不会打印 raylib 日志' -ForegroundColor Yellow
}
else {
    Write-Host '==> 之后构建出来的程序会正常打印 raylib 日志' -ForegroundColor Yellow
}

Write-Host "==> 下一步：cmake --build --preset $Preset" -ForegroundColor Gray
