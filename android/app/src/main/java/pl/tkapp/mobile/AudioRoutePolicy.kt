package pl.tkapp.mobile

/** A route that can be selected for an active SIP call. */
enum class CallAudioRoute(val label: String) {
    DEFAULT("Automatycznie"),
    SPEAKER("Głośnik"),
    BLUETOOTH("Bluetooth")
}

/** Pure fallback policy so route selection remains testable without Android hardware. */
object AudioRoutePolicy {
    fun fallback(
        requested: CallAudioRoute,
        available: Set<CallAudioRoute>
    ): CallAudioRoute? = when {
        requested in available -> requested
        CallAudioRoute.DEFAULT in available -> CallAudioRoute.DEFAULT
        CallAudioRoute.SPEAKER in available -> CallAudioRoute.SPEAKER
        CallAudioRoute.BLUETOOTH in available -> CallAudioRoute.BLUETOOTH
        else -> null
    }
}
