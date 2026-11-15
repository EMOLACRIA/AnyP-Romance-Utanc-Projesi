#include "RomanceRunner.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "RomanceMode.h"
#include "UObject/ConstructorHelpers.h"

// Movement feel. Everything is in centimetres at real scale: a 176 cm runner on the parkour pack's
// 4 m building grid, with gravity at -2600 (Config/DefaultEngine.ini).
// The pack's assumptions these were sized against: 4 m running gap, 3 m wall-run walls,
// 2 m x 6 m wall-jump shaft, 1.0 m slide gap, 0.75-1.2 m vault obstacles, 4.3 m storeys.
namespace Tune
{
    // Ground. A flat running jump covers about 6 m, so a 4 m gap is comfortable and 8 m needs extra speed.
    constexpr float RunSpeed = 920.f;
    constexpr float CrouchSpeed = 300.f;
    constexpr float GroundAccel = 4200.f;
    constexpr float GroundBrake = 2400.f;
    constexpr float GroundFriction = 7.2f;
    constexpr float JumpSpeed = 855.f;        // 1.4 m apex, 0.66 s in the air
    constexpr float StepHeight = 40.f;
    constexpr float CoyoteTime = .11f;
    constexpr float JumpBuffer = .15f;

    // Air: strafing adds speed up to AirWishSpeed along the held direction without touching momentum.
    constexpr float AirWishSpeed = 300.f;
    constexpr float AirAccel = 2400.f;
    constexpr float AirMaxSpeed = 2200.f;

    // Slide
    constexpr float SlideEnter = 600.f;       // speed needed to start sliding
    constexpr float SlideKeep = 220.f;        // below this the slide ends
    constexpr float SlideDrag = 100.f;
    constexpr float SlideDownhill = 1000.f;   // times the slope's sine
    constexpr float SlideTurn = 86.f;
    constexpr float SlideJumpBoost = 340.f;

    // Wall run and wall kick
    constexpr float WallReach = 150.f;        // from the capsule centre
    constexpr float RideSpeed = 1250.f;
    constexpr float MaxRide = 2.f;
    constexpr float RideMinEntry = 350.f;
    constexpr float RideMinAlong = .5f;       // share of the approach that must run along the wall
    constexpr float RideSink = 50.f;
    constexpr float RideSinkGain = 30.f;
    constexpr float RideStick = 80.f;
    constexpr float KickAway = 700.f;
    constexpr float KickUp = 1000.f;          // about 1.9 m per kick; three or four climb the 6 m shaft
    constexpr float KickMaxAlong = 1400.f;
    constexpr float KickReattach = .35f;

    // Slingshot
    constexpr float SlingDuration = 4.5f;
    constexpr float SlingReel = 400.f;
    constexpr float SlingMinRope = 250.f;
    constexpr float SlingMinReach = 250.f;
    constexpr float SlingPump = 800.f;
    constexpr float SlingReleaseHop = 500.f;

    // Rocket jump: a charge at the feet lifts about 5.5 m, one storey with room to spare.
    constexpr float BlastRadius = 600.f;
    constexpr float BlastNear = 1900.f;
    constexpr float BlastFar = 700.f;
    constexpr float SwitchRadius = 350.f;
    constexpr float ThrowSpeed = 3000.f;
}

namespace
{
    constexpr uint8 WallRide = 1;
}

URomanceMovement::URomanceMovement()
{
    MaxWalkSpeed = Tune::RunSpeed;
    MaxWalkSpeedCrouched = Tune::CrouchSpeed;
    MaxAcceleration = Tune::GroundAccel;
    BrakingDecelerationWalking = Tune::GroundBrake;
    GroundFriction = Tune::GroundFriction;
    JumpZVelocity = Tune::JumpSpeed;
    AirControl = 1.f;
    AirControlBoostMultiplier = 0;
    FallingLateralFriction = 0;
    BrakingDecelerationFalling = 0;
    MaxStepHeight = Tune::StepHeight;
    SetWalkableFloorAngle(53);
    NavAgentProps.bCanCrouch = true;
    bCanWalkOffLedgesWhenCrouching = true;
    SetCrouchedHalfHeight(ARomanceRunner::CrouchedHalfHeight);
}

