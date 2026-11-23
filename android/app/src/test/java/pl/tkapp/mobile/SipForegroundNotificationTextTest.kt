package pl.tkapp.mobile

import org.junit.Assert.assertEquals
import org.junit.Test

class SipForegroundNotificationTextTest {
    @Test
    fun incomingCallShowsPeerWithoutAccountDetails() {
        assertEquals(
            "Połączenie przychodzące: 12345",
            SipForegroundNotificationText.from(SipUiState(callPhase = "incoming", peer = "12345"))
        )
    }

    @Test
    fun connectedCallShowsActivePeer() {
        assertEquals(
            "Rozmowa: 12345",
            SipForegroundNotificationText.from(SipUiState(callPhase = "connected", peer = "12345"))
        )
    }

    @Test
    fun registeredIdleStateShowsRegistration() {
        assertEquals(
            "Konto SIP zarejestrowane",
            SipForegroundNotificationText.from(SipUiState(registered = true))
        )
    }

    @Test
    fun connectingCallShowsProgress() {
        assertEquals(
            "Łączenie: 12345",
            SipForegroundNotificationText.from(SipUiState(callPhase = "ringing", peer = "12345"))
        )
    }

    @Test
    fun disconnectedStateDoesNotPretendToBeRegistered() {
        assertEquals(
            "Rozłączono",
            SipForegroundNotificationText.from(SipUiState())
        )
    }
}
