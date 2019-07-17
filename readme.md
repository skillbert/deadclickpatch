### The bug
In short, the bug is caused by the client not handling key presses in sequence, but instead trying to handle batches of keyboard events at the same time.  

Now the real problem is that key press events can be batched up in many situations. The game only looks at the resulting keyboard state after processing all the batched keyboard events. For example a batched key down and key up event for the same key cancel each other out and the result is that nothing happens. Also the order in which keys were pressed can be lost if two different key presses are batched.

### Reproduction
The bug only happens for hotkeys, any textbox input including chat is handled correctly and not affected by the bug. Despite being called the dead-click bug, it doesn't actually affect clicks.

An easy way to test all these issues is setting the in-game fps cap to 5 in the graphics settings. If you now press and release a key in 0.2sec the client won't see it at all.

### How the patch works
This patch prevents rs from processing any key messages that it can't handle. It does this by halting all input to rs when a key press is detected that doesn't combine well with previous presses in the same cycle. Input processing is allowed again after the rs client outputs a frame to the screen which signals that a new input cycle is starting.

### Using the patch
https://github.com/skillbert/deadclickpatch

This program is mostly a personal programming challenge and out of frustration. I wouldn't recommend using it on your main account and for that reason i'm not including pre compiled binaries.

Compile using visual studio 2015 with same bitness as your rs client. Run patchrs.exe when the rs client is running, you have to do this at every startup.