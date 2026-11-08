package pl.tkapp.mobile

import android.content.Context

data class SipAccount(
    val username: String = "",
    val password: String = "",
    val displayName: String = "",
    val domain: String = "15.235.62.162",
    val port: Int = 5060
)

class SipAccountStore(context: Context) {
    private val prefs = context.getSharedPreferences("sip_account", Context.MODE_PRIVATE)

    fun load() = SipAccount(
        username = prefs.getString("username", "").orEmpty(),
        password = prefs.getString("password", "").orEmpty(),
        displayName = prefs.getString("displayName", "").orEmpty(),
        domain = prefs.getString("domain", "15.235.62.162").orEmpty(),
        port = prefs.getInt("port", 5060)
    )

    fun save(account: SipAccount) {
        prefs.edit()
            .putString("username", account.username.trim())
            .putString("displayName", account.displayName.trim())
            .putString("domain", account.domain.trim())
            .putInt("port", account.port.coerceIn(1, 65535))
            .apply()
    }

    fun clear() = prefs.edit().clear().apply()
}
