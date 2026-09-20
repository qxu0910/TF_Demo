param([int]$Port=8082, [int]$Workers=2, [int]$Capacity=8, [int]$DemoWorkMs=0, [string]$Distribution='Ubuntu')
$ErrorActionPreference='Stop'
$scriptPath=Join-Path $PSScriptRoot 'start.sh'
$linuxPath=(& wsl -d $Distribution -- wslpath -a $scriptPath).Trim()
if ($LASTEXITCODE -ne 0) { throw '无法转换 WSL 路径，请检查发行版。' }
& wsl -d $Distribution -- bash $linuxPath $Port $Workers $Capacity $DemoWorkMs
if ($LASTEXITCODE -ne 0) { throw 'C++ Runtime 启动或执行失败。' }