void URomanceMovement::RequestJump()
{
    BufferedJump = Tune::JumpBuffer;
    bJumpHeld = true;
}

void URomanceMovement::ReleaseJump()
{
    bJumpReleased = bJumpHeld && IsWallRiding();
    bJumpHeld = false;
}

void URomanceMovement::SetSlideHeld(bool bHeld)
{
    bSlideHeld = bHeld;
    bWantsToCrouch = bHeld;
}

void URomanceMovement::ClearParkour()
{
    bJumpHeld = false;
    bJumpReleased = false;
    SetSlideHeld(false);
    bSliding = false;
    RideNormal = FVector::ZeroVector;
    RideDirection = FVector::ZeroVector;
    RideTime = 0;
    ReattachTime = 0;
    CoyoteTime = 0;
    BufferedJump = 0;
    bSling = false;
    SlingTime = 0;
}

void URomanceMovement::StartSling(const FVector& Anchor)
{
    SlingAnchor = Anchor;
    RopeLength = FMath::Max(Tune::SlingMinRope, static_cast<float>(FVector::Dist(Anchor, UpdatedComponent->GetComponentLocation())));
    SlingTime = Tune::SlingDuration;
    bSling = true;
    bSliding = false;
    if (IsWallRiding())
    {
        SetMovementMode(MOVE_Falling);
    }
    Sound(static_cast<uint8>(ERomanceCue::Wall));
}

void URomanceMovement::StopSling(bool bBoost)
{
    if (!bSling)
    {
        return;
    }
    bSling = false;
    SlingTime = 0;
    if (bBoost && IsFalling())
    {
        // Letting go with a jump keeps the swing and adds a hop.
        Velocity.Z += Tune::SlingReleaseHop;
        Sound(static_cast<uint8>(ERomanceCue::Jump));
    }
}

void URomanceMovement::Blast(const FVector& Origin)
{
    const FVector Offset = UpdatedComponent->GetComponentLocation() - Origin;
    const float Distance = Offset.Size();
    if (Distance > Tune::BlastRadius)
    {
        return;
    }
    // Mostly away from the blast, always with some lift, so a charge at the feet is a rocket jump.
    const FVector Away = Distance > 1.f ? Offset / Distance : FVector::UpVector;
    const FVector Push = (Away + FVector(0, 0, .55f)).GetSafeNormal();
    SetMovementMode(MOVE_Falling);
    Velocity.Z = FMath::Max(Velocity.Z, 0.);
    Velocity += Push * FMath::Lerp(Tune::BlastNear, Tune::BlastFar, Distance / Tune::BlastRadius);
    bSliding = false;
    ReattachTime = FMath::Max(ReattachTime, .2f);
    CoyoteTime = 0;
}

bool URomanceMovement::IsWallRiding() const
{
    return MovementMode == MOVE_Custom && CustomMovementMode == WallRide;
}

void URomanceMovement::Sound(uint8 Cue) const
{
    if (const auto* Mode = GetWorld()->GetAuthGameMode<ARomanceMode>())
    {
        Mode->Cue(static_cast<ERomanceCue>(Cue));
    }
}

void URomanceMovement::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
    if (IsFalling())
    {
        // Accelerate in the requested direction without discarding the incoming momentum.
        const FVector Wish = Acceleration.GetSafeNormal2D();
        const float Gain = Tune::AirWishSpeed - FVector::DotProduct(Velocity, Wish);
        if (Gain > 0)
        {
            Velocity += Wish * FMath::Min(Tune::AirAccel * DeltaTime, Gain);
        }
        Velocity = Velocity.GetClampedToMaxSize2D(Tune::AirMaxSpeed);
        return;
    }

    if (bSliding && IsMovingOnGround())
    {
        const FVector Wish = Acceleration.GetSafeNormal2D();
        const float Speed = Velocity.Size2D();
        const FVector Heading = FMath::VInterpNormalRotationTo(Velocity.GetSafeNormal2D(),
            Wish.IsNearlyZero() ? Velocity.GetSafeNormal2D() : Wish, DeltaTime, Tune::SlideTurn);
        const FVector Downhill = FVector::VectorPlaneProject(FVector::DownVector, CurrentFloor.HitResult.ImpactNormal);
        Velocity = Heading * FMath::Max(0.f, Speed - Tune::SlideDrag * DeltaTime) + Downhill * (Tune::SlideDownhill * DeltaTime);
        Velocity.Z = 0;
        return;
    }

    Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

