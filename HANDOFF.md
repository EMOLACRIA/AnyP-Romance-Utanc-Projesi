# Any Romance — devir notu

Bu not, işi yeni bir sohbette sürdürecek kişi içindir. Önce tamamını oku, sonra işe başla.

## Hedef

**Any Romance**: birinci şahıs parkur-speedrun date-sim. Oyunu Unreal Engine 5.8'de, bu boş projede (`AnyRomanceUE`) yeniden kuruyoruz.

- **Aktarılacak:** oyunun kuralları ve hareket mekanikleri.
- **Aktarılmayacak:** eski harita (ada). Bölümler, hazırladığımız asset paketlerinden yeni baştan kurulacak.

## Oyun kuralları

- Oyuncu bir başlangıç noktasından ilk date'ine koşar. Süre dardır.
- Belirli süreyi aşarsa date'in buluşacağı kişi mekânı terk eder ve o date kaçar.
- Zamanında yetişirse date-sim başlar: **4 soru, her soruda 3 seçenek**.
- Her seçenek 1t, 2t ya da 3t süre götürür; **t = 15 saniye**. Seçenekler ayrıca kalp (ilgi) puanı verir.
- Date sırasında koşu saati durur; seçilen cevabın süresi saate eklenir.
- İlk date bitince oyuncu ikinci date'e, sonra üçüncüye koşar. **Üçüncü date bitince oyun biter** ve sonuç ekranı gelir.
- Üç karakter: Ari, Bea, Jules. 12 soru ve 36 cevap hazır (aşağıda).

## Kaynaklar

| Ne | Nerede |
|---|---|
| Yeni proje (burası) | `C:\Users\emo\Documents\Unreal Projects\AnyRomanceUE` |
| Eski Unreal denemesi (referans kod) | `C:\Program Files\Epic Games\UE_5.8\Projects\AnyRomance\Source\AnyRomance` |
| Asset paketleri | `C:\Users\emo\Desktop\asset pack` |
| Orijinal Godot projesi | `https://github.com/EMOLACRIA/Any-Romance` (gizli depo; erişim yok) |

### Eski Unreal denemesindeki referans kod
Kopyalanabilir ve uyarlanabilir; ama **ölçeği farklı** (aşağıya bak).

- `RomanceMode.h/.cpp` — oyun durumu (Title, Running, Date, Paused, Results), date sırası, süre ve son teslim mantığı, kalp puanı, checkpoint, kayıt, girdi bağlama (`ARomanceController`).
- `RomanceRunner.h/.cpp` — hareket bileşeni: koşu, zıplama (coyote time, jump buffer), duvarda koşma ve duvardan tekme, kayma ve kayarak zıplama, **slingshot**, **rocket jump** (`ARomanceCharge`), kamera salınımı.
- `RomanceHUD.cpp` — Canvas ile çizilen arayüz: saat, kalpler, yön göstergesi, date ekranı, menüler.
- `OriginalDialogue.inl` — 12 soru, 36 cevap, kalp ve süre değerleri. Aynen kullanılabilir.
- `IslandSet.h/.cpp` — eski adanın hedef noktaları ve ışığı. **Aktarılmayacak**; yerine yeni bölümün noktaları gelecek.
- `RomanceVerification.cpp` — komut satırından çalışan kural ve hareket testleri (`-RomanceVerifyRules` vb.). Yeni projede benzeri kurulmalı.

### Mekanikler (hepsi aktarılacak)
| Mekanik | Tuş | Davranış |
|---|---|---|
| Koşu, zıplama | WASD, Space | Havada yön kontrolü tam; momentum korunur |
| Wallrun | W basılıyken duvara yakın zıpla | Duvar boyunca koşar; süresi sınırlı |
| Walljump | Wallrun sırasında Space | Duvardan uzağa ve yukarı tekme; karşılıklı duvarlarda zincirlenir |
| Slide, slide jump | Ctrl ya da C | Hızlıyken kayar, yokuş aşağı hızlanır; kayarken zıplamak ek hız verir |
| Rocket jump | Sağ fare ya da Q | Baktığı yöne patlayıcı atar; patlama oyuncuyu uzağa ve yukarı iter. `BlastSwitch` etiketli aktörleri tetikler |
| Slingshot | Sol fare (basılı) | Baktığı yüzeye tutunur, ipte sallanır; ip yavaşça sarılır. Space ile sıçrayarak bırakır |
| Checkpoint | R | Son checkpoint'e döner; saat işlemeye devam eder |

