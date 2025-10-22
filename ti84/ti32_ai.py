"""
TI-32 AI Assistant for TI-84 Plus CE Python
Communicates with ESP32-S3 via USB to query DeepSeek AI

Usage:
1. Connect calculator to ESP32-S3 via USB
2. Run this program
3. Enter your question
4. Wait for AI response
"""

import time
import sys

# TI-84 CE Python string variable functions
try:
    from ti_system import *
except ImportError:
    # Fallback for testing on PC
    print("Warning: ti_system not available (running on PC?)")
    
    class MockStr:
        def __init__(self):
            self.data = {}
        
        def store(self, index, value):
            self.data[index] = value
            print(f"[Mock] Str{index} = '{value}'")
        
        def recall(self, index):
            return self.data.get(index, "")
    
    str_var = MockStr()


def clear_screen():
    """Clear the screen"""
    try:
        import ti_draw
        ti_draw.clear()
    except:
        print("\n" * 10)


def display_text(text, x=10, y=10):
    """Display text on screen"""
    try:
        import ti_draw
        ti_draw.draw_text(x, y, text)
    except:
        print(text)


def get_input(prompt):
    """Get text input from user"""
    print(prompt)
    try:
        return input("> ")
    except:
        # If input() not available, use a default
        return "What is 2+2?"


def send_question(question):
    """
    Send question to ESP32 via string variables
    Protocol:
    - Str1 = question
    - Str0 = "GO" (trigger)
    - Wait for Str0 to become "DONE" or "ERROR"
    - Read answer from Str2
    """
    print("Sending question to AI...")
    
    # Store question in Str1
    try:
        str_var.store(1, question)
    except:
        pass
    
    # Trigger ESP32 by setting Str0 to "GO"
    try:
        str_var.store(0, "GO")
    except:
        pass
    
    # Wait for response (poll Str0 for status)
    max_wait = 30  # 30 seconds timeout
    start_time = time.time()
    
    while time.time() - start_time < max_wait:
        try:
            status = str_var.recall(0)
        except:
            status = ""
        
        if status == "DONE":
            # Read answer from Str2
            try:
                answer = str_var.recall(2)
                return answer, True
            except:
                return "Error reading response", False
        
        elif status == "ERROR":
            return "ESP32 reported an error", False
        
        elif status == "WAIT":
            print("Processing...")
        
        # Small delay to avoid overwhelming the system
        time.sleep(0.5)
    
    return "Timeout waiting for response", False


def main():
    """Main program loop"""
    clear_screen()
    
    print("=" * 30)
    print("TI-32 AI Assistant")
    print("DeepSeek-R1 via ESP32-S3")
    print("=" * 30)
    print()
    
    # Initialize - clear status string
    try:
        str_var.store(0, "")
    except:
        pass
    
    while True:
        print()
        print("Options:")
        print("1. Ask a question")
        print("2. Math solver")
        print("3. Quick facts")
        print("4. Exit")
        print()
        
        choice = get_input("Select (1-4): ")
        
        if choice == "1":
            # Ask any question
            question = get_input("Your question: ")
            if question and question.strip():
                answer, success = send_question(question.strip())
                print()
                print("Answer:")
                print("-" * 30)
                print(answer)
                print("-" * 30)
                
                if not success:
                    print("Failed to get response")
        
        elif choice == "2":
            # Math solver
            problem = get_input("Math problem: ")
            if problem and problem.strip():
                question = f"Solve: {problem.strip()}"
                answer, success = send_question(question)
                print()
                print("Solution:")
                print("-" * 30)
                print(answer)
                print("-" * 30)
        
        elif choice == "3":
            # Quick facts
            print()
            print("Quick fact topics:")
            print("a) Science")
            print("b) History")
            print("c) Math")
            topic = get_input("Choose topic: ")
            
            questions = {
                "a": "Tell me an interesting science fact",
                "b": "Tell me an interesting history fact", 
                "c": "Tell me an interesting math fact"
            }
            
            if topic.lower() in questions:
                answer, success = send_question(questions[topic.lower()])
                print()
                print(answer)
        
        elif choice == "4":
            print("Goodbye!")
            break
        
        else:
            print("Invalid choice")
        
        # Pause before next iteration
        try:
            input("\nPress ENTER to continue...")
        except:
            time.sleep(1)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nExiting...")
    except Exception as e:
        print(f"Error: {e}")
        try:
            import traceback
            traceback.print_exc()
        except:
            pass
