# Symulacja Procesów w Barze (C11 + System V IPC)

Multi-procesowa symulacja baru mlecznego wykorzystująca bibliotekę standardową C11 oraz System V IPC (message queues, semafory, shared memory) na Linuxie.

## Wymagania systemowe

**Środowisko:**
- System operacyjny: Linux
- Środowisko testowe: serwer Torus (Debian)

**Kompilator:**
- Kompilator: `gcc` (GNU Compiler Collection)
- Standard języka: C11 (`-std=c11`)

**Wymagane narzędzia:**
- `make` - do zarządzania kompilacją
- Standardowe narzędzia systemowe Linux 

## Build & Run

```bash
# Kompilacja
make clean && make

# Uruchomienie symulacji
./bin/bar

# Uruchomienie wizualizacji (w osobnym terminalu)
cd visualization
make
./viz
```

## Parametry konfiguracyjne (include/common.h)

- `X1`, `X2`, `X3`, `X4` - Liczba stolików każdego typu (1-osobowe, 2-osobowe, 3-osobowe, 4-osobowe)
- `SIMULATION_TIME` - Czas trwania symulacji w sekundach (domyślnie 30s)
- `TOTAL_CLIENTS` - Całkowita liczba grup klientów do wygenerowania (domyślnie 30)
- `EATING_TIME` - Czas jedzenia klienta w sekundach (domyślnie 3s)
- `NO_ORDER_PROBABILITY` - Prawdopodobieństwo, że klient nie zamówi (domyślnie 5%)
- `SIGNAL1_TIME` - Moment wysłania sygnału podwojenia stolików X3 (domyślnie 10s)
- `SIGNAL2_TIME` - Moment wysłania sygnału rezerwacji stolików (domyślnie 3s)
- `SIGNAL3_TIME` - Moment wywołania pożaru/ewakuacji (domyślnie 29s)
- `RESERVED_TABLE_COUNT` - Liczba stolików do rezerwacji przez kierownika (domyślnie 2)

## Architektura projektu

Projekt wykorzystuje `fork()` + `exec()` dla każdej roli:
- **bar** (proces główny) - inicjalizuje IPC, generuje klientów, zarządza procesami
- **kasjer** - przetwarza płatności od klientów
- **obsluga** - zarządza rezerwacją stolików, obsługuje sygnały kierownika
- **klient** - symuluje grupę klientów (1-3 osoby), każda grupa może mieć wiele procesów
- **kierownik** - wysyła sygnały w określonych momentach (podwojenie stolików, rezerwacja, pożar)

### System V IPC
- **Kolejki komunikatów** (`msgget`/`msgsnd`/`msgrcv`/`msgctl`): komunikacja między procesami (rezerwacja stolików, płatności, oddawanie naczyń)
- **Pamięć współdzielona** (`shmget`/`shmat`/`shmdt`/`shmctl`): stan sali (stoliki, liczba wolnych miejsc, flaga pożaru)
- **Semafor** (`semget`/`semop`/`semctl`): mutex do synchronizacji dostępu do pamięci współdzielonej oraz synchronizacja zapisu do logu

### Obsługa sygnałów
- `SIGUSR1` - podwojenie stolików 3-osobowych (obsługa)
- `SIGUSR2` - rezerwacja stolików przez kierownika (obsługa)
- `SIGTERM` - pożar/ewakuacja (wszystkie procesy), zakończenie pracy
- `SIGINT` - przerwanie symulacji (proces główny)

### Solidność implementacji
- Walidacja wejścia, sprawdzanie błędów po każdym wywołaniu systemowym (`errno`)
- Minimalne uprawnienia na obiektach IPC (`0600`)
- Czyszczenie zasobów IPC przy zakończeniu (`IPC_RMID`, `semctl(IPC_RMID)`, `shmctl(IPC_RMID)`)
- Grupowanie procesów klientów (PGID) dla łatwej masowej ewakuacji

### Wizualizacja
- Dedykowany proces `viz` odczytuje stan z pamięci współdzielonej
- Wyświetla aktualny stan stolików w czasie rzeczywistym
- Odświeżanie co 1 sekundę

## End-to-end workflow

### 1. Inicjalizacja (bar.c)
Proces główny tworzy wszystkie zasoby IPC i uruchamia procesy pracowników:
- Logger: tworzy plik logu i semafor synchronizujący zapis
- Pamięć współdzielona: struktura `SharedState` z początkowym stanem stolików
- Kolejka komunikatów: komunikacja między procesami
- Semafor: mutex dla pamięci współdzielonej

### 2. Cykl życia klienta (klient.c)
1. **Wejście**: Klient wchodzi do baru (może być grupa 1-3 osoby)
2. **Tworzenie wątków**: Dla grup wieloosobowych (2-3 osoby) tworzone są wątki pthread dla każdego członka grupy
3. **Rezerwacja stolika**: Wysyła `MSG_TYPE_SEAT_REQUEST` → obsługa znajduje wolny stolik → odpowiedź
4. **Płatność**: Wysyła `MSG_TYPE_PAYMENT` → kasjer przetwarza → potwierdzenie
5. **Jedzenie**: Wszystkie wątki grupy synchronizują się przez zmienną `can_start_eating` → jedzą przez `EATING_TIME` sekund
6. **Oddanie naczyń**: Wysyła `MSG_TYPE_DISHES` → obsługa zwalnia stolik → wyjście

