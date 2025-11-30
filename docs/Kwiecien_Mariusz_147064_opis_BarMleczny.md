# Temat 14 – Bar mleczny

## Opis projektu

Projekt polega na symulacji funkcjonowania baru mlecznego, składającego się z sali jadalnej ze stolikami o różnej pojemności, stanowiska kasowego, pracownika obsługi wydającego posiłki oraz kierownika. Symulacja zostanie zrealizowana w środowisku Linux przy użyciu procesów (`fork()`, `exec()`) oraz mechanizmów IPC: semaforów, pamięci dzielonej (do przechowywania stanu stolików) i kolejek komunikatów.

## Założenia funkcjonalne

- W barze znajdują się stoliki: **X1 (1-os.), X2 (2-os.), X3 (3-os.), X4 (4-os.)**.
- Maksymalna liczba klientów jedzących posiłek wynosi **N** (suma miejsc przy stolikach).
- Klienci przychodzą losowo, pojedynczo lub w grupach (2, 3-osobowych).
- **Zasada obsługi:**
  - Około 5% klientów rezygnuje przed złożeniem zamówienia.
  - Pozostali płacą w kasie, odbierają zamówienie i dopiero wtedy szukają miejsca.
  - **Zasada dosiadania:** Różne grupy nie mogą siedzieć przy tym samym stoliku, chyba że są **równoliczne** (np. dwie grupy 2-osobowe przy stoliku 4-osobowym).
- Po jedzeniu **jedna losowo wybrana osoba z grupy** odnosi naczynia za całą grupę do miejsca zwrotu.
- Kierownik może wysłać trzy sygnały sterujące:
  - **Sygnał 1 – dostawienie stolików:** podwojenie liczby stolików 3-osobowych (X3). Możliwe tylko raz w trakcie działania.
  - **Sygnał 2 – rezerwacja:** wyłączenie z użytku określonej liczby miejsc (ustalanej z pracownikiem).
  - **Sygnał 3 – pożar:** klienci natychmiast opuszczają bar (bez odnoszenia naczyń), pracownicy kończą pracę.

## Procesy w systemie

W systemie funkcjonują procesy reprezentujące:

- kierownika (zarządzanie sygnałami),
- pracownika obsługi (wydawanie dań, zarządzanie stolikami),
- kasjera (obsługa płatności),
- klientów (generowani jako grupy, procesy potomne).

---

## Konfiguracja domyślna

- **X1 (stoliki 1-osobowe):** 2 sztuki = 2 miejsca
- **X2 (stoliki 2-osobowe):** 2 sztuki = 4 miejsca
- **X3 (stoliki 3-osobowe):** 2 sztuki = 6 miejsc (można podwoić Sygnałem 1)
- **X4 (stoliki 4-osobowe):** 2 sztuki = 8 miejsc
- **Suma N =** 20 miejsc

# Testy funkcjonalne

Poniżej znajdują się testy wymagane do weryfikacji poprawności działania symulacji.

---

## Test 1 – Limit miejsc i zasada dosiadania się

**Cel:** weryfikacja poprawności zajmowania miejsc zgodnie z regułą równoliczności grup.
**Przebieg:**

1. Ustawić małą liczbę stolików (np. tylko stoliki 4-osobowe).
2. Wprowadzić grupę 2-osobową (zajmuje połowę stolika).
3. Wprowadzić grupę 3-osobową (nie powinna usiąść przy tym stoliku).
4. Wprowadzić kolejną grupę 2-osobową (powinna się dosiąść).

**Oczekiwany wynik:** grupy zajmują miejsca optymalnie, ale zgodnie z zakazem łączenia grup o różnej liczebności.

---

## Test 2 – Sygnał 1 (Dostawienie stolików X3)

**Cel:** sprawdzenie dynamicznej zmiany konfiguracji sali.
**Przebieg:**

1. Doprowadzić do zapełnienia wszystkich stolików 3-osobowych (X3).
2. Wygenerować kolejkę grup 3-osobowych oczekujących na miejsce.
3. Wysłać **Sygnał 1** od kierownika.
4. Ponowić próbę wysłania Sygnału 1.

**Oczekiwany wynik:**

- po pierwszym sygnale liczba miejsc X3 podwaja się, kolejka maleje,
- drugi sygnał jest ignorowany (operacja jednorazowa).

---

## Test 3 – Sygnał 2 (Rezerwacja stolików)

**Cel:** weryfikacja blokowania dostępu do zasobów.
**Przebieg:**

1. Uruchomić symulację z wolnymi stolikami.
2. Wysłać **Sygnał 2** nakazujący rezerwację części stolików.
3. Wprowadzić nowych klientów.

**Oczekiwany wynik:** klienci nie zajmują zarezerwowanych stolików, mimo że fizycznie są one puste (zmniejszona pula dostępnych miejsc w pamięci dzielonej).

---

## Test 4 – Sygnał 3 (Pożar/Ewakuacja)

**Cel:** sprawdzenie poprawnego czyszczenia zasobów w sytuacji awaryjnej.
**Przebieg:**

1. Doprowadzić do pełnego obłożenia baru (klienci jedzą, stoją w kolejce).
2. Wysłać **Sygnał 3** (pożar).

**Oczekiwany wynik:**

- klienci natychmiast kończą procesy (log: "Ucieczka bez zwrotu naczyń"),
- pracownicy zamykają kasę i kończą pracę,
- brak procesów zombie i "wiszących" semaforów w systemie (`ipcs`).

---

# Link do repozytorium GitHub

https://github.com/hvper2/milkbar-process-simulation
