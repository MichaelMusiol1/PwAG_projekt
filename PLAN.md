# Plan projektu

Gra RPG z widokiem z pierwszej osoby w stylu Might and Magic VII (PwAG, grupa 1).
Opis gry, budowanie i sterowanie są w [README.md](README.md).

## Założenia

- C++17, OpenGL 3.3 core (tylko shadery), GLSL 330, CMake. Windows (Visual Studio albo MinGW) i Linux.
- Biblioteki: freeglut (okno i wejście, jak GLUT na laboratoriach) i GLM (matematyka). Nic więcej.
- Widok z pierwszej osoby, jedna postać.
- Potwory i NPC to sprite'y 2D z klatkami przodu i tyłu, przedmioty na ziemi to ikony,
  a budynki, drzewa i dekoracje to modele OBJ.
- Własny interfejs: prostokąty, ikony i czcionka bitmapowa z polskimi znakami.
- Tekstury w BMP, kolor magenta (255, 0, 255) jest przezroczysty.
- Świat, postacie i nazwy są nasze, z MM7 bierzemy tylko mechaniki.

## Harmonogram

| Etap | Termin | Zakres | Stan |
|---|---|---|---|
| 1 | 26.11.2026 | budowanie, silnik graficzny, dolina i krypta, ruch, kolizje, przejścia | gotowe |
| 2 | 05.12.2026 | przedmioty i ekwipunek | do zrobienia |
| 3 | 14.12.2026 | potwory i walka | do zrobienia |
| 4 | 15.01.2027 | NPC, dialogi, zadanie, docelowe modele i grafiki (wersja beta) | do zrobienia |
| 5 | 29.01.2027 | testy, poprawki, raport końcowy (wersja finalna) | do zrobienia |

W raporcie ekwipunek był na 23.12, ale robimy go przed walką, bo walka korzysta ze statystyk broni
i z przedmiotów wypadających z potworów.

## Etap 1 - zrobione

- CMake + `build.cmd` / `run.cmd` (Windows i Linux), freeglut budowany razem z grą.
- Grafika: oświetlenie Phonga na piksel (słońce + do 16 świateł punktowych), mgła, teren z czterech tekstur
  mieszanych splatmapą, niebo, woda, sprite'y, rozpad na cząsteczki (shader geometrii), interfejs 2D.
- Dolina Strumieni z heightmapy (strefy: Wioska, Las, Wzgórza, Ruiny, Jezioro) i Krypta Zapomnianych z siatki znaków.
- Kolizje (okręgi i prostokąty, ściany krypty, granice mapy), przejścia między lokacjami, latarnia gracza.
- HUD, ekran pomocy, siatka trójkątów (F2), wolna kamera (F3), znaczniki świateł (F4), zrzuty ekranu (F12).

## Etap 2 - przedmioty i ekwipunek (`inventory.cpp`)

- Przedmioty: sztylet, miecz, topór, zbroja skórzana, kolczuga, mała i duża mikstura, złoto, amulet (do zadania).
- Przedmioty leżą na ziemi jako ikony, E podnosi najbliższy, złoto od razu trafia do licznika.
- Ekwipunek 4 x 4 pola, osobne miejsce na broń i zbroję. Atak i obrona zależą od założonych przedmiotów.
- Ekran ekwipunku (I): ikony, statystyki postaci, opis po najechaniu myszą; LPM użyj / załóż, PPM wyrzuć.

## Etap 3 - walka (`combat.cpp`)

- Potwory: szczur, wilk, goblin, szkielet, boss krypty.
- Zachowanie: stoi lub chodzi przy swoim miejscu, goni gracza, gdy go zobaczy, atakuje z bliska,
  wraca, gdy gracz ucieknie.
- Sprite: przód albo tył zależnie od tego, jak potwór stoi względem gracza; klatki kroku, ataku i śmierci.
- Gracz atakuje LPM albo Spacją. Cios w plecy zadaje podwójne obrażenia.
- Po śmierci potwór rozpada się na cząsteczki i zostawia łup. Doświadczenie i kolejne poziomy postaci.
- Ekran śmierci gracza (R - nowa gra).

## Etap 4 - NPC, dialogi, zadanie (`dialog.cpp`)

- NPC w dolinie: sołtys (daje zadanie), zielarka (leczy, raz daje miksturę), strażnik przy krypcie.
- Rozmowa w oknie na dole ekranu: portret, tekst i opcje wybierane klawiszami 1-4.
- Zadanie "Amulet sołtysa": boss krypty ma amulet, po jego oddaniu sołtys daje nagrodę.
- Podmiana tymczasowych modeli i grafik na docelowe (tylko pliki w `data/`).

