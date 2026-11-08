package pl.tkapp.mobile

import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Backspace
import androidx.compose.material.icons.filled.Call
import androidx.compose.material.icons.filled.Contacts
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

private val TkYellow = Color(0xFFFFD000)

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val accountStore = SipAccountStore(this)
        val historyStore = CallHistoryStore(this)
        val audioRoutes = AudioRouteController(this)
        setContent {
            var dark by remember { mutableStateOf(isSystemInDarkTheme()) }
            TkTheme(dark) {
                DialerScreen(
                    dark = dark,
                    initialAccount = accountStore.load(),
                    history = historyStore.load(),
                    onToggleTheme = { dark = !dark },
                    onSaveAccount = { accountStore.save(it) },
                    onClearHistory = { historyStore.clear() },
                    onSpeakerChanged = { audioRoutes.setSpeakerEnabled(it) },
                    onCall = {
                        Toast.makeText(this, "SIP engine is not yet connected on Android.", Toast.LENGTH_LONG).show()
                    }
                )
            }
        }
    }
}

@Composable
private fun TkTheme(dark: Boolean, content: @Composable () -> Unit) {
    val scheme = if (dark) darkColorScheme(
        primary = TkYellow, onPrimary = Color(0xFF211B00),
        background = Color(0xFF151515), surface = Color(0xFF202020),
        surfaceVariant = Color(0xFF414141), onSurface = Color(0xFFF5F5F5)
    ) else lightColorScheme(
        primary = TkYellow, onPrimary = Color(0xFF211B00),
        background = Color(0xFFF5F5F5), surface = Color.White,
        surfaceVariant = Color(0xFFE4E4E4), onSurface = Color(0xFF151515)
    )
    MaterialTheme(colorScheme = scheme, content = content)
}

