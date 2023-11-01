#import "@templates/ass:0.1.1": *

#show: doc => template(
  project: "Code review 2",
  class: "Töl103M",
  doc
  )

#set heading(numbering: "1.1")

#let question(body) = {
  rect(
    inset: 8pt, 
    width: 100%, 
    text(weight:"light", style: "normal", size: 10pt, align(center, body )))
}

= Lownet

There are some aspects of _LowNet_ that could be improved. The first thing that comes to mind is overall security. _LowNet_ is set up kind of like a radio so everybody can theoretically see every message sent to everybody. If you don't reject messages that you're not supposed to see, you'll see all messages no matter who sent them or who was supposed to receive them.

A simple band-aid solution to this problem would be to encrypt our message with some key that we know the receiver has, for example the _mac address_. Since our _packet_ includes who the receiver, _DST Node_, is, we would need to encrypt that aswell. Now we have a packet with some encrypted _DST node_ and message. The receiver would then decrypt the _DST Node_ and check if it matches their _Node ID_, if it does then they can decrypt the message. If it doesn't match, they can just ignore the message. This way we can make sure that only the receiver can read the message. 

This is ofcourse not foolproof since we have a list of all the _Node ID's_ and _MAC addresses_ in the _Node Table_ making the encryption very easy to break. 

I think this would be interesting to design and with it some kind of _key exchange_ to make sure that only the receiver and sender have the key to decrypt the message.


= Scope

This review covers the `DHT22` humidity and temperature sensor `ESP32` firmware driver, aswell as a couple of questions regarding assignment 2.

= Overview

The code provided serves to interface with a `DHT22` sensor from an `ESP32` microcontroller.
The driver reads the sensor data and provides it to the user in a convenient way.
If the data is corrupt somehow then the driver will return an error code instead.

= Critical Issues

The driver code is well put together with no apparent critical issues.

= Comments

The only possible issue i could find would be the `TODO` that is present in production code. It is not a critical issue but it should be addressed and removed before the code is released.
