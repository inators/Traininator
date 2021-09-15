# Traininator
![Traininator in its case](https://github.com/inators/Traininator/blob/main/img/Traininator_in_case.jpg?raw=true%29)
### A JMRI WiThrottle controller

 - Built around the popular ESP32 microcontroller with built in wifi.
 - Fully customizable.
 - Easy to use for all in the family.  Set it up once and it just keeps working.
 - Once programmed with your JMRI server and wifi information you can turn on/off track power and select a loco using only the buttons and forward switch.
 
 This was inspired by the great work of geoffb's [Simple Wifi Throttle That You Can Customize](https://model-railroad-hobbyist.com/node/35652).  I did a complete rewrite of the software but the hardware is basically the same as his except that I added a small function button and I use an inline microUSB power button to turn it on and off.  
 
 ## Parts List with links I used:
 -	[ESP32 Module](https://www.amazon.com/gp/product/B08D5ZD528/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) I bought mine from Amazon but if you are getting anything from adafruit  like the panel mount usb cable they have a bunch of nice modules to choose from.
 -	[A nice big button for the horn](https://www.amazon.com/gp/product/B07ZYH4B9R/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) I bought a bunch since I'll probably use them anyway
 -	[Six toggle switches](https://www.amazon.com/gp/product/B079JG2YSM/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1)
 -	A mini button (got a box of them on ebay so I don't have a specific link)
 -	[An inline microUSB switch](https://www.amazon.com/gp/product/B018BFWLRU/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1)
 -	A linear potentiometer (once again I bought a little box of them on ebay) - You can always you a regular knob style potentiometer but I like the old school feel of the sliding lever.
 -	[A relatively compact USB rechargable battery](https://www.amazon.com/gp/product/B08GLYSTLZ/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1)
- [Panel mount Micro USB to charge the battery](https://www.adafruit.com/product/3258) This was from adafruit
- [i2c Display module](https://www.amazon.com/gp/product/B08L7QW7SR/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1)

Instead of a circuit board I just opted for the ugly wiring technique.  The case is on [Tinkercad](https://www.tinkercad.com/things/2L7K1vGAjS5-traininator), click the link or search for Traininator.  You can see below I have some standoff printed on the front and some plastic pieces that hold the power button, screen, and mini button held strong.  
![Traininator opened up](https://github.com/inators/Traininator/blob/c6cc4fd54a4e9cd1cd1765979c7a4b41e5b041de/img/Traininator_open.jpg?raw=true)
### Basic wiring instructions
The only thing that really matters here is that the screen is connected to the proper i2c pins and whichever type of pot you use is wired to an one of the ADC pins.  All of the switches and buttons can be on pretty much any other pin as long as you have some way of knowing what pin you use.  You'll see in my code where I define what pins are what that this is flexible.  Still go to geoffb's blog linked above if you want to see what pins he used.  We use all the same stuff except I use a different oled screen (same pinout) and I have an extra button to map.

All of the switches and buttons will have visual feedback on the display so if you mix up which button is which just use the display then change your button function order here:
```
// functions - Forward = 99, backward = 98, programming button = 97

// I'm picking my switches to control 0,1,2,3,7,8 since 8 is mute and 7 is dim on my controller

unsigned char trainFunction[] = {97, 99, 98, 0, 1, 3, 7, 8, 2}; // What functions do you want the buttons to turn on/off?
```
### Functions
The function button serves a few purposes.  There is a visual reminder on the screen so you know what will happen if you click the horn.
 - Double click the button to issue a stop command to your train
 - Click once (wait 2 seconds) then use the horn button to turn on track power.
- Click a second time (after waiting 2 seconds) and use the horn to turn off track power
- Click a third time to choose which loco you will use (more below)
- Click a fourth through eigth time to decide which functions the switches will control (more below).

To select a loco you use the function button as described above then click the horn.  The throttle shuts off and the display changes.  When the forward / reverse switch is forward the digit you are editing will change.  Switch it to the neutral position to stop the changes then use the horn button to continue to the next digit.  The screen will say "ok" to confirm then move on to the next digit.
Choosing a function is the same except you only have two digits that you need to choose.  You can change the function for any of the five switches.  Note the horn button is always function two unless you change it in the program.         