## Etap 5 - wersja finalna

- Pełne przejście gry na Windowsie i Linuxie, poprawki, balans.
- Raport końcowy: opis projektu, specyfikacja wewnętrzna (moduły i najważniejsze funkcje), instrukcja obsługi,
  opis testów ze zrzutami ekranu, wnioski. Na liście narzędzi bez Dear ImGui (nie jest używany),
  dopisać freeglut i GLM; API: OpenGL 3.3 core.
- Jeśli zostanie czas: cykl dnia i nocy, minimapa, sklep u kowala.

## Formaty danych

**Mapy** (`data/maps/*.txt`) to pliki tekstowe, jedna komenda w linii. Opis wszystkich komend jest na początku
każdego pliku. Przykład:
```
prop   dom.obj dom.bmp  47 80  135  1.0  6x5    # model tekstura x z kąt skala kolizja [świecenie]
light  26 78 3.0   1.0 0.78 0.45  11 flicker    # x z wysokość  kolor  zasięg  [migotanie]
zone   10 62 55 104 Wioska                      # prostokąt strefy i nazwa
portal 100 21.2 1.4 krypta.txt                  # przejście do innej mapy
monster wilk 70 40                              # miejsca potworów, NPC i przedmiotów (etapy 2-4)
```
Kolizja: promień okręgu (`0.6`), prostokąt `szerokość x głębokość` modelu (`6x5`) albo `0`.
Pozycję gracza do wpisania w mapę widać na górnym pasku po naciśnięciu F4.

**Krypta** to siatka znaków (jedno pole = 3 m): `#` ściana, `.` podłoga, `S` start, `E` wyjście,
`T` pochodnia na ścianie, `K` koksownik, `P` kolumna, `r g s B` potwory, `m M $ t k` przedmioty.

**Modele OBJ** z Blendera: File > Export > Wavefront OBJ, zaznaczone Triangulate Faces, UV i Normals,
Forward -Z, Up Y. 1 jednostka = 1 m, punkt (0, 0, 0) u podstawy, jedna tekstura na model.
Przód modelu (np. drzwi domu) od strony -Z.

**Tekstury** BMP 24-bit (albo 32-bit z kanałem alfa), najlepiej 256, 512 albo 1024 px.

**Sprite'y potworów i NPC:** jeden plik BMP na postać, tło magenta, 4 kolumny x 2 wiersze klatek:
```
          kol. 0   kol. 1   kol. 2   kol. 3
wiersz 0: stoi     krok     atak     martwy     <- przód
wiersz 1: stoi     krok     atak     martwy     <- tył
```
Klatka 64 x 64 px (boss 128 x 128). NPC potrzebują tylko kolumn 0 i 1.

**Ikony przedmiotów:** 64 x 64 px, tło magenta. Ta sama bitmapa służy za ikonę w ekwipunku i za przedmiot na ziemi.

## Grafiki do przygotowania

- Modele OBJ z teksturą: drzewo liściaste, sosna, dom (2 wersje), skała, płot, studnia, latarnia,
  wejście do krypty, kolumna, ruina muru, koksownik, pochodnia, skrzynia.
- Sprite'y: szczur, wilk, goblin, szkielet, boss; NPC: sołtys, zielarka, strażnik.
- Ikony: sztylet, miecz, topór, zbroja skórzana, kolczuga, mała i duża mikstura, złoto, amulet.

Obecne modele są tymczasowe. Wystarczy podmienić plik o tej samej nazwie w `data/models/` albo `data/textures/`.

## Wykorzystany kod z laboratoriów

| Co | Skąd |
|---|---|
| ładowanie funkcji OpenGL (`extensionSetup`) | lab3-6 |
| wczytywanie i kompilacja shaderów z logami błędów | lab3, lab6 |
| wczytywanie OBJ i indeksowanie wierzchołków przez `std::map` (VBO + IBO) | lab3 |
| wczytywanie BMP (poprawione: wyrównanie wierszy, 32-bit, przezroczystość) | lab2 |
| materiały i światła, wzory Phonga | lab2, lab4 |
| falujące normalne (woda) | lab4 |
| `discard` w shaderze fragmentów (przezroczyste piksele) | lab5 |
| shader geometrii rozbijający trójkąty na punkty (rozpad potwora) | lab6 |
| licznik FPS w tytule okna | lab3 |
| GLM, `glext.h`, tekstura trawy | lab2, lab3-6 |

## Poza zakresem

Tryb wieloosobowy, otwarty świat, system umiejętności, pogoda, zapis gry, drużyna kilku postaci,
walka turowa, cienie, dźwięk.