bool URomanceMovement::FindRideWall(FHitResult& Wall) const
{
    const FVector Center = UpdatedComponent->GetComponentLocation();
    FCollisionQueryParams Trace(SCENE_QUERY_STAT(RomanceRideWall), false, CharacterOwner);
    float Distance = Tune::WallReach;
    for (int32 Index = 0; Index < 12; ++Index)
    {
        const float Theta = UE_TWO_PI * Index / 12;
        const FVector Offset(FMath::Cos(Theta) * Tune::WallReach, FMath::Sin(Theta) * Tune::WallReach, 0);
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByChannel(Hit, Center, Center + Offset, ECC_Visibility, Trace) || !Hit.Component.IsValid())
        {
            continue;
        }
        // The level marks rideable walls with a RideWall tag, on the actor or on the component.
        const AActor* Owner = Hit.GetActor();
        const bool bRideable = Hit.Component->ComponentHasTag(TEXT("RideWall")) || (Owner && Owner->ActorHasTag(TEXT("RideWall")));
        // Imported walls face many directions. Test the actual surface normal.
        if (bRideable && FMath::Abs(Hit.ImpactNormal.Z) < .60f && Hit.Distance < Distance)
        {
            Distance = Hit.Distance;
            Wall = Hit;
        }
    }
    return Wall.bBlockingHit;
}

void URomanceMovement::EnterRide(const FHitResult& Wall)
{
    RideNormal = Wall.ImpactNormal.GetSafeNormal2D();
    RideDirection = FVector::VectorPlaneProject(Velocity.GetSafeNormal2D(), RideNormal).GetSafeNormal2D();
    if (RideDirection.IsNearlyZero())
    {
        return;
    }

    RideTime = Tune::MaxRide;
    Velocity = RideDirection * FMath::Max(Tune::RideSpeed, static_cast<float>(Velocity.Size2D()));
    BufferedJump = 0;
    CoyoteTime = 0;
    SetMovementMode(MOVE_Custom, WallRide);
    Sound(static_cast<uint8>(ERomanceCue::Wall));
}

void URomanceMovement::JumpFromWall(const FVector& Normal)
{
    // Keep whatever speed runs along the wall, so a kick out of a wall run carries the run forward
    // and a kick straight off a facing wall (a shaft) goes across and up without drifting sideways.
    const FVector Along = FVector::VectorPlaneProject(FVector(Velocity.X, Velocity.Y, 0), Normal).GetClampedToMaxSize2D(Tune::KickMaxAlong);

    SetMovementMode(MOVE_Falling);
    Velocity = FVector(Along.X, Along.Y, 0) + Normal * Tune::KickAway + FVector(0, 0, Tune::KickUp);
    ReattachTime = Tune::KickReattach;
    BufferedJump = 0;
    CoyoteTime = 0;
    Sound(static_cast<uint8>(ERomanceCue::Jump));
}

void URomanceMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
    Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
    const bool bOnGround = IsMovingOnGround();
    CoyoteTime = bOnGround ? Tune::CoyoteTime : FMath::Max(0.f, CoyoteTime - DeltaSeconds);
    ReattachTime = FMath::Max(0.f, ReattachTime - DeltaSeconds);

    const bool bJumpPending = BufferedJump > 0;
    BufferedJump = FMath::Max(0.f, BufferedJump - DeltaSeconds);
    const bool bRelease = bJumpReleased;
    bJumpReleased = false;
    bSliding = bOnGround && bSlideHeld
        && Velocity.SizeSquared2D() >= FMath::Square(bSliding ? Tune::SlideKeep : Tune::SlideEnter);

    if (bSling)
    {
        SlingTime -= DeltaSeconds;
        if (bJumpPending || SlingTime <= 0)
        {
            StopSling(bJumpPending);
            BufferedJump = 0;
            return;
        }

        RopeLength = FMath::Max(Tune::SlingMinRope, RopeLength - Tune::SlingReel * DeltaSeconds);
        const FVector ToAnchor = SlingAnchor - UpdatedComponent->GetComponentLocation();
        const float Distance = ToAnchor.Size();
        if (Distance > RopeLength)
        {
            // The rope is taut: leave the ground, drop any speed away from the anchor and follow the reel.
            const FVector Dir = ToAnchor / Distance;
            if (!IsFalling())
            {
                SetMovementMode(MOVE_Falling);
            }
            const float Radial = FVector::DotProduct(Velocity, Dir);
            const float Wanted = FMath::Min(Tune::SlingReel, (Distance - RopeLength) / FMath::Max(DeltaSeconds, UE_KINDA_SMALL_NUMBER));
            if (Radial < Wanted)
            {
                Velocity += Dir * (Wanted - Radial);
            }
            // Pump the swing with the movement keys, along the rope's tangent only.
            const FVector Wish = Acceleration.GetSafeNormal();
            Velocity += FVector::VectorPlaneProject(Wish, Dir) * (Tune::SlingPump * DeltaSeconds);
        }
        if (IsFalling())
        {
            return;
        }
    }

    if ((bOnGround && (bJumpPending || (bJumpHeld && !bSlideHeld))) || (bJumpPending && CoyoteTime > 0))
    {
        const FVector Wish = Acceleration.GetSafeNormal2D();
        Velocity += bSliding ? Wish * Tune::SlideJumpBoost : FVector::ZeroVector;
        Velocity.Z = JumpZVelocity;
        SetMovementMode(MOVE_Falling);
        bSliding = false;
        CoyoteTime = 0;
        BufferedJump = 0;
        Sound(static_cast<uint8>(ERomanceCue::Jump));
        return;
    }

    if (IsWallRiding() && (bJumpPending || bRelease))
    {
        JumpFromWall(RideNormal);
        return;
    }

    if (!IsFalling() || ReattachTime > 0)
    {
        return;
    }

    FHitResult Wall;
    if (!FindRideWall(Wall))
    {
        return;
    }

    const FVector Normal = Wall.ImpactNormal.GetSafeNormal2D();
    if (bJumpPending)
    {
        JumpFromWall(Normal);
        return;
    }
    // A ride starts only when the runner is travelling along the wall while holding forward.
    // Running straight at a wall does not start one, so facing walls stay free for wall kicks.
    const FVector Wish = Acceleration.GetSafeNormal2D();
    const FVector Forward = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
    const FVector Heading = Velocity.GetSafeNormal2D();
    if (FVector::DotProduct(Wish, Forward) > .40 && Velocity.SizeSquared2D() > FMath::Square(Tune::RideMinEntry)
        && FVector::DotProduct(Heading, Normal) < .39
        && FVector::VectorPlaneProject(Heading, Normal).Size2D() >= Tune::RideMinAlong)
    {
        EnterRide(Wall);
    }
}

