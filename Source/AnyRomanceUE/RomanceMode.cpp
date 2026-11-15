#include "RomanceMode.h"

#include "Algo/Sort.h"
#include "Camera/CameraActor.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RomanceCourse.h"
#include "RomanceRunner.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace Romance
{
    const TCHAR* const Names[] = {TEXT("Ari"), TEXT("Bea"), TEXT("Jules")};
    // Used when a date point in the level has no label of its own.
    const TCHAR* const Places[] = {TEXT("City Cafe"), TEXT("Harbor Cafe"), TEXT("Mountain Lodge")};
    const FColor Accents[] = {FColor(64, 178, 153), FColor(233, 124, 119), FColor(170, 137, 218)};

    #include "OriginalDialogue.inl"

    FString Clock(float Seconds)
    {
        const int32 Whole = FMath::Max(0, FMath::CeilToInt(Seconds));
        return FString::Printf(TEXT("%02d:%02d"), Whole / 60, Whole % 60);
    }
}

namespace
{
    constexpr const TCHAR* RomanceSaveSlot = TEXT("AnyRomance");
    // Seconds on the run clock at which each date starts, and how long each one waits afterwards.
    // A date point in the level can replace its own pair.
    const float OriginalSchedule[] = {180.f, 480.f, 780.f};
    const float OriginalGrace[] = {35.f, 25.f, 15.f};
    // With no FallLine actor in the level, the runner is returned to the checkpoint this far below the lowest point.
    constexpr float FallMargin = 250.f;
    constexpr float PointReachHeight = 220.f;
    constexpr float PadFlightSpeed = 1800.f;
}

ARomanceMode::ARomanceMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = ARomanceRunner::StaticClass();
    PlayerControllerClass = ARomanceController::StaticClass();
    HUDClass = ARomanceHUD::StaticClass();
}

void ARomanceMode::GatherCourse()
{
    if (bGathered)
    {
        return;
    }
    bGathered = true;
    DatePoints.Init(nullptr, Romance::Dates);
    float Lowest = TNumericLimits<float>::Max();
    for (TActorIterator<ARomancePoint> It(GetWorld()); It; ++It)
    {
        ARomancePoint* Point = *It;
        Lowest = FMath::Min(Lowest, static_cast<float>(Point->Feet().Z));
        switch (Point->Kind)
        {
        case ERomancePointKind::Start:
            if (!StartPoint) { StartPoint = Point; }
            break;
        case ERomancePointKind::Checkpoint:
            Checkpoints.Add(Point);
            break;
        case ERomancePointKind::Date:
            if (DatePoints.IsValidIndex(Point->Order) && !DatePoints[Point->Order]) { DatePoints[Point->Order] = Point; }
            break;
        case ERomancePointKind::Pad:
            Pads.Add(Point);
            break;
        case ERomancePointKind::Anchor:
            Anchors.Add(Point);
            break;
        case ERomancePointKind::Marker:
            break;
        }
        if (!Point->Hint.IsEmpty())
        {
            HintPoints.Add(Point);
        }
    }
    Algo::Sort(Checkpoints, [](const TObjectPtr<ARomancePoint>& A, const TObjectPtr<ARomancePoint>& B) { return A->Order < B->Order; });
    DateCount = 0;
    while (DateCount < Romance::Dates && DatePoints[DateCount])
    {
        ++DateCount;
    }

    if (Lowest < TNumericLimits<float>::Max())
    {
        FallZ = Lowest - FallMargin;
    }
    TArray<AActor*> Tagged;
    UGameplayStatics::GetAllActorsWithTag(this, TEXT("FallLine"), Tagged);
    if (Tagged.Num())
    {
        FallZ = Tagged[0]->GetActorLocation().Z;
    }
    UGameplayStatics::GetAllActorsWithTag(this, TEXT("TitleCamera"), Tagged);
    TitleCamera = Tagged.Num() ? Tagged[0] : nullptr;
    for (int32 Index = 0; Index < Romance::Dates; ++Index)
    {
        UGameplayStatics::GetAllActorsWithTag(this, *FString::Printf(TEXT("DateActor%d"), Index), Tagged);
        DateActors.Add(Tagged.Num() ? Tagged[0] : nullptr);
    }

    if (StartPoint)
    {
        StartFeet = StartPoint->Feet();
        StartYaw = StartPoint->Yaw();
    }
}

