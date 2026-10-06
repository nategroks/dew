# dew

> Terminal to-do list with a pomodoro timer (waves) and a Game of Life garden.
> Tasks live in `~/.local/share/dew/tasks.md`, settings in `~/.config/dew/config`.
> More information: `man dew`.

- Open the to-do list:

`dew`

- Add a task to the Backlog from the shell:

`dew add {{fix the grub theme}}`

- Add a task straight to Today:

`dew add -t {{email the site}}`

- Use a different task file:

`dew --file {{path/to/tasks.md}}`

- Inside dew, start, pause or resume a wave on the selected task:

`<Space>`

- Inside dew, check off the selected task (a wolf runs across the board):

`x`

- Inside dew, switch the wave length between 15, 25 and 45 minutes (breaks follow):

`m`

- Inside dew, see today's garden:

`w`

- Inside dew, switch the Game of Life rule that waves run (conway, highlife, day & night, ...):

`r`
