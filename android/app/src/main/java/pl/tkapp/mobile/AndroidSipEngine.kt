package pl.tkapp.mobile

import android.os.Handler
import android.os.Looper
import org.pjsip.pjsua2.Account
import org.pjsip.pjsua2.AccountConfig
import org.pjsip.pjsua2.AudioMedia
import org.pjsip.pjsua2.AuthCredInfo
import org.pjsip.pjsua2.Call
import org.pjsip.pjsua2.CallInfo
import org.pjsip.pjsua2.CallOpParam
import org.pjsip.pjsua2.CodecInfoVector2
import org.pjsip.pjsua2.Endpoint
import org.pjsip.pjsua2.EpConfig
import org.pjsip.pjsua2.OnCallMediaStateParam
import org.pjsip.pjsua2.OnCallStateParam
import org.pjsip.pjsua2.OnIncomingCallParam
import org.pjsip.pjsua2.OnRegStateParam
import org.pjsip.pjsua2.TransportConfig
import org.pjsip.pjsua2.pjmedia_type
import org.pjsip.pjsua2.pjsip_inv_state
import org.pjsip.pjsua2.pjsip_status_code
import org.pjsip.pjsua2.pjsip_transport_type_e
import org.pjsip.pjsua2.pjsua_call_media_status

/**
 * PJSUA2-backed Android SIP lifecycle.
 *
 * This mirrors the desktop profile: UDP transport, Digest authentication,
 * 16 kHz mono audio, 20 ms frames, and the same codec preference order.
 * The endpoint currently lives while the owning activity is alive; a
 * foreground call service is a separate production step.
 */
data class SipUiState(
    val registered: Boolean = false,
    val registrationText: String = "Rozłączono",
    val registrationCode: Int = 0,
    val callPhase: String = "idle",
    val peer: String = "",
    val incoming: Boolean = false,
    val confirmed: Boolean = false,
    val muted: Boolean = false,
    val speaker: Boolean = false,
    val error: String? = null
)

