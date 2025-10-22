"""
Simple TI-32 AI Test - Minimal example
For TI-84 Plus CE Python

This is a minimal example that asks one question and displays the answer.
Use this to test your USB connection before using the full ti32_ai.py program.
"""

import time

# Mock for PC testing
try:
    from ti_system import *
except:
    class MockStr:
        def __init__(self):
            self.data = {0: "", 1: "", 2: ""}
        def store(self, i, v):
            self.data[i] = v
            print(f"Str{i} = '{v}'")
        def recall(self, i):
            return self.data.get(i, "")
    str_var = MockStr()

print("TI-32 Simple Test")
print("="*20)

# Ask a simple question
question = "What is 2+2?"
print(f"Question: {question}")

# Send to ESP32
str_var.store(1, question)
str_var.store(0, "GO")

print("Waiting for answer...")

# Poll for response (10 second timeout)
for i in range(20):
    status = str_var.recall(0)
    
    if status == "DONE":
        answer = str_var.recall(2)
        print("Answer:")
        print(answer)
        break
    elif status == "ERROR":
        print("Error occurred")
        break
    
    time.sleep(0.5)
    if i % 2 == 0:
        print(".")
else:
    print("Timeout")

print("Test complete")
