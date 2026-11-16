package pl.tkapp.mobile

import org.junit.Assert.assertEquals
import org.junit.Test

class CallHistoryRecordFactoryTest {
    @Test
    fun unansweredIncomingCallIsMarkedMissed() {
        val record = CallHistoryRecordFactory.create(
            peer = "sip:100@example.net",
            incoming = true,
            confirmed = false,
            startedAt = 1_700_000_000_000L,
            durationSeconds = 0L
        )

        assertEquals(CallDirection.MISSED, record.direction)
        assertEquals(0L, record.durationSeconds)
    }

    @Test
    fun answeredIncomingCallKeepsItsDirectionAndDuration() {
        val record = CallHistoryRecordFactory.create(
            peer = "sip:100@example.net",
            incoming = true,
            confirmed = true,
            startedAt = 1_700_000_000_000L,
            durationSeconds = 42L
        )

        assertEquals(CallDirection.INCOMING, record.direction)
        assertEquals(42L, record.durationSeconds)
    }

    @Test
    fun outgoingCallIsRecordedEvenWhenItWasNotAnswered() {
        val record = CallHistoryRecordFactory.create(
            peer = "sip:200@example.net",
            incoming = false,
            confirmed = false,
            startedAt = 1_700_000_000_000L,
            durationSeconds = -1L
        )

        assertEquals(CallDirection.OUTGOING, record.direction)
        assertEquals(0L, record.durationSeconds)
    }

    @Test
    fun blankPeerGetsASafeFallbackLabel() {
        val record = CallHistoryRecordFactory.create(
            peer = "",
            incoming = true,
            confirmed = false,
            startedAt = 1_700_000_000_000L,
            durationSeconds = 0L
        )

        assertEquals("Nieznany numer", record.peer)
    }
}
