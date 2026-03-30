<div align="center">

  <h1>The Anicept SBFM</h1>
  
  ![Main Image](https://cdn.hackclub.com/019d40ac-3dbc-7696-9277-a4d6da392339/Screenshot%202026-03-30%20163511.png)

  <h3>By Anicetus</h3>

  <p display="inline">
  
  <a href="https://creativecommons.org/licenses/by-nc/4.0/">
    <img src="https://licensebuttons.net/l/by-nc/4.0/88x31.png" alt="Creative Commons Attribution-NonCommercial 4.0 International License"></a> <a href="https://hackclub.com/highway">
    <img alt="Funded by Hack Club" src="https://img.shields.io/badge/Hack_Club-Funded-ec3750?style=for-the-badge&logo=hackclub&logoColor=ec3750"></img>
  </a>

  </p>

  <h4>The Self-Built Filament Monitor, a smart, DIY 3D printer tool that does more than your average filament detector.</h4>
</div>

<sub>This work is licensed under a
[Creative Commons Attribution-NonCommercial 4.0 International License](https://creativecommons.org/licenses/by-nc/4.0/).
</sub>

## About

Hi! I'm Anicetus, and I love 3D printers and making custom hardware. Recently, however, I had an issue where my 3D printer's extruder lost its grip on the filament, and so stopped extruding it in the middle of a print. This was, of course, not ideal, and caused a lot of wasted filament and time. To stop this from happening again, I introduce to you the SBFM! There are a lot of different filament sensors out there, ranging from the humble microswitch to costly, ultraprecise modules.

But this is one of the first to be precise, cheap, and _fully_ homemade. No custom PCBs. No plastic pieces that can't be 3D printed. This is the true maker's filament detector!

## Features

- A photoelectric sensor to measure how quickly the filament is moving
- A cheap but reliable ESP32C3 SuperMini microcontroller, which sends data over WiFi to the 3D printer host
- Bearings to ensure filament passes through smoothly
- ECAS04 connectors for routing filament
- A fully 3D printable case
- A compact form factor
- ... all this for under $20!

## Assembly + Usage

Assembly video coming soon! For now, please reference the CAD design to see how everything fits together :)

Once it's assembled and filament passes through right, upload the test code to make sure that the ESP32 reads the filament movement accurately. Then upload the real code, inputting the information needed for the pi to access the data over wifi, and add the code to the pi that accesses this data, processes it, and gives instructions to klipper based on it.

## Bill of Materials

|Part                |Link                                                |Cost  |Notes                                                                                                                                                                                            |
|--------------------|----------------------------------------------------|------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
|ESP32C3 SuperMini   |https://www.aliexpress.us/item/3256807292829704.html|$2.64 |These abound on AliExpress for very cheap                                                                                                                                                        |
|Photoelectric sensor|https://www.ebay.com/itm/182156491057               |$8.41 |For some reason, AliExpress doesn't seem to have any of these from reliable sellers. Note that this eBay link is selling two sensors, but it's still cheaper than one sensor from another seller.|
|7x11x3 bearings     |https://www.aliexpress.us/item/3256807442294068.html|$2.59 |3 needed                                                                                                                                                                                         |
|ECAS04 collet clips |https://www.aliexpress.us/item/3256806459368820.html|$1.89 |2 needed                                                                                                                                                                                         |
|Screws              |https://www.aliexpress.us/item/2251832857570651.html|$1.85 |M2 self-tapping, 14mm long, pan head (3 needed)                                                                                                                                                  |
|Subtotal            |                                                    |$17.38|                                                                                                                                                                                                 |
|Tax                 |                                                    |$1.43 |                                                                                                                                                                                                 |
|Total               |                                                    |$18.81|                                                                                                                                                                                                 |

## Contributing

<sub>aha somebody wishes to help me i see</sub>

If you'd like to request changes, suggest additions, or forcefully make me edit things, feel free to contact me via email (`me at anicetus dot dev`, preferred) or Discord (anic3tus).
