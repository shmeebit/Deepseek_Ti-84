try:
    from ti_system import store_string, recall_string
except ImportError:
    def store_string(idx, s):
        print(f"[stub] store_string({idx}, {s!r})")
    def recall_string(idx):
        print(f"[stub] recall_string({idx})")
        return ""

import time

def set_str(i, s):
    store_string(i, str(s))

def get_str(i):
    return recall_string(i)

def ask_once(q: str, timeout_s: int = 30):
    if not q:
        return "", False
    set_str(1, q)
    set_str(0, "GO")
    t0 = time.time()
    while True:
        time.sleep(0.3)
        t = get_str(0)
        if t == "DONE":
            return get_str(2), True
        if t == "ERROR":
            return "ERROR", False
        if time.time() - t0 > timeout_s:
            return "TIMEOUT", False

if __name__ == "__main__":
    print("DeepSeek TI-84 Bridge (clean)")
    while True:
        try:
            q = input("Question> ")
        except Exception:
            q = ""
        if not q:
            break
        ans, ok = ask_once(q)
        print(("OK: " if ok else "ERR: ") + ans)