AActor* ARomanceMode::ChoosePlayerStart_Implementation(AController* Player)
{
    GatherCourse();
    if (!StartPoint)
    {
        return Super::ChoosePlayerStart_Implementation(Player);
    }
    return GetWorld()->SpawnActor<APlayerStart>(StartFeet + FVector(0, 0, ARomanceRunner::StandingHalfHeight + 3.f),
        FRotator(0, StartYaw, 0));
}

void ARomanceMode::StartPlay()
{
    Super::StartPlay();
    GatherCourse();
    const bool bVerifyRules = FParse::Param(FCommandLine::Get(), TEXT("RomanceVerifyRules"));
    const bool bVerifyMovement = FParse::Param(FCommandLine::Get(), TEXT("RomanceVerifyMovement"));
    bTestingCourse = FParse::Param(FCommandLine::Get(), TEXT("RomanceVerifyCourse"));
    bTesting = bVerifyRules || bVerifyMovement || bTestingCourse;
    Settings = bTesting ? nullptr : Cast<URomanceSave>(UGameplayStatics::LoadGameFromSlot(RomanceSaveSlot, 0));
    if (!Settings)
    {
        Settings = NewObject<URomanceSave>(this);
    }

    Portraits = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Island/T_DatePortraits.T_DatePortraits"));
    const TCHAR* Sounds[] = {TEXT("Jump"), TEXT("Wall"), TEXT("Launch"), TEXT("Land"), TEXT("Checkpoint"), TEXT("Choice")};
    for (const TCHAR* Name : Sounds)
    {
        const FString Path = FString::Printf(TEXT("/Game/Island/A_%s.A_%s"), Name, Name);
        Cues.Add(LoadObject<USoundBase>(nullptr, *Path));
    }

    if (!bTesting && !FApp::IsUnattended())
    {
        USoundBase* Score = LoadObject<USoundBase>(nullptr, TEXT("/Game/Island/A_Island.A_Island"));
        if (Score)
        {
            Ambience = UGameplayStatics::SpawnSound2D(this, Score, .68f);
        }
    }
    if (ARomanceRunner* Player = Runner())
    {
        if (!StartPoint)
        {
            // A level without a start point: begin wherever the engine placed the runner.
            StartFeet = Player->GetActorLocation() - FVector(0, 0, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            StartYaw = Player->GetActorRotation().Yaw;
        }
        Player->SetReducedMotion(Settings->bReducedMotion);
    }
    ChangeScreen(ERomanceScreen::Title);

    if (bVerifyRules)
    {
        VerifyRules();
    }
    else if (bVerifyMovement || bTestingCourse)
    {
        BeginRun(false);
        TestStep = 0;
    }
    else if (FParse::Value(FCommandLine::Get(), TEXT("RomancePreview="), PreviewDate))
    {
        WritePreview();
    }
}

ARomanceRunner* ARomanceMode::Runner() const
{
    const APlayerController* PC = GetWorld()->GetFirstPlayerController();
    return PC ? Cast<ARomanceRunner>(PC->GetPawn()) : nullptr;
}

const ARomancePoint* ARomanceMode::DatePoint(int32 Index) const
{
    return DatePoints.IsValidIndex(Index) ? DatePoints[Index].Get() : nullptr;
}

FString ARomanceMode::Place(int32 Index) const
{
    const ARomancePoint* Point = DatePoint(Index);
    return Point && !Point->Label.IsEmpty() ? Point->Label : FString(Romance::Places[Index]);
}

FString ARomanceMode::HintText() const
{
    return HintPoints.IsValidIndex(RouteHint) && HintPoints[RouteHint] ? HintPoints[RouteHint]->Hint : FString();
}

float ARomanceMode::LookSensitivity() const
{
    return Settings ? Settings->Sensitivity : 1.f;
}

void ARomanceMode::Cue(ERomanceCue Sound, float Volume) const
{
    const int32 Index = static_cast<int32>(Sound);
    if (!bTesting && Cues.IsValidIndex(Index) && Cues[Index])
    {
        UGameplayStatics::PlaySound2D(this, Cues[Index], Volume);
    }
}

void ARomanceMode::SwitchHit(AActor* Switch)
{
    if (!Switch || TrippedSwitches.Contains(Switch))
    {
        return;
    }
    TrippedSwitches.Add(Switch);

    // The pack ships each switch twice: SM_Blast_Switch (red) and SM_Blast_Switch_On (green). Show the green one.
    UStaticMesh* Before = nullptr;
    if (const AStaticMeshActor* Body = Cast<AStaticMeshActor>(Switch))
    {
        UStaticMeshComponent* Shape = Body->GetStaticMeshComponent();
        Before = Shape->GetStaticMesh();
        const FString Package = Before ? Before->GetPackage()->GetName() + TEXT("_On") : FString();
        if (Before && FPackageName::DoesPackageExist(Package))
        {
            if (UStaticMesh* Lit = LoadObject<UStaticMesh>(nullptr, *(Package + TEXT(".") + FPackageName::GetShortName(Package))))
            {
                Shape->SetMobility(EComponentMobility::Movable);
                Shape->SetStaticMesh(Lit);
            }
        }
    }
    SwitchMeshes.Add(Before);

    int32 Opened = 0;
    for (const FName& Tag : Switch->Tags)
    {
        if (Tag == TEXT("BlastSwitch"))
        {
            continue;
        }
        TArray<AActor*> Targets;
        UGameplayStatics::GetAllActorsWithTag(this, Tag, Targets);
        for (AActor* Target : Targets)
        {
            if (Target != Switch && !Target->ActorHasTag(TEXT("BlastSwitch")) && !OpenedActors.Contains(Target))
            {
                if (ARomanceMover* Mover = Cast<ARomanceMover>(Target))
                {
                    Mover->Open();
                }
                else
                {
                    Target->SetActorHiddenInGame(true);
                    Target->SetActorEnableCollision(false);
                }
                OpenedActors.Add(Target);
                ++Opened;
            }
        }
    }
    Cue(ERomanceCue::Checkpoint);
    Notify(Opened ? TEXT("Switch triggered. The way is open.") : TEXT("Switch triggered."));
}

void ARomanceMode::ResetSwitches()
{
    for (const TWeakObjectPtr<AActor>& Target : OpenedActors)
    {
        if (ARomanceMover* Mover = Cast<ARomanceMover>(Target.Get()))
        {
            Mover->ResetMover();
        }
        else if (Target.IsValid())
        {
            Target->SetActorHiddenInGame(false);
            Target->SetActorEnableCollision(true);
        }
    }
    for (int32 Index = 0; Index < TrippedSwitches.Num(); ++Index)
    {
        const AStaticMeshActor* Body = Cast<AStaticMeshActor>(TrippedSwitches[Index].Get());
        if (Body && SwitchMeshes.IsValidIndex(Index) && SwitchMeshes[Index])
        {
            Body->GetStaticMeshComponent()->SetStaticMesh(SwitchMeshes[Index]);
        }
    }
    OpenedActors.Reset();
    TrippedSwitches.Reset();
    SwitchMeshes.Reset();
}

bool ARomanceMode::FindAnchor(const FVector& From, const FVector& Aim, float Range, float MinReach, FVector& Point) const
{
    // Within 7 degrees of the crosshair, or within a metre of the aim line when the anchor is close.
    constexpr float MaxAngleCos = .9925f;
    constexpr float MaxMiss = 100.f;
    float Best = -1.f;
    for (const ARomancePoint* Anchor : Anchors)
    {
        if (!Anchor)
        {
            continue;
        }
        const FVector To = Anchor->GetActorLocation() - From;
        const float Distance = To.Size();
        const float Along = FVector::DotProduct(To, Aim);
        if (Distance < MinReach || Distance > Range || Along <= 0)
        {
            continue;
        }
        const float Cos = Along / Distance;
        const float Miss = FMath::Sqrt(FMath::Max(0.f, Distance * Distance - Along * Along));
        if ((Cos < MaxAngleCos && Miss > MaxMiss) || Cos <= Best)
        {
            continue;
        }
        // Something solid well short of the anchor hides it.
        FHitResult Hit;
        FCollisionQueryParams Trace(SCENE_QUERY_STAT(RomanceAnchor), false, Runner());
        if (GetWorld()->LineTraceSingleByChannel(Hit, From, Anchor->GetActorLocation(), ECC_Visibility, Trace)
            && Hit.Distance < Distance - 80.f)
        {
            continue;
        }
        Best = Cos;
        Point = Anchor->GetActorLocation();
    }
    return Best > 0;
}

void ARomanceMode::UpdateDateActors()
{
    for (int32 Index = 0; Index < DateActors.Num(); ++Index)
    {
        if (AActor* Waiting = DateActors[Index])
        {
            const bool bLeft = DateState[Index].Result == EDateResult::Missed
                || (DateState[Index].Result == EDateResult::Pending && Index < DateCount && TimeToClose(Index) < 0);
            Waiting->SetActorHiddenInGame(bLeft);
        }
    }
}

void ARomanceMode::ChangeScreen(ERomanceScreen Next)
{
    Screen = Next;
    ScreenAge = 0;
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    ARomanceRunner* Player = Runner();
    if (!PC || !Player)
    {
        return;
    }

    Player->SetRunning(CanRun());
    PC->SetViewTarget(Next == ERomanceScreen::Title && TitleCamera ? TitleCamera.Get() : Player);
    PC->bShowMouseCursor = !CanRun();
    if (CanRun())
    {
        PC->SetInputMode(FInputModeGameOnly());
    }
    else
    {
        FInputModeGameAndUI Input;
        Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Input.SetHideCursorDuringCapture(false);
        PC->SetInputMode(Input);
    }
    if (Ambience)
    {
        Ambience->SetVolumeMultiplier(Next == ERomanceScreen::Date ? .44f : .68f);
    }
}

void ARomanceMode::BeginRun(bool bUseOriginalSchedule)
{
    for (FDateState& Date : DateState)
    {
        Date = {};
    }
    bOriginalSchedule = bUseOriginalSchedule;
    NextDate = 0;
    ActiveDate = INDEX_NONE;
    NearbyDate = INDEX_NONE;
    Line = 0;
    Falls = 0;
    RunTime = 0;
    PadCooldown = 0;
    NearPad = INDEX_NONE;
    RouteHint = INDEX_NONE;
    NoticeTime = 0;
    CheckpointIndex = -1;
    CheckpointYaw = StartYaw;
    CheckpointFeet = StartFeet;
    bNewBest = false;
    ResetSwitches();
    ChangeScreen(ERomanceScreen::Running);
    if (ARomanceRunner* Player = Runner())
    {
        Player->RespawnAt(CheckpointFeet, CheckpointYaw);
    }
}

void ARomanceMode::Confirm()
{
    switch (Screen)
    {
    case ERomanceScreen::Title: BeginRun(false); break;
    case ERomanceScreen::Running: Interact(); break;
    case ERomanceScreen::Paused: Pause(); break;
    case ERomanceScreen::Results: BeginRun(bOriginalSchedule); break;
    case ERomanceScreen::Date: break;
    }
}

void ARomanceMode::Pause()
{
    if (Screen == ERomanceScreen::Running)
    {
        ChangeScreen(ERomanceScreen::Paused);
    }
    else if (Screen == ERomanceScreen::Paused)
    {
        ChangeScreen(ERomanceScreen::Running);
    }
}

void ARomanceMode::ReturnToTitle()
{
    ChangeScreen(ERomanceScreen::Title);
}

void ARomanceMode::ChangeSensitivity(float Amount)
{
    if (Screen == ERomanceScreen::Paused)
    {
        Settings->Sensitivity = FMath::Clamp(Settings->Sensitivity + Amount, .3f, 2.3f);
        UGameplayStatics::SaveGameToSlot(Settings, RomanceSaveSlot, 0);
    }
}

void ARomanceMode::ToggleReducedMotion()
{
    if (Screen == ERomanceScreen::Paused)
    {
        Settings->bReducedMotion = !Settings->bReducedMotion;
        if (ARomanceRunner* Player = Runner())
        {
            Player->SetReducedMotion(Settings->bReducedMotion);
        }
        UGameplayStatics::SaveGameToSlot(Settings, RomanceSaveSlot, 0);
    }
}

float ARomanceMode::Deadline(int32 Index) const
{
    const ARomancePoint* Point = DatePoint(Index);
    return !bOriginalSchedule && Point && Point->Deadline > 0 ? Point->Deadline : OriginalSchedule[Index];
}

float ARomanceMode::Grace(int32 Index) const
{
    const ARomancePoint* Point = DatePoint(Index);
    return !bOriginalSchedule && Point && Point->Grace > 0 ? Point->Grace : OriginalGrace[Index];
}

float ARomanceMode::TimeToClose(int32 Index) const
{
    return Deadline(Index) + Grace(Index) - RunTime;
}

int32 ARomanceMode::TotalHearts() const
{
    int32 Total = 0;
    for (const FDateState& Date : DateState)
    {
        if (Date.Result == EDateResult::Met)
        {
            Total += Date.Hearts;
        }
    }
    return Total;
}

void ARomanceMode::Notify(const FString& Text)
{
    Notice = Text;
    NoticeTime = 3.1f;
}

void ARomanceMode::Interact()
{
    if (CanRun() && NearbyDate != INDEX_NONE)
    {
        EnterDate(NearbyDate);
    }
}

void ARomanceMode::EnterDate(int32 Index)
{
    if (!CanRun() || Index < NextDate || Index >= DateCount)
    {
        return;
    }
    for (int32 Skipped = NextDate; Skipped < Index; ++Skipped)
    {
        DateState[Skipped] = {EDateResult::Missed, 0, RunTime};
    }

    NextDate = Index + 1;
    NearbyDate = INDEX_NONE;
    FDateState& Date = DateState[Index];
    Date.Arrival = RunTime;
    const float Lateness = RunTime - Deadline(Index);
    if (Lateness > Grace(Index))
    {
        Date.Result = EDateResult::Missed;
        Date.Hearts = 0;
        Notify(FString::Printf(TEXT("%s had to leave."), Romance::Names[Index]));
        if (NextDate >= DateCount)
        {
            FinishRun();
        }
        return;
    }

    Date.Result = EDateResult::Met;
    if (Lateness > 0)
    {
        Date.Hearts = FMath::Max(0, Date.Hearts - (Lateness < Grace(Index) * .5f ? 1 : 2));
    }
    ActiveDate = Index;
    Line = 0;
    ChangeScreen(ERomanceScreen::Date);
    ARomanceRunner* Player = Runner();
    if (const ARomancePoint* Point = DatePoint(Index); Point && Player && Player->GetController())
    {
        // Look at the date: the character standing there if the level has one, else the spot itself.
        const AActor* Waiting = DateActors.IsValidIndex(Index) ? DateActors[Index].Get() : nullptr;
        const FVector Face = Waiting ? Waiting->GetActorLocation() + FVector(0, 0, 140) : Point->Feet() + FVector(0, 0, 130);
        Player->GetController()->SetControlRotation((Face - Player->GetPawnViewLocation()).Rotation());
    }
    Cue(ERomanceCue::Choice);
}

void ARomanceMode::Choose(int32 ChoiceIndex)
{
    if (Screen != ERomanceScreen::Date || !FMath::IsWithin(ChoiceIndex, 0, 3))
    {
        return;
    }

    const Romance::FChoice& Choice = Romance::Lines[ActiveDate][Line].Choices[ChoiceIndex];
    FDateState& Date = DateState[ActiveDate];
    Date.Hearts = FMath::Clamp(Date.Hearts + Choice.Hearts, 0, Romance::MaxHearts);
    RunTime += Romance::TurnSeconds * Choice.Turns;
    Cue(ERomanceCue::Choice);
    if (++Line < 4)
    {
        return;
    }

    const int32 Finished = ActiveDate;
    ActiveDate = INDEX_NONE;
    if (NextDate >= DateCount)
    {
        FinishRun();
    }
    else
    {
        ChangeScreen(ERomanceScreen::Running);
        Notify(FString::Printf(TEXT("%s  /  %d of %d hearts"), Romance::Names[Finished], Date.Hearts, Romance::MaxHearts));
    }
}

void ARomanceMode::FinishRun()
{
    const int32 Hearts = TotalHearts();
    bNewBest = Hearts > Settings->BestHearts || (Hearts == Settings->BestHearts && RunTime < Settings->BestTime);
    if (bNewBest && !bTesting)
    {
        Settings->BestHearts = Hearts;
        Settings->BestTime = RunTime;
        UGameplayStatics::SaveGameToSlot(Settings, RomanceSaveSlot, 0);
    }
    ChangeScreen(ERomanceScreen::Results);
}

void ARomanceMode::ResetCheckpoint()
{
    if (CanRun())
    {
        if (ARomanceRunner* Player = Runner())
        {
            ++Falls;
            Player->RespawnAt(CheckpointFeet, CheckpointYaw);
            PadCooldown = 0;
            Notify(TEXT("Back at your checkpoint. The date clock keeps running."));
        }
    }
}

void ARomanceMode::HandleCourse(float DeltaSeconds)
{
    ARomanceRunner* Player = Runner();
    if (!Player)
    {
        return;
    }

    const FVector Position = Player->GetActorLocation();
    const FVector Feet = Position - FVector(0, 0, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    if (Feet.Z < FallZ)
    {
        ResetCheckpoint();
        return;
    }

    const auto Near = [&Feet](const FVector& Marker, float Distance, float Vertical)
    {
        return FVector::DistSquared2D(Feet, Marker) < FMath::Square(Distance)
            && FMath::Abs(Feet.Z - Marker.Z) < Vertical;
    };

    // Any later checkpoint counts, so a shortcut that skips one still registers the next.
    for (int32 Index = CheckpointIndex + 1; Index < Checkpoints.Num(); ++Index)
    {
        const ARomancePoint* Marker = Checkpoints[Index];
        if (Marker && Near(Marker->Feet(), Marker->Radius, PointReachHeight))
        {
            CheckpointIndex = Index;
            CheckpointFeet = Marker->Feet();
            CheckpointYaw = Marker->Yaw();
            Notify(Marker->Label.IsEmpty() ? FString(TEXT("Checkpoint")) : FString::Printf(TEXT("Checkpoint  /  %s"), *Marker->Label));
            Cue(ERomanceCue::Checkpoint);
        }
    }

    PadCooldown = FMath::Max(0.f, PadCooldown - DeltaSeconds);
    NearPad = INDEX_NONE;
    for (int32 Index = 0; Index < Pads.Num(); ++Index)
    {
        const ARomancePoint* Pad = Pads[Index];
        if (!Pad || !Near(Pad->Feet(), Pad->Radius + 70, 480))
        {
            continue;
        }
        NearPad = Index;
        if (PadCooldown <= 0 && Player->GetVelocity().Z <= 40 && Near(Pad->Feet(), Pad->Radius, PointReachHeight))
        {
            PadCooldown = 1.3f;
            Player->Parkour()->ClearParkour();
            const FVector Delta = Pad->GetActorTransform().TransformPosition(Pad->PadTarget) - Feet;
            const float Gravity = FMath::Abs(Player->Parkour()->GetGravityZ());
            const float FlightTime = FMath::Max(Delta.Size2D() / PadFlightSpeed,
                FMath::Sqrt(2.f * FMath::Abs(Delta.Z) / Gravity) + .85f);
            const FVector Launch(Delta.X / FlightTime, Delta.Y / FlightTime,
                Delta.Z / FlightTime + .5f * Gravity * FlightTime);
            Player->LaunchCharacter(Launch, true, true);
            Cue(ERomanceCue::Launch);
        }
    }

    NearbyDate = INDEX_NONE;
    for (int32 Index = NextDate; Index < DateCount; ++Index)
    {
        if (Near(DatePoints[Index]->Feet(), DatePoints[Index]->Radius, PointReachHeight))
        {
            NearbyDate = Index;
            break;
        }
    }

    RouteHint = INDEX_NONE;
    float NearestHint = TNumericLimits<float>::Max();
    for (int32 Index = 0; Index < HintPoints.Num(); ++Index)
    {
        const float Distance = HintPoints[Index] ? FVector::DistSquared(Feet, HintPoints[Index]->Feet()) : NearestHint;
        if (Distance < NearestHint)
        {
            NearestHint = Distance;
            RouteHint = Index;
        }
    }
}

void ARomanceMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ScreenAge += DeltaSeconds;
    UpdateDateActors();
    if (CanRun())
    {
        RunTime += DeltaSeconds;
        NoticeTime = FMath::Max(0.f, NoticeTime - DeltaSeconds);
        HandleCourse(DeltaSeconds);
    }
    if (TestStep != INDEX_NONE)
    {
        if (bTestingCourse)
        {
            VerifyCourse(DeltaSeconds);
        }
        else
        {
            VerifyMovement(DeltaSeconds);
        }
    }
}

// -RomancePreview=N starts a run and saves a screenshot, then quits. N = 0..2 opens that date, anything else
// stays on the course. -RomanceView=X,Y,Z,Pitch,Yaw looks from a free camera instead of the runner's eyes.
// -RomanceShot=Name names the file (Saved/Shots/Name.png); -RomanceShotDelay=Seconds waits longer first.
void ARomanceMode::WritePreview()
{
    BeginRun(false);
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    ARomanceRunner* Player = Runner();
    if (const ARomancePoint* Point = DatePoint(PreviewDate); Point && Player)
    {
        const FVector Forward = FRotator(0, Point->Yaw(), 0).Vector();
        Player->RespawnAt(Point->Feet() + Forward * 240, Point->Yaw() + 180);
        EnterDate(PreviewDate);
    }

    FString View;
    if (FParse::Value(FCommandLine::Get(), TEXT("RomanceView="), View, false) && PC)
    {
        TArray<FString> Parts;
        View.ParseIntoArray(Parts, TEXT(","));
        if (Parts.Num() == 5)
        {
            const FVector At(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
            const FRotator Look(FCString::Atof(*Parts[3]), FCString::Atof(*Parts[4]), 0);
            if (ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(At, Look))
            {
                PC->SetViewTarget(Camera);
            }
            if (AHUD* HUD = PC->GetHUD())
            {
                HUD->bShowHUD = false;
            }
            if (Player)
            {
                Player->SetRunning(false);
            }
        }
    }

    FString Name = TEXT("AnyRomancePreview");
    FParse::Value(FCommandLine::Get(), TEXT("RomanceShot="), Name);
    float Delay = 4.f;
    FParse::Value(FCommandLine::Get(), TEXT("RomanceShotDelay="), Delay);
    const FString File = FPaths::ProjectSavedDir() / TEXT("Shots") / Name + TEXT(".png");
    FTimerHandle Screenshot;
    GetWorldTimerManager().SetTimer(Screenshot, [File]
    {
        FScreenshotRequest::RequestScreenshot(File, true, false);
    }, Delay, false);
    FTimerHandle Exit;
    GetWorldTimerManager().SetTimer(Exit, [] { FPlatformMisc::RequestExit(false); }, Delay + 2.f, false);
}

ARomanceMode* ARomanceController::Game() const { return GetWorld()->GetAuthGameMode<ARomanceMode>(); }
ARomanceRunner* ARomanceController::Runner() const { return Cast<ARomanceRunner>(GetPawn()); }

void ARomanceController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAxisKey(EKeys::MouseX, this, &ARomanceController::LookX);
    InputComponent->BindAxisKey(EKeys::MouseY, this, &ARomanceController::LookY);
    for (const FKey Key : {EKeys::SpaceBar, EKeys::MouseScrollDown})
    {
        InputComponent->BindKey(Key, IE_Pressed, this, &ARomanceController::JumpPressed);
        InputComponent->BindKey(Key, IE_Released, this, &ARomanceController::JumpReleased);
    }
    for (const FKey Key : {EKeys::LeftControl, EKeys::C})
    {
        InputComponent->BindKey(Key, IE_Pressed, this, &ARomanceController::SlidePressed);
        InputComponent->BindKey(Key, IE_Released, this, &ARomanceController::SlideReleased);
    }
    InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ARomanceController::Interact);
    InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ARomanceController::Confirm);
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ARomanceController::Pause);
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ARomanceController::Reset);
    InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ARomanceController::First);
    InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ARomanceController::Second);
    InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ARomanceController::Third);
    InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &ARomanceController::Left);
    InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &ARomanceController::Right);
    InputComponent->BindKey(EKeys::M, IE_Pressed, this, &ARomanceController::ToggleMotion);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ARomanceController::MouseClick);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ARomanceController::MouseReleased);
    for (const FKey Key : {EKeys::RightMouseButton, EKeys::Q})
    {
        InputComponent->BindKey(Key, IE_Pressed, this, &ARomanceController::Charge);
    }
}

