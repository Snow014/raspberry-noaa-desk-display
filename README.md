This is where I store the code used to display the latest NOAA images from Raspberry-NOAAv2 on a TFT display ran by an esp32.

The ESP sets up a webserver, and listens for POST requests at http://<ESP32-IP>/filename. It expects a JSON payload, containing the file path to the image under "filename", and the type of processing of the image under "proc_type".
REMEMBER: The tJPG library does not handle the progressive JPG files on the pi, those need to be converted to baseline JPG before you can show them on the esp. 

An example of the filename link that the ESP expects: 
<PI-ADDRESS>/images/NOAA-18-20240522-105721-MSA-precip-non-progressive.jpg
The modified Noaa_enhancements.sh saves a copy of each image generated as <ORIGINAL-FILENAME>-non-progressive.jpg.
