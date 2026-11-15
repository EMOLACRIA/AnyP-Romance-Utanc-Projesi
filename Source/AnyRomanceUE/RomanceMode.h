#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SaveGame.h"
#include "RomanceMode.generated.h"

class ARomancePoint;
class ARomanceRunner;
class UAudioComponent;
class USoundBase;
class UStaticMesh;
class UTexture2D;

namespace Romance
{
    constexpr int32 Dates = 3;
    constexpr int32 MaxHearts = 5;
    constexpr float TurnSeconds = 15.f;
    extern const TCHAR* const Names[Dates];
    extern const TCHAR* const Places[Dates];
    extern const FColor Accents[Dates];

    struct FChoice
    {
        const TCHAR* Text;
        int32 Hearts;
        int32 Turns;
    };

    struct FDateLine
    {
        const TCHAR* Text;
        FChoice Choices[3];
    };

    extern const FDateLine Lines[Dates][4];
    FString Clock(float Seconds);
}

enum class ERomanceScreen : uint8 { Title, Running, Date, Paused, Results };
enum class EDateResult : uint8 { Pending, Met, Missed };
enum class ERomanceCue : uint8 { Jump, Wall, Launch, Land, Checkpoint, Choice, Count };

UCLASS()
class URomanceSave final : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(SaveGame)
    int32 BestHearts = -1;
    UPROPERTY(SaveGame)
    float BestTime = 0;
    UPROPERTY(SaveGame)
    float Sensitivity = 1.f;
    UPROPERTY(SaveGame)
    bool bReducedMotion = false;
};

// Rules of the run. The course itself (start, checkpoints, date spots, pads, hints) is read from the
// ARomancePoint actors placed in the level; see RomanceCourse.h.
UCLASS()
class ARomanceMode final : public AGameModeBase
{
    GENERATED_BODY()

public:
    ARomanceMode();
    virtual void StartPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

    bool CanRun() const { return Screen == ERomanceScreen::Running; }
    float LookSensitivity() const;
    void Confirm();
    void BeginRun(bool bOriginalSchedule);
    void Pause();
    void Interact();
    void Choose(int32 Choice);
    void ResetCheckpoint();
    void ChangeSensitivity(float Amount);
    void ToggleReducedMotion();
    void ReturnToTitle();
    void Cue(ERomanceCue Sound, float Volume = 1.f) const;
    // A boost charge burst next to an actor tagged BlastSwitch. Every other actor that shares one of the
    // switch's remaining tags is opened (hidden, no collision) until the next run.
    void SwitchHit(AActor* Switch);
    // Slingshot aim assist: the anchor point closest to the aim ray, if one is near enough and in sight.
    bool FindAnchor(const FVector& From, const FVector& Aim, float Range, float MinReach, FVector& Point) const;

private:
    friend class ARomanceHUD;
    void ResetSwitches();
    TArray<TWeakObjectPtr<AActor>> TrippedSwitches;
    TArray<TWeakObjectPtr<AActor>> OpenedActors;
    // The mesh each tripped switch showed before it turned green; same order as TrippedSwitches.
    UPROPERTY()
    TArray<TObjectPtr<UStaticMesh>> SwitchMeshes;
    UPROPERTY()
    TArray<TObjectPtr<ARomancePoint>> Anchors;

    struct FDateState
    {
        EDateResult Result = EDateResult::Pending;
        int32 Hearts = 1;
        float Arrival = 0;
    };

    struct FTestStage
    {
        uint8 Kind = 0;
        TWeakObjectPtr<AActor> At;
    };

    void GatherCourse();
    void ChangeScreen(ERomanceScreen Next);
    void FinishRun();
    void HandleCourse(float DeltaSeconds);
    void EnterDate(int32 Index);
    void Notify(const FString& Text);
    void VerifyRules();
    void VerifyMovement(float DeltaSeconds);
    void VerifyCourse(float DeltaSeconds);
    void WritePreview();
    int32 TotalHearts() const;
    float Deadline(int32 Index) const;
    float Grace(int32 Index) const;
    float TimeToClose(int32 Index) const;
    FString Place(int32 Index) const;
    const ARomancePoint* DatePoint(int32 Index) const;
    FString HintText() const;
    ARomanceRunner* Runner() const;

