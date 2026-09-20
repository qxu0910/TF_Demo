param([int]$Port = 8081, [ValidateSet('cpp','mock')][string]$RuntimeMode = 'cpp', [string]$RuntimeUrl = 'http://127.0.0.1:8082', [int]$TimeoutMs = 2000)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$javaCommand = Get-Command java -ErrorAction SilentlyContinue
$compilerCommand = Get-Command javac -ErrorAction SilentlyContinue
if ($javaCommand -and $compilerCommand) {
    $javaPath = $javaCommand.Source
    $javacPath = $compilerCommand.Source
} else {
    $bundled = Get-ChildItem -Path (Join-Path $projectRoot 'tmp/toolchains/jdk/*/bin/javac.exe') -ErrorAction SilentlyContinue | Select-Object -First 1
    if (!$bundled) { throw '需要 JDK 17+：请安装 JDK 并将 bin 加入 PATH。' }
    $javacPath = $bundled.FullName
    $javaPath = Join-Path $bundled.Directory.FullName 'java.exe'
}
$dependency = Join-Path $projectRoot 'tmp/java-libs/gson-2.14.0.jar'
if (!(Test-Path -LiteralPath $dependency)) {
    New-Item -ItemType Directory -Force -Path (Split-Path $dependency -Parent) | Out-Null
    Invoke-WebRequest -Uri 'https://repo.maven.apache.org/maven2/com/google/code/gson/gson/2.14.0/gson-2.14.0.jar' -OutFile $dependency
}
$classes = Join-Path $PSScriptRoot 'target/classes'
New-Item -ItemType Directory -Force -Path $classes | Out-Null
$sources = Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'src/main/java/factory') -Filter '*.java' | ForEach-Object FullName
& $javacPath --release 17 --add-modules jdk.httpserver -encoding UTF-8 -cp $dependency -d $classes $sources
if ($LASTEXITCODE -ne 0) { throw 'Java compilation failed' }
# 使用项目内完整路径，避免 Windows 短格式 TEMP 路径导致 JDK 内部回环连接失败。
& $javaPath --add-modules jdk.httpserver "-Djdk.net.unixdomain.tmpdir=$projectRoot/tmp" "-Dfactory.frontend=$projectRoot/frontend" "-Dfactory.runtime.mode=$RuntimeMode" "-Dfactory.runtime.url=$RuntimeUrl" "-Dfactory.runtime.timeoutMs=$TimeoutMs" -cp "$classes;$dependency" factory.Gateway $Port
if ($LASTEXITCODE -ne 0) { throw 'Java gateway stopped with an error' }
