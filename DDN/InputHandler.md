# Input Handler Design Doc

Only design for PC.

Keyboards, mouse movement and buttons can generate different commands.

Each frame, commands are put into a link-list. Different components of the rendering engine or the game engine can loop through this link-list, pick out the element/command that it listen to or consume and move forward.