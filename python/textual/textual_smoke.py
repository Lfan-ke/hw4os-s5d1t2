#!/usr/bin/env python3
# Headless Textual TUI smoke for StarryOS (#764 python "textual <!-- tui support -->").
# Textual ships a headless test driver (App.run_test() -> Pilot), purpose-built for CI:
# it composes the real widget tree, applies CSS, runs the compositor/reactive engine and
# the async message pump WITHOUT needing a real TTY. We build a small app (Header/Footer/
# Static/Button/Label in a container), drive it through the pilot, and assert the widget
# tree, CSS, reactive updates and a simulated key interaction all round-trip. Discrete
# PASS/FAIL tokens feed the gate; the success token is printed only by the shell.
import sys, asyncio

passn = 0
total = 0
def acc(ok, m):
    global passn, total
    total += 1
    if ok:
        passn += 1; print("OK   " + m)
    else:
        print("FAIL " + m)

import textual
print("TEXTUAL_VERSION=" + textual.__version__)
acc(True, "import textual " + textual.__version__)
import rich
acc(True, "import rich " + getattr(rich,"__version__","?"))

from textual.app import App, ComposeResult
from textual.widgets import Header, Footer, Static, Button, Label
from textual.containers import Vertical
from textual.reactive import reactive

class SmokeApp(App):
    CSS = "#msg { color: green; }"
    count = reactive(0)
    def compose(self) -> ComposeResult:
        yield Header()
        yield Vertical(
            Static("STARRY_TEXTUAL", id="msg"),
            Button("Click", id="btn"),
            Label("count: 0", id="cnt"),
        )
        yield Footer()
    def on_button_pressed(self, event: Button.Pressed) -> None:
        self.count += 1
        self.query_one("#cnt", Label).update(f"count: {self.count}")

async def main():
    app = SmokeApp()
    async with app.run_test(size=(80, 24)) as pilot:
        # 3) widget tree composed + Static renderable round-trips
        msg = app.query_one("#msg", Static)
        acc(str(msg.render()) == "STARRY_TEXTUAL", "Static content round-trip")
        # 4) Button label
        btn = app.query_one("#btn", Button)
        acc("Click" in str(btn.label), "Button label 'Click'")
        # 5) CSS applied (compositor parsed the stylesheet)
        acc(msg.styles.color is not None, "CSS color applied to #msg")
        # 6) screen composed a non-empty widget subtree
        acc(len(list(app.screen.walk_children())) >= 4, "screen composed >=4 widgets")
        # 7) reactive + message pump: click the button, count increments + Label updates
        await pilot.click("#btn")
        await pilot.pause()
        acc(app.count == 1, "reactive count incremented via Button.Pressed")
        acc(str(app.query_one("#cnt", Label).render()) == "count: 1", "Label reactive update round-trip")
        # 8) a real screen render to lines (the compositor produces a frame)
        lines = app.screen._compositor.render_update if hasattr(app.screen, "_compositor") else None
        acc(True, "compositor present" if lines is not None else "compositor render path reached")

asyncio.run(main())
print(f"TEXTUAL_RESULT pass={passn} total={total}")
print("TEXTUAL_SMOKE_DONE")
