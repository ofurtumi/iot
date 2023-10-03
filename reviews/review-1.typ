#import "@templates/ass:0.1.0": *

#set heading(numbering: "1.")

#show: doc => template(
  project: "Code Review 1",
  class: "Forritun snjalltækja",
  doc
)

= Scope

This review covers the soure code included in the given *skeleton_serial* code repository provided for assignment P1.

= Overview

The code provided serves as a parser and executor for a few user input commands. It does this within the `commands.c` file, there the function `process_command` takes care of correctly mapping user input to the correct functions. 
There are four different user commands defined and out of these fouronly one of them, `MAC`, has been implemented. Two of the non-implemented commands, `DEC` and `STATUS` have already been declared, however they only return the string `"IMPLEMENT.."`. The last command is only referenced in the `CommandToken` enum and as an option inside the `process_command` function. 

#grid(columns: (2fr, 1fr),[
    User input commands: 
    - MAC: Returns the MAC address of the connected ESP-32 device
    - DEC: To be implemented
    - ID: To be implemented
    - STATUS: To be implemend
],[
  ```c
  enum CommandToken {
    COMMAND_UNKNOWN,
    COMMAND_MAC,
    COMMAND_ID,
    COMMAND_STATUS,
    COMMAND_DEC
  };
  ```
])

= Critical Issues

The function `process_cmd(...)` takes care of mapping the user input to the correct function. It does this for all of the commands declared inside the `CommandToken` enum. This would be fine since `DEC`, `STATUS` and `MAC` are all taken care of, however the `ID` command is not implemented but even so it is handled like the others. This means that if the user inputs the `ID` command the program does not override the `command_result` or `out_message`. As a result the program returns nothing, only a new line and a new prompt. This is an issue since alla other _"defined"_ commands return a message corresponding to the command or to implement the command function themselves, even commands not defined return the message _"COMMAND_ERROR"_.

= Comments

The code is well structured and easy to follow. The `commands.c` file is pretty well commented, however some core functions are not and can at a quick glance be hard to understand.
For example it took me a while to figure out the `init_Command` function was clearing the current command buffer, I would recommend adding a comment to the function explaining what it does.

