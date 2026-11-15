#include "RomanceCourse.h"

#include "Components/ArrowComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ARomanceMover::ARomanceMover()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
}

void ARomanceMover::BeginPlay()
{
    Super::BeginPlay();
    Home = GetActorLocation();
}

void ARomanceMover::Open()
{
    bOpen = true;
    SetActorTickEnabled(true);
}

void ARomanceMover::ResetMover()
{
    bOpen = false;
    Alpha = 0;
    SetActorTickEnabled(false);
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    Place();
}

void ARomanceMover::Place()
{
    SetActorLocation(Home + Travel * FMath::SmoothStep(0.f, 1.f, Alpha));
}

void ARomanceMover::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Alpha = FMath::FInterpConstantTo(Alpha, 1.f, DeltaSeconds, 1.f / FMath::Max(Seconds, .05f));
    Place();
    if (Alpha >= 1.f)
    {
        SetActorTickEnabled(false);
        if (bVanishWhenOpen)
        {
            SetActorHiddenInGame(true);
            SetActorEnableCollision(false);
        }
    }
}

ARomancePoint::ARomancePoint()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Point"));
#if WITH_EDITORONLY_DATA
    Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Facing"));
    if (Arrow)
    {
        Arrow->SetupAttachment(RootComponent);
        Arrow->SetRelativeLocation(FVector(0, 0, 20));
        Arrow->ArrowSize = 1.5f;
        Arrow->ArrowColor = FColor(248, 177, 123);
    }
#endif
}

ARomanceCityBlock::ARomanceCityBlock()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Block"));
    // Static like the instances, or they refuse to attach and stay at the world origin.
    RootComponent->SetMobility(EComponentMobility::Static);
    Blocks =CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Blocks"));
    Roofs = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Roofs"));
    for (UHierarchicalInstancedStaticMeshComponent* Part : {Blocks.Get(), Roofs.Get()})
    {
        Part->SetupAttachment(RootComponent);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetMobility(EComponentMobility::Static);
    }
}

void ARomanceCityBlock::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Rebuild();
}

void ARomanceCityBlock::AddPieces(UStaticMesh* Mesh, UMaterialInterface* Material, const TArray<FTransform>& Transforms,
    bool bShadow, float CullDistance)
{
    if (!Mesh || Transforms.IsEmpty())
    {
        return;
    }
    bManual = true;
    UHierarchicalInstancedStaticMeshComponent* Part = nullptr;
    for (UHierarchicalInstancedStaticMeshComponent* Candidate : Pieces)
    {
        if (Candidate && Candidate->GetStaticMesh() == Mesh
            && (Candidate->OverrideMaterials.Num() ? Candidate->OverrideMaterials[0].Get() : nullptr) == Material)
        {
            Part = Candidate;
            break;
        }
    }
    if (!Part)
    {
        Part = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transactional);
        Part->SetMobility(EComponentMobility::Static);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetStaticMesh(Mesh);
        if (Material)
        {
            Part->SetMaterial(0, Material);
        }
        Part->SetCastShadow(bShadow);
        if (CullDistance > 0.f)
        {
            Part->SetCullDistances(FMath::RoundToInt(CullDistance * .85f), FMath::RoundToInt(CullDistance));
        }
        Part->SetupAttachment(RootComponent);
        // An instance component is saved with the actor, like one added by hand in the editor.
        AddInstanceComponent(Part);
        Part->RegisterComponent();
        Pieces.Add(Part);
    }
    Part->AddInstances(Transforms, false, false);
}

void ARomanceCityBlock::ClearPieces()
{
    for (UHierarchicalInstancedStaticMeshComponent* Part : Pieces)
    {
        if (Part)
        {
            RemoveInstanceComponent(Part);
            Part->DestroyComponent();
        }
    }
    Pieces.Reset();
    bManual = false;
}

void ARomanceCityBlock::Rebuild()
{
    if (bManual)
    {
        return;
    }
    Blocks->ClearInstances();
    Roofs->ClearInstances();
    Blocks->SetStaticMesh(BlockMesh);
    Roofs->SetStaticMesh(RoofMesh);
    Blocks->SetMaterial(0, BlockMaterial);
    Blocks->SetCastShadow(!bFlatGrid);
    if (!BlockMesh)
    {
        return;
    }
    for (int32 X = 0; X < CellsX; ++X)
    {
        for (int32 Y = 0; Y < CellsY; ++Y)
        {
            const FVector Base(X * Cell, Y * Cell, 0);
            if (bFlatGrid)
            {
                Blocks->AddInstance(FTransform(FRotator::ZeroRotator, Base, InstanceScale));
                continue;
            }
            // Only the outer ring is ever seen.
            if (X == 0 || Y == 0 || X == CellsX - 1 || Y == CellsY - 1)
            {
                for (int32 Floor = 0; Floor < Floors; ++Floor)
                {
                    Blocks->AddInstance(FTransform(FRotator::ZeroRotator, Base + FVector(0, 0, Floor * Storey), InstanceScale));
                }
            }
            if (RoofMesh)
            {
                Roofs->AddInstance(FTransform(FRotator::ZeroRotator, Base + FVector(0, 0, Floors * Storey + 30.f), InstanceScale));
            }
        }
    }
}
