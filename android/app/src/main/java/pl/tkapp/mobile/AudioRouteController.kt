package pl.tkapp.mobile

import android.content.Context
import android.media.AudioDeviceCallback
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.os.Build
import android.os.Handler
import android.os.Looper

data class AudioRouteSnapshot(
    val route: CallAudioRoute,
    val availableRoutes: Set<CallAudioRoute>
)

/**
 * Owns Android's communication-device routing for SIP calls.
 *
 * Android 12+ uses the communication-device API (including BLE headsets).
 * Older releases use the legacy speakerphone / Bluetooth SCO controls.
 * The normal route is left to Android so wired headsets continue to work.
 */
class AudioRouteController(context: Context) {
    private val audio = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    private val mainHandler = Handler(Looper.getMainLooper())

    private var listener: ((AudioRouteSnapshot) -> Unit)? = null
    private var callActive = false
    private var closed = false
    private var selectedRoute = CallAudioRoute.DEFAULT
    private var previousMode = AudioManager.MODE_NORMAL
    private var previousSpeakerphone = false
    private var previousBluetoothSco = false

    private val deviceCallback = object : AudioDeviceCallback() {
        override fun onAudioDevicesAdded(addedDevices: Array<out AudioDeviceInfo>) = refreshRoutes()
        override fun onAudioDevicesRemoved(removedDevices: Array<out AudioDeviceInfo>) = refreshRoutes()
    }

    init {
        audio.registerAudioDeviceCallback(deviceCallback, mainHandler)
    }

    fun setListener(value: ((AudioRouteSnapshot) -> Unit)?) {
        listener = value
        publish()
    }

    fun beginCall() {
        check(!closed) { "Sterownik audio został zamknięty." }
        if (!callActive) {
            @Suppress("DEPRECATION")
            run {
                previousMode = audio.mode
                previousSpeakerphone = audio.isSpeakerphoneOn
                previousBluetoothSco = audio.isBluetoothScoOn
                audio.mode = AudioManager.MODE_IN_COMMUNICATION
            }
            callActive = true
            selectedRoute = CallAudioRoute.DEFAULT
            applyRoute(CallAudioRoute.DEFAULT)
        }
        publish()
    }

    fun selectRoute(route: CallAudioRoute) {
        check(!closed) { "Sterownik audio został zamknięty." }
        check(callActive) { "Trasa audio może być zmieniana tylko podczas rozmowy." }
        val available = availableRoutes()
        check(route in available) {
            if (route == CallAudioRoute.BLUETOOTH) "Nie wykryto dostępnego zestawu Bluetooth."
            else "Wybrana trasa audio nie jest dostępna."
        }
        applyRoute(route)
        selectedRoute = route
        publish()
    }

    fun setSpeakerEnabled(enabled: Boolean) {
        selectRoute(if (enabled) CallAudioRoute.SPEAKER else CallAudioRoute.DEFAULT)
    }

    fun endCall() {
        if (!callActive) {
            publish()
            return
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            audio.clearCommunicationDevice()
        } else {
            @Suppress("DEPRECATION")
            run {
                if (previousBluetoothSco) {
                    audio.startBluetoothSco()
                    audio.isBluetoothScoOn = true
                } else {
                    audio.isBluetoothScoOn = false
                    audio.stopBluetoothSco()
                }
                audio.isSpeakerphoneOn = previousSpeakerphone
            }
        }

        @Suppress("DEPRECATION")
        audio.mode = previousMode
        callActive = false
        selectedRoute = CallAudioRoute.DEFAULT
        publish()
    }

    fun close() {
        if (closed) return
        endCall()
        audio.unregisterAudioDeviceCallback(deviceCallback)
        listener = null
        closed = true
    }

