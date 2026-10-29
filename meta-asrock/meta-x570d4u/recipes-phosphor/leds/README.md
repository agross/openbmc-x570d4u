# UID LED

`x570d4u-uid-led.bb` replaces the LED Group Manager executable through a
systemd drop-in while retaining the standard enclosure-identify D-Bus interface.
GPIO A0 reports the latched LED state (low means lit). GPIO C6 emulates the
physical button with a 200 ms open-drain low pulse and is released afterwards.
The setter pulses only when the requested and actual states differ, then checks
the result; polling publishes changes made with the physical button. Shutdown
releases the button line without changing the latched state. The controller
also exposes the existing `bmc_booted` logical group.
