package pl.tkapp.mobile

/** Pure SIP input validation shared by the Android engine and JVM unit tests. */
object SipInputRules {
    private val usernamePattern = Regex("^[A-Za-z0-9_.!~*'()+%\\-]{1,128}$")
    private val identityDomainPattern = Regex("^[A-Za-z0-9.\\-]+$")
    private val dialNumberPattern = Regex("^\\+?[0-9*#]{1,64}$")
    private val separators = Regex("""[\\s()\\-]""")

    fun isValidUsername(value: String): Boolean = usernamePattern.matches(value)

    fun isValidIdentityDomain(value: String): Boolean =
        identityDomainPattern.matches(value)

    fun normalizeDialNumber(value: String): String = value.trim().replace(separators, "")

    fun isValidDialNumber(value: String): Boolean = dialNumberPattern.matches(value)
}