void URomanceMovement::PhysCustom(float DeltaTime, int32 Iterations)
{
    if (!IsWallRiding() || DeltaTime < MIN_TICK_TIME)
    {
        return;
    }

    RideTime -= DeltaTime;
    const FVector Wish = Acceleration.GetSafeNormal2D();
    const FVector Forward = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
    FHitResult Wall;
    if (RideTime <= 0 || FVector::DotProduct(Wish, Forward) <= .36 || !FindRideWall(Wall))
    {
        ReattachTime = Tune::KickReattach;
        SetMovementMode(MOVE_Falling);
        StartNewPhysics(DeltaTime, Iterations);
        return;
    }

    RideNormal = Wall.ImpactNormal.GetSafeNormal2D();
    const FVector Tangent = FVector::VectorPlaneProject(RideDirection, RideNormal).GetSafeNormal2D();
    RideDirection = Tangent.IsNearlyZero() ? RideDirection : Tangent;
    Velocity = RideDirection * FMath::Max(Tune::RideSpeed, static_cast<float>(Velocity.Size2D()));
    Velocity.Z = -Tune::RideSink - (Tune::MaxRide - RideTime) * Tune::RideSinkGain;
    const FVector Delta = (Velocity - RideNormal * Tune::RideStick) * DeltaTime;
    FHitResult Hit;
    SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
    if (Hit.IsValidBlockingHit())
    {
        if (IsWalkable(Hit))
        {
            ProcessLanded(Hit, DeltaTime * (1.f - Hit.Time), Iterations);
            return;
        }
        SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
    }
}

ARomanceRunner::ARomanceRunner(const FObjectInitializer& Initializer)
    : Super(Initializer.SetDefaultSubobjectClass<URomanceMovement>(ACharacter::CharacterMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, StandingHalfHeight);
    BaseEyeHeight = StandingEyeOffset;
    CrouchedEyeHeight = CrouchedEyeOffset;
    bUseControllerRotationYaw = true;
    Eyes = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPerson"));
    Eyes->SetupAttachment(GetCapsuleComponent());
    Eyes->SetRelativeLocation(FVector(0, 0, CameraHeight));
    Eyes->bUsePawnControlRotation = true;
    Eyes->bOverrideAspectRatioAxisConstraint = true;
    Eyes->SetAspectRatioAxisConstraint(AspectRatio_MaintainYFOV);
    Eyes->SetFieldOfView(91.f);
    Eyes->PostProcessSettings.bOverride_AutoExposureMethod = true;
    Eyes->PostProcessSettings.AutoExposureMethod = AEM_Manual;
    Eyes->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Eyes->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    Eyes->PostProcessSettings.bOverride_AutoExposureBias = true;
    Eyes->PostProcessSettings.AutoExposureBias = -.15f;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Ball(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Rope = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rope"));
    Hook = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hook"));
    Rope->SetStaticMesh(Cylinder.Object);
    Hook->SetStaticMesh(Ball.Object);
    for (UStaticMeshComponent* Part : {Rope.Get(), Hook.Get()})
    {
        Part->SetupAttachment(GetCapsuleComponent());
        Part->SetUsingAbsoluteLocation(true);
        Part->SetUsingAbsoluteRotation(true);
        Part->SetUsingAbsoluteScale(true);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCastShadow(false);
        Part->SetVisibility(false);
    }
    Hook->SetWorldScale3D(FVector(.16f));
}

void ARomanceRunner::BeginPlay()
{
    Super::BeginPlay();
    // Project art replaces the placeholder shapes when it exists.
    if (FPackageName::DoesPackageExist(TEXT("/Game/Romance/M_Rope")))
    {
        Rope->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Romance/M_Rope.M_Rope")));
    }
    if (FPackageName::DoesPackageExist(TEXT("/Game/ParkourPack/SM_Sling_Hook")))
    {
        if (UStaticMesh* HookMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ParkourPack/SM_Sling_Hook.SM_Sling_Hook")))
        {
            Hook->SetStaticMesh(HookMesh);
            Hook->SetWorldScale3D(FVector(1.6f));
        }
    }
}

URomanceMovement* ARomanceRunner::Parkour() const
{
    return CastChecked<URomanceMovement>(GetCharacterMovement());
}

void ARomanceRunner::SetRunning(bool bRunning)
{
    Parkour()->ClearParkour();
    Parkour()->StopMovementImmediately();
    if (bRunning)
    {
        Parkour()->SetMovementMode(MOVE_Falling);
    }
    else
    {
        Parkour()->DisableMovement();
    }
}

void ARomanceRunner::RespawnAt(FVector Feet, float Yaw)
{
    Parkour()->ClearParkour();
    UnCrouch();
    Parkour()->StopMovementImmediately();
    const FRotator Heading(0, Yaw, 0);
    SetActorLocationAndRotation(Feet + FVector(0, 0, StandingHalfHeight + 3),
        Heading, false, nullptr, ETeleportType::TeleportPhysics);
    if (GetController())
    {
        GetController()->SetControlRotation(Heading);
    }
    Parkour()->SetMovementMode(MOVE_Falling);
    CameraHeight = StandingEyeOffset;
    Eyes->SetRelativeLocation(FVector(0, 0, CameraHeight));
    LandingDip = 0;
    CameraRoll = 0;
}

void ARomanceRunner::ThrowCharge()
{
    if (ChargeTimer > 0)
    {
        return;
    }
    const FRotator Aim = GetControlRotation();
    const FVector From = GetPawnViewLocation() + Aim.Vector() * 60.f;
    FActorSpawnParameters Spawn;
    Spawn.Owner = this;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if (auto* Charge = GetWorld()->SpawnActor<ARomanceCharge>(From, Aim, Spawn))
    {
        // Thrown where the player looks, carrying most of the runner's own speed.
        Charge->Throw(Aim.Vector() * Tune::ThrowSpeed + GetVelocity() * .6f + FVector(0, 0, 150));
        ChargeTimer = ChargeDelay;
    }
}

bool ARomanceRunner::FindSlingPoint(FVector& Point) const
{
    const FVector From = GetPawnViewLocation();
    // Anchor rings first: aiming near one is enough. Otherwise the rope takes whatever surface is in the crosshair.
    if (const auto* Mode = GetWorld()->GetAuthGameMode<ARomanceMode>();
        Mode && Mode->FindAnchor(From, GetControlRotation().Vector(), SlingRange, Tune::SlingMinReach, Point))
    {
        return true;
    }
    const FVector To = From + GetControlRotation().Vector() * SlingRange;
    FCollisionQueryParams Trace(SCENE_QUERY_STAT(RomanceSling), false, this);
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Trace) || Hit.Distance < Tune::SlingMinReach)
    {
        return false;
    }
    Point = Hit.ImpactPoint;
    return true;
}

