#import "@preview/polylux:0.3.1": *

#set page(paper: "presentation-16-9", fill: rgb(230,230,230))
#set text(size: 25pt)
#set grid(columns: (1fr, 1fr), gutter: 16pt)

#polylux-slide[
  #align(horizon + center)[
    = Home Assistant

    Þorvaldur Tumi Baldursson\
    Hákon Ingi Rafnsson

    TÖL103M
  ]
]

#polylux-slide[
  #grid(
    align(right,
      image("./logo.png")
    ),
    align(horizon)[
    = What is Home Assistant?
    - Home Assistant is an open-source home automation platform
    - Flexible and customizable
    - Community-driven
    ]
  )
]

#polylux-slide[
  #grid(
    align(horizon)[
    = What can it be used for?
    - Control lights, switches and other devices
    - Automate tasks through timers and events
    - Monitor sensors
    - Control media players
    ],
    align(right,
      image("./vert.png")
    )
  )
]

#polylux-slide[
  #grid(
    align(right + horizon,
      image("./infografic.png")
    ),
    align(horizon)[
    = How does it work?
    - Runs on a local device
    - Connects to devices through various protocols
      - Wifi, Zigbee, Z-Wave or MQTT
    - Can be accessed through a web interface
    - Can be controlled through a mobile app
    ],
    
  )
]

#polylux-slide[
  #grid(
    align(horizon)[
    = Use cases?
    - Automatically turning on lights when you get home
    - Automatically turning on lights when it gets dark
    - A really loud and bright alarm clock
    - Monitor electricity usage
    ],
    align(right,
      image("./triggers.png")
    )
  )
]

#polylux-slide[
    #align(center)[= Any questions?]
    #align(horizon + center)[If not, thank you for your time!]
    #align(bottom + center)[Þorvaldur Tumi Baldursson - Hákon Ingi Rafnsson]
    
]