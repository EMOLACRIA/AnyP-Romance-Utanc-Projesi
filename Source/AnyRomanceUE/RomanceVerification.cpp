#include "RomanceMode.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RomanceCourse.h"
#include "RomanceRunner.h"

// Command-line checks, run with the game closed:
//   -RomanceVerifyRules      date order, clock, lateness, hearts and the ending. Needs no particular level.
//   -RomanceVerifyMovement   jump and rocket jump at the start point, then one stage per actor tagged
//                            VerifyRide, VerifySlide or VerifySling in the level. Each stage places the runner
//                            on that actor, facing its way, and plays the move against the level's real collision.
//   -RomanceVerifyCourse     the course as placed in the level: the runner is set on every checkpoint in order and
//                            must land on solid ground there and register it; then on every date spot, where the
//                            date must open, take its four replies and move on, until the run ends on the results.
// These are mechanic and placement checks. None of them runs the parkour between the points.

void ARomanceMode::VerifyRules()
{
    const auto Require = [](bool Passed, const TCHAR* Description)
    {
        if (!Passed)
        {
            UE_LOG(LogTemp, Fatal, TEXT("ANY_ROMANCE_RULES failed: %s"), Description);
        }
    };

    // The rules cover all three dates even when the loaded level has fewer date points.
    DateCount = Romance::Dates;
    BeginRun(true);
    for (int32 Index = 0; Index < Romance::Dates; ++Index)
    {
        const float Before = RunTime;
        EnterDate(Index);
        Require(Screen == ERomanceScreen::Date && ActiveDate == Index, TEXT("start a date"));
        Tick(2.f);
        Require(RunTime == Before, TEXT("reading pauses the parkour clock"));
        for (int32 Reply = 0; Reply < 4; ++Reply)
        {
            Choose(2);
        }
        Require(FMath::IsNearlyEqual(RunTime, Before + 180.f), TEXT("four three-turn replies cost 180 seconds"));
        Require(DateState[Index].Hearts == 5, TEXT("all original positive responses give five hearts"));
    }
    Require(Screen == ERomanceScreen::Results && TotalHearts() == 15, TEXT("complete the perfect run"));

    BeginRun(true);
    Require(Deadline(0) == 180.f && Deadline(1) == 480.f && Deadline(2) == 780.f, TEXT("original schedule"));
    RunTime = 188.f;
    EnterDate(0);
    Require(DateState[0].Hearts == 0 && Screen == ERomanceScreen::Date, TEXT("late arrival loses a heart"));
    Choose(0);
    Require(DateState[0].Hearts == 0 && RunTime == 203.f, TEXT("hearts clamp and a one-turn choice costs 15 seconds"));

    BeginRun(true);
    RunTime = 810.f;
    EnterDate(2);
    Require(Screen == ERomanceScreen::Results && TotalHearts() == 0, TEXT("skipped dates and a missed finale still wrap"));
    Require(DateState[0].Result == EDateResult::Missed && DateState[1].Result == EDateResult::Missed
        && DateState[2].Result == EDateResult::Missed, TEXT("account for every missed date"));
    UE_LOG(LogTemp, Display, TEXT("ANY_ROMANCE_RULES passed: all dates, 12 replies, timing, lateness, skipping, ending and replay"));
    FPlatformMisc::RequestExit(false);
}

