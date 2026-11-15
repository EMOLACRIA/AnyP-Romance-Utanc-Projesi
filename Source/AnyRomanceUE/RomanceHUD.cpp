#include "RomanceMode.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "RomanceCourse.h"
#include "RomanceRunner.h"

namespace
{
    FLinearColor RGB(FColor Color)
    {
        return FLinearColor::FromSRGBColor(Color);
    }

    const FLinearColor Ink = RGB(FColor(14, 40, 44));
    const FLinearColor LightInk = RGB(FColor(36, 66, 67));
    const FLinearColor Paper = RGB(FColor(255, 239, 207));
    const FLinearColor Card = RGB(FColor(255, 248, 231));
    const FLinearColor Teal = RGB(FColor(21, 113, 102));
    const FLinearColor Warm = RGB(FColor(248, 177, 123));
    const FLinearColor Gray = RGB(FColor(98, 119, 112));
    const FLinearColor White = FLinearColor::White;

    FLinearColor Alpha(FLinearColor Color, float Value)
    {
        Color.A = Value;
        return Color;
    }

    class FRomanceCanvas
    {
    public:
        FRomanceCanvas(ARomanceHUD& InHud, UCanvas& InCanvas)
            : Hud(InHud), Canvas(InCanvas), S(FMath::Min(Canvas.ClipX / 960.f, Canvas.ClipY / 720.f)),
              W(Canvas.ClipX / S), H(Canvas.ClipY / S), Font(GEngine->GetMediumFont()), Display(GEngine->GetLargeFont())
        {
        }

        void Box(FLinearColor Color, float X, float Y, float Width, float Height) const
        {
            Hud.DrawRect(Color, X * S, Y * S, Width * S, Height * S);
        }

        void Label(const FString& Text, FLinearColor Color, float X, float Y, float Size,
            float Alignment = 0.f, bool bDisplay = false) const
        {
            UFont* Face = bDisplay ? Display : Font;
            if (Alignment != 0)
            {
                float Width, Height;
                Hud.GetTextSize(Text, Width, Height, Face, Size);
                X -= Alignment * Width;
            }
            Hud.DrawText(Text, Color, X * S, Y * S, Face, Size * S);
        }

        void Line(FLinearColor Color, FVector2D Start, FVector2D End, float Width) const
        {
            Hud.DrawLine(Start.X * S, Start.Y * S, End.X * S, End.Y * S, Color, Width * S);
        }

        void Wrapped(const FString& Text, FLinearColor Color, float X, float Y, float Width, float Size,
            float LineHeight, bool bDisplay = false) const
        {
            TArray<FString> Words;
            Text.ParseIntoArrayWS(Words);
            FString Current;
            for (const FString& Word : Words)
            {
                const FString Next = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
                float Measured, Height;
                Hud.GetTextSize(Next, Measured, Height, bDisplay ? Display : Font, Size);
                if (Measured > Width && !Current.IsEmpty())
                {
                    Label(Current, Color, X, Y, Size, 0, bDisplay);
                    Y += LineHeight;
                    Current = Word;
                }
                else
                {
                    Current = Next;
                }
            }
            Label(Current, Color, X, Y, Size, 0, bDisplay);
        }

        void Heart(FVector2D At, float Size, FLinearColor Color) const
        {
            // Two rounded lobes and a point, drawn with the canvas white tile.
            for (int32 Row = 0; Row < 7; ++Row)
            {
                constexpr float Widths[] = {.71f, 1.f, 1.f, .88f, .66f, .45f, .19f};
                const float Width = Widths[Row] * Size;
                const float Y = At.Y + Row * Size / 8.f;
                if (Row == 0)
                {
                    Box(Color, At.X - Size * .36f, Y, Size * .28f, Size / 8.f + .45f);
                    Box(Color, At.X + Size * .08f, Y, Size * .28f, Size / 8.f + .45f);
                }
                else
                {
                    Box(Color, At.X - Width * .5f, Y, Width, Size / 8.f + .45f);
                }
            }
        }

        ARomanceHUD& Hud;
        UCanvas& Canvas;
        float S, W, H;
        UFont* Font;
        UFont* Display;
    };
}

