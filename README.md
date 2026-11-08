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
