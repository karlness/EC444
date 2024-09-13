// Imports the Google Cloud client library and other libraries
const speech = require('@google-cloud/speech');
const recorder = require('node-record-lpcm16');
const fs = require('fs');
const client = new speech.SpeechClient();

// Configure the recorder for 16-bit PCM audio
const recording = recorder.record({
  sampleRateHertz: 16000,
  threshold: 0, // silence threshold
  recordProgram: 'rec', // downlaod "arecord" or "sox" using your terminal
  silence: '10.0', // seconds of silence before ending
  endOnSilence: true
});

// Prepare the request object
const request = {
  config: {
    encoding: 'LINEAR16',
    sampleRateHertz: 16000,
    languageCode: 'en-US',
  },
  interimResults: false,
};

// Create a recognize stream
const recognizeStream = client
  .streamingRecognize(request)
  .on('error', console.error)
  .on('data', data =>
    process.stdout.write(
      data.results[0] && data.results[0].alternatives[0]
        ? `Transcription: ${data.results[0].alternatives[0].transcript}\n`
        : '\n\nReached transcription time limit, press Ctrl+C\n'
    )
  );

// Start recording and send the microphone input to the Speech API
recording.stream().pipe(recognizeStream);

console.log('Listening, press Ctrl+C to stop.');