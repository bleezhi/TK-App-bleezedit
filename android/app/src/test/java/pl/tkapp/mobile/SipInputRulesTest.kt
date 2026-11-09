package pl.tkapp.mobile

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class SipInputRulesTest {
    @Test
    fun normalizesDialStringLikeDesktopClient() {
        assertEquals("+486212345#", SipInputRules.normalizeDialNumber(" +48 (62) 123-45# "))
    }

    @Test
    fun acceptsSupportedDialCharacters() {
        assertTrue(SipInputRules.isValidDialNumber("+486212345"))
        assertTrue(SipInputRules.isValidDialNumber("*123"))
        assertTrue(SipInputRules.isValidDialNumber("#"))
    }

    @Test
    fun rejectsInvalidDialCharacters() {
        assertFalse(SipInputRules.isValidDialNumber("+"))
        assertFalse(SipInputRules.isValidDialNumber("12A3"))
        assertFalse(SipInputRules.isValidDialNumber("123,4"))
    }

    @Test
    fun validatesSipUsernameAndOptionalIdentityDomain() {
        assertTrue(SipInputRules.isValidUsername("tk.user-42"))
        assertFalse(SipInputRules.isValidUsername("user@domain"))
        assertFalse(SipInputRules.isValidUsername("two words"))
        assertTrue(SipInputRules.isValidIdentityDomain("pbx.example.net"))
        assertFalse(SipInputRules.isValidIdentityDomain("https://pbx.example.net"))
    }
}
