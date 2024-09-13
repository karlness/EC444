const speech = require('@google-cloud/speech');
const recorder = require('node-record-lpcm16');
const dgram = require('dgram');

// Creates a client for the Google Cloud Speech API
const client = new speech.SpeechClient();

// Creates a UDP client using dgram
const udpClient = dgram.createSocket('udp4');

const ESP32_IP = '192.168.1.32'; // Replace with your ESP32's actual IP address
const UDP_PORT = 3334; // The port on which the ESP32 UDP server is listening


function sendCommand(command) {
    const message = Buffer.from(command);
    udpClient.send(message, 0, message.length, UDP_PORT, ESP32_IP, (err) => {
        if (err) {
            console.error('UDP message send error:', err);
        } else {
            console.log(`Sent command: ${command}`);
            // Also log the command to the console
            console.log(`Command recognized: ${command}`);
        }
    });
}


// Configure the recorder for 16-bit PCM audio
const recording = recorder.record({
    sampleRateHertz: 16000,
    threshold: 0, // Silence threshold
    recordProgram: 'rec', // 'rec', 'arecord', or 'sox' should be installed on the system
    silence: '1.0', // Seconds of silence before the recording ends
    endOnSilence: true
});

// Prepare the request object for streaming recognize
const request = {
    config: {
        encoding: 'LINEAR16',
        sampleRateHertz: 16000,
        languageCode: 'en-US',
    },
    interimResults: false, // Set to true if you want interim results
};

// Create a recognize stream
const recognizeStream = client.streamingRecognize(request)
    .on('error', console.error)
    .on('data', data => {
        const transcript = data.results[0] && data.results[0].alternatives[0]
            ? data.results[0].alternatives[0].transcript.trim().toLowerCase()
            : '';

        console.log(`Transcription: ${transcript}`);

        // Send commands based on the transcription
        if (transcript.includes("go forward")) {
            sendCommand('forward');
        } else if (transcript.includes("go backward")) {
            sendCommand('backward');
        } else if (transcript.includes("stop")) {
            sendCommand('stop');
        } else if (transcript.includes("right")) {
            sendCommand('right');
        } else if (transcript.includes("left")) {
            sendCommand('left');
        } else if (transcript.includes("center")) {
            sendCommand('center');
        }
    });

// Start recording and pipe the audio input to the Speech API
recording.stream().pipe(recognizeStream);

console.log('Listening, press Ctrl+C to stop.');
