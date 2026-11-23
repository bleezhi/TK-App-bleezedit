package pl.tkapp.mobile

/** Stable, non-sensitive text for the persistent SIP foreground notification. */
internal object SipForegroundNotificationText {
    fun from(state: SipUiState): String = when (state.callPhase) {
        "incoming" -> "Połączenie przychodzące: ${state.peer.ifBlank { "Nieznany numer" }}"
        "connected" -> "Rozmowa: ${state.peer.ifBlank { "Nieznany numer" }}"
        "connecting", "ringing" -> "Łączenie: ${state.peer.ifBlank { "Nieznany numer" }}"
        else -> if (state.registered) "Konto SIP zarejestrowane" else state.registrationText
    }
}