void ARomanceHUD::DrawHUD()
{
    Super::DrawHUD();
    Buttons.Reset();
    ARomanceMode* Mode = GetWorld()->GetAuthGameMode<ARomanceMode>();
    if (!Canvas || !Mode || !Mode->Settings)
    {
        return;
    }

    const FRomanceCanvas UI(*this, *Canvas);
    const float W = UI.W;
    const float H = UI.H;
    const float Middle = W * .5f;
    float MouseX = -1000;
    float MouseY = -1000;
    if (PlayerOwner->bShowMouseCursor)
    {
        PlayerOwner->GetMousePosition(MouseX, MouseY);
    }

    const auto Button = [&](float X, float Y, float Width, float Height, EAction Action)
    {
        FBox2D Bounds({X * UI.S, Y * UI.S}, {(X + Width) * UI.S, (Y + Height) * UI.S});
        Buttons.Add({Bounds, Action});
        return Bounds.IsInside(FVector2D(MouseX, MouseY));
    };
    const auto MenuButton = [&](float X, float Y, float Width, const FString& Title,
        const FString& Key, EAction Action, bool bMain)
    {
        const bool bHover = Button(X, Y, Width, 49, Action);
        UI.Box(bMain ? (bHover ? LightInk : Teal) : (bHover ? Paper : Card), X, Y, Width, 49);
        UI.Box(bMain ? Warm : Teal, X, Y, 4, 49);
        UI.Label(Title, bMain ? Card : Ink, X + 17, Y + 13, 1.08f);
        UI.Label(Key, bMain ? Paper : Gray, X + Width - 14, Y + 16, .75f, 1);
    };

    const int32 MaxTotal = FMath::Max(1, Mode->DateCount) * Romance::MaxHearts;
    if (Mode->Screen == ERomanceScreen::Running)
    {
        const int32 Target = FMath::Min(Mode->NextDate, Romance::Dates - 1);
        const float Until = Mode->Deadline(Target) - Mode->RunTime;
        const FLinearColor Accent = RGB(Romance::Accents[Target]);
        UI.Box(Alpha(Ink, .91f), 24, 24, 292, 115);
        UI.Box(Accent, 24, 24, 292, 4);
        UI.Label(FString::Printf(TEXT("DATE %02d  /  %s"), Target + 1, Romance::Names[Target]), Paper, 40, 38, .92f);
        UI.Label(Mode->Place(Target), Card, 40, 61, 1.30f);
        const FString Timer = Until >= 0 ? FString::Printf(TEXT("%s  to date"), *Romance::Clock(Until))
            : Mode->TimeToClose(Target) >= 0 ? FString::Printf(TEXT("%s  until they leave"), *Romance::Clock(Mode->TimeToClose(Target)))
            : TEXT("Appointment missed");
        UI.Label(Timer, Until >= 0 ? Accent : Warm, 40, 101, .88f);
        UI.Label(Romance::Clock(Mode->RunTime), Paper, 300, 102, .84f, 1);

        UI.Box(Alpha(Ink, .91f), W - 200, 24, 176, 51);
        UI.Heart({W - 180, 42}, 15, Warm);
        UI.Label(FString::Printf(TEXT("%02d / %02d  HEARTS"), Mode->TotalHearts(), MaxTotal), Paper, W - 163, 38, .88f);

        if (const ARomanceRunner* Runner = Mode->Runner())
        {
            const FVector Here = Runner->GetActorLocation();
            const ARomancePoint* Spot = Mode->NextDate < Mode->DateCount ? Mode->DatePoint(Target) : nullptr;
            const FVector Point = Spot ? Spot->Feet() : Here;
            const float Distance = FVector::Dist2D(Here, Point);
            if (Spot && Distance > 400)
            {
                const float Relative = FMath::FindDeltaAngleDegrees(PlayerOwner->GetControlRotation().Yaw, (Point - Here).Rotation().Yaw);
                const float X = Middle + FMath::Clamp(Relative / 75.f, -1.f, 1.f) * 135.f;
                UI.Box(Alpha(Ink, .74f), Middle - 163, 24, 326, 38);
                UI.Line(Alpha(Paper, .7f), {Middle, 58}, {Middle, 62}, 1);
                UI.Heart({X, 33}, 12, Accent);
                UI.Label(FString::Printf(TEXT("%d m"), FMath::RoundToInt(Distance / 100)), Card, X, 47, .58f, .5f);
            }

            const URomanceMovement* Move = Runner->Parkour();
            const float Speed = Runner->GetVelocity().Size2D() / 100.f;
            UI.Box(Alpha(Ink, .87f), 24, H - 89, 186, 64);
            UI.Label(Move->IsSlinging() ? TEXT("SLING") : Move->IsWallRiding() ? TEXT("WALL RIDE") : Move->IsSliding() ? TEXT("SLIDE") : TEXT("SPEED"),
                Move->IsSlinging() || Move->IsWallRiding() || Move->IsSliding() ? Accent : Paper, 38, H - 80, .76f);

            // Speed streaks in from the screen edge once the runner is well past running speed.
            const float Rush = FMath::Clamp((Runner->GetVelocity().Size() - 1050.f) / 900.f, 0.f, 1.f);
            if (Rush > 0 && !Mode->Settings->bReducedMotion)
            {
                const FVector2D Centre(Canvas->ClipX * .5f, Canvas->ClipY * .5f);
                const float Reach = Centre.Size();
                const int32 Beat = FMath::FloorToInt(GetWorld()->GetTimeSeconds() * 24.f);
                for (int32 Streak = 0; Streak < 26; ++Streak)
                {
                    const FRandomStream Random(Beat * 131 + Streak * 17);
                    const float Angle = Random.FRand() * UE_TWO_PI;
                    const float Near = FMath::Lerp(.66f, .46f, Rush) + Random.FRand() * .22f;
                    const float Length = (.07f + .15f * Random.FRand()) * Rush;
                    const FVector2D Way(FMath::Cos(Angle), FMath::Sin(Angle));
                    const FVector2D From = Centre + Way * Reach * Near;
                    const FVector2D To = Centre + Way * Reach * (Near + Length);
                    DrawLine(From.X, From.Y, To.X, To.Y, Alpha(White, .08f + .2f * Rush), 1.5f * UI.S);
                }
            }

            // Boost charge: a bar that refills, bottom right.
            UI.Box(Alpha(Ink, .87f), W - 150, H - 62, 126, 37);
            UI.Label(TEXT("CHARGE"), Runner->ChargeReady() >= 1.f ? Warm : Gray, W - 138, H - 55, .66f);
            UI.Box(LightInk, W - 82, H - 46, 46, 4);
            UI.Box(Warm, W - 82, H - 46, 46 * Runner->ChargeReady(), 4);

            // Slingshot: the rope while hanging, and a ring on the dot when a surface is in reach.
            FVector SlingPoint;
            if (Move->IsSlinging())
            {
                const FVector Screen = Project(Move->GetSlingAnchor());
                // The rope itself is drawn in the world by the runner; this marks where it holds.
                if (Screen.Z > 0)
                {
                    UI.Box(Accent, Screen.X / UI.S - 3, Screen.Y / UI.S - 3, 6, 6);
                }
            }
            else if (Runner->FindSlingPoint(SlingPoint))
            {
                const float Y = H * .5f;
                UI.Box(Alpha(Accent, .9f), Middle - 7, Y - 7, 14, 1.5f);
                UI.Box(Alpha(Accent, .9f), Middle - 7, Y + 5.5f, 14, 1.5f);
                UI.Box(Alpha(Accent, .9f), Middle - 7, Y - 7, 1.5f, 14);
                UI.Box(Alpha(Accent, .9f), Middle + 5.5f, Y - 7, 1.5f, 14);
            }
            UI.Label(FString::Printf(TEXT("%04.1f m/s"), Speed), Card, 38, H - 58, 1.0f);
            UI.Box(LightInk, 116, H - 45, 78, 4);
            UI.Box(Accent, 116, H - 45, 78 * FMath::Clamp(Speed / 18.f, 0.f, 1.f), 4);
        }

        const float DotY = H * .5f;
        UI.Box(Alpha(Card, .92f), Middle - 2, DotY - 2, 4, 4);
        if (Mode->NearbyDate != INDEX_NONE)
        {
            const int32 Index = Mode->NearbyDate;
            UI.Box(Ink, Middle - 164, DotY + 48, 328, 50);
            UI.Box(Accent, Middle - 151, DotY + 59, 28, 28);
            UI.Label(TEXT("E"), Ink, Middle - 137, DotY + 62, .92f, .5f);
            UI.Label(Mode->TimeToClose(Index) >= 0
                ? FString::Printf(TEXT("Talk to %s"), Romance::Names[Index]) : TEXT("Check the meeting spot"),
                Card, Middle - 111, DotY + 63, .97f);
        }

        const FString Tip = Mode->NoticeTime > 0 ? Mode->Notice : Mode->HintText();
        if (!Tip.IsEmpty())
        {
            const float TipWidth = FMath::Min(606.f, W - 448.f);
            UI.Box(Alpha(Ink, .87f), Middle - TipWidth * .5f, H - 92, TipWidth, 39);
            UI.Label(Tip, Mode->NoticeTime > 0 ? Accent : Card, Middle, H - 81, W < 1100 ? .81f : .90f, .5f);
        }
        UI.Label(TEXT("WASD  Move    SPACE  Jump / kick    CTRL  Slide    LMB  Sling    RMB  Charge    R  Checkpoint    ESC  Pause"),
            Card, Middle, H - 35, .75f, .5f);
        // After a fall or a reset the screen comes back from dark.
        if (Mode->ResetFlash > 0)
        {
            UI.Box(Alpha(Ink, FMath::Clamp(Mode->ResetFlash / .55f, 0.f, 1.f) * .92f), 0, 0, W, H);
        }
        return;
    }

    if (Mode->Screen == ERomanceScreen::Title)
    {
        UI.Box(Alpha(Ink, .22f), 0, 0, W, H);
        const float X = 43;
        const float Y = H * .5f - 273;
        UI.Box(Alpha(Ink, .95f), X, Y, 455, 546);
        UI.Box(Warm, X, Y, 455, 5);
        UI.Label(TEXT("TODAY'S  ITINERARY"), Warm, X + 31, Y + 37, .94f);
        UI.Label(TEXT("Any-Romance"), Card, X + 29, Y + 66, 2.92f, 0, true);
        UI.Wrapped(TEXT("Three dates, one clock. Run the rooftops to Ari, then Bea, then Jules, and be on time for each of them."),
            Paper, X + 32, Y + 157, 384, 1.15f, 25);
        UI.Line(LightInk, {X + 32, Y + 252}, {X + 423, Y + 252}, 1);
        UI.Wrapped(TEXT("The clock pauses while you read. Each reply spends time you could use on the next route."),
            Paper, X + 32, Y + 274, 384, .97f, 22);

        MenuButton(X + 32, Y + 359, 391, TEXT("Start the run"), TEXT("ENTER"), EAction::Start, true);
        MenuButton(X + 32, Y + 417, 391, TEXT("Original schedule"), TEXT("3:00 / 8:00 / 13:00"), EAction::Classic, false);
        const FString Record = Mode->Settings->BestHearts < 0 ? TEXT("First-person parkour  /  Dating sim")
            : FString::Printf(TEXT("Personal best  /  %d hearts  /  %s"), Mode->Settings->BestHearts,
                *Romance::Clock(Mode->Settings->BestTime));
        UI.Label(Record, Paper, X + 32, Y + 501, .83f);
        return;
    }

    // Panels ease in: the backdrop darkens and the card rises into place.
    const float Arrive = FMath::Clamp(Mode->ScreenAge / .24f, 0.f, 1.f);
    const float Rise = FMath::Square(1.f - Arrive) * 54.f;
    UI.Box(Alpha(Ink, .65f * Arrive), 0, 0, W, H);
    if (Mode->Screen == ERomanceScreen::Date)
    {
        const int32 Index = Mode->ActiveDate;
        const Romance::FDateLine& Dialogue = Romance::Lines[Index][Mode->Line];
        const FLinearColor Accent = RGB(Romance::Accents[Index]);
        const float X = Middle - 458;
        const float Y = H * .5f - 272 + Rise;
        const float PortraitWidth = 303;
        const float Height = 544;
        const float DialogX = X + PortraitWidth;
        const float DialogWidth = 613;
        UI.Box(Accent, X, Y, PortraitWidth, Height);
        if (Mode->Portraits)
        {
            const float U = (Index + .008f) / 3.f;
            const float USize = .984f / 3.f;
            DrawTexture(Mode->Portraits, X * UI.S, Y * UI.S, PortraitWidth * UI.S, Height * UI.S, U, .02f, USize, .96f,
                White, BLEND_Opaque);
        }
        UI.Box(Alpha(Ink, .88f), X, Y + Height - 74, PortraitWidth, 74);
        UI.Label(Romance::Names[Index], Card, X + 23, Y + Height - 68, 1.8f, 0, true);
        UI.Label(Mode->Place(Index), Paper, X + 24, Y + Height - 31, .85f);

        UI.Box(Card, DialogX, Y, DialogWidth, Height);
        UI.Box(Accent, DialogX, Y, DialogWidth, 5);
        UI.Label(FString::Printf(TEXT("DATE %02d   /   REPLY %d OF 4"), Index + 1, Mode->Line + 1),
            Teal, DialogX + 28, Y + 23, .87f);
        for (int32 Heart = 0; Heart < Romance::MaxHearts; ++Heart)
        {
            UI.Heart({DialogX + DialogWidth - 123 + Heart * 21.f, Y + 27}, 15,
                Heart < Mode->DateState[Index].Hearts ? RGB(FColor(194, 78, 83)) : RGB(FColor(225, 203, 190)));
        }
        UI.Wrapped(Dialogue.Text, Ink, DialogX + 28, Y + 63, DialogWidth - 56, 1.62f, 34, true);
        UI.Label(TEXT("CHOOSE YOUR REPLY"), Gray, DialogX + 28, Y + 175, .83f);

        for (int32 ChoiceIndex = 0; ChoiceIndex < 3; ++ChoiceIndex)
        {
            const Romance::FChoice& Choice = Dialogue.Choices[ChoiceIndex];
            const float Row = Y + 203 + ChoiceIndex * 80;
            const float Left = DialogX + 27;
            const float Width = DialogWidth - 54;
            const bool bHover = Button(Left, Row, Width, 70,
                static_cast<EAction>(static_cast<uint8>(EAction::ChoiceOne) + ChoiceIndex));
            UI.Box(bHover ? Paper : RGB(FColor(246, 232, 205)), Left, Row, Width, 70);
            UI.Box(bHover ? Teal : Accent, Left, Row, 3, 70);
            UI.Box(Teal, Left + 12, Row + 22, 25, 26);
            UI.Label(FString::FromInt(ChoiceIndex + 1), Card, Left + 24.5f, Row + 24, .92f, .5f);
            UI.Wrapped(Choice.Text, Ink, Left + 50, Row + 15, Width - 132, 1.14f, 21);
            UI.Label(FString::Printf(TEXT("+%ds"), Choice.Turns * static_cast<int32>(Romance::TurnSeconds)),
                Teal, Left + Width - 14, Row + 24, .99f, 1.f);
        }
        UI.Line(RGB(FColor(217, 204, 179)), {DialogX + 27, Y + 452}, {DialogX + DialogWidth - 27, Y + 452}, 1);
        UI.Label(TEXT("ITINERARY CLOCK"), Gray, DialogX + 28, Y + 470, .75f);
        UI.Label(Romance::Clock(Mode->RunTime), Ink, DialogX + 28, Y + 493, 1.10f);
        if (Mode->NextDate < Mode->DateCount)
        {
            UI.Label(FString::Printf(TEXT("Next: %s at %s"), Romance::Names[Mode->NextDate],
                *Romance::Clock(Mode->Deadline(Mode->NextDate))), Teal, DialogX + DialogWidth - 27, Y + 495, .92f, 1.f);
        }
        return;
    }

    const float X = Middle - 303;
    const float Y = H * .5f - 266 + Rise;
    UI.Box(Card, X, Y, 606, 532);
    UI.Box(Teal, X, Y, 606, 5);
    if (Mode->Screen == ERomanceScreen::Paused)
    {
        UI.Label(TEXT("Paused"), Ink, X + 31, Y + 30, 2.15f, 0, true);
        UI.Label(TEXT("MOUSE SENSITIVITY"), Teal, X + 31, Y + 126, .90f);
        const bool bLess = Button(X + 31, Y + 157, 43, 43, EAction::Less);
        const bool bMore = Button(X + 166, Y + 157, 43, 43, EAction::More);
        UI.Box(bLess ? LightInk : Teal, X + 31, Y + 157, 43, 43);
        UI.Box(bMore ? LightInk : Teal, X + 166, Y + 157, 43, 43);
        UI.Label(TEXT("-"), White, X + 52, Y + 163, 1.36f, .5f);
        UI.Label(TEXT("+"), White, X + 187, Y + 163, 1.36f, .5f);
        UI.Label(FString::Printf(TEXT("%.1fx"), Mode->Settings->Sensitivity), Ink, X + 120, Y + 164, 1.18f, .5f);
        MenuButton(X + 31, Y + 228, 544, Mode->Settings->bReducedMotion ? TEXT("Reduced motion on") : TEXT("Reduced motion off"),
            TEXT("M"), EAction::Motion, false);
        MenuButton(X + 31, Y + 344, 544, TEXT("Resume run"), TEXT("ESC"), EAction::Resume, true);
        MenuButton(X + 31, Y + 404, 544, TEXT("Return to title"), TEXT("END RUN"), EAction::Title, false);
        return;
    }

    UI.Label(TEXT("ITINERARY COMPLETE"), Teal, X + 31, Y + 30, .91f);
    UI.Label(Mode->TotalHearts() == MaxTotal ? TEXT("Perfect timing") : TEXT("What a day"), Ink, X + 29, Y + 54, 2.1f, 0, true);
    UI.Label(FString::Printf(TEXT("%d / %d hearts     %s elapsed     %d checkpoint resets"), Mode->TotalHearts(), MaxTotal,
        *Romance::Clock(Mode->RunTime), Mode->Falls), Teal, X + 31, Y + 128, 1.05f);
    if (Mode->bNewBest)
    {
        UI.Label(TEXT("PERSONAL BEST"), Teal, X + 574, Y + 32, .78f, 1);
    }
    for (int32 Index = 0; Index < Mode->DateCount; ++Index)
    {
        const auto& Date = Mode->DateState[Index];
        const float Row = Y + 170 + Index * 59.f;
        UI.Box(Paper, X + 31, Row, 544, 51);
        UI.Box(RGB(Romance::Accents[Index]), X + 31, Row, 4, 51);
        UI.Label(Romance::Names[Index], Ink, X + 45, Row + 6, 1.07f);
        UI.Label(Mode->Place(Index), Gray, X + 45, Row + 27, .77f);
        UI.Label(Date.Result == EDateResult::Met
            ? FString::Printf(TEXT("%d hearts   /   %s"), Date.Hearts, *Romance::Clock(Date.Arrival)) : TEXT("Missed"),
            Teal, X + 558, Row + 16, .93f, 1.f);
    }
    MenuButton(X + 31, Y + 376, 544, TEXT("Run again"), TEXT("ENTER"), EAction::Resume, true);
    MenuButton(X + 31, Y + 436, 544, TEXT("Return to title"), TEXT(""), EAction::Title, false);
}

void ARomanceHUD::Click()
{
    ARomanceMode* Mode = GetWorld()->GetAuthGameMode<ARomanceMode>();
    float X, Y;
    if (!Mode || !PlayerOwner || !PlayerOwner->bShowMouseCursor || !PlayerOwner->GetMousePosition(X, Y))
    {
        return;
    }

    for (const FButton& Entry : Buttons)
    {
        if (!Entry.Bounds.IsInside(FVector2D(X, Y)))
        {
            continue;
        }
        switch (Entry.Action)
        {
        case EAction::Start: Mode->BeginRun(false); break;
        case EAction::Classic: Mode->BeginRun(true); break;
        case EAction::Resume: Mode->Confirm(); break;
        case EAction::Title: Mode->ReturnToTitle(); break;
        case EAction::Less: Mode->ChangeSensitivity(-.1f); break;
        case EAction::More: Mode->ChangeSensitivity(.1f); break;
        case EAction::Motion: Mode->ToggleReducedMotion(); break;
        case EAction::ChoiceOne: Mode->Choose(0); break;
        case EAction::ChoiceTwo: Mode->Choose(1); break;
        case EAction::ChoiceThree: Mode->Choose(2); break;
        }
        return;
    }
}