void ARomanceController::PlayerTick(float DeltaSeconds)
{
    Super::PlayerTick(DeltaSeconds);
    if (ARomanceRunner* ControlledRunner = Runner(); ControlledRunner && Game() && Game()->CanRun())
    {
        const FRotator Heading(0, GetControlRotation().Yaw, 0);
        const float Forward = static_cast<int32>(IsInputKeyDown(EKeys::W)) - static_cast<int32>(IsInputKeyDown(EKeys::S));
        const float Right = static_cast<int32>(IsInputKeyDown(EKeys::D)) - static_cast<int32>(IsInputKeyDown(EKeys::A));
        ControlledRunner->AddMovementInput(Heading.RotateVector(FVector(Forward, Right, 0)).GetClampedToMaxSize(1));
    }
}

void ARomanceController::LookX(float Value)
{
    if (Game() && Game()->CanRun())
    {
        AddYawInput(Value * Game()->LookSensitivity() * .45f);
    }
}

void ARomanceController::LookY(float Value)
{
    if (Game() && Game()->CanRun())
    {
        AddPitchInput(-Value * Game()->LookSensitivity() * .45f);
    }
}

void ARomanceController::JumpPressed()
{
    if (Runner() && Game() && Game()->CanRun()) { Runner()->Parkour()->RequestJump(); }
}