@Composable
private fun DialerScreen(
    dark: Boolean,
    initialAccount: SipAccount,
    history: List<CallRecord>,
    onToggleTheme: () -> Unit,
    onSaveAccount: (SipAccount) -> Unit,
    onClearHistory: () -> Unit,
    onSpeakerChanged: (Boolean) -> Unit,
    onCall: () -> Unit
) {
    var number by remember { mutableStateOf("") }
    var page by remember { mutableIntStateOf(0) }
    var username by remember { mutableStateOf(initialAccount.username) }
    var password by remember { mutableStateOf(initialAccount.password) }
    var displayName by remember { mutableStateOf(initialAccount.displayName) }
    var domain by remember { mutableStateOf(initialAccount.domain) }
    var port by remember { mutableStateOf(initialAccount.port.toString()) }
    var speaker by remember { mutableStateOf(false) }
    val keys = listOf("1", "2", "3", "4", "5", "6", "7", "8", "9", "−", "0", "contacts")
    Scaffold(
        containerColor = MaterialTheme.colorScheme.background,
        bottomBar = {
            NavigationBar(containerColor = MaterialTheme.colorScheme.surface) {
                NavigationBarItem(selected = page == 0, onClick = { page = 0 }, icon = { Icon(Icons.Default.Call, null) }, label = { Text("Telefon") })
                NavigationBarItem(selected = page == 1, onClick = { page = 1 }, icon = { Icon(Icons.Default.History, null) }, label = { Text("Historia") })
                NavigationBarItem(selected = page == 2, onClick = { page = 2 }, icon = { Icon(Icons.Default.Contacts, null) }, label = { Text("Kontakty") })
                NavigationBarItem(selected = page == 3, onClick = { page = 3 }, icon = { Icon(Icons.Default.Settings, null) }, label = { Text("Ustawienia") })
            }
        }
    ) { padding ->
        Column(
            modifier = Modifier.fillMaxSize().padding(padding).padding(horizontal = 22.dp, vertical = 18.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text("TK App", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
                    Text(listOf("Telekomunikacja Kaliska", "Historia połączeń", "Kontakty", "Ustawienia")[page], color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                TextButton(onClick = onToggleTheme) { Text(if (dark) "Jasny" else "Ciemny") }
            }
            Spacer(Modifier.height(18.dp))
            when (page) {
                0 -> {
                    Text("Wybierz numer", style = MaterialTheme.typography.titleMedium)
                    Spacer(Modifier.height(12.dp))
                    Box(Modifier.fillMaxWidth().height(64.dp).background(MaterialTheme.colorScheme.surface, RoundedCornerShape(18.dp)).padding(horizontal = 16.dp), contentAlignment = Alignment.Center) {
                        BasicTextField(
                            value = number,
                            onValueChange = { number = it.filter { c -> c.isDigit() || c in "*+#" }.take(80) },
                            singleLine = true,
                            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Phone),
                            textStyle = TextStyle(color = MaterialTheme.colorScheme.onSurface, fontSize = 26.sp, textAlign = TextAlign.Center),
                            decorationBox = { inner ->
                                if (number.isEmpty()) Text("Numer telefonu", color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 20.sp)
                                inner()
                            }
                        )
                    }
                    Spacer(Modifier.weight(1f))
                    Column(verticalArrangement = Arrangement.spacedBy(14.dp)) {
                        keys.chunked(3).forEach { row ->
                            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                                row.forEach { key ->
                                    FilledTonalButton(
                                        onClick = { when (key) {
                                            "−" -> number = number.dropLast(1)
                                            "contacts" -> page = 2
                                            else -> number += key
                                        } },
                                        modifier = Modifier.size(82.dp), shape = CircleShape,
                                        contentPadding = PaddingValues(0.dp),
                                        colors = ButtonDefaults.filledTonalButtonColors(containerColor = MaterialTheme.colorScheme.surfaceVariant, contentColor = MaterialTheme.colorScheme.onSurface)
                                    ) {
                                        if (key == "contacts") Icon(Icons.Default.Contacts, "Kontakty", Modifier.size(30.dp))
                                        else if (key == "−") Icon(Icons.Default.Backspace, "Usuń cyfrę", Modifier.size(27.dp))
                                        else Text(key, fontSize = 30.sp, fontWeight = FontWeight.Normal)
                                    }
                                }
                            }
                        }
                    }
                    Spacer(Modifier.height(22.dp))
                    FilledIconButton(onClick = onCall, enabled = number.isNotBlank(), modifier = Modifier.size(86.dp), shape = CircleShape,
                        colors = IconButtonDefaults.filledIconButtonColors(containerColor = TkYellow, contentColor = Color.Black)
                    ) { Icon(Icons.Default.Call, "Zadzwoń", Modifier.size(34.dp)) }
                    Spacer(Modifier.height(12.dp))
                }
                1 -> {
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                        Text("Ostatnie połączenia", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.SemiBold)
                        TextButton(onClick = onClearHistory) { Text("Wyczyść") }
                    }
                    if (history.isEmpty()) EmptyPage("Brak połączeń", "Historia zostanie wypełniona po podłączeniu silnika SIP.")
                    else LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        items(history) { item ->
                            ElevatedCard(Modifier.fillMaxWidth()) {
                                Column(Modifier.padding(14.dp)) {
                                    Text(item.peer, fontWeight = FontWeight.SemiBold)
                                    Text("${item.direction.name} · ${SimpleDateFormat("dd.MM.yyyy HH:mm", Locale.getDefault()).format(Date(item.startedAt))}", color = MaterialTheme.colorScheme.onSurfaceVariant)
                                    Text("Czas: ${item.durationSeconds / 60}:${(item.durationSeconds % 60).toString().padStart(2, '0')}")
                                }
                            }
                        }
                    }
                }
                2 -> EmptyPage("Kontakty", "Kontakty będą dostępne po dodaniu synchronizacji.")
                3 -> {
                    LazyColumn(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                        item {
                            Text("Konto SIP", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.SemiBold)
                            Text("Ustawienia są zapisywane lokalnie na tym urządzeniu.", color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                        item { OutlinedTextField(username, { username = it }, label = { Text("Login SIP") }, modifier = Modifier.fillMaxWidth(), singleLine = true) }
                        item { OutlinedTextField(password, { password = it }, label = { Text("Hasło SIP") }, modifier = Modifier.fillMaxWidth(), singleLine = true, visualTransformation = PasswordVisualTransformation()) }
                        item { OutlinedTextField(displayName, { displayName = it }, label = { Text("Nazwa wyświetlana") }, modifier = Modifier.fillMaxWidth(), singleLine = true) }
                        item { OutlinedTextField(domain, { domain = it }, label = { Text("Domena / adres serwera") }, modifier = Modifier.fillMaxWidth(), singleLine = true) }
                        item { OutlinedTextField(port, { port = it.filter(Char::isDigit).take(5) }, label = { Text("Port SIP") }, modifier = Modifier.fillMaxWidth(), singleLine = true, keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number)) }
                        item {
                            Button(onClick = {
                                val parsedPort = port.toIntOrNull()
                                if (username.isBlank() || domain.isBlank() || parsedPort == null || parsedPort !in 1..65535) {
                                    return@Button
                                }
                                onSaveAccount(SipAccount(username, password, displayName, domain, parsedPort))
                            }, modifier = Modifier.fillMaxWidth()) { Text("Zapisz konto") }
                        }
                        item {
                            HorizontalDivider()
                            Text("Audio", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.SemiBold)
                            Text("Sterowanie trasą audio jest gotowe do podłączenia z silnikiem rozmów.", color = MaterialTheme.colorScheme.onSurfaceVariant)
                            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                                Column(Modifier.weight(1f)) {
                                    Text("Głośnik")
                                    Text("Przełącznik trasy głośnika Androida", color = MaterialTheme.colorScheme.onSurfaceVariant, style = MaterialTheme.typography.bodySmall)
                                }
                                Switch(checked = speaker, onCheckedChange = { speaker = it; onSpeakerChanged(it) })
                            }
                        }
                        item {
                            Text("Stan SIP: niepołączony", color = MaterialTheme.colorScheme.onSurfaceVariant)
                            Text("Rejestracja, odbieranie i wykonywanie połączeń wymagają jeszcze portu PJSIP dla Androida.", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun EmptyPage(title: String, description: String) {
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.Center, horizontalAlignment = Alignment.CenterHorizontally) {
        Text(title, style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.SemiBold)
        Spacer(Modifier.height(8.dp))
        Text(description, color = MaterialTheme.colorScheme.onSurfaceVariant, textAlign = TextAlign.Center)
    }
}