void ARomanceRunner::StartSling()
{
    FVector Point;
    if (FindSlingPoint(Point))
    {
        Parkour()->StartSling(Point);
    }
}

void ARomanceRunner::StopSling()
{
    Parkour()->StopSling(false);
}

ARomanceCharge::ARomanceCharge()
{
    PrimaryActorTick.bCanEverTick = true;
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    SetRootComponent(Body);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Ball(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Ball.Succeeded())
    {
        Body->SetStaticMesh(Ball.Object);
    }
    Body->SetWorldScale3D(FVector(.25f));
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetCastShadow(false);
}

ARomanceBurst::ARomanceBurst()
{
    PrimaryActorTick.bCanEverTick = true;
    Shell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell"));
    SetRootComponent(Shell);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Ball(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Shell->SetStaticMesh(Ball.Object);
    Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Shell->SetCastShadow(false);
    Shell->SetVisibility(false);
    Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
    Flash->SetupAttachment(Shell);
    Flash->SetUsingAbsoluteScale(true);
    Flash->bUseInverseSquaredFalloff = false;
    Flash->SetLightFalloffExponent(2.f);
    Flash->SetAttenuationRadius(1300.f);
    Flash->SetLightColor(FLinearColor(1.f, .55f, .2f));
    Flash->SetCastShadows(false);
    Flash->SetIntensity(0.f);
}

void ARomanceBurst::BeginPlay()
{
    Super::BeginPlay();
    if (FPackageName::DoesPackageExist(TEXT("/Game/Romance/M_Burst")))
    {
        if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Romance/M_Burst.M_Burst")))
        {
            Paint = Shell->CreateDynamicMaterialInstance(0, Base);
            Shell->SetVisibility(true);
        }
    }
}

void ARomanceBurst::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    constexpr float Life = .32f;
    Age += DeltaSeconds;
    const float T = FMath::Clamp(Age / Life, 0.f, 1.f);
    const float Out = 1.f - FMath::Square(1.f - T);
    // The engine sphere is 1 m across; grow to most of the blast radius.
    Shell->SetWorldScale3D(FVector(FMath::Lerp(.8f, Tune::BlastRadius / 50.f * .7f, Out)));
    if (Paint)
    {
        Paint->SetScalarParameterValue(TEXT("Fade"), 1.f - T);
    }
    Flash->SetIntensity(26.f * FMath::Square(1.f - T));
    if (T >= 1.f)
    {
        Destroy();
    }
}

