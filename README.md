Jest to aplikacja dla Telekomunikacji Kaliskiej.

Wersja: beta

## Platform builds

The desktop application is built with Qt/CMake. The Android client is a separate
native Kotlin + Jetpack Compose application under `android/`.

- **Linux:** AppImage (existing desktop build).
- **Windows:** portable ZIP (existing desktop build).
- **Android:** debug APK with the native PJSUA2 SIP engine.

GitHub Actions builds Linux and Windows on pushes to `main`, pull requests,
and version tags. The Android workflow builds when Android sources change,
on pull requests touching Android, and on version tags.

### Android status

The Android app now includes the official PJSUA2 Java/JNI bindings and a Kotlin
`AndroidSipEngine`. The dialer UI is connected to SIP registration, outgoing
calls, incoming-call answer/reject, call-state updates, and bidirectional audio
media. During an active call, the UI exposes PJSIP microphone mute and an audio-route
selector. Android 12+ uses `AudioManager.setCommunicationDevice()` for the
speaker and connected Bluetooth headsets (including BLE); Android 8–11 use
the legacy Bluetooth SCO route where a compatible headset is available.
Android device changes update the available route choices. The app requests
the Android `RECORD_AUDIO` runtime permission before starting the SIP stack.

The Android profile follows the desktop TK profile: UDP to
`15.235.62.162:5060`, Digest authentication, an optional separate
authentication username, optional identity domain, 16 kHz mono audio, 20 ms
frames, and the desktop codec preference order. SIP passwords remain
memory-only and are never written to preferences.

**Verification boundary:** CI builds the native PJSIP dependency and packages
the Android APK, but no live registration or real-device call test has yet
verified the TK account/server path. The endpoint currently lives for the
activity lifetime; a foreground call service is still needed for reliable
background calls and lifecycle survival. Bluetooth routing has buildable platform-specific implementation, but still
needs physical-device verification with both a classic headset and a BLE
headset. Automatic call-history event persistence and recording are not
implemented yet.

### PJSIP Android build

The Android CI builds the official PJSIP **2.15.1** source for `arm64-v8a`
using the Android NDK, then generates the official PJSUA2 SWIG Java/JNI
bindings. The generated classes and native libraries are staged as a short-lived
CI artifact and included in the debug APK build when the workflow succeeds.

To reproduce locally (Linux with Android NDK, SWIG, Git and build tools):

```sh
ANDROID_NDK_ROOT="$ANDROID_HOME/ndk/27.2.12479018" \
  PJSIP_VERSION=2.15.1 ./scripts/build-pjsip-android.sh
cd android
gradle :app:assembleDebug
```

CI currently targets arm64-v8a; emulator x86_64 and additional device ABIs
remain future work.
