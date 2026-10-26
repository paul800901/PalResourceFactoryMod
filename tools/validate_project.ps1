$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildingFile = Join-Path $projectRoot 'src\palschema\PalResourceFactory\buildings\facilities.json'
$nativeSource = Join-Path $projectRoot 'native\src\PalResourceFactoryNative.cpp'
$data = Get-Content -Raw -LiteralPath $buildingFile | ConvertFrom-Json -AsHashtable

function Assert-Equal {
    param(
        [Parameter(Mandatory)]$Actual,
        [Parameter(Mandatory)]$Expected,
        [Parameter(Mandatory)][string]$Message
    )
    if ($Actual -ne $Expected) {
        throw "$Message (expected '$Expected', got '$Actual')"
    }
}

Assert-Equal $data.Count 2 'Exactly two buildings must be declared'

$processor = $data['PRF_EggResourceProcessor']
$breeder = $data['PRF_ResourceBreedingFacility']
if ($null -eq $processor -or $null -eq $breeder) {
    throw 'Both stable custom building IDs must exist'
}

Assert-Equal $processor.Technology.LevelCap 1 'Processor technology display level'
Assert-Equal $breeder.Technology.LevelCap 1 'Breeder technology display level'
Assert-Equal $processor.Technology.IsBossTechnology $false 'Processor must use ordinary technology'
Assert-Equal $breeder.Technology.IsBossTechnology $false 'Breeder must use ordinary technology'
Assert-Equal $processor.BuildingData.ContainsKey('RequiredEnergyType') $false 'Processor must not require power in the test build'
Assert-Equal $breeder.BuildingData.ContainsKey('RequiredEnergyType') $false 'Breeder must not require power in the test build'
Assert-Equal $processor.BuildingData.ContainsKey('ConsumeEnergySpeed') $false 'Processor power draw must be absent in the test build'
Assert-Equal $breeder.BuildingData.ContainsKey('ConsumeEnergySpeed') $false 'Breeder power draw must be absent in the test build'
Assert-Equal $processor.BlueprintClassName 'BP_BuildObject_MultiElectricHatchingPalEgg' 'Processor Blueprint class'
Assert-Equal $breeder.BlueprintClassName 'BP_BuildObject_MultiElectricHatchingPalEggWithBreed' 'Breeder Blueprint class'
Assert-Equal $processor.BlueprintClassSoft '/Game/Pal/Blueprint/MapObject/BuildObject/BP_BuildObject_MultiElectricHatchingPalEgg.BP_BuildObject_MultiElectricHatchingPalEgg_C' 'Processor Blueprint path'
Assert-Equal $breeder.BlueprintClassSoft '/Game/Pal/Blueprint/MapObject/BuildObject/BP_BuildObject_MultiElectricHatchingPalEggWithBreed.BP_BuildObject_MultiElectricHatchingPalEggWithBreed_C' 'Breeder Blueprint path'
Assert-Equal $processor.ContainsKey('Assignments') $false 'Processor must not request Pal workers'
Assert-Equal $breeder.Assignments.Count 1 'Breeder assignment row count'
Assert-Equal $breeder.Assignments[0].WorkSuitability 'Anyone' 'Breeder assignment must accept either parent regardless of work suitability'
Assert-Equal $breeder.Assignments[0].WorkSuitabilityRank 0 'Breeder assignment must not require a work-suitability rank'
Assert-Equal $breeder.Assignments[0].WorkType 'AncientBreedFarm' 'Breeder parent assignment work type'
Assert-Equal $breeder.Assignments[0].WorkerMaxNum 2 'Breeder must reserve exactly two parent slots'
Assert-Equal $breeder.Assignments[0].bBaseCampWorkerWorkable $true 'Breeder parents must be assignable from the base'
Assert-Equal $breeder.Assignments[0].bPlayerWorkable $false 'Breeder must not request player work'

$expectedProcessor = @(
    ,@('Wood', 1)
)
$expectedBreeder = @(
    ,@('Wood', 1)
)

function Assert-Materials {
    param(
        [Parameter(Mandatory)]$Building,
        [Parameter(Mandatory)]$Expected,
        [Parameter(Mandatory)][string]$Label
    )
    for ($index = 1; $index -le $Expected.Count; $index++) {
        Assert-Equal $Building.BuildingData["Material${index}_Id"] $Expected[$index - 1][0] "$Label material $index ID"
        Assert-Equal $Building.BuildingData["Material${index}_Count"] $Expected[$index - 1][1] "$Label material $index count"
    }
    $firstUnexpectedIndex = $Expected.Count + 1
    if ($Building.BuildingData.Keys | Where-Object { $_ -match "^Material[$firstUnexpectedIndex-9]_" }) {
        throw "$Label contains an unexpected extra construction material"
    }
}

Assert-Materials $processor $expectedProcessor 'Processor'
Assert-Materials $breeder $expectedBreeder 'Breeder'

if (($processor.BlueprintClassSoft + $breeder.BlueprintClassSoft) -match '/Game/PalResourceFactory/') {
    throw 'The login-isolation build must not load a custom child Blueprint'
}

$nativeText = Get-Content -Raw -LiteralPath $nativeSource
foreach ($requiredNativeText in @(
    '0.3.4-interaction-bootstrap-preview',
    'global_event_hook=false',
    'processing hooks ready',
    'takeover queued facility=',
    'if (!clear_egg_slot(runtime, slot_index))',
    'if (!remove_rep_info(model, slot_index))'
)) {
    if (-not $nativeText.Contains($requiredNativeText)) {
        throw "Native takeover invariant is missing: $requiredNativeText"
    }
}
if ($nativeText -match 'reward committed but hatch record removal failed') {
    throw 'The obsolete reward-before-hatch-removal transaction is still present'
}
if ($nativeText -match 'RegisterProcessEventPreCallback') {
    throw 'The login-unsafe global ProcessEvent hook must not be registered'
}

Write-Host 'validate_project: PASS'
