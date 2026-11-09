package pl.tkapp.mobile

import android.content.Context

object TkSipProfile {
    const val server = "15.235.62.162"
    const val port = 5060
    const val transport = "udp"

    fun registrarUri(): String = "sip:$server:$port;transport=$transport"

    fun authUsername(account: SipAccount): String =
        account.authUsername.trim().ifEmpty { account.username.trim() }

    fun identityUri(account: SipAccount): String {
        val username = account.username.trim()
        val domain = account.identityDomain.trim().ifBlank { server }
        val displayName = account.displayName.trim()
            .replace("\\", "\\\\")
            .replace("\"", "\\\"")
        return if (displayName.isEmpty()) {
            "sip:$username@$domain"
        } else {
            "\"$displayName\" <sip:$username@$domain>"
        }
    }
}

data class SipAccount(
    val username: String = "",
    val authUsername: String = "",
    val password: String = "",
    val displayName: String = "TK App",
    val identityDomain: String = ""
)

class SipAccountStore(context: Context) {
    private val prefs = context.getSharedPreferences("sip_account", Context.MODE_PRIVATE)

    fun load(): SipAccount {
        val savedIdentityDomain = prefs.getString("identityDomain", null)
        val legacyDomain = prefs.getString("domain", "").orEmpty()
        val identityDomain = savedIdentityDomain
            ?: legacyDomain.takeUnless { it.isBlank() || it == TkSipProfile.server }.orEmpty()

        return SipAccount(
            username = prefs.getString("username", "").orEmpty(),
            authUsername = prefs.getString("authUsername", "").orEmpty(),
            password = "", // SIP passwords are never persisted.
            displayName = prefs.getString("displayName", "TK App").orEmpty(),
            identityDomain = identityDomain
        )
    }

    fun save(account: SipAccount) {
        prefs.edit()
            .putString("username", account.username.trim())
            .putString("authUsername", account.authUsername.trim())
            .putString("displayName", account.displayName.trim())
            .putString("identityDomain", account.identityDomain.trim())
            .remove("domain")
            .remove("port")
            .apply()
    }

    fun clear() = prefs.edit().clear().apply()
}