## Kritik fark: ölçek

Eski denemede ada küçültülmüştü: oyuncu **89 cm** boyundaydı, yerçekimi `-1960`, koşu hızı 870 uu/s idi. Asset paketleri ise **gerçek ölçekte** (1 birim = 1 cm, karakter yaklaşık 180 cm varsayıldı).

Yeni projede **gerçek ölçek** kullan: standart 88 cm yarı yükseklikli kapsül, yaklaşık 180 cm karakter. Eski koddaki tüm hız, mesafe ve yükseklik sabitleri (koşu hızı, zıplama, wallrun mesafesi, patlama yarıçapı, ip menzili, kamera yüksekliği) buna göre yeniden ayarlanmalı; kabaca iki katı iyi bir başlangıçtır ama parkur paketinin ölçülerine göre denenmeli.

Parkur paketinin varsaydığı ölçüler: wallrun duvarı en az 3 m, walljump şaftı 2 m genişlik × 6 m yükseklik, kayma boşluğu 1,0 m, koşarak atlanan boşluk 4 m, bina ızgarası 4 m, çatı yürüme yüksekliği kat × 4 + 0,3 m.

## Asset paketleri

`C:\Users\emo\Desktop\asset pack` altında dört paket var; hepsi aynı hand-painted çizgide. Her paketin kendi `README.md` dosyası içerik, ölçü ve origin kurallarını anlatır; **bölüm kurmadan önce oku**.

| Paket | Adet | Ne için |
|---|---|---|
| `ParkourPack` | 68 | Rota: çatı modülleri, wallrun ve walljump duvarları, kayma parçaları, rocket jump düğmeleri ve kapıları, slingshot tutunma noktaları, checkpoint, start/finish |
| `CafeAssetPack` | 38 | Date mekânı: modern şehir kafesi |
| `HarborCafePack` | 38 | Date mekânı: liman kafesi |
| `MountainCafePack` | 55 | Date mekânı: karlı dağ evi kafesi |

- **İçe aktarma:** `asset pack\import_to_unreal.py` dört paketi `/Game/<Paket>` altına alır, Nanite'ı kapatır ve kaydeder. Editörde Tools > Execute Python Script ile ya da komut satırından çalıştırılır. UE 5.8.4'te denendi.
- **Dosyalar:** her pakette `unreal\SM_<Ad>.fbx` (çarpışma kabuklarıyla) ve `textures\T_<Ad>.png`.
- **Yön:** Blender'daki -Y, UE'de +Y olur; assetlerin önü UE'de +Y'ye bakar.
- **Görsel olanlar:** checkpoint halkası, zıplama pedi, düğmeler, kapı, platform, tutunma halkaları yalnızca mesh'tir; davranışları kodda yazılacak.
- **Işık:** ateş, lamba ve pencere parıltıları dokuya boyalıdır; ışık yaymaz.
- **Izgaralar uyuşmaz:** paketlerin duvar ve zemin modülleri birbirine oturmaz. Her date mekânı kendi paketinden kurulmalı; parkur rotası bunları birbirine bağlar.

## Bölüm planı (öneri)

Üç date, üç mekân; aralarını parkur rotaları bağlar:

1. Başlangıç → **Date 1**: şehir çatıları üzerinden modern kafeye (`ParkourPack` + `CafeAssetPack`).
2. Date 1 → **Date 2**: liman kafesine (`HarborCafePack`).
3. Date 2 → **Date 3**: dağ evi kafesine (`MountainCafePack`).

Her rotada mekanikler sırayla öğretilsin ve sonra birleştirilsin. Paketlerin `.blend` dosyalarındaki demo sahneler (`Demo_Parkour`, `Demo_Cafe`, `Demo_Harbor`, `Demo_Mountain`) yerleşim örneğidir.

