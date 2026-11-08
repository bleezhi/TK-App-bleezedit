package pl.tkapp.mobile

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject

enum class CallDirection { INCOMING, OUTGOING, MISSED }

data class CallRecord(
    val peer: String,
    val direction: CallDirection,
    val startedAt: Long,
    val durationSeconds: Long
)

class CallHistoryStore(context: Context) {
    private val prefs = context.getSharedPreferences("call_history", Context.MODE_PRIVATE)

    fun load(): List<CallRecord> {
        val raw = prefs.getString("records", "[]") ?: "[]"
        return runCatching {
            val array = JSONArray(raw)
            (0 until array.length()).map { i ->
                val item = array.getJSONObject(i)
                CallRecord(
                    peer = item.optString("peer"),
                    direction = runCatching { CallDirection.valueOf(item.optString("direction")) }.getOrDefault(CallDirection.MISSED),
                    startedAt = item.optLong("startedAt"),
                    durationSeconds = item.optLong("durationSeconds")
                )
            }.sortedByDescending { it.startedAt }
        }.getOrDefault(emptyList())
    }

    fun add(record: CallRecord) {
        val records = (listOf(record) + load()).take(100)
        val array = JSONArray()
        records.forEach { record ->
            array.put(JSONObject().apply {
                put("peer", record.peer)
                put("direction", record.direction.name)
                put("startedAt", record.startedAt)
                put("durationSeconds", record.durationSeconds)
            })
        }
        prefs.edit().putString("records", array.toString()).apply()
    }

    fun clear() = prefs.edit().remove("records").apply()
}
