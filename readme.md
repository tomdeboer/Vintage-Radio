*Vintage Radio*
----
This project started out a birthday gift for a friend, but has changed shape many times over the years. The idea was to take a '60s vintage radio and upgrade it as well as I can with stereo, bluetooth, IR remote control, motorised volume knobs and quality >50 W speakers.

Now that the project is really taking shape, I thought: why not make the code and documents open source!

**Note: this is a work in progress at the moment.**

### What can it do?
As mentioned, the radio packs some serious punch when it comes to audio. It can play your tunes loudly and clearly. It also has a bluetooth receiver so you can stream your audio wirelessly. In addition to bluetooth, it has the standard red and white tulip audio jacks and a 3,5 mm audio/headphone jack. The headphone jack is actually an input and has some special functionality: it can mimic headphone controls. This means when you connect your phone to the radio via the headphone jack, your phone will think there's a headphone attached. Together with an infrared receiver, this allows your to control the volume or playback of your phone, as if you were touching the playback controls of your headphone. As a finishing touch, the vintage knobs turn a motorised potentiometer. So when you control the volume externally, i.e. using the remote, the knob will catch up with you and will turn to the appropriate position as if you were turning it yourself!

**Summing up:**

* \>50W audio amplifier
* Bluetooth audio
* Tulip audio connectors
* 3,5mm jack with headphone controls
* Infrared remote
* Self-turning volume knob!


### How do I use it?
The goal of this repository is to provide some sort of manual or inspiration for building your own upgraded vintage radio. It'll contain the Eagle board files of the PCB I designed for the project and there'll be links to all the datasheets you need. The board itself houses a Arduino Pro Mini 5V (for easy development) and contains digital pots for controlling the volume and controlling the headphone functionality. The bluetooth board is a separate RN52 break out board. I used the one from Spark Fun.

### Docs
[Arduino Pro mini](http://www.arduino.cc/en/Main/ArduinoBoardProMini)

[MCP41010 (digital pots)](http://ww1.microchip.com/downloads/en/DeviceDoc/11195c.pdf)

### BOM
- 1x Arduino Pro Mini $10
- 3x MCP41010 $1.50
- 6x 4.7k 5% resistor $1.50
- 10x Screw Terminals (2 conn) 3,5mm grid $5
- 4x Screw Terminals (3 conn) 3,5mm grid $2
- 1x 3.3V converter $2

*Total cost ~$25*

- '60s radio $20
- 35W Amplifier $60
- RN52 Bluetooth break out $40
- 50W JBL car Speakers $75

*Total: ~$195*