class AndroidSipEngine(
    private val audioRoutes: AudioRouteController
) {
    private val main = Handler(Looper.getMainLooper())

    private var listener: ((SipUiState) -> Unit)? = null
    private var state = SipUiState()
    private var endpoint: Endpoint? = null
    private var account: TkAccount? = null
    private var activeCall: TkCall? = null
    private var captureMedia: AudioMedia? = null
    private var playbackMedia: AudioMedia? = null
    private var callAudio: AudioMedia? = null
    private var incomingCall = false
    private var confirmed = false
    private var generation = 0
    private var currentServer = "15.235.62.162"
    private var currentPort = 5060

    fun setListener(value: ((SipUiState) -> Unit)?) {
        listener = value
        value?.invoke(state)
    }

    fun currentState(): SipUiState = state

    fun connect(config: SipAccount) {
        if (!onMainThread()) {
            main.post { connect(config) }
            return
        }
        if (activeCall != null) {
            update { it.copy(error = "Najpierw zakończ trwające połączenie.") }
            return
        }
        if (endpoint != null) destroyEndpoint()

        val username = config.username.trim()
        val password = config.password
        val server = config.domain.trim()
        val port = config.port
        if (!username.matches(Regex("^[A-Za-z0-9_.!~*'()+%\\-]{1,128}$"))) {
            update { it.copy(error = "Wpisz poprawny login SIP.") }
            return
        }
        if (password.isEmpty()) {
            update { it.copy(error = "Wpisz hasło SIP. Hasło nie jest zapisywane na urządzeniu.") }
            return
        }
        if (server.isBlank() || server.any { !(it.isLetterOrDigit() || it == '.' || it == '-') } ||
            port !in 1..65535
        ) {
            update { it.copy(error = "Nieprawidłowy serwer lub port SIP.") }
            return
        }

        generation++
        currentServer = server
        currentPort = port
        update {
            it.copy(
                registered = false,
                registrationText = "Uruchamianie SIP…",
                registrationCode = 0,
                callPhase = "idle",
                peer = "",
                incoming = false,
                confirmed = false,
                muted = false,
                speaker = false,
                error = null
            )
        }

        try {
            val ep = Endpoint()
            endpoint = ep
            ep.libCreate()

            val epConfig = EpConfig()
            epConfig.getUaConfig().setUserAgent("TK App/2.0 (Telekomunikacja Kaliska; UDP)")
            epConfig.getUaConfig().setThreadCnt(2)
            epConfig.getLogConfig().setLevel(2)
            epConfig.getLogConfig().setConsoleLevel(0)

            val media = epConfig.getMedConfig()
            media.setClockRate(16000)
            media.setSndClockRate(16000)
            media.setChannelCount(1)
            media.setAudioFramePtime(20)
            media.setNoVad(true)
            media.setEcTailLen(128)

            ep.libInit(epConfig)

            val transport = TransportConfig()
            transport.setPort(0)
            ep.transportCreate(pjsip_transport_type_e.PJSIP_TRANSPORT_UDP, transport)

            val identity = if (config.displayName.isBlank()) {
                "sip:$username@$server"
            } else {
                val display = config.displayName.replace("\\", "\\\\").replace("\"", "\\\"")
                "\"$display\" <sip:$username@$server>"
            }
            val accountConfig = AccountConfig()
            accountConfig.setIdUri(identity)
            accountConfig.getRegConfig().setRegistrarUri("sip:$server:$port;transport=udp")
            accountConfig.getRegConfig().setTimeoutSec(300)
            accountConfig.getRegConfig().setRetryIntervalSec(30)
            accountConfig.getRegConfig().setFirstRetryIntervalSec(5)
            accountConfig.getRegConfig().setRegisterOnAdd(true)
            accountConfig.getSipConfig().getAuthCreds().add(
                AuthCredInfo("Digest", "*", username, 0, password)
            )

            val sipAccount = TkAccount(this)
            account = sipAccount
            sipAccount.create(accountConfig, true)

            ep.libStart()
            configureCodecs(ep)

            captureMedia = ep.audDevManager().getCaptureDevMedia()
            playbackMedia = ep.audDevManager().getPlaybackDevMedia()
            update { it.copy(registrationText = "Rejestracja…", error = null) }
        } catch (error: Exception) {
            destroyEndpoint()
            update {
                it.copy(
                    registered = false,
                    registrationText = "Błąd SIP",
                    registrationCode = 0,
                    error = error.message ?: "Nie udało się uruchomić PJSIP."
                )
            }
        }
    }

    fun disconnect() {
        if (!onMainThread()) {
            main.post { disconnect() }
            return
        }
        if (activeCall != null) {
            update { it.copy(error = "Najpierw zakończ trwające połączenie.") }
            return
        }
        destroyEndpoint()
        update {
            it.copy(
                registered = false,
                registrationText = "Rozłączono",
                registrationCode = 0,
                callPhase = "idle",
                peer = "",
                incoming = false,
                confirmed = false,
                muted = false,
                speaker = false,
                error = null
            )
        }
    }

    fun dial(rawNumber: String) {
        if (!onMainThread()) {
            main.post { dial(rawNumber) }
            return
        }
        val currentAccount = account
        if (currentAccount == null || !state.registered) {
            update { it.copy(error = "Najpierw połącz konto SIP.") }
            return
        }
        if (activeCall != null) {
            update { it.copy(error = "Trwa już rozmowa.") }
            return
        }
        val number = rawNumber.trim().replace(Regex("[\\s()\\-]"), "")
        if (!number.matches(Regex("^\\+?[0-9*#]{1,64}$"))) {
            update { it.copy(error = "Nieprawidłowy numer telefonu.") }
            return
        }

        try {
            val call = TkCall(this, currentAccount, -1)
            activeCall = call
            incomingCall = false
            confirmed = false
            val params = CallOpParam(true)
            params.getOpt().setAudioCount(1)
            params.getOpt().setVideoCount(0)
            call.makeCall("sip:$number@$currentServer:$currentPort;transport=udp", params)
            update {
                it.copy(
                    callPhase = "connecting",
                    peer = number,
                    incoming = false,
                    confirmed = false,
                    muted = false,
                    error = null
                )
            }
        } catch (error: Exception) {
            activeCall?.let { safeDelete(it) }
            activeCall = null
            update { it.copy(callPhase = "idle", peer = "", error = error.message ?: "Nie udało się rozpocząć połączenia.") }
        }
    }

    fun answer() {
        val call = activeCall ?: return
        if (!incomingCall || confirmed) return
        try {
            val params = CallOpParam(true)
            params.setStatusCode(pjsip_status_code.PJSIP_SC_OK)
            call.answer(params)
            update { it.copy(error = null) }
        } catch (error: Exception) {
            update { it.copy(error = error.message ?: "Nie udało się odebrać połączenia.") }
        }
    }

    fun hangup() {
        val call = activeCall ?: return
        try {
            val params = CallOpParam(true)
            if (incomingCall && !confirmed) {
                params.setStatusCode(pjsip_status_code.PJSIP_SC_DECLINE)
            }
            call.hangup(params)
        } catch (error: Exception) {
            update { it.copy(error = error.message ?: "Nie udało się zakończyć połączenia.") }
        }
    }

    fun setMuted(muted: Boolean) {
        val audio = callAudio
        val capture = captureMedia
        try {
            if (audio != null && capture != null) {
                if (muted) capture.stopTransmit(audio) else capture.startTransmit(audio)
            }
            update { it.copy(muted = muted, error = null) }
        } catch (error: Exception) {
            update { it.copy(error = error.message ?: "Nie udało się zmienić mikrofonu.") }
        }
    }

    fun setSpeakerEnabled(enabled: Boolean) {
        try {
            audioRoutes.setSpeakerEnabled(enabled)
            update { it.copy(speaker = enabled, error = null) }
        } catch (error: Exception) {
            update { it.copy(error = error.message ?: "Nie udało się zmienić trasy audio.") }
        }
    }

    fun release() {
        if (!onMainThread()) {
            main.post { release() }
            return
        }
        destroyEndpoint()
        update {
            it.copy(
                registered = false,
                registrationText = "Rozłączono",
                registrationCode = 0,
                callPhase = "idle",
                peer = "",
                incoming = false,
                confirmed = false,
                muted = false,
                speaker = false
            )
        }
    }

    private fun configureCodecs(ep: Endpoint) {
        val codecs: CodecInfoVector2 = ep.codecEnum2()
        for (index in 0 until codecs.size()) {
            val id = codecs.get(index).getCodecId()
            val normalized = id.lowercase()
            val priority = when {
                normalized.startsWith("opus/") -> 255
                normalized.startsWith("pcmu/") -> 240
                normalized.startsWith("pcma/") -> 230
                normalized.startsWith("g722/") -> 220
                else -> 0
            }
            ep.codecSetPriority(id, priority.toShort())
        }
    }

    private fun onRegistration(code: Int, reason: String, expiration: Int, callbackGeneration: Int) {
        main.post {
            if (callbackGeneration != generation) return@post
            val registered = code == 200 && expiration > 0
            update {
                it.copy(
                    registered = registered,
                    registrationText = if (registered) "Połączono" else if (code == 0) "Rejestracja…" else "$code $reason",
                    registrationCode = code,
                    error = if (code >= 400 && code != 401 && code != 407) "Rejestracja SIP: $code $reason" else null
                )
            }
        }
    }

    private fun onIncoming(account: TkAccount, callId: Int, callbackGeneration: Int) {
        val call = TkCall(this, account, callId)
        main.post {
            if (callbackGeneration != generation) {
                safeDelete(call)
                return@post
            }
            if (activeCall != null) {
                try {
                    val busy = CallOpParam(true)
                    busy.setStatusCode(pjsip_status_code.PJSIP_SC_BUSY_HERE)
                    call.hangup(busy)
                } catch (_: Exception) {
                }
                safeDelete(call)
                return@post
            }
            activeCall = call
            incomingCall = true
            confirmed = false
            val peer = try { call.getInfo().getRemoteUri() } catch (_: Exception) { "Nieznany numer" }
            try {
                val ringing = CallOpParam(true)
                ringing.setStatusCode(pjsip_status_code.PJSIP_SC_RINGING)
                call.answer(ringing)
            } catch (error: Exception) {
                update { it.copy(error = error.message ?: "Nie udało się odebrać połączenia przychodzącego.") }
            }
            update {
                it.copy(
                    callPhase = "incoming",
                    peer = peer,
                    incoming = true,
                    confirmed = false,
                    muted = false,
                    error = null
                )
            }
        }
    }

    private fun onCallState(
        call: TkCall,
        callState: pjsip_inv_state,
        peer: String,
        callbackGeneration: Int
    ) {
        main.post {
            if (callbackGeneration != generation || activeCall !== call) return@post
            when (callState) {
                pjsip_inv_state.PJSIP_INV_STATE_DISCONNECTED -> {
                    disconnectAudio(call)
                    safeDelete(call)
                    activeCall = null
                    incomingCall = false
                    confirmed = false
                    update {
                        it.copy(
                            callPhase = "idle",
                            peer = "",
                            incoming = false,
                            confirmed = false,
                            muted = false,
                            speaker = false,
                            error = null
                        )
                    }
                }
                pjsip_inv_state.PJSIP_INV_STATE_CONFIRMED -> {
                    confirmed = true
                    audioRoutes.beginCall()
                    update {
                        it.copy(
                            callPhase = "connected",
                            peer = peer,
                            incoming = incomingCall,
                            confirmed = true,
                            error = null
                        )
                    }
                }
                else -> {
                    val phase = if (incomingCall) "incoming" else if (callState == pjsip_inv_state.PJSIP_INV_STATE_EARLY) "ringing" else "connecting"
                    update { it.copy(callPhase = phase, peer = peer, error = null) }
                }
            }
        }
    }

    private fun onCallAudio(call: TkCall, audio: AudioMedia, callbackGeneration: Int) {
        main.post {
            if (callbackGeneration != generation || activeCall !== call) return@post
            try {
                val capture = captureMedia ?: endpoint?.audDevManager()?.getCaptureDevMedia()
                    ?: throw IllegalStateException("Brak urządzenia mikrofonu.")
                val playback = playbackMedia ?: endpoint?.audDevManager()?.getPlaybackDevMedia()
                    ?: throw IllegalStateException("Brak urządzenia głośnika.")
                captureMedia = capture
                playbackMedia = playback
                if (!state.muted) capture.startTransmit(audio)
                audio.startTransmit(playback)
                callAudio = audio
                audioRoutes.beginCall()
                update { it.copy(error = null) }
            } catch (error: Exception) {
                update { it.copy(error = error.message ?: "Nie udało się uruchomić audio rozmowy.") }
            }
        }
    }

    private fun disconnectAudio(call: TkCall) {
        val audio = callAudio
        try {
            if (audio != null) {
                captureMedia?.stopTransmit(audio)
                playbackMedia?.let { audio.stopTransmit(it) }
            }
        } catch (_: Exception) {
        }
        callAudio = null
        audioRoutes.endCall()
    }

    private fun destroyEndpoint() {
        generation++
        val ep = endpoint
        try {
            ep?.libDestroy()
        } catch (_: Exception) {
        }
        activeCall?.let { safeDelete(it) }
        activeCall = null
        account?.let { safeDelete(it) }
        account = null
        endpoint?.let { safeDelete(it) }
        endpoint = null
        captureMedia = null
        playbackMedia = null
        callAudio = null
        incomingCall = false
        confirmed = false
        audioRoutes.endCall()
    }

    private fun update(transform: (SipUiState) -> SipUiState) {
        state = transform(state)
        listener?.invoke(state)
    }

    private fun onMainThread(): Boolean = Looper.myLooper() == Looper.getMainLooper()

    private fun safeDelete(value: Any) {
        try {
            when (value) {
                is Call -> value.delete()
                is Account -> value.delete()
                is Endpoint -> value.delete()
            }
        } catch (_: Exception) {
        }
    }

    private class TkAccount(
        private val engine: AndroidSipEngine
    ) : Account() {
        private val callbackGeneration = engine.generation

        override fun onRegState(prm: OnRegStateParam) {
            engine.onRegistration(
                prm.getCode(),
                prm.getReason(),
                prm.getExpiration(),
                callbackGeneration
            )
        }

        override fun onIncomingCall(prm: OnIncomingCallParam) {
            engine.onIncoming(this, prm.getCallId(), callbackGeneration)
        }
    }

    private class TkCall(
        private val engine: AndroidSipEngine,
        account: Account,
        callId: Int
    ) : Call(account, callId) {
        private val callbackGeneration = engine.generation

        override fun onCallState(prm: OnCallStateParam) {
            try {
                val info: CallInfo = getInfo()
                engine.onCallState(
                    this,
                    info.getState(),
                    info.getRemoteUri(),
                    callbackGeneration
                )
            } catch (error: Exception) {
                engine.main.post {
                    if (callbackGeneration == engine.generation) {
                        engine.update { it.copy(error = error.message ?: "Błąd stanu połączenia SIP.") }
                    }
                }
            }
        }

        override fun onCallMediaState(prm: OnCallMediaStateParam) {
            try {
                val info = getInfo()
                val media = info.getMedia()
                for (index in 0 until media.size()) {
                    val item = media.get(index)
                    if (item.getType() == pjmedia_type.PJMEDIA_TYPE_AUDIO &&
                        item.getStatus() == pjsua_call_media_status.PJSUA_CALL_MEDIA_ACTIVE
                    ) {
                        engine.onCallAudio(this, getAudioMedia(index), callbackGeneration)
                    }
                }
            } catch (error: Exception) {
                engine.main.post {
                    if (callbackGeneration == engine.generation) {
                        engine.update { it.copy(error = error.message ?: "Błąd konfiguracji audio SIP.") }
                    }
                }
            }
        }
    }
}