void ARomanceMode::VerifyCourse(float DeltaSeconds)
{
    ARomanceRunner* Player = Runner();
    const int32 Stops = Checkpoints.Num() + DateCount;
    if (Screen == ERomanceScreen::Results || (DateCount == 0 && TestStep >= Stops))
    {
        checkf(TotalHearts() == DateCount * Romance::MaxHearts, TEXT("Every date in the level should be met with five hearts"));
        UE_LOG(LogTemp, Display, TEXT("ANY_ROMANCE_COURSE passed: %d checkpoints, %d dates, %d hearts, clock %s"),
            Checkpoints.Num(), DateCount, TotalHearts(), *Romance::Clock(RunTime));
        TestStep = INDEX_NONE;
        FPlatformMisc::RequestExit(false);
        return;
    }
    if (Screen == ERomanceScreen::Date)
    {
        Choose(2);
        if (Screen != ERomanceScreen::Date)
        {
            ++TestStep;
            TestTime = 0;
        }
        return;
    }

    const bool bDate = TestStep >= Checkpoints.Num();
    const int32 DateIndex = TestStep - Checkpoints.Num();
    const ARomancePoint* Stop = bDate ? DatePoints[DateIndex].Get() : Checkpoints[TestStep].Get();
    const FString Name = bDate ? FString(Romance::Names[DateIndex]) : Stop->Label;
    if (TestTime == 0)
    {
        Falls = 0;
        Player->RespawnAt(Stop->Feet(), Stop->Yaw());
    }
    TestTime += DeltaSeconds;

    const float Feet = Player->GetActorLocation().Z - Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const bool bStanding = TestTime > .4 && Player->Parkour()->IsMovingOnGround() && FMath::Abs(Feet - Stop->Feet().Z) < 60.f;
    if (bStanding && bDate && NearbyDate == DateIndex)
    {
        UE_LOG(LogTemp, Display, TEXT("ANY_ROMANCE_COURSE date %s reached at %s"), *Name, *Stop->Feet().ToCompactString());
        Interact();
    }
    else if (bStanding && !bDate && CheckpointIndex == TestStep)
    {
        ++TestStep;
        TestTime = 0;
    }
    else if (TestTime > 3 || Falls > 0)
    {
        UE_LOG(LogTemp, Fatal, TEXT("ANY_ROMANCE_COURSE failed at %s '%s': point=%s feet-z=%.1f ground=%d checkpoint=%d nearby=%d falls=%d"),
            bDate ? TEXT("date") : TEXT("checkpoint"), *Name, *Stop->Feet().ToCompactString(), Feet,
            Player->Parkour()->IsMovingOnGround(), CheckpointIndex, NearbyDate, Falls);
    }
}

namespace
{
    enum : uint8 { TestJump, TestBlast, TestRide, TestSlide, TestSling, TestSwitch };
    const TCHAR* const TestNames[] = {TEXT("jump"), TEXT("rocket jump"), TEXT("wall ride and kick"), TEXT("slide"),
        TEXT("slingshot"), TEXT("blast switch")};
}

