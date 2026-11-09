package pl.tkapp.mobile

import org.junit.Assert.assertEquals
import org.junit.Test

class SipProfileTest {
    @Test
    fun registrarMatchesTheDesktopUdpProfile() {
        assertEquals("sip:15.235.62.162:5060;transport=udp", TkSipProfile.registrarUri())
    }

    @Test
    fun authorizationUsernameDefaultsToSipUsername() {
        assertEquals("account42", TkSipProfile.authUsername(SipAccount(username = "account42")))
    }

    @Test
    fun explicitAuthorizationUsernameTakesPrecedence() {
        assertEquals(
            "digest-user",
            TkSipProfile.authUsername(SipAccount(username = "account42", authUsername = "digest-user"))
        )
    }

    @Test
    fun identityUriUsesRegistrarHostByDefault() {
        assertEquals("sip:account42@15.235.62.162", TkSipProfile.identityUri(SipAccount(username = "account42")))
    }

    @Test
    fun identityUriSupportsOptionalDomainAndDisplayName() {
        assertEquals(
            "\"TK Lab\" <sip:account42@pbx.example.net>",
            TkSipProfile.identityUri(
                SipAccount(username = "account42", displayName = "TK Lab", identityDomain = "pbx.example.net")
            )
        )
    }
}
