#!/usr/bin/env python3
# Headless casca TUI smoke for StarryOS (#764 python "casca"). casca is a pure-python
# CLI-UI library with CSS-like styling. We build its widget tree (Label/Button/Container),
# round-trip widget content, drive its Redux-style Store (reducer + dispatch -> state
# transition = the reactive core), and feed a KeyEvent through App.handle_input (the
# input/control path) -- all without a real TTY. Discrete PASS/FAIL tokens; success token
# printed only by the shell after pass==total.
import importlib.metadata as M

passn = 0
total = 0
def acc(ok, m):
    global passn, total
    total += 1
    if ok:
        passn += 1; print("OK   " + m)
    else:
        print("FAIL " + m)

import casca
ver = M.version("casca")
print("CASCA_VERSION=" + ver)
acc(ver.startswith("1.0"), "import casca " + ver)

# 2) Label content round-trip + set_text
lb = casca.Label("STARRY_CASCA", id="msg")
acc(lb.text == "STARRY_CASCA", "Label.text round-trip (got %r)" % lb.text)
lb.set_text("UPDATED")
acc(lb.text == "UPDATED", "Label.set_text round-trip (got %r)" % lb.text)

# 3) Button content
bt = casca.Button("Click", id="go")
acc(bt.text == "Click", "Button.text round-trip (got %r)" % bt.text)

# 4) Container holds children
co = casca.Container(casca.Label("a"), casca.Button("b"))
kids = getattr(co, "children", None) or getattr(co, "_children", None) or []
acc(len(list(kids)) == 2, "Container holds 2 children (got %d)" % len(list(kids)))

# 5) App subclass with build_ui composes
class SmokeApp(casca.App):
    def build_ui(self):
        return casca.Container(
            casca.Label("STARRY_CASCA", id="msg"),
            casca.Button("Go", id="go"),
        )
app = SmokeApp()
acc(app is not None, "App(build_ui) instantiates")

# 6) Redux-style Store: reducer + dispatch -> state transition (the reactive core)
def reducer(state, action):
    state = state or {"count": 0}
    if action.get("type") == "INC":
        return {"count": state["count"] + 1}
    return state
store = casca.create_store(reducer) if hasattr(casca, "create_store") else casca.Store(reducer)
before = store.get_state()
store.dispatch({"type": "INC"})
store.dispatch({"type": "INC"})
after = store.get_state()
acc(after.get("count") == 2, "Store reducer+dispatch count 0->2 (got %r)" % after)

# 7) input/control path: feed a KeyEvent through handle_input without error
try:
    ev = casca.KeyEvent("q", False)
    app.handle_input(ev)
    acc(True, "App.handle_input(KeyEvent) processed")
except Exception as e:
    acc(False, "App.handle_input raised %s: %s" % (type(e).__name__, str(e)[:80]))

# 8) Keys table present (control key constants the app dispatches on)
acc(hasattr(casca, "Keys") and len([k for k in dir(casca.Keys) if not k.startswith("_")]) > 0,
    "Keys table present")

print("CASCA_RESULT pass=%d total=%d" % (passn, total))
print("CASCA_SMOKE_DONE")
