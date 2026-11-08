package pl.tkapp.mobile

import android.content.Context
import android.media.AudioManager

/**
 * Android call-audio routing adapter. The SIP engine should call beginCall()
 * after media is established and endCall() when the call is released.
 */
class AudioRouteController(context: Context) {
    private val audio = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager

    fun beginCall() {
        @Suppress("DEPRECATION")
        run {
            audio.mode = AudioManager.MODE_IN_COMMUNICATION
            audio.isSpeakerphoneOn = false
        }
    }

    fun setSpeakerEnabled(enabled: Boolean) {
        @Suppress("DEPRECATION")
        audio.isSpeakerphoneOn = enabled
    }

    fun setMuted(muted: Boolean) {
        audio.isMicrophoneMute = muted
    }

    fun endCall() {
        @Suppress("DEPRECATION")
        run {
            audio.isMicrophoneMute = false
            audio.isSpeakerphoneOn = false
            audio.mode = AudioManager.MODE_NORMAL
        }
    }
}