void ARomanceMode::VerifyMovement(float DeltaSeconds)
{
    ARomanceRunner* Player = Runner();
    URomanceMovement* Move = Player->Parkour();
    if (TestStages.IsEmpty())
    {
        checkf(FMath::IsNearlyEqual(Player->GetCapsuleComponent()->GetScaledCapsuleRadius(), ARomanceRunner::CapsuleRadius)
            && FMath::IsNearlyEqual(Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), ARomanceRunner::StandingHalfHeight),
            TEXT("The runner should have a 176 cm tall capsule with a 34 cm radius"));
        TestStages.Add({TestJump, nullptr});
        TestStages.Add({TestBlast, nullptr});
        const TPair<const TCHAR*, uint8> StageTags[] = {{TEXT("VerifyRide"), TestRide}, {TEXT("VerifySlide"), TestSlide}, {TEXT("VerifySling"), TestSling},
            {TEXT("VerifySwitch"), TestSwitch}};
        for (const auto& Tag : StageTags)
        {
            TArray<AActor*> Found;
            UGameplayStatics::GetAllActorsWithTag(this, Tag.Key, Found);
            for (AActor* Actor : Found)
            {
                TestStages.Add({Tag.Value, Actor});
            }
        }
    }

    const FTestStage& Stage = TestStages[TestStep];
    const AActor* At = Stage.At.Get();
    const FVector Forward = At ? At->GetActorForwardVector().GetSafeNormal2D() : FRotator(0, StartYaw, 0).Vector();
    if (TestTime == 0)
    {
        bTestActed = bTestSaw = false;
        Falls = 0;
        Player->RespawnAt(At ? At->GetActorLocation() : StartFeet, At ? At->GetActorRotation().Yaw : StartYaw);
        if (Stage.Kind == TestRide || Stage.Kind == TestSlide)
        {
            Move->Velocity = Forward * 1100.f;
        }
        if ((Stage.Kind == TestSling || Stage.Kind == TestSwitch) && At)
        {
            Player->GetController()->SetControlRotation(At->GetActorRotation());
        }
        if (Stage.Kind == TestBlast)
        {
            Player->GetController()->SetControlRotation(FRotator(-89, StartYaw, 0));
        }
        TestOrigin = Player->GetActorLocation();
    }
    TestTime += DeltaSeconds;

    const FVector Here = Player->GetActorLocation();
    bool Passed = false;
    switch (Stage.Kind)
    {
    case TestJump:
    {
        if (!bTestActed && TestTime > .3 && Move->IsMovingOnGround())
        {
            Move->RequestJump();
            Move->ReleaseJump();
            bTestActed = true;
        }
        const float Apex = FMath::Square(Move->JumpZVelocity) / (2.f * FMath::Abs(Move->GetGravityZ()));
        bTestSaw |= Here.Z > StartFeet.Z + ARomanceRunner::StandingHalfHeight + Apex * .85f;
        Passed = TestTime > .8 && bTestActed && bTestSaw && Move->IsMovingOnGround();
        break;
    }
    case TestBlast:
        if (!bTestActed && TestTime > .3 && Move->IsMovingOnGround() && Player->ChargeReady() >= 1.f)
        {
            Player->ThrowCharge();
            bTestActed = true;
        }
        Passed = bTestActed && Move->IsFalling() && Move->Velocity.Z > 1000;
        break;
    case TestRide:
        Player->AddMovementInput(Forward);
        bTestSaw |= Move->IsWallRiding();
        if (bTestSaw && !bTestActed && TestTime > .13)
        {
            Move->RequestJump();
            Move->ReleaseJump();
            bTestActed = true;
        }
        Passed = bTestSaw && bTestActed && Move->IsFalling() && Move->Velocity.Z > 500;
        break;
    case TestSlide:
        Player->AddMovementInput(Forward);
        Move->SetSlideHeld(true);
        bTestSaw |= Move->IsSliding() && Player->bIsCrouched;
        Passed = bTestSaw && FVector::DotProduct(Here - TestOrigin, Forward) > 500;
        break;
    case TestSling:
        if (!bTestActed && TestTime > .4)
        {
            Player->StartSling();
            bTestActed = true;
            TestMark = FVector::Dist(Here, Move->GetSlingAnchor());
        }
        bTestSaw |= Move->IsSlinging();
        Passed = bTestActed && bTestSaw && FVector::Dist(Here, Move->GetSlingAnchor()) < TestMark - 150;
        break;
    case TestSwitch:
    {
        // Throw a charge where the marker looks; a switch there must turn and start its door or lift.
        // Wait out the cooldown left by the previous stage's charge.
        if (!bTestActed && TestTime > .3 && Move->IsMovingOnGround() && Player->ChargeReady() >= 1.f)
        {
            Player->ThrowCharge();
            bTestActed = true;
        }
        const ARomanceMover* Mover = OpenedActors.Num() ? Cast<ARomanceMover>(OpenedActors[0].Get()) : nullptr;
        Passed = bTestActed && TrippedSwitches.Num() == 1 && Mover && Mover->IsOpen();
        break;
    }
    }

    if (Passed)
    {
        UE_LOG(LogTemp, Display, TEXT("ANY_ROMANCE_MOVEMENT passed stage %d (%s) at %s, velocity=%s"), TestStep,
            TestNames[Stage.Kind], *Here.ToCompactString(), *Move->Velocity.ToCompactString());
        Move->SetSlideHeld(false);
        Move->StopSling(false);
        ResetSwitches();
        TestTime = 0;
        if (++TestStep == TestStages.Num())
        {
            ResetCheckpoint();
            checkf(FVector::DistSquared2D(CheckpointFeet, Player->GetActorLocation()) < 1, TEXT("Checkpoint reset location"));
            UE_LOG(LogTemp, Display, TEXT("ANY_ROMANCE_MOVEMENT passed: %d stages, checkpoint reset"), TestStages.Num());
            TestStep = INDEX_NONE;
            FPlatformMisc::RequestExit(false);
        }
    }
    else if (TestTime > 5 || Falls > 0)
    {
        UE_LOG(LogTemp, Fatal, TEXT("ANY_ROMANCE_MOVEMENT failed stage %d (%s): pos=%s vel=%s ground=%d saw=%d acted=%d falls=%d"),
            TestStep, TestNames[Stage.Kind], *Here.ToCompactString(), *Move->Velocity.ToCompactString(),
            Move->IsMovingOnGround(), bTestSaw, bTestActed, Falls);
    }
}