Kullanıcı bölüm tasarımına henüz karar vermedi; rotaların uzunluğu, süre hedefleri ve mekân sırası ona sorulmalı.

## Önerilen iş sırası

1. Kodu taşı: oyun modu, karakter ve hareket bileşeni, HUD, diyalog verisi. Ada ile ilgili her şeyi çıkar; hedef ve checkpoint noktalarını seviyeden okunan aktörlere (ya da etiketlere) çevir.
2. Hareket sabitlerini gerçek ölçeğe göre ayarla.
3. Asset paketlerini içe aktar.
4. Yalnızca `ParkourPack` ile kısa bir **test rotası** kur; tüm mekanikleri oynanır hâle getir. Kullanıcı hissi onaylamadan uzun bölüm kurma.
5. Tutunma halkası, patlayıcı düğmesi, kapı, platform, checkpoint ve zıplama pedi için davranışları yaz; patlayıcının geçici küresini `SM_Boost_Charge` ile değiştir.
6. Üç date mekânını ve aralarındaki rotaları kur.
7. Kural ve hareket testlerini komut satırından çalışır hâle getir.

## Durum (14 Kasım 2026)

İş sırasının 1. ve 2. adımı bitti, 3. adım yalnızca `ParkourPack` için yapıldı, 4. adım kullanıcının his onayını bekliyor.

- **Kod taşındı** (`Source/AnyRomanceUE`): `RomanceMode`, `RomanceRunner`, `RomanceHUD`, `RomanceVerification`, `OriginalDialogue.inl`. Ada kodu yok.
- **Rota seviyeden okunur:** `RomanceCourse.h` içindeki `ARomancePoint` aktörleri (Start, Checkpoint, Date, Pad, Marker) ve etiketler (`RideWall`, `BlastSwitch`, `TitleCamera`, `FallLine`). Seviyede kaç Date noktası varsa koşu o kadar date sonra biter.
- **Gerçek ölçek:** kapsül 34 × 88 cm, göz 156 cm, yerçekimi `-2600`. Bütün his sabitleri `RomanceRunner.cpp` başındaki `Tune` bloğunda. Eski değerlerin iki katı alınmadı: iki katı 15 m'lik zıplama verir, paket ise 4 m'lik boşluk varsayıyor. Koşu 9,2 m/s, zıplama 1,4 m yükseklik ve yaklaşık 6 m menzil.
- **Eski koddan bilerek ayrılan yerler:** duvar tekmesi duvar boyunca olan hızı korur (eskiden en az 920'ye zorlanıyordu; 2 m'lik şaftta oyuncuyu yana fırlatıyordu); duvar koşusu yalnızca duvar boyunca giderken başlar, duvara dik koşarken başlamaz.
- **Test rotası:** `/Game/Maps/L_TestRoute`, `Scripts/build_test_route.py` ile kurulur (yeniden çalıştırmak haritayı baştan kurar). `Scripts/import_parkour.py` yalnızca `ParkourPack`'i içe aktarır. Öteki üç paket henüz içe aktarılmadı.
- **Eski projeden kopyalananlar:** `Content/Island` altında date portreleri ve ses efektleri.
- **Testler:** `UnrealEditor-Cmd.exe <uproject> /Game/Maps/L_TestRoute -game -nullrhi -unattended -RomanceVerifyRules` (ya da `-RomanceVerifyMovement`). İkisi de geçiyor. Hareket testi mekanikleri tek tek dener; rotanın baştan sona koşulabildiğini kanıtlamaz.
- **Ekran görüntüsü:** `-game -windowed -RomancePreview=9 -RomanceView=X,Y,Z,Pitch,Yaw -RomanceShot=ad` → `Saved/Shots/ad.png`.
- **His onayı:** kullanıcı test rotasını oynadı ve hareket hissini onayladı (14 Kasım 2026).
- **5. adım, kod yazıldı:** `ARomanceMover` (düğmeyle yukarı kayan kapı, yükselen platform), `Anchor` noktası (slingshot nişanı halkaya yakınken ipi halkaya kilitler; halka yoksa baktığı yüzeye tutunur), tetiklenen düğmenin `_On` mesh'ine dönmesi, zıplama pedi (`Pad` noktası). Patlayıcı `SM_Boost_Charge` kullanıyor. Checkpoint halkası nokta yarıçapıyla tetiklenir. `-RomanceVerifyMovement` artık `VerifySwitch` etiketini de dener.
- **Bekleyen:** bu kod oyun hedefinde derlendi; editör modülü derlenmedi ve `build_test_route.py` yeniden çalıştırılmadı (editör açıktı). Editör kapanınca: editör hedefini derle, betiği çalıştır, iki testi çalıştır.
- **Asset paketleri:** dördü de içe aktarıldı (`Scripts/import_cafes.py` üç kafe paketini alır).
- **5. adım doğrulandı:** editör modülü derlendi, test rotası yeniden kuruldu, testler geçti. (Yukarıdaki "Bekleyen" maddesi kapandı.)
- **6. adım, ilk sürüm kuruldu:** `/Game/Maps/L_AnyRomance` (varsayılan harita), `Scripts/build_main_level.py` + `Scripts/route_kit.py`. Kullanıcı kararları: şehir → liman → dağ, rota başına 60-90 sn, saatler rotaya göre. Rotalar 288 / 316 / 316 m; date saatleri 95 / 345 / 600 sn. Bu saatler ölçüm değil tahmindir (ortalama 4 m/s + %30 pay, date başına 150 sn cevap payı); kullanıcı oynayınca ayarlanmalı. Mekânlar paketlerin demo yerleşimlerinden aktarıldı.
- **Ana haritada testler:** kural testi ve 21 hareket aşaması (her duvar koşusu, kayma kapısı, vinç ve düğmeli kapı) geçiyor. Rotalar baştan sona oynanmadı; boşluk mesafeleri, vinç geçişleri ve şaftlar oyunda doğrulanmadı.
- **7. adım:** `-RomanceVerifyCourse` eklendi: koşucuyu sırayla her checkpoint'e ve date noktasına koyar; sağlam zemine inip noktayı tetiklemesini, date'in açılıp dört cevabın verilmesini ve koşunun sonuç ekranında bitmesini ister. Ana haritada 27 checkpoint ve 3 date ile geçiyor. Üç testi iki haritada birden çalıştırmak için: `powershell -ExecutionPolicy Bypass -File Scripts\run_tests.ps1` (editör kapalı olmalı).
- **Testlerin kapsamadığı:** noktalar arasındaki parkuru hiçbir test koşmaz. Boşlukların atlanabildiği, vinç geçişleri ve şaftlar yalnızca oynanarak doğrulanır.
- **Henüz yapılmadı:** ana haritanın oynanarak denenmesi ve date saatlerinin ayarı.

