const {SerialPort} = require('serialport');
const {ReadlineParser} = require('@serialport/parser-readline');
const fs = require('fs'); 


const port = new SerialPort({
  path: '/dev/cu.usbserial-01558ACA',
  //path: 'COM',
  baudRate: 115200
});

const parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));


function appendDataToFile(data) {
  // Format the current time
  const now = new Date().toISOString();
  // Format the string to be saved: current time, sensor-value
  const line = `${now}, ${data}\n`;

  // Append the formatted string to the file sensor-data.csv
  fs.appendFile('sensor-data.csv', line, (err) => {
    if (err) {
      console.error('Failed to write data to file:', err);
    }
  });
}

// Serial port event listeners
port.on("open", () => {
  console.log('Serial port now open');
});

parser.on('data', data => {
  console.log('The word from ESP32:', data);
  // Also write the sensor data to the file
  appendDataToFile(data);
});
