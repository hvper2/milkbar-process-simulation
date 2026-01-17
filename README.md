# Symulacja Procesów w Barze (C11 + System V IPC)

Multi-procesowa symulacja baru mlecznego wykorzystująca bibliotekę standardową C11 oraz System V IPC (message queues, semafory, shared memory) na Linuxie.

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
- `CLIENT_INTERVAL` - Interwał generowania nowych klientów w sekundach (domyślnie 1s)
- `EATING_TIME` - Czas jedzenia klienta w sekundach (domyślnie 3s)
- `NO_ORDER_PROBABILITY` - Prawdopodobieństwo, że klient nie zamówi (domyślnie 5%)
- `SIGNAL1_TIME` - Moment wysłania sygnału podwojenia stolików X3 (domyślnie 10s)
- `SIGNAL2_TIME` - Moment wysłania sygnału rezerwacji stolików (domyślnie 15s)
- `SIGNAL3_TIME` - Moment wywołania pożaru/ewakuacji (domyślnie 29s)
- `RESERVED_TABLE_COUNT` - Liczba stolików do rezerwacji przez kierownika (domyślnie 2)

## Architektura projektu

### Multi-procesowy pipeline
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
- `SIGUSR1` - podwojenie stolików 3-osobowych (obsługa), synchronizacja rozpoczęcia jedzenia (klienci)
- `SIGUSR2` - rezerwacja stolików przez kierownika (obsługa), sygnał wyjścia dla procesów potomnych (klienci)
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
2. **Rezerwacja stolika**: Wysyła `MSG_TYPE_SEAT_REQUEST` → obsługa znajduje wolny stolik → odpowiedź
3. **Płatność**: Wysyła `MSG_TYPE_PAYMENT` → kasjer przetwarza → potwierdzenie
4. **Jedzenie**: Wszystkie procesy grupy synchronizują się przez `SIGUSR1` → jedzą przez `EATING_TIME` sekund
5. **Oddanie naczyń**: Wysyła `MSG_TYPE_DISHES` → obsługa zwalnia stolik → wyjście

### 3. Sygnały kierownika (kierownik.c)
- **SIGNAL1_TIME**: `SIGUSR1` → podwojenie stolików 3-osobowych (2 → 4 stoliki)
- **SIGNAL2_TIME**: `SIGUSR2` + wiadomość → rezerwacja losowych stolików (oznaczone jako -1)
- **SIGNAL3_TIME**: Pożar → ustawia `fire_alarm=1`, wysyła `SIGTERM` do wszystkich klientów, zamyka bar

## Linki do kodu - wymagane funkcje systemowe

### a. Tworzenie i obsługa plików
- `creat()` - tworzenie pliku logu: [`src/utils.c#L21`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L21)
- `open()` - otwieranie pliku logu: [`src/utils.c#L27`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L27), [`src/utils.c#L74`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L74)
- `close()` - zamykanie pliku logu: [`src/utils.c#L26`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L26), [`src/utils.c#L106`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L106)
- `write()` - zapis do pliku logu: [`src/utils.c#L91`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L91)
- `unlink()` - usuwanie starego pliku logu: [`src/utils.c#L19`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L19)

