#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RomanceRunner.generated.h"

class UCameraComponent;
class UStaticMeshComponent;

UCLASS()
class URomanceMovement final : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    URomanceMovement();
    void RequestJump();
    void ReleaseJump();
    void SetSlideHeld(bool bHeld);
    void ClearParkour();
    bool IsWallRiding() const;
    bool IsSliding() const { return bSliding; }
    FVector GetRideNormal() const { return RideNormal; }

    // Slingshot: hang from a point and swing on a rope that slowly reels in.
    void StartSling(const FVector& Anchor);
    void StopSling(bool bBoost);
    bool IsSlinging() const { return bSling; }
    FVector GetSlingAnchor() const { return SlingAnchor; }

    // Rocket jump: push the runner away from a blast.
    void Blast(const FVector& Origin);

protected:
    virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
    virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
    virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

private:
    bool FindRideWall(FHitResult& Wall) const;
    void EnterRide(const FHitResult& Wall);
    void JumpFromWall(const FVector& Normal);
    void Sound(uint8 Cue) const;

    FVector RideNormal = FVector::ZeroVector;
    FVector RideDirection = FVector::ZeroVector;
    float RideTime = 0;
    float ReattachTime = 0;
    float CoyoteTime = 0;
    float BufferedJump = 0;
    bool bSlideHeld = false;
    bool bSliding = false;
    bool bJumpHeld = false;
    bool bJumpReleased = false;

    FVector SlingAnchor = FVector::ZeroVector;
    float RopeLength = 0;
    float SlingTime = 0;
    bool bSling = false;
};

// The thrown boost charge. It flies on a simple arc and bursts on the first surface it meets.
UCLASS()
class ARomanceCharge final : public AActor
{
    GENERATED_BODY()

public:
    ARomanceCharge();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void Throw(const FVector& InVelocity) { Velocity = InVelocity; }

private:
    void Burst(const FVector& At);

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Body;

    FVector Velocity = FVector::ZeroVector;
    float Fuse = 1.6f;
};

// The burst a charge leaves: a quick expanding shell and a flash of light. Visual only.
UCLASS()
class ARomanceBurst final : public AActor
{
    GENERATED_BODY()

public:
    ARomanceBurst();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Shell;
    UPROPERTY()
    TObjectPtr<class UPointLightComponent> Flash;
    UPROPERTY()
    TObjectPtr<class UMaterialInstanceDynamic> Paint;

    float Age = 0;
};

UCLASS()
class ARomanceRunner final : public ACharacter
{
    GENERATED_BODY()

public:
    // Real scale: a 176 cm capsule with eyes at 156 cm; 88 cm tall while sliding.
    static constexpr float CapsuleRadius = 34.f;
    static constexpr float StandingHalfHeight = 88.f;
    static constexpr float CrouchedHalfHeight = 44.f;
    static constexpr float StandingEyeOffset = 68.f;
    static constexpr float CrouchedEyeOffset = 22.f;

    explicit ARomanceRunner(const FObjectInitializer& Initializer);
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& View) override;
    virtual FVector GetPawnViewLocation() const override;
    virtual void Landed(const FHitResult& Hit) override;
    virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
    virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

    URomanceMovement* Parkour() const;
    void SetRunning(bool bRunning);
    void RespawnAt(FVector Feet, float Yaw);
    void SetReducedMotion(bool bEnabled) { bReduceMotion = bEnabled; }
    // Camera shake, 0..1. It fades by itself and is skipped with reduced motion.
    void AddShake(float Amount) { Trauma = FMath::Clamp(Trauma + Amount, 0.f, 1.f); }
    void KickFov(float Degrees) { FovKick = FMath::Max(FovKick, Degrees); }

    static constexpr float ChargeDelay = 1.4f;
    static constexpr float SlingRange = 3000.f;
    void ThrowCharge();
    float ChargeReady() const { return 1.f - FMath::Clamp(ChargeTimer / ChargeDelay, 0.f, 1.f); }
    bool FindSlingPoint(FVector& Point) const;
    void StartSling();
    void StopSling();

private:
    float ChargeTimer = 0;

    UPROPERTY()
    TObjectPtr<UCameraComponent> Eyes;
    // The slingshot rope and its hook, drawn in the world while hanging.
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Rope;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Hook;

    float Trauma = 0;
    float FovKick = 0;
    float CameraHeight = StandingEyeOffset;
    float CameraRoll = 0;
    float CameraFov = 91;
    float Stride = 0;
    float LandingDip = 0;
    bool bReduceMotion = false;
};