void ARomanceCharge::BeginPlay()
{
    Super::BeginPlay();
    // The parkour pack's charge replaces the placeholder ball once the pack is imported.
    static const TCHAR* PackCharge = TEXT("/Game/ParkourPack/SM_Boost_Charge.SM_Boost_Charge");
    if (FPackageName::DoesPackageExist(TEXT("/Game/ParkourPack/SM_Boost_Charge")))
    {
        if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, PackCharge))
        {
            Body->SetStaticMesh(Mesh);
            Body->SetWorldScale3D(FVector(1.f));
        }
    }
}

void ARomanceCharge::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Velocity.Z += GetWorld()->GetGravityZ() * DeltaSeconds;
    const FVector From = GetActorLocation();
    const FVector To = From + Velocity * DeltaSeconds;
    FCollisionQueryParams Trace(SCENE_QUERY_STAT(RomanceCharge), false, this);
    Trace.AddIgnoredActor(GetOwner());
    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Trace))
    {
        Burst(Hit.ImpactPoint + Hit.ImpactNormal * 4.f);
        return;
    }
    SetActorLocation(To);
    AddActorLocalRotation(FRotator(540.f * DeltaSeconds, 0, 0));
    Fuse -= DeltaSeconds;
    if (Fuse <= 0)
    {
        Burst(To);
    }
}

void ARomanceCharge::Burst(const FVector& At)
{
    auto* Mode = GetWorld()->GetAuthGameMode<ARomanceMode>();
    if (auto* Runner = Cast<ARomanceRunner>(GetOwner()))
    {
        Runner->Parkour()->Blast(At);
        // The closer the burst, the harder the camera is shaken and the wider the view kicks.
        const float Near = 1.f - FMath::Clamp(FVector::Dist(Runner->GetActorLocation(), At) / 1400.f, 0.f, 1.f);
        Runner->AddShake(.85f * Near);
        Runner->KickFov(9.f * Near);
    }
    GetWorld()->SpawnActor<ARomanceBurst>(At, FRotator::ZeroRotator);

    // Anything tagged BlastSwitch inside the burst is triggered.
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(RomanceBlast), false, this);
    GetWorld()->OverlapMultiByChannel(Overlaps, At, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Tune::SwitchRadius), Query);
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Other = Overlap.GetActor();
        if (Mode && Other && Other->ActorHasTag(TEXT("BlastSwitch")))
        {
            Mode->SwitchHit(Other);
        }
    }
    if (Mode)
    {
        Mode->Cue(ERomanceCue::Launch);
    }
    Destroy();
}