### b. Tworzenie procesów
- `fork()` - tworzenie procesów potomnych: [`src/bar.c#L19`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/bar.c#L19), [`src/bar.c#L35`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/bar.c#L35), [`src/bar.c#L96`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/bar.c#L96), [`src/klient.c#L92`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/klient.c#L92)
- `execl()` - uruchamianie programów: [`src/bar.c#L25`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/bar.c#L25), [`src/bar.c#L44`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/bar.c#L44), [`src/bar.c#L100`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/bar.c#L100)
- `exit()` - zakończenie procesu: [`src/bar.c#L27`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/bar.c#L27), [`src/utils.c#L13`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L13)
- `waitpid()` - oczekiwanie na zakończenie procesu: [`src/bar.c#L154`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/bar.c#L154), [`src/bar.c#L169-L171`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/bar.c#L169-L171), [`src/klient.c#L252`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/klient.c#L252)

### c. Obsługa sygnałów
- `kill()` - wysyłanie sygnału do procesu: [`src/kierownik.c#L35`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/kierownik.c#L35), [`src/kierownik.c#L58`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/kierownik.c#L58), [`src/klient.c#L17`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/klient.c#L17)
- `killpg()` - wysyłanie sygnału do grupy procesów: [`src/kierownik.c#L79`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/kierownik.c#L79), [`src/kierownik.c#L109`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/kierownik.c#L109)
- `signal()` - rejestracja handlera sygnału: [`src/bar.c#L60-L62`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/bar.c#L60-L62), [`src/klient.c#L75-L78`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/klient.c#L75-L78), [`src/obsluga.c#L234-L237`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/obsluga.c#L234-L237)

### d. Synchronizacja procesów (semafor)
- `semget()` - tworzenie/otwieranie semafora: [`src/utils.c#L33`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L33), [`src/utils.c#L149`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L149)
- `semctl()` - kontrola semafora (SETVAL, IPC_RMID): [`src/utils.c#L39`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L39), [`src/utils.c#L155`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L155), [`src/utils.c#L175`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L175)
- `semop()` - operacje na semaforze (wait/signal): [`src/utils.c#L82`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L82), [`src/utils.c#L100`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L100), [`src/bar.c#L135`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/bar.c#L135), [`src/obsluga.c#L19`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/obsluga.c#L19), [`src/obsluga.c#L32`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/obsluga.c#L32)

### e. Segmenty pamięci dzielonej
- `shmget()` - tworzenie/otwieranie segmentu: [`src/utils.c#L114`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L114), [`src/utils.c#L188`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L188), [`src/klient.c#L28`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/klient.c#L28)
- `shmat()` - dołączanie segmentu: [`src/utils.c#L120`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L120), [`src/utils.c#L193`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L193), [`src/klient.c#L30`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/klient.c#L30)
- `shmdt()` - odłączanie segmentu: [`src/utils.c#L131`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L131), [`src/utils.c#L202`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L202), [`src/bar.c#L164`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/bar.c#L164), [`src/obsluga.c#L463`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/obsluga.c#L463)
- `shmctl()` - kontrola segmentu (IPC_RMID): [`src/utils.c#L165`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L165)

### f. Kolejki komunikatów
- `msgget()` - tworzenie/otwieranie kolejki: [`src/utils.c#L139`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L139), [`src/utils.c#L202`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L202), [`src/kierownik.c#L46`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/kierownik.c#L46)
- `msgsnd()` - wysyłanie wiadomości: [`src/klient.c#L132`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/klient.c#L132), [`src/klient.c#L169`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/klient.c#L169), [`src/kasjer.c#L51`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/kasjer.c#L51), [`src/obsluga.c#L326`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/obsluga.c#L326)
- `msgrcv()` - odbieranie wiadomości: [`src/kasjer.c#L25`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/kasjer.c#L25), [`src/klient.c#L141`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/klient.c#L141), [`src/obsluga.c#L265`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/obsluga.c#L265)
- `msgctl()` - kontrola kolejki (IPC_RMID): [`src/utils.c#L170`](https://github.com/hvper2/milkbar-process-simulation/blob/main/src/utils.c#L170)

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

1. Ustawić małą liczbę stolików (np. tylko stoliki 4-osobowe).
2. Wprowadzić grupy 2-osobowe lub 1-osobowe (zajmuje połowę stolika).
3. Wprowadzić grupę 3-osobową (nie powinna usiąść przy tym stoliku).
4. Wprowadzić kolejną grupę 2-osobową (powinna się dosiąść).

**Oczekiwany wynik:** grupy zajmują miejsca optymalnie, ale zgodnie z zakazem łączenia grup o różnej liczebności.

**Weryfikacja:** Sprawdzić w logach (`logs/symulacja.log`) lub wizualizacji, że grupy 2-osobowe dosiadają się do stolików 4-osobowych, a grupa 3-osobowa otrzymuje odpowiedź "BRAK MIEJSCA" lub zostaje przydzielona do innego stolika.

---

### Test 2 – Sygnał 1 (Dostawienie stolików X3)

**Cel:** sprawdzenie dynamicznej zmiany konfiguracji sali.

**Przebieg:**

1. Ustawić tylko stoliki 3-osobowe (X3).
2. Doprowadzić do zapełnienia wszystkich stolików 3-osobowych (X3).
3. Wygenerować kolejne grupy klientów oczekujących na miejsce.
4. Wysłać **Sygnał 1** od kierownika (ustawić `SIGNAL1_TIME` w `common.h`).
5. Ponowić próbę wysłania Sygnału 1 (w kodzie kierownika jest test jednostkowy).

**Oczekiwany wynik:**

- po pierwszym sygnale liczba miejsc X3 podwaja się, kolejka maleje,
- drugi sygnał jest ignorowany (operacja jednorazowa).

**Weryfikacja:** 
- W logach pojawi się komunikat: `"OBSLUGA: X3 podwojone: 2 -> 4 stolików (+6 miejsc)"`
- Po drugim sygnale: `"KIEROWNIK: [TEST] Stoliki 3-osobowe NIE zostały podwojone ponownie!"`
- Sprawdzić w pamięci współdzielonej, że `x3_doubled == 1` i `effective_x3 == 4`

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

**Cel:** sprawdzenie poprawnego czyszczenia zasobów w sytuacji awaryjnej.

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
   ps aux | grep -E "(bar|kasjer|obsluga|klient|kierownik)" | grep -v grep
   ```
   Powinno zwrócić puste (wszystkie procesy zakończone).

3. **Sprawdzenie zasobów IPC:**
   ```bash
   ipcs -m  # Pamięć współdzielona
   ipcs -q  # Kolejki komunikatów
   ipcs -s  # Semafory
   ```
   Wszystkie zasoby powinny być zwolnione (brak obiektów z kluczem `0x12345678`).

4. **Sprawdzenie procesów zombie:**
   ```bash
   ps aux | grep "Z" | grep -v grep
   ```
   Brak procesów zombie.

---

### Uruchamianie testów

Aby uruchomić testy, zmodyfikuj parametry w `include/common.h`:

- **Test 1:** Ustaw `X1=0, X2=0, X3=0, X4=2` (tylko stoliki 4-osobowe)
- **Test 2:** Ustaw `X1=0, X2=0, X3=2, X4=0` i `SIGNAL1_TIME=5`
- **Test 3:** Ustaw `SIGNAL2_TIME=10` i `RESERVED_TABLE_COUNT=2`
- **Test 4:** Ustaw `SIGNAL3_TIME=15` i zwiększ `MAX_CLIENTS` aby zapełnić bar

Następnie uruchom symulację:
```bash
make clean && make
./bin/bar
```

Sprawdź wyniki w `logs/symulacja.log` i za pomocą narzędzi systemowych (`ipcs`, `ps`).
