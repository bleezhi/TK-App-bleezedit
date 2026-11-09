# TK App

Aplikacja telefoniczna dla **Telekomunikacji Kaliskiej**. Projekt obejmuje aplikację desktopową opartą na Qt oraz natywnego klienta na Androida.

**Wersja:** beta

## Wersje aplikacji

| Platforma | Technologia | Sposób dystrybucji |
|---|---|---|
| Linux | Qt / CMake | AppImage |
| Windows | Qt / CMake | Archiwum ZIP (wersja przenośna) |
| Android | Kotlin / Jetpack Compose / PJSUA2 | APK |

Aplikacja desktopowa pozostaje główną, istniejącą implementacją. Klient Android jest rozwijany jako osobna aplikacja natywna w katalogu `android/`.

## Funkcje

### Aplikacja desktopowa

- Obsługa konta i połączeń SIP przez PJSUA.
- Połączenia przychodzące i wychodzące.
- Konfiguracja urządzeń audio.
- Nagrywanie rozmów do WAV lub OPUS.
- Tryb „Nie przeszkadzać” (DND).
- Ustawienia konta, audio i nagrywania.

### Android

- Natywny interfejs w Kotlinie i Jetpack Compose.
- Oficjalne powiązania Java/JNI biblioteki PJSUA2.
- Rejestracja konta SIP oraz obsługa połączeń wychodzących i przychodzących.
- Odbieranie i odrzucanie połączeń oraz aktualizacja stanu rozmowy.
- Wyciszanie mikrofonu przez PJSIP.
- Wybór trasy audio: automatyczna, głośnik lub podłączony zestaw Bluetooth.
- Android 12 i nowszy korzysta z `AudioManager.setCommunicationDevice()`; Android 8–11 używa starszego mechanizmu Bluetooth SCO, jeśli zestaw go obsługuje.
- Aplikacja prosi o uprawnienie `RECORD_AUDIO` przed uruchomieniem stosu SIP.

**Ważne:** kod Androida i biblioteka PJSIP są budowane w CI, a APK jest składany w ramach workflow. Nie oznacza to jeszcze potwierdzenia działania z prawdziwym kontem TK — rejestracja na serwerze i rzeczywista rozmowa nie zostały dotąd zweryfikowane w teście integracyjnym na urządzeniu.

## Konfiguracja SIP Telekomunikacji Kaliskiej

Klient korzysta z profilu sieciowego istniejącej aplikacji desktopowej:

| Parametr | Wartość |
|---|---|
| Serwer rejestracji SIP | `15.235.62.162` |
| Port | `5060` |
| Transport | UDP |
| Uwierzytelnianie | SIP Digest |
| Częstotliwość próbkowania audio | 16 kHz, mono |
| Długość ramki audio | 20 ms |

Login autoryzacyjny może być inny niż login konta. Domena tożsamości jest opcjonalna; domyślnie używany jest adres serwera. Hasło SIP nie jest zapisywane w ustawieniach aplikacji — pozostaje w pamięci procesu.

Preferencje kodeków klienta desktopowego obejmują Opus, PCMU, PCMA i G.722. Klient Android korzysta z profilu kodeków skonfigurowanego w swoim silniku PJSIP; dostępność konkretnego kodeka zależy od dołączonej implementacji.

**Bezpieczeństwo połączenia:** profil TK używa UDP, a SIP/RTP nie są szyfrowane. Korzystaj z zaufanej sieci lub VPN. Nie umieszczaj haseł ani danych logowania w zgłoszeniach, logach czy repozytorium.

## Budowanie klienta Android

CI kompiluje oficjalne źródła PJSIP w wersji **2.15.1** dla architektury `arm64-v8a`, a następnie generuje oficjalne powiązania Java/JNI PJSUA2. Wygenerowane klasy i biblioteki natywne są przekazywane jako artefakt CI i dołączane podczas budowania APK.

### Wymagania

- Linux
- Android SDK
- Android NDK w wersji `27.2.12479018`
- SWIG
- Git i standardowe narzędzia kompilacyjne
- Gradle

### Budowanie PJSIP i APK

W katalogu głównym repozytorium:

```sh
ANDROID_NDK_ROOT="$ANDROID_HOME/ndk/27.2.12479018" \
  PJSIP_VERSION=2.15.1 ./scripts/build-pjsip-android.sh

cd android
gradle :app:assembleDebug
```

Obecnie kompilowana jest architektura `arm64-v8a`. Obsługa emulatorów `x86_64` i dodatkowych architektur Androida pozostaje do zrobienia.

## Stan rozwoju i ograniczenia Androida

- **Rejestracja i rozmowy:** istnieje implementacja cyklu życia SIP, ale nie została jeszcze potwierdzona rzeczywistym połączeniem z kontem TK.
- **Praca w tle:** silnik PJSUA2 jest obecnie powiązany z cyklem życia aktywności. Do niezawodnej obsługi rozmów po przejściu aplikacji w tło potrzebna jest usługa pierwszoplanowa (Foreground Service).
- **Bluetooth:** obsługa tras audio jest zaimplementowana na poziomie platformy, ale wymaga testów na fizycznych urządzeniach — zarówno z zestawami klasycznymi, jak i BLE.
- **Historia połączeń:** automatyczny zapis zdarzeń historii połączeń nie jest jeszcze zaimplementowany.
- **Nagrywanie na Androidzie:** nie jest jeszcze zaimplementowane.
- **G.726:** nie jest obecnie włączony w zestawie kodeków Androida; wymaga znalezienia i zweryfikowania zgodnej implementacji.

## Budowanie aplikacji desktopowej

Aplikacja desktopowa korzysta z Qt i CMake. Szczegóły konfiguracji oraz zależności znajdują się w pliku `CMakeLists.txt` i katalogu `packaging/`. Nie trzeba budować klienta Androida, aby rozwijać lub uruchamiać aplikację desktopową.

## Automatyczne budowanie (GitHub Actions)

GitHub Actions automatycznie buduje wersje Linux i Windows po zmianach w gałęzi `main`, dla pull requestów oraz tagów wersji. Workflow Androida uruchamia się po zmianach w kodzie Androida, dla pull requestów obejmujących ten kod oraz dla tagów wersji.

Gotowe pliki można znaleźć w sekcji **Actions → wybrane uruchomienie workflow → Artifacts**.

## Licencja

Informacje o licencji znajdują się w pliku [LICENSE](LICENSE).