void ARomanceController::JumpReleased()
{
    if (Runner()) { Runner()->Parkour()->ReleaseJump(); }
}

void ARomanceController::SlidePressed()
{
    if (Runner() && Game() && Game()->CanRun()) { Runner()->Parkour()->SetSlideHeld(true); }
}

void ARomanceController::SlideReleased()
{
    if (Runner()) { Runner()->Parkour()->SetSlideHeld(false); }
}

void ARomanceController::Interact() { if (Game()) { Game()->Interact(); } }
void ARomanceController::Confirm() { if (Game()) { Game()->Confirm(); } }
void ARomanceController::Pause() { if (Game()) { Game()->Pause(); } }
void ARomanceController::Reset() { if (Game()) { Game()->ResetCheckpoint(); } }
void ARomanceController::First() { if (Game()) { Game()->Choose(0); } }
void ARomanceController::Second() { if (Game()) { Game()->Choose(1); } }
void ARomanceController::Third() { if (Game()) { Game()->Choose(2); } }
void ARomanceController::Left() { if (Game()) { Game()->ChangeSensitivity(-.1f); } }
void ARomanceController::Right() { if (Game()) { Game()->ChangeSensitivity(.1f); } }
void ARomanceController::ToggleMotion() { if (Game()) { Game()->ToggleReducedMotion(); } }
void ARomanceController::MouseClick()
{
    // While running the left button fires the slingshot; on menus it clicks.
    if (Runner() && Game() && Game()->CanRun())
    {
        Runner()->StartSling();
    }
    else if (auto* Hud = Cast<ARomanceHUD>(GetHUD()))
    {
        Hud->Click();
    }
}

void ARomanceController::MouseReleased()
{
    if (Runner()) { Runner()->StopSling(); }
}

void ARomanceController::Charge()
{
    if (Runner() && Game() && Game()->CanRun()) { Runner()->ThrowCharge(); }
}