    private fun applyRoute(route: CallAudioRoute) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            when (route) {
                CallAudioRoute.DEFAULT -> audio.clearCommunicationDevice()
                CallAudioRoute.SPEAKER,
                CallAudioRoute.BLUETOOTH -> {
                    val device = findCommunicationDevice(route)
                        ?: throw IllegalStateException(
                            if (route == CallAudioRoute.BLUETOOTH) {
                                "Nie wykryto dostępnego zestawu Bluetooth."
                            } else {
                                "Głośnik nie jest dostępny."
                            }
                        )
                    if (!audio.setCommunicationDevice(device)) {
                        throw IllegalStateException("Android nie przyjął wybranej trasy audio.")
                    }
                }
            }
        } else {
            @Suppress("DEPRECATION")
            when (route) {
                CallAudioRoute.DEFAULT -> {
                    audio.isBluetoothScoOn = false
                    audio.stopBluetoothSco()
                    audio.isSpeakerphoneOn = false
                }
                CallAudioRoute.SPEAKER -> {
                    audio.isBluetoothScoOn = false
                    audio.stopBluetoothSco()
                    audio.isSpeakerphoneOn = true
                }
                CallAudioRoute.BLUETOOTH -> {
                    audio.isSpeakerphoneOn = false
                    audio.startBluetoothSco()
                    audio.isBluetoothScoOn = true
                }
            }
        }
    }

    private fun availableRoutes(): Set<CallAudioRoute> {
        val routes = mutableSetOf(CallAudioRoute.DEFAULT)
        val devices = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            audio.availableCommunicationDevices
        } else {
            audio.getDevices(AudioManager.GET_DEVICES_OUTPUTS).toList()
        }
        if (devices.any { it.type == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER }) {
            routes += CallAudioRoute.SPEAKER
        }
        if (devices.any { isBluetoothCommunicationDevice(it.type) }) {
            routes += CallAudioRoute.BLUETOOTH
        }
        return routes
    }

    private fun findCommunicationDevice(route: CallAudioRoute): AudioDeviceInfo? {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) return null
        return audio.availableCommunicationDevices.firstOrNull { device ->
            when (route) {
                CallAudioRoute.SPEAKER -> device.type == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER
                CallAudioRoute.BLUETOOTH -> isBluetoothCommunicationDevice(device.type)
                CallAudioRoute.DEFAULT -> false
            }
        }
    }

    private fun isBluetoothCommunicationDevice(type: Int): Boolean =
        type == AudioDeviceInfo.TYPE_BLUETOOTH_SCO ||
            (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S &&
                type == AudioDeviceInfo.TYPE_BLE_HEADSET) ||
            (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P &&
                type == AudioDeviceInfo.TYPE_HEARING_AID)

    private fun currentRoute(): CallAudioRoute {
        if (!callActive) return CallAudioRoute.DEFAULT
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val device = audio.communicationDevice ?: return CallAudioRoute.DEFAULT
            return when {
                device.type == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER -> CallAudioRoute.SPEAKER
                isBluetoothCommunicationDevice(device.type) -> CallAudioRoute.BLUETOOTH
                else -> CallAudioRoute.DEFAULT
            }
        }
        @Suppress("DEPRECATION")
        return when {
            audio.isBluetoothScoOn -> CallAudioRoute.BLUETOOTH
            audio.isSpeakerphoneOn -> CallAudioRoute.SPEAKER
            else -> CallAudioRoute.DEFAULT
        }
    }

    private fun refreshRoutes() {
        if (callActive) {
            val available = availableRoutes()
            val fallback = AudioRoutePolicy.fallback(selectedRoute, available)
            if (fallback != null && fallback != selectedRoute) {
                try {
                    applyRoute(fallback)
                    selectedRoute = fallback
                } catch (_: Exception) {
                    selectedRoute = CallAudioRoute.DEFAULT
                }
            }
        }
        publish()
    }

    private fun publish() {
        listener?.invoke(AudioRouteSnapshot(currentRoute(), availableRoutes()))
    }
}