void ARomanceRunner::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ChargeTimer = FMath::Max(0.f, ChargeTimer - DeltaSeconds);
    const float Speed = GetVelocity().Size2D();
    Stride += DeltaSeconds * 11.f * FMath::Min(Speed / Tune::RunSpeed, 1.3f);
    LandingDip = FMath::FInterpTo(LandingDip, 0.f, DeltaSeconds, 13.f);

    const bool bGround = Parkour()->IsMovingOnGround();
    const float Bob = bGround && !bReduceMotion ? FMath::Sin(Stride) * 2.f * FMath::Min(Speed / 550.f, 1.5f) : 0.f;
    const float Eye = bIsCrouched ? CrouchedEyeOffset : StandingEyeOffset;
    CameraHeight = FMath::FInterpTo(CameraHeight, Eye + Bob - (bReduceMotion ? 0.f : LandingDip), DeltaSeconds, 14.f);
    Eyes->SetRelativeLocation(FVector(0, 0, CameraHeight));
    const float Roll = Parkour()->IsWallRiding() && !bReduceMotion
        ? FVector::DotProduct(GetActorRightVector(), Parkour()->GetRideNormal()) * 10.f : 0.f;
    CameraRoll = FMath::FInterpTo(CameraRoll, Roll, DeltaSeconds, 8.f);
    const float Fov = bReduceMotion ? 91.f : 91.f + FMath::GetMappedRangeValueClamped(FVector2D(600, 1800), FVector2D(0, 9), Speed);
    CameraFov = FMath::FInterpTo(CameraFov, Fov, DeltaSeconds, 6.f);
    FovKick = FMath::FInterpTo(FovKick, 0.f, DeltaSeconds, 5.f);
    Eyes->SetFieldOfView(CameraFov + (bReduceMotion ? 0.f : FovKick));
    Trauma = FMath::Max(0.f, Trauma - DeltaSeconds * 1.7f);

    // Rope from the right hand to the anchor.
    const bool bHanging = Parkour()->IsSlinging();
    Rope->SetVisibility(bHanging);
    Hook->SetVisibility(bHanging);
    if (bHanging)
    {
        const FVector Hand = Eyes->GetComponentLocation() + Eyes->GetComponentRotation().RotateVector(FVector(38, 24, -26));
        const FVector Anchor = Parkour()->GetSlingAnchor();
        const FVector Span = Anchor - Hand;
        Rope->SetWorldLocationAndRotation((Hand + Anchor) * .5f, FRotationMatrix::MakeFromZ(Span).Rotator());
        Rope->SetWorldScale3D(FVector(.035f, .035f, Span.Size() / 100.f));
        Hook->SetWorldLocationAndRotation(Anchor, Span.Rotation());
    }
}

void ARomanceRunner::CalcCamera(float DeltaTime, FMinimalViewInfo& View)
{
    Super::CalcCamera(DeltaTime, View);
    View.Rotation.Roll += CameraRoll;
    if (!bReduceMotion && Trauma > 0)
    {
        const float Shake = Trauma * Trauma;
        const float Time = GetWorld()->GetTimeSeconds() * 26.f;
        View.Rotation.Pitch += FMath::PerlinNoise1D(Time) * 2.4f * Shake;
        View.Rotation.Yaw += FMath::PerlinNoise1D(Time + 37.f) * 2.4f * Shake;
        View.Rotation.Roll += FMath::PerlinNoise1D(Time + 71.f) * 3.2f * Shake;
    }
}

FVector ARomanceRunner::GetPawnViewLocation() const
{
    return Eyes ? Eyes->GetComponentLocation() : Super::GetPawnViewLocation();
}

void ARomanceRunner::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
    // On the ground the capsule centre drops with the crouch; carry the eyes so the view glides down.
    if (Parkour()->IsMovingOnGround())
    {
        CameraHeight += ScaledHalfHeightAdjust;
        Eyes->SetRelativeLocation(FVector(0, 0, CameraHeight));
    }
}

void ARomanceRunner::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
    if (Parkour()->IsMovingOnGround())
    {
        CameraHeight -= ScaledHalfHeightAdjust;
        Eyes->SetRelativeLocation(FVector(0, 0, CameraHeight));
    }
}

void ARomanceRunner::Landed(const FHitResult& Hit)
{
    const float Impact = FMath::Abs(GetVelocity().Z);
    Super::Landed(Hit);
    if (Impact > 300.f)
    {
        LandingDip = FMath::GetMappedRangeValueClamped(FVector2D(300, 2200), FVector2D(1.f, 12.f), Impact);
        AddShake(FMath::GetMappedRangeValueClamped(FVector2D(1100, 2800), FVector2D(0.f, .6f), Impact));
        if (const auto* Mode = GetWorld()->GetAuthGameMode<ARomanceMode>())
        {
            Mode->Cue(ERomanceCue::Land, FMath::Clamp(Impact / 1400.f, .23f, .8f));
        }
    }
}