## Polish turu (15 Kasım 2026)

Kullanıcı geri bildirimi: hareket hissi iyi; ama dünya bomboş ve prototip gibi, binaların hepsi aynı (asset paketleri bu ölçek için yetmiyor), juice eksik, date'lerde karakter yok.

- **Juice (kod):** patlamada büyüyen parlak kabuk + ışık + kamera sarsıntısı + FOV tepmesi (`ARomanceBurst`, `AddShake`, `KickFov`); sert inişte sarsıntı; slingshot ipi ve kancası dünyada çiziliyor; yüksek hızda ekran kenarında hız çizgileri; date ve menü panelleri kayarak geliyor. Sarsıntı ve çizgiler "reduced motion" açıkken kapalı. Hareket sabitlerine dokunulmadı.
- **Date karakterleri:** Ari, Bea, Jules'un giydirilmiş modelleri eski projenin `Art\SelectedCharacters` klasöründen `/Game/Characters` altına alındı ve mekânlara yerleştirildi (`DateActor0..2` etiketi). Date kaçınca karakter gizlenir. Date noktası karakterin bir adım önündedir.
- **Dünya:** `ARomanceCityBlock` (instanced, çarpışmasız) ile üç rotanın iki yanına 294 arka plan binası; sekiz cephe rengi (`/Game/Romance/MI_Block_0..7`, rota binalarına da uygulanıyor); limanda ufka kadar deniz ve kayıklar; dağ evinin arkasında büyütülmüş kayalardan zirveler; bulut, koyu zemin, sokak seviyesinde sis.
- **Betikler:** `Scripts/make_polish_assets.py` (malzemeler + karakter içe aktarma; önce bu), sonra `Scripts/build_main_level.py`.
- **Bilinen sınırlar:** arka plan şehri hâlâ tek bina modülünün renk ve boy çeşitlemesi; gerçek çeşitlilik için yeni bina/sokak assetleri gerekir. Karakterler sabit duruyor (animasyon yok). Juice oyunda oynanarak denenmedi; yalnızca derleme ve komut satırı testleri doğrulandı. Yeni ses yok.

