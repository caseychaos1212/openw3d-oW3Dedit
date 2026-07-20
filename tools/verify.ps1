[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'MinSizeRel', 'RelWithDebInfo')]
    [string]$Configuration = 'Debug',

    [ValidateSet('Both', 'Structured', 'Hex')]
    [string]$Mode = 'Both',

    [string]$CorpusPath,
    [string]$OutputRoot,
    [string]$MSBuildPath,

    [switch]$NoBuild,
    [switch]$BuildOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$solutionPath = Join-Path $repoRoot 'oW3DEdit.sln'
$localSettingsPath = Join-Path $PSScriptRoot 'verify.local.psd1'

function Resolve-MSBuildPath {
    param([string]$ExplicitPath)

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        if (-not (Test-Path -LiteralPath $ExplicitPath -PathType Leaf)) {
            throw "MSBuild was not found at '$ExplicitPath'."
        }
        return (Resolve-Path -LiteralPath $ExplicitPath).Path
    }

    $fromPath = Get-Command 'MSBuild.exe' -ErrorAction SilentlyContinue
    if ($null -ne $fromPath) {
        return $fromPath.Source
    }

    $vsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vsWhere -PathType Leaf) {
        $installationPath = & $vsWhere -latest -products '*' `
            -requires Microsoft.Component.MSBuild -property installationPath
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($installationPath)) {
            $candidate = Join-Path $installationPath.Trim() 'MSBuild\Current\Bin\MSBuild.exe'
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                return $candidate
            }
        }
    }

    $editionCandidates = @('Community', 'Professional', 'Enterprise', 'BuildTools')
    foreach ($edition in $editionCandidates) {
        $candidate = Join-Path $env:ProgramFiles `
            "Microsoft Visual Studio\2022\$edition\MSBuild\Current\Bin\MSBuild.exe"
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    throw 'MSBuild.exe was not found. Install Visual Studio 2022 C++ tools or pass -MSBuildPath.'
}

function Resolve-CorpusPath {
    param([string]$ExplicitPath)

    $candidate = $ExplicitPath
    if ([string]::IsNullOrWhiteSpace($candidate)) {
        $candidate = $env:OW3D_TEST_CORPUS
    }

    if ([string]::IsNullOrWhiteSpace($candidate) -and
        (Test-Path -LiteralPath $localSettingsPath -PathType Leaf)) {
        $settings = Import-PowerShellDataFile -LiteralPath $localSettingsPath
        if ($settings.ContainsKey('CorpusPath')) {
            $candidate = [string]$settings.CorpusPath
        }
    }

    if ([string]::IsNullOrWhiteSpace($candidate)) {
        $candidate = Join-Path $repoRoot 'local-test-corpus'
    }

    if (-not (Test-Path -LiteralPath $candidate -PathType Container)) {
        throw @"
The W3D corpus is not accessible from Windows PowerShell at '$candidate'.
Pass -CorpusPath, set OW3D_TEST_CORPUS, or create tools\verify.local.psd1.
WSL symlinks on Windows drives may be readable from WSL but not from Windows.
"@
    }

    return (Resolve-Path -LiteralPath $candidate).Path
}

try {
    if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) {
        throw "Solution was not found at '$solutionPath'."
    }

    if (-not $NoBuild) {
        $resolvedMSBuild = Resolve-MSBuildPath -ExplicitPath $MSBuildPath
        Write-Host "Building $Configuration|x64 with $resolvedMSBuild"
        & $resolvedMSBuild $solutionPath '/t:Build' "/p:Configuration=$Configuration" `
            '/p:Platform=x64' '/m' '/nologo'
        if ($LASTEXITCODE -ne 0) {
            throw "MSBuild failed with exit code $LASTEXITCODE."
        }
    }

    if ($BuildOnly) {
        Write-Host 'Build verification completed.'
        exit 0
    }

    $resolvedCorpus = Resolve-CorpusPath -ExplicitPath $CorpusPath
    if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
        $OutputRoot = Join-Path $repoRoot 'test-output'
    }
    $resolvedOutput = [System.IO.Path]::GetFullPath($OutputRoot)
    $corpusPrefix = $resolvedCorpus.TrimEnd('\') + '\'
    if ($resolvedOutput.Equals($resolvedCorpus, [System.StringComparison]::OrdinalIgnoreCase) -or
        $resolvedOutput.StartsWith($corpusPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'The output directory must not be the corpus or a directory inside the corpus.'
    }
    New-Item -ItemType Directory -Path $resolvedOutput -Force | Out-Null

    $executablePath = Join-Path $repoRoot "$Configuration\oW3DEdit.exe"
    if (-not (Test-Path -LiteralPath $executablePath -PathType Leaf)) {
        throw "Built executable was not found at '$executablePath'."
    }

    Write-Host "Corpus: $resolvedCorpus"
    Write-Host "Output: $resolvedOutput"
    Write-Host "Mode:   $Mode"
    # Release is linked as a Windows GUI application. PowerShell's call operator
    # does not reliably wait for GUI applications, so $LASTEXITCODE can be unset
    # while validation is still running. Start the process explicitly and wait
    # for its real exit code instead.
    $validationArguments = @(
        '--verify-roundtrip'
        ('"{0}"' -f $resolvedCorpus)
        '--verify-output'
        ('"{0}"' -f $resolvedOutput)
        '--verify-mode'
        $Mode.ToLowerInvariant()
    )
    $validationProcess = Start-Process -FilePath $executablePath `
        -ArgumentList $validationArguments -NoNewWindow -Wait -PassThru
    $validationExitCode = $validationProcess.ExitCode

    if ($validationExitCode -ne 0) {
        [Console]::Error.WriteLine("Round-trip validation failed with exit code $validationExitCode.")
        exit $validationExitCode
    }

    Write-Host 'Build and round-trip verification completed successfully.'
    exit 0
}
catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 2
}