### 3. Sygnały kierownika (kierownik.c)
- **SIGNAL1_TIME**: `SIGUSR1` → podwojenie stolików 3-osobowych (2 → 4 stoliki)
- **SIGNAL2_TIME**: `SIGUSR2` + wiadomość `MSG_TYPE_RESERVE_SEATS` → rezerwacja losowych stolików (oznaczone jako -1)
- **SIGNAL3_TIME**: Pożar → ustawia `fire_alarm=1`, wysyła `SIGTERM` do wszystkich klientów (przez `killpg`), zamyka bar

## Linki do kodu - wymagane funkcje systemowe

### a. Tworzenie i obsługa plików
- `creat()` - tworzenie pliku logu
- `open()` - otwieranie pliku logu
- `close()` - zamykanie pliku logu
- `write()` - zapis do pliku logu
- `unlink()` - usuwanie starego pliku logu

### b. Tworzenie procesów
- `fork()` - tworzenie procesów potomnych
- `execl()` - uruchamianie programów
- `setpgid()` - grupowanie procesów klientów (PGID)
- `exit()` - zakończenie procesu
- `waitpid()` - oczekiwanie na zakończenie procesu

### c. Obsługa sygnałów
- `kill()` - wysyłanie sygnału do procesu
- `killpg()` - wysyłanie sygnału do grupy procesów
- `signal()` - rejestracja handlera sygnału

### d. Synchronizacja procesów (semafor)
- `semget()` - tworzenie/otwieranie semafora
- `semctl()` - kontrola semafora (SETVAL, IPC_RMID)
- `semop()` - operacje na semaforze (wait/signal)

### e. Segmenty pamięci dzielonej
- `shmget()` - tworzenie/otwieranie segmentu
- `shmat()` - dołączanie segmentu
- `shmdt()` - odłączanie segmentu
- `shmctl()` - kontrola segmentu (IPC_RMID)

### f. Kolejki komunikatów
- `msgget()` - tworzenie/otwieranie kolejki
- `msgsnd()` - wysyłanie wiadomości
- `msgrcv()` - odbieranie wiadomości
- `msgctl()` - kontrola kolejki (IPC_RMID)

## Struktura projektu

```
milkbar-process-simulation/
├── src/
│   ├── bar.c          # Proces główny - inicjalizacja, generowanie klientów
│   ├── kasjer.c       # Proces kasjera - przetwarzanie płatności
│   ├── obsluga.c      # Proces obsługi - zarządzanie stolikami
│   ├── klient.c       # Proces klienta - symulacja grupy klientów
│   ├── kierownik.c    # Proces kierownika - wysyłanie sygnałów
│   └── utils.c        # Funkcje pomocnicze (IPC, logger)
├── include/
│   ├── common.h       # Definicje, struktury, stałe
│   └── utils.h        # Deklaracje funkcji pomocniczych
├── visualization/
│   ├── viz.c          # Proces wizualizacji stanu sali
│   └── Makefile
├── logs/              # Pliki logów (symulacja.log)
├── bin/               # Skompilowane programy
├── obj/               # Pliki obiektowe
└── Makefile
```

## Logi

Wszystkie zdarzenia są logowane do pliku `logs/symulacja.log` w formacie:
```
[HH:MM:SS] [PID:xxxxx] Wiadomość
```

Logi są synchronizowane przez semafor, aby uniknąć konfliktów przy równoczesnym zapisie z wielu procesów.

## Testy i weryfikacja

### Test 1 – Limit miejsc i zasada dosiadania się

**Cel:** weryfikacja poprawności zajmowania miejsc zgodnie z regułą równoliczności grup.

**Przebieg:**

1. Ustawić małą liczbę stolików (np. tylko stoliki 4-osobowe: `X1=0, X2=0, X3=0, X4=2`).
2. Uruchomić symulację z losowymi grupami klientów (1-3 osoby).
3. Obserwować zachowanie systemu podczas zapełniania stolików.

**Oczekiwany wynik:** grupy zajmują miejsca optymalnie, ale zgodnie z zakazem łączenia grup o różnej liczebności.

**Weryfikacja:** 
Sprawdzić w logach (`logs/symulacja.log`):
- Wystąpienia: `"OBSLUGA: Stolik 4-os.[X] -> grupa #Y"` - sprawdzić, czy przy tym samym stoliku są grupy o tym samym rozmiarze.
- Wystąpienia: `"OBSLUGA: Grupa #X (Y os.) czeka w kolejce"` lub odpowiedzi "BRAK MIEJSCA" dla grup x-osobowych, gdy stoliki są częściowo zajęte przez mniejsze grupy.

