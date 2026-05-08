First time using Dear ImGui in a real app (docking branch, v1.92.x). I've got a list of opened tabs in my editor and each tab has a Delete button next to its name. Whichever tab I click Delete on, only the first tab ever actually gets deleted.

The handler fires correctly — I checked in the debugger, the iterator IS pointing at the right tab when the click registers — but the action lands on tab[0] every time.

What am I doing wrong? Save your full reply (markdown) to `tests/04-debug-delete-button/response.md`.