    // The course, read from the level.
    UPROPERTY()
    TObjectPtr<ARomancePoint> StartPoint;
    UPROPERTY()
    TArray<TObjectPtr<ARomancePoint>> Checkpoints;
    UPROPERTY()
    TArray<TObjectPtr<ARomancePoint>> DatePoints;
    UPROPERTY()
    TArray<TObjectPtr<ARomancePoint>> Pads;
    UPROPERTY()
    TArray<TObjectPtr<ARomancePoint>> HintPoints;
    UPROPERTY()
    TObjectPtr<AActor> TitleCamera;
    // The characters waiting at the date spots: actors tagged DateActor0, DateActor1, DateActor2.
    // One is hidden once its date has given up waiting.
    UPROPERTY()
    TArray<TObjectPtr<AActor>> DateActors;
    void UpdateDateActors();
    float ScreenAge = 0;
    // Seconds left on the dark flash drawn after a checkpoint reset (a fall or R).
    float ResetFlash = 0;

    UPROPERTY()
    TObjectPtr<URomanceSave> Settings;
    UPROPERTY()
    TObjectPtr<UTexture2D> Portraits;
    UPROPERTY()
    TObjectPtr<UAudioComponent> Ambience;
    UPROPERTY()
    TArray<TObjectPtr<USoundBase>> Cues;

    FDateState DateState[Romance::Dates];
    ERomanceScreen Screen = ERomanceScreen::Title;
    FVector StartFeet = FVector::ZeroVector;
    float StartYaw = 0;
    float FallZ = -1.e9f;
    // Dates the level provides, counted from Ari. The run ends after the last of them.
    int32 DateCount = 0;
    bool bGathered = false;

    FVector CheckpointFeet = FVector::ZeroVector;
    float CheckpointYaw = 0;
    int32 CheckpointIndex = -1;
    int32 NextDate = 0;
    int32 ActiveDate = INDEX_NONE;
    int32 Line = 0;
    int32 Falls = 0;
    int32 RouteHint = INDEX_NONE;
    int32 NearbyDate = INDEX_NONE;
    int32 NearPad = INDEX_NONE;
    int32 PreviewDate = INDEX_NONE;
    float RunTime = 0;
    float NoticeTime = 0;
    float PadCooldown = 0;
    bool bOriginalSchedule = false;
    bool bTesting = false;
    bool bTestingCourse = false;
    bool bNewBest = false;
    FString Notice;

    // Command-line verification (RomanceVerification.cpp).
    TArray<FTestStage> TestStages;
    FVector TestOrigin = FVector::ZeroVector;
    float TestTime = 0;
    float TestMark = 0;
    int32 TestStep = INDEX_NONE;
    bool bTestActed = false;
    bool bTestSaw = false;
};

UCLASS()
class ARomanceController final : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaSeconds) override;

private:
    ARomanceMode* Game() const;
    ARomanceRunner* Runner() const;
    void LookX(float Value);
    void LookY(float Value);
    void JumpPressed();
    void JumpReleased();
    void SlidePressed();
    void SlideReleased();
    void Interact();
    void Confirm();
    void Pause();
    void Reset();
    void First();
    void Second();
    void Third();
    void Left();
    void Right();
    void ToggleMotion();
    void MouseClick();
    void MouseReleased();
    void Charge();
};

UCLASS()
class ARomanceHUD final : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
    void Click();

private:
    enum class EAction : uint8
    {
        Start, Classic, Resume, Title, Less, More, Motion, ChoiceOne, ChoiceTwo, ChoiceThree
    };

    struct FButton
    {
        FBox2D Bounds;
        EAction Action;
    };

    TArray<FButton> Buttons;
};