## CityPack ve sokaklı şehir (15 Kasım 2026)

Kullanıcı geri bildirimi: bütün binalar aynı, dünya boş. Yeni bir asset paketi üretildi ve ana haritada kullanıldı.

- **Paket:** `asset pack\CityPack` (31 asset, öteki paketlerle aynı boya/bake hattı; `README.md` ölçü ve origin kurallarını anlatır). 6 cephe bloğu, 4 zemin kat bloğu, 3 çatı kenarı + köşeleri, yol / yaya geçidi / kaldırım / bordür karoları, lamba, ağaç, bank, araba, minibüs, durak, trafik ışığı, çöp kutusu, yangın musluğu. Bloklar `Building_Block_4x4` ile aynı 4 × 4 × 4 m kutu ve aynı kutu çarpışması. Yeniden üretme: `blender -b --factory-startup --python scripts\build_city_pack.py` (yaklaşık 15 dk; adı verilen assetleri tek tek de üretir).
- **İçe aktarma:** `Scripts/import_city.py` → `/Game/CityPack`; blok ve çatı kenarlarında Nanite açık. Sonra `Scripts/make_polish_assets.py`: her instanced mesh için `/Game/Romance/City/MI_<Mesh>_<n>` (cepheler 5 renk, araçlar 6 renk).
- **Kod:** `ARomanceCityBlock::AddPieces` — betiğin verdiği mesh + malzeme + transform listesini instance olarak çizer (her mesh/malzeme çifti tek bileşen, çarpışma yok). Şehrin tamamı üç aktördür: `CityGround`, `CityBuildings`, `CityStreet`. Deniz hâlâ eski `flat_grid` yolunu kullanır.
- **Yerleşim** (`Scripts/route_kit.py`, `city()`): dünya ızgarasında iki şeritli sokak ağı, kavşaklarda yaya geçidi; her parsel bordürlü kaldırımla çevrili; parsellerde bina, boş kalanlarda küçük meydan ya da otopark. Her bina bir stil seçer (ana cephe, araya karışan ikinci cephe, köşe cumbası, zemin katlar, çatı kenarı, renk); yüksek ve geniş olanların üstünde daha dar bir kule olur; çatılarda su deposu, klima, pano. Şehir rotaların 88 m çevresine yayılır. `kit.lane` rota koridoru, `kit.void` şehir olmayan yer (deniz, dağ), `kit.core` rota binalarının altı (ağaç ve lamba konmaz).
- **Rota binaları:** yalnızca kutunun içinde kalan bloklar (`ROUTE_FACADES`, `ROUTE_GROUNDS`) bina başına değişerek kullanılır; taban alanı, yükseklik ve çarpışma aynı. Altı test geçiyor.
- **Bilinen sınırlar:** dağın çevresi hâlâ düz koyu zemin. Şehrin dış kenarı sisin içinde aniden biter. Kare hızı ölçülmedi (yaklaşık 16 bin cephe ve 15 bin zemin instance'ı var); düşerse `city(reach=...)` küçültülür. Oynanarak denenmedi; ekran görüntüleri `Saved/Shots/city_*.png`.

## Revizyon turu: mekânlar, düşme, deniz, NPC (15 Kasım 2026)

Kullanıcı geri bildirimi: date mekânları düzensiz ve çirkin; düşme noktası yok; kapalı mekânlara girilemiyor; deniz binalar ve karayla kötü birleşiyor; E ile konuşulan NPC'lerin yeri kötü.

- **Girilemeyen mekânlar:** kafe ve dağ evindeki kapı kanatları yarı açıktı ve çarpışmaları kapı boşluğunu kapatıyordu. Kanatlar duvara yaslanacak kadar açıldı ve çarpışmaları kapatıldı.
- **Şehir kafesi:** iki duvarlı demo dekoru yerine dört duvarlı, tavanlı ve çatılı kapalı bir kafe (kapı gelen oyuncuya bakar), dışında tabela, saksılar, menü panosu; çatıda masalı teras ve çevresinde parapet (iniş ve harbor çıkışında açıklık).
- **Düşme:** sokak, rıhtım ve denizde zemin yok. Ayakları `FallLine` etiketli işaretin (z = -30) altına inen oyuncu düşmüş sayılır: ekran kararıp açılır, ses çalar, "You fell" yazısı çıkar, son checkpoint'e döner, saat işlemeye devam eder. `R` ile dönüşte de aynı kararma var.
- **Deniz ve kıyı:** su sokak seviyesinin 1,2 m altına (z -120) indi; kıyı çizgisi harbor-local y = 1100 (dünya y = 31000). Şehir denize her değdiği yerde taş rıhtım duvarı, bordür, korkuluk ve fener alır (`kit.sea`, `route_kit.city`). İskele 40 cm'ye indi, kazıklar suya uzandı, iskele kenarına korkuluk ve fener eklendi. Rota 3 artık suyun içinde değil, karada kıyı boyunca gider ve rota 2'den 12 m uzakta başlar (date atlanamasın diye); iskeledeki ped onun ilk çatısına fırlatır.
- **Dağ:** kayalar suyun altına iner, kara tarafı karla kaplı, kıyısı kaya sırasıyla biter; şehir dağın önüne kadar uzanır.
- **NPC'ler:** Ari kafenin içinde iki kişilik masanın yanında kapıya dönük; Bea iskelede şemsiye altındaki masanın yanında iskeleye dönük; Jules dağ evinin kapısından 1,6 m içeride kapıya dönük, kapıdan bakınca görünüyor. Date noktası her birinin bir adım önünde.
- **Hata düzeltmesi:** 180° döndürülmüş çerçevelerde kutu koordinatları kayan nokta hatası yüzünden bir hücre sırasını düşürüyordu (`route_kit.box` artık yuvarlıyor).
- Altı test geçiyor. Oynanarak denenmedi.

## Çalışma notları

- **Açık editör derlemeyi engellerse:** yalnızca proje seçme ekranında bekleyen (bu projeyi yüklememiş) bir editör varsa `Build.bat` komutuna `-NoHotReloadFromIDE` eklemek yeter. Editör bu projeyi açmışsa önce kapatılmalı.

- **Motor:** UE 5.8.4, `C:\Program Files\Epic Games\UE_5.8`. Proje boş bir C++ projesi; modül adı `AnyRomanceUE`. Oyun hedefi derlendi, editör modülü henüz derlenmedi (ilk açılışta editör derlemeyi soracak).
- **Derleme:** `Engine\Build\BatchFiles\Build.bat AnyRomanceUEEditor Win64 Development -Project=<uproject> -WaitMutex`. Editör açıkken Live Coding bu derlemeyi engeller; o durumda editörde Ctrl+Alt+F11 kullan ya da oyun hedefini (`AnyRomanceUE`) derle.
- **Arayüzsüz Python:** `UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<dosya> -unattended -nullrhi`. Bu modda `get_editor_subsystem` boş döner; `unreal.get_default_object(...)` kullan.
- **Blender:** yeni asset gerekirse aynı boya/bake hattı her paketin `scripts\cafe_lib.py` dosyasındadır. Blender MCP bağlantısı zaman zaman kopuyor; arayüzsüz Blender (`blender -b ... --python`) aynı işi görür.
- **Sürüm kontrolü:** makinede git kurulu değil, proje bir depoya bağlı değil.
- **Doğrulama:** şimdiye kadar hiçbir mekanik oyunda oynanarak denenmedi; yalnızca derleme doğrulandı. Hareket hissi kullanıcıyla birlikte ayarlanmalı.
- **Dil:** kullanıcı Türkçe yazıyor.
