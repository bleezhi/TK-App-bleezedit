package pl.tkapp.mobile

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class AudioRoutePolicyTest {
    @Test
    fun keepsRequestedRouteWhenItIsAvailable() {
        assertEquals(
            CallAudioRoute.BLUETOOTH,
            AudioRoutePolicy.fallback(
                CallAudioRoute.BLUETOOTH,
                setOf(CallAudioRoute.DEFAULT, CallAudioRoute.SPEAKER, CallAudioRoute.BLUETOOTH)
            )
        )
    }

    @Test
    fun unavailableBluetoothFallsBackToAndroidDefault() {
        assertEquals(
            CallAudioRoute.DEFAULT,
            AudioRoutePolicy.fallback(
                CallAudioRoute.BLUETOOTH,
                setOf(CallAudioRoute.DEFAULT, CallAudioRoute.SPEAKER)
            )
        )
    }

    @Test
    fun speakerIsUsedWhenItIsTheOnlyAvailableFallback() {
        assertEquals(
            CallAudioRoute.SPEAKER,
            AudioRoutePolicy.fallback(CallAudioRoute.DEFAULT, setOf(CallAudioRoute.SPEAKER))
        )
    }

    @Test
    fun returnsNullWhenNoRouteIsAvailable() {
        assertNull(AudioRoutePolicy.fallback(CallAudioRoute.BLUETOOTH, emptySet()))
    }
}
