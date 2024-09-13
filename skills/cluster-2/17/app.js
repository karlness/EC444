const express = require('express');
const fs = require('fs').promises; // Use promises with fs for async
const csvtojson = require("csvtojson");
const app = express();


app.use(express.static('public'));
app.use(express.urlencoded({ extended: true }));

// Route to serve the index.html file
app.get('/', async (req, res) => {
  try {
    const data = await fs.readFile('index.html');
    res.writeHead(200, {'Content-Type': 'text/html'});
    res.end(data);
  } catch (err) {
    res.status(500).send('Error reading the index.html file');
  }
});

// Route to send CSV data as JSON
app.get('/data', async (req, res) => {
  try {
    const jsonArray = await csvtojson().fromFile("stocks-csv.txt");
    console.log(jsonArray); // Log the JSON array to the console
    res.send(jsonArray); // Send the JSON array to the client
  } catch (err) {
    res.status(500).send('Error converting CSV to JSON');
  }
});


app.listen(2000, () => {
  console.log("Server is running on port 2000");
});
