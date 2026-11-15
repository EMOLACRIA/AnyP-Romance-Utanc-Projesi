#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Actor.h"
#include "RomanceCourse.generated.h"

class UArrowComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

UENUM()
enum class ERomancePointKind : uint8
{
    Start,      // where a run begins
    Checkpoint, // R returns to the last one reached; Order sets the sequence
    Date,       // meeting spot; Order 0, 1, 2 = Ari, Bea, Jules
    Pad,        // launches the runner to PadTarget
    Marker,     // no gameplay; carries a hint or a tag
    Anchor      // slingshot grab point; place it on the ring. Aiming near it snaps the rope to it
};

// A course point placed in the level. The actor sits on the walking surface (the runner's feet)
// and its arrow shows the facing used for respawns. The game mode reads every point when play starts.
//
// Other things the game reads from the level, by tag:
//   RideWall     (actor or component)  surface that can be wall-run and kicked off
//   BlastSwitch  (actor)               triggered by a boost charge; opens actors sharing its other tags
//   TitleCamera  (camera actor)        view behind the title screen
//   FallLine     (any actor)           its height replaces the automatic fall line
UCLASS()
class ARomancePoint final : public AActor
{
    GENERATED_BODY()

public:
    ARomancePoint();

    UPROPERTY(EditAnywhere, Category = "Romance")
    ERomancePointKind Kind = ERomancePointKind::Checkpoint;

    UPROPERTY(EditAnywhere, Category = "Romance")
    int32 Order = 0;

    // Checkpoint name, or the date's place name.
    UPROPERTY(EditAnywhere, Category = "Romance")
    FString Label;

    // Shown at the bottom of the screen while this is the nearest point with a hint.
    UPROPERTY(EditAnywhere, Category = "Romance")
    FString Hint;

    // How close the runner's feet must come, in cm.
    UPROPERTY(EditAnywhere, Category = "Romance")
    float Radius = 250.f;

    // Dates only: seconds on the run clock, and how long they wait after it. 0 keeps the original schedule.
    UPROPERTY(EditAnywhere, Category = "Romance")
    float Deadline = 0.f;

    UPROPERTY(EditAnywhere, Category = "Romance")
    float Grace = 0.f;

    // Pads only: landing spot, relative to this actor.
    UPROPERTY(EditAnywhere, Category = "Romance", meta = (MakeEditWidget))
    FVector PadTarget = FVector(0, 0, 400);

    FVector Feet() const { return GetActorLocation(); }
    float Yaw() const { return GetActorRotation().Yaw; }

private:
#if WITH_EDITORONLY_DATA
    UPROPERTY()
    TObjectPtr<UArrowComponent> Arrow;
#endif
};

// A mesh that moves when a BlastSwitch sharing one of its tags is triggered: a gate door that slides up
// and gets out of the way, or a lift platform that rises and stays. It returns home when a run restarts.
UCLASS()
class ARomanceMover final : public AStaticMeshActor
{
    GENERATED_BODY()

public:
    ARomanceMover();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // World-space offset from the placed position to the open position, in cm.
    UPROPERTY(EditAnywhere, Category = "Romance")
    FVector Travel = FVector(0, 0, 230);

    UPROPERTY(EditAnywhere, Category = "Romance")
    float Seconds = .45f;

    // Doors: disappear once fully open so nothing is left hanging in the way.
    UPROPERTY(EditAnywhere, Category = "Romance")
    bool bVanishWhenOpen = false;

    void Open();
    void ResetMover();
    bool IsOpen() const { return bOpen; }

private:
    void Place();

    FVector Home = FVector::ZeroVector;
    float Alpha = 0;
    bool bOpen = false;
};

// Scenery drawn as instances, with no collision: a background building (a hollow shell of building blocks
// with a tiled roof) or, with bFlatGrid, one mesh repeated on a flat grid (the sea). One actor per building
// keeps a whole skyline cheap. Call Rebuild after changing the properties from a script.
UCLASS()
class ARomanceCityBlock final : public AActor
{
    GENERATED_BODY()

public:
    ARomanceCityBlock();
    virtual void OnConstruction(const FTransform& Transform) override;

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Romance")
    void Rebuild();

    UPROPERTY(EditAnywhere, Category = "Romance")
    TObjectPtr<UStaticMesh> BlockMesh;

    UPROPERTY(EditAnywhere, Category = "Romance")
    TObjectPtr<UStaticMesh> RoofMesh;

    // Replaces the block mesh's own material, for example a tinted facade.
    UPROPERTY(EditAnywhere, Category = "Romance")
    TObjectPtr<UMaterialInterface> BlockMaterial;

    UPROPERTY(EditAnywhere, Category = "Romance")
    int32 CellsX = 2;

    UPROPERTY(EditAnywhere, Category = "Romance")
    int32 CellsY = 2;

    UPROPERTY(EditAnywhere, Category = "Romance")
    int32 Floors = 3;

    // Spacing between instances, in cm.
    UPROPERTY(EditAnywhere, Category = "Romance")
    float Cell = 400.f;

    UPROPERTY(EditAnywhere, Category = "Romance")
    float Storey = 400.f;

    UPROPERTY(EditAnywhere, Category = "Romance")
    FVector InstanceScale = FVector::OneVector;

    UPROPERTY(EditAnywhere, Category = "Romance")
    bool bFlatGrid = false;

    // Set by AddPieces: the instances were laid out by a script, so Rebuild leaves them alone.
    UPROPERTY(EditAnywhere, Category = "Romance")
    bool bManual = false;

    // Scripted scenery: adds instances of Mesh (drawn with Material, or the mesh's own when null) at
    // transforms relative to this actor. Each mesh and material pair shares one instanced component,
    // so a whole district of mixed facades, street tiles and props costs a handful of components.
    // CullDistance (cm) hides small props far away; 0 never culls.
    UFUNCTION(BlueprintCallable, Category = "Romance")
    void AddPieces(UStaticMesh* Mesh, UMaterialInterface* Material, const TArray<FTransform>& Transforms,
        bool bShadow = true, float CullDistance = 0.f);

    UFUNCTION(BlueprintCallable, Category = "Romance")
    void ClearPieces();

private:
    UPROPERTY(VisibleAnywhere, Category = "Romance")
    TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Pieces;

    UPROPERTY(VisibleAnywhere, Category = "Romance")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Blocks;

    UPROPERTY(VisibleAnywhere, Category = "Romance")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Roofs;
};
