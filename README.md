Jest to aplikacja dla Telekomunikacji Kaliskiej.


Wersja: beta


## Platform builds

The desktop application is built with Qt/CMake. The Android client is a separate
native Kotlin + Jetpack Compose application under `android/`.

- **Linux:** AppImage (existing desktop build).
- **Windows:** portable ZIP (existing desktop build).
- **Android:** debug APK (native dialer UI foundation).

GitHub Actions builds Linux and Windows on pushes to `main`, pull requests,
and version tags. The Android workflow builds when Android sources change,
on pull requests touching Android, and on version tags.

### Android status

The Android app contains a Material 3 dialer, locally saved SIP account settings (password remains memory-only), a persistent call-history store, and an Android audio-routing adapter. The history UI is ready for SIP call events. SIP registration, incoming/outgoing call media, Bluetooth routing integration, and recording still require the Android PJSIP/JNI port. The desktop SIP engine remains the functional reference implementation.

### PJSIP Android build

The Android CI now builds the official PJSIP **2.15.1** source for `arm64-v8a`
using the Android NDK, then generates the official PJSUA2 SWIG Java/JNI
bindings. The generated classes and native libraries are staged as a short-lived
CI artifact and included in the debug APK build.

To reproduce locally (Linux with Android NDK, SWIG, Git and build tools):

```sh
ANDROID_NDK_ROOT="$ANDROID_HOME/ndk/27.2.12479018" \
  PJSIP_VERSION=2.15.1 ./scripts/build-pjsip-android.sh
cd android
./gradlew :app:assembleDebug
```

This is the native dependency and packaging milestone, not yet the SIP client
integration. The next step is a Kotlin service around PJSUA2 (endpoint lifecycle,
registration, call state, and media callbacks). CI currently targets arm64-v8a;
emulator x86_64 and additional device ABIs remain future work.