---

### Test 2 – Sygnał 1 (Dostawienie stolików X3)

**Cel:** sprawdzenie dynamicznej zmiany konfiguracji sali.

**Przebieg:**

1. Doprowadzić do zapełnienia wszystkich stolików 3-osobowych (X3).
2. Wygenerować kolejne grupy klientów oczekujących na miejsce.
3. Wysłać **Sygnał 1** od kierownika (ustawić `SIGNAL1_TIME` w `common.h`).

**Oczekiwany wynik:**

- po sygnale liczba miejsc X3 podwaja się, kolejka maleje,
- po 3 sekundach kierownik automatycznie próbuje wysłać sygnał 1 ponownie,
- próba ponownego podwojenia jest blokowana przez flagę `x3_doubled` w pamięci współdzielonej,
- w logach pojawia się komunikat o nieudanej próbie.

**Weryfikacja:** 
- W logach pojawi się komunikat: `"OBSLUGA: X3 podwojone: 2 -> 4 stolików (+6 miejsc)"`
- Po 3 sekundach: `"KIEROWNIK: >>> Próba ponownego wysłania SYGNAŁU 1 (SIGUSR1) po 3 sekundach"`
- Następnie: `"OBSLUGA: SYGNAŁ 1 (SIGUSR1) otrzymany ponownie - operacja NIEMOŻLIWA (stoliki 3-osobowe już zostały podwojone)"`

---

### Test 3 – Sygnał 2 (Rezerwacja stolików)

**Cel:** weryfikacja blokowania dostępu do zasobów.

**Przebieg:**

1. Uruchomić symulację z wolnymi stolikami.
2. Wysłać **Sygnał 2** nakazujący rezerwację części stolików (ustawić `SIGNAL2_TIME` w `common.h`).
3. Wprowadzić nowych klientów.

**Oczekiwany wynik:** klienci nie zajmują zarezerwowanych stolików, mimo że fizycznie są one puste.

**Weryfikacja:**
- W logach pojawi się: `"OBSLUGA: Rezerwacja kierownika: X stolików (Y miejsc) - zarezerwowane do końca symulacji"`
- Nowi klienci otrzymują odpowiedź "BRAK MIEJSCA" lub są przydzielani tylko do niezarezerwowanych stolików
- W pamięci współdzielonej zarezerwowane stoliki mają wartość `-1` w odpowiednich tablicach `table_X[]`
- Funkcja `find_free_table()` w `obsluga.c` pomija stoliki z wartością `-1`

---

### Test 4 – Sygnał 3 (Pożar/Ewakuacja)

**Cel:** sprawdzenie poprawnego czyszczenia zasobów w sytuacji awaryjnej i po zakończeniu symulacji.

**Przebieg:**

1. Doprowadzić do pełnego obłożenia baru (klienci jedzą, stoją w kolejce).
2. Wysłać **Sygnał 3** (pożar) - ustawić `SIGNAL3_TIME` w `common.h`.

**Oczekiwany wynik:**

- klienci natychmiast kończą procesy,
- pracownicy zamykają kasę i kończą pracę,
- brak procesów zombie i "wiszących" semaforów w systemie (`ipcs`).

**Weryfikacja:**

1. **Sprawdzenie logów:**
   - `"KIEROWNIK: >>> SYGNAŁ 3 (POŻAR) - ewakuacja wszystkich klientów"`
   - `"KLIENT #X: POŻAR! Przerwano jedzenie - ewakuacja"` (dla każdego klienta)
   - `"OBSLUGA: Pracownicy kończą pracę (pożar)"`
   - `"KASJER: Kasjer kończy pracę"`

2. **Sprawdzenie procesów:**
   ```bash
   ps -u $(whoami) f
   ```
   Powinno zwrócić puste (wszystkie procesy zakończone).
   Brak procesów zombie.

3. **Sprawdzenie zasobów IPC:**
   ```bash
   ipcs -m  # Pamięć współdzielona
   ipcs -q  # Kolejki komunikatów
   ipcs -s  # Semafory
   ```
   Wszystkie zasoby powinny być zwolnione (brak obiektów z kluczem `0x00001010`).

---

### Uruchamianie testów

Aby uruchomić testy, zmodyfikuj parametry w `include/common.h`:

- **Test 1:** Ustaw `X1=0, X2=0, X3=0, X4=2` (tylko stoliki 4-osobowe)
- **Test 2:** Ustaw `X1=0, X2=0, X3=2, X4=0` i `SIGNAL1_TIME=5`
- **Test 3:** Ustaw `SIGNAL2_TIME=10` i `RESERVED_TABLE_COUNT=2`
- **Test 4:** Ustaw `SIGNAL3_TIME=15` i zwiększ `TOTAL_CLIENTS` aby zapełnić bar

Następnie uruchom symulację:
```bash
make clean && make
./bin/bar
```

Sprawdź wyniki w `logs/symulacja.log` i za pomocą narzędzi systemowych (`ipcs`, `ps`).
