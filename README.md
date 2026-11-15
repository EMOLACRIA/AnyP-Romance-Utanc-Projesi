# Any Romance

Birinci şahıs parkur-speedrun date-sim. Unreal Engine 5.8 (C++), modül adı `AnyRomanceUE`.

Bir başlangıç noktasından ilk date'e koşarsın; süre dardır. Yetişirsen date-sim başlar: dört soru, her soruda üç cevap, her cevap 15, 30 ya da 45 saniye götürür. Üç date (Ari, Bea, Jules) bitince sonuç ekranı gelir.

## Çalıştırma

1. Unreal Engine 5.8 ve Visual Studio 2022 (C++ oyun geliştirme) kurulu olmalı.
2. Depoyu Git LFS ile klonla (`git lfs install`, sonra `git clone`); haritalar ve assetler LFS'tedir.
3. `AnyRomanceUE.uproject` dosyasını aç; editör modülü derlemeyi sorarsa kabul et.
4. Editör `L_AnyRomance` haritasıyla açılır. Play, sonra Enter.

| Tuş | İş |
|---|---|
| WASD, fare | Hareket, bakış |
| Space | Zıplama; duvar koşusunda tekme |
| Ctrl / C | Kayma |
| Sol fare (basılı) | Slingshot |
| Sağ fare / Q | Patlayıcı (ayak dibinde rocket jump, düğmeleri tetikler) |
| E | Date ile konuş |
| 1-3 | Cevap seç |
| R | Son checkpoint |
| Esc | Duraklat |

## Klasörler

- `Source/AnyRomanceUE` — oyun kuralları (`RomanceMode`), hareket (`RomanceRunner`), arayüz (`RomanceHUD`), seviyeden okunan rota aktörleri (`RomanceCourse`), komut satırı testleri (`RomanceVerification`).
- `Scripts` — haritaları ve malzemeleri kuran Unreal Python betikleri, `run_tests.ps1`.
- `Content/Maps` — `L_AnyRomance` (ana harita), `L_TestRoute` (his testi rotası).
- `HANDOFF.md` — projenin güncel durumu, kararlar, derleme ve test komutları.

Asset paketlerinin kaynak dosyaları (Blender, FBX) bu depoda değildir; betikler onları `C:\Users\emo\Desktop\asset pack` altında arar. İçe aktarılmış hâlleri `Content` altındadır, haritaları yeniden kurmak için kaynaklar gerekmez.

## Testler

Editör kapalıyken:

```
powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1
```
