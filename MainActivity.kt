package com.example.taffa

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.*
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.platform.LocalDensity
import com.example.taffa.ui.theme.TaffaTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import android.util.Log
import kotlinx.coroutines.withContext

import okhttp3.OkHttpClient
import okhttp3.Request

import org.tensorflow.lite.Interpreter
import android.content.Context
import androidx.compose.ui.platform.LocalContext
import java.nio.ByteBuffer
import java.nio.ByteOrder

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            TaffaTheme {
                InfoScreen()
            }
        }
    }
}

// Load the TensorFlow Lite model from the assets folder
fun loadModel(context: Context): Interpreter {
    val assetManager = context.assets
    val modelFile = assetManager.open("model.tflite")


    Log.d("ModelLoading", "Model file opened successfully: ${modelFile.available()} bytes")


    val byteBuffer = ByteBuffer.allocateDirect(modelFile.available())
    byteBuffer.put(modelFile.readBytes())
    byteBuffer.rewind()

    return Interpreter(byteBuffer)
}

fun predict(model: Interpreter, inputValue: Float): Float {
    val inputBuffer = ByteBuffer.allocateDirect(4).order(ByteOrder.nativeOrder())
    inputBuffer.putFloat(inputValue)

    val outputBuffer = Array(1) { FloatArray(1) }

    model.run(inputBuffer, outputBuffer)

    val outputBufferString = outputBuffer.joinToString(", ") { it.joinToString(", ") }
    Log.d("FloatArrayLog", "Output: $outputBufferString")

    return outputBuffer[0][0]
}

@Composable
fun InfoScreen() {
    var serverResponse by remember { mutableStateOf("Loading...") }
    var formattedWaitingTimePrediction by remember { mutableStateOf("0") }

    val context = LocalContext.current
    val model: Interpreter? = remember(context) {
        Log.d("ModelLoad", "Model loaded successfully")
        loadModel(context)
    }

    LaunchedEffect(Unit) {

        if (model == null) {
            Log.e("ModelLoadError", "TensorFlow Lite model failed to load.")
        } else {
            Log.d("ModelLoad", "TensorFlow Lite model loaded successfully.")
        }
    }

    LaunchedEffect(Unit) {
        while (true) {
            val response = withContext(Dispatchers.IO) {
                fetchDataFromServer()
            }
            serverResponse = response ?: "Error fetching data"

            val numberOfVisitors = serverResponse.toIntOrNull() ?: 0

            model?.let {
                val waitingTimePrediction = predict(it, numberOfVisitors.toFloat())
                formattedWaitingTimePrediction = String.format("%.1f", waitingTimePrediction)
            }


            delay(15000)
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(horizontal = 16.dp),
        verticalArrangement = Arrangement.Top,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text(
            text = "TÄFFÄ",
            color = Color.Blue,
            fontSize = 32.sp,
            fontWeight = FontWeight.Bold,
            textAlign = TextAlign.Center,
            modifier = Modifier.padding(top = 50.dp)
        )

        Spacer(modifier = Modifier.weight(1f))

        Text(
            text = "Number of visitors: $serverResponse / 200",
            color = Color.Black,
            fontSize = 22.sp,
            textAlign = TextAlign.Center,
            modifier = Modifier.padding(bottom = 10.dp)
        )

        Text(
            text = "Estimated waiting time: $formattedWaitingTimePrediction mins",
            color = Color.Black,
            fontSize = 22.sp,
            textAlign = TextAlign.Center,
            modifier = Modifier.padding(bottom = with(LocalDensity.current) { 72.sp.toDp() })
        )

    }
}

suspend fun fetchDataFromServer(): String? {
    return try {
        val client = OkHttpClient()
        val request = Request.Builder()
            .url("") // Enter the server endpoint here
            .build()

        val response = client.newCall(request).execute()

        if (response.isSuccessful) {
            val responseBody = response.body?.string()
            if (responseBody.isNullOrEmpty()) {
                "Error: Empty response body"
            } else {
                responseBody
            }
        } else {
            "Error: HTTP ${response.code}"
        }
    } catch (e: Exception) {
        Log.e("FetchDataError", "Exception occurred while making network request", e)

        "Error: ${e.localizedMessage ?: "Unknown error"}"
    }
}

@Preview(showBackground = true)
@Composable
fun InfoScreenPreview() {
    TaffaTheme {
        InfoScreen()
    }
}
