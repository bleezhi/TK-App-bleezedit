package pl.tkapp.mobile

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Binder
import android.os.Build
import android.os.IBinder

/**
 * Keeps the PJSUA2 endpoint alive independently of the Compose activity.
 *
 * The service is promoted to a foreground service only after the user starts
 * SIP from the visible UI. It remains alive when the activity is recreated or
 * moved to the background, and owns the audio route controller and call history
 * callbacks for the lifetime of the SIP stack.
 */
class SipForegroundService : Service() {
    inner class LocalBinder : Binder() {
        val service: SipForegroundService
            get() = this@SipForegroundService
    }

    private val binder = LocalBinder()
    private val stateListeners = linkedSetOf<(SipUiState) -> Unit>()
    private val historyListeners = linkedSetOf<() -> Unit>()
    private lateinit var engine: AndroidSipEngine
    private lateinit var historyStore: CallHistoryStore
    private var foregroundStarted = false

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
        historyStore = CallHistoryStore(applicationContext)
        engine = AndroidSipEngine(AudioRouteController(applicationContext))
        engine.setListener { state ->
            if (foregroundStarted) {
                getSystemService(NotificationManager::class.java)
                    .notify(NOTIFICATION_ID, buildNotification(state))
            }
            stateListeners.toList().forEach { it(state) }
        }
        engine.setCallFinishedListener { record ->
            historyStore.add(record)
            historyListeners.toList().forEach { it() }
        }
    }

    override fun onBind(intent: Intent?): IBinder = binder

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_START) {
            startInForeground()
        }
        return START_NOT_STICKY
    }

    fun addStateListener(listener: (SipUiState) -> Unit) {
        stateListeners += listener
        listener(engine.currentState())
    }

    fun removeStateListener(listener: (SipUiState) -> Unit) {
        stateListeners -= listener
    }

    fun addHistoryListener(listener: () -> Unit) {
        historyListeners += listener
        listener()
    }

    fun removeHistoryListener(listener: () -> Unit) {
        historyListeners -= listener
    }

    fun startSip(account: SipAccount) {
        // startForegroundService() is requested by the activity first; promote
        // synchronously here so slow PJSIP initialization cannot miss Android's
        // foreground-service startup deadline.
        startInForeground()
        engine.connect(account)
    }

    fun disconnect() {
        engine.disconnect()
        val state = engine.currentState()
        if (state.callPhase == "idle" && state.registrationText == "Rozłączono") {
            stopForegroundIfIdle()
        }
    }

    fun dial(number: String) = engine.dial(number)
    fun answer() = engine.answer()
    fun hangup() = engine.hangup()
    fun setMuted(muted: Boolean) = engine.setMuted(muted)
    fun setSpeakerEnabled(enabled: Boolean) = engine.setSpeakerEnabled(enabled)
    fun setAudioRoute(route: CallAudioRoute) = engine.setAudioRoute(route)

    private fun startInForeground() {
        if (foregroundStarted) return
        val notification = buildNotification(engine.currentState())
        when {
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE -> {
                val types = ServiceInfo.FOREGROUND_SERVICE_TYPE_PHONE_CALL or
                    ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE
                startForeground(NOTIFICATION_ID, notification, types)
            }
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.R -> {
                startForeground(
                    NOTIFICATION_ID,
                    notification,
                    ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE
                )
            }
            else -> startForeground(NOTIFICATION_ID, notification)
        }
        foregroundStarted = true
    }

    private fun stopForegroundIfIdle() {
        if (foregroundStarted) {
            stopForeground(STOP_FOREGROUND_REMOVE)
            foregroundStarted = false
        }
        stopSelf()
    }

    private fun buildNotification(state: SipUiState): Notification {
        val openApp = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java).apply {
                flags = Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP
            },
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        return Notification.Builder(this, NOTIFICATION_CHANNEL)
            .setSmallIcon(android.R.drawable.sym_action_call)
            .setContentTitle("TK App")
            .setContentText(SipForegroundNotificationText.from(state))
            .setContentIntent(openApp)
            .setCategory(Notification.CATEGORY_CALL)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .build()
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return
        val channel = NotificationChannel(
            NOTIFICATION_CHANNEL,
            "Połączenia SIP",
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = "Stan rejestracji i aktywnych połączeń TK App"
            setShowBadge(false)
        }
        getSystemService(NotificationManager::class.java).createNotificationChannel(channel)
    }

    override fun onDestroy() {
        engine.setListener(null)
        engine.setCallFinishedListener(null)
        engine.release()
        stateListeners.clear()
        historyListeners.clear()
        if (foregroundStarted) stopForeground(STOP_FOREGROUND_REMOVE)
        foregroundStarted = false
        super.onDestroy()
    }

    companion object {
        const val ACTION_START = "pl.tkapp.mobile.action.START_SIP"
        private const val NOTIFICATION_CHANNEL = "tk_app_sip"
        private const val NOTIFICATION_ID = 2001
    }
}
