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

The Android app currently contains the Material 3 dialer interface, keypad,
number editing, light/dark themes, and navigation placeholders for history,
contacts, and settings. SIP registration, audio routing, incoming calls,
call history persistence, and recording are not yet implemented on Android.
The desktop SIP engine remains the functional reference implementation.
