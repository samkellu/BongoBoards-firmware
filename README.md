<h1>Bongo Boards</h1>

A series of three custom boards featuring an interactive OLED screen!
And yes, of course they run DOOM.

<p style="display: flex;">
<img src="readme/BongoCat.gif" width="48%" style="margin-right: auto;">
<img src="readme/Doom.gif" width="48%">
</p>

<h2>Bongo 75</h2>
A 75%-ish (basically a 68% with an F-row) 3D printed board. This is the culmination of the knowledge gained from the two previous boards, and is the recommended option if you wish to follow along at home.

<br>

<img src="readme/75_top.jpg" width="100%">

<br>

<img src="readme/75_side.jpg" width="100%">

<h2>DoomPad</h2>
A 3D printed standalone numpad.

<br>

<img src="readme/numpad.jpg" height="400px">

<h2>Bongo80</h2>
An 80% TKL layout using a KBDFans tiger lite chassis, but handwired with an OLED. This was the prototype for the boards that came after it, and suffered greatly from my inability to manufacture plates and cases at the time.

<br>

![Bongo80](https://i.imgur.com/bL6ZGjVh.jpg?1)

<h2>QMK</h2>
This project relies on the underlying framework provided by the QMK project, and will be contributed there once I am happy with the DOOM minigame. The intention is to have the two OLED "Apps" as standalone components that can be included in anyones firmware (granted they have a supported screen).

<br>

Currently the DOOM minigame can be emulated for development within a basic SDL rewrite of the QMK OLED driver, which can be built using the included makefile.

<img src="readme/emulator.gif" width="100%">